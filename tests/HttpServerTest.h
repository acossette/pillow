#ifndef HTTPSERVERTEST_H
#define HTTPSERVERTEST_H

#include <QObject>
#include <QPointer>

namespace Pillow
{
	class HttpServer;
	class HttpRequest;
}
class QIODevice;

uint qHash(const QPointer<Pillow::HttpRequest>& ptr);

class HttpServerTestBase : public QObject
{
    Q_OBJECT

protected slots: // Test slots.
	void init();
	void cleanup();
	
	void testInit() { cleanup(); init(); }
	void testHandlesConnectionsAsRequests();
	void testHandlesConcurrentConnections();
	void testReusesRequests();
	void testDestroysRequests();
	
protected:
	QObject* server;
	QList<Pillow::HttpRequest*> handledRequests;
	QList<QPointer<Pillow::HttpRequest> > guardedHandledRequests;

	void sendRequest(QIODevice* device, const QByteArray& content);
	void sendResponses();
	void sendConcurrentRequests(int concurrencyLevel);
	
protected slots:
	void requestReady(Pillow::HttpRequest* request);
	
protected:
	virtual QObject* createServer() = 0;
	virtual QIODevice* createClientConnection() = 0;	
};

class HttpServerTest : public HttpServerTestBase
{
	Q_OBJECT
	
private slots: // Test slots.
	void init() { HttpServerTestBase::init(); }
	void cleanup() { HttpServerTestBase::cleanup(); }
	
	void testInit() { HttpServerTestBase::testInit(); }
	void testHandlesConnectionsAsRequests() { HttpServerTestBase::testHandlesConnectionsAsRequests(); }
	void testHandlesConcurrentConnections() { HttpServerTestBase::testHandlesConcurrentConnections(); }
	void testReusesRequests() { HttpServerTestBase::testReusesRequests(); }
	void testDestroysRequests() { HttpServerTestBase::testDestroysRequests(); }

protected:
	virtual QObject* createServer();
	virtual QIODevice* createClientConnection();
};

class HttpsServerTest : public HttpServerTestBase
{
	Q_OBJECT
	
private slots: // Test slots.
	void init() { HttpServerTestBase::init(); }
	void cleanup() { HttpServerTestBase::cleanup(); }
	
	void testInit() { HttpServerTestBase::testInit(); }
	void testHandlesConnectionsAsRequests() { HttpServerTestBase::testHandlesConnectionsAsRequests(); }
	void testHandlesConcurrentConnections() { HttpServerTestBase::testHandlesConcurrentConnections(); }
	void testReusesRequests() { HttpServerTestBase::testReusesRequests(); }
	void testDestroysRequests() { HttpServerTestBase::testDestroysRequests(); }

protected:
	virtual QObject* createServer();
	virtual QIODevice* createClientConnection();
};

class HttpLocalServerTest : public HttpServerTestBase
{
	Q_OBJECT
	
private slots: // Test slots.
	void init() { HttpServerTestBase::init(); }
	void cleanup() { HttpServerTestBase::cleanup(); }
	
	void testInit() { HttpServerTestBase::testInit(); }
	void testHandlesConnectionsAsRequests() { HttpServerTestBase::testHandlesConnectionsAsRequests(); }
	void testHandlesConcurrentConnections() { HttpServerTestBase::testHandlesConcurrentConnections(); }
	void testReusesRequests() { HttpServerTestBase::testReusesRequests(); }
	void testDestroysRequests() { HttpServerTestBase::testDestroysRequests(); }

protected:
	virtual QObject* createServer();
	virtual QIODevice* createClientConnection();
};

#endif // HTTPSERVERTEST_H
