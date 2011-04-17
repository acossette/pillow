#ifndef _PILLOW_HTTPHANDLERPROXY_H_
#define _PILLOW_HTTPHANDLERPROXY_H_

#include "HttpHandler.h"
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkAccessManager>

namespace Pillow 
{
	class ElasticNetworkAccessManager;
	
	class HttpHandlerProxy : public Pillow::HttpHandler
	{
		Q_OBJECT
		QUrl _proxiedUrl;
		ElasticNetworkAccessManager* networkAccessManager;
		
	public:
		HttpHandlerProxy(QObject *parent = 0);
		HttpHandlerProxy(const QUrl &proxiedUrl, QObject *parent = 0);
		
		const QUrl& proxiedUrl() const { return _proxiedUrl; }
		void setProxiedUrl(const QUrl& proxiedUrl);
		
	public:
		virtual bool handleRequest(Pillow::HttpConnection *request);	
	};
	
	class HttpHandlerProxyPipe : public QObject
	{
		Q_OBJECT
		Pillow::HttpConnection* _request;
		QNetworkReply* _proxiedRequest;
		
	public:
		HttpHandlerProxyPipe(Pillow::HttpConnection* request, QNetworkReply* proxiedRequest);
		~HttpHandlerProxyPipe();
		
		Pillow::HttpConnection* request() const { return _request; }
		QNetworkReply* proxiedRequest() const { return _proxiedRequest; }
	
	private slots:
		void teardown();
		void proxiedRequest_readyRead();
		void proxiedRequest_finished();
	};
	
	class ElasticNetworkAccessManager : public QNetworkAccessManager
	{
		Q_OBJECT
	
	public:
		ElasticNetworkAccessManager(QObject* parent);
	};
}

#endif // _PILLOW_HTTPHANDLERPROXY_H_
