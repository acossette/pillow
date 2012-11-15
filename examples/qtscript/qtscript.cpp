#include <QtCore/QCoreApplication>
#include <QtScript/QScriptEngine>
#include "HttpServer.h"
#include "HttpHandler.h"
#include "HttpHandlerQtScript.h"
using namespace Pillow;

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	HttpServer server(QHostAddress(QHostAddress::Any), 4567);
	if (!server.isListening()) exit(1);
	qDebug() << "Ready";

	QScriptEngine* scriptEngine = new QScriptEngine(&server);

	HttpHandlerStack* handler = new HttpHandlerStack(&server);
		new HttpHandlerLog(handler);
		new HttpHandlerQtScriptFile(scriptEngine, "test.js", "handleRequest", true, handler);
		new HttpHandler404(handler);
	QObject::connect(&server, SIGNAL(requestReady(Pillow::HttpConnection*)),
					 handler, SLOT(handleRequest(Pillow::HttpConnection*)));

	return a.exec();
}
