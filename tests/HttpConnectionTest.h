#ifndef HTTPCONNECTIONTEST_H
#define HTTPCONNECTIONTEST_H

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
class HttpConnectionTest : public QObject
{
	Q_OBJECT

public:
	HttpConnectionTest();

protected:
	Pillow::HttpConnection* connection;
	QSignalSpy* readySpy, *completedSpy, *closedSpy;
	bool reuseConnection;

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
	void testHugePost();
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
	void testWriteChunkedResponseContent();
	void testWriteResponseWithoutRequest();
	void testMultipacketResponse();
	void testReadsRequestParams();
	void testReuseRequest();

	void benchmarkSimpleGetClose();
	void benchmarkSimpleGetKeepAlive();
};

class HttpConnectionTcpSocketTest : public HttpConnectionTest
{
	Q_OBJECT
	QPointer<QTcpServer> server;
	QPointer<QTcpSocket> client;

public:
	HttpConnectionTcpSocketTest();

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
	void testInitialState() { HttpConnectionTest::testInitialState(); }
	void testSimpleGet() { HttpConnectionTest::testSimpleGet(); }
	void testSimplePost() { HttpConnectionTest::testSimplePost(); }
	void testIncrementalPost() { HttpConnectionTest::testIncrementalPost(); }
	void testHugePost() { HttpConnectionTest::testHugePost(); }
	void testInvalidRequestHeaders() { HttpConnectionTest::testInvalidRequestHeaders(); }
	void testOversizedRequestHeaders() { HttpConnectionTest::testOversizedRequestHeaders(); }
	void testInvalidRequestContent() { HttpConnectionTest::testInvalidRequestContent(); }
	void testWriteSimpleResponse() { HttpConnectionTest::testWriteSimpleResponse(); }
	void testWriteSimpleResponseString() { HttpConnectionTest::testWriteSimpleResponseString(); }
	void testConnectionKeepAlive() { HttpConnectionTest::testConnectionKeepAlive(); }
	void testConnectionClose() { HttpConnectionTest::testConnectionClose(); }
	void testPipelinedRequests() { HttpConnectionTest::testPipelinedRequests(); }
	void testClientClosesConnectionEarly() { HttpConnectionTest::testClientClosesConnectionEarly(); }
	void testClientExpects100Continue() { HttpConnectionTest::testClientExpects100Continue(); }
	void testHeadShouldNotSendResponseContent() { HttpConnectionTest::testHeadShouldNotSendResponseContent(); }
	void testWriteIncrementalResponseContent() { HttpConnectionTest::testWriteIncrementalResponseContent(); }
	void testWriteChunkedResponseContent() { HttpConnectionTest::testWriteChunkedResponseContent(); }
	void testWriteResponseWithoutRequest() { HttpConnectionTest::testWriteResponseWithoutRequest(); }
	void testMultipacketResponse() { HttpConnectionTest::testMultipacketResponse(); }
	void testReadsRequestParams() { HttpConnectionTest::testReadsRequestParams(); }
	void testReuseRequest() { HttpConnectionTest::testReuseRequest(); }

	void benchmarkSimpleGetClose() { HttpConnectionTest::benchmarkSimpleGetClose(); }
	void benchmarkSimpleGetKeepAlive() { HttpConnectionTest::benchmarkSimpleGetKeepAlive(); }
};

#if !defined(PILLOW_NO_SSL) && !defined(QT_NO_SSL)

class SslTestServer;
class QSslSocket;
class HttpConnectionSslSocketTest : public HttpConnectionTest
{
	Q_OBJECT
	QPointer<SslTestServer> server;
	QPointer<QSslSocket> client;

public:
	HttpConnectionSslSocketTest();

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
	void testInitialState() { HttpConnectionTest::testInitialState(); }
	void testSimpleGet() { HttpConnectionTest::testSimpleGet(); }
	void testSimplePost() { HttpConnectionTest::testSimplePost(); }
	void testIncrementalPost() { HttpConnectionTest::testIncrementalPost(); }
	void testHugePost() { HttpConnectionTest::testHugePost(); }
	void testInvalidRequestHeaders() { HttpConnectionTest::testInvalidRequestHeaders(); }
	void testOversizedRequestHeaders() { HttpConnectionTest::testOversizedRequestHeaders(); }
	void testInvalidRequestContent() { HttpConnectionTest::testInvalidRequestContent(); }
	void testWriteSimpleResponse() { HttpConnectionTest::testWriteSimpleResponse(); }
	void testWriteSimpleResponseString() { HttpConnectionTest::testWriteSimpleResponseString(); }
	void testConnectionKeepAlive() { HttpConnectionTest::testConnectionKeepAlive(); }
	void testConnectionClose() { HttpConnectionTest::testConnectionClose(); }
	void testPipelinedRequests() { HttpConnectionTest::testPipelinedRequests(); }
	void testClientClosesConnectionEarly() { HttpConnectionTest::testClientClosesConnectionEarly(); }
	void testClientExpects100Continue() { HttpConnectionTest::testClientExpects100Continue(); }
	void testHeadShouldNotSendResponseContent() { HttpConnectionTest::testHeadShouldNotSendResponseContent(); }
	void testWriteIncrementalResponseContent() { HttpConnectionTest::testWriteIncrementalResponseContent(); }
	void testWriteResponseWithoutRequest() { HttpConnectionTest::testWriteResponseWithoutRequest(); }
	void testMultipacketResponse() { HttpConnectionTest::testMultipacketResponse(); }
	void testReadsRequestParams() { HttpConnectionTest::testReadsRequestParams(); }

	void benchmarkSimpleGetClose() { HttpConnectionTest::benchmarkSimpleGetClose(); }
	void benchmarkSimpleGetKeepAlive() { HttpConnectionTest::benchmarkSimpleGetKeepAlive(); }
};

#else

class HttpConnectionSslSocketTest : public QObject
{
	Q_OBJECT
};

#endif // !defined(PILLOW_NO_SSL) && !defined(QT_NO_SSL)

class HttpConnectionLocalSocketTest : public HttpConnectionTest
{
	Q_OBJECT
	QPointer<QLocalServer> server;
	QPointer<QLocalSocket> client;

public:
	HttpConnectionLocalSocketTest();

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
	void testInitialState() { HttpConnectionTest::testInitialState(); }
	void testSimpleGet() { HttpConnectionTest::testSimpleGet(); }
	void testSimplePost() { HttpConnectionTest::testSimplePost(); }
	void testIncrementalPost() { HttpConnectionTest::testIncrementalPost(); }
	void testHugePost() { HttpConnectionTest::testHugePost(); }
	void testInvalidRequestHeaders() { HttpConnectionTest::testInvalidRequestHeaders(); }
	void testOversizedRequestHeaders() { HttpConnectionTest::testOversizedRequestHeaders(); }
	void testInvalidRequestContent() { HttpConnectionTest::testInvalidRequestContent(); }
	void testWriteSimpleResponse() { HttpConnectionTest::testWriteSimpleResponse(); }
	void testWriteSimpleResponseString() { HttpConnectionTest::testWriteSimpleResponseString(); }
	void testConnectionKeepAlive() { HttpConnectionTest::testConnectionKeepAlive(); }
	void testConnectionClose() { HttpConnectionTest::testConnectionClose(); }
	void testPipelinedRequests() { HttpConnectionTest::testPipelinedRequests(); }
	void testClientClosesConnectionEarly() { HttpConnectionTest::testClientClosesConnectionEarly(); }
	void testClientExpects100Continue() { HttpConnectionTest::testClientExpects100Continue(); }
	void testHeadShouldNotSendResponseContent() { HttpConnectionTest::testHeadShouldNotSendResponseContent(); }
	void testWriteIncrementalResponseContent() { HttpConnectionTest::testWriteIncrementalResponseContent(); }
	void testWriteResponseWithoutRequest() { HttpConnectionTest::testWriteResponseWithoutRequest(); }
	void testMultipacketResponse() { HttpConnectionTest::testMultipacketResponse(); }
	void testReadsRequestParams() { HttpConnectionTest::testReadsRequestParams(); }

	void benchmarkSimpleGetClose() { HttpConnectionTest::benchmarkSimpleGetClose(); }
	void benchmarkSimpleGetKeepAlive() { HttpConnectionTest::benchmarkSimpleGetKeepAlive(); }
};

class HttpConnectionBufferTest : public HttpConnectionTest
{
	Q_OBJECT
	QPointer<QBuffer> inputBuffer;
	QPointer<QBuffer> outputBuffer;

public:
	HttpConnectionBufferTest();

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
	void testInitialState() { HttpConnectionTest::testInitialState(); }
	void testSimpleGet() { HttpConnectionTest::testSimpleGet(); }
	void testSimplePost() { HttpConnectionTest::testSimplePost(); }
	void testIncrementalPost() { HttpConnectionTest::testIncrementalPost(); }
	void testHugePost() { HttpConnectionTest::testHugePost(); }
	void testInvalidRequestHeaders() { HttpConnectionTest::testInvalidRequestHeaders(); }
	void testOversizedRequestHeaders() { HttpConnectionTest::testOversizedRequestHeaders(); }
	void testInvalidRequestContent() { HttpConnectionTest::testInvalidRequestContent(); }
	void testWriteSimpleResponse() { HttpConnectionTest::testWriteSimpleResponse(); }
	void testWriteSimpleResponseString() { HttpConnectionTest::testWriteSimpleResponseString(); }
	void testConnectionKeepAlive() { HttpConnectionTest::testConnectionKeepAlive(); }
	void testConnectionClose() { HttpConnectionTest::testConnectionClose(); }
	void testPipelinedRequests() { HttpConnectionTest::testPipelinedRequests(); }
	void testClientClosesConnectionEarly() { HttpConnectionTest::testClientClosesConnectionEarly(); }
	void testClientExpects100Continue() { HttpConnectionTest::testClientExpects100Continue(); }
	void testHeadShouldNotSendResponseContent() { HttpConnectionTest::testHeadShouldNotSendResponseContent(); }
	void testWriteIncrementalResponseContent() { HttpConnectionTest::testWriteIncrementalResponseContent(); }
	void testWriteResponseWithoutRequest() { HttpConnectionTest::testWriteResponseWithoutRequest(); }
	void testMultipacketResponse() { HttpConnectionTest::testMultipacketResponse(); }
	void testReadsRequestParams() { HttpConnectionTest::testReadsRequestParams(); }

	void benchmarkSimpleGetClose() { HttpConnectionTest::benchmarkSimpleGetClose(); }
	void benchmarkSimpleGetKeepAlive() { HttpConnectionTest::benchmarkSimpleGetKeepAlive(); }
};

#endif // HTTPCONNECTIONTEST_H
