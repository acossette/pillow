#ifndef _PILLOW_HTTPHANDLERSIMPLEROUTER_H_
#define _PILLOW_HTTPHANDLERSIMPLEROUTER_H_

#include "HttpHandler.h"
#include "HttpRequest.h"
#include <QtCore/QRegExp>

namespace Pillow
{
	class HttpHandlerSimpleRouterPrivate;
	class HttpHandlerSimpleRouter : public Pillow::HttpHandler
	{
		Q_OBJECT
		Q_DECLARE_PRIVATE(HttpHandlerSimpleRouter)
		HttpHandlerSimpleRouterPrivate* d_ptr;
		
	public:
		HttpHandlerSimpleRouter(QObject* parent = 0);
		~HttpHandlerSimpleRouter();
		
		void addRoute(const QString& path, Pillow::HttpHandler* handler);
		void addRoute(const QString& path, QObject* object, const char* member);
		void addRoute(const QString& path, int statusCode, const Pillow::HttpHeaderCollection& headers, const QByteArray& content = QByteArray());
		
		QRegExp pathToRegExp(const QString& path);
		
	public:
		bool handleRequest(Pillow::HttpRequest *request);
	};
}

#endif // _PILLOW_HTTPHANDLERSIMPLEROUTER_H_
