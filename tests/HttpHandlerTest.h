#ifndef HTTPHANDLERTEST_H
#define HTTPHANDLERTEST_H

#include <QtCore/QObject>

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
	
protected:
	Pillow::HttpRequest* createGetRequest(const QByteArray& path = "/");
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

#endif // HTTPHANDLERTEST_H
