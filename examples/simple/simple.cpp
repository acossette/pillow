#include <QtCore/QtCore>

#include "HttpServer.h"
#include "HttpHandler.h"

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	Pillow::HttpServer server(QHostAddress(QHostAddress::Any), 4567);
	if (!server.isListening())
		exit(1);
	qDebug() << "Ready";

	Pillow::HttpHandler* handler = new Pillow::HttpHandlerFixed(200, "Hello from pillow!", &server);
	
//	Pillow::HttpHandler* handler = new Pillow::HttpHandlerStack(&server);
//		new Pillow::HttpHandlerLog(handler);
//		new Pillow::HttpHandlerFixed(200, "Hello from pillow!", handler);
	
	QObject::connect(&server, SIGNAL(requestReady(Pillow::HttpRequest*)), handler, SLOT(handleRequest(Pillow::HttpRequest*)));

    return a.exec();
}
