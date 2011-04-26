#ifndef HTTPHANDLERTEST_H
#define HTTPHANDLERTEST_H

#include <QtCore/QObject>
#include <HttpConnection.h>

namespace Pillow { class HttpConnection; }

class HttpHandlerTestBase : public QObject
{
    Q_OBJECT

protected slots:
	void outputBuffer_bytesWritten();
	void requestCompleted(Pillow::HttpConnection* request);
	
protected:
	QByteArray responseBuffer;
	QByteArray response;
	Pillow::HttpParamCollection requestParams;
	
protected:
	Pillow::HttpConnection* createGetRequest(const QByteArray& path = "/", const QByteArray& httpVersion = "1.0");
	Pillow::HttpConnection* createPostRequest(const QByteArray& path = "/", const QByteArray& content = QByteArray(), const QByteArray& httpVersion = "1.0");
	Pillow::HttpConnection* createRequest(const QByteArray& method, const QByteArray& path = "/", const QByteArray& content = QByteArray(), const QByteArray& httpVersion = "1.0");
	
};

class HttpHandlerTest : public HttpHandlerTestBase
{
	Q_OBJECT
	
private slots:
	void testHandlerStack();
	void testHandlerFixed();
	void testHandler404();
	void testHandlerLog();
};

class HttpHandlerFileTest : public HttpHandlerTestBase
{
	Q_OBJECT
	QString testPath;
	
private slots:
	void initTestCase();
	void testServesFiles();
};

class HttpHandlerSimpleRouterTest : public HttpHandlerTestBase
{
	Q_OBJECT
	
protected:
	Q_INVOKABLE void handleRequest1(Pillow::HttpConnection* request);
	
protected slots:
	void handleRequest2(Pillow::HttpConnection* request);
	
private slots:
	void testHandlerRoute();
	void testQObjectMetaCallRoute();
	void testQObjectSlotCallRoute();
	void testStaticRoute();
	void testPathParams();
	void testPathSplats();
	void testMatchesMethod();
	void testUnmatchedRequestAction();
	void testMethodMismatchAction();
	void testSupportsMethodParam();
};

#endif // HTTPHANDLERTEST_H
