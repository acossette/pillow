#include "HttpHandlerProxyTest.h"
#include <HttpConnection.h>
#include <HttpHandlerProxy.h>
#include <HttpServer.h>
#include <HttpHandlerSimpleRouter.h>
#include <QtTest/QtTest>

class ClosingHandler : public Pillow::HttpHandler
{
	virtual bool handleRequest(Pillow::HttpConnection *connection)
	{
		connection->writeResponse(200);
		connection->close();
		return true;
	}
};

class PrematureClosingHandler : public Pillow::HttpHandler
{
	virtual bool handleRequest(Pillow::HttpConnection *connection)
	{
		connection->close();
		return true;
	}
};


class InvalidHandler : public Pillow::HttpHandler
{
	virtual bool handleRequest(Pillow::HttpConnection *connection)
	{
		connection->outputDevice()->write("FDSPIFUDSAFIUDUSAF DSFIASDUF DIFUSADIFU ASDFDSIF DUSAFDSA FDSOFIDSOFIDSOIFDSOIFDSOIFODSIFDISOI fDSIFDSIFIDFIIFIFIFIFI");
		return true;
	}	
};

class ContentLengthMismatchedHandler : public Pillow::HttpHandler
{
	virtual bool handleRequest(Pillow::HttpConnection *connection)
	{
		connection->writeHeaders(200, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Content-Length", "10"));
		connection->outputDevice()->write("12345678901234567890123456789012345678901234567890123456789012345678901234567890");
		return true;
	}	
};

class CapturingHandler : public Pillow::HttpHandler
{
public:
	QByteArray requestMethod;
	QByteArray requestUri;
	QByteArray requestFragment;
	Pillow::HttpHeaderCollection requestHeaders;
	QByteArray requestContent;
	
	virtual bool handleRequest(Pillow::HttpConnection *connection)
	{
		(requestMethod = connection->requestMethod()).detach();
		(requestUri = connection->requestUri()).detach();
		(requestFragment = connection->requestFragment()).detach();
		(requestContent = connection->requestContent()).detach();
		(requestHeaders = connection->requestHeaders()).detach();
		for (int i = 0, iE = requestHeaders.size(); i < iE; ++i)
		{
			requestHeaders[i].first.detach();
			requestHeaders[i].second.detach();
		}
		connection->writeResponse(200, Pillow::HttpHeaderCollection(), requestMethod + " captured!");
		return true;
	}	
};

class HoldingHandler : public Pillow::HttpHandler
{
public:
	QList<Pillow::HttpConnection*> connections;
	
	virtual bool handleRequest(Pillow::HttpConnection *connection)
	{
		connections.append(connection);
		return true;
	}
};

HttpHandlerProxyTest::HttpHandlerProxyTest() :
    HttpHandlerTestBase(), router(NULL)
{
	qRegisterMetaType<Pillow::HttpConnection*>("Pillow::HttpConnection*");
}

void HttpHandlerProxyTest::init()
{
	server = new Pillow::HttpServer(this);
	server->listen();

	router = new Pillow::HttpHandlerSimpleRouter();
	router->addRoute("GET", "/first", 200, Pillow::HttpHeaderCollection(), "first content");
	router->addRoute("GET", "/second", 200, Pillow::HttpHeaderCollection(), "second content");
	router->addRoute("GET", "/closing", new ClosingHandler());
	router->addRoute("GET", "/premature_closing", new PrematureClosingHandler());
	router->addRoute("GET", "/invalid", new InvalidHandler());
	router->addRoute("GET", "/explosive", 500, Pillow::HttpHeaderCollection(), "explosive content");
	router->addRoute("GET", "/third", 200, Pillow::HttpHeaderCollection(), "second content");
	router->addRoute("GET", "/bad_length", new ContentLengthMismatchedHandler());
	router->addRoute("", "/capturing", capturingHandler = new CapturingHandler());
	router->addRoute("GET", "/holding", holdingHandler = new HoldingHandler());
	
	connect(server, SIGNAL(requestReady(Pillow::HttpConnection*)), router, SLOT(handleRequest(Pillow::HttpConnection*)));
}

void HttpHandlerProxyTest::cleanup()
{
	delete router;
	delete server;
}

QUrl HttpHandlerProxyTest::serverUrl() const
{
	return QUrl(QString("http://127.0.0.1:%1").arg(server->serverPort()));
}

bool waitForResponse(Pillow::HttpConnection* connection)
{
	QSignalSpy completedSpy(connection, SIGNAL(requestCompleted(Pillow::HttpConnection*)));
	QSignalSpy closedSpy(connection, SIGNAL(closed(Pillow::HttpConnection*)));
	
	while (completedSpy.isEmpty() && closedSpy.isEmpty())
		QCoreApplication::processEvents();
	
	return completedSpy.size() > 0;
}

void HttpHandlerProxyTest::testSuccessfulResponse()
{
	Pillow::HttpHandlerProxy handler(serverUrl());
	Pillow::HttpConnection* request = createGetRequest("/capturing?key1=value1&key2=value2%20with%20escaped#and_fragment", "1.1");

	QVERIFY(handler.handleRequest(request));
	QVERIFY(waitForResponse(request)); // The response should complete successfully.
	QVERIFY(response.startsWith("HTTP/1.1 200"));
	QVERIFY(response.endsWith("\r\n\r\nGET captured!"));
	QVERIFY(capturingHandler->requestMethod == "GET");
	QVERIFY(capturingHandler->requestUri == "/capturing?key1=value1&key2=value2%20with%20escaped");
	//QVERIFY(capturingHandler->requestFragment == "and_fragment"); // QNetworkAccessManager seems not to pass fragments in the requests it sends.
	QVERIFY(capturingHandler->requestContent.isEmpty());
}

void HttpHandlerProxyTest::testClosingResponse()
{
	Pillow::HttpHandlerProxy handler(serverUrl());
	Pillow::HttpConnection* request = createGetRequest("/closing");
	
	QVERIFY(handler.handleRequest(request));
	QVERIFY(waitForResponse(request)); // The response should complete sucessfully
	QVERIFY(response.startsWith("HTTP/1.0 200"));
}

void HttpHandlerProxyTest::testPrematureClosingResponse()
{
	Pillow::HttpHandlerProxy handler(serverUrl());
	Pillow::HttpConnection* request = createGetRequest("/premature_closing");
	
	QVERIFY(handler.handleRequest(request));
	QVERIFY(waitForResponse(request)); // The response should still complete sucessfully
	QVERIFY(response.startsWith("HTTP/1.0 503")); // Service unavailable.
}

void HttpHandlerProxyTest::testInvalidResponse()
{
	Pillow::HttpHandlerProxy handler(serverUrl());
	Pillow::HttpConnection* request = createGetRequest("/invalid");
	
	QVERIFY(handler.handleRequest(request));
	QVERIFY(waitForResponse(request)); // The response should still complete sucessfully
	QVERIFY(response.startsWith("HTTP/1.0 503")); // Service unavailable.
}

void HttpHandlerProxyTest::testContentLengthMismatchedResponse()
{
	Pillow::HttpHandlerProxy handler(serverUrl());
	Pillow::HttpConnection* request = createGetRequest("/bad_length");
	
	// If the proxied server sends too much data, we expect the proxy handler to act as 
	// a good HTTP client and disregard the extra data, then close the proxied connection (which is hard to test).
	QVERIFY(handler.handleRequest(request));
	QVERIFY(waitForResponse(request)); 
	QVERIFY(response.startsWith("HTTP/1.0 200"));
	QVERIFY(response.endsWith("\r\n\r\n1234567890"));
}

QUrl createProxyServer(const QUrl& proxiedUrl)
{
	Pillow::HttpServer* server = new Pillow::HttpServer(QHostAddress::LocalHost, 0);
	Pillow::HttpHandlerProxy* proxyHandler = new Pillow::HttpHandlerProxy(proxiedUrl);
	QObject::connect(server, SIGNAL(requestReady(Pillow::HttpConnection*)), proxyHandler, SLOT(handleRequest(Pillow::HttpConnection*)));
	return QString("http://127.0.0.1:%1").arg(server->serverPort());
}

void HttpHandlerProxyTest::testProxyChain()
{
	// Setup a cool chain of 4 proxy.
	// Could also create a proxy loop to make things nicely explode.
	// Request => (Proxy handler) => Server 4 (Proxy 4) => Server 3 (Proxy 3) => Server 2 (Proxy 2) => Server (Handlers)
	
	QUrl outerProxyUrl = 
			createProxyServer(
				createProxyServer(
					createProxyServer(
						createProxyServer(serverUrl()))));
	
	Pillow::HttpHandlerProxy handler(outerProxyUrl);
	Pillow::HttpConnection* request;

	request = createGetRequest("/first", "1.0");
	QVERIFY(handler.handleRequest(request));
	QVERIFY(waitForResponse(request));
	QVERIFY(response.startsWith("HTTP/1.0 200"));
	QVERIFY(response.endsWith("\r\n\r\nfirst content"));
	QVERIFY(request->state() == Pillow::HttpConnection::Closed);
	
	request = createGetRequest("/first", "1.1");
	QVERIFY(handler.handleRequest(request));
	QVERIFY(waitForResponse(request));
	QVERIFY(response.startsWith("HTTP/1.1 200"));
	QVERIFY(response.endsWith("\r\n\r\nfirst content"));
	QVERIFY(request->state() == Pillow::HttpConnection::ReceivingHeaders);

	request = createGetRequest("/explosive", "1.1");
	QVERIFY(handler.handleRequest(request));
	QVERIFY(waitForResponse(request));
	QVERIFY(response.startsWith("HTTP/1.1 500"));		
	QVERIFY(request->state() == Pillow::HttpConnection::ReceivingHeaders);
	
	request = createGetRequest("/second", "1.1");
	QVERIFY(handler.handleRequest(request));
	QVERIFY(waitForResponse(request));
	QVERIFY(response.startsWith("HTTP/1.1 200"));
	QVERIFY(response.endsWith("\r\n\r\nsecond content"));
	QVERIFY(request->state() == Pillow::HttpConnection::ReceivingHeaders);

	request = createGetRequest("/premature_closing", "1.1");
	QVERIFY(handler.handleRequest(request));
	QVERIFY(waitForResponse(request));
	QVERIFY(response.startsWith("HTTP/1.1 503"));	
	QVERIFY(request->state() == Pillow::HttpConnection::ReceivingHeaders);
	
	request = createGetRequest("/closing", "1.1");
	QVERIFY(handler.handleRequest(request));
	QVERIFY(waitForResponse(request));
	QVERIFY(response.startsWith("HTTP/1.1 200"));
	QVERIFY(request->state() == Pillow::HttpConnection::ReceivingHeaders);

	request = createGetRequest("/invalid", "1.1");
	QVERIFY(handler.handleRequest(request));
	QVERIFY(waitForResponse(request));
	QVERIFY(response.startsWith("HTTP/1.1 503"));	
	QVERIFY(request->state() == Pillow::HttpConnection::ReceivingHeaders);

	request = createGetRequest("/bad_length", "1.1");
	QVERIFY(handler.handleRequest(request));
	QVERIFY(waitForResponse(request));
	QVERIFY(response.startsWith("HTTP/1.1 200"));		
	QVERIFY(response.endsWith("\r\n\r\n1234567890"));
	QVERIFY(request->state() == Pillow::HttpConnection::ReceivingHeaders);
	
	request = createGetRequest("/capturing?key1=value1&key2=value2%20with%20escaped#and_fragment", "1.1");
	QVERIFY(handler.handleRequest(request));
	QVERIFY(waitForResponse(request));
	QVERIFY(response.startsWith("HTTP/1.1 200"));
	QVERIFY(capturingHandler->requestMethod == "GET");
	QVERIFY(capturingHandler->requestUri == "/capturing?key1=value1&key2=value2%20with%20escaped");
	QVERIFY(capturingHandler->requestContent.isEmpty());	
}

void HttpHandlerProxyTest::testNonGetRequest()
{
	Pillow::HttpHandlerProxy handler(serverUrl());
	Pillow::HttpConnection* request; 
	
	request = createPostRequest("/capturing?key1=value1", "some data");
	QVERIFY(handler.handleRequest(request));
	QVERIFY(waitForResponse(request)); // The response should complete successfully.
	QVERIFY(response.startsWith("HTTP/1.0 200"));
	QVERIFY(response.endsWith("\r\n\r\nPOST captured!"));
	QVERIFY(capturingHandler->requestMethod == "POST");
	QVERIFY(capturingHandler->requestUri == "/capturing?key1=value1");
	QVERIFY(capturingHandler->requestContent == "some data");	

	request = createRequest("OPTIONS", "/capturing?key1=value1", QByteArray(), "1.1");
	QVERIFY(handler.handleRequest(request));
	QVERIFY(waitForResponse(request)); // The response should complete successfully.
	QVERIFY(response.startsWith("HTTP/1.1 200"));
	QVERIFY(response.endsWith("\r\n\r\nOPTIONS captured!"));
	QVERIFY(capturingHandler->requestMethod == "OPTIONS");
	QVERIFY(capturingHandler->requestUri == "/capturing?key1=value1");
	QVERIFY(capturingHandler->requestContent.isEmpty());	

	request = createRequest("TEAPOT", "/capturing?key1=value1", QByteArray(), "1.1");
	QVERIFY(handler.handleRequest(request));
	QVERIFY(waitForResponse(request)); // The response should complete successfully.
	QVERIFY(response.startsWith("HTTP/1.1 200"));
	QVERIFY(response.endsWith("\r\n\r\nTEAPOT captured!"));
	QVERIFY(capturingHandler->requestMethod == "TEAPOT");
	QVERIFY(capturingHandler->requestUri == "/capturing?key1=value1");
	QVERIFY(capturingHandler->requestContent.isEmpty());		
}

void HttpHandlerProxyTest::testHandlesMultipleConcurrentRequests()
{
	Pillow::HttpHandlerProxy handler(serverUrl());

	for (int i = 0; i < 50; ++i)
		handler.handleRequest(createGetRequest("/holding"));
	
	QElapsedTimer t; t.start();
	while (t.elapsed() < 200) // Fragile, I know...
		QCoreApplication::processEvents();
	
	QCOMPARE(holdingHandler->connections.size(), 50);
}

class CustomProxyPipe: public Pillow::HttpHandlerProxyPipe
{
public:
	CustomProxyPipe(Pillow::HttpConnection* request, QNetworkReply* proxiedReply)
		: Pillow::HttpHandlerProxyPipe(request, proxiedReply)
	{}
	
	virtual void pump(const QByteArray &data)
	{
		_request->writeContent(QByteArray(data.size(), '*'));
	}
};

class CustomProxy: public Pillow::HttpHandlerProxy
{
public:
	CustomProxy(const QUrl& url) : Pillow::HttpHandlerProxy(url)
	{}
	
	virtual QNetworkReply* createProxiedReply(Pillow::HttpConnection *request, QNetworkRequest proxiedRequest)
	{
		return Pillow::HttpHandlerProxy::createProxiedReply(request, proxiedRequest);
	}
	
	virtual Pillow::HttpHandlerProxyPipe* createPipe(Pillow::HttpConnection *request, QNetworkReply *proxiedReply)
	{
		return new CustomProxyPipe(request, proxiedReply);
	}
};

void HttpHandlerProxyTest::testCustomProxyPipe()
{
	CustomProxy handler(serverUrl());
	Pillow::HttpConnection* request = createGetRequest("/capturing", "1.1");

	QVERIFY(handler.handleRequest(request));
	QVERIFY(waitForResponse(request)); // The response should complete successfully.
	QVERIFY(response.startsWith("HTTP/1.1 200"));
	QVERIFY(response.endsWith("\r\n\r\n*************"));
	QVERIFY(capturingHandler->requestMethod == "GET");
	QVERIFY(capturingHandler->requestUri == "/capturing");
	QVERIFY(capturingHandler->requestContent.isEmpty());
}
