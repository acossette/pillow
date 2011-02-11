#ifndef HTTPHANDLERTEST_H
#define HTTPHANDLERTEST_H

#include <QtCore/QObject>
#include <HttpRequest.h>

namespace Pillow { class HttpRequest; }

class HttpHandlerTestBase : public QObject
{
    Q_OBJECT

protected slots:
	void outputBuffer_bytesWritten();
	void requestCompleted(Pillow::HttpRequest* request);
	
protected:
	QByteArray responseBuffer;
	QByteArray response;
	Pillow::HttpParamCollection requestParams;
	
protected:
	Pillow::HttpRequest* createGetRequest(const QByteArray& path = "/");
	Pillow::HttpRequest* createPostRequest(const QByteArray& path = "/", const QByteArray& content = QByteArray());
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
	Q_INVOKABLE void handleRequest1(Pillow::HttpRequest* request);
	
protected slots:
	void handleRequest2(Pillow::HttpRequest* request);
	
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
