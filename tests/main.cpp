#include <QtCore/QCoreApplication>
#include "HttpConnectionTest.h"
#include "HttpServerTest.h"
#include "HttpsServerTest.h"
#include "HttpHandlerTest.h"
#include "HttpHandlerProxyTest.h"
#include "Helpers.h"

template<class T> int execTest()
{
	T t; return QTest::qExec(&t);
}

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	int result = 0;
	result += execTest<HttpConnectionTcpSocketTest>();
	result += execTest<HttpConnectionSslSocketTest>();
	result += execTest<HttpConnectionLocalSocketTest>();
	result += execTest<HttpConnectionBufferTest>();
	result += execTest<HttpServerTest>();
	result += execTest<HttpsServerTest>();
	result += execTest<HttpLocalServerTest>();
	result += execTest<HttpHandlerTest>();
	result += execTest<HttpHandlerFileTest>();
	result += execTest<HttpHandlerSimpleRouterTest>();
	result += execTest<HttpHandlerProxyTest>();

	PILLOW_TEST_RUN(ByteArrayHelpersTest, result);
	PILLOW_TEST_RUN(HttpRequestWriterTest, result);
	PILLOW_TEST_RUN(HttpResponseParserTest, result);
	PILLOW_TEST_RUN(HttpClientTest, result);
	PILLOW_TEST_RUN(NetworkAccessManagerTest, result);
	PILLOW_TEST_RUN(HttpHeaderTest, result);
	PILLOW_TEST_RUN(HttpHeaderCollectionTest, result);

	return result;
}
