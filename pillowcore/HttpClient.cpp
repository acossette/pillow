#include "HttpClient.h"
#include "ByteArrayHelpers.h"
#include <QtCore/QIODevice>
#include <QtCore/QUrl>
#include <QtNetwork/QTcpSocket>

//
// Pillow::HttpRequestWriter
//

Pillow::HttpRequestWriter::HttpRequestWriter(QObject *parent)
	: QObject(parent), _device(0)
{
}

void Pillow::HttpRequestWriter::get(const QByteArray &path, const Pillow::HttpHeaderCollection &headers)
{
	write("GET", path, headers);
}

void Pillow::HttpRequestWriter::head(const QByteArray &path, const Pillow::HttpHeaderCollection &headers)
{
	write("HEAD", path, headers);
}

void Pillow::HttpRequestWriter::post(const QByteArray &path, const Pillow::HttpHeaderCollection &headers, const QByteArray &data)
{
	write("POST", path, headers, data);
}

void Pillow::HttpRequestWriter::put(const QByteArray &path, const Pillow::HttpHeaderCollection &headers, const QByteArray &data)
{
	write("PUT", path, headers, data);
}

void Pillow::HttpRequestWriter::deleteResource(const QByteArray &path, const Pillow::HttpHeaderCollection &headers)
{
	write("DELETE", path, headers);
}

void Pillow::HttpRequestWriter::write(const QByteArray &method, const QByteArray &path, const Pillow::HttpHeaderCollection &headers, const QByteArray &data)
{
	if (_device == 0)
	{
		qWarning() << "Pillow::HttpRequestWriter::write: called while device is not set. Not proceeding.";
		return;
	}

	QByteArray buffer;
	buffer.reserve(8192);
	buffer.append(method).append(' ').append(path).append(" HTTP/1.1\r\n");

	for (const Pillow::HttpHeader *h = headers.constBegin(), *hE = headers.constEnd(); h < hE; ++h)
		buffer.append(h->first).append(": ").append(h->second).append("\r\n");

	if (!data.isEmpty())
	{
		buffer.append("Content-Length: ");
		Pillow::ByteArrayHelpers::appendNumber<int, 10>(buffer, data.size());
		buffer.append("\r\n");
	}

	buffer.append("\r\n");

	if (!data.isEmpty())
	{
		buffer.append(data);
	}

	_device->write(buffer);
}

void Pillow::HttpRequestWriter::setDevice(QIODevice *device)
{
	if (_device == device) return;
	_device = device;
	/* emit deviceChanged(); */
}

//
// Pillow::HttpResponseParser
//

Pillow::HttpResponseParser::HttpResponseParser()
	: _lastWasValue(false)
{
	parser.data = this;
	parser_settings.on_message_begin = parser_on_message_begin;
	parser_settings.on_header_field = parser_on_header_field;
	parser_settings.on_header_value = parser_on_header_value;
	parser_settings.on_headers_complete = parser_on_headers_complete;
	parser_settings.on_body = parser_on_body;
	parser_settings.on_message_complete = parser_on_message_complete;
	clear();
}

int Pillow::HttpResponseParser::inject(const char *data, int length)
{
	size_t consumed = http_parser_execute(&parser, &parser_settings, data, length);
	if (parser.http_errno == HPE_PAUSED) parser.http_errno = HPE_OK; // Unpause the parser that got paused upon completing the message.
	return consumed;
}

void Pillow::HttpResponseParser::injectEof()
{
	http_parser_execute(&parser, &parser_settings, 0, 0);
	if (parser.http_errno == HPE_PAUSED) parser.http_errno = HPE_OK; // Unpause the parser that got paused upon completing the message.
}

void Pillow::HttpResponseParser::clear()
{
	http_parser_init(&parser, HTTP_RESPONSE);
	_headers.clear();
	_content.clear();
}

QByteArray Pillow::HttpResponseParser::errorString() const
{
	const char* desc = http_errno_description(HTTP_PARSER_ERRNO(&parser));
	return QByteArray::fromRawData(desc, qstrlen(desc));
}

void Pillow::HttpResponseParser::messageBegin()
{
	_headers.clear();
	_content.clear();
	_lastWasValue = false;
}

void Pillow::HttpResponseParser::headersComplete()
{
}

void Pillow::HttpResponseParser::messageContent(const char *data, int length)
{
	_content.append(data, length);
}

void Pillow::HttpResponseParser::messageComplete()
{
}

int Pillow::HttpResponseParser::parser_on_message_begin(http_parser *parser)
{
	Pillow::HttpResponseParser* self = reinterpret_cast<Pillow::HttpResponseParser*>(parser->data);
	self->messageBegin();
	return 0;
}

int Pillow::HttpResponseParser::parser_on_header_field(http_parser *parser, const char *at, size_t length)
{
	Pillow::HttpResponseParser* self = reinterpret_cast<Pillow::HttpResponseParser*>(parser->data);
	self->pushHeader();
	self->_field.append(at, length);
	return 0;
}

int Pillow::HttpResponseParser::parser_on_header_value(http_parser *parser, const char *at, size_t length)
{
	Pillow::HttpResponseParser* self = reinterpret_cast<Pillow::HttpResponseParser*>(parser->data);
	self->_value.append(at, length);
	self->_lastWasValue = true;
	return 0;
}

int Pillow::HttpResponseParser::parser_on_headers_complete(http_parser *parser)
{
	Pillow::HttpResponseParser* self = reinterpret_cast<Pillow::HttpResponseParser*>(parser->data);
	self->pushHeader();
	self->headersComplete();
	return 0;
}

int Pillow::HttpResponseParser::parser_on_body(http_parser *parser, const char *at, size_t length)
{
	Pillow::HttpResponseParser* self = reinterpret_cast<Pillow::HttpResponseParser*>(parser->data);
	self->messageContent(at, static_cast<int>(length));
	return 0;
}

int Pillow::HttpResponseParser::parser_on_message_complete(http_parser *parser)
{
	Pillow::HttpResponseParser* self = reinterpret_cast<Pillow::HttpResponseParser*>(parser->data);
	self->messageComplete();

	// Pause the parser to avoid parsing the next message (if there is another one in the buffer).
	http_parser_pause(parser, 1);

	return 0;
}

inline void Pillow::HttpResponseParser::pushHeader()
{
	if (_lastWasValue)
	{
		_headers << Pillow::HttpHeader(_field, _value);
		_field.clear();
		_value.clear();
		_lastWasValue = false;
	}
}

//
// Pillow::HttpClient
//

Pillow::HttpClient::HttpClient(QObject *parent)
	: QObject(parent), _responsePending(false), _error(NoError)
{
	_device = new QTcpSocket(this);
	connect(_device, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(device_error(QAbstractSocket::SocketError)));
	connect(_device, SIGNAL(connected()), this, SLOT(device_connected()));
	connect(_device, SIGNAL(readyRead()), this, SLOT(device_readyRead()));
	_requestWriter.setDevice(_device);

	_baseRequestHeaders << Pillow::HttpHeader("Accept", "*");
}

bool Pillow::HttpClient::responsePending() const
{
	return _responsePending;
}

Pillow::HttpClient::Error Pillow::HttpClient::error() const
{
	return _error;
}

QByteArray Pillow::HttpClient::consumeContent()
{
	QByteArray c = _content;
	_content = QByteArray();
	return c;
}

void Pillow::HttpClient::get(const QUrl &url, const Pillow::HttpHeaderCollection &headers)
{
	request("GET", url, headers);
}

void Pillow::HttpClient::head(const QUrl &url, const Pillow::HttpHeaderCollection &headers)
{
	request("HEAD", url, headers);
}

void Pillow::HttpClient::post(const QUrl &url, const Pillow::HttpHeaderCollection &headers, const QByteArray &data)
{
	request("POST", url, headers, data);
}

void Pillow::HttpClient::put(const QUrl &url, const Pillow::HttpHeaderCollection &headers, const QByteArray &data)
{
	request("PUT", url, headers, data);
}

void Pillow::HttpClient::deleteResource(const QUrl &url, const Pillow::HttpHeaderCollection &headers)
{
	request("DELETE", url, headers);
}

void Pillow::HttpClient::request(const QByteArray &method, const QUrl &url, const Pillow::HttpHeaderCollection &headers, const QByteArray &data)
{
	if (_responsePending)
	{
		qWarning() << "Pillow::HttpClient::request: cannot send new request while another one is under way. Request pipelining is not supported.";
		return;
	}

	// We can reuse an active connection if the request is for the same host and port, so make note of those parameters before they are overwritten.
	const QString previousHost = _request.url.host();
	const int previousPort = _request.url.port();

	Pillow::HttpClientRequest newRequest;
	newRequest.method = method;
	newRequest.url = url;
	newRequest.headers = headers;
	newRequest.data = data;

	_request = newRequest;
	_responsePending = true;
	_error = NoError;
	clear();

	if (_device->state() == QAbstractSocket::ConnectedState && url.host() == previousHost && url.port() == previousPort)
		sendRequest();
	else
	{
		if (_device->state() != QAbstractSocket::UnconnectedState)
			_device->disconnectFromHost();
		_device->connectToHost(url.host(), url.port());
	}
}

void Pillow::HttpClient::abort()
{
	if (!_responsePending)
	{
		qWarning("Pillow::HttpClient::abort(): called while not running.");
		return;
	}
	if (_device) _device->close();
	_responsePending = false;
	_error = AbortedError;
}

void Pillow::HttpClient::device_error(QAbstractSocket::SocketError error)
{
	if (!_responsePending)
	{
		// Errors that happen while we are not waiting for a response are ok. We'll try to
		// recover on the next time we get a request.
		return;
	}

	switch (error)
	{
	case QAbstractSocket::RemoteHostClosedError:
		_error = RemoteHostClosedError;
		break;
	default:
		_error = NetworkError;
	}

	_responsePending = false;
	emit finished();
}

void Pillow::HttpClient::device_connected()
{
	sendRequest();
}

void Pillow::HttpClient::device_readyRead()
{
	//TODO: if (!responsePending()) return;

	QByteArray data = _device->readAll();
	int consumed = inject(data);

	if (!hasError() && consumed < data.size() && responsePending())
	{
		// We had multiple responses in the buffer?
		// It was a 100 Continue since we are still response pending.
		consumed += inject(data.constData() + consumed, data.size() - consumed);
	}

	if (Pillow::HttpResponseParser::hasError())
	{
		_responsePending = false;
		_error = ResponseInvalidError;
		_device->close();
	}
}

void Pillow::HttpClient::sendRequest()
{
	//TODO: support cancelling? if (!responsePending()) return;

	QByteArray uri = _request.url.encodedPath();
	const QByteArray query = _request.url.encodedQuery();
	if (!query.isEmpty()) uri.append('?').append(query);

	Pillow::HttpHeaderCollection headers = _baseRequestHeaders;
	if (!_request.headers.isEmpty())
	{
		headers.reserve(headers.size() + _request.headers.size());
		for (int i = 0, iE = _request.headers.size(); i < iE; ++i)
			headers.append(_request.headers.at(i));
	}

	_requestWriter.write(_request.method, uri, headers, _request.data);
}

void Pillow::HttpClient::messageBegin()
{
	Pillow::HttpResponseParser::messageBegin();
}

void Pillow::HttpClient::headersComplete()
{
	Pillow::HttpResponseParser::headersComplete();
}

void Pillow::HttpClient::messageContent(const char *data, int length)
{
	Pillow::HttpResponseParser::messageContent(data, length);
	emit contentReadyRead();
}

void Pillow::HttpClient::messageComplete()
{
	if (statusCode() != 100) // Ignore 100 Continue responses.
	{
		Pillow::HttpResponseParser::messageComplete();
		_responsePending = false;
		emit finished();
	}
}
