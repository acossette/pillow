#include <QtCore/QtCore>

#include "HttpServer.h"
#include "HttpHandler.h"
using namespace Pillow;

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	HttpServer server(QHostAddress(QHostAddress::Any), 4567);
	if (!server.isListening())
		exit(1);
	qDebug() << "Ready";

//	HttpHandler* handler = new HttpHandlerStack(&server);
//		new HttpHandlerLog(handler);
//		new HttpHandler404(handler);
	HttpHandler* handler = new HttpHandlerFixed(200, "Hello from pillow!", &server);
	QObject::connect(&server, SIGNAL(requestReady(HttpRequest*)), handler, SLOT(handleRequest(HttpRequest*)));

    return a.exec();
}
