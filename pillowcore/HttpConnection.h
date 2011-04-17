#ifndef _PILLOW_HTTPCONNECTION_H_
#define _PILLOW_HTTPCONNECTION_H_

#include <QObject>
#include "parser/parser.h"
#include <QtNetwork/QHostAddress>
class QIODevice;

namespace Pillow
{
	//
	// HttpConnection
	//

	typedef QPair<QByteArray, QByteArray> HttpHeader;
	typedef QVector<HttpHeader> HttpHeaderCollection;
	typedef QPair<QString, QString> HttpParam;
	typedef QVector<HttpParam> HttpParamCollection;

	struct HttpHeaderRef 
	{ 
		int fieldPos, fieldLength, valuePos, valueLength; 
		HttpHeaderRef(int fieldPos, int fieldLength, int valuePos, int valueLength) 
			: fieldPos(fieldPos), fieldLength(fieldLength), valuePos(valuePos), valueLength(valueLength) {}
		HttpHeaderRef() {}
	};

	class HttpConnection : public QObject
	{
		Q_OBJECT
		QIODevice* _inputDevice,* _outputDevice;
		http_parser _parser;

	public:
		enum State { Uninitialized, ReceivingHeaders, ReceivingContent, SendingHeaders, SendingContent, Completed, Flushing, Closed };
		enum { MaximumRequestHeaderLength = 32 * 1024 };
		enum { MaximumRequestContentLength = 128 * 1024 * 1024 };
		Q_ENUMS(State);

	private:
		State _state;

		// Request fields.
		QByteArray _requestBuffer;
		QByteArray _requestMethod, _requestUri, _requestFragment, _requestPath, _requestQueryString, _requestHttpVersion, _requestContent;
		QVector<HttpHeaderRef> _requestHeadersRef;
		HttpHeaderCollection _requestHeaders;
		int _requestContentLength;
		HttpParamCollection _requestParams;

		// Response fields.
		QByteArray _responseHeadersBuffer;
		int _responseStatusCode;
		qint64 _responseContentLength, _responseContentBytesSent;
		bool _responseConnectionKeepAlive;

	private:
		void transitionToReceivingHeaders();
		void transitionToReceivingContent();
		void transitionToSendingHeaders();
		void transitionToSendingContent();
		void transitionToCompleted();
		void transitionToFlushing();
		void transitionToClosed();
		void writeRequestErrorResponse(int statusCode = 400); // Used internally when an error happens while receiving a request. It sends an error response to the client and closes the connection right away.
		static void parser_http_field(void *data, const char *field, size_t flen, const char *value, size_t vlen);

	private slots:
		void processInput();
		void flush();
		void drain();

	public:
		HttpConnection(QObject* parent = 0);
		HttpConnection(QIODevice* inputOutputDevice, QObject* parent = 0);
		HttpConnection(QIODevice* inputDevice, QIODevice* outputDevice, QObject* parent = 0);
		~HttpConnection();

		void initialize(QIODevice* inputDevice, QIODevice* outputDevice);
		
		inline QIODevice* inputDevice() const { return _inputDevice; }
		inline QIODevice* outputDevice() const { return _outputDevice; }
		inline State state() const { return _state; }

		QHostAddress remoteAddress() const;

		// Request members. Note: the underlying shared QByteArray data remains valid until the completed() signal
		// is emitted. Call detach() on the QByteArrays if you wish to create a deep copy of the data and keep it longer.
		inline const QByteArray& requestMethod() const { return _requestMethod; }
		inline const QByteArray& requestUri() const { return _requestUri; }
		inline const QByteArray& requestFragment() const { return _requestFragment; }
		inline const QByteArray& requestPath() const { return _requestPath; }
		inline const QByteArray& requestQueryString() const { return _requestQueryString; }
		inline const QByteArray& requestHttpVersion() const { return _requestHttpVersion; }
		inline const HttpHeaderCollection& requestHeaders() const { return _requestHeaders; }
		QByteArray getRequestHeaderValue(const QByteArray& field);
		inline const QByteArray& requestContent() const { return _requestContent; }
		
		const HttpParamCollection& requestParams();
		QString getRequestParam(const QString& name);
		void setRequestParam(const QString& name, const QString& value);

		// Response members.
		int responseStatusCode() const { return _responseStatusCode; }
		qint64 responseContentLength() const { return _responseContentLength; }

	public slots:
		void writeResponse(int statusCode = 200, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection(), const QByteArray& content = QByteArray());
		void writeResponseString(int statusCode = 200, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection(), const QString& content = QString());
		void writeHeaders(int statusCode = 200, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection());
		void writeContent(const QByteArray& content);
		void close(); // Close communication channels right away, no matter if a response was sent or not.

	signals:
		void requestReady(Pillow::HttpConnection* self);     // The request is ready to be processed, all request headers and content have been received.
		void requestCompleted(Pillow::HttpConnection* self); // The response is completed, all response headers and content have been sent.
		void closed(Pillow::HttpConnection* self);			 // The connection is closing, no further requests will arrive on this object.
	};
}

#endif // _PILLOW_HTTPCONNECTION_H_
