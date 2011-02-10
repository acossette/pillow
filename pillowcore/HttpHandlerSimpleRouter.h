#ifndef _PILLOW_HTTPHANDLERSIMPLEROUTER_H_
#define _PILLOW_HTTPHANDLERSIMPLEROUTER_H_

#include "HttpHandler.h"
#include "HttpRequest.h"
#include <QtCore/QRegExp>
#include <QtCore/QStringList>

namespace Pillow
{
	class HttpHandlerSimpleRouterPrivate;
	class HttpHandlerSimpleRouter : public Pillow::HttpHandler
	{
		Q_OBJECT
		Q_DECLARE_PRIVATE(HttpHandlerSimpleRouter)
		HttpHandlerSimpleRouterPrivate* d_ptr;
		Q_ENUMS(RoutingErrorAction)
		
	public:
		enum RoutingErrorAction { Return4xxResponse, Passthrough };
				
	public:
		HttpHandlerSimpleRouter(QObject* parent = 0);
		~HttpHandlerSimpleRouter();
		
		void addRoute(const QString& path, Pillow::HttpHandler* handler) { addRoute(QByteArray(), path, handler); }
		void addRoute(const QString& path, QObject* object, const char* member) { addRoute(QByteArray(), path, object, member); }
		void addRoute(const QString& path, int statusCode, const Pillow::HttpHeaderCollection& headers, const QByteArray& content = QByteArray()) { addRoute(QByteArray(), path, statusCode, headers, content); }
		void addRoute(const QByteArray& method, const QString& path, Pillow::HttpHandler* handler);
		void addRoute(const QByteArray& method, const QString& path, QObject* object, const char* member);
		void addRoute(const QByteArray& method, const QString& path, int statusCode, const Pillow::HttpHeaderCollection& headers, const QByteArray& content = QByteArray());
		
		QRegExp pathToRegExp(const QString& path, QStringList* outParamNames = NULL);
		
		RoutingErrorAction unmatchedRequestAction() const;
		void setUnmatchedRequestAction(RoutingErrorAction action);
		
		RoutingErrorAction methodMismatchAction() const;		
		void setMethodMismatchAction(RoutingErrorAction action);
		
		bool acceptsMethodParam() const;
		void setAcceptsMethodParam(bool accept);
		
	public:
		bool handleRequest(Pillow::HttpRequest *request);
	};
}

#endif // _PILLOW_HTTPHANDLERSIMPLEROUTER_H_
