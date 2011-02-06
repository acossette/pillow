#ifndef _PILLOW_HTTPSERVER_H_
#define _PILLOW_HTTPSERVER_H_

#include <QtNetwork/QTcpServer>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QHostAddress>

namespace Pillow
{
	class HttpRequest;
	
	//
	// HttpServer
	//
	
	class HttpServerPrivate;
	
	class HttpServer : public QTcpServer
	{
		Q_OBJECT
		Q_PROPERTY(QHostAddress serverAddress READ serverAddress)
		Q_PROPERTY(int serverPort READ serverPort)
		Q_PROPERTY(bool listening READ isListening)
		Q_DECLARE_PRIVATE(HttpServer)
		HttpServerPrivate* d_ptr;
		
	private slots:
		void request_closed(Pillow::HttpRequest* request);
		
	protected:
		virtual void incomingConnection(int socketDescriptor);
		HttpRequest* createHttpRequest();
	
	public:
		HttpServer(QObject* parent = 0);
		HttpServer(const QHostAddress& serverAddress, quint16 serverPort, QObject *parent = 0);
		~HttpServer();
		
	signals:
		void requestReady(Pillow::HttpRequest* request);
	};
		
	//
	// HttpLocalServer
	//
		
	class HttpLocalServer : public QLocalServer
	{
		Q_OBJECT
		Q_DECLARE_PRIVATE(HttpServer)
		HttpServerPrivate* d_ptr;
		
	private slots:
		void this_newConnection();
		void request_closed(Pillow::HttpRequest* request);

	protected:
		virtual void incomingConnection(quintptr socketDescriptor);

	public:
		HttpLocalServer(QObject* parent = 0);
		HttpLocalServer(const QString& serverName, QObject *parent = 0);
		
	signals:
		void requestReady(Pillow::HttpRequest* request);	
	};	
}

#endif // _PILLOW_HTTPSERVER_H_
