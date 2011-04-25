#include <QtCore/QtCore>
#include <QtTest/QTest>
#include "HttpConnectionTest.h"
#include "HttpServerTest.h"
#include "HttpsServerTest.h"
#include "HttpHandlerTest.h"
#include "HttpHandlerProxyTest.h"

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
	return result;
}
