#include "HttpConnection.h"
#include "HttpHelpers.h"
#include "ByteArrayHelpers.h"
#include "parser/parser.h"
#include <QtCore/QIODevice>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QLocalSocket>
#include <QtCore/QVarLengthArray>

//
// Helpers
//

namespace Tokens
{
	static const QByteArray crLfToken("\r\n");
	static const QByteArray connectionToken("connection");
	static const QByteArray contentLengthToken("content-length");
	static const QByteArray contentLengthOutToken("Content-Length: ");
	static const QByteArray contentTypeToken("content-type");
	static const QByteArray expectToken("expect");
	static const QByteArray hundredDashContinueToken("100-continue");
	static const QByteArray keepAliveToken("keep-alive");
	static const QByteArray colonSpaceToken(": ");
	static const QByteArray closeToken("close");
	static const QByteArray contentTypeTextPlainTokenHeaderToken("Content-Type: text/plain\r\n");
	static const QByteArray connectionKeepAliveHeaderToken("Connection: keep-alive\r\n");
	static const QByteArray connectionCloseHeaderToken("Connection: close\r\n");
	static const QByteArray transferEncodingToken("transfer-encoding");
	static const QByteArray chunkedToken("chunked");
	static const QByteArray httpSlash11Token("HTTP/1.1");
	static const QByteArray headToken("HEAD");
}

using namespace Pillow::ByteArrayHelpers;
using namespace Tokens;

namespace Pillow
{
	struct HttpHeaderRef
	{
		int fieldPos, fieldLength, valuePos, valueLength;
		inline HttpHeaderRef(int fieldPos, int fieldLength, int valuePos, int valueLength)
			: fieldPos(fieldPos), fieldLength(fieldLength), valuePos(valuePos), valueLength(valueLength) {}
		inline HttpHeaderRef() {}
	};
}
Q_DECLARE_TYPEINFO(Pillow::HttpHeaderRef, Q_PRIMITIVE_TYPE);

#define PERCENT_DECODABLE(fieldName) \
	QByteArray _##fieldName; \
	mutable QString _##fieldName##Decoded; \
	inline const QString& fieldName##Decoded() const { if (_##fieldName##Decoded.isEmpty() && !_##fieldName.isEmpty()) _##fieldName##Decoded = Pillow::ByteArrayHelpers::percentDecode(_##fieldName); return _##fieldName##Decoded; }
#define FORWARD_PERCENT_DECODABLE(fieldName) \
	const QByteArray& Pillow::HttpConnection::fieldName() const { return d_ptr->_##fieldName; } \
	const QString& Pillow::HttpConnection::fieldName##Decoded() const { return d_ptr->fieldName##Decoded(); }

//
// HttpConnectionPrivate
//

namespace Pillow
{
	class HttpConnectionPrivate
	{
		Q_DECLARE_PUBLIC(HttpConnection)
		HttpConnection* q_ptr;

	public:
		HttpConnectionPrivate(HttpConnection* connection);

	public:
		Pillow::HttpConnection::State _state;
		QIODevice* _inputDevice,* _outputDevice;
		http_parser _parser;

		// Request fields.
		QByteArray _requestBuffer;
		QByteArray _requestMethod, _requestHttpVersion, _requestContent;
		PERCENT_DECODABLE(requestUri)
		PERCENT_DECODABLE(requestFragment)
		PERCENT_DECODABLE(requestPath)
		PERCENT_DECODABLE(requestQueryString)
		QVarLengthArray<Pillow::HttpHeaderRef, 32> _requestHeadersRef;
		Pillow::HttpHeaderCollection _requestHeaders;
		int _requestContentLength; int _requestContentLengthHeaderIndex;
		bool _requestHttp11;
		Pillow::HttpParamCollection _requestParams;

		// Response fields.
		QByteArray _responseHeadersBuffer;
		int _responseStatusCode;
		qint64 _responseContentLength, _responseContentBytesSent;
		bool _responseConnectionKeepAlive;
		bool _responseChunkedTransferEncoding;

	public:
		void initialize();
		void processInput();
		void setupRequestHeaders();
		void transitionToReceivingHeaders();
		void transitionToReceivingContent();
		void transitionToSendingHeaders();
		void transitionToSendingContent();
		void transitionToCompleted();
		void flush();
		void drain();
		void transitionToFlushing();
		void transitionToClosed();
		void writeRequestErrorResponse(int statusCode = 400); // Used internally when an error happens while receiving a request. It sends an error response to the client and closes the connection right away.

		static void parser_http_field(void *data, const char *field, size_t flen, const char *value, size_t vlen);
	};
}

Pillow::HttpConnectionPrivate::HttpConnectionPrivate(HttpConnection *connection)
	: q_ptr(connection), _state(Pillow::HttpConnection::Uninitialized), _inputDevice(0), _outputDevice(0)
{
}

inline void Pillow::HttpConnectionPrivate::initialize()
{
	memset(&_parser, 0, sizeof(http_parser));
	_parser.data = this;
	_parser.http_field = &HttpConnectionPrivate::parser_http_field;

	// Clear any leftover data from a previous potentially failed request (that would not have gone though "transitionToCompleted")
	if (_requestBuffer.capacity() <= Pillow::HttpConnection::MaximumRequestHeaderLength) _requestBuffer.data_ptr()->size = 0;
	else _requestBuffer.clear();
	_requestHeadersRef.clear();
	if (_requestParams.capacity() > 16) _requestParams.clear();
	else while(!_requestParams.isEmpty()) _requestParams.pop_back();

	// Enter the initial working state and schedule processing of any data already available on the device.
	transitionToReceivingHeaders();
	if (_inputDevice->bytesAvailable() > 0) QTimer::singleShot(0, q_ptr, SLOT(processInput()));
}

inline void Pillow::HttpConnectionPrivate::processInput()
{
	if (_state != Pillow::HttpConnection::ReceivingHeaders && _state != Pillow::HttpConnection::ReceivingContent) return;

	qint64 bytesAvailable = _inputDevice->bytesAvailable();
	if (bytesAvailable > 0)
	{
		if (_requestBuffer.capacity() < _requestBuffer.size() + bytesAvailable)
			_requestBuffer.reserve(_requestBuffer.size() + bytesAvailable + 1);
		qint64 bytesRead = _inputDevice->read(_requestBuffer.data() + _requestBuffer.size(), bytesAvailable);
		_requestBuffer.data_ptr()->size += bytesRead;
		_requestBuffer.data_ptr()->data[_requestBuffer.data_ptr()->size] = 0;
	}

	if (_state == Pillow::HttpConnection::ReceivingHeaders)
	{
		if (!_requestBuffer.isEmpty())
			thin_http_parser_execute(&_parser, _requestBuffer.constData(), _requestBuffer.size(), _parser.nread);

		if (_parser.nread > Pillow::HttpConnection::MaximumRequestHeaderLength || thin_http_parser_has_error(&_parser))
			return writeRequestErrorResponse(400); // Bad client Request!
		else if (thin_http_parser_is_finished(&_parser))
			transitionToReceivingContent();
	}
	else if (_state == Pillow::HttpConnection::ReceivingContent)
	{
		if (_requestBuffer.size() - int(_parser.body_start) >= _requestContentLength)
			transitionToSendingHeaders(); // Finished receiving the content.
	}
}

inline void Pillow::HttpConnectionPrivate::parser_http_field(void *data, const char *field, size_t flen, const char *value, size_t vlen)
{
	Pillow::HttpConnectionPrivate* request = reinterpret_cast<Pillow::HttpConnectionPrivate*>(data);
	if (flen == 14 && asciiEqualsCaseInsensitive(field, 14, "content-length", 14))
		request->_requestContentLengthHeaderIndex = request->_requestHeadersRef.size();
	const char* begin = request->_requestBuffer.constData();
	request->_requestHeadersRef.append(HttpHeaderRef(field - begin, flen, value - begin, vlen));
}

inline void Pillow::HttpConnectionPrivate::transitionToReceivingHeaders()
{
	if (_state == Pillow::HttpConnection::ReceivingHeaders) return;
	_state = Pillow::HttpConnection::ReceivingHeaders;

	thin_http_parser_init(&_parser);
	_requestContentLength = 0;
	_requestContentLengthHeaderIndex = -1;
	_requestHttp11 = false;
}

inline void Pillow::HttpConnectionPrivate::setupRequestHeaders()
{
	char* data = _requestBuffer.data();

	while (_requestHeaders.size() > _requestHeadersRef.size()) _requestHeaders.pop_back();
	if (_requestHeaders.capacity() == 0 && _requestHeadersRef.size() > 0) _requestHeaders.resize(_requestHeadersRef.size());
	else while (_requestHeaders.size() < _requestHeadersRef.size()) _requestHeaders.push_back(HttpHeader());

	HttpHeader* header = _requestHeaders.begin();
	for (const HttpHeaderRef* ref = _requestHeadersRef.data(), *refE = _requestHeadersRef.data() + _requestHeadersRef.size(); ref < refE; ++ref, ++header)
	{
		setFromRawDataAndNullterm(header->first, data, ref->fieldPos, ref->fieldLength);
		setFromRawDataAndNullterm(header->second, data, ref->valuePos, ref->valueLength);
	}
}

inline void Pillow::HttpConnectionPrivate::transitionToReceivingContent()
{
	if (_state == Pillow::HttpConnection::ReceivingContent) return;
	_state = Pillow::HttpConnection::ReceivingContent;

	setupRequestHeaders();

	bool contentLengthParseOk = true;
	if (_requestContentLengthHeaderIndex >= 0)
		_requestContentLength = _requestHeaders.at(_requestContentLengthHeaderIndex).second.toInt(&contentLengthParseOk);

	// Exit early if the client sent an incorrect or unacceptable content-length.
	if (_requestContentLength < 0)
		return writeRequestErrorResponse(400); // Invalid request: negative content length does not make sense.
	else if (_requestContentLength > Pillow::HttpConnection::MaximumRequestContentLength || !contentLengthParseOk)
		return writeRequestErrorResponse(413); // Request entity too large.

	if (_requestContentLength > 0)
	{
		if (q_ptr->requestHeaderValue(expectToken) == hundredDashContinueToken)
			_outputDevice->write("HTTP/1.1 100 Continue\r\n\r\n");// The client politely wanted to know if it could proceed with his payload. All clear!

		// Resize the request buffer right away to avoid too many reallocs later.
		// NOTE: This invalidates the request headers QByteArrays if the reallocation
		// changes the buffer's address (very likely unless the content-length is tiny).
		_requestBuffer.reserve(_parser.body_start + _requestContentLength + 1);

		// So do invalidate the request headers.
		if (_requestHeaders.size() > 0) _requestHeaders.pop_back();

		// Pump; the content may already be sitting in the buffers.
		processInput();
	}
	else
	{
		transitionToSendingHeaders(); // No content to receive. Go straight to sending headers.
	}
}

inline void Pillow::HttpConnectionPrivate::transitionToSendingHeaders()
{
	if (_state == Pillow::HttpConnection::SendingHeaders) return;
	_state = Pillow::HttpConnection::SendingHeaders;

	// Prepare and null terminate the request fields.

	if (_requestHeaders.size() != _requestHeadersRef.size())
		setupRequestHeaders();

	char* data = _requestBuffer.data();

	if (_parser.query_string_len == 0)
	{
		// The request uri has no query string, we can use the data straight because it means that inserting null after the path
		// will not destroy the uri (which does not include the fragment).
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

	if (!_requestUriDecoded.isEmpty()) _requestUriDecoded = QString();
	if (!_requestFragmentDecoded.isEmpty())_requestFragmentDecoded = QString();
	if (!_requestPathDecoded.isEmpty())_requestPathDecoded = QString();
	if (!_requestQueryStringDecoded.isEmpty())_requestQueryStringDecoded = QString();

	_requestHttp11 = _requestHttpVersion == httpSlash11Token;

	setFromRawData(_requestContent, _requestBuffer.constData(), _parser.body_start, _requestContentLength);

	// Reset our known information about the response.
	_responseContentLength = -1;   // The response content-length is initially unknown.
	_responseContentBytesSent = 0; // No content bytes transfered yet.
	_responseConnectionKeepAlive = true;
	_responseChunkedTransferEncoding = false;
	emit q_ptr->requestReady(q_ptr);
}

inline void Pillow::HttpConnectionPrivate::transitionToSendingContent()
{
	if (_state == Pillow::HttpConnection::SendingContent) return;
	_state = Pillow::HttpConnection::SendingContent;

	if (_responseHeadersBuffer.capacity() > 4096)
		_responseHeadersBuffer.clear();
	else
		_responseHeadersBuffer.data_ptr()->size = 0;

	if (_responseContentLength == 0 || _requestMethod == headToken)
		transitionToCompleted();

	if (_responseContentLength < 0 && !_responseChunkedTransferEncoding)
	{
		// The response content length needs to be specified or chunked transfer encoding used to support keep-alive. Forcefully disable it.
		_responseConnectionKeepAlive = false;
	}
}

inline void Pillow::HttpConnectionPrivate::transitionToCompleted()
{
	if (_state == Pillow::HttpConnection::Completed) return;
	if (_state == Pillow::HttpConnection::Closed)
	{
		qWarning() << "HttpConnection::transitionToCompleted called while the request is in the closed state.";
	}
	_state = Pillow::HttpConnection::Completed;
	emit q_ptr->requestCompleted(q_ptr);

	// Preserve any existing data in the request buffer that did not belong to the completed request.
	// Reuse the already allocated buffer if it is not too large.
	int remainingBytes = _requestBuffer.size() - int(_parser.body_start) - _requestContentLength;
	if (remainingBytes > 0) _requestBuffer = _requestBuffer.right(remainingBytes);
	else if (_requestBuffer.capacity() <= Pillow::HttpConnection::MaximumRequestHeaderLength) _requestBuffer.data_ptr()->size = 0;
	else _requestBuffer.clear();

	_requestHeadersRef.clear();

	if (_requestParams.capacity() > 16) _requestParams.clear();
	else while(!_requestParams.isEmpty()) _requestParams.pop_back();

	_requestContent.data_ptr()->size = 0;

	if (_responseConnectionKeepAlive)
	{
		flush(); // Done writing for this request, make sure the data is pushed right away to the client.
		transitionToReceivingHeaders();
		processInput();
	}
	else
	{
		transitionToFlushing();
	}
}

inline void Pillow::HttpConnectionPrivate::flush()
{
	if (_outputDevice != 0 && _outputDevice->bytesToWrite() > 0)
	{
		if (qobject_cast<QTcpSocket*>(_outputDevice))
			static_cast<QTcpSocket*>(_outputDevice)->flush();
		else if (qobject_cast<QLocalSocket*>(_outputDevice))
			static_cast<QLocalSocket*>(_outputDevice)->flush();
	}
}

inline void Pillow::HttpConnectionPrivate::drain()
{
	if (_state != Pillow::HttpConnection::Flushing) return;
	flush();
	if (_outputDevice != 0 && _outputDevice->bytesToWrite() == 0) transitionToClosed();
}

inline void Pillow::HttpConnectionPrivate::transitionToFlushing()
{
	// This is a transient state that is meant to fully drain the write buffers and
	// wait for all the data to make it to the kernel before closing the connection.
	if (_state == Pillow::HttpConnection::Flushing) return;
	_state = Pillow::HttpConnection::Flushing;

	drain(); // Will transition to closed also if there was no data at all to flush.
	if (_state == Pillow::HttpConnection::Flushing) // A first flush was not enough. Schedule more flushes.
		QObject::connect(_outputDevice, SIGNAL(bytesWritten(qint64)), q_ptr, SLOT(drain()));
}

inline void Pillow::HttpConnectionPrivate::transitionToClosed()
{
	if (_state == Pillow::HttpConnection::Closed) return;
	_state = Pillow::HttpConnection::Closed;

	if (_inputDevice && _inputDevice->isOpen()) _inputDevice->close();
	if (_outputDevice && (_inputDevice != _outputDevice) && _outputDevice->isOpen()) _outputDevice->close();
	emit q_ptr->closed(q_ptr);

	QObject::disconnect(_inputDevice, 0, q_ptr, 0);
	if (_inputDevice != _outputDevice) QObject::disconnect(_outputDevice, 0, q_ptr, 0);
	_inputDevice = 0;
	_outputDevice = 0;
}

void Pillow::HttpConnectionPrivate::writeRequestErrorResponse(int statusCode)
{
	if (_state == Pillow::HttpConnection::Closed)
	{
		qWarning() << "HttpConnection::writeRequestErrorResponse called while state is already 'Closed'";
		return;
	}

	qDebug() << "HttpConnection: request error. Sending http status" << statusCode << "and closing connection.";

	QByteArray _responseHeadersBuffer; _responseHeadersBuffer.reserve(1024);
	_responseHeadersBuffer.append("HTTP/1.0 ").append(HttpProtocol::StatusCodes::getStatusCodeAndMessage(statusCode)).append(crLfToken);
	_responseHeadersBuffer.append("Connection: close").append(crLfToken);
	_responseHeadersBuffer.append(crLfToken); // End of headers.
	_outputDevice->write(_responseHeadersBuffer);
	transitionToFlushing();
}


//
// HttpConnection
//

using namespace Pillow;
using namespace Pillow::ByteArrayHelpers;
using namespace Tokens;

inline void appendHeader(QByteArray& byteArray, const HttpHeader& header)
{
	byteArray.append(header.first).append(colonSpaceToken).append(header.second).append(crLfToken);
}

HttpConnection::HttpConnection(QObject* parent /*= 0*/)
	: QObject(parent), d_ptr(new Pillow::HttpConnectionPrivate(this))
{
}

HttpConnection::~HttpConnection()
{}

void HttpConnection::initialize(QIODevice* inputDevice, QIODevice* outputDevice)
{
	if (inputDevice != d_ptr->_inputDevice)
	{
		d_ptr->_inputDevice = inputDevice;

		connect(d_ptr->_inputDevice, SIGNAL(readyRead()), this, SLOT(processInput()));

		if (qobject_cast<QAbstractSocket*>(d_ptr->_inputDevice) || qobject_cast<QLocalSocket*>(d_ptr->_inputDevice))
			connect(d_ptr->_inputDevice, SIGNAL(disconnected()), this, SLOT(close()));
		else
			connect(d_ptr->_inputDevice, SIGNAL(aboutToClose()), this, SLOT(close()));
	}

	d_ptr->_outputDevice = outputDevice;
	d_ptr->initialize();
}

HttpConnection::State HttpConnection::state() const
{
	return d_ptr->_state;
}

QIODevice *HttpConnection::inputDevice() const
{
	return d_ptr->_inputDevice;
}

QIODevice *HttpConnection::outputDevice() const
{
	return d_ptr->_outputDevice;
}

void HttpConnection::processInput()
{
	d_ptr->processInput();
}

void HttpConnection::flush()
{
	d_ptr->flush();
}

void HttpConnection::drain()
{
	d_ptr->drain();
}

void HttpConnection::writeResponse(int statusCode, const HttpHeaderCollection& headers, const QByteArray& content)
{
	if (d_ptr->_state != SendingHeaders)
	{
		qWarning() << "HttpConnection::writeResponse called while state is not 'SendingHeaders', not proceeding with sending headers.";
		return;
	}

	// Calculate the Content-Length header so it can be set in WriteHeaders, unless it is already present.
	d_ptr->_responseContentLength = content.size();
	writeHeaders(statusCode, headers);
	if (!content.isEmpty() && d_ptr->_requestMethod != headToken) writeContent(content);
}

void HttpConnection::writeResponseString(int statusCode, const HttpHeaderCollection& headers, const QString& content)
{
	writeResponse(statusCode, headers, content.toUtf8());
}

void HttpConnection::writeHeaders(int statusCode, const HttpHeaderCollection& headers)
{
	if (d_ptr->_state != SendingHeaders)
	{
		qWarning() << "HttpConnection::writeHeaders called while state is not 'SendingHeaders', not proceeding with sending headers.";
		return;
	}

	const char* statusCodeAndMessage = HttpProtocol::StatusCodes::getStatusCodeAndMessage(statusCode);
	if (statusCodeAndMessage == NULL)
	{
		// Huh? Trying to send a bad status code...
		qWarning() << "HttpConnection::writeHeaders:" << statusCode << "is not a valid Http status code. Using 500 Internal Server Error instead.";
		statusCodeAndMessage = HttpProtocol::StatusCodes::getStatusMessage(500); // Internal server error.
	}
	d_ptr->_responseStatusCode = statusCode;

	if (d_ptr->_responseHeadersBuffer.capacity() == 0)
		d_ptr->_responseHeadersBuffer.reserve(1024);

	d_ptr->_responseHeadersBuffer.append(d_ptr->_requestHttpVersion).append(' ').append(statusCodeAndMessage).append(crLfToken);

	const HttpHeader* contentTypeHeader = 0;
	const HttpHeader* connectionHeader = 0;
	const HttpHeader* transferEncodingHeader = 0;

	// Grab headers that are important to us so we can check their values and consistency.
	for (const HttpHeader* header = headers.constBegin(), *headerE = headers.constEnd(); header != headerE; ++header)
	{
		if (asciiEqualsCaseInsensitive(header->first, contentLengthToken))
		{
			bool ok = false;
			d_ptr->_responseContentLength = header->second.toLongLong(&ok);
			if (!ok)
			{
				// Somebody trying to be a bad server? Pretend we don't know the length.
				qWarning() << "HttpConnection::writeHeaders: Invalid content-length header specified. Sending response as if content-length was unknown.";
				d_ptr->_responseContentLength = -1;
			}
		}
		else if (asciiEqualsCaseInsensitive(header->first, contentTypeToken)) contentTypeHeader = header;
		else if (asciiEqualsCaseInsensitive(header->first, connectionToken)) connectionHeader = header;
		else if (asciiEqualsCaseInsensitive(header->first, transferEncodingToken)) transferEncodingHeader = header;
		else
		{
			// Not a special header for us. Write it out to the buffer.
			appendHeader(d_ptr->_responseHeadersBuffer, *header);
		}
	}

	if (transferEncodingHeader && asciiEqualsCaseInsensitive(transferEncodingHeader->second, chunkedToken))
	{
		if (d_ptr->_requestHttp11)
		{
			if (d_ptr->_responseContentLength == -1)
				d_ptr->_responseChunkedTransferEncoding = true;
			else
			{
				qWarning() << "HttpConnection::writeHeaders: Using chunked transfer encoding with a known content-lenght does not make sense. Not using chunked transfer encoding.";
				transferEncodingHeader = 0; // Suppress header
			}
		}
		else
		{
			// Chunked transfer encoding is not supported on Http below 1.1.
			transferEncodingHeader = 0;
		}
	}

	// Negotiate keep-alive between client and server.
	bool clientWantsKeepAlive;

	if (d_ptr->_requestHttp11)
	{
		// Keep-Alive by default, unless "close" is specified.
		clientWantsKeepAlive = !asciiEqualsCaseInsensitive(requestHeaderValue(connectionToken), closeToken);
	}
	else
	{
		// Close by default, unless "keep-alive" is specified.
		clientWantsKeepAlive = asciiEqualsCaseInsensitive(requestHeaderValue(connectionToken), keepAliveToken);
	}

	if (clientWantsKeepAlive)
	{
		// To be able to keep the connection alive, the response length needs to be known, or chunked encoding be used.
		bool serverWantsKeepAlive = d_ptr->_responseContentLength >= 0 || d_ptr->_responseChunkedTransferEncoding;

		if (serverWantsKeepAlive && connectionHeader)
		{
			// Server is Keep-Alive by default, unless "close" is specified.
			serverWantsKeepAlive = !asciiEqualsCaseInsensitive(connectionHeader->second, closeToken);
		}

		d_ptr->_responseConnectionKeepAlive = serverWantsKeepAlive;
	}
	else
		d_ptr->_responseConnectionKeepAlive = false;

	// Automatically add essential headers.
	if (d_ptr->_responseContentLength != -1) {d_ptr-> _responseHeadersBuffer.append(contentLengthOutToken); appendNumber<int, 10>(d_ptr->_responseHeadersBuffer, d_ptr->_responseContentLength); d_ptr->_responseHeadersBuffer.append(crLfToken); }
	if (contentTypeHeader) { appendHeader(d_ptr->_responseHeadersBuffer, *contentTypeHeader); } else if (d_ptr->_responseContentLength > 0) { d_ptr->_responseHeadersBuffer.append(contentTypeTextPlainTokenHeaderToken); }
	if (!d_ptr->_requestHttp11 || !d_ptr->_responseConnectionKeepAlive) d_ptr->_responseHeadersBuffer.append(d_ptr->_responseConnectionKeepAlive ? connectionKeepAliveHeaderToken : connectionCloseHeaderToken);
	if (transferEncodingHeader) { appendHeader(d_ptr->_responseHeadersBuffer, *transferEncodingHeader); }
	d_ptr->_responseHeadersBuffer.append(crLfToken); // End of headers.
	d_ptr->_outputDevice->write(d_ptr->_responseHeadersBuffer);
	d_ptr->transitionToSendingContent();
}

void HttpConnection::writeContent(const QByteArray& content)
{
	if (d_ptr->_state != SendingContent)
	{
		qWarning() << "HttpConnection::writeContent called while state is not 'SendingContent'. Not proceeding with sending content of size" << content.size() << "bytes.";
		return;
	}
	else if (d_ptr->_responseContentLength == 0)
	{
		qWarning() << "HttpConnection::writeContent called while the specified response content-length is 0. Not proceeding with sending content of size" << content.size() << "bytes.";
		return;
	}
	else if (d_ptr->_responseContentLength > 0 && content.size() + d_ptr->_responseContentBytesSent > d_ptr->_responseContentLength)
	{
		qWarning() << "HttpConnection::writeContent called trying to send more data (" << (content.size() + d_ptr->_responseContentBytesSent) << "bytes) than the specified response content-length of" << d_ptr->_responseContentLength << "bytes.";
		return;
	}

	if (content.size() > 0 && d_ptr->_requestMethod != headToken)
	{
		d_ptr->_responseContentBytesSent += content.size();
		if (d_ptr->_responseChunkedTransferEncoding)
		{
			QByteArray buffer; appendNumber<int, 16>(buffer, content.size()); buffer.append(crLfToken);
			d_ptr->_outputDevice->write(buffer);
		}
		d_ptr->_outputDevice->write(content);

		if (d_ptr->_responseChunkedTransferEncoding)
			d_ptr->_outputDevice->write(crLfToken);

		if (d_ptr->_responseContentBytesSent == d_ptr->_responseContentLength)
			d_ptr->transitionToCompleted();
	}
}

void HttpConnection::endContent()
{
	if (d_ptr->_state != SendingContent)
	{
		qWarning() << "HttpConnection::endContent called while state is not 'SendingContent'. Not proceeding.";
		return;
	}

	if (d_ptr->_responseContentLength >= 0)
	{
		qWarning() << "HttpConnection::endContent called while the response content-length is specified. Call the close() method to forcibly end the connection without sending enough data.";
		return;
	}

	if (d_ptr->_responseChunkedTransferEncoding)
		d_ptr->_outputDevice->write("0\r\n\r\n", 5);
	else
		d_ptr->_responseConnectionKeepAlive = false;

	d_ptr->transitionToCompleted();
}

void Pillow::HttpConnection::close()
{
	if (d_ptr->_state != Closed)
		d_ptr->transitionToClosed();
}

int HttpConnection::responseStatusCode() const
{
	return d_ptr->_responseStatusCode;
}

qint64 HttpConnection::responseContentLength() const
{
	return d_ptr->_responseContentLength;
}

QByteArray HttpConnection::requestHeaderValue(const QByteArray &field)
{
	for (const HttpHeader* header = d_ptr->_requestHeaders.constBegin(), *headerE = d_ptr->_requestHeaders.constEnd(); header != headerE; ++header)
	{
		if (asciiEqualsCaseInsensitive(field, header->first))
			return header->second;
	}
	return QByteArray();
}
const Pillow::HttpParamCollection& Pillow::HttpConnection::requestParams()
{
	if (d_ptr->_requestParams.isEmpty() && !d_ptr->_requestQueryString.isEmpty())
	{
		// The params have not yet been initialized. Parse them.
		const char paramDelimiter = '&', keyValueDelimiter = '=';
		for (const char* c = d_ptr->_requestQueryString.constBegin(), *cE = d_ptr->_requestQueryString.constEnd(); c < cE;)
		{
			const char *paramEnd, *keyEnd;
			for (paramEnd = c; paramEnd < cE; ++paramEnd) if (*paramEnd == paramDelimiter) break; // Find the param delimiter, or the end of string.
			for (keyEnd = c; keyEnd < paramEnd; ++keyEnd) if (*keyEnd == keyValueDelimiter) break; // Find the key value delimiter, or the end of the param.

			if (keyEnd < paramEnd)
			{
				// Key-value pair.
				d_ptr->_requestParams << HttpParam(percentDecode(c, keyEnd - c), percentDecode(keyEnd + 1, paramEnd - (keyEnd + 1)));
			}
			else
			{
				// Key without value.
				d_ptr->_requestParams << HttpParam(percentDecode(c, paramEnd - c), QString());
			}
			c = paramEnd + 1;
		}
	}
	return d_ptr->_requestParams;
}

QString Pillow::HttpConnection::requestParamValue(const QString &name)
{
	requestParams();
	for (int i = 0, iE = d_ptr->_requestParams.size(); i < iE; ++i)
	{
		const HttpParam& param = d_ptr->_requestParams.at(i);
		if (param.first.compare(name, Qt::CaseInsensitive) == 0)
			return param.second;
	}
	return QString();
}

void Pillow::HttpConnection::setRequestParam(const QString &name, const QString &value)
{
	requestParams();
	for (int i = 0, iE = d_ptr->_requestParams.size(); i < iE; ++i)
	{
		const HttpParam& param = d_ptr->_requestParams.at(i);
		if (param.first.compare(name, Qt::CaseInsensitive) == 0)
		{
			d_ptr->_requestParams[i] = HttpParam(name, value);
			return;
		}
	}
	d_ptr->_requestParams << HttpParam(name, value);
}

QHostAddress HttpConnection::remoteAddress() const
{
	return qobject_cast<QAbstractSocket*>(d_ptr->_inputDevice) ? static_cast<QAbstractSocket*>(d_ptr->_inputDevice)->peerAddress() : QHostAddress();
}

const QByteArray &HttpConnection::requestMethod() const
{
	return d_ptr->_requestMethod;
}

FORWARD_PERCENT_DECODABLE(requestUri)
FORWARD_PERCENT_DECODABLE(requestFragment)
FORWARD_PERCENT_DECODABLE(requestPath)
FORWARD_PERCENT_DECODABLE(requestQueryString)

const QByteArray &HttpConnection::requestHttpVersion() const
{
	return d_ptr->_requestHttpVersion;
}

const QByteArray &HttpConnection::requestContent() const
{
	return d_ptr->_requestContent;
}

const HttpHeaderCollection &HttpConnection::requestHeaders() const
{
	return d_ptr->_requestHeaders;
}
