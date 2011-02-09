#include "HttpRequest.h"
#include "HttpHelpers.h"
#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QIODevice>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QLocalSocket>
using namespace Pillow;

inline void setFromRawDataAndNullterm(QByteArray& target, char* data, int start, int length)
{
	// Reminder: switching between a deep copy/unshared target QByteArray and a shared data QByteArray
	// will *not* cause memory leaks as Qt allocates and frees the QByteArray control data+buffer in one block.
	// So the only downside that will happen if an unshared QByteArray is altered to share data is a few bytes wasted
	// until the QByteArray control block releases the control data + the unshared buffer at some point in the future.

	if (target.data_ptr()) target.data_ptr()->alloc = 0;
	if (length == 0)
		target.setRawData("", 0);
	else
	{
		target.setRawData(data + start, length);
		*(data + start + length) = 0; // Null terminate the string.
	}
}

template <typename Integer>
inline void appendNumber(QByteArray& target, const Integer number)
{
	int start = target.size();
	Integer n = number; if (n < 0) n = -n;
	do { target.append(char(n % 10) + '0');	} while ((n /= 10) > 0);
	if (number < 0) target.append('-');

	// Reverse the string we've just output
	int end = target.size() - 1;
	char* data = target.data();
	while (start < end)
	{
		char c = data[start];
		data[start] = data[end];
		data[end] = c;
		++start; --end;
	}
}

//
// HttpRequest
//

static const int responseHeadersBufferRecyclingCapacity = 4096; // The capacity above which the response buffer will not be preserved between requests.
static const QByteArray crLfToken("\r\n");
static const QByteArray connectionToken("connection");
static const QByteArray contentLengthToken("content-length");
static const QByteArray contentTypeToken("content-type");
static const QByteArray expectToken("expect");
static const QByteArray hundredDashContinueToken("100-continue");
static const QByteArray keepAliveToken("keep-alive");

HttpRequest::HttpRequest(QObject* parent /*= 0*/)
	: QObject(parent), _inputDevice(NULL), _outputDevice(NULL), _state(Uninitialized)
{
}

HttpRequest::HttpRequest(QIODevice* inputOutputDevice, QObject* parent /* = 0 */)
	: QObject(parent), _inputDevice(NULL), _outputDevice(NULL), _state(Uninitialized)
{
	initialize(inputOutputDevice, inputOutputDevice);
}

HttpRequest::HttpRequest(QIODevice* inputDevice, QIODevice* outputDevice, QObject* parent /*= 0*/)
	: QObject(parent), _inputDevice(NULL), _outputDevice(NULL), _state(Uninitialized)
{
	initialize(inputDevice, outputDevice);
}

HttpRequest::~HttpRequest()
{}

void HttpRequest::initialize(QIODevice* inputDevice, QIODevice* outputDevice)
{		
	memset(&_parser, 0, sizeof(http_parser));
	_parser.data = this;
	_parser.http_field = &HttpRequest::parser_http_field;
	_requestHeadersRef.reserve(16);

	if (inputDevice != _inputDevice)
	{
		_inputDevice = inputDevice;

		connect(_inputDevice, SIGNAL(readyRead()), this, SLOT(processInput()));
	
		if (qobject_cast<QAbstractSocket*>(_inputDevice) || qobject_cast<QLocalSocket*>(_inputDevice))
			connect(_inputDevice, SIGNAL(disconnected()), this, SLOT(close()));
		else
			connect(_inputDevice, SIGNAL(aboutToClose()), this, SLOT(close()));
	}
	
	_outputDevice = outputDevice;

	// Enter the initial working state and schedule processing of any data already available on the device.
	transitionToReceivingHeaders();
	if (_inputDevice->bytesAvailable() > 0) QTimer::singleShot(0, this, SLOT(processInput()));
}

void HttpRequest::processInput()
{
	if (_state != ReceivingHeaders && _state != ReceivingContent) return;
	
	qint64 bytesAvailable = _inputDevice->bytesAvailable();
	if (bytesAvailable > 0)
	{
		if (_requestBuffer.capacity() < _requestBuffer.size() + bytesAvailable)
			_requestBuffer.reserve(_requestBuffer.size() + bytesAvailable + 1);
		qint64 bytesRead = _inputDevice->read(_requestBuffer.data() + _requestBuffer.size(), bytesAvailable);
		_requestBuffer.data_ptr()->size += bytesRead;
		_requestBuffer.data_ptr()->data[_requestBuffer.data_ptr()->size] = 0;
	}
	
	if (_state == ReceivingHeaders)
	{
		if (!_requestBuffer.isEmpty())
		{
			if (_requestBuffer.size() > MaximumRequestHeaderLength)
				return writeRequestErrorResponse(400); // Bad client Request!
			else
				thin_http_parser_execute(&_parser, _requestBuffer.constData(), _requestBuffer.size(), _parser.nread);
		}

		if (thin_http_parser_is_finished(&_parser))
			transitionToReceivingContent();
		else if (thin_http_parser_has_error(&_parser))
			return writeRequestErrorResponse(400); // Bad client Request!
	}
	else if (_state == ReceivingContent)
	{
		if (_requestBuffer.size() - int(_parser.body_start) >= _requestContentLength)
			transitionToSendingHeaders(); // Finished receiving the content.
	}
}

void HttpRequest::transitionToReceivingHeaders()
{
	if (_state == ReceivingHeaders) return;
	_state = ReceivingHeaders;
	
	thin_http_parser_init(&_parser);
	_requestContentLength = 0;
}

void HttpRequest::transitionToReceivingContent()
{
	if (_state == ReceivingContent) return;
	_state = ReceivingContent;

	char* data = _requestBuffer.data();

	// Prepare and null terminate the fields.
	if (_parser.query_string_len == 0)
	{
		// The request uri has no query string, we can use the data straight because it means that inserting null after the path
		// will not destroy the uri.
		setFromRawDataAndNullterm(_requestUri, data, _parser.request_uri_start, _parser.request_uri_len);
	}
	else
	{
		// Create deep copy for requestUri; it would otherwise get a null inserted after the path.
		_requestUri = QByteArray(data + _parser.request_uri_start, _parser.request_uri_len);
	}
	setFromRawDataAndNullterm(_requestMethod, data, _parser.request_method_start, _parser.request_method_len);
	setFromRawDataAndNullterm(_requestFragment, data, _parser.fragment_start, _parser.fragment_len);
	setFromRawDataAndNullterm(_requestPath, data, _parser.request_path_start, _parser.request_path_len);
	setFromRawDataAndNullterm(_requestQueryString, data, _parser.query_string_start, _parser.query_string_len);
	setFromRawDataAndNullterm(_requestHttpVersion, data, _parser.http_version_start, _parser.http_version_len);
	
	while (_requestHeaders.size() > _requestHeadersRef.size()) _requestHeaders.pop_back();
	if (_requestHeaders.size() != _requestHeadersRef.size()) _requestHeaders.resize(_requestHeadersRef.size());
	
	for (int i = 0, iE = _requestHeadersRef.size(); i < iE; ++i)
	{
		const HttpHeaderRef& ref = _requestHeadersRef.at(i);
		HttpHeader& header = _requestHeaders[i];
		setFromRawDataAndNullterm(header.first, data, ref.fieldPos, ref.fieldLength);
		setFromRawDataAndNullterm(header.second, data, ref.valuePos, ref.valueLength);
	}
	
	QByteArray requestContentLengthValue = getRequestHeaderValue(contentLengthToken); bool parseOk = true;
	_requestContentLength = requestContentLengthValue.isEmpty() ? 0 : requestContentLengthValue.toInt(&parseOk);

	if (_requestContentLength < 0)
		return writeRequestErrorResponse(400); // Invalid request: negative content length does not make sense.
	else if (_requestContentLength > MaximumRequestContentLength || !parseOk)
		return writeRequestErrorResponse(413); // Request entity too large.
	else if (_requestContentLength == 0)
		transitionToSendingHeaders(); // No content to receive. Go straight to sending headers.
	else if (getRequestHeaderValue(expectToken) == hundredDashContinueToken)
		_outputDevice->write("HTTP/1.1 100 Continue\r\n\r\n");// The client politely wanted to know if it could proceed with his payload. All clear!
	
	processInput();
}

void HttpRequest::transitionToSendingHeaders()
{
	if (_state == SendingHeaders) return;
	_state = SendingHeaders;

	// Reset our known information about the response.
	_responseContentLength = -1;   // The response content-length is initially unknown.
	_responseContentBytesSent = 0; // No content bytes transfered yet.
	_responseConnectionKeepAlive = true;
	emit ready(this);
}

void HttpRequest::transitionToSendingContent()
{
	if (_state == SendingContent) return;
	_state= SendingContent;

	if (_responseHeadersBuffer.capacity() > responseHeadersBufferRecyclingCapacity)
		_responseHeadersBuffer.clear();
	else
		_responseHeadersBuffer.data_ptr()->size = 0;

	if (_responseContentLength == 0 || _requestMethod == "HEAD")
		transitionToCompleted();

	if (_responseContentLength < 0)
	{
		// The response content length needs to be known to support keep-alive. Forcefully disable it.
		_responseConnectionKeepAlive = false;
	}
}

void HttpRequest::transitionToCompleted()
{
	if (_state == Completed)  return;
	if (_state == Closed)
	{
		qWarning() << "HttpRequest::transitionToCompleted called while the request is in the closed state.";
	}
	_state = Completed;
	emit completed(this);
	
	// Preserve any existing data in the request buffer that did not belong to the completed request.
	// Reuse the already allocated buffer if it is not too large.
	int remainingBytes = _requestBuffer.size() - int(_parser.body_start) - _requestContentLength;
	if (remainingBytes > 0) _requestBuffer = _requestBuffer.right(remainingBytes);
	else if (_requestBuffer.capacity() <= MaximumRequestHeaderLength) _requestBuffer.data_ptr()->size = 0;
	else _requestBuffer.clear();

	if (_requestHeadersRef.capacity() > 16) _requestHeadersRef.clear();
	else while (!_requestHeadersRef.isEmpty()) _requestHeadersRef.pop_back();
	
	if (_requestParams.capacity() > 16) _requestParams.clear();
	else while(!_requestParams.isEmpty()) _requestParams.pop_back();

	if (_responseConnectionKeepAlive)
	{
		flush(); // Done writing for this request, make sure the data is pushed right away to the client. 
		transitionToReceivingHeaders();
		processInput();
	}
	else
		transitionToFlushing();
}

void HttpRequest::transitionToFlushing()
{
	// This is a transient state that is meant to fully drain the write buffers and
	// wait for all the data to make it to the kernel before closing the connection.
	if (_state == Flushing) return;
	_state = Flushing;

	drain(); // Will transition to closed also if there was no data at all to flush.
	if (_state == Flushing) // A first flush was not enough. Schedule more flushes.
		connect(_outputDevice, SIGNAL(bytesWritten(qint64)), this, SLOT(drain()));
}

void HttpRequest::transitionToClosed()
{
	if (_state == Closed) return;
	_state = Closed;

	if (_inputDevice && _inputDevice->isOpen()) _inputDevice->close();
	if (_outputDevice && (_inputDevice != _outputDevice) && _outputDevice->isOpen()) _outputDevice->close();
	emit closed(this);
	
	disconnect(_inputDevice, NULL, this, NULL);
	_inputDevice = NULL;
	_outputDevice = NULL;
}

void HttpRequest::flush()
{
	if (_outputDevice->bytesToWrite() > 0)
	{
		if (qobject_cast<QTcpSocket*>(_outputDevice)) static_cast<QTcpSocket*>(_outputDevice)->flush();
		else if (qobject_cast<QLocalSocket*>(_outputDevice)) static_cast<QLocalSocket*>(_outputDevice)->flush();
	}
}

void HttpRequest::drain()
{
	if (_state != Flushing) return;
	flush();
	if (_outputDevice->bytesToWrite() == 0) transitionToClosed();
}

void HttpRequest::writeRequestErrorResponse(int statusCode)
{
	if (_state == Closed)
	{
		qWarning() << "HttpRequest::writeRequestErrorResponse called while state is already 'Closed'";
		return;
	}

	qDebug() << "HttpRequest: request error! (sending http status" << statusCode << "and closing connection.)";

	QByteArray _responseHeadersBuffer; _responseHeadersBuffer.reserve(1024);
	_responseHeadersBuffer.append("HTTP/1.0 ").append(HttpProtocol::StatusCodes::getStatusCodeAndMessage(statusCode)).append(crLfToken);
	_responseHeadersBuffer.append("Connection: close").append(crLfToken);
	_responseHeadersBuffer.append(crLfToken); // End of headers.
	_outputDevice->write(_responseHeadersBuffer);
	transitionToFlushing();
}

void HttpRequest::writeResponse(int statusCode, const HttpHeaderCollection& headers, const QByteArray& content)
{
	if (_state != SendingHeaders)
	{
		qWarning() << "HttpRequest::writeResponse called while state is not 'SendingHeaders', not proceeding with sending headers.";
		return;
	}

	// Calculate the Content-Length header so it can be set in WriteHeaders, unless it is already present.
	_responseContentLength = content.size();
	writeHeaders(statusCode, headers);
	if (!content.isEmpty() && _requestMethod != "HEAD") writeContent(content);
}

void HttpRequest::writeResponseString(int statusCode, const HttpHeaderCollection& headers, const QString& content)
{
	writeResponse(statusCode, headers, content.toUtf8());
}

void HttpRequest::writeHeaders(int statusCode, const HttpHeaderCollection& headers)
{
	if (_state != SendingHeaders)
	{
		qWarning() << "HttpRequest::writeHeaders called while state is not 'SendingHeaders', not proceeding with sending headers.";
		return;
	}

	const char* statusCodeAndMessage = HttpProtocol::StatusCodes::getStatusCodeAndMessage(statusCode);
	if (statusCodeAndMessage == NULL)
	{
		// Huh? Trying to send a bad status code...
		qWarning() << "HttpRequest::writeHeaders:" << statusCode << "is not a valid Http status code.";
		statusCodeAndMessage = HttpProtocol::StatusCodes::getStatusMessage(500); // Internal server error.
	}
	_responseStatusCode = statusCode;

	if (_responseHeadersBuffer.capacity() == 0)
		_responseHeadersBuffer.reserve(2048);

	_responseHeadersBuffer.append(_requestHttpVersion).append(' ').append(statusCodeAndMessage).append(crLfToken);

	bool contentLengthFound = false;
	bool contentTypeFound = false;
	bool connectionFound = false, connectionKeepAlive = true; // Http 1.1 defaults to keep-alive.

	for (int i = 0, iE = headers.size(); i < iE; ++i)
	{
		const HttpHeader& header = headers.at(i);
		QByteArray field = header.first.toLower();
		if (!contentLengthFound && field == contentLengthToken) { contentLengthFound = true; _responseContentLength = header.second.toLongLong(); }
		else if (!contentTypeFound && field == contentTypeToken) contentTypeFound = true;
		else if (!connectionFound && field == connectionToken) { connectionFound = true; connectionKeepAlive = header.second.toLower() == keepAliveToken; }

		_responseHeadersBuffer.append(header.first).append(": ").append(header.second).append(crLfToken);
	}

	if (connectionKeepAlive)
	{
		// We'd like to keep the connection alive... Check if the client would rather have the connection closed.
		QByteArray requestConnectionValue = getRequestHeaderValue(connectionToken);

		if (_requestHttpVersion == "HTTP/1.0")
		{
			// HTTP/1.0 defaults to "close" unless it is "keep-alive".
			connectionKeepAlive = qstrnicmp(requestConnectionValue.constData(), keepAliveToken.constData(), keepAliveToken.size()) == 0;
		}
		else
		{
			// HTTP/1.1 defaults to "keep-alive" unless it is "close".
			connectionKeepAlive = !(qstrnicmp(requestConnectionValue.constData(), "close", 5) == 0);
		}

		if (!connectionKeepAlive) connectionFound = false; // The client wants the connection closed. Make sure we output the connection header.
	}

	_responseConnectionKeepAlive = connectionKeepAlive;

	// Automatically add essential headers.
	if (!contentLengthFound) { _responseHeadersBuffer.append("Content-Length: "); appendNumber(_responseHeadersBuffer, _responseContentLength); _responseHeadersBuffer.append(crLfToken); }
	if (!contentTypeFound && (contentLengthFound || _responseContentLength > 0)) _responseHeadersBuffer.append("Content-Type: text/plain").append(crLfToken);
	if (!connectionFound) _responseHeadersBuffer.append(connectionKeepAlive ? "Connection: keep-alive": "Connection: close").append(crLfToken);
	_responseHeadersBuffer.append(crLfToken); // End of headers.
	_outputDevice->write(_responseHeadersBuffer);
	transitionToSendingContent();
}

void HttpRequest::writeContent(const QByteArray& content)
{
	if (_state != SendingContent)
	{
		qWarning() << "HttpRequest::writeContent called while state is not 'SendingContent'. Not proceeding with sending content of size" << content.size() << "bytes.";
		return;
	}
	else if (_responseContentLength == 0)
	{
		qWarning() << "HttpRequest::writeContent called while the specified response content-length is 0. Not proceeding with sending content of size" << content.size() << "bytes.";
		return;
	}
	else if (_responseContentLength > 0 && content.size() + _responseContentBytesSent > _responseContentLength)
	{
		qWarning() << "HttpRequest::writeContent called trying to send more data (" << (content.size() + _responseContentBytesSent) << "bytes) than the specified response content-length of" << _responseContentLength << "bytes.";
		return;
	}

	if (content.size() > 0 && _requestMethod != "HEAD")
	{
		_responseContentBytesSent += content.size();
		_outputDevice->write(content);

		if (_responseContentBytesSent == _responseContentLength)
			transitionToCompleted();
	}
}

void Pillow::HttpRequest::close()
{
	if (_state != Closed)
		transitionToClosed();
}

QByteArray HttpRequest::getRequestHeaderValue(const QByteArray &field)
{
	for (int i = 0, iE = _requestHeaders.size(); i < iE; ++i)
	{
		const HttpHeader& header = _requestHeaders.at(i);
		if (field.size() == header.first.size() && qstrnicmp(field.constData(), header.first.constData(), field.size()) == 0)
			return header.second;
	}
	return QByteArray();
}

QByteArray HttpRequest::requestContent() const
{
	if (_requestContentLength == 0)
		return QByteArray();
	else if (int(_parser.body_start) < _requestBuffer.size())
		return _requestBuffer.mid(_parser.body_start, _requestContentLength);

	return QByteArray();
}

const Pillow::HttpParamCollection& Pillow::HttpRequest::requestParams()
{
	if (_requestParams.isEmpty() && !_requestQueryString.isEmpty())
	{
		// The params have not yet been initialized. Parse them.
		int nameStartPos = 0, valueStartPos = 0, valueEndPos = 0;
		QByteArray name, value;
		while ((valueStartPos = _requestQueryString.indexOf('=', nameStartPos + 1)) > 0)
		{
			valueEndPos = _requestQueryString.indexOf('&', valueStartPos + 1);
			if (valueEndPos == -1) valueEndPos = _requestQueryString.size();
			
			name.setRawData(_requestQueryString.constData() + nameStartPos, valueStartPos - nameStartPos);
			value.setRawData(_requestQueryString.constData() + valueStartPos + 1, valueEndPos - valueStartPos - 1);
			_requestParams << HttpParam(QUrl::fromPercentEncoding(name), QUrl::fromPercentEncoding(value));
			
			nameStartPos = valueEndPos + 1;	
			name.data_ptr()->alloc = 0; value.data_ptr()->alloc = 0;
		}
	}
	return _requestParams;
}

QString Pillow::HttpRequest::getRequestParam(const QString &name)
{
	requestParams();
	for (int i = 0, iE = _requestParams.size(); i < iE; ++i)
	{
		const HttpParam& param = _requestParams.at(i);
		if (param.first.compare(name, Qt::CaseInsensitive) == 0)
			return param.second;
	}
	return QString();
}

void Pillow::HttpRequest::setRequestParam(const QString &name, const QString &value)
{
	requestParams();
	for (int i = 0, iE = _requestParams.size(); i < iE; ++i)
	{
		const HttpParam& param = _requestParams.at(i);
		if (param.first.compare(name, Qt::CaseInsensitive) == 0)
		{
			_requestParams[i] = HttpParam(name, value);
			return;
		}
	}
	_requestParams << HttpParam(name, value);
}

QHostAddress HttpRequest::remoteAddress() const
{
	return qobject_cast<QAbstractSocket*>(_inputDevice) ? static_cast<QAbstractSocket*>(_inputDevice)->peerAddress() : QHostAddress();
}

void HttpRequest::parser_http_field(void *data, const char *field, size_t flen, const char *value, size_t vlen)
{	
	HttpRequest* request = reinterpret_cast<HttpRequest*>(data);
	const char* begin = request->_requestBuffer.constData();
	request->_requestHeadersRef.append(HttpHeaderRef(field - begin, flen, value - begin, vlen));
}
