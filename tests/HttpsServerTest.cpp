#ifndef PILLOW_NO_SSL

#include "HttpsServerTest.h"
#include <HttpsServer.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtNetwork/QSslSocket>
#include <QtNetwork/QSslKey>
#include <QtNetwork/QSslCertificate>

static QSslCertificate sslCertificate()
{
	static QSslCertificate certificate;
	if (certificate.isNull())
	{
		QFile file(":/test.crt");
		if (file.open(QIODevice::ReadOnly))
			certificate = QSslCertificate(&file);
		else
			qWarning() << "Failed to open SSL certificate file 'test.crt'";
	}
	return certificate;
}

static QSslKey sslPrivateKey()
{
	static QSslKey key;
	if (key.isNull())
	{
		QFile file(":/test.key");
		if (file.open(QIODevice::ReadOnly))
			key = QSslKey(&file, QSsl::Rsa);
		else
			qWarning() << "Failed to open SSL key file 'test.key'";
	}
	return key;
}

QObject* HttpsServerTest::createServer()
{
	return new Pillow::HttpsServer(sslCertificate(), sslPrivateKey(), QHostAddress::Any, 4588);
}

QIODevice * HttpsServerTest::createClientConnection()
{
	QSslSocket* socket = new QSslSocket(server);
	socket->setLocalCertificate(sslCertificate());
	socket->setPrivateKey(sslPrivateKey());
	socket->setPeerVerifyMode(QSslSocket::VerifyNone);
	socket->connectToHostEncrypted("127.0.0.1", 4588);
	while (socket->state() != QAbstractSocket::ConnectedState || !socket->isEncrypted())
		QCoreApplication::processEvents();
	return socket;
}

#endif // !PILLOW_NO_SSL
