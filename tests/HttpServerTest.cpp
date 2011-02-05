#include "HttpServerTest.h"
#include <HttpServer.h>
#include <HttpRequest.h>
#include <QtTest/QtTest>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QLocalSocket>

static void wait(int milliseconds = 5)
{
	QElapsedTimer t; t.start();
	do
	{
		QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
	} 
	while (t.elapsed() < milliseconds);
}

uint qHash(const QPointer<Pillow::HttpRequest>& ptr)
{
	return qHash(uint(static_cast<Pillow::HttpRequest*>(ptr)));
}

void HttpServerTestBase::init()
{
	server = createServer();
	connect(server, SIGNAL(requestReady(Pillow::HttpRequest*)), this, SLOT(requestReady(Pillow::HttpRequest*)));
}

void HttpServerTestBase::cleanup()
{
	delete server;
	handledRequests.clear();
	guardedHandledRequests.clear();
}

void HttpServerTestBase::requestReady(Pillow::HttpRequest *request)
{
	handledRequests << request;
	guardedHandledRequests << request;
}

void HttpServerTestBase::sendRequest(QIODevice *device, const QByteArray &content)
{
	QByteArray request;
	request.append("GET / HTTP/1.0\r\nContent-Length: ")
			.append(QByteArray::number(content.size()))
			.append("\r\n\r\n").append(content);
	
	device->write(request);
}

void HttpServerTestBase::sendResponses()
{
	foreach (Pillow::HttpRequest* request, guardedHandledRequests)
	{
		if (request && request->state() == Pillow::HttpRequest::SendingHeaders)
		{
			// Echo the request content as the response content.			
			request->writeResponse(200, Pillow::HttpHeaderCollection(), request->requestContent());
		}
	}
	wait();
}

void HttpServerTestBase::sendConcurrentRequests(int concurrencyLevel)
{
	int startingHandledRequests = handledRequests.size();
	QVector<QIODevice*> clients;
	
	for (int j = 0; j < concurrencyLevel; ++j) clients << createClientConnection();
	for (int j = 0; j < concurrencyLevel; ++j) sendRequest(clients.at(j), QByteArray("Hello").append(QByteArray::number(j)));

	while (handledRequests.size() < startingHandledRequests + concurrencyLevel)
		QCoreApplication::processEvents();
	
	wait(10);
	sendResponses();
	wait(10);	
	
	foreach (QIODevice* client, clients) { client->readAll(), delete client; }
}

void HttpServerTestBase::testHandlesConnectionsAsRequests()
{
	QIODevice* client = createClientConnection();
	sendRequest(client, "Hello");
	client->write("GET / HTTP/1.0\r\n\r\n");
	wait();
	sendResponses();
	QCOMPARE(handledRequests.size(), 1);
	QByteArray response = client->readAll();
	QVERIFY(response.startsWith("HTTP/1.0 200 OK"));
	QVERIFY(response.endsWith("Hello"));
}

void HttpServerTestBase::testHandlesConcurrentConnections()
{
	const int clientCount = 100;
	QVector<QIODevice*> clients;
	for (int i = 0; i < clientCount; ++i)
		clients << createClientConnection();

	for (int i = 0; i < clientCount; ++i)
	{
		sendRequest(clients.at(i), QByteArray("Hello").append(QByteArray::number(i)));
		QVERIFY(handledRequests.isEmpty());
	}
	
	wait(100);
	sendResponses();
	wait(100);
	QCOMPARE(handledRequests.size(), clientCount);
	
	for (int i = 0; i < clientCount; ++i)
	{
		QIODevice* client = clients.at(i);
		QByteArray response = client->readAll();
		QVERIFY(response.startsWith("HTTP/1.0 200 OK"));
		QVERIFY(response.endsWith(QByteArray("Hello").append(QByteArray::number(i))));
	}
}

void HttpServerTestBase::testReusesRequests()
{
	const int iterations = 5;
	const int clientCount = 50;
	for (int i = 0; i < iterations; ++i) sendConcurrentRequests(clientCount);

	QCOMPARE(handledRequests.size(), iterations * clientCount);
	QCOMPARE(handledRequests.toSet().size(), 50);
	
	// Handling way more request should reuse those unique requests.
	handledRequests.clear();
	guardedHandledRequests.clear();
	for (int i = 0; i < iterations * 2; ++i) sendConcurrentRequests(clientCount * 2);
	QCOMPARE(handledRequests.size(), iterations * clientCount * 4);
	QVERIFY(handledRequests.toSet().size() > guardedHandledRequests.toSet().size());
	QCOMPARE(guardedHandledRequests.toSet().size(), 51); // The 50 request objects still alive + a NULL pointer for all requests that were collected.
}

void HttpServerTestBase::testDestroysRequests()
{
	const int iterations = 5;
	const int clientCount = 52;
	for (int i = 0; i < iterations; ++i) sendConcurrentRequests(clientCount);

	QCOMPARE(handledRequests.size(), iterations * clientCount);
	QVERIFY(handledRequests.toSet().size() >= guardedHandledRequests.toSet().size());
	QCOMPARE(guardedHandledRequests.toSet().size(), 51); // There should remain the internally pooled objects. + 1 for the NULL pointer.

	delete server; server = NULL;
	QCOMPARE(guardedHandledRequests.toSet().size(), 1); // All requests should now have been destroyed. Only NULL is remaining.
}

//
// HttpServerTest
//

QObject* HttpServerTest::createServer()
{
	return new Pillow::HttpServer(QHostAddress::Any, 4577);
}

QIODevice * HttpServerTest::createClientConnection()
{
	QTcpSocket* socket = new QTcpSocket(server);
	socket->connectToHost(QHostAddress::LocalHost, 4577);
	while (socket->state() != QAbstractSocket::ConnectedState)
		QCoreApplication::processEvents();
	return socket;
}

//
// HttpLocalServerTest
//

QObject* HttpLocalServerTest::createServer()
{
	return new Pillow::HttpLocalServer("Pillow_HttpLocalServerTest");
}

QIODevice * HttpLocalServerTest::createClientConnection()
{
	QLocalSocket* socket = new QLocalSocket(server);
	socket->connectToServer("Pillow_HttpLocalServerTest");
	while (socket->state() != QLocalSocket::ConnectedState)
		wait(10);
	return socket;
}

