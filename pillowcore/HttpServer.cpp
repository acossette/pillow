#include "HttpServer.h"
#include "HttpRequest.h"
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QSslSocket>
#include <QtNetwork/QLocalSocket>
using namespace Pillow;

//
// HttpServer
//

HttpServer::HttpServer(QObject *parent)
: QTcpServer(parent)
{
	connect(this, SIGNAL(newConnection()), this, SLOT(this_newConnection()));
}

HttpServer::HttpServer(const QHostAddress &serverAddress, quint16 serverPort, QObject *parent)
:	QTcpServer(parent)
{
	connect(this, SIGNAL(newConnection()), this, SLOT(this_newConnection()));

	if (!listen(serverAddress, serverPort))
		qWarning() << QString("HttpServer::HttpServer: could not bind to %1:%2 for listening: %3").arg(serverAddress.toString()).arg(serverPort).arg(errorString());
}

void HttpServer::incomingConnection(int socketDescriptor)
{
	QTcpSocket* socket = new QTcpSocket();
	if (socket->setSocketDescriptor(socketDescriptor))
		addPendingConnection(socket);
	else
	{
		qWarning() << "HttpServer::incomingConnection: failed to set socket descriptor '" << socketDescriptor << "' on socket.";
		delete socket; 
	}
}

void HttpServer::this_newConnection()
{
	// Fix for Windows:
	// Try to destroy existing connections that are closed and that are pending deferred deletion.
	// This avoids deferring deletions for too long when the server is swamped with new connections,
	// because on Windows new connections seem to have priority over the deferred deletes..

	const QList<QObject*>& children = this->children();
	for (int i = children.size() - 1; i >= 0; --i)
	{
		if (qobject_cast<HttpRequest*>(children.at(i)))
		{
			HttpRequest* request = static_cast<HttpRequest*>(children.at(i));
			if (request->state() == HttpRequest::Closed)
				delete request;
		}
	}

	HttpRequest* request = new HttpRequest(nextPendingConnection(), this);
	connect(request, SIGNAL(ready(HttpRequest*)), this, SIGNAL(requestReady(HttpRequest*)));
	connect(request, SIGNAL(closed(HttpRequest*)), request, SLOT(deleteLater()));
}

//
// HttpsServer
//

HttpsServer::HttpsServer(QObject *parent)
	: HttpServer(parent)
{
}

HttpsServer::HttpsServer(const QSslCertificate& certificate, const QSslKey& privateKey, const QHostAddress &serverAddress, quint16 serverPort, QObject *parent)
	: HttpServer(serverAddress, serverPort, parent), _certificate(certificate), _privateKey(privateKey)
{
}

void HttpsServer::setCertificate(const QSslCertificate &certificate)
{
	_certificate = certificate;
}

void HttpsServer::setPrivateKey(const QSslKey &privateKey)
{
	_privateKey = privateKey;
}

void HttpsServer::incomingConnection(int socketDescriptor)
{
	QSslSocket* sslSocket = new QSslSocket(this);
	if (sslSocket->setSocketDescriptor(socketDescriptor))
	{
		sslSocket->setPrivateKey(privateKey());
		sslSocket->setLocalCertificate(certificate());
		sslSocket->startServerEncryption();
		connect(sslSocket, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslSocket_sslErrors(QList<QSslError>)));
		connect(sslSocket, SIGNAL(encrypted()), this, SLOT(sslSocket_encrypted()));
		addPendingConnection(sslSocket);
	}
	else
	{
		qWarning() << "HttpsServer::incomingConnection: failed to set socket descriptor '" << socketDescriptor << "' on ssl socket.";
		delete sslSocket;
	}
}

void HttpsServer::sslSocket_sslErrors(const QList<QSslError>&)
{
//	foreach (const QSslError& sslError, sslErrors)
//	{
//		qDebug() << sender() << "SSL error:" << sslError.errorString();
//	}
}

void HttpsServer::sslSocket_encrypted()
{
//	qDebug() << sender() << "SSL encrypted";
}

//
// HttpLocalServer
//

HttpLocalServer::HttpLocalServer(QObject *parent)
	: QLocalServer(parent)
{
}

HttpLocalServer::HttpLocalServer(const QString& serverName, QObject *parent /*= 0*/)
	: QLocalServer(parent)
{
	connect(this, SIGNAL(newConnection()), this, SLOT(this_newConnection()));

	if (!listen(serverName))
		qWarning() << QString("HttpLocalServer::HttpLocalServer: could not bind to %1 for listening: %2").arg(serverName).arg(errorString());
}

void HttpLocalServer::this_newConnection()
{
	HttpRequest* request = new HttpRequest(nextPendingConnection(), this);
	connect(request, SIGNAL(ready(HttpRequest*)), this, SIGNAL(requestReady(HttpRequest*)));
	connect(request, SIGNAL(closed(HttpRequest*)), request, SLOT(deleteLater()));
}
