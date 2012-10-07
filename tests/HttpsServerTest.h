#ifndef HTTPSSERVERTEST_H
#define HTTPSSERVERTEST_H

#include "HttpServerTest.h"

#if !defined(PILLOW_NO_SSL) && !defined(QT_NO_SSL)

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

#else

#include <QtCore/QObject>

class HttpsServerTest : public QObject
{
	Q_OBJECT
};

#endif // !defined(PILLOW_NO_SSL) && !defined(QT_NO_SSL)

#endif // HTTPSSERVERTEST_H
