#ifndef PILLOW_HTTPCLIENT_H
#define PILLOW_HTTPCLIENT_H

#ifndef QOBJECT_H
#include <QObject>
#endif // QOBJECT_H
#ifndef QURL_H
#include <QUrl>
#endif // QWURL_H
#ifndef PILLOW_HTTPCONNECTION_H
#include "HttpConnection.h"
#endif // PILLOW_HTTPCONNECTION_H
#ifndef http_parser_h
#include "parser/http_parser.h"
#endif // http_parser_h

class QIODevice;
class QTcpSocket;

namespace Pillow
{
	class HttpRequestWriter : public QObject
	{
		Q_OBJECT

	public:
		explicit HttpRequestWriter(QObject* parent = 0);

		inline QIODevice* device() const { return _device; }

	public:
		void get(const QByteArray& path, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection());
		void head(const QByteArray& path, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection());
		void post(const QByteArray& path, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection(), const QByteArray& data = QByteArray());
		void put(const QByteArray& path, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection(), const QByteArray& data = QByteArray());
		void deleteResource(const QByteArray& path, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection());

		void write(const QByteArray& method, const QByteArray& path, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection(), const QByteArray& data = QByteArray());

	public slots:
		void setDevice(QIODevice* device);

	private:
		QIODevice* _device;
	};

	class HttpResponseParser
	{
	public:
		HttpResponseParser();

	public:
		// The "int inject(x)" methods return the number of bytes that were consumed.
		int inject(const char* data, int length);
		inline int inject(const QByteArray& data) { return inject(data.constBegin(), data.size()); }

		// Let the parser know that the connection/device/source has closed and will not be providing more data.
		void injectEof();
		//void inject(const QIODevice* fromDevice);

		// Clear the parser back to the initial state, ready to parse a new message.
		void clear();

	public:
		// Parser status members.
		inline bool hasError() const { return !(parser.http_errno == HPE_OK || parser.http_errno == HPE_PAUSED); }
		inline http_errno error() const { return static_cast<http_errno>(parser.http_errno); }
		QByteArray errorString() const;

		// Response members.
		inline quint16 statusCode() const { return parser.status_code; }
		inline quint16 httpMajor() const { return parser.http_major; }
		inline quint16 httpMinor() const { return parser.http_minor; }
		inline Pillow::HttpHeaderCollection headers() const { return _headers; }
		inline const QByteArray& content() const { return _content; }

		inline bool shouldKeepAlive() const { return http_should_keep_alive(const_cast<http_parser*>(&parser)); }
		inline bool completesOnEof() const { return http_message_needs_eof(const_cast<http_parser*>(&parser)); }

	protected:
		virtual void messageBegin();
		virtual void headersComplete();
		virtual void messageComplete();

	private:
		static int parser_on_message_begin(http_parser* parser);
		static int parser_on_url(http_parser* parser, const char *at, size_t length);
		static int parser_on_header_field(http_parser* parser, const char *at, size_t length);
		static int parser_on_header_value(http_parser* parser, const char *at, size_t length);
		static int parser_on_headers_complete(http_parser* parser);
		static int parser_on_body(http_parser* parser, const char *at, size_t length);
		static int parser_on_message_complete(http_parser* parser);

	private:
		void pushHeader();

	private:
		http_parser parser;
		http_parser_settings parser_settings;
		QByteArray _field, _value;
		bool _lastWasValue;
		Pillow::HttpHeaderCollection _headers;
		QByteArray _content;
	};

	struct HttpClientRequest
	{
		QByteArray method;
		QUrl url;
		Pillow::HttpHeaderCollection headers;
		QByteArray data;
	};

	class HttpClient : public QObject, protected HttpResponseParser
	{
		Q_OBJECT
		Q_FLAGS(Error)

	public:
		enum Error
		{
			NoError,				// There was no error in sending and receiving the previous request (if any),
									// including the 4xx and 5xx responses with represent client and server errors.
			NetworkError,			// There was a network error (unable to connect, could not resolve host, etc.).
			ResponseInvalidError,	// The response from the server could not be parsed as a valid Http response.
			RemoteHostClosedError,  // The remote server has closed the connection before sending a full reply.
			AbortedError            // The request was aborted before a pending response was completed.
			//, HttpError?
		};

	public:
		HttpClient(QObject* parent = 0);

	public:
		// Request members.
		void get(const QUrl& url, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection());
		void head(const QUrl& url, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection());
		void post(const QUrl& url, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection(), const QByteArray& data = QByteArray());
		void put(const QUrl& url, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection(), const QByteArray& data = QByteArray());
		void deleteResource(const QUrl& url, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection());
		void request(const QByteArray& method, const QUrl& url, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection(), const QByteArray& data = QByteArray());

		void abort();

	public:
		// Response Members.
		bool responsePending() const;
		Error error() const;

		inline int statusCode() const { return static_cast<int>(HttpResponseParser::statusCode()); }
		inline Pillow::HttpHeaderCollection headers() const { return HttpResponseParser::headers(); }
		inline const QByteArray& content() const { return HttpResponseParser::content(); }

	signals:
		void finished();

	private slots:
		void device_error(QAbstractSocket::SocketError error);
		void device_connected();
		void device_readyRead();

	private:
		void sendRequest();

	protected:
		void messageBegin();
		void headersComplete();
		void messageComplete();

	private:
		QTcpSocket* _device;
		Pillow::HttpHeaderCollection _baseRequestHeaders;
		Pillow::HttpClientRequest _request;
		Pillow::HttpRequestWriter _requestWriter;
		Pillow::HttpResponseParser _responseParser;
		bool _responsePending;
		Error _error;
	};
} // namespace Pillow

#endif // PILLOW_HTTPCLIENT_H
