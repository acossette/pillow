#ifndef HTTPREQUESTTEST_H
#define HTTPREQUESTTEST_H

#include <QObject>
#include <QPointer>
#include <QtNetwork/QSslError>
class QTcpServer;
class QTcpSocket;
class QLocalServer;
class QLocalSocket;
class QSignalSpy;
class QBuffer;
namespace Pillow { class HttpConnection; }
class HttpRequestTest : public QObject
{
    Q_OBJECT

public:
	HttpRequestTest();	
	
protected:
	Pillow::HttpConnection* request;
	QSignalSpy* readySpy, *completedSpy, *closedSpy;
	bool reuseRequest;
	
protected: // Helper methods.
	virtual void clientWrite(const QByteArray& data) = 0;
	virtual void clientFlush(bool wait = true) = 0;
	virtual QByteArray clientReadAll() = 0;	
	virtual void clientClose() = 0;
	virtual bool isClientConnected() = 0;

protected slots: // Test methods.
	virtual void init();
	virtual void cleanup();

	// Behavior tests.
	void testInitialState();
	void testSimpleGet();
	void testSimplePost();
	void testIncrementalPost();
	void testInvalidRequestHeaders();
	void testOversizedRequestHeaders();
	void testInvalidRequestContent();
	void testWriteSimpleResponse();
	void testWriteSimpleResponseString();
	void testConnectionKeepAlive();
	void testConnectionClose();
	void testPipelinedRequests();
	void testClientClosesConnectionEarly();
	void testClientExpects100Continue();
	void testHeadShouldNotSendResponseContent();
	void testWriteIncrementalResponseContent();
	void testWriteResponseWithoutRequest();
	void testMultipacketResponse();
	void testReadsRequestParams();
	void testReuseRequest();
	
	void benchmarkSimpleGetClose();
	void benchmarkSimpleGetKeepAlive();
};

class HttpRequestTcpSocketTest : public HttpRequestTest
{
    Q_OBJECT
	QPointer<QTcpServer> server;
	QPointer<QTcpSocket> client;
	
public:
	HttpRequestTcpSocketTest();

protected: // Helper methods.
	virtual void clientWrite(const QByteArray& data);
	virtual void clientFlush(bool wait = true);
	virtual QByteArray clientReadAll();	
	virtual void clientClose();
	virtual bool isClientConnected();

protected slots:
	void server_newConnection();	
	
private slots: // Test methods.
	virtual void init();
	virtual void cleanup();

	void testInit() { cleanup(); init(); }	
	
	// Behavior tests.
	void testInitialState() { HttpRequestTest::testInitialState(); }
	void testSimpleGet() { HttpRequestTest::testSimpleGet(); }
	void testSimplePost() { HttpRequestTest::testSimplePost(); }
	void testIncrementalPost() { HttpRequestTest::testIncrementalPost(); }
	void testInvalidRequestHeaders() { HttpRequestTest::testInvalidRequestHeaders(); }
	void testOversizedRequestHeaders() { HttpRequestTest::testOversizedRequestHeaders(); }
	void testInvalidRequestContent() { HttpRequestTest::testInvalidRequestContent(); }
	void testWriteSimpleResponse() { HttpRequestTest::testWriteSimpleResponse(); }
	void testWriteSimpleResponseString() { HttpRequestTest::testWriteSimpleResponseString(); }
	void testConnectionKeepAlive() { HttpRequestTest::testConnectionKeepAlive(); }
	void testConnectionClose() { HttpRequestTest::testConnectionClose(); }
	void testPipelinedRequests() { HttpRequestTest::testPipelinedRequests(); }
	void testClientClosesConnectionEarly() { HttpRequestTest::testClientClosesConnectionEarly(); }
	void testClientExpects100Continue() { HttpRequestTest::testClientExpects100Continue(); }
	void testHeadShouldNotSendResponseContent() { HttpRequestTest::testHeadShouldNotSendResponseContent(); }
	void testWriteIncrementalResponseContent() { HttpRequestTest::testWriteIncrementalResponseContent(); }
	void testWriteResponseWithoutRequest() { HttpRequestTest::testWriteResponseWithoutRequest(); }
	void testMultipacketResponse() { HttpRequestTest::testMultipacketResponse(); }
	void testReadsRequestParams() { HttpRequestTest::testReadsRequestParams(); }
	void testReuseRequest() { HttpRequestTest::testReuseRequest(); }
	
	void benchmarkSimpleGetClose() { HttpRequestTest::benchmarkSimpleGetClose(); }
	void benchmarkSimpleGetKeepAlive() { HttpRequestTest::benchmarkSimpleGetKeepAlive(); }
};

#ifndef PILLOW_NO_SSL

class SslTestServer;
class QSslSocket;
class HttpRequestSslSocketTest : public HttpRequestTest
{
    Q_OBJECT
	QPointer<SslTestServer> server;
	QPointer<QSslSocket> client;
	
public:
	HttpRequestSslSocketTest();

protected: // Helper methods.
	virtual void clientWrite(const QByteArray& data);
	virtual void clientFlush(bool wait = true);
	virtual QByteArray clientReadAll();	
	virtual void clientClose();
	virtual bool isClientConnected();

public slots:
	void server_newConnection();	
	void sslSocket_encrypted();
	void sslSocket_sslErrors(const QList<QSslError>& sslErrors);
	
private slots: // Test methods.
	virtual void init();
	virtual void cleanup();

	void testInit() { cleanup(); init(); }
	
	// Behavior tests.
	void testInitialState() { HttpRequestTest::testInitialState(); }
	void testSimpleGet() { HttpRequestTest::testSimpleGet(); }
	void testSimplePost() { HttpRequestTest::testSimplePost(); }
	void testIncrementalPost() { HttpRequestTest::testIncrementalPost(); }
	void testInvalidRequestHeaders() { HttpRequestTest::testInvalidRequestHeaders(); }
	void testOversizedRequestHeaders() { HttpRequestTest::testOversizedRequestHeaders(); }
	void testInvalidRequestContent() { HttpRequestTest::testInvalidRequestContent(); }
	void testWriteSimpleResponse() { HttpRequestTest::testWriteSimpleResponse(); }
	void testWriteSimpleResponseString() { HttpRequestTest::testWriteSimpleResponseString(); }
	void testConnectionKeepAlive() { HttpRequestTest::testConnectionKeepAlive(); }
	void testConnectionClose() { HttpRequestTest::testConnectionClose(); }
	void testPipelinedRequests() { HttpRequestTest::testPipelinedRequests(); }
	void testClientClosesConnectionEarly() { HttpRequestTest::testClientClosesConnectionEarly(); }
	void testClientExpects100Continue() { HttpRequestTest::testClientExpects100Continue(); }
	void testHeadShouldNotSendResponseContent() { HttpRequestTest::testHeadShouldNotSendResponseContent(); }
	void testWriteIncrementalResponseContent() { HttpRequestTest::testWriteIncrementalResponseContent(); }
	void testWriteResponseWithoutRequest() { HttpRequestTest::testWriteResponseWithoutRequest(); }
	void testMultipacketResponse() { HttpRequestTest::testMultipacketResponse(); }
	void testReadsRequestParams() { HttpRequestTest::testReadsRequestParams(); }
	
	void benchmarkSimpleGetClose() { HttpRequestTest::benchmarkSimpleGetClose(); }
	void benchmarkSimpleGetKeepAlive() { HttpRequestTest::benchmarkSimpleGetKeepAlive(); }
};

#else

class HttpRequestSslSocketTest : public QObject
{
	Q_OBJECT
};

#endif // !PILLOW_NO_SSL

class HttpRequestLocalSocketTest : public HttpRequestTest
{
    Q_OBJECT
	QPointer<QLocalServer> server;
	QPointer<QLocalSocket> client;
	
public:
	HttpRequestLocalSocketTest();

protected: // Helper methods.
	virtual void clientWrite(const QByteArray& data);
	virtual void clientFlush(bool wait = true);
	virtual QByteArray clientReadAll();	
	virtual void clientClose();
	virtual bool isClientConnected();

protected slots:
	void server_newConnection();	
	
private slots: // Test methods.
	virtual void init();
	virtual void cleanup();
	
	void testInit() { cleanup(); init(); }
	
	// Behavior tests.
	void testInitialState() { HttpRequestTest::testInitialState(); }
	void testSimpleGet() { HttpRequestTest::testSimpleGet(); }
	void testSimplePost() { HttpRequestTest::testSimplePost(); }
	void testIncrementalPost() { HttpRequestTest::testIncrementalPost(); }
	void testInvalidRequestHeaders() { HttpRequestTest::testInvalidRequestHeaders(); }
	void testOversizedRequestHeaders() { HttpRequestTest::testOversizedRequestHeaders(); }
	void testInvalidRequestContent() { HttpRequestTest::testInvalidRequestContent(); }
	void testWriteSimpleResponse() { HttpRequestTest::testWriteSimpleResponse(); }
	void testWriteSimpleResponseString() { HttpRequestTest::testWriteSimpleResponseString(); }
	void testConnectionKeepAlive() { HttpRequestTest::testConnectionKeepAlive(); }
	void testConnectionClose() { HttpRequestTest::testConnectionClose(); }
	void testPipelinedRequests() { HttpRequestTest::testPipelinedRequests(); }
	void testClientClosesConnectionEarly() { HttpRequestTest::testClientClosesConnectionEarly(); }
	void testClientExpects100Continue() { HttpRequestTest::testClientExpects100Continue(); }
	void testHeadShouldNotSendResponseContent() { HttpRequestTest::testHeadShouldNotSendResponseContent(); }
	void testWriteIncrementalResponseContent() { HttpRequestTest::testWriteIncrementalResponseContent(); }
	void testWriteResponseWithoutRequest() { HttpRequestTest::testWriteResponseWithoutRequest(); }
	void testMultipacketResponse() { HttpRequestTest::testMultipacketResponse(); }
	void testReadsRequestParams() { HttpRequestTest::testReadsRequestParams(); }
	
	void benchmarkSimpleGetClose() { HttpRequestTest::benchmarkSimpleGetClose(); }
	void benchmarkSimpleGetKeepAlive() { HttpRequestTest::benchmarkSimpleGetKeepAlive(); }
};

class HttpRequestBufferTest : public HttpRequestTest
{
    Q_OBJECT
	QPointer<QBuffer> inputBuffer;
	QPointer<QBuffer> outputBuffer;
	
public:
	HttpRequestBufferTest();

protected: // Helper methods.
	virtual void clientWrite(const QByteArray& data);
	virtual void clientFlush(bool wait = true);
	virtual QByteArray clientReadAll();	
	virtual void clientClose();
	virtual bool isClientConnected();
	
private slots: // Test methods.
	virtual void init();
	virtual void cleanup();
	
	void testInit() { cleanup(); init(); }
	
	// Behavior tests.
	void testInitialState() { HttpRequestTest::testInitialState(); }
	void testSimpleGet() { HttpRequestTest::testSimpleGet(); }
	void testSimplePost() { HttpRequestTest::testSimplePost(); }
	void testIncrementalPost() { HttpRequestTest::testIncrementalPost(); }
	void testInvalidRequestHeaders() { HttpRequestTest::testInvalidRequestHeaders(); }
	void testOversizedRequestHeaders() { HttpRequestTest::testOversizedRequestHeaders(); }
	void testInvalidRequestContent() { HttpRequestTest::testInvalidRequestContent(); }
	void testWriteSimpleResponse() { HttpRequestTest::testWriteSimpleResponse(); }
	void testWriteSimpleResponseString() { HttpRequestTest::testWriteSimpleResponseString(); }
	void testConnectionKeepAlive() { HttpRequestTest::testConnectionKeepAlive(); }
	void testConnectionClose() { HttpRequestTest::testConnectionClose(); }
	void testPipelinedRequests() { HttpRequestTest::testPipelinedRequests(); }
	void testClientClosesConnectionEarly() { HttpRequestTest::testClientClosesConnectionEarly(); }
	void testClientExpects100Continue() { HttpRequestTest::testClientExpects100Continue(); }
	void testHeadShouldNotSendResponseContent() { HttpRequestTest::testHeadShouldNotSendResponseContent(); }
	void testWriteIncrementalResponseContent() { HttpRequestTest::testWriteIncrementalResponseContent(); }
	void testWriteResponseWithoutRequest() { HttpRequestTest::testWriteResponseWithoutRequest(); }
	void testMultipacketResponse() { HttpRequestTest::testMultipacketResponse(); }
	void testReadsRequestParams() { HttpRequestTest::testReadsRequestParams(); }
	
	void benchmarkSimpleGetClose() { HttpRequestTest::benchmarkSimpleGetClose(); }
	void benchmarkSimpleGetKeepAlive() { HttpRequestTest::benchmarkSimpleGetKeepAlive(); }
};

#endif // HTTPREQUESTTEST_H
