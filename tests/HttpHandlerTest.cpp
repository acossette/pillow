#include "HttpHandlerTest.h"
#include "HttpHandler.h"
#include "HttpHandlerSimpleRouter.h"
#include "HttpRequest.h"
#include <QtCore/QBuffer>
#include <QtCore/QCoreApplication>
#include <QtTest/QtTest>
using namespace Pillow;

Pillow::HttpRequest * HttpHandlerTestBase::createGetRequest(const QByteArray &path)
{
	QByteArray data = QByteArray().append("GET ").append(path).append(" HTTP/1.0\r\n\r\n");
	QBuffer* inputBuffer = new QBuffer(); inputBuffer->open(QIODevice::ReadWrite);
	QBuffer* outputBuffer = new QBuffer(); outputBuffer->open(QIODevice::ReadWrite);
	connect(outputBuffer, SIGNAL(bytesWritten(qint64)), this, SLOT(outputBuffer_bytesWritten()));
	
	Pillow::HttpRequest* request = new Pillow::HttpRequest(inputBuffer, outputBuffer, this);
	connect(request, SIGNAL(completed(Pillow::HttpRequest*)), this, SLOT(requestCompleted(Pillow::HttpRequest*)));
	inputBuffer->setParent(request);
	outputBuffer->setParent(request);
	
	inputBuffer->write(data);
	inputBuffer->seek(0);
	
	while (request->state() != Pillow::HttpRequest::SendingHeaders)
		QCoreApplication::processEvents();
	
	return request;
}

Pillow::HttpRequest * HttpHandlerTestBase::createPostRequest(const QByteArray &path, const QByteArray &content)
{
	QByteArray data = QByteArray().append("POST ").append(path).append(" HTTP/1.0\r\n");
	if (content.size() > 0) data.append("Content-Length: ").append(QByteArray::number(content.size())).append("\r\n");
	data.append("\r\n").append(content);
	QBuffer* inputBuffer = new QBuffer(); inputBuffer->open(QIODevice::ReadWrite);
	QBuffer* outputBuffer = new QBuffer(); outputBuffer->open(QIODevice::ReadWrite);
	connect(outputBuffer, SIGNAL(bytesWritten(qint64)), this, SLOT(outputBuffer_bytesWritten()));
	
	Pillow::HttpRequest* request = new Pillow::HttpRequest(inputBuffer, outputBuffer, this);
	connect(request, SIGNAL(completed(Pillow::HttpRequest*)), this, SLOT(requestCompleted(Pillow::HttpRequest*)));
	inputBuffer->setParent(request);
	outputBuffer->setParent(request);
	
	inputBuffer->write(data);
	inputBuffer->seek(0);
	
	while (request->state() != Pillow::HttpRequest::SendingHeaders)
		QCoreApplication::processEvents();
	
	return request;
}

void HttpHandlerTestBase::requestCompleted(Pillow::HttpRequest* request)
{
	QCoreApplication::processEvents();
	response = responseBuffer;
	responseBuffer = QByteArray();
	requestParams = request->requestParams();
}

void HttpHandlerTestBase::outputBuffer_bytesWritten()
{
	QBuffer* buffer = static_cast<QBuffer*>(sender());
	responseBuffer.append(buffer->data());
	if (buffer->isOpen()) buffer->seek(0);
}

class MockHandler : public Pillow::HttpHandler
{
public:
	MockHandler(const QByteArray& acceptPath, int statusCode, QObject* parent)
		: Pillow::HttpHandler(parent), acceptPath(acceptPath), statusCode(statusCode), handleRequestCount(0)
	{}
	
	QByteArray acceptPath;
	int statusCode;
	int handleRequestCount;	
	
	bool handleRequest(Pillow::HttpRequest *request)
	{
		++handleRequestCount;
		
		if (acceptPath == request->requestPath())
		{
			request->writeResponse(statusCode);
			return true;
		}
		return false;
	}
};

void HttpHandlerTest::testHandlerStack()
{
	HttpHandlerStack handler;
	MockHandler* mock1 = new MockHandler("/1", 200, &handler);
	MockHandler* mock1_1 = new MockHandler("/", 403, mock1);
	new QObject(&handler); // Some dummy object, also child of handler.
	MockHandler* mock2 = new MockHandler("/2", 302, &handler);
	MockHandler* mock3 = new MockHandler("/", 500, &handler);
	MockHandler* mock4 = new MockHandler("/", 200, &handler);
	
	bool handled = handler.handleRequest(createGetRequest("/"));
	QVERIFY(handled);
	QVERIFY(response.startsWith("HTTP/1.0 500"));
	QCOMPARE(mock1->handleRequestCount, 1);
	QCOMPARE(mock1_1->handleRequestCount, 0);
	QCOMPARE(mock2->handleRequestCount, 1);
	QCOMPARE(mock3->handleRequestCount, 1);
	QCOMPARE(mock4->handleRequestCount, 0);
	
	handled = handler.handleRequest(createGetRequest("/2"));
	QVERIFY(handled);
	QVERIFY(response.startsWith("HTTP/1.0 302"));
	QCOMPARE(mock1->handleRequestCount, 2);
	QCOMPARE(mock1_1->handleRequestCount, 0);
	QCOMPARE(mock2->handleRequestCount, 2);
	QCOMPARE(mock3->handleRequestCount, 1);
	QCOMPARE(mock4->handleRequestCount, 0);
}

void HttpHandlerTest::testHandlerFixed()
{
	bool handled = HttpHandlerFixed(403, "Fixed test").handleRequest(createGetRequest());
	QVERIFY(handled);
	QVERIFY(response.startsWith("HTTP/1.0 403"));
	QVERIFY(response.endsWith("\r\n\r\nFixed test"));
}

void HttpHandlerTest::testHandler404()
{
	bool handled = HttpHandler404().handleRequest(createGetRequest("/some_path"));
	QVERIFY(handled);
	QVERIFY(response.startsWith("HTTP/1.0 404"));
	QVERIFY(response.contains("/some_path"));	
}

void HttpHandlerTest::testHandlerLog()
{
	QBuffer buffer; buffer.open(QIODevice::ReadWrite);
	Pillow::HttpRequest* request1 = createGetRequest("/first");
	Pillow::HttpRequest* request2 = createGetRequest("/second");
	Pillow::HttpRequest* request3 = createGetRequest("/third");
	
	HttpHandlerLog handler(&buffer, &buffer);
	QVERIFY(!handler.handleRequest(request1));
	QVERIFY(!handler.handleRequest(request2));
	QVERIFY(!handler.handleRequest(request3));
	QVERIFY(buffer.data().isEmpty());
	request3->writeResponse(302);
	request1->writeResponse(200);
	request2->writeResponse(500);
	
	// The log handler should write the log entries as they are completed.
	buffer.seek(0);
	QVERIFY(buffer.readLine().contains("GET /third"));
	QVERIFY(buffer.readLine().contains("GET /first"));
	QVERIFY(buffer.readLine().contains("GET /second"));
	QVERIFY(buffer.readLine().isEmpty());
}

void HttpHandlerFileTest::initTestCase()
{
	testPath = QDir::tempPath() + "/HttpHandlerFileTest";
	QDir(testPath).mkpath(".");
	QVERIFY(QFile::exists(testPath));
	
	QByteArray bigData(16 * 1024 * 1024, '-');
	
	{ QFile f(testPath + "/first"); f.open(QIODevice::WriteOnly); f.write("first content"); f.flush(); f.close(); }
	{ QFile f(testPath + "/second"); f.open(QIODevice::WriteOnly); f.write("second content"); f.flush(); f.close(); }
	{ QFile f(testPath + "/large"); f.open(QIODevice::WriteOnly); f.write(bigData); f.flush(); f.close(); }
	{ QFile f(testPath + "/first"); f.open(QIODevice::ReadOnly); QCOMPARE(f.readAll(), QByteArray("first content")); }
	{ QFile f(testPath + "/second"); f.open(QIODevice::ReadOnly); QCOMPARE(f.readAll(), QByteArray("second content")); }
	{ QFile f(testPath + "/large"); f.open(QIODevice::ReadOnly); QCOMPARE(f.readAll(), bigData); }
}

void HttpHandlerFileTest::testServesFiles()
{
	HttpHandlerFile handler(testPath);
	QVERIFY(!handler.handleRequest(createGetRequest("/")));
	QVERIFY(!handler.handleRequest(createGetRequest("/bad_path")));
	QVERIFY(!handler.handleRequest(createGetRequest("/another_bad")));
	
	Pillow::HttpRequest* request = createGetRequest("/first");
	QVERIFY(handler.handleRequest(request));
	QVERIFY(response.startsWith("HTTP/1.0 200 OK"));
	QVERIFY(response.endsWith("first content"));

	response.clear();
	
	// Note: the large files test currently fails when the output device is a QBuffer.
	request = createGetRequest("/large");
	QVERIFY(handler.handleRequest(request));
	while (response.isEmpty())
		QCoreApplication::processEvents();	
	QVERIFY(response.size() > 16 * 1024 * 1024);
	QVERIFY(response.startsWith("HTTP/1.0 200 OK"));
	QVERIFY(response.endsWith(QByteArray(16 * 1024 * 1024, '-')));
}

void HttpHandlerSimpleRouterTest::testHandlerRoute()
{
	HttpHandlerSimpleRouter handler;
	handler.addRoute("/some_path", new HttpHandlerFixed(303, "Hello"));
	handler.addRoute("/other/path", new HttpHandlerFixed(404, "World"));
	handler.addRoute("/some_path/even/deeper", new HttpHandlerFixed(200, "!"));
	
	QVERIFY(!handler.handleRequest(createGetRequest("/should_not_match")));	
	QVERIFY(!handler.handleRequest(createGetRequest("/should/not/match/either")));	
	QVERIFY(!handler.handleRequest(createGetRequest("/some_path/should_not_match")));
	
	QVERIFY(handler.handleRequest(createGetRequest("/other/path")));
	QVERIFY(response.startsWith("HTTP/1.0 404"));
	QVERIFY(response.endsWith("World"));
	response.clear();

	QVERIFY(handler.handleRequest(createGetRequest("/some_path/even/deeper?with=query_string")));
	QVERIFY(response.startsWith("HTTP/1.0 200"));
	QVERIFY(response.endsWith("!"));
	response.clear();
}

void HttpHandlerSimpleRouterTest::testQObjectMetaCallRoute()
{
	HttpHandlerSimpleRouter handler;
	handler.addRoute("/first", this, "handleRequest1");
	handler.addRoute("/first/second", this, "handleRequest2");
	
	QVERIFY(!handler.handleRequest(createGetRequest("/should_not_match")));	
	QVERIFY(!handler.handleRequest(createGetRequest("/should/not/match/either")));	
	QVERIFY(!handler.handleRequest(createGetRequest("/first/should_not_match")));
	QVERIFY(!handler.handleRequest(createGetRequest("/first/second/should_not_match")));
	
	QVERIFY(handler.handleRequest(createGetRequest("/first")));
	QVERIFY(response.startsWith("HTTP/1.0 403"));
	QVERIFY(response.endsWith("Hello"));
	response.clear();

	QVERIFY(handler.handleRequest(createGetRequest("/first/second?with=query_string")));
	QVERIFY(response.startsWith("HTTP/1.0 200"));
	QVERIFY(response.endsWith("World"));
	response.clear();
}

void HttpHandlerSimpleRouterTest::testQObjectSlotCallRoute()
{
	HttpHandlerSimpleRouter handler;
	handler.addRoute("/route", this, SLOT(handleRequest2(Pillow::HttpRequest*)));

	QVERIFY(handler.handleRequest(createGetRequest("/route")));	
	QVERIFY(response.startsWith("HTTP/1.0 200"));
	QVERIFY(response.endsWith("World"));
	response.clear();
}

void HttpHandlerSimpleRouterTest::testStaticRoute()
{
	HttpHandlerSimpleRouter handler;
	handler.addRoute("/first", 200, Pillow::HttpHeaderCollection(), "First Route");
	handler.addRoute("/first/second", 404, Pillow::HttpHeaderCollection(), "Second Route");
	handler.addRoute("/third", 500, Pillow::HttpHeaderCollection(), "Third Route");
	
	QVERIFY(!handler.handleRequest(createGetRequest("/should_not_match")));	
	QVERIFY(!handler.handleRequest(createGetRequest("/should/not/match/either")));	
	QVERIFY(!handler.handleRequest(createGetRequest("/first/should_not_match")));
	QVERIFY(!handler.handleRequest(createGetRequest("/first/second/should_not_match")));
	
	QVERIFY(handler.handleRequest(createGetRequest("/first")));
	QVERIFY(response.startsWith("HTTP/1.0 200"));
	QVERIFY(response.endsWith("First Route"));
	response.clear();

	QVERIFY(handler.handleRequest(createGetRequest("/third?with=query_string#and_fragment")));
	QVERIFY(response.startsWith("HTTP/1.0 500"));
	QVERIFY(response.endsWith("Third Route"));
	response.clear();
}

void HttpHandlerSimpleRouterTest::handleRequest1(Pillow::HttpRequest *request)
{
	request->writeResponse(403, Pillow::HttpHeaderCollection(), "Hello");
}

void HttpHandlerSimpleRouterTest::handleRequest2(Pillow::HttpRequest *request)
{
	request->writeResponse(200, Pillow::HttpHeaderCollection(), "World");
}

void HttpHandlerSimpleRouterTest::testPathParams()
{
	HttpHandlerSimpleRouter handler;
	handler.addRoute("/first/:with_param", 200, Pillow::HttpHeaderCollection(), "First Route"); 
	handler.addRoute("/second/:with_param/and/:another", 200, Pillow::HttpHeaderCollection(), "Second Route");
	handler.addRoute("/third/:with/:many/:params", 200, Pillow::HttpHeaderCollection(), "Third Route");
	
	QVERIFY(handler.handleRequest(createGetRequest("/first/some_param-value")));
	QVERIFY(response.startsWith("HTTP/1.0 200"));
	QVERIFY(response.endsWith("First Route"));
	QCOMPARE(requestParams.size(), 1);
	QCOMPARE(requestParams.at(0).first, QString("with_param"));
	QCOMPARE(requestParams.at(0).second, QString("some_param-value"));
	response.clear();	

	QVERIFY(handler.handleRequest(createGetRequest("/second/some_param-value/and/another_value")));
	QVERIFY(response.startsWith("HTTP/1.0 200"));
	QVERIFY(response.endsWith("Second Route"));
	QCOMPARE(requestParams.size(), 2);
	QCOMPARE(requestParams.at(0).first, QString("with_param"));
	QCOMPARE(requestParams.at(0).second, QString("some_param-value"));
	QCOMPARE(requestParams.at(1).first, QString("another"));
	QCOMPARE(requestParams.at(1).second, QString("another_value"));
	response.clear();	

	QVERIFY(handler.handleRequest(createGetRequest("/third/some_param-value/another_value/and_a_last_one?with=overriden&extra=bonus_query_param#and_fragment")));
	QVERIFY(response.startsWith("HTTP/1.0 200"));
	QVERIFY(response.endsWith("Third Route"));
	QCOMPARE(requestParams.size(), 4);
	QCOMPARE(requestParams.at(0).first, QString("with"));
	QCOMPARE(requestParams.at(0).second, QString("some_param-value")); // The route param should have overriden the query string param.
	QCOMPARE(requestParams.at(1).first, QString("extra"));
	QCOMPARE(requestParams.at(1).second, QString("bonus_query_param"));
	QCOMPARE(requestParams.at(2).first, QString("many"));
	QCOMPARE(requestParams.at(2).second, QString("another_value"));
	QCOMPARE(requestParams.at(3).first, QString("params"));
	QCOMPARE(requestParams.at(3).second, QString("and_a_last_one"));
	response.clear();
	
	QVERIFY(!handler.handleRequest(createGetRequest("/first/some_param-value/and_extra_stuff")));
	QVERIFY(!handler.handleRequest(createGetRequest("/second/some_param-value/bad_part/another_value")));
	QVERIFY(!handler.handleRequest(createGetRequest("/third/some_param-value/another_value/and_a_last_one/and_extra_stuff")));
}

void HttpHandlerSimpleRouterTest::testPathSplats()
{
	HttpHandlerSimpleRouter handler;
	handler.addRoute("/first/*with_splat", 200, Pillow::HttpHeaderCollection(), "First Route");
	handler.addRoute("/second/:with_param/and/*splat", 200, Pillow::HttpHeaderCollection(), "Second Route");
	handler.addRoute("/third/*with/two/*splats", 200, Pillow::HttpHeaderCollection(), "Third Route");
	
	QVERIFY(handler.handleRequest(createGetRequest("/first/")));
	QVERIFY(response.startsWith("HTTP/1.0 200"));
	QVERIFY(response.endsWith("First Route"));
	QCOMPARE(requestParams.size(), 1);
	QCOMPARE(requestParams.at(0).first, QString("with_splat"));
	QCOMPARE(requestParams.at(0).second, QString(""));	
	response.clear();		

	QVERIFY(handler.handleRequest(createGetRequest("/first/with/anything-after.that/really_I_tell_you.html")));
	QVERIFY(response.startsWith("HTTP/1.0 200"));
	QVERIFY(response.endsWith("First Route"));
	QCOMPARE(requestParams.size(), 1);
	QCOMPARE(requestParams.at(0).first, QString("with_splat"));
	QCOMPARE(requestParams.at(0).second, QString("with/anything-after.that/really_I_tell_you.html"));	
	response.clear();		

	QVERIFY(handler.handleRequest(createGetRequest("/second/some-param-value/and/")));
	QVERIFY(response.startsWith("HTTP/1.0 200"));
	QVERIFY(response.endsWith("Second Route"));
	QCOMPARE(requestParams.size(), 2);
	QCOMPARE(requestParams.at(0).first, QString("with_param"));
	QCOMPARE(requestParams.at(0).second, QString("some-param-value"));	
	QCOMPARE(requestParams.at(1).first, QString("splat"));
	QCOMPARE(requestParams.at(1).second, QString(""));
	response.clear();			

	QVERIFY(handler.handleRequest(createGetRequest("/second/some-param-value/and/extra/stuff/splatted.at/the.end?with=bonus_query_param#and_fragment")));
	QVERIFY(response.startsWith("HTTP/1.0 200"));
	QVERIFY(response.endsWith("Second Route"));
	QCOMPARE(requestParams.size(), 3);
	QCOMPARE(requestParams.at(0).first, QString("with"));
	QCOMPARE(requestParams.at(0).second, QString("bonus_query_param"));	
	QCOMPARE(requestParams.at(1).first, QString("with_param"));
	QCOMPARE(requestParams.at(1).second, QString("some-param-value"));	
	QCOMPARE(requestParams.at(2).first, QString("splat"));
	QCOMPARE(requestParams.at(2).second, QString("extra/stuff/splatted.at/the.end"));
	response.clear();
	
	QVERIFY(handler.handleRequest(createGetRequest("/third/some/path/two/and/another/path%20with%20spaces.txt")));
	QVERIFY(response.startsWith("HTTP/1.0 200"));
	QVERIFY(response.endsWith("Third Route"));
	QCOMPARE(requestParams.size(), 2);
	QCOMPARE(requestParams.at(0).first, QString("with"));
	QCOMPARE(requestParams.at(0).second, QString("some/path"));	
	QCOMPARE(requestParams.at(1).first, QString("splats"));
	QCOMPARE(requestParams.at(1).second, QString("and/another/path with spaces.txt"));
	response.clear();				

	QVERIFY(!handler.handleRequest(createGetRequest("/first")));
	QVERIFY(!handler.handleRequest(createGetRequest("/second/some_param-value/")));
	QVERIFY(!handler.handleRequest(createGetRequest("/second/some_param-value/and")));
	QVERIFY(!handler.handleRequest(createGetRequest("/second/some_param-value/bad_part/splat/splat/splat")));	
}

void HttpHandlerSimpleRouterTest::testMatchesMethod()
{
	HttpHandlerSimpleRouter handler;
	handler.addRoute("GET", "/get", 200, Pillow::HttpHeaderCollection(), "First Route"); 
	handler.addRoute("POST", "/post", 200, Pillow::HttpHeaderCollection(), "Second Route");
	handler.addRoute("GET", "/both", 200, Pillow::HttpHeaderCollection(), "Third Route (GET)"); 
	handler.addRoute("POST", "/both", 200, Pillow::HttpHeaderCollection(), "Third Route (POST)");
	
	QVERIFY(handler.handleRequest(createGetRequest("/get")));
	QVERIFY(!handler.handleRequest(createPostRequest("/get")));
	QVERIFY(!handler.handleRequest(createGetRequest("/post")));
	QVERIFY(handler.handleRequest(createPostRequest("/post")));
	
	QVERIFY(handler.handleRequest(createGetRequest("/both")));
	QVERIFY(response.startsWith("HTTP/1.0 200"));
	QVERIFY(response.endsWith("Third Route (GET)"));
	response.clear();					

	QVERIFY(handler.handleRequest(createPostRequest("/both")));
	QVERIFY(response.startsWith("HTTP/1.0 200"));
	QVERIFY(response.endsWith("Third Route (POST)"));
	response.clear();
}

void HttpHandlerSimpleRouterTest::testUnmatchedRequestAction()
{
	HttpHandlerSimpleRouter handler;
	QVERIFY(handler.unmatchedRequestAction() == HttpHandlerSimpleRouter::Passthrough);
	handler.setUnmatchedRequestAction(HttpHandlerSimpleRouter::Return4xxResponse);
	handler.addRoute("GET", "/a", 200, Pillow::HttpHeaderCollection(), "First Route (GET)");
	handler.addRoute("DELETE", "/a", 200, Pillow::HttpHeaderCollection(), "First Route (DELETE)");

	QVERIFY(handler.handleRequest(createGetRequest("/unmatched/route")));
	QVERIFY(response.startsWith("HTTP/1.0 404"));
	response.clear();	
}

void HttpHandlerSimpleRouterTest::testMethodMismatchAction()
{
	HttpHandlerSimpleRouter handler;
	QVERIFY(handler.methodMismatchAction() == HttpHandlerSimpleRouter::Passthrough);
	handler.setMethodMismatchAction(HttpHandlerSimpleRouter::Return4xxResponse);
	handler.addRoute("GET", "/a", 200, Pillow::HttpHeaderCollection(), "First Route (GET)");
	handler.addRoute("DELETE", "/a", 200, Pillow::HttpHeaderCollection(), "First Route (DELETE)");

	QVERIFY(handler.handleRequest(createPostRequest("/a")));
	QVERIFY(response.startsWith("HTTP/1.0 405"));
	QVERIFY(response.contains("Allow: GET, DELETE"));
	response.clear();	
}

void HttpHandlerSimpleRouterTest::testSupportsMethodParam()
{
	HttpHandlerSimpleRouter handler;
	handler.addRoute("POST", "/a", 200, Pillow::HttpHeaderCollection(), "Route");
	handler.addRoute("DELETE", "/b", 200, Pillow::HttpHeaderCollection(), "Route");
	
	QVERIFY(handler.acceptsMethodParam() == false);	
	QVERIFY(!handler.handleRequest(createGetRequest("/a")));
	QVERIFY(handler.handleRequest(createPostRequest("/a")));
	QVERIFY(!handler.handleRequest(createGetRequest("/a?_method=post")));
	QVERIFY(!handler.handleRequest(createGetRequest("/b?_method=delete")));
	QVERIFY(!handler.handleRequest(createPostRequest("/b?_method=delete")));
	
	handler.setAcceptsMethodParam(true);
	QVERIFY(!handler.handleRequest(createGetRequest("/a")));
	QVERIFY(handler.handleRequest(createPostRequest("/a")));
	QVERIFY(handler.handleRequest(createGetRequest("/a?_method=POST")));
	QVERIFY(handler.handleRequest(createGetRequest("/b?_method=DELETE")));
	QVERIFY(handler.handleRequest(createPostRequest("/b?_method=DELETE")));
	QVERIFY(handler.handleRequest(createGetRequest("/b?_method=delete")));
	QVERIFY(handler.handleRequest(createPostRequest("/b?_method=delete")));
}

