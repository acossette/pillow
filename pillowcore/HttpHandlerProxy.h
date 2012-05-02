#ifndef _PILLOW_HTTPHANDLERPROXY_H_
#define _PILLOW_HTTPHANDLERPROXY_H_

#include "HttpHandler.h"
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>

namespace Pillow 
{
	class ElasticNetworkAccessManager;
	class HttpHandlerProxyPipe;
	
	class HttpHandlerProxy : public Pillow::HttpHandler
	{
		Q_OBJECT
		QUrl _proxiedUrl;
		
	protected:
		ElasticNetworkAccessManager* _networkAccessManager;
		
	public:
		HttpHandlerProxy(QObject *parent = 0);
		HttpHandlerProxy(const QUrl &proxiedUrl, QObject *parent = 0);
		
		const QUrl& proxiedUrl() const { return _proxiedUrl; }
		void setProxiedUrl(const QUrl& proxiedUrl);
		
		inline ElasticNetworkAccessManager *networkAccessManager() const { return _networkAccessManager; }
		
	protected:
		virtual QNetworkReply* createProxiedReply(Pillow::HttpConnection* request, QNetworkRequest proxiedRequest);
		virtual Pillow::HttpHandlerProxyPipe* createPipe(Pillow::HttpConnection* request, QNetworkReply* proxiedReply);
			
	public:
		virtual bool handleRequest(Pillow::HttpConnection *request);	
	};
	
	class HttpHandlerProxyPipe : public QObject
	{
		Q_OBJECT
		
	protected:
		Pillow::HttpConnection* _request;
		QNetworkReply* _proxiedReply;
		bool _headersSent;
		bool _broken;
		
	public:
		HttpHandlerProxyPipe(Pillow::HttpConnection* request, QNetworkReply* proxiedReply);
		~HttpHandlerProxyPipe();
		
		Pillow::HttpConnection* request() const { return _request; }
		QNetworkReply* proxiedReply() const { return _proxiedReply; }
	
	protected slots:
		virtual void teardown();
		virtual void sendHeaders();
		virtual void pump(const QByteArray& data);
		
	private slots:
		void proxiedReply_readyRead();
		void proxiedReply_finished();
	};
	
	class ElasticNetworkAccessManager : public QNetworkAccessManager
	{
		Q_OBJECT
	
	public:
		ElasticNetworkAccessManager(QObject* parent);
		~ElasticNetworkAccessManager();
		
	protected:
		virtual QNetworkReply* createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData);
	};
}

#endif // _PILLOW_HTTPHANDLERPROXY_H_
