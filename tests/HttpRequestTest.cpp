#include "HttpRequestTest.h"
#include "HttpRequest.h"
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QLocalSocket>
#include <QtNetwork/QSslSocket>
#include <QtNetwork/QSslKey>
#include <QtNetwork/QSslCertificate>
#include <QtCore/QFile>
#include <QtCore/QBuffer>
using namespace Pillow;

static void wait(int milliseconds = 5)
{
	QElapsedTimer t; t.start();
	do
	{
		QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
	} 
	while (t.elapsed() < milliseconds);
}

HttpRequestTest::HttpRequestTest()
    : request(NULL), readySpy(NULL), completedSpy(NULL), closedSpy(NULL)
{
	qRegisterMetaType<Pillow::HttpRequest*>("Pillow::HttpRequest*");
}

void HttpRequestTest::init()
{
}

void HttpRequestTest::cleanup()
{
}

void HttpRequestTest::testInitialState()
{
	QCOMPARE(request->state(), HttpRequest::ReceivingHeaders);
	QCOMPARE(request->requestMethod(), QByteArray());
	QCOMPARE(request->requestUri(), QByteArray());
	QCOMPARE(request->requestFragment(), QByteArray());
	QCOMPARE(request->requestPath(), QByteArray());
	QCOMPARE(request->requestQueryString(), QByteArray());
	QVERIFY(request->requestHeaders().isEmpty());
	QCOMPARE(request->requestContent(), QByteArray());
}

void HttpRequestTest::testSimpleGet()
{
	clientWrite("GET /test/index.html?key=value#fragment HTTP/1.0\r\n");
	clientWrite("Host: example.org\r\n");
	clientWrite("Weird: \r\n");
	clientWrite("X-Dummy: DummyValue\r\n");
	clientWrite("\r\n"); clientFlush();

	QCOMPARE(request->state(), HttpRequest::SendingHeaders);
	QCOMPARE(request->requestMethod(), QByteArray("GET"));
	QCOMPARE(request->requestUri(), QByteArray("/test/index.html?key=value"));
	QCOMPARE(request->requestFragment(), QByteArray("fragment"));
	QCOMPARE(request->requestPath(), QByteArray("/test/index.html"));
	QCOMPARE(request->requestQueryString(), QByteArray("key=value"));
	QCOMPARE(request->requestHttpVersion(), QByteArray("HTTP/1.0"));
	QCOMPARE(request->requestHeaders().size(), 3);
	QCOMPARE(request->requestHeaders().at(0).first, QByteArray("Host"));
	QCOMPARE(request->requestHeaders().at(0).second, QByteArray("example.org"));
	QCOMPARE(request->requestHeaders().at(1).first, QByteArray("Weird"));
	QCOMPARE(request->requestHeaders().at(1).second, QByteArray(""));
	QCOMPARE(request->requestHeaders().at(2).first, QByteArray("X-Dummy"));
	QCOMPARE(request->requestHeaders().at(2).second, QByteArray("DummyValue"));
	QCOMPARE(request->getRequestHeaderValue("x-DUmmY"), QByteArray("DummyValue"));
	QCOMPARE(request->getRequestHeaderValue("missing"), QByteArray());
	QCOMPARE(request->requestContent(), QByteArray());
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);
	QVERIFY(isClientConnected());

	// Safety test: check that all returned QByteArrays are null-terminated. Even if some internally come
	// from QByteArray::fromRawData, we still want to provide null-terminated QByteArrays.
	QVERIFY(qstrcmp(request->requestMethod().constData(), "GET") == 0);
	QVERIFY(qstrcmp(request->requestUri().constData(), "/test/index.html?key=value") == 0);
	QVERIFY(qstrcmp(request->requestFragment().constData(), "fragment") == 0);
	QVERIFY(qstrcmp(request->requestPath().constData(), "/test/index.html") == 0);
	QVERIFY(qstrcmp(request->requestQueryString().constData(), "key=value") == 0);
	QVERIFY(qstrcmp(request->requestHttpVersion().constData(), "HTTP/1.0") == 0);
	QVERIFY(qstrcmp(request->requestHeaders().at(0).first.constData(), "Host") == 0);
	QVERIFY(qstrcmp(request->requestHeaders().at(0).second.constData(), "example.org") == 0);
	QVERIFY(qstrcmp(request->requestHeaders().at(1).first.constData(), "Weird") == 0);
	QVERIFY(qstrcmp(request->requestHeaders().at(1).second.constData(),"") == 0);
}

void HttpRequestTest::testSimplePost()
{
	clientWrite("POST /postpath#frag HTTP/1.0\r\n");
	clientWrite("Host: example.org\r\n");
	clientWrite("conTEnt-leNGtH: 11\r\n");
	clientWrite("\r\n");
	clientWrite("contentdata"); clientFlush();

	QCOMPARE(request->state(), HttpRequest::SendingHeaders);
	QCOMPARE(request->requestMethod(), QByteArray("POST"));
	QCOMPARE(request->requestUri(), QByteArray("/postpath"));
	QCOMPARE(request->requestFragment(), QByteArray("frag"));
	QCOMPARE(request->requestPath(), QByteArray("/postpath"));
	QCOMPARE(request->requestQueryString(), QByteArray());
	QCOMPARE(request->requestHeaders().size(), 2);
	QCOMPARE(request->requestHeaders().at(0).first, QByteArray("Host"));
	QCOMPARE(request->requestHeaders().at(0).second, QByteArray("example.org"));
	QCOMPARE(request->requestHeaders().at(1).first, QByteArray("conTEnt-leNGtH"));
	QCOMPARE(request->requestHeaders().at(1).second, QByteArray("11"));
	QCOMPARE(request->requestContent(), QByteArray("contentdata"));
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);
	QVERIFY(isClientConnected());

	QVERIFY(qstrcmp(request->requestQueryString().constData(), "") == 0);
	QVERIFY(qstrcmp(request->requestFragment().constData(), "frag") == 0);
}

void HttpRequestTest::testIncrementalPost()
{
	clientWrite("POST /test/stuff HTTP/1.0\r\n"); clientFlush();

	QCOMPARE(request->state(), HttpRequest::ReceivingHeaders);

	clientWrite("Content-Length: 8\r\n");
	clientWrite("\r\n"); clientFlush();

	QCOMPARE(request->state(), HttpRequest::ReceivingContent);
	QCOMPARE(request->requestMethod(), QByteArray("POST"));
	QCOMPARE(request->requestUri(), QByteArray("/test/stuff"));
	QCOMPARE(request->requestFragment(), QByteArray());
	QCOMPARE(request->requestPath(), QByteArray("/test/stuff"));
	QCOMPARE(request->requestQueryString(), QByteArray());
	QCOMPARE(request->requestHeaders().size(), 1);
	QCOMPARE(request->requestHeaders().at(0).first, QByteArray("Content-Length"));
	QCOMPARE(request->requestHeaders().at(0).second, QByteArray("8"));
	QCOMPARE(request->requestContent(), QByteArray()); // It should not mistake headers for request content
	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);

	// Send incomplete post data.
	clientWrite("some"); clientFlush();

	QCOMPARE(request->state(), HttpRequest::ReceivingContent);
	QCOMPARE(request->requestMethod(), QByteArray("POST"));		// Receiving some content should not clear the request header info.
	QCOMPARE(request->requestUri(), QByteArray("/test/stuff"));
	QCOMPARE(request->requestFragment(), QByteArray());
	QCOMPARE(request->requestPath(), QByteArray("/test/stuff"));
	QCOMPARE(request->requestQueryString(), QByteArray());
	QCOMPARE(request->requestHeaders().size(), 1);
	QCOMPARE(request->requestHeaders().at(0).first, QByteArray("Content-Length"));
	QCOMPARE(request->requestHeaders().at(0).second, QByteArray("8"));
	QCOMPARE(request->requestContent(), QByteArray("some"));
	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);

	// Send remaining post data.
	clientWrite("data"); clientFlush();

	QCOMPARE(request->state(), HttpRequest::SendingHeaders);
	QCOMPARE(request->requestContent(), QByteArray("somedata"));
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);
	QVERIFY(isClientConnected());
}

void HttpRequestTest::testInvalidRequestHeaders()
{
	clientWrite("INVALID REQUEST HEADERS\r\n");
	clientWrite("Invalid headers\r\n");
	clientWrite("Should close"); clientFlush();

	QCOMPARE(request->state(), HttpRequest::Closed);
	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 1);
	QVERIFY(clientReadAll().startsWith("HTTP/1.0 400"));
	QVERIFY(!isClientConnected());
}

void HttpRequestTest::testOversizedRequestHeaders()
{
	QByteArray data;
	data.append("GET /test/stuff HTTP/1.0\r\n")
	    .append("Some-Header: ").append(QByteArray(HttpRequest::MaximumRequestHeaderLength, 'a')).append("\r\n")
	    .append("\r\n");
	clientWrite(data); 	clientFlush(); 

	QCOMPARE(request->state(), HttpRequest::Closed);
	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 1);
	QVERIFY(clientReadAll().startsWith("HTTP/1.0 400")); // Bad Request
	QVERIFY(!isClientConnected());
}

void HttpRequestTest::testInvalidRequestContent()
{
	clientWrite("POST / HTTP/1.0\r\n");
	clientWrite("Content-Length: abcdefg\r\n");
	clientWrite("\r\n");
	clientWrite("Hello"); clientFlush();
	
	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 1);
	QVERIFY(clientReadAll().startsWith("HTTP/1.0 413")); // Too large
	QVERIFY(!isClientConnected());

	cleanup(); init();
	clientWrite("POST / HTTP/1.0\r\n");
	clientWrite("Content-Length: -1\r\n");
	clientWrite("\r\n");
	clientWrite("Hello"); clientFlush();

	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 1);
	QVERIFY(clientReadAll().startsWith("HTTP/1.0 400")); // Bad request.
	QVERIFY(!isClientConnected());

	cleanup(); init();
	clientWrite("POST / HTTP/1.0\r\n");
	clientWrite("Content-Length: 204800000000\r\n"); // Too large
	clientWrite("\r\n");
	clientWrite("Hello"); clientFlush();

	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 1);
	QVERIFY(clientReadAll().startsWith("HTTP/1.0 413"));
	QVERIFY(!isClientConnected());

	// In the case where the client sends more data than it is supposed too, the request shall
	// process the request while honoring the expected content-length. Extra data will be kept
	// in buffer for a future request (which will most likely end up being a 400 bad request).
	cleanup(); init();
	clientWrite("POST / HTTP/1.0\r\n");
	clientWrite("Content-Length: 3\r\n"); // Too large!
	clientWrite("\r\n");
	clientWrite("Hello"); clientFlush();

	QCOMPARE(request->state(), HttpRequest::SendingHeaders);
	QCOMPARE(request->requestContent(), QByteArray("Hel"));
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);
	QVERIFY(isClientConnected());
}

void HttpRequestTest::testWriteSimpleResponse()
{
	clientWrite("GET / HTTP/1.0\r\n");
	clientWrite("\r\n"); clientFlush();

	request->writeResponse(200, HttpHeaderCollection() << HttpHeader("Some-Header", "Some Value") << HttpHeader("Other-Header", "OtherValue"), "response content");
	wait();

	QCOMPARE(clientReadAll(), QByteArray("HTTP/1.0 200 OK\r\nSome-Header: Some Value\r\nOther-Header: OtherValue\r\nContent-Length: 16\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nresponse content"));
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
}

void HttpRequestTest::testWriteSimpleResponseString()
{
	clientWrite("GET / HTTP/1.0\r\n");
	clientWrite("\r\n"); clientFlush();

	QString content = QString::fromUtf8("résponçë côntent"); // those 16 utf8 chars -> 20 bytes
	
	request->writeResponseString(200, HttpHeaderCollection(), content );
	wait();

	QCOMPARE(clientReadAll(), QByteArray("HTTP/1.0 200 OK\r\nContent-Length: 20\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n") + content.toUtf8());
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
}

void HttpRequestTest::testConnectionKeepAlive()
{
	// For HTTP/1.1, the server should use keep-alive by default.
	clientWrite("GET /first/request?hello=world#frag HTTP/1.1\r\n");
	clientWrite("\r\n"); clientFlush(); 

	request->writeResponse(200); 
	wait();
	QVERIFY(clientReadAll().startsWith("HTTP/1.1 200"));
	QVERIFY(isClientConnected());
	QCOMPARE(request->state(), HttpRequest::ReceivingHeaders);
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
	QCOMPARE(closedSpy->size(), 0);
	QCOMPARE(request->requestUri(), QByteArray("/first/request?hello=world"));
	QCOMPARE(request->requestFragment(), QByteArray("frag"));
	QCOMPARE(request->requestPath(), QByteArray("/first/request"));
	QCOMPARE(request->requestQueryString(), QByteArray("hello=world"));

	// So it should be possible to send subsequent requests on the same connection.
	clientWrite("GET /other HTTP/1.1\r\n");
	clientWrite("\r\n"); clientFlush();

	request->writeResponse(302); 
	wait();
	QVERIFY(clientReadAll().startsWith("HTTP/1.1 302"));
	QVERIFY(isClientConnected());
	QCOMPARE(request->state(), HttpRequest::ReceivingHeaders);
	QCOMPARE(readySpy->size(), 2);
	QCOMPARE(completedSpy->size(), 2);
	QCOMPARE(closedSpy->size(), 0);
	QCOMPARE(request->requestUri(), QByteArray("/other"));
	QCOMPARE(request->requestFragment(), QByteArray());
	QCOMPARE(request->requestPath(), QByteArray("/other"));
	QCOMPARE(request->requestQueryString(), QByteArray());

	clientWrite("GET /another HTTP/1.1\r\n");
	clientWrite("\r\n"); clientFlush();	

	request->writeResponse(304); 
	wait();
	QVERIFY(clientReadAll().startsWith("HTTP/1.1 304"));
	QVERIFY(isClientConnected());
	QCOMPARE(request->state(), HttpRequest::ReceivingHeaders);
	QCOMPARE(readySpy->size(), 3);
	QCOMPARE(completedSpy->size(), 3);
	QCOMPARE(closedSpy->size(), 0);

	// For HTTP/1.0, keep-alive is done only if requested by the client.
	clientWrite("GET /another/but?http=1.0 HTTP/1.0\r\n");
	clientWrite("Connection: keep-alive\r\n");
	clientWrite("\r\n"); clientFlush(); 

	request->writeResponse(200); 
	wait();
	QVERIFY(clientReadAll().startsWith("HTTP/1.0 200"));
	QVERIFY(isClientConnected());
	QCOMPARE(request->state(), HttpRequest::ReceivingHeaders);
	QCOMPARE(readySpy->size(), 4);
	QCOMPARE(completedSpy->size(), 4);
	QCOMPARE(closedSpy->size(), 0);
	QCOMPARE(request->requestUri(), QByteArray("/another/but?http=1.0"));
	QCOMPARE(request->requestFragment(), QByteArray());
	QCOMPARE(request->requestPath(), QByteArray("/another/but"));
	QCOMPARE(request->requestQueryString(), QByteArray("http=1.0"));

	clientWrite("GET /another/but/http/1.0/without/keepalive HTTP/1.0\r\n");
	clientWrite("\r\n"); clientFlush(); 

	request->writeResponse(200); 
	wait();
	QVERIFY(clientReadAll().startsWith("HTTP/1.0 200"));
	QVERIFY(!isClientConnected());
	QCOMPARE(request->state(), HttpRequest::Closed);
	QCOMPARE(readySpy->size(), 5);
	QCOMPARE(completedSpy->size(), 5);
	QCOMPARE(closedSpy->size(), 1);
}

void HttpRequestTest::testConnectionClose()
{
	// The server should close the connection if the client specifies Connection: close for any protocol version.
	clientWrite("GET / HTTP/1.1\r\n");
	clientWrite("Connection: close\r\n");
	clientWrite("\r\n"); clientFlush(); 

	request->writeResponse(200); 
	wait();
	QVERIFY(clientReadAll().startsWith("HTTP/1.1 200"));
	QVERIFY(!isClientConnected());
	QCOMPARE(request->state(), HttpRequest::Closed);
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
	QCOMPARE(closedSpy->size(), 1);

	cleanup(); init();
	clientWrite("GET /another HTTP/1.0\r\n");
	clientWrite("Connection: close\r\n");
	clientWrite("\r\n"); clientFlush(); 

	request->writeResponse(302);
	wait();
	QVERIFY(clientReadAll().startsWith("HTTP/1.0 302"));
	QVERIFY(!isClientConnected());
	QCOMPARE(request->state(), HttpRequest::Closed);
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
	QCOMPARE(closedSpy->size(), 1);

	// The server should also close the connection if the responbse specifies Connection: close.
	cleanup(); init();
	clientWrite("GET /other HTTP/1.1\r\n");
	clientWrite("\r\n"); clientFlush(); 

	request->writeResponse(304, HttpHeaderCollection() << HttpHeader("Connection", "close")); 
	wait();
	QVERIFY(clientReadAll().startsWith("HTTP/1.1 304"));
	QVERIFY(!isClientConnected());
	QCOMPARE(request->state(), HttpRequest::Closed);
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
	QCOMPARE(closedSpy->size(), 1);
}

void HttpRequestTest::testClientClosesConnectionEarly()
{
	clientWrite("GET / HTTP/1.1\r\n");
	clientClose();
	while (request->state() != HttpRequest::Closed) 
		wait();

	QCOMPARE(request->state(), HttpRequest::Closed);
	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 1);
	QVERIFY(!isClientConnected());
}

void HttpRequestTest::testPipelinedRequests()
{
	// It should support pipelined request, and correctly handle them one at a time.
	clientWrite("GET /first HTTP/1.1\r\n\r\nGET /second HTTP/1.1\r\n\r\nGET /third HTTP/1.1\r\n\r\n");
	clientFlush(); 
	
	QCOMPARE(request->state(), HttpRequest::SendingHeaders);
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);
	request->writeResponse(200);
	wait();

	QCOMPARE((int)request->state(), (int)HttpRequest::SendingHeaders);
	QCOMPARE(readySpy->size(), 2);
	QCOMPARE(completedSpy->size(), 1);
	QCOMPARE(closedSpy->size(), 0);
	request->writeResponse(302);

	QCOMPARE(request->state(), HttpRequest::SendingHeaders);
	QCOMPARE(readySpy->size(), 3);
	QCOMPARE(completedSpy->size(), 2);
	QCOMPARE(closedSpy->size(), 0);
	request->writeResponse(304);

	QCOMPARE(request->state(), HttpRequest::ReceivingHeaders);
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

void HttpRequestTest::testClientExpects100Continue()
{
	clientWrite("POST /somefile HTTP/1.1\r\nContent-length: 5\r\nExpect: 100-continue\r\n\r\n");
	clientFlush(); 

	// The server should send a 100 response right away, without waiting for data.
	QCOMPARE(request->state(), HttpRequest::ReceivingContent);
	QCOMPARE(clientReadAll(), QByteArray("HTTP/1.1 100 Continue\r\n\r\n"));
	QCOMPARE(readySpy->size(), 0);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);

	clientWrite("hello"); clientFlush(); 

	QCOMPARE(request->state(), HttpRequest::SendingHeaders);
	QCOMPARE(request->requestContent(), QByteArray("hello"));
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);
}

void HttpRequestTest::testHeadShouldNotSendResponseContent()
{
	clientWrite("HEAD / HTTP/1.0\r\n");
	clientWrite("\r\n"); clientFlush();

	request->writeResponse(200, HttpHeaderCollection() << HttpHeader("Some-Header", "Some Value"), "response content");
	wait();

	QCOMPARE(clientReadAll(), QByteArray("HTTP/1.0 200 OK\r\nSome-Header: Some Value\r\nContent-Length: 16\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n"));
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
}

void HttpRequestTest::testWriteIncrementalResponseContent()
{
	clientWrite("GET / HTTP/1.0\r\n");
	clientWrite("\r\n"); clientFlush();

	request->writeHeaders(200, HttpHeaderCollection() << HttpHeader("Content-Length", "10"));
	wait();
	
	QCOMPARE(request->state(), HttpRequest::SendingContent);
	QVERIFY(isClientConnected());
	QVERIFY(clientReadAll().startsWith("HTTP/1.0 200"));
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);

	request->writeContent("hello");	
	wait(50);
	QCOMPARE(request->state(), HttpRequest::SendingContent);
	QVERIFY(isClientConnected());
	QCOMPARE(clientReadAll(), QByteArray("hello"));
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);
	
	request->writeContent("th");
	wait();
	QCOMPARE(request->state(), HttpRequest::SendingContent);
	QVERIFY(isClientConnected());
	QCOMPARE(clientReadAll(), QByteArray("th"));
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 0);
	QCOMPARE(closedSpy->size(), 0);

	request->writeContent("ere");
	wait();
	QCOMPARE(request->state(), HttpRequest::Closed);
	QVERIFY(!isClientConnected());
	QCOMPARE(clientReadAll(), QByteArray("ere"));
	QCOMPARE(readySpy->size(), 1);
	QCOMPARE(completedSpy->size(), 1);
	QCOMPARE(closedSpy->size(), 1);
}

void HttpRequestTest::testWriteResponseWithoutRequest()
{
	QVERIFY(request->state() == HttpRequest::ReceivingHeaders); // Precondition check.
	request->writeResponse(200, HttpHeaderCollection(), "hello");
	wait();
	QCOMPARE(request->state(), HttpRequest::ReceivingHeaders);
	QVERIFY(isClientConnected());
	QCOMPARE(clientReadAll(), QByteArray());

	clientWrite("GET / HTTP/1.0\r\n");
	clientWrite("Content-Length: 5\r\n");
	clientWrite("\r\n"); clientFlush();

	QVERIFY(request->state() == HttpRequest::ReceivingContent); 
	request->writeResponse(200, HttpHeaderCollection(), "hello");
	wait();
	QCOMPARE(request->state(), HttpRequest::ReceivingContent);
	QVERIFY(isClientConnected());
	QCOMPARE(clientReadAll(), QByteArray());
	
	clientWrite("hello"); clientFlush();
	QVERIFY(request->state() == HttpRequest::SendingHeaders); 
}

void HttpRequestTest::testMultipacketResponse()
{
	// This is to test whether the HttpRequest correctly drains its send buffers
	// before closing the connection.
	clientWrite("GET / HTTP/1.0\r\n");
	clientWrite("\r\n"); clientFlush();

	const int payloadSize = 32 * 1024 * 1024; // Needs to be very, very large for it to work in this test.
	QByteArray r(payloadSize, '-'); 
	QVERIFY(r.size() == payloadSize); // Precondition: check that the alloc worked.

	request->writeHeaders(200, HttpHeaderCollection() << HttpHeader("Content-Length", QByteArray::number(r.size())));
	wait();
	QVERIFY(clientReadAll().size() > 32);
	QVERIFY(request->outputDevice()->bytesToWrite() == 0); // Precondition: all headers made it to the client.
	
	request->writeContent(r);

	QByteArray receivedData; receivedData.reserve(r.size());
	while (isClientConnected() || receivedData.size() < r.size())
	{
		wait();
		receivedData.append(clientReadAll());
	}
	
	QCOMPARE(request->state(), HttpRequest::Closed);
	QCOMPARE(receivedData.size(), r.size());
	QCOMPARE(receivedData, r);
}

void HttpRequestTest::benchmarkSimpleGetClose()
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

		while (request->state() != HttpRequest::SendingHeaders)
			QCoreApplication::processEvents();

		request->writeResponse(200, HttpHeaderCollection(), "Test");

		while (request->state() != HttpRequest::Closed)
			QCoreApplication::processEvents();

		clientReadAll();		

		while (isClientConnected())
			QCoreApplication::processEvents();

		cleanup();
	}
}

void HttpRequestTest::benchmarkSimpleGetKeepAlive()
{
	QBENCHMARK
	{
		clientWrite("GET /test/index.html?key=value#fragment HTTP/1.1\r\n");
		clientWrite("Host: example.org\r\n");
		clientWrite("X-Dummy: DummyValue\r\n");
		clientWrite("\r\n"); 
		clientFlush(false);

		while (request->state() != HttpRequest::SendingHeaders)
			QCoreApplication::processEvents();

		request->writeResponse(200, HttpHeaderCollection(), "Test");

		while (request->state() != HttpRequest::ReceivingHeaders)
			QCoreApplication::processEvents();

		clientReadAll();		
	}
}

//
// HttpRequestTcpSocketTest
//

HttpRequestTcpSocketTest::HttpRequestTcpSocketTest()
	: server(NULL), client(NULL)
{
}

void HttpRequestTcpSocketTest::server_newConnection()
{
	request = new HttpRequest(server->nextPendingConnection());
}

void HttpRequestTcpSocketTest::init()
{
	server = new QTcpServer();
	QVERIFY(server->listen());
	connect(server, SIGNAL(newConnection()), this, SLOT(server_newConnection()));	
	client = new QTcpSocket();
	client->connectToHost(QHostAddress::LocalHost, server->serverPort());
	QVERIFY(client->waitForConnected(1000));
	while (request == NULL)
		QCoreApplication::processEvents();
	readySpy = new QSignalSpy(request, SIGNAL(ready(Pillow::HttpRequest*)));
	completedSpy = new QSignalSpy(request, SIGNAL(completed(Pillow::HttpRequest*)));
	closedSpy = new QSignalSpy(request, SIGNAL(closed(Pillow::HttpRequest*)));
}

void HttpRequestTcpSocketTest::cleanup()
{
	if (request) delete request; request = NULL;
	if (server) delete server; server = NULL;
	if (client) delete client; client = NULL;
	if (readySpy) delete readySpy; readySpy = NULL;
	if (completedSpy) delete completedSpy; completedSpy = NULL;
	if (closedSpy) delete closedSpy; closedSpy = NULL;
}

void HttpRequestTcpSocketTest::clientWrite(const QByteArray &data)
{
	client->write(data);
}

void HttpRequestTcpSocketTest::clientFlush(bool _wait /* = true */)
{
	client->flush();
	if (_wait)
		wait();
}

QByteArray HttpRequestTcpSocketTest::clientReadAll()
{
	return client->readAll();
}

bool HttpRequestTcpSocketTest::isClientConnected()
{
	return client->state() == QAbstractSocket::ConnectedState && client->isOpen();
}

void HttpRequestTcpSocketTest::clientClose()
{
	client->disconnectFromHost();
	client->close();
	while (client->state() == QAbstractSocket::ConnectedState) wait();
}

//
// HttpRequestSslSocketTest
//

class SslTestServer : public QTcpServer
{
public:
	HttpRequestSslSocketTest* test;
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

HttpRequestSslSocketTest::HttpRequestSslSocketTest()
	: server(NULL), client(NULL)
{
}

void HttpRequestSslSocketTest::server_newConnection()
{
	request = new HttpRequest(server->nextPendingConnection());
}

void HttpRequestSslSocketTest::sslSocket_encrypted()
{
}

void HttpRequestSslSocketTest::sslSocket_sslErrors(const QList<QSslError>& )
{
}

void HttpRequestSslSocketTest::init()
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

	while (request == NULL) QCoreApplication::processEvents();
	readySpy = new QSignalSpy(request, SIGNAL(ready(Pillow::HttpRequest*)));
	completedSpy = new QSignalSpy(request, SIGNAL(completed(Pillow::HttpRequest*)));
	closedSpy = new QSignalSpy(request, SIGNAL(closed(Pillow::HttpRequest*)));
}

void HttpRequestSslSocketTest::cleanup()
{
	if (request) delete request; request = NULL;
	if (server) delete server; server = NULL;
	if (client) delete client; client = NULL;
	if (readySpy) delete readySpy; readySpy = NULL;
	if (completedSpy) delete completedSpy; completedSpy = NULL;
	if (closedSpy) delete closedSpy; closedSpy = NULL;
}

void HttpRequestSslSocketTest::clientWrite(const QByteArray &data)
{
	client->write(data);
}

void HttpRequestSslSocketTest::clientFlush(bool _wait /* = true */)
{
	client->flush();
	if (_wait)
		wait(50);
}

QByteArray HttpRequestSslSocketTest::clientReadAll()
{
	return client->readAll();
}

bool HttpRequestSslSocketTest::isClientConnected()
{
	return client->state() == QAbstractSocket::ConnectedState && client->isOpen();
}

void HttpRequestSslSocketTest::clientClose()
{
	client->disconnectFromHost();
	client->close();
	while (client->state() == QAbstractSocket::ConnectedState) wait();
}

//
// HttpRequestLocalSocketTest
//

HttpRequestLocalSocketTest::HttpRequestLocalSocketTest()
	: server(NULL), client(NULL)
{
}

void HttpRequestLocalSocketTest::server_newConnection()
{
	request = new HttpRequest(server->nextPendingConnection());
}

void HttpRequestLocalSocketTest::init()
{
	server = new QLocalServer();
	server->removeServer("pillowtest");
	QVERIFY(server->listen("pillowtest"));
	connect(server, SIGNAL(newConnection()), this, SLOT(server_newConnection()));	
	client = new QLocalSocket();
	client->connectToServer("pillowtest");
	QVERIFY(client->waitForConnected(1000));
	while (request == NULL)
		QCoreApplication::processEvents();
	readySpy = new QSignalSpy(request, SIGNAL(ready(Pillow::HttpRequest*)));
	completedSpy = new QSignalSpy(request, SIGNAL(completed(Pillow::HttpRequest*)));
	closedSpy = new QSignalSpy(request, SIGNAL(closed(Pillow::HttpRequest*)));
}

void HttpRequestLocalSocketTest::cleanup()
{
	if (request) delete request; request = NULL;
	if (server) delete server; server = NULL;
	if (client) delete client; client = NULL;
	if (readySpy) delete readySpy; readySpy = NULL;
	if (completedSpy) delete completedSpy; completedSpy = NULL;
	if (closedSpy) delete closedSpy; closedSpy = NULL;
}

void HttpRequestLocalSocketTest::clientWrite(const QByteArray &data)
{
	client->write(data);
}

void HttpRequestLocalSocketTest::clientFlush(bool _wait /* = true */)
{
	client->flush();
	if (_wait)
		wait();
}

QByteArray HttpRequestLocalSocketTest::clientReadAll()
{
	return client->readAll();
}

void HttpRequestLocalSocketTest::clientClose()
{
	client->close();
}

bool HttpRequestLocalSocketTest::isClientConnected()
{
	wait(20); // On Windows, we must wait quite a bit for the client connection state to be updated.
	return client->state() == QLocalSocket::ConnectedState && client->isOpen();
}


//
// HttpRequestBufferTest
//

HttpRequestBufferTest::HttpRequestBufferTest()
    : inputBuffer(NULL), outputBuffer(NULL)
{
}

void HttpRequestBufferTest::init()
{
	inputBuffer = new QBuffer(); inputBuffer->open(QIODevice::ReadWrite);
	outputBuffer = new QBuffer(); outputBuffer->open(QIODevice::ReadWrite);
		
	request = new HttpRequest(inputBuffer, outputBuffer, NULL);
	
	readySpy = new QSignalSpy(request, SIGNAL(ready(Pillow::HttpRequest*)));
	completedSpy = new QSignalSpy(request, SIGNAL(completed(Pillow::HttpRequest*)));
	closedSpy = new QSignalSpy(request, SIGNAL(closed(Pillow::HttpRequest*)));
}

void HttpRequestBufferTest::cleanup()
{
	if (request) delete request; request = NULL;
	if (inputBuffer) delete inputBuffer; inputBuffer = NULL;
	if (outputBuffer) delete outputBuffer; outputBuffer = NULL;
	if (readySpy) delete readySpy; readySpy = NULL;
	if (completedSpy) delete completedSpy; completedSpy = NULL;
	if (closedSpy) delete closedSpy; closedSpy = NULL;
}

void HttpRequestBufferTest::clientWrite(const QByteArray &data)
{
	inputBuffer->write(data);
}

void HttpRequestBufferTest::clientFlush(bool _wait /* = true */)
{
	if (inputBuffer->isOpen()) inputBuffer->seek(0);
	QCoreApplication::processEvents();
	if (_wait) wait();
	if (inputBuffer->isOpen()) inputBuffer->seek(0);
	inputBuffer->buffer().clear();
}

QByteArray HttpRequestBufferTest::clientReadAll()
{
	QCoreApplication::processEvents();
	if (outputBuffer->isOpen()) outputBuffer->seek(0);
	QByteArray data = outputBuffer->buffer();
	if (outputBuffer->isOpen()) outputBuffer->seek(0);
	outputBuffer->buffer().clear();
	return data;
}

void HttpRequestBufferTest::clientClose()
{
	inputBuffer->close();
	outputBuffer->close();
}

bool HttpRequestBufferTest::isClientConnected()
{
	return inputBuffer->isOpen();
}

