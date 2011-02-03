#include "HttpServer.h"
#include "HttpRequest.h"
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QSslSocket>
#include <QtNetwork/QLocalSocket>
using namespace Pillow;

//
// HttpServer
//

namespace Pillow
{
	class HttpServerPrivate
	{
	public:
		enum { MaximumReserveCount = 50 };
		
	public:
		QObject* q_ptr;
		QList<HttpRequest*> reservedRequests;
		
	public:
		HttpServerPrivate(QObject* server) 
			: q_ptr(server)
		{}
		
		~HttpServerPrivate()
		{
			while (!reservedRequests.isEmpty())
				delete reservedRequests.takeLast();
		}
		
		HttpRequest* takeRequest()
		{
			HttpRequest* request = NULL;
			
			if (reservedRequests.isEmpty())
			{
				request = new HttpRequest(q_ptr);
				QObject::connect(request, SIGNAL(ready(Pillow::HttpRequest*)), q_ptr, SIGNAL(requestReady(Pillow::HttpRequest*)));
				QObject::connect(request, SIGNAL(closed(Pillow::HttpRequest*)), q_ptr, SLOT(request_closed(Pillow::HttpRequest*)));
			}
			else
				request = reservedRequests.takeLast();
			
			return request;
		}
		
		void putRequest(HttpRequest* request)
		{
			while (reservedRequests.size() >= MaximumReserveCount)
				delete reservedRequests.takeLast();

			reservedRequests.append(request);
		}
	};
}

HttpServer::HttpServer(QObject *parent)
: QTcpServer(parent), d_ptr(new HttpServerPrivate(this))
{
}

HttpServer::HttpServer(const QHostAddress &serverAddress, quint16 serverPort, QObject *parent)
:	QTcpServer(parent), d_ptr(new HttpServerPrivate(this))
{
	if (!listen(serverAddress, serverPort))
		qWarning() << QString("HttpServer::HttpServer: could not bind to %1:%2 for listening: %3").arg(serverAddress.toString()).arg(serverPort).arg(errorString());
}

HttpServer::~HttpServer()
{
	delete d_ptr;
}

void HttpServer::incomingConnection(int socketDescriptor)
{
	QTcpSocket* socket = new QTcpSocket();
	if (socket->setSocketDescriptor(socketDescriptor))
	{
		addPendingConnection(socket);
		nextPendingConnection();
		createHttpRequest()->initialize(socket, socket);		
	}
	else
	{
		qWarning() << "HttpServer::incomingConnection: failed to set socket descriptor '" << socketDescriptor << "' on socket.";
		delete socket; 
	}
}

void HttpServer::request_closed(Pillow::HttpRequest *request)
{
	request->inputDevice()->deleteLater();
	d_ptr->putRequest(request);
}

HttpRequest* Pillow::HttpServer::createHttpRequest()
{
	return d_ptr->takeRequest();
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
		nextPendingConnection();
		createHttpRequest()->initialize(sslSocket, sslSocket);
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
	: QLocalServer(parent), d_ptr(new HttpServerPrivate(this))
{
	connect(this, SIGNAL(newConnection()), this, SLOT(this_newConnection()));
}

HttpLocalServer::HttpLocalServer(const QString& serverName, QObject *parent /*= 0*/)
	: QLocalServer(parent), d_ptr(new HttpServerPrivate(this))
{
	connect(this, SIGNAL(newConnection()), this, SLOT(this_newConnection()));

	if (!listen(serverName))
		qWarning() << QString("HttpLocalServer::HttpLocalServer: could not bind to %1 for listening: %2").arg(serverName).arg(errorString());
}

void HttpLocalServer::this_newConnection()
{
	QIODevice* device = nextPendingConnection();
	d_ptr->takeRequest()->initialize(device, device);
}

void HttpLocalServer::request_closed(Pillow::HttpRequest *request)
{
	request->inputDevice()->deleteLater();
	d_ptr->putRequest(request);
}

