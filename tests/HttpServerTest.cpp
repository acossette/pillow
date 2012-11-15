#include "HttpServerTest.h"
#include <HttpServer.h>
#include <HttpConnection.h>
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QLocalSocket>

uint qHash(const QPointer<Pillow::HttpConnection>& ptr)
{
	Pillow::HttpConnection* c = static_cast<Pillow::HttpConnection*>(ptr);
	return qHash(c);
}

void HttpServerTestBase::init()
{
	server = createServer();
	connect(server, SIGNAL(requestReady(Pillow::HttpConnection*)), this, SLOT(requestReady(Pillow::HttpConnection*)));
}

void HttpServerTestBase::cleanup()
{
	delete server;
	handledRequests.clear();
	guardedHandledRequests.clear();
}

void HttpServerTestBase::requestReady(Pillow::HttpConnection *request)
{
	handledRequests << request;
	guardedHandledRequests << request;
}

void HttpServerTestBase::sendRequest(QIODevice *device, const QByteArray &content)
{
	int oldHandledRequestsCount = handledRequests.size();

	QByteArray request;
	request.append("GET / HTTP/1.0\r\nContent-Length: ")
			.append(QByteArray::number(content.size()))
			.append("\r\n\r\n").append(content);
	device->write(request);

	while(oldHandledRequestsCount == handledRequests.size())
		QCoreApplication::processEvents();
}

void HttpServerTestBase::sendResponses()
{
	foreach (Pillow::HttpConnection* request, guardedHandledRequests)
	{
		if (request && request->state() == Pillow::HttpConnection::SendingHeaders)
		{
			// Echo the request content as the response content.
			request->writeResponse(200, Pillow::HttpHeaderCollection(), request->requestContent());
		}
	}
}

void HttpServerTestBase::sendConcurrentRequests(int concurrencyLevel)
{
	int startingHandledRequests = handledRequests.size();
	QVector<QIODevice*> clients;

	for (int j = 0; j < concurrencyLevel; ++j) clients << createClientConnection();
	for (int j = 0; j < concurrencyLevel; ++j) sendRequest(clients.at(j), QByteArray("Hello").append(QByteArray::number(j)));

	while (handledRequests.size() < startingHandledRequests + concurrencyLevel)
		QCoreApplication::processEvents();

	sendResponses();

	foreach (QIODevice* client, clients)
	{
		while (client->bytesAvailable() == 0) QCoreApplication::processEvents();
		client->readAll(), delete client;
	}
}

void HttpServerTestBase::testHandlesConnectionsAsRequests()
{
	QIODevice* client = createClientConnection();
	sendRequest(client, "Hello");
	sendResponses();
	while (client->bytesAvailable() == 0) QCoreApplication::processEvents();
	QCOMPARE(handledRequests.size(), 1);
	QByteArray response = client->readAll();
	QVERIFY(response.startsWith("HTTP/1.0 200 OK"));
	QVERIFY(response.endsWith("Hello"));
}

void HttpServerTestBase::testHandlesConcurrentConnections()
{
	const int clientCount = 10; // Note: on Windows the maximum number of concurrent QLocalSocket clients is 62, so putting this well below the limit.
	QVector<QIODevice*> clients;
	for (int i = 0; i < clientCount; ++i)
		clients << createClientConnection();

	for (int i = 0; i < clientCount; ++i)
		sendRequest(clients.at(i), QByteArray("Hello").append(QByteArray::number(i)));

	sendResponses();
	QCOMPARE(handledRequests.size(), clientCount);

	for (int i = 0; i < clientCount; ++i)
	{
		QIODevice* client = clients.at(i);
		while (client->bytesAvailable() == 0) QCoreApplication::processEvents();
		QByteArray response = client->readAll();
		QVERIFY(response.startsWith("HTTP/1.0 200 OK"));
		QVERIFY(response.endsWith(QByteArray("Hello").append(QByteArray::number(i))));
	}
}

void HttpServerTestBase::testReusesRequests()
{
	const int iterations = 3;
	const int clientCount = 25;
	for (int i = 0; i < iterations; ++i) sendConcurrentRequests(clientCount);

	QCOMPARE(handledRequests.size(), iterations * clientCount);
	QCOMPARE(handledRequests.toSet().size(), 25);

	// Handling way more request should reuse those unique requests.
	handledRequests.clear();
	guardedHandledRequests.clear();
	for (int i = 0; i < iterations * 2; ++i) sendConcurrentRequests(clientCount + 5);
	QCOMPARE(handledRequests.size(), iterations * 2 * (clientCount + 5));
	QVERIFY(handledRequests.toSet().size() > guardedHandledRequests.toSet().size());
	QCOMPARE(guardedHandledRequests.toSet().size(), 26); // The pooled request objects still alive + a NULL pointer for all requests that were collected.
}

void HttpServerTestBase::testDestroysRequests()
{
	const int iterations = 2;
	const int clientCount = 27;
	for (int i = 0; i < iterations; ++i) sendConcurrentRequests(clientCount);

	QCOMPARE(handledRequests.size(), iterations * clientCount);
	QVERIFY(handledRequests.toSet().size() >= guardedHandledRequests.toSet().size());
	QCOMPARE(guardedHandledRequests.toSet().size(), 26); // There should remain the internally pooled objects. + 1 for the NULL pointer.

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
	while (socket->state() != QAbstractSocket::ConnectedState && socket->error() == QAbstractSocket::UnknownSocketError)
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
	QSignalSpy spy(server, SIGNAL(newConnection()));
	QLocalSocket* socket = new QLocalSocket(server);
	socket->connectToServer("Pillow_HttpLocalServerTest");
	while (spy.isEmpty() && socket->error() == QLocalSocket::UnknownSocketError)
		QCoreApplication::processEvents();

	if (socket->error() != QLocalSocket::UnknownSocketError)
		qDebug() << "Unexpected QLocalSocket error:" << socket->errorString();

	return socket;
}

