#ifndef _PILLOW_HTTPSERVER_H_
#define _PILLOW_HTTPSERVER_H_

#include <QtNetwork/QTcpServer>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslKey>
#include <QtNetwork/QSslError>

namespace Pillow
{
	class HttpRequest;
	
	//
	// HttpServer
	//
	
	class HttpServer : public QTcpServer
	{
		Q_OBJECT
		Q_PROPERTY(QHostAddress serverAddress READ serverAddress)
		Q_PROPERTY(int serverPort READ serverPort)
		Q_PROPERTY(bool listening READ isListening)
		
	private slots:
		void this_newConnection();
		
	protected:
		virtual void incomingConnection(int socketDescriptor);
	
	public:
		HttpServer(QObject* parent = 0);
		HttpServer(const QHostAddress& serverAddress, quint16 serverPort, QObject *parent = 0);
		
	signals:
		void requestReady(Pillow::HttpRequest* request);
	};
	
	//
	// HttpsServer
	//
	
	class HttpsServer : public HttpServer
	{
		Q_OBJECT
		QSslCertificate _certificate;
		QSslKey _privateKey;
	
	public slots:
		void sslSocket_encrypted();
		void sslSocket_sslErrors(const QList<QSslError>& sslErrors);
		
	protected:
		virtual void incomingConnection(int socketDescriptor);
	
	public:
		HttpsServer(QObject* parent = 0);
		HttpsServer(const QSslCertificate& certificate, const QSslKey& privateKey, const QHostAddress& serverAddress, quint16 serverPort, QObject *parent = 0);	
		
		const QSslCertificate& certificate() const { return _certificate; }
		const QSslKey& privateKey() const { return _privateKey; }
		
	public slots:
		void setCertificate(const QSslCertificate& certificate);
		void setPrivateKey(const QSslKey& privateKey);
	};
	
	//
	// HttpLocalServer
	//
		
	class HttpLocalServer : public QLocalServer
	{
		Q_OBJECT
		
	private slots:
		void this_newConnection();
		
	public:
		HttpLocalServer(QObject* parent = 0);
		HttpLocalServer(const QString& serverName, QObject *parent = 0);
		
	signals:
		void requestReady(Pillow::HttpRequest* request);	
	};	
}

#endif // _PILLOW_HTTPSERVER_H_
