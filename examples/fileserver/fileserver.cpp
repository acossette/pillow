#include <QtCore/QtCore>

#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpHandler.h"
using namespace Pillow;

class HttpHandlerStats : public HttpHandler
{
public:
	HttpHandlerStats(QObject* parent = 0) : HttpHandler(parent) {}

	virtual bool handleRequest(HttpRequest* rq)
	{
		HttpServer* server = NULL;
		HttpHandler* handler = NULL;
		
		for (QObject* h = parent(); h != NULL; h = h->parent())
		{
			if (qobject_cast<HttpHandler*>(h)) handler = static_cast<HttpHandler*>(h);
			else if (qobject_cast<HttpServer*>(h)) server = static_cast<HttpServer*>(h);
		}
		
		if (rq->requestPath() == "/_stats")
		{
			QByteArray result;
			if (server != NULL)
			{
				result.append("Alive connections: ").append(QByteArray::number(server->findChildren<HttpRequest*>().size())).append("\n");
			}
			if (handler != NULL)
			{
				result.append("Alive big file transfers: ").append(QByteArray::number(handler->findChildren<HttpHandlerFileTransfer*>().size())).append("\n");
			}
			
			rq->writeResponse(200, HttpHeaderCollection(), result);
			return true;
		}
		return false;
	}
};

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	HttpServer server(QHostAddress(QHostAddress::Any), 4567);
	if (!server.isListening())
		exit(1);
	qDebug() << "Ready";

	HttpHandlerStack* handler = new HttpHandlerStack(&server);
		new HttpHandlerLog(handler);
		new HttpHandlerStats(handler);
		new HttpHandlerFile("/home/alcos/public", handler);
		new HttpHandler404(handler);
	QObject::connect(&server, SIGNAL(requestReady(HttpRequest*)), handler, SLOT(handleRequest(HttpRequest*)));

    return a.exec();
}
