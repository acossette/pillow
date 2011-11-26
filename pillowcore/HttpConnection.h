#ifndef _PILLOW_HTTPCONNECTION_H_
#define _PILLOW_HTTPCONNECTION_H_

#include <QObject>
#include "parser/parser.h"
#include <QtNetwork/QHostAddress>
#include <QtCore/QVector>
class QIODevice;

namespace Pillow
{
    typedef QPair<QByteArray, QByteArray> HttpHeader;
    typedef QVector<HttpHeader> HttpHeaderCollection;
    typedef QPair<QString, QString> HttpParam;
    typedef QVector<HttpParam> HttpParamCollection;
    
    struct HttpHeaderRef 
    { 
        int fieldPos, fieldLength, valuePos, valueLength; 
        inline HttpHeaderRef(int fieldPos, int fieldLength, int valuePos, int valueLength) 
            : fieldPos(fieldPos), fieldLength(fieldLength), valuePos(valuePos), valueLength(valueLength) {}
        inline HttpHeaderRef() {}
    };
}
Q_DECLARE_TYPEINFO(Pillow::HttpHeaderRef, Q_PRIMITIVE_TYPE);


namespace Pillow
{
	//
	// HttpConnection
	//

	class HttpConnection : public QObject
	{
		Q_OBJECT
		Q_PROPERTY(State state READ state)		
		Q_PROPERTY(QByteArray requestMethod READ requestMethod)
		Q_PROPERTY(QByteArray requestUri READ requestUri)
		Q_PROPERTY(QByteArray requestPath READ requestPath)
		Q_PROPERTY(QByteArray requestQueryString READ requestQueryString)
		Q_PROPERTY(QByteArray requestFragment READ requestFragment)
		Q_PROPERTY(QByteArray requestHttpVersion READ requestHttpVersion)
		Q_PROPERTY(QByteArray requestContent READ requestContent)
				
	public:
		enum State { Uninitialized, ReceivingHeaders, ReceivingContent, SendingHeaders, SendingContent, Completed, Flushing, Closed };
		enum { MaximumRequestHeaderLength = 32 * 1024 };
		enum { MaximumRequestContentLength = 128 * 1024 * 1024 };
		Q_ENUMS(State);

	private:
		State _state;
		QIODevice* _inputDevice,* _outputDevice;
		http_parser _parser;
	
		// Request fields.
		QByteArray _requestBuffer;
		QByteArray _requestMethod, _requestUri, _requestFragment, _requestPath, _requestQueryString, _requestHttpVersion, _requestContent;
		mutable QString _requestUriDecoded, _requestFragmentDecoded, _requestPathDecoded, _requestQueryStringDecoded;
		QVector<Pillow::HttpHeaderRef> _requestHeadersRef;
		Pillow::HttpHeaderCollection _requestHeaders;
		int _requestContentLength;
        bool _requestHttp11;
		Pillow::HttpParamCollection _requestParams;

		// Response fields.
		QByteArray _responseHeadersBuffer;
		int _responseStatusCode;
		qint64 _responseContentLength, _responseContentBytesSent;
		bool _responseConnectionKeepAlive;
        bool _responseChunkedTransferEncoding;

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

		// Request members. Note: the underlying shared QByteArray data remains valid until either the completed() 
		// or closed()  signals is emitted. Call detach() on your copy of the QByteArrays if you wish to keep it longer.
		inline const QByteArray& requestMethod() const { return _requestMethod; }
		inline const QByteArray& requestUri() const { return _requestUri; }
		inline const QByteArray& requestFragment() const { return _requestFragment; }
		inline const QByteArray& requestPath() const { return _requestPath; }
		inline const QByteArray& requestQueryString() const { return _requestQueryString; }
		inline const QByteArray& requestHttpVersion() const { return _requestHttpVersion; }
		inline const QByteArray& requestContent() const { return _requestContent; }

		// Request members, decoded version. Use those rather than manually decoding the raw data returned by the methods 
		// above when decoded values are desired (they are cached).
		const QString& requestUriDecoded() const;
		const QString& requestFragmentDecoded() const;
		const QString& requestPathDecoded() const;
		const QString& requestQueryStringDecoded() const;
		
	public slots:
		// Request headers.
		inline const Pillow::HttpHeaderCollection& requestHeaders() const { return _requestHeaders; }
		QByteArray requestHeaderValue(const QByteArray& field);
		
		// Request params.
		const Pillow::HttpParamCollection& requestParams();
		QString requestParamValue(const QString& name);
		void setRequestParam(const QString& name, const QString& value);

		// Response members.
		void writeResponse(int statusCode = 200, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection(), const QByteArray& content = QByteArray());
		void writeResponseString(int statusCode = 200, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection(), const QString& content = QString());
		void writeHeaders(int statusCode = 200, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection());
		void writeContent(const QByteArray& content);
        void endContent();

		int responseStatusCode() const { return _responseStatusCode; }
		qint64 responseContentLength() const { return _responseContentLength; }

		void close(); // Close communication channels right away, no matter if a response was sent or not.

	signals:
		void requestReady(Pillow::HttpConnection* self);     // The request is ready to be processed, all request headers and content have been received.
		void requestCompleted(Pillow::HttpConnection* self); // The response is completed, all response headers and content have been sent.
		void closed(Pillow::HttpConnection* self);			 // The connection is closing, no further requests will arrive on this object.
	};
}

#endif // _PILLOW_HTTPCONNECTION_H_
