#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslKey>
#include <QtNetwork/QSslSocket>
#include "HttpsServer.h"
#include "HttpHandler.h"
using namespace Pillow;

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	QFile certificateFile(":/test.crt");
	if (!certificateFile.open(QIODevice::ReadOnly))
	{
		qDebug() << "Could not open certificate file 'test.crt'";
		exit(2);
	}
	QSslCertificate certificate(&certificateFile);

	QFile keyFile(":/test.key");
	if (!keyFile.open(QIODevice::ReadOnly))
	{
		qDebug() << "Could not open key file 'test.key'";
		exit(3);
	}
	QSslKey key(&keyFile, QSsl::Rsa);

	HttpsServer server(certificate, key, QHostAddress(QHostAddress::Any), 4567);
	if (!server.isListening()) exit(1);
	qDebug() << "Ready";

	HttpHandler* handler = new HttpHandlerStack(&server);
		new HttpHandlerLog(handler);
		new HttpHandler404(handler);
	QObject::connect(&server, SIGNAL(requestReady(Pillow::HttpConnection*)),
					 handler, SLOT(handleRequest(Pillow::HttpConnection*)));

//  Client socket example:
//	QSslSocket sslSocket;
//	sslSocket.setLocalCertificate(certificate);
//	sslSocket.setPrivateKey(key);
//	sslSocket.setPeerVerifyMode(QSslSocket::VerifyNone);
//	sslSocket.connectToHostEncrypted("127.0.0.1", 4567);
//	sslSocket.waitForConnected();
//	qDebug() << "Connected";
//	sslSocket.write("GET / HTTP/1.0\r\n\r\n");
//	sslSocket.flush();
//	qDebug() << "Request sent... waiting for reply" << sslSocket.state();
//	while (sslSocket.state() == QAbstractSocket::ConnectedState) QCoreApplication::processEvents();
//	qDebug() << "Got reply: " << sslSocket.state() << sslSocket.readAll();

	return a.exec();
}
