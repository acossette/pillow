#ifndef HTTPHANDLERTEST_H
#define HTTPHANDLERTEST_H

#include <QObject>

namespace Pillow { class HttpRequest; }

class HttpHandlerTestBase : public QObject
{
    Q_OBJECT

protected slots:
	void requestCompleted(Pillow::HttpRequest* request);
	
protected:
	QByteArray response;	
	
protected:
	Pillow::HttpRequest* createGetRequest(const QByteArray& path = "/");
	QByteArray readResponse(Pillow::HttpRequest* request);
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

#endif // HTTPHANDLERTEST_H
