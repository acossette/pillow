#include "HttpConnectionTest.h"
#include "HttpConnection.h"
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QLocalSocket>
#include <QtCore/QFile>
#include <QtCore/QBuffer>
using namespace Pillow;

static void wait(int milliseconds = 10)
{
	QElapsedTimer t; t.start();
	do
	{
		QCoreApplication::processEvents(QEventLoop::AllEvents);
	}
	while (!t.hasExpired(milliseconds));
}

HttpConnectionTest::HttpConnectionTest()
	: connection(NULL), readySpy(NULL), completedSpy(NULL), closedSpy(NULL), reuseConnection(false)
{
	qRegisterMetaType<Pillow::HttpConnection*>("Pillow::HttpConnection*");
}

void HttpConnectionTest::init()
{
}

void HttpConnectionTest::cleanup()
{
}

void HttpConnectionTest::testInitialState()
{
	QCOMPARE(connection->state(), HttpConnection::ReceivingHeaders);
	QCOMPARE(connection->requestMethod(), QByteArray());
	QCOMPARE(connection->requestUri(), QByteArray());
	QCOMPARE(connection->requestFragment(), QByteArray());
	QCOMPARE(connection->requestPath(), QByteArray());
	QCOMPARE(connection->requestQueryString(), QByteArray());
	QVERIFY(connection->requestHeaders().isEmpty());
	QCOMPARE(connection->requestContent(), QByteArray());

	QCOMPARE(connection->requestUriDecoded(), QString());
	QCOMPARE(connection->requestFragmentDecoded(), QString());
	QCOMPARE(connection->requestPathDecoded(), QString());
	QCOMPARE(connection->requestQueryStringDecoded(), QString());
}

void HttpConnectionTest::testSimpleGet()
{
	clientWrite("GET /first%20test/some%20index.html?key=one%20value#and%20fragment HTTP/1.0\r\n");
	clientWrite("Host: example.org\r\n");
	clientWrite("Weird: \r\n");
	clientWrite("X-Dummy: DummyValue\r\n");
	clientWrite("\r\n"); clientFlush();

	QCOMPARE(connection->state(), HttpConnection::SendingHeaders);
	QCOMPARE(connection->requestMethod(), QByteArray("GET"));
	QCOMPARE(connection->requestUri(), QByteArray("/first%20test/some%20index.html?key=one%20value"));
	QCOMPARE(connection->requestUriDecoded(), QString("/first test/some index.html?key=one value"));
	QCOMPARE(connection->requestFragment(), QByteArray("and%20fragment"));
	QCOMPARE(connection->requestFragmentDecoded(), QString("and fragment"));
	QCOMPARE(connection->requestPath(), QByteArray("/first%20test/some%20index.html"));
	QCOMPARE(connection->requestPathDecoded(), QString("/first test/some index.html"));
	QCOMPARE(connection->requestQueryString(), QByteArray("key=one%20value"));
	QCOMPARE(connection->requestQueryStringDecoded(), QString("key=one value"));
	QCOMPARE(connection->requestHttpVersion(), QByteArray("HTTP/1.0"));
	QCOMPARE(connection->requestHeaders().size(), 3);
	QCOMPARE(connection->requestHeaders().at(0).first, QByteArray("Host"));
	QCOMPARE(connection->requestHeaders().at(0).second, QByteArray("example.org"));
	QCOMPARE(connection->requestHeaders().at(1).first, QByteArray("Weird"));
	QCOMPARE(connection->requestHeaders().at(1).second, QByteArray(""));
	QCOMPARE(connection->requestHeaders().at(2).first, QByteArray("X-Dummy"));
	QCOMPARE(connection->requestHeaders().at(2).second, QByteArray("DummyValue"));
	QCOMPARE(connection->requestHeaderValue("x-DUmmY"), QByteArray("DummyValue"));
	QCOMPARE(connection->requestHeaderValue("missing"), QByteArray());
	QCOMPARE(connection->requestContent(), QByteArray());
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);
	QVERIFY(isClientConnected());

	// Safety test: check that all returned QByteArrays are null-terminated. Even if some internally come
	// from QByteArray::fromRawData, we still want to provide null-terminated QByteArrays.
	QVERIFY(qstrcmp(connection->requestMethod().constData(), "GET") == 0);
	QVERIFY(qstrcmp(connection->requestUri().constData(), "/first%20test/some%20index.html?key=one%20value") == 0);
	QVERIFY(qstrcmp(connection->requestFragment().constData(), "and%20fragment") == 0);
	QVERIFY(qstrcmp(connection->requestPath().constData(), "/first%20test/some%20index.html") == 0);
	QVERIFY(qstrcmp(connection->requestQueryString().constData(), "key=one%20value") == 0);
	QVERIFY(qstrcmp(connection->requestHttpVersion().constData(), "HTTP/1.0") == 0);
	QVERIFY(qstrcmp(connection->requestHeaders().at(0).first.constData(), "Host") == 0);
	QVERIFY(qstrcmp(connection->requestHeaders().at(0).second.constData(), "example.org") == 0);
	QVERIFY(qstrcmp(connection->requestHeaders().at(1).first.constData(), "Weird") == 0);
	QVERIFY(qstrcmp(connection->requestHeaders().at(1).second.constData(),"") == 0);
}

void HttpConnectionTest::testSimplePost()
{
	clientWrite("POST /postpath#frag HTTP/1.0\r\n");
	clientWrite("Host: example.org\r\n");
	clientWrite("conTEnt-leNGtH: 11\r\n");
	clientWrite("\r\n");
	clientWrite("contentdata"); clientFlush();

	QCOMPARE(connection->state(), HttpConnection::SendingHeaders);
	QCOMPARE(connection->requestMethod(), QByteArray("POST"));
	QCOMPARE(connection->requestUri(), QByteArray("/postpath"));
	QCOMPARE(connection->requestFragment(), QByteArray("frag"));
	QCOMPARE(connection->requestPath(), QByteArray("/postpath"));
	QCOMPARE(connection->requestQueryString(), QByteArray());
	QCOMPARE(connection->requestHeaders().size(), 2);
	QCOMPARE(connection->requestHeaders().at(0).first, QByteArray("Host"));
	QCOMPARE(connection->requestHeaders().at(0).second, QByteArray("example.org"));
	QCOMPARE(connection->requestHeaders().at(1).first, QByteArray("conTEnt-leNGtH"));
	QCOMPARE(connection->requestHeaders().at(1).second, QByteArray("11"));
	QCOMPARE(connection->requestContent(), QByteArray("contentdata"));
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);
	QVERIFY(isClientConnected());

	QVERIFY(qstrcmp(connection->requestQueryString().constData(), "") == 0);
	QVERIFY(qstrcmp(connection->requestFragment().constData(), "frag") == 0);
}

void HttpConnectionTest::testIncrementalPost()
{
	clientWrite("POST /test/stuff HTTP/1.0\r\n"); clientFlush();

	QCOMPARE(connection->state(), HttpConnection::ReceivingHeaders);

	clientWrite("Content-Length: 8\r\n");
	clientWrite("\r\n"); clientFlush();

	QCOMPARE(connection->state(), HttpConnection::ReceivingContent);
	QCOMPARE(connection->requestContent(), QByteArray()); // It should not mistake headers for request content
	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);

	// Send incomplete post data.
	clientWrite("some"); clientFlush();

	QCOMPARE(connection->state(), HttpConnection::ReceivingContent);
	QCOMPARE(connection->requestContent(), QByteArray()); // The request content should not be available until it is all received.
	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);

	// Send remaining post data.
	clientWrite("data"); clientFlush();

	QCOMPARE(connection->state(), HttpConnection::SendingHeaders);
	QCOMPARE(connection->requestMethod(), QByteArray("POST"));		// Receiving some content should not clear the request header info.
	QCOMPARE(connection->requestUri(), QByteArray("/test/stuff"));
	QCOMPARE(connection->requestFragment(), QByteArray());
	QCOMPARE(connection->requestPath(), QByteArray("/test/stuff"));
	QCOMPARE(connection->requestQueryString(), QByteArray());
	QCOMPARE(connection->requestHeaders().size(), 1);
	QCOMPARE(connection->requestHeaders().at(0).first, QByteArray("Content-Length"));
	QCOMPARE(connection->requestHeaders().at(0).second, QByteArray("8"));
	QCOMPARE(connection->requestContent(), QByteArray("somedata"));
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);
	QVERIFY(isClientConnected());
}

void HttpConnectionTest::testHugePost()
{
	// Note: this test is very timing dependent, especially on windows.

	QByteArray postData(8 * 1024 * 1024, '*');
	QByteArray clientRequest;
	clientRequest.append("POST /test HTTP/1.1\r\n")
				 .append("Content-Length: ").append(QByteArray::number(postData.size())).append("\r\n")
				 .append("\r\n").append(postData);


	QCOMPARE(connection->state(), HttpConnection::ReceivingHeaders);

	clientWrite(clientRequest);
	clientFlush(); wait(100);

	QCOMPARE(connection->state(), HttpConnection::SendingHeaders);
	QCOMPARE(connection->requestContent(), postData);
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);
	QVERIFY(isClientConnected());

	connection->writeResponse(200, Pillow::HttpHeaderCollection(), "Thank you");
	QByteArray data = clientReadAll();
	QVERIFY(data.startsWith("HTTP/1.1 200 OK"));
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
	QCOMPARE(closedSpy->size(), 0);
	QVERIFY(isClientConnected());
}

void HttpConnectionTest::testInvalidRequestHeaders()
{
	clientWrite("INVALID REQUEST HEADERS\r\n");
	clientWrite("Invalid headers\r\n");
	clientWrite("Should close"); clientFlush();

	QVERIFY(clientReadAll().startsWith("HTTP/1.0 400"));
	QCOMPARE(connection->state(), HttpConnection::Closed);
	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 1);
	QVERIFY(!isClientConnected());
}

void HttpConnectionTest::testOversizedRequestHeaders()
{
	QByteArray data;
	data.append("GET /test/stuff HTTP/1.0\r\n")
		.append("Some-Header: ").append(QByteArray(HttpConnection::MaximumRequestHeaderLength, 'a')).append("\r\n")
		.append("\r\n");
	clientWrite(data); 	clientFlush();

	QVERIFY(clientReadAll().startsWith("HTTP/1.0 400")); // Bad Request
	QCOMPARE(connection->state(), HttpConnection::Closed);
	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 1);
	QVERIFY(!isClientConnected());
}

void HttpConnectionTest::testInvalidRequestContent()
{
	clientWrite("POST / HTTP/1.0\r\n");
	clientWrite("Content-Length: abcdefg\r\n");
	clientWrite("\r\n");
	clientWrite("Hello"); clientFlush();

	QVERIFY(clientReadAll().startsWith("HTTP/1.0 413")); // Too large
	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 1);
	QVERIFY(!isClientConnected());

	cleanup(); init();
	clientWrite("POST / HTTP/1.0\r\n");
	clientWrite("Content-Length: -1\r\n");
	clientWrite("\r\n");
	clientWrite("Hello"); clientFlush();

	QVERIFY(clientReadAll().startsWith("HTTP/1.0 400")); // Bad request.
	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 1);
	QVERIFY(!isClientConnected());

	cleanup(); init();
	clientWrite("POST / HTTP/1.0\r\n");
	clientWrite("Content-Length: 204800000000\r\n"); // Too large
	clientWrite("\r\n");
	clientWrite("Hello"); clientFlush();

	QVERIFY(clientReadAll().startsWith("HTTP/1.0 413"));
	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 1);
	QVERIFY(!isClientConnected());

	// In the case where the client sends more data than it is supposed too, the request shall
	// process the request while honoring the expected content-length. Extra data will be kept
	// in buffer for a future request (which will most likely end up being a 400 bad request).
	cleanup(); init();
	clientWrite("POST / HTTP/1.0\r\n");
	clientWrite("Content-Length: 3\r\n"); // Too large!
	clientWrite("\r\n");
	clientWrite("Hello"); clientFlush();

	QCOMPARE(connection->state(), HttpConnection::SendingHeaders);
	QCOMPARE(connection->requestContent(), QByteArray("Hel"));
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);
	QVERIFY(isClientConnected());
}

void HttpConnectionTest::testWriteSimpleResponse()
{
	clientWrite("GET / HTTP/1.0\r\n");
	clientWrite("\r\n"); clientFlush();

	connection->writeResponse(200, HttpHeaderCollection() << HttpHeader("Some-Header", "Some Value") << HttpHeader("Other-Header", "OtherValue"), "response content");

	QByteArray data = clientReadAll();
	QCOMPARE(data, QByteArray("HTTP/1.0 200 OK\r\nSome-Header: Some Value\r\nOther-Header: OtherValue\r\nContent-Length: 16\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nresponse content"));
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
}

void HttpConnectionTest::testWriteSimpleResponseString()
{
	clientWrite("GET / HTTP/1.0\r\n");
	clientWrite("\r\n"); clientFlush();

	QString content = QString::fromUtf8("résponçë côntent"); // those 16 utf8 chars -> 20 bytes

	connection->writeResponseString(200, HttpHeaderCollection(), content );

	QCOMPARE(clientReadAll(), QByteArray("HTTP/1.0 200 OK\r\nContent-Length: 20\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n") + content.toUtf8());
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
}

void HttpConnectionTest::testConnectionKeepAlive()
{
	// For HTTP/1.1, the server should use keep-alive by default.
	clientWrite("GET /first/request?hello=world#frag HTTP/1.1\r\n");
	clientWrite("\r\n"); clientFlush();

	connection->writeResponse(200);
	QVERIFY(clientReadAll().startsWith("HTTP/1.1 200"));
	QVERIFY(isClientConnected());
	QCOMPARE(connection->state(), HttpConnection::ReceivingHeaders);
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
	QCOMPARE(closedSpy->size(), 0);
	QCOMPARE(connection->requestUri(), QByteArray("/first/request?hello=world"));
	QCOMPARE(connection->requestFragment(), QByteArray("frag"));
	QCOMPARE(connection->requestPath(), QByteArray("/first/request"));
	QCOMPARE(connection->requestQueryString(), QByteArray("hello=world"));

	// So it should be possible to send subsequent requests on the same connection.
	clientWrite("GET /other HTTP/1.1\r\n");
	clientWrite("\r\n"); clientFlush();

	connection->writeResponse(302);
	QVERIFY(clientReadAll().startsWith("HTTP/1.1 302"));
	QVERIFY(isClientConnected());
	QCOMPARE(connection->state(), HttpConnection::ReceivingHeaders);
	QCOMPARE(readySpy->size(), 2);
	QCOMPARE(completedSpy->size(), 2);
	QCOMPARE(closedSpy->size(), 0);
	QCOMPARE(connection->requestUri(), QByteArray("/other"));
	QCOMPARE(connection->requestFragment(), QByteArray());
	QCOMPARE(connection->requestPath(), QByteArray("/other"));
	QCOMPARE(connection->requestQueryString(), QByteArray());

	clientWrite("GET /another HTTP/1.1\r\n");
	clientWrite("\r\n"); clientFlush();

	connection->writeResponse(304);
	QVERIFY(clientReadAll().startsWith("HTTP/1.1 304"));
	QVERIFY(isClientConnected());
	QCOMPARE(connection->state(), HttpConnection::ReceivingHeaders);
	QCOMPARE(readySpy->size(), 3);
	QCOMPARE(completedSpy->size(), 3);
	QCOMPARE(closedSpy->size(), 0);

	// For HTTP/1.0, keep-alive is done only if requested by the client.
	clientWrite("GET /another/but?http=1.0 HTTP/1.0\r\n");
	clientWrite("cOnnEcTion: keep-alive\r\n");
	clientWrite("\r\n"); clientFlush();

	connection->writeResponse(200);
	QVERIFY(clientReadAll().startsWith("HTTP/1.0 200"));
	QVERIFY(isClientConnected());
	QCOMPARE(connection->state(), HttpConnection::ReceivingHeaders);
	QCOMPARE(readySpy->size(), 4);
	QCOMPARE(completedSpy->size(), 4);
	QCOMPARE(closedSpy->size(), 0);
	QCOMPARE(connection->requestUri(), QByteArray("/another/but?http=1.0"));
	QCOMPARE(connection->requestFragment(), QByteArray());
	QCOMPARE(connection->requestPath(), QByteArray("/another/but"));
	QCOMPARE(connection->requestQueryString(), QByteArray("http=1.0"));

	clientWrite("GET /another/but/http/1.0/without/keepalive HTTP/1.0\r\n");
	clientWrite("\r\n"); clientFlush();

	connection->writeResponse(200);
	QVERIFY(clientReadAll().startsWith("HTTP/1.0 200"));
	QVERIFY(!isClientConnected());
	QCOMPARE(connection->state(), HttpConnection::Closed);
	QCOMPARE(readySpy->size(), 5);
	QCOMPARE(completedSpy->size(), 5);
	QCOMPARE(closedSpy->size(), 1);
}

void HttpConnectionTest::testConnectionClose()
{
	// The server should close the connection if the client specifies Connection: close for any protocol version.
	clientWrite("GET / HTTP/1.1\r\n");
	clientWrite("coNNEction: close\r\n");
	clientWrite("\r\n"); clientFlush();

	connection->writeResponse(200);
	QVERIFY(clientReadAll().startsWith("HTTP/1.1 200"));
	QVERIFY(!isClientConnected());
	QCOMPARE(connection->state(), HttpConnection::Closed);
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
	QCOMPARE(closedSpy->size(), 1);

	cleanup(); init();
	clientWrite("GET /another HTTP/1.0\r\n");
	clientWrite("Connection: close\r\n");
	clientWrite("\r\n"); clientFlush();

	connection->writeResponse(302);
	QVERIFY(clientReadAll().startsWith("HTTP/1.0 302"));
	QVERIFY(!isClientConnected());
	QCOMPARE(connection->state(), HttpConnection::Closed);
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
	QCOMPARE(closedSpy->size(), 1);

	// The server should also close the connection if the responbse specifies Connection: close.
	cleanup(); init();
	clientWrite("GET /other HTTP/1.1\r\n");
	clientWrite("\r\n"); clientFlush();

	connection->writeResponse(304, HttpHeaderCollection() << HttpHeader("Connection", "close"));
	QVERIFY(clientReadAll().startsWith("HTTP/1.1 304"));
	QVERIFY(!isClientConnected());
	QCOMPARE(connection->state(), HttpConnection::Closed);
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
	QCOMPARE(closedSpy->size(), 1);

	// The server should also close the response content-length is not known and chunked transfer encoding is not used.
	cleanup(); init();
	clientWrite("GET /and_more HTTP/1.1\r\n");
	clientWrite("\r\n"); clientFlush();

	connection->writeHeaders(200, HttpHeaderCollection());
	QByteArray data = clientReadAll();
	QVERIFY(data.startsWith("HTTP/1.1 200"));
	QVERIFY(data.toLower().contains("connection: close"));
	QVERIFY(isClientConnected());
	QCOMPARE(connection->state(), HttpConnection::SendingContent);
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);
	connection->writeContent("hello");
	connection->endContent();
	data = clientReadAll();
	QCOMPARE(data, QByteArray("hello"));
	QVERIFY(!isClientConnected());
	QCOMPARE(connection->state(), HttpConnection::Closed);
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
	QCOMPARE(closedSpy->size(), 1);
}

void HttpConnectionTest::testClientClosesConnectionEarly()
{
	clientWrite("GET / HTTP/1.1\r\n");
	clientFlush();
	clientClose();
	while (connection->state() != HttpConnection::Closed)
		QCoreApplication::processEvents();

	QCOMPARE(connection->state(), HttpConnection::Closed);
	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 1);
	QVERIFY(!isClientConnected());

	cleanup(); init();
	clientWrite("GET /another HTTP/1.0\r\n");
	clientFlush();
	clientClose();
	while (connection->state() != HttpConnection::Closed)
		QCoreApplication::processEvents();

	QCOMPARE(connection->state(), HttpConnection::Closed);
	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 1);
	QVERIFY(!isClientConnected());
}

void HttpConnectionTest::testPipelinedRequests()
{
	// It should support pipelined request, and correctly handle them one at a time.
	clientWrite("GET /first HTTP/1.1\r\n\r\nGET /second HTTP/1.1\r\n\r\nGET /third HTTP/1.1\r\n\r\n");
	clientFlush();

	QCOMPARE(connection->state(), HttpConnection::SendingHeaders);
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);
	connection->writeResponse(200);
	wait();

	QCOMPARE((int)connection->state(), (int)HttpConnection::SendingHeaders);
	QCOMPARE(readySpy->size(), 2);
	QCOMPARE(completedSpy->size(), 1);
	QCOMPARE(closedSpy->size(), 0);
	connection->writeResponse(302);

	QCOMPARE(connection->state(), HttpConnection::SendingHeaders);
	QCOMPARE(readySpy->size(), 3);
	QCOMPARE(completedSpy->size(), 2);
	QCOMPARE(closedSpy->size(), 0);
	connection->writeResponse(304);

	QCOMPARE(connection->state(), HttpConnection::ReceivingHeaders);
	QCOMPARE(readySpy->size(), 3);
	QCOMPARE(completedSpy->size(), 3);
	QCOMPARE(closedSpy->size(), 0);

	wait(50);

	QByteArray receivedResponse = clientReadAll();
	int firstResponseIndex = receivedResponse.indexOf("200");
	int secondResponseIndex = receivedResponse.indexOf("302");
	int thirdResponseIndex = receivedResponse.indexOf("304");
	QVERIFY(firstResponseIndex > 0 && firstResponseIndex < secondResponseIndex);
	QVERIFY(secondResponseIndex > 0 && secondResponseIndex < thirdResponseIndex);
}

void HttpConnectionTest::testClientExpects100Continue()
{
	clientWrite("POST /somefile HTTP/1.1\r\nContent-length: 5\r\nExpect: 100-continue\r\n\r\n");
	clientFlush();

	// The server should send a 100 response right away, without waiting for data.
	QCOMPARE(connection->state(), HttpConnection::ReceivingContent);
	QCOMPARE(clientReadAll(), QByteArray("HTTP/1.1 100 Continue\r\n\r\n"));
	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);

	clientWrite("hello"); clientFlush();

	QCOMPARE(connection->state(), HttpConnection::SendingHeaders);
	QCOMPARE(connection->requestContent(), QByteArray("hello"));
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);
}

void HttpConnectionTest::testHeadShouldNotSendResponseContent()
{
	clientWrite("HEAD / HTTP/1.0\r\n");
	clientWrite("\r\n"); clientFlush();

	connection->writeResponse(200, HttpHeaderCollection() << HttpHeader("Some-Header", "Some Value"), "response content");

	QCOMPARE(clientReadAll(), QByteArray("HTTP/1.0 200 OK\r\nSome-Header: Some Value\r\nContent-Length: 16\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n"));
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
}

void HttpConnectionTest::testWriteIncrementalResponseContent()
{
	clientWrite("GET / HTTP/1.0\r\n");
	clientWrite("\r\n"); clientFlush();

	connection->writeHeaders(200, HttpHeaderCollection() << HttpHeader("Content-Length", "10"));

	QVERIFY(clientReadAll().startsWith("HTTP/1.0 200"));
	QCOMPARE(connection->state(), HttpConnection::SendingContent);
	QVERIFY(isClientConnected());
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);

	connection->writeContent("hello");
	QCOMPARE(clientReadAll(), QByteArray("hello"));
	QCOMPARE(connection->state(), HttpConnection::SendingContent);
	QVERIFY(isClientConnected());
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);

	connection->writeContent("th");
	QCOMPARE(clientReadAll(), QByteArray("th"));
	QCOMPARE(connection->state(), HttpConnection::SendingContent);
	QVERIFY(isClientConnected());
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);

	connection->writeContent("ere");
	QCOMPARE(clientReadAll(), QByteArray("ere"));
	QCOMPARE(connection->state(), HttpConnection::Closed);
	QVERIFY(!isClientConnected());
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
	QCOMPARE(closedSpy->size(), 1);

	// Next, try sending an incremental response without specifying the content-length.
	cleanup(); init();

	clientWrite("GET / HTTP/1.0\r\n");
	clientWrite("\r\n"); clientFlush();

	connection->writeHeaders(200);

	QByteArray sentHeaders = clientReadAll();
	QVERIFY(!sentHeaders.toLower().contains("content-length"));

	QCOMPARE(connection->state(), HttpConnection::SendingContent);
	QVERIFY(isClientConnected());
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);

	connection->writeContent("1234");
	QCOMPARE(clientReadAll(), QByteArray("1234"));
	QCOMPARE(connection->state(), HttpConnection::SendingContent);
	QVERIFY(isClientConnected());
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);

	connection->writeContent("567890");
	QCOMPARE(clientReadAll(), QByteArray("567890"));
	QCOMPARE(connection->state(), HttpConnection::SendingContent);
	QVERIFY(isClientConnected());
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);

	QByteArray expected = QByteArray(32 * 1024, '*');
	connection->writeContent(expected);
	connection->flush();
	wait();
	QByteArray actual = clientReadAll();
	QCOMPARE(actual, expected);
	QCOMPARE(connection->state(), HttpConnection::SendingContent);
	QVERIFY(isClientConnected());
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);

	connection->endContent();
	QCOMPARE(clientReadAll(), QByteArray());
	QCOMPARE(connection->state(), HttpConnection::Closed);
	QVERIFY(!isClientConnected());
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
	QCOMPARE(closedSpy->size(), 1);
}

void HttpConnectionTest::testWriteChunkedResponseContent()
{
	clientWrite("GET / HTTP/1.1\r\n");
	clientWrite("\r\n"); clientFlush();

	connection->writeHeaders(200, HttpHeaderCollection() << HttpHeader("Transfer-Encoding", "Chunked"));
	QByteArray clientReceived = clientReadAll();

	QVERIFY(clientReceived.startsWith("HTTP/1.1 200"));
	QVERIFY(clientReceived.toLower().contains("transfer-encoding: chunked\r\n"));
	QCOMPARE(connection->state(), HttpConnection::SendingContent);
	QVERIFY(isClientConnected());
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);

	connection->writeContent("hello");
	QCOMPARE(clientReadAll(), QByteArray("5\r\nhello\r\n"));
	QCOMPARE(connection->state(), HttpConnection::SendingContent);

	connection->writeContent("there! How are you?");
	QCOMPARE(clientReadAll(), QByteArray("13\r\nthere! How are you?\r\n"));
	QCOMPARE(connection->state(), HttpConnection::SendingContent);

	connection->writeContent(QByteArray());
	QCOMPARE(clientReadAll(), QByteArray());
	QCOMPARE(connection->state(), HttpConnection::SendingContent);

	connection->writeContent("1234567890");
	QCOMPARE(clientReadAll(), QByteArray("a\r\n1234567890\r\n"));
	QCOMPARE(connection->state(), HttpConnection::SendingContent);

	connection->endContent();
	QCOMPARE(clientReadAll(), QByteArray("0\r\n\r\n"));
	QCOMPARE(connection->state(), HttpConnection::ReceivingHeaders);
	QVERIFY(isClientConnected());
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
	QCOMPARE(closedSpy->size(), 0);

	// Trying to specify chunked encoding when there is no data to send does not make sense.
	clientWrite("GET / HTTP/1.1\r\n");
	clientWrite("\r\n"); clientFlush();
	QCOMPARE(readySpy->size(), 2);
	QCOMPARE(completedSpy->size(), 1);
	QCOMPARE(closedSpy->size(), 0);
	connection->writeResponse(200, HttpHeaderCollection() << HttpHeader("Transfer-Encoding", "Chunked"), QByteArray());
	clientReceived = clientReadAll().toLower();
	QVERIFY(clientReceived.contains("content-length: 0"));
	QVERIFY(!clientReceived.contains("chunked"));
	QCOMPARE(readySpy->size(), 2);
	QCOMPARE(completedSpy->size(), 2);
	QCOMPARE(closedSpy->size(), 0);

	// Using chunked encoding does nothing on HTTP/1.0.
	clientWrite("GET / HTTP/1.0\r\n");
	clientWrite("\r\n"); clientFlush();
	QCOMPARE(readySpy->size(), 3);
	QCOMPARE(completedSpy->size(), 2);
	QCOMPARE(closedSpy->size(), 0);
	connection->writeResponse(200, HttpHeaderCollection() << HttpHeader("Transfer-Encoding", "Chunked"), QByteArray("Hello"));
	clientReceived = clientReadAll().toLower();
	QVERIFY(!clientReceived.contains("chunked"));
	QCOMPARE(readySpy->size(), 3);
	QCOMPARE(completedSpy->size(), 3);
	QCOMPARE(closedSpy->size(), 1);
}

void HttpConnectionTest::testWriteResponseWithoutRequest()
{
	QVERIFY(connection->state() == HttpConnection::ReceivingHeaders); // Precondition check.
	connection->writeResponse(200, HttpHeaderCollection(), "hello");
	wait();
	QCOMPARE(connection->state(), HttpConnection::ReceivingHeaders);
	QVERIFY(isClientConnected());
	QCOMPARE(clientReadAll(), QByteArray());

	clientWrite("GET / HTTP/1.0\r\n");
	clientWrite("Content-Length: 5\r\n");
	clientWrite("\r\n"); clientFlush();

	QVERIFY(connection->state() == HttpConnection::ReceivingContent);
	connection->writeResponse(200, HttpHeaderCollection(), "hello");
	wait();
	QCOMPARE(connection->state(), HttpConnection::ReceivingContent);
	QVERIFY(isClientConnected());
	QCOMPARE(clientReadAll(), QByteArray());

	clientWrite("hello"); clientFlush();
	QVERIFY(connection->state() == HttpConnection::SendingHeaders);
}

void HttpConnectionTest::testMultipacketResponse()
{
	// This is to test whether the HttpConnection correctly drains its send buffers
	// before closing the connection.
	clientWrite("GET / HTTP/1.0\r\n");
	clientWrite("\r\n"); clientFlush();

	const int payloadSize = 32 * 1024 * 1024; // Needs to be very, very large for it to work in this test.
	QByteArray r(payloadSize, '-');
	QVERIFY(r.size() == payloadSize); // Precondition: check that the alloc worked.

	connection->writeHeaders(200, HttpHeaderCollection() << HttpHeader("Content-Length", QByteArray::number(r.size())));
	QVERIFY(clientReadAll().size() > 32);
	QVERIFY(connection->outputDevice()->bytesToWrite() == 0); // Precondition: all headers made it to the client.

	connection->writeContent(r);

	QByteArray receivedData; receivedData.reserve(r.size());
	while (isClientConnected() || receivedData.size() < r.size())
		receivedData.append(clientReadAll());

	QCOMPARE(connection->state(), HttpConnection::Closed);
	QCOMPARE(receivedData.size(), r.size());
	QCOMPARE(receivedData, r);
}

void HttpConnectionTest::testReadsRequestParams()
{
	QVERIFY(connection->requestParams().isEmpty());

	clientWrite("GET /path?first=value&second=other%20value&composite[property]=another+value&without_value&=without_name&final=true#fragment HTTP/1.1\r\n");
	clientWrite("\r\n"); clientFlush();

	QCOMPARE(connection->requestParamValue("first"), QString("value"));
	QCOMPARE(connection->requestParamValue("fIRsT"), QString("value"));
	QCOMPARE(connection->requestParamValue("second"), QString("other value"));
	QCOMPARE(connection->requestParamValue("composite[property]"), QString("another+value"));
	QCOMPARE(connection->requestParamValue("composITe[propERtY]"), QString("another+value"));
	QCOMPARE(connection->requestParamValue("final"), QString("true"));

	Pillow::HttpParamCollection params = connection->requestParams();
	QCOMPARE(params.size(),  6);
	QCOMPARE(params.at(0).first, QString("first"));
	QCOMPARE(params.at(0).second, QString("value"));
	QCOMPARE(params.at(1).first, QString("second"));
	QCOMPARE(params.at(1).second, QString("other value"));
	QCOMPARE(params.at(2).first, QString("composite[property]"));
	QCOMPARE(params.at(2).second, QString("another+value"));
	QCOMPARE(params.at(3).first, QString("without_value"));
	QCOMPARE(params.at(3).second, QString(""));
	QCOMPARE(params.at(4).first, QString(""));
	QCOMPARE(params.at(4).second, QString("without_name"));
	QCOMPARE(params.at(5).first, QString("final"));
	QCOMPARE(params.at(5).second, QString("true"));

	connection->setRequestParam("hello", "world");
	QCOMPARE(connection->requestParams().size(), 7);
	QCOMPARE(connection->requestParamValue("heLLo"), QString("world"));
	connection->setRequestParam("hello", "there");
	QCOMPARE(connection->requestParams().size(), 7);
	QCOMPARE(connection->requestParamValue("hEllo"), QString("there"));

	// They should be cleared between requests.
	connection->writeResponse(200);
	QVERIFY(clientReadAll().startsWith("HTTP/1.1 200"));
	QCOMPARE(connection->state(), HttpConnection::ReceivingHeaders);
	clientWrite("GET /other HTTP/1.1\r\n");
	clientWrite("\r\n"); clientFlush();
	QCOMPARE(connection->requestParams().size(), 0);

	connection->writeResponse(200);
	QVERIFY(clientReadAll().startsWith("HTTP/1.1 200"));
	QCOMPARE(connection->state(), HttpConnection::ReceivingHeaders);
	clientWrite("GET /final?a=b HTTP/1.1\r\n");
	clientWrite("\r\n"); clientFlush();
	QCOMPARE(connection->requestParams().size(), 1);
	QCOMPARE(connection->requestParamValue("a"), QString("b"));

	// Some edge cases
	connection->writeResponse(200);
	QVERIFY(clientReadAll().startsWith("HTTP/1.1 200"));
	QCOMPARE(connection->state(), HttpConnection::ReceivingHeaders);
	clientWrite("GET /final?nameonly HTTP/1.1\r\n");
	clientWrite("\r\n"); clientFlush();
	QCOMPARE(connection->requestParams().size(), 1);
	QCOMPARE(connection->requestParamValue("nameonly"), QString(""));

	connection->writeResponse(200);
	QVERIFY(clientReadAll().startsWith("HTTP/1.1 200"));
	QCOMPARE(connection->state(), HttpConnection::ReceivingHeaders);
	clientWrite("GET /final?=valueonly HTTP/1.1\r\n");
	clientWrite("\r\n"); clientFlush();
	QCOMPARE(connection->requestParams().size(), 1);
	QCOMPARE(connection->requestParamValue(""), QString("valueonly"));

}

void HttpConnectionTest::testReuseRequest()
{
	reuseConnection = true;
	QObject* firstRequest = connection;

	clientWrite("GET / HTTP/1.0\r\n");
	clientWrite("\r\n"); clientFlush();
	connection->writeResponse(200, HttpHeaderCollection() << HttpHeader("Some-Header", "Some Value") << HttpHeader("Other-Header", "OtherValue"), "response content");
	QCOMPARE(completedSpy->size(), 1);

	cleanup();
	init();

	QCOMPARE(completedSpy->size(), 0);
	QVERIFY(connection == firstRequest);
	clientWrite("GET / HTTP/1.0\r\n");
	clientWrite("\r\n"); clientFlush();
	connection->writeResponse(200, HttpHeaderCollection() << HttpHeader("Some-Header", "Some Value") << HttpHeader("Other-Header", "OtherValue"), "response content");
	QCOMPARE(completedSpy->size(), 1);

	cleanup(); init();

	clientWrite("INVALID REQUEST HEADERS\r\n");
	clientWrite("Invalid headers\r\n");
	clientWrite("Should close"); clientFlush();

	QVERIFY(clientReadAll().startsWith("HTTP/1.0 400"));
	QCOMPARE(connection->state(), HttpConnection::Closed);
	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 1);
	QVERIFY(!isClientConnected());

	cleanup(); init();

	QCOMPARE(completedSpy->size(), 0);
	QVERIFY(connection == firstRequest);
	clientWrite("GET / HTTP/1.0\r\n");
	clientWrite("\r\n"); clientFlush();
	connection->writeResponse(200, HttpHeaderCollection() << HttpHeader("Some-Header", "Some Value") << HttpHeader("Other-Header", "OtherValue"), "response content");
	QCOMPARE(completedSpy->size(), 1);

	cleanup(); init();

	testInvalidRequestContent();
	QVERIFY(connection == firstRequest);

	cleanup(); init();

	testSimpleGet();
	QVERIFY(connection == firstRequest);
}

void HttpConnectionTest::benchmarkSimpleGetClose()
{
	cleanup();

	QBENCHMARK
	{
		init();

		clientWrite("GET /test/index.html?key=value#fragment HTTP/1.0\r\n");
		clientWrite("Host: example.org\r\n");
		clientWrite("X-Dummy: DummyValue\r\n");
		clientWrite("\r\n");
		clientFlush(false);

		while (connection->state() != HttpConnection::SendingHeaders)
			QCoreApplication::processEvents();

		connection->writeResponse(200, HttpHeaderCollection(), "Test");

		while (connection->state() != HttpConnection::Closed)
			QCoreApplication::processEvents();

		clientReadAll();

		while (isClientConnected())
			QCoreApplication::processEvents();

		cleanup();
	}
}

void HttpConnectionTest::benchmarkSimpleGetKeepAlive()
{
	QBENCHMARK
	{
		clientWrite("GET /test/index.html?key=value#fragment HTTP/1.1\r\n");
		clientWrite("Host: example.org\r\n");
		clientWrite("X-Dummy: DummyValue\r\n");
		clientWrite("\r\n");
		clientFlush(false);

		while (connection->state() != HttpConnection::SendingHeaders)
			QCoreApplication::processEvents();

		connection->writeResponse(200, HttpHeaderCollection(), "Test");

		while (connection->state() != HttpConnection::ReceivingHeaders)
			QCoreApplication::processEvents();

		clientReadAll();
	}
}

//
// HttpConnectionTcpSocketTest
//

HttpConnectionTcpSocketTest::HttpConnectionTcpSocketTest()
	: server(NULL), client(NULL)
{
}

void HttpConnectionTcpSocketTest::server_newConnection()
{
	QIODevice* device = server->nextPendingConnection();
	if (!reuseConnection || connection == 0)
	{
		connection = new HttpConnection();
	}
	connection->initialize(device, device);
}

void HttpConnectionTcpSocketTest::init()
{
	server = new QTcpServer();
	QVERIFY(server->listen());
	connect(server, SIGNAL(newConnection()), this, SLOT(server_newConnection()));
	client = new QTcpSocket();
	client->connectToHost(QHostAddress::LocalHost, server->serverPort());
	QVERIFY(client->waitForConnected(1000));
	while (connection == NULL || connection->inputDevice() == NULL)
		QCoreApplication::processEvents();
	readySpy = new QSignalSpy(connection, SIGNAL(requestReady(Pillow::HttpConnection*)));
	completedSpy = new QSignalSpy(connection, SIGNAL(requestCompleted(Pillow::HttpConnection*)));
	closedSpy = new QSignalSpy(connection, SIGNAL(closed(Pillow::HttpConnection*)));
}

void HttpConnectionTcpSocketTest::cleanup()
{
	if (connection && !reuseConnection) { delete connection; connection = NULL; }
	if (server) delete server; server = NULL;
	if (client) delete client; client = NULL;
	if (readySpy) delete readySpy; readySpy = NULL;
	if (completedSpy) delete completedSpy; completedSpy = NULL;
	if (closedSpy) delete closedSpy; closedSpy = NULL;
}

void HttpConnectionTcpSocketTest::clientWrite(const QByteArray &data)
{
	client->write(data);
}

void HttpConnectionTcpSocketTest::clientFlush(bool _wait /* = true */)
{
	QSignalSpy s(connection->inputDevice(), SIGNAL(readyRead()));
	client->flush();
	while (client->bytesToWrite() > 0) QCoreApplication::processEvents();
	QCoreApplication::processEvents();
	if (_wait) while (s.size() == 0) QCoreApplication::processEvents();
}

QByteArray HttpConnectionTcpSocketTest::clientReadAll()
{
	QElapsedTimer timer; timer.start();
	while (client->bytesAvailable() == 0 && !timer.hasExpired(500)) QCoreApplication::processEvents();
	return client->readAll();
}

bool HttpConnectionTcpSocketTest::isClientConnected()
{
	wait();
	return client->state() == QAbstractSocket::ConnectedState && client->isOpen();
}

void HttpConnectionTcpSocketTest::clientClose()
{
	client->disconnectFromHost();
	client->close();
	while (client->state() == QAbstractSocket::ConnectedState) wait();
}

#ifndef PILLOW_NO_SSL

//
// HttpConnectionSslSocketTest
//

#include <QtNetwork/QSslSocket>
#include <QtNetwork/QSslKey>
#include <QtNetwork/QSslCertificate>

class SslTestServer : public QTcpServer
{
public:
	HttpConnectionSslSocketTest* test;
	QSslCertificate certificate;
	QSslKey key;

protected:
	virtual void incomingConnection(int socketDescriptor)
	{
		QSslSocket* sslSocket = new QSslSocket(this);
		if (sslSocket->setSocketDescriptor(socketDescriptor))
		{
			sslSocket->setPrivateKey(key);
			sslSocket->setLocalCertificate(certificate);
			sslSocket->startServerEncryption();
			connect(sslSocket, SIGNAL(encrypted()), test, SLOT(sslSocket_encrypted()));
			connect(sslSocket, SIGNAL(sslErrors(QList<QSslError>)), test, SLOT(sslSocket_sslErrors(QList<QSslError>)));
			addPendingConnection(sslSocket);
		}
		else
		{
			delete sslSocket;
		}
	}
};

HttpConnectionSslSocketTest::HttpConnectionSslSocketTest()
	: server(NULL), client(NULL)
{
}

void HttpConnectionSslSocketTest::server_newConnection()
{
	QIODevice* device = server->nextPendingConnection();
	connection = new HttpConnection();
	connection->initialize(device, device);
}

void HttpConnectionSslSocketTest::sslSocket_encrypted()
{
}

void HttpConnectionSslSocketTest::sslSocket_sslErrors(const QList<QSslError>& )
{
}

void HttpConnectionSslSocketTest::init()
{
	QVERIFY(QFile::exists(":/test.crt"));
	QVERIFY(QFile::exists(":/test.key"));
	QFile certificateFile(":/test.crt"); QVERIFY(certificateFile.open(QIODevice::ReadOnly));
	QFile keyFile(":/test.key"); QVERIFY(keyFile.open(QIODevice::ReadOnly));
	QSslCertificate certificate(&certificateFile);
	QSslKey key(&keyFile, QSsl::Rsa);

	server = new SslTestServer();
	server->test = this;
	server->certificate = certificate;
	server->key = key;
	QVERIFY(server->listen());
	connect(server, SIGNAL(newConnection()), this, SLOT(server_newConnection()));

	client = new QSslSocket();
	client->setLocalCertificate(certificate);
	client->setPrivateKey(key);
	client->setPeerVerifyMode(QSslSocket::VerifyNone);
	client->connectToHostEncrypted("127.0.0.1", server->serverPort());
	connect(client, SIGNAL(encrypted()), this, SLOT(sslSocket_encrypted()));
	connect(client, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslSocket_sslErrors(QList<QSslError>)));
	QVERIFY(client->waitForConnected());
	while (!client->isEncrypted()) QCoreApplication::processEvents();
	QVERIFY(client->isEncrypted());

	while (connection == NULL) QCoreApplication::processEvents();
	readySpy = new QSignalSpy(connection, SIGNAL(requestReady(Pillow::HttpConnection*)));
	completedSpy = new QSignalSpy(connection, SIGNAL(requestCompleted(Pillow::HttpConnection*)));
	closedSpy = new QSignalSpy(connection, SIGNAL(closed(Pillow::HttpConnection*)));
}

void HttpConnectionSslSocketTest::cleanup()
{
	if (connection) delete connection; connection = NULL;
	if (server) delete server; server = NULL;
	if (client) delete client; client = NULL;
	if (readySpy) delete readySpy; readySpy = NULL;
	if (completedSpy) delete completedSpy; completedSpy = NULL;
	if (closedSpy) delete closedSpy; closedSpy = NULL;
}

void HttpConnectionSslSocketTest::clientWrite(const QByteArray &data)
{
	client->write(data);
}

void HttpConnectionSslSocketTest::clientFlush(bool _wait /* = true */)
{
	QSignalSpy s(connection->inputDevice(), SIGNAL(readyRead()));
	client->flush();
	while (client->bytesToWrite() > 0) QCoreApplication::processEvents();
	QCoreApplication::processEvents();
	if (_wait) while (s.size() == 0) QCoreApplication::processEvents();
}

QByteArray HttpConnectionSslSocketTest::clientReadAll()
{
	QElapsedTimer timer; timer.start();
	while (client->bytesAvailable() == 0 && !timer.hasExpired(2000)) QCoreApplication::processEvents();
	QCoreApplication::processEvents();
	return client->readAll();
}

bool HttpConnectionSslSocketTest::isClientConnected()
{
	wait();
	return client->state() == QAbstractSocket::ConnectedState && client->isOpen();
}

void HttpConnectionSslSocketTest::clientClose()
{
	client->disconnectFromHost();
	client->close();
	while (client->state() == QAbstractSocket::ConnectedState) wait();
}

#endif // !PILLOW_NO_SSL

//
// HttpConnectionLocalSocketTest
//

HttpConnectionLocalSocketTest::HttpConnectionLocalSocketTest()
	: server(NULL), client(NULL)
{
}

void HttpConnectionLocalSocketTest::server_newConnection()
{
	QIODevice* device = server->nextPendingConnection();
	connection = new HttpConnection();
	connection->initialize(device, device);
}

void HttpConnectionLocalSocketTest::init()
{
	server = new QLocalServer();
	server->removeServer("pillowtest");
	QVERIFY(server->listen("pillowtest"));
	connect(server, SIGNAL(newConnection()), this, SLOT(server_newConnection()));
	client = new QLocalSocket();
	client->connectToServer("pillowtest");
	QVERIFY(client->waitForConnected(1000));
	while (connection == NULL)
		QCoreApplication::processEvents();
	readySpy = new QSignalSpy(connection, SIGNAL(requestReady(Pillow::HttpConnection*)));
	completedSpy = new QSignalSpy(connection, SIGNAL(requestCompleted(Pillow::HttpConnection*)));
	closedSpy = new QSignalSpy(connection, SIGNAL(closed(Pillow::HttpConnection*)));
}

void HttpConnectionLocalSocketTest::cleanup()
{
	if (connection) delete connection; connection = NULL;
	if (server) delete server; server = NULL;
	if (client) delete client; client = NULL;
	if (readySpy) delete readySpy; readySpy = NULL;
	if (completedSpy) delete completedSpy; completedSpy = NULL;
	if (closedSpy) delete closedSpy; closedSpy = NULL;
}

void HttpConnectionLocalSocketTest::clientWrite(const QByteArray &data)
{
	client->write(data);
}

void HttpConnectionLocalSocketTest::clientFlush(bool _wait /* = true */)
{
	QSignalSpy s(connection->inputDevice(), SIGNAL(readyRead()));
	client->flush();
	while (client->bytesToWrite() > 0) QCoreApplication::processEvents();
	QCoreApplication::processEvents();
	if (_wait) while (s.size() == 0) QCoreApplication::processEvents();
}

QByteArray HttpConnectionLocalSocketTest::clientReadAll()
{
	QElapsedTimer timer; timer.start();
	while (client->bytesAvailable() == 0 && !timer.hasExpired(500)) QCoreApplication::processEvents();
	return client->readAll();
}

void HttpConnectionLocalSocketTest::clientClose()
{
	client->close();
}

bool HttpConnectionLocalSocketTest::isClientConnected()
{
	wait(20); // On Windows, we must wait quite a bit for the client connection state to be updated.
	return client->state() == QLocalSocket::ConnectedState && client->isOpen();
}


//
// HttpConnectionBufferTest
//

HttpConnectionBufferTest::HttpConnectionBufferTest()
	: inputBuffer(NULL), outputBuffer(NULL)
{
}

void HttpConnectionBufferTest::init()
{
	inputBuffer = new QBuffer(); inputBuffer->open(QIODevice::ReadWrite);
	outputBuffer = new QBuffer(); outputBuffer->open(QIODevice::ReadWrite);

	connection = new HttpConnection(NULL);
	connection->initialize(inputBuffer, outputBuffer);

	readySpy = new QSignalSpy(connection, SIGNAL(requestReady(Pillow::HttpConnection*)));
	completedSpy = new QSignalSpy(connection, SIGNAL(requestCompleted(Pillow::HttpConnection*)));
	closedSpy = new QSignalSpy(connection, SIGNAL(closed(Pillow::HttpConnection*)));
}

void HttpConnectionBufferTest::cleanup()
{
	if (connection) delete connection; connection = NULL;
	if (inputBuffer) delete inputBuffer; inputBuffer = NULL;
	if (outputBuffer) delete outputBuffer; outputBuffer = NULL;
	if (readySpy) delete readySpy; readySpy = NULL;
	if (completedSpy) delete completedSpy; completedSpy = NULL;
	if (closedSpy) delete closedSpy; closedSpy = NULL;
}

void HttpConnectionBufferTest::clientWrite(const QByteArray &data)
{
	inputBuffer->write(data);
}

void HttpConnectionBufferTest::clientFlush(bool _wait /* = true */)
{
	if (inputBuffer->isOpen()) inputBuffer->seek(0);
	QCoreApplication::processEvents();
	if (_wait) wait();
	if (inputBuffer->isOpen()) inputBuffer->seek(0);
	inputBuffer->buffer().clear();
}

QByteArray HttpConnectionBufferTest::clientReadAll()
{
	QCoreApplication::processEvents();
	if (outputBuffer->isOpen()) outputBuffer->seek(0);
	QByteArray data = outputBuffer->buffer();
	if (outputBuffer->isOpen()) outputBuffer->seek(0);
	outputBuffer->buffer().clear();
	return data;
}

void HttpConnectionBufferTest::clientClose()
{
	inputBuffer->close();
	outputBuffer->close();
}

bool HttpConnectionBufferTest::isClientConnected()
{
	return inputBuffer->isOpen();
}


