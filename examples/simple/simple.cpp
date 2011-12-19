#include <QtCore/QtCore>

#include "HttpServer.h"
#include "HttpHandler.h"
#include "HttpConnection.h"

static const QByteArray someHeaderToken("Some-Header");
static const QString someParam("Some-Param");
static const QByteArray helloWorldToken("Hello World!");

class SimpleExerciser : public Pillow::HttpHandler
{
	qint64 n;
public:
	SimpleExerciser(QObject* parent = 0) : Pillow::HttpHandler(parent), n(0)
	{}

	bool handleRequest(Pillow::HttpConnection *connection)
	{
		n += connection->requestContent().size();
		n += connection->requestFragment().size();
		n += connection->requestHeaderValue(someHeaderToken).size();
		n += connection->requestMethod().size();
		n += connection->requestPath().size();
		n += connection->requestQueryString().size();
		n += connection->requestUri().size();
		n += connection->requestUriDecoded().size();
		n += connection->requestFragmentDecoded().size();
		n += connection->requestPathDecoded().size();
		n += connection->requestQueryStringDecoded().size();
		n += connection->requestParamValue(someParam).size();
		connection->writeResponse(200, Pillow::HttpHeaderCollection(), helloWorldToken);
		return true;
	}
};

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	Pillow::HttpServer server(QHostAddress(QHostAddress::Any), 4567);
	if (!server.isListening())
		exit(1);
	qDebug() << "Ready";

	Pillow::HttpHandler* handler = new Pillow::HttpHandlerFixed(200, "", &server);

//	Pillow::HttpHandler* handler = new SimpleExerciser(&server);

//	Pillow::HttpHandler* handler = new Pillow::HttpHandlerStack(&server);
//		new Pillow::HttpHandlerLog(handler);
//		new Pillow::HttpHandlerFixed(200, "Hello from pillow!", handler);

	QObject::connect(&server, SIGNAL(requestReady(Pillow::HttpConnection*)), handler, SLOT(handleRequest(Pillow::HttpConnection*)));

	return a.exec();
}
