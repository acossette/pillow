#include <QtCore/QtCore>
#include <QtTest/QTest>
#include "HttpRequestTest.h"
#include "HttpServerTest.h"
#include "HttpHandlerTest.h"

template<class T> int execTest()
{
	T t; return QTest::qExec(&t);
}

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	int result = 0;
//	result += execTest<HttpRequestTcpSocketTest>();
//	result += execTest<HttpRequestSslSocketTest>();
//	result += execTest<HttpRequestLocalSocketTest>();
//	result += execTest<HttpRequestBufferTest>();
//	result += execTest<HttpServerTest>();
//	result += execTest<HttpsServerTest>();
//	result += execTest<HttpLocalServerTest>();
	result += execTest<HttpHandlerTest>();
	return result;
}
