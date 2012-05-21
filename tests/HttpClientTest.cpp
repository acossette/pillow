#include <QtCore/QObject>
#include <QtTest/QtTest>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkCookie>
#include <HttpClient.h>
#include <HttpConnection.h>
#include <HttpServer.h>
#include <HttpHandlerSimpleRouter.h>
#include <HttpHelpers.h>
#include "Helpers.h"
#include <functional>

typedef QList<QByteArray> Chunks;
Q_DECLARE_METATYPE(Chunks)
Q_DECLARE_METATYPE(QAbstractSocket::SocketState)
Q_DECLARE_METATYPE(QNetworkReply::NetworkError)

class NetworkAccessManagerTest : public QObject
{
	Q_OBJECT
	Pillow::NetworkAccessManager *nam;
	TestServer server;

private slots:
	void initTestCase()
	{
		QVERIFY(server.listen(QHostAddress::LocalHost, 4571));
	}

	void init()
	{
		nam = new Pillow::NetworkAccessManager();
		QVERIFY(server.receivedRequests.isEmpty());
		QVERIFY(server.receivedConnections.isEmpty());
		QVERIFY(server.receivedSockets.isEmpty());
	}

	void cleanup()
	{
		delete nam; nam = 0;
		server.receivedRequests.clear();
		server.receivedConnections.clear();
		server.receivedSockets.clear();
	}

private:
	QUrl testUrl() const { return QUrl("http://127.0.0.1:4571/test/path"); }

private slots:
	void should_send_valid_requests_data()
	{
		QTest::addColumn<QByteArray>("method");
		QTest::addColumn<QUrl>("url");
		QTest::addColumn<Pillow::HttpHeaderCollection>("headers");
		QTest::addColumn<QByteArray>("content");
		QTest::addColumn<HttpRequestData>("expectedRequestData");

		Pillow::HttpHeaderCollection baseExpectedHeaders;
		baseExpectedHeaders << Pillow::HttpHeader("Host", "127.0.0.1:4571");

		QTest::newRow("Simple GET") << QByteArray("GET")
									<< QUrl("http://127.0.0.1:4571/")
									<< Pillow::HttpHeaderCollection()
									<< QByteArray()
									<< HttpRequestData()
									   .withMethod("GET")
									   .withUri("/").withPath("/").withQueryString("").withFragment("")
									   .withHttpVersion("HTTP/1.1")
									   .withContent("")
									   .withHeaders(baseExpectedHeaders);

		QTest::newRow("GET with headers") << QByteArray("GET")
									<< QUrl("http://127.0.0.1:4571/some/path?and=query")
									<< (Pillow::HttpHeaderCollection()
										<< Pillow::HttpHeader("X-Some", "Header")
										<< Pillow::HttpHeader("X-And-Another", "even-better; header"))
									<< QByteArray()
									<< HttpRequestData()
									   .withMethod("GET")
									   .withUri("/some/path?and=query").withPath("/some/path").withQueryString("and=query").withFragment("")
									   .withHttpVersion("HTTP/1.1")
									   .withContent("")
									   .withHeaders(Pillow::HttpHeaderCollection(baseExpectedHeaders)
													<< Pillow::HttpHeader("X-Some", "Header")
													<< Pillow::HttpHeader("X-And-Another", "even-better; header"));
		QTest::newRow("GET with connection: close") << QByteArray("GET")
									<< QUrl("http://127.0.0.1:4571/")
									<< (Pillow::HttpHeaderCollection()
										<< Pillow::HttpHeader("Connection", "close"))
									<< QByteArray()
									<< HttpRequestData()
									   .withMethod("GET")
									   .withUri("/").withPath("/").withQueryString("").withFragment("")
									   .withHttpVersion("HTTP/1.1")
									   .withContent("")
									   .withHeaders(Pillow::HttpHeaderCollection(baseExpectedHeaders)
													<< Pillow::HttpHeader("Connection", "close"));
		QTest::newRow("Simple PUT") << QByteArray("PUT")
									<< QUrl("http://127.0.0.1:4571/some/path")
									<< (Pillow::HttpHeaderCollection())
									<< QByteArray("Some sent data")
									<< HttpRequestData()
									   .withMethod("PUT")
									   .withUri("/some/path").withPath("/some/path").withQueryString("").withFragment("")
									   .withHttpVersion("HTTP/1.1")
									   .withContent("Some sent data")
									   .withHeaders(Pillow::HttpHeaderCollection(baseExpectedHeaders)
													<< Pillow::HttpHeader("Content-Length", "14"));
		QTest::newRow("Large POST") << QByteArray("POST")
									<< QUrl("http://127.0.0.1:4571/some/large/path")
									<< (Pillow::HttpHeaderCollection())
									<< QByteArray(128 * 1024, '*')
									<< HttpRequestData()
									   .withMethod("POST")
									   .withUri("/some/large/path").withPath("/some/large/path").withQueryString("").withFragment("")
									   .withHttpVersion("HTTP/1.1")
									   .withContent(QByteArray(128 * 1024, '*'))
									   .withHeaders(Pillow::HttpHeaderCollection(baseExpectedHeaders)
													<< Pillow::HttpHeader("Content-Length", "131072"));
	}

	void should_send_valid_requests()
	{
		QFETCH(QByteArray, method);
		QFETCH(QUrl, url);
		QFETCH(Pillow::HttpHeaderCollection, headers);
		QFETCH(QByteArray, content);
		QFETCH(HttpRequestData, expectedRequestData);

		QNetworkRequest request;
		request.setUrl(url);
		foreach (const Pillow::HttpHeader &header, headers) request.setRawHeader(header.first, header.second);
		QBuffer requestContent(&content); requestContent.open(QIODevice::ReadOnly);

		nam->sendCustomRequest(request, method, &requestContent);

		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedRequests.size(), 1);
		QCOMPARE(server.receivedRequests.first(), expectedRequestData);
	}

	void should_receive_response()
	{
		QNetworkReply *r = nam->get(QNetworkRequest(testUrl()));
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(201, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("X-Some", "Header"), "Hello World!");

		QVERIFY(waitForSignal(r, SIGNAL(finished())));

		QCOMPARE(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 201);
		QCOMPARE(r->readAll(), QByteArray("Hello World!"));
		QCOMPARE(r->rawHeaderPairs(), Pillow::HttpHeaderCollection()
				 << Pillow::HttpHeader("X-Some", "Header")
				 << Pillow::HttpHeader("Content-Length", "12")
				 << Pillow::HttpHeader("Content-Type", "text/plain"));
	}

	void should_set_cooked_headers_from_received_headers()
	{
		QNetworkReply *r = nam->get(QNetworkRequest(testUrl()));
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(
					302,
					Pillow::HttpHeaderCollection()
					<< Pillow::HttpHeader("Location", "http://example.org")
					<< Pillow::HttpHeader("Content-Type", "some/type")
					<< Pillow::HttpHeader("Last-Modified", Pillow::HttpProtocol::Dates::getHttpDate(QDateTime(QDate(2012, 4, 30), QTime(8, 9, 0))))
					<< Pillow::HttpHeader("Set-Cookie", "ChocolateCookie=Very Delicious")
					, "Hello!");

		QVERIFY(waitForSignal(r, SIGNAL(finished())));
		QCOMPARE(r->readAll(), QByteArray("Hello!"));
		QCOMPARE(r->rawHeaderPairs(), Pillow::HttpHeaderCollection()
				 << Pillow::HttpHeader("Location", "http://example.org")
				 << Pillow::HttpHeader("Last-Modified", "Mon, 30 Apr 2012 12:09:00 GMT")
				 << Pillow::HttpHeader("Content-Length", "6")
				 << Pillow::HttpHeader("Content-Type", "some/type")
				 << Pillow::HttpHeader("Set-Cookie", "ChocolateCookie=Very Delicious")
				 );

		QCOMPARE(r->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl(), QUrl("http://example.org"));
		QCOMPARE(r->header(QNetworkRequest::ContentTypeHeader).toByteArray(), QByteArray("some/type"));
		QCOMPARE(r->header(QNetworkRequest::ContentLengthHeader).toInt(), 6);
		QCOMPARE(r->header(QNetworkRequest::LastModifiedHeader).toDateTime(), QDateTime(QDate(2012, 4, 30), QTime(8, 9, 0)));

		QList<QNetworkCookie> cookies;
		cookies << QNetworkCookie("ChocolateCookie", "Very Delicious");
		QCOMPARE(r->header(QNetworkRequest::SetCookieHeader).value<QList<QNetworkCookie> >(), cookies);
	}

	void should_store_received_cookies_in_the_jar()
	{
		class JarOpener : public QNetworkCookieJar
		{
		public:
			QList<QNetworkCookie> getAllCookies() const { return QNetworkCookieJar::allCookies(); }
		};

		QCOMPARE(static_cast<JarOpener*>(nam->cookieJar())->getAllCookies().size(), 0);

		QNetworkReply *r = nam->get(QNetworkRequest(testUrl()));
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200,
					Pillow::HttpHeaderCollection()
					<< Pillow::HttpHeader("Set-Cookie", "SomeCookie=Super Chocolate")
					<< Pillow::HttpHeader("seT-CoOKie", "AnotherCookie=Mega Caramel")
					, "Hello!");

		QVERIFY(waitForSignal(r, SIGNAL(finished())));
		QCOMPARE(static_cast<JarOpener*>(nam->cookieJar())->getAllCookies().size(), 2);

		QList<QNetworkCookie> cookies;
		cookies << QNetworkCookie("SomeCookie", "Super Chocolate");
		cookies << QNetworkCookie("AnotherCookie", "Mega Caramel");
		QCOMPARE(r->header(QNetworkRequest::SetCookieHeader).value<QList<QNetworkCookie> >(), cookies);
	}

	void should_send_cookies_from_the_jar()
	{
		QNetworkCookieJar jar;
		QNetworkCookie c1("First", "FirstValue");
		QNetworkCookie c2("Second", "SecondValue");
		QNetworkCookie c3("Third", "ThirdValue");
		jar.setCookiesFromUrl(QList<QNetworkCookie>() << c1 << c3, QUrl("http://127.0.0.1:4571/"));
		jar.setCookiesFromUrl(QList<QNetworkCookie>() << c2, QUrl("http://127.0.1.1:7777/"));

		nam->setCookieJar(&jar); jar.setParent(0);
		nam->get(QNetworkRequest(testUrl()));
		QVERIFY(server.waitForRequest());

		int cookieHeaderCount = 0;
		Pillow::HttpHeaderCollection headers = server.receivedConnections.last()->requestHeaders();
		foreach (const Pillow::HttpHeader &h, headers)
		{
			if (h.first.toLower() == "cookie")
				cookieHeaderCount++;
		}

		QCOMPARE(cookieHeaderCount, 1);

		QByteArray cookieValue = server.receivedConnections.last()->requestHeaderValue("Cookie");
		QCOMPARE(cookieValue, QByteArray("First=FirstValue; Third=ThirdValue"));
	}

	void should_set_headers_as_soon_as_they_are_received()
	{
		QNetworkReply *r = nam->get(QNetworkRequest(testUrl()));
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeHeaders(200, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("X-Some", "Header") << Pillow::HttpHeader("Content-Length", "12") << Pillow::HttpHeader("Content-Type", "text/plain"));

		QSignalSpy finishedSpy(r, SIGNAL(finished()));
		QVERIFY(waitForSignal(r, SIGNAL(metaDataChanged())));
		QVERIFY(finishedSpy.isEmpty()); // The request should not be finished yet even though headers have been received

		QCOMPARE(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
		QCOMPARE(r->rawHeaderPairs(), Pillow::HttpHeaderCollection()
				 << Pillow::HttpHeader("X-Some", "Header")
				 << Pillow::HttpHeader("Content-Length", "12")
				 << Pillow::HttpHeader("Content-Type", "text/plain"));
		QCOMPARE(r->header(QNetworkRequest::ContentTypeHeader).toByteArray(), QByteArray("text/plain"));
		QCOMPARE(r->header(QNetworkRequest::ContentLengthHeader).toInt(), 12);
		QCOMPARE(r->readAll(), QByteArray());

		server.receivedConnections.last()->writeContent("Hello World!");

		QVERIFY(waitForSignal(r, SIGNAL(finished())));

		QCOMPARE(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
		QCOMPARE(r->rawHeaderPairs(), Pillow::HttpHeaderCollection()
				 << Pillow::HttpHeader("X-Some", "Header")
				 << Pillow::HttpHeader("Content-Length", "12")
				 << Pillow::HttpHeader("Content-Type", "text/plain"));
		QCOMPARE(r->header(QNetworkRequest::ContentTypeHeader).toByteArray(), QByteArray("text/plain"));
		QCOMPARE(r->header(QNetworkRequest::ContentLengthHeader).toInt(), 12);
		QCOMPARE(r->readAll(), QByteArray("Hello World!"));
	}

	void should_make_request_accessible()
	{
		QNetworkRequest originalRequest;
		originalRequest.setUrl(testUrl());
		originalRequest.setRawHeader("One-Header", "OneValue");
		originalRequest.setRawHeader("Another-Header", "AnotherValue");
		originalRequest.setPriority(QNetworkRequest::HighPriority);

		QNetworkReply *r = nam->post(originalRequest, QByteArray("hello"));
		QCOMPARE(r->request(), originalRequest);
		QCOMPARE(r->url(), testUrl());
		QCOMPARE(r->operation(), QNetworkAccessManager::PostOperation);
	}

	void should_allow_aborting_reply()
	{
		QNetworkReply *r = nam->get(QNetworkRequest(testUrl()));
		QVERIFY(server.waitForRequest());
		QVERIFY(server.receivedSockets.last()->state() == QAbstractSocket::ConnectedState);
		QPointer<QTcpSocket> socket = server.receivedSockets.last();
		QVERIFY(socket != 0);
		QSignalSpy errorSpy(r, SIGNAL(error(QNetworkReply::NetworkError)));
		QSignalSpy finishedSpy(r, SIGNAL(finished()));
		r->abort();
		QCOMPARE(finishedSpy.size(), 1);
		QCOMPARE(errorSpy.size(), 1);
		QCOMPARE(errorSpy.last().first().value<QNetworkReply::NetworkError>(), QNetworkReply::OperationCanceledError);
		QVERIFY(waitFor([=]{ return socket == 0; })); // The socket should get closed and deleted.
	}

	void should_emit_readyRead_when_new_content_is_available()
	{
		QNetworkReply *r = nam->get(QNetworkRequest(testUrl()));
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeHeaders(200);

		QCOMPARE(r->bytesAvailable(), qint64(0));

		server.receivedConnections.last()->writeContent("Hello");
		QVERIFY(waitForSignal(r, SIGNAL(readyRead())));
		QCOMPARE(r->bytesAvailable(), qint64(5));
		QCOMPARE(r->pos(), qint64(0));
		QCOMPARE(r->read(3), QByteArray("Hel"));
		QCOMPARE(r->bytesAvailable(), qint64(2));
		QCOMPARE(r->pos(), qint64(0));

		server.receivedConnections.last()->writeContent("the");
		QVERIFY(waitForSignal(r, SIGNAL(readyRead())));
		QCOMPARE(r->bytesAvailable(), qint64(5));
		QCOMPARE(r->read(4), QByteArray("loth"));
		QCOMPARE(r->bytesAvailable(), qint64(1));
		QCOMPARE(r->pos(), qint64(0));

		server.receivedConnections.last()->writeContent("world!");
		QVERIFY(waitForSignal(r, SIGNAL(readyRead())));
		QCOMPARE(r->bytesAvailable(), qint64(7));
		QCOMPARE(r->read(5), QByteArray("eworl"));
		QCOMPARE(r->bytesAvailable(), qint64(2));
		QCOMPARE(r->read(2), QByteArray("d!"));
		QCOMPARE(r->bytesAvailable(), qint64(0));
	}

	void should_report_errors()
	{
		{
			QNetworkReply *r = nam->get(QNetworkRequest(QUrl("http://4230948234.423.423.423.423.32.423.4/bad")));
			QSignalSpy errorSpy(r, SIGNAL(error(QNetworkReply::NetworkError)));
			QVERIFY(waitFor([&]{ return errorSpy.size() > 0; }));
			QCOMPARE(errorSpy.last().first().value<QNetworkReply::NetworkError>(), QNetworkReply::UnknownNetworkError);
		}

		{
			QNetworkReply *r = nam->get(QNetworkRequest(testUrl()));
			QSignalSpy errorSpy(r, SIGNAL(error(QNetworkReply::NetworkError)));
			QVERIFY(server.waitForRequest());
			server.receivedConnections.last()->outputDevice()->write("=-=-=-=-=-FFFFFFFUUUUUUUUUUU=-=-=-=-=-=!\r\n");
			QVERIFY(waitFor([&]{ return errorSpy.size() > 0; }));
			QCOMPARE(errorSpy.last().first().value<QNetworkReply::NetworkError>(), QNetworkReply::ProtocolUnknownError);
		}
	}

	void should_use_default_QNetworkAccessManager_implementation_for_non_http_schemes()
	{
		QByteArray url = "data:text/plain;base64," + QByteArray("Hello World!").toBase64();

		QNetworkReply *r = nam->get(QNetworkRequest(QUrl(url)));
		QVERIFY(waitForSignal(r, SIGNAL(finished())));

		QCOMPARE(r->readAll(), QByteArray("Hello World!"));
	}

	void should_reuse_existing_connection_to_same_server_when_available()
	{
		QNetworkReply *r = nam->get(QNetworkRequest(testUrl()));
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForSignal(r, SIGNAL(finished())));

		r = nam->get(QNetworkRequest(testUrl()));
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(201);
		QVERIFY(waitForSignal(r, SIGNAL(finished())));

		QCOMPARE(server.receivedSockets.size(), 2);
		QVERIFY(server.receivedSockets.at(0) == server.receivedSockets.at(1));
		QVERIFY(server.receivedSockets.at(0) != 0);

		r = nam->get(QNetworkRequest(testUrl()));
		QNetworkReply *r4 = nam->get(QNetworkRequest(testUrl()));
		QNetworkReply *r5 = nam->get(QNetworkRequest(testUrl()));

		QVERIFY(server.waitForRequest());
		QVERIFY(waitFor([&]{ return server.receivedConnections.size() == 5; }));
		QVERIFY(server.receivedSockets.at(2) == server.receivedSockets.at(1));
		QVERIFY(server.receivedSockets.at(3) != server.receivedSockets.at(2));
		QVERIFY(server.receivedSockets.at(4) != server.receivedSockets.at(2));
		QVERIFY(server.receivedSockets.at(4) != server.receivedSockets.at(3));

		server.receivedConnections.at(2)->writeResponse(400);
		server.receivedConnections.at(3)->writeResponse(404);
		server.receivedConnections.at(4)->writeResponse(500);

		QVERIFY(waitFor([&]{ return !r->isRunning() && !r4->isRunning() && !r5->isRunning(); }));
	}
};
PILLOW_TEST_DECLARE(NetworkAccessManagerTest)

class HttpClientTest : public QObject
{
	Q_OBJECT

	Pillow::HttpClient *client;
	TestServer server;
	TestServer server2;

private slots:
	void initTestCase()
	{
		QVERIFY(server.listen(QHostAddress::LocalHost, 4569));
		QVERIFY(server2.listen(QHostAddress::LocalHost, 4570));
		qRegisterMetaType<Pillow::HttpConnection*>("Pillow::HttpConnection*");
	}

	void init()
	{
		client = new Pillow::HttpClient();
		QVERIFY(server.receivedRequests.isEmpty());
		QVERIFY(server.receivedConnections.isEmpty());
		QVERIFY(server.receivedSockets.isEmpty());
		QVERIFY(server2.receivedRequests.isEmpty());
		QVERIFY(server2.receivedConnections.isEmpty());
		QVERIFY(server2.receivedSockets.isEmpty());
	}

	void cleanup()
	{
		delete client; client = 0;
		server.receivedRequests.clear();
		server.receivedConnections.clear();
		server.receivedSockets.clear();
		server2.receivedRequests.clear();
		server2.receivedConnections.clear();
		server2.receivedSockets.clear();
	}

private:
	bool waitForResponse(int maxTime = 500)
	{
		QElapsedTimer t; t.start();
		while (client->responsePending() && t.elapsed() < maxTime) QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
		if (client->responsePending())
		{
			qWarning() << "Timed out waiting for response";
			return false;
		}
		return true;
	}

	bool waitForContentReadyRead(int maxTime = 500)
	{
		return waitForSignal(client, SIGNAL(contentReadyRead()), maxTime);
	}

	QUrl testUrl() { return QUrl("http://127.0.0.1:4569/test");	}

protected slots:
	void abortSender() { static_cast<Pillow::HttpClient*>(sender())->abort(); }
	void sendRequest() { client->get(testUrl()); }

private slots:
	void should_be_initially_blank()
	{
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QVERIFY(!client->responsePending());
		QCOMPARE(client->headers(), Pillow::HttpHeaderCollection());
		QCOMPARE(client->statusCode(), 0);
		QCOMPARE(client->content(), QByteArray());
	}

	void should_send_valid_requests_data()
	{
		QTest::addColumn<QByteArray>("method");
		QTest::addColumn<QUrl>("url");
		QTest::addColumn<Pillow::HttpHeaderCollection>("headers");
		QTest::addColumn<QByteArray>("content");
		QTest::addColumn<HttpRequestData>("expectedRequestData");

		Pillow::HttpHeaderCollection baseExpectedHeaders;
		baseExpectedHeaders << Pillow::HttpHeader("Host", "127.0.0.1:4569");

		QTest::newRow("Simple GET") << QByteArray("GET")
									<< QUrl("http://127.0.0.1:4569/")
									<< Pillow::HttpHeaderCollection()
									<< QByteArray()
									<< HttpRequestData()
									   .withMethod("GET")
									   .withUri("/").withPath("/").withQueryString("").withFragment("")
									   .withHttpVersion("HTTP/1.1")
									   .withContent("")
									   .withHeaders(baseExpectedHeaders);

		QTest::newRow("GET with headers") << QByteArray("GET")
									<< QUrl("http://127.0.0.1:4569/some/path?and=query")
									<< (Pillow::HttpHeaderCollection()
										<< Pillow::HttpHeader("X-Some", "Header")
										<< Pillow::HttpHeader("X-And-Another", "even-better; header"))
									<< QByteArray()
									<< HttpRequestData()
									   .withMethod("GET")
									   .withUri("/some/path?and=query").withPath("/some/path").withQueryString("and=query").withFragment("")
									   .withHttpVersion("HTTP/1.1")
									   .withContent("")
									   .withHeaders(Pillow::HttpHeaderCollection(baseExpectedHeaders)
													<< Pillow::HttpHeader("X-Some", "Header")
													<< Pillow::HttpHeader("X-And-Another", "even-better; header"));
		QTest::newRow("GET with connection: close") << QByteArray("GET")
									<< QUrl("http://127.0.0.1:4569/")
									<< (Pillow::HttpHeaderCollection()
										<< Pillow::HttpHeader("Connection", "close"))
									<< QByteArray()
									<< HttpRequestData()
									   .withMethod("GET")
									   .withUri("/").withPath("/").withQueryString("").withFragment("")
									   .withHttpVersion("HTTP/1.1")
									   .withContent("")
									   .withHeaders(Pillow::HttpHeaderCollection(baseExpectedHeaders)
													<< Pillow::HttpHeader("Connection", "close"));
		QTest::newRow("Simple PUT") << QByteArray("PUT")
									<< QUrl("http://127.0.0.1:4569/some/path")
									<< (Pillow::HttpHeaderCollection())
									<< QByteArray("Some sent data")
									<< HttpRequestData()
									   .withMethod("PUT")
									   .withUri("/some/path").withPath("/some/path").withQueryString("").withFragment("")
									   .withHttpVersion("HTTP/1.1")
									   .withContent("Some sent data")
									   .withHeaders(Pillow::HttpHeaderCollection(baseExpectedHeaders)
													<< Pillow::HttpHeader("Content-Length", "14"));
		QTest::newRow("Large POST") << QByteArray("POST")
									<< QUrl("http://127.0.0.1:4569/some/large/path")
									<< (Pillow::HttpHeaderCollection())
									<< QByteArray(128 * 1024, '*')
									<< HttpRequestData()
									   .withMethod("POST")
									   .withUri("/some/large/path").withPath("/some/large/path").withQueryString("").withFragment("")
									   .withHttpVersion("HTTP/1.1")
									   .withContent(QByteArray(128 * 1024, '*'))
									   .withHeaders(Pillow::HttpHeaderCollection(baseExpectedHeaders)
													<< Pillow::HttpHeader("Content-Length", "131072"));
	}

	void should_send_valid_requests()
	{
		QFETCH(QByteArray, method);
		QFETCH(QUrl, url);
		QFETCH(Pillow::HttpHeaderCollection, headers);
		QFETCH(QByteArray, content);
		QFETCH(HttpRequestData, expectedRequestData);

		client->request(method, url, headers, content);

		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedRequests.size(), 1);
		QCOMPARE(server.receivedRequests.first(), expectedRequestData);
	}

	void should_add_host_header_to_requests()
	{
		client->get(QUrl("http://127.0.0.1:4569/"));
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedConnections.size(), 1);
		QCOMPARE(server.receivedConnections.last()->requestHeaderValue("Host"), QByteArray("127.0.0.1:4569"));
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());

		// Verify that connecting to another server sends an updated host header.
		TestServer otherServer;
		QVERIFY(otherServer.listen(QHostAddress::LocalHost, 4578));

		client->get(QUrl("http://127.0.0.1:4578/"));
		QVERIFY(otherServer.waitForRequest());
		QCOMPARE(otherServer.receivedConnections.size(), 1);
		QCOMPARE(otherServer.receivedConnections.last()->requestHeaderValue("Host"), QByteArray("127.0.0.1:4578"));
	}

	void should_be_pending_response_after_sending_request()
	{
		QVERIFY(!client->responsePending());
		client->get(testUrl());
		QVERIFY(client->responsePending());
	}

	void should_receive_response()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QVERIFY(client->responsePending());
		server.receivedConnections.first()->writeResponse();
		QVERIFY(waitForResponse());
		QVERIFY(!client->responsePending());
	}

	void should_parse_received_response_data()
	{
		qRegisterMetaType<Chunks>("Chunks");
		QTest::addColumn<int>("statusCode");
		QTest::addColumn<Pillow::HttpHeaderCollection>("headers");
		QTest::addColumn<Chunks>("contentChunks");
		QTest::addColumn<Pillow::HttpHeaderCollection>("expectedHeaders");

		QTest::newRow("Simple 200") << 200
									<< (Pillow::HttpHeaderCollection())
									<< (Chunks() << "Hello World")
									<< (Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Content-Length", "11") << Pillow::HttpHeader("Content-Type", "text/plain"));
		QTest::newRow("Many headers") << 200
									  << (Pillow::HttpHeaderCollection() << Pillow::HttpHeader("a", "b") << Pillow::HttpHeader("c", "d") << Pillow::HttpHeader("e", "f") << Pillow::HttpHeader("g", "hhhhhhhhhhhhhhhh"))
									  << (Chunks() << "Hello World")
									  << (Pillow::HttpHeaderCollection() << Pillow::HttpHeader("a", "b") << Pillow::HttpHeader("c", "d") << Pillow::HttpHeader("e", "f") << Pillow::HttpHeader("g", "hhhhhhhhhhhhhhhh") << Pillow::HttpHeader("Content-Length", "11") << Pillow::HttpHeader("Content-Type", "text/plain"));
		QTest::newRow("No Content") << 304
									<< (Pillow::HttpHeaderCollection())
									<< (Chunks())
									<< (Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Content-Length", "0"));
		QTest::newRow("Chunked Response") << 404
										  << (Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Transfer-Encoding", "Chunked") << Pillow::HttpHeader("Content-Type", "chunky/bacon"))
										  << (Chunks() << "Hello" << "the" << "world!")
										  << (Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Content-Type", "chunky/bacon") << Pillow::HttpHeader("Transfer-Encoding", "Chunked"));

		QTest::newRow("Delicious Stuff") << 400
											<< (Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Content-Type", "delicious-chocolate"))
											<< (Chunks() << "I like chocolate!")
											<< (Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Content-Length", "17") << Pillow::HttpHeader("Content-Type", "delicious-chocolate"));
	}

	void should_parse_received_response()
	{
		QFETCH(int, statusCode);
		QFETCH(Pillow::HttpHeaderCollection, headers);
		QFETCH(Chunks, contentChunks);
		QFETCH(Pillow::HttpHeaderCollection, expectedHeaders);
		QByteArray expectedContent;

		client->get(testUrl());
		QVERIFY(server.waitForRequest());

		if (contentChunks.isEmpty())
			server.receivedConnections.first()->writeResponse(statusCode, headers, QByteArray());
		else if (contentChunks.size() == 1)
		{
			server.receivedConnections.first()->writeResponse(statusCode, headers, contentChunks.first());
			expectedContent.append(contentChunks.first());
		}
		else
		{
			server.receivedConnections.first()->writeHeaders(statusCode, headers);
			foreach (const QByteArray& chunk, contentChunks)
			{
				server.receivedConnections.first()->writeContent(chunk);
				expectedContent.append(chunk);
			}
			server.receivedConnections.first()->endContent();
		}
		QVERIFY(waitForResponse());

		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(client->statusCode(), statusCode);
		QCOMPARE(client->headers(), expectedHeaders);
		QCOMPARE(client->content(), expectedContent);
	}

	void should_allow_sending_and_receiving_multiple_requests_one_after_another()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedRequests.size(), 1);
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedRequests.size(), 2);
		server.receivedConnections.last()->writeResponse(201);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 201);

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedRequests.size(), 3);
		server.receivedConnections.last()->writeResponse(202);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 202);
	}

	void should_clear_the_previous_response_when_sending_a_new_request()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedRequests.size(), 1);
		server.receivedConnections.last()->writeResponse(404, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Hello", "World"), "Hello World!");
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 404);
		QVERIFY(!client->headers().isEmpty());
		QVERIFY(!client->content().isEmpty());

		client->get(testUrl());
		QCOMPARE(client->statusCode(), 0);
		QVERIFY(client->headers().isEmpty());
		QVERIFY(client->content().isEmpty());
	}

	void should_not_send_another_request_while_response_is_pending_because_it_does_not_support_pipelining()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedRequests.size(), 1);
		QVERIFY(client->responsePending());

		QTest::ignoreMessage(QtWarningMsg, "Pillow::HttpClient::request: cannot send new request while another one is under way. Request pipelining is not supported.");
		client->get(testUrl());
		QCOMPARE(server.receivedRequests.size(), 1);

		server.receivedConnections.first()->writeResponse(200, Pillow::HttpHeaderCollection(), "Hello from Pillow");
		QVERIFY(waitForResponse());

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedRequests.size(), 2);
	}

	void should_allow_sending_subsequent_requests_to_different_hosts()
	{
		// First request, to main server.
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QVERIFY(server.receivedConnections.size() == 1);
		QPointer<QObject> firstSocket = server.receivedSockets.first();
		QVERIFY(firstSocket != 0);
		QVERIFY(client->responsePending());
		server.receivedConnections.first()->writeResponse(200);
		QVERIFY(waitForResponse());

		// Second request, to a different server.
		QVERIFY(server2.receivedConnections.size() == 0);
		client->get(QUrl("http://127.0.0.1:4570/"));
		QVERIFY(server2.waitForRequest());
		QVERIFY(server2.receivedConnections.size() == 1);
		QVERIFY(client->responsePending());
		server2.receivedConnections.first()->writeResponse(200);
		QVERIFY(waitForResponse());
		QVERIFY(firstSocket == 0); // The first connection should have been broken by the client connecting to a different host.

		// And back to the first server.
		QVERIFY(server.receivedConnections.size() == 1);
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QVERIFY(server.receivedConnections.size() == 2);
	}

	void should_report_network_errors()
	{
		client->get(QUrl("http://popopopopopopopopopopopopo.popo:64999/should/not/work"));
		QVERIFY(client->responsePending());
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NetworkError);
		QCOMPARE(client->statusCode(), 0);
		QVERIFY(!client->responsePending());

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.first()->writeResponse(200);
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
	}

	void should_report_error_and_close_connection_on_invalid_responses()
	{
		// Else, we may try to reuse a socket where our state is bad with the server.
		client->get(testUrl());
		QVERIFY(server.waitForRequest());

		QCOMPARE(server.receivedSockets.size(), 1);
		QCOMPARE(server.receivedSockets.last()->state(), QAbstractSocket::ConnectedState);
		QPointer<QTcpSocket> serverSideSocket = server.receivedSockets.last();
		qRegisterMetaType<QAbstractSocket::SocketState>("QAbstractSocket::SocketState");
		QSignalSpy serverSocketStateSpy(serverSideSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)));

		server.receivedConnections.last()->outputDevice()->write("=-=-=-=-=-FFFFFFFUUUUUUUUUUU=-=-=-=-=-=!\r\n");
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::ResponseInvalidError);

		QVERIFY(waitFor([&]{ return serverSideSocket == 0; }));
		QVERIFY(serverSideSocket == 0);
		QVERIFY(!serverSocketStateSpy.isEmpty());
		QCOMPARE(serverSocketStateSpy.last().first().value<QAbstractSocket::SocketState>(), QAbstractSocket::UnconnectedState);
	}

	void should_clear_a_previous_error_when_sending_a_new_request()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->outputDevice()->write("=-=-=-=-=-FFFFFFFUUUUUUUUUUU=-=-=-=-=-=!\r\n");
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::ResponseInvalidError);

		client->get(testUrl());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse();
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
	}

	void should_report_error_if_server_closes_connection_early()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->close();
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::RemoteHostClosedError);

		client->get(testUrl());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse();
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedSockets.last()->write("HTTP/1.1 200 OK\r\n");
		server.receivedSockets.last()->flush();
		server.receivedConnections.last()->close();
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::RemoteHostClosedError);

		client->get(testUrl());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(201);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 201);
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedSockets.last()->write("HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\nhello world"); // Missing one byte in the content!
		server.receivedSockets.last()->flush();
		QTest::qWait(50);
		server.receivedConnections.last()->close();
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::RemoteHostClosedError);
	}

	void should_not_report_error_if_server_closes_connection_after_responding()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(201);
		server.receivedConnections.last()->close();
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 201);
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
	}

	void should_silently_ignore_unexpected_data_received_while_not_waiting_for_response_and_close_connection()
	{
		// Example where server returns an extra, valid HTTP response, such as a
		// server indicating a timeout on a keep-alive connection.
		client->get(testUrl()); QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(202); QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 202);

		QPointer<QTcpSocket> socket = server.receivedSockets.last();
		socket->write("HTTP/1.0 400 Bad Request\r\nConnection: close\r\n\r\nThis connection was inactive for too long!");

		waitFor([&]{ return socket == 0; }); // The client should close the socket, which will close and delete it on the server.
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(client->statusCode(), 202);

		// Example where server returns extra junk.
		client->get(testUrl()); QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(202); QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 202);

		socket = server.receivedSockets.last();
		socket->write("=-=-=-=-=-FFFFFFFUUUUUUUUUUUU!");

		waitFor([&]{ return socket == 0; }); // The client should close the socket, which will close and delete it on the server.
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(client->statusCode(), 202);

	}

	void should_emit_finished_after_receiving_response()
	{
		QSignalSpy finishedSpy(client, SIGNAL(finished()));

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.first()->writeResponse(202);
		QVERIFY(waitForResponse());

		QCOMPARE(client->statusCode(), 202);
		QCOMPARE(finishedSpy.size(), 1);
	}

	void should_emit_finished_after_encountering_an_error()
	{
		QSignalSpy finishedSpy(client, SIGNAL(finished()));

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.first()->close();
		QVERIFY(waitForResponse());

		QCOMPARE(client->statusCode(), 0);
		QCOMPARE(finishedSpy.size(), 1);
	}

	void should_emit_finished_when_aborted()
	{
		QSignalSpy finishedSpy(client, SIGNAL(finished()));

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		client->abort();
		QVERIFY(waitForResponse());

		QCOMPARE(client->statusCode(), 0);
		QCOMPARE(finishedSpy.size(), 1);
	}

	void should_be_abortable()
	{
		client->abort(); // Should not do anything besides a warning.
		QVERIFY(client->error() == Pillow::HttpClient::NoError);

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		client->abort();
		server.receivedConnections.last()->writeResponse(202);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 0);
		QCOMPARE(client->error(), Pillow::HttpClient::AbortedError);

		// And recover from it.
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(202);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 202);
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
	}

	void should_close_connection_if_aborted_while_waiting_for_response()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QVERIFY(server.receivedSockets.last()->state() == QAbstractSocket::ConnectedState);
		QPointer<QTcpSocket> socket = server.receivedSockets.last();
		QVERIFY(socket != 0);
		client->abort();
		QVERIFY(waitForResponse());
		QVERIFY(waitFor([=]{ return socket == 0; })); // The socket should get closed and deleted.
	}

	void should_close_connection_and_not_discard_previous_response_if_aborted_while_not_waiting_for_response()
	{
		QSignalSpy finishedSpy(client, SIGNAL(finished()));

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QPointer<QTcpSocket> socket = server.receivedSockets.last();
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());

		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(finishedSpy.size(), 1);
		QVERIFY(socket != 0);
		QVERIFY(!client->responsePending());

		client->abort();
		QVERIFY(waitFor([=]{ return socket == 0; })); // The socket should get closed and deleted.

		// Should not have modified the results from the previous response, nor emitted finished() again.
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(finishedSpy.size(), 1);
	}

	void should_reuse_existing_connection_to_same_host_and_port()
	{
		client->get(testUrl()); QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200); QVERIFY(waitForResponse());

		client->get(testUrl()); QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(201); QVERIFY(waitForResponse());

		client->get(testUrl()); QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(202); QVERIFY(waitForResponse());

		QCOMPARE(server.receivedSockets.size(), 3);
		QVERIFY(server.receivedSockets.at(0) == server.receivedSockets.at(1) && server.receivedSockets.at(1) == server.receivedSockets.at(2));
	}

	void should_allow_consuming_partial_responses_to_support_streaming()
	{
		// With chunked transfer encoding.
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeHeaders(201, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Transfer-Encoding", "chunked"));
		server.receivedConnections.last()->writeContent("hello");
		QVERIFY(waitForContentReadyRead());
		QCOMPARE(client->statusCode(), 201);
		QCOMPARE(client->content(), QByteArray("hello"));
		QCOMPARE(client->consumeContent(), QByteArray("hello"));
		QCOMPARE(client->content(), QByteArray());
		QCOMPARE(client->consumeContent(), QByteArray());

		server.receivedConnections.last()->writeContent(" ");
		QVERIFY(waitForContentReadyRead());
		QCOMPARE(client->consumeContent(), QByteArray(" "));

		server.receivedConnections.last()->writeContent("world!");
		QVERIFY(waitForContentReadyRead());
		QCOMPARE(client->consumeContent(), QByteArray("world!"));

		server.receivedConnections.last()->endContent();
		QVERIFY(waitForResponse());

		QCOMPARE(client->content(), QByteArray());
		QCOMPARE(client->consumeContent(), QByteArray());

		// Without chunked transfer encoding.
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeHeaders(202);
		server.receivedConnections.last()->writeContent("hello");
		QVERIFY(waitForContentReadyRead());
		QCOMPARE(client->statusCode(), 202);
		QCOMPARE(client->content(), QByteArray("hello"));
		QCOMPARE(client->consumeContent(), QByteArray("hello"));
		QCOMPARE(client->content(), QByteArray());
		QCOMPARE(client->consumeContent(), QByteArray());

		server.receivedConnections.last()->writeContent(" world!");
		QVERIFY(waitForContentReadyRead());
		QCOMPARE(client->consumeContent(), QByteArray(" world!"));

		server.receivedConnections.last()->endContent();
		QVERIFY(waitForResponse());

		QCOMPARE(client->content(), QByteArray());
		QCOMPARE(client->consumeContent(), QByteArray());
	}

	void should_ignore_100_continue_responses()
	{
		QSignalSpy headersCompleteSpy(client, SIGNAL(headersCompleted()));
		QSignalSpy finishedSpy(client, SIGNAL(finished()));

		client->post(testUrl(), Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Expect", "100-continue"), "post data");
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(201, Pillow::HttpHeaderCollection(), "content");
		QVERIFY(waitForResponse(500));
		QCOMPARE(client->statusCode(), 201);
		QCOMPARE(client->content(), QByteArray("content"));
		QCOMPARE(headersCompleteSpy.size(), 1);
		QCOMPARE(finishedSpy.size(), 1);
	}

	void should_emit_headersCompleted_when_headers_have_been_received()
	{
		client->post(testUrl(), Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Expect", "100-continue"), "post data");
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeHeaders(200, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("First", "FirstValue") << Pillow::HttpHeader("Second", "SecondValue") << Pillow::HttpHeader("Content-Length", "12"));

		QCOMPARE(client->statusCode(), 0);
		QCOMPARE(client->headers(), Pillow::HttpHeaderCollection());

		QSignalSpy finishedSpy(client, SIGNAL(finished()));
		QVERIFY(waitForSignal(client, SIGNAL(headersCompleted())));
		QVERIFY(finishedSpy.isEmpty()); // Should not have finished the request yet.

		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->headers(), Pillow::HttpHeaderCollection() << Pillow::HttpHeader("First", "FirstValue") << Pillow::HttpHeader("Second", "SecondValue") << Pillow::HttpHeader("Content-Length", "12") << Pillow::HttpHeader("Content-Type", "text/plain"));
		QCOMPARE(client->content(), QByteArray());

		server.receivedConnections.last()->writeContent("Some content");
		QVERIFY(waitForSignal(client, SIGNAL(finished())));
		QCOMPARE(finishedSpy.size(), 1);

		QCOMPARE(client->content(), QByteArray("Some content"));
	}

	void should_report_error_given_an_unsupported_request()
	{
		QSKIP("Not implemented", SkipAll);
	}

	void should_support_head_requests()
	{
		client->head(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200, Pillow::HttpHeaderCollection(), "12345");
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->content(), QByteArray());

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(201, Pillow::HttpHeaderCollection(), "abcde");
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(client->statusCode(), 201);
		QCOMPARE(client->content(), QByteArray("abcde"));
	}

	void should_have_a_configurable_receive_buffer()
	{
		QSKIP("Not implemented", SkipAll);
	}

	void should_allow_specifying_connection_keepAliveTimeout()
	{
		QCOMPARE(client->keepAliveTimeout(), -1); // -1 Means infinite keep-alive.

		// Warning: timing-sensitive test.
		client->setKeepAliveTimeout(50);

		client->get(testUrl()); QVERIFY(server.waitForRequest());
		QPointer<QObject> socket = server.receivedSockets.last();
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse()); QCOMPARE(client->error(), Pillow::HttpClient::NoError);

		QTest::qWait(60); // Wait more than the keep alive timeout.

		client->get(testUrl()); QVERIFY(server.waitForRequest());
		QPointer<QObject> socket2 = server.receivedSockets.last();
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse()); QCOMPARE(client->error(), Pillow::HttpClient::NoError);

		QVERIFY(socket == 0); // It will have closed the initial socket and used a new one.
		QVERIFY(socket2 != 0);

		QTest::qWait(1); // Wait less than the keep alive timeout.

		client->get(testUrl()); QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse()); QCOMPARE(client->error(), Pillow::HttpClient::NoError);

		QVERIFY(socket2 != 0); // It will have reused the still-good connection.
	}

	void should_disable_keep_alive_when_keepAliveTimeout_is_zero()
	{
		client->setKeepAliveTimeout(0);

		client->get(testUrl()); QVERIFY(server.waitForRequest());

		QSignalSpy connectionClosedSpy(server.receivedConnections.last(), SIGNAL(closed(Pillow::HttpConnection*)));
		QPointer<QObject> socket = server.receivedSockets.last();

		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse()); QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QVERIFY(waitFor([&]{ return connectionClosedSpy.size() == 1; }));

		client->get(testUrl()); QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse()); QCOMPARE(client->error(), Pillow::HttpClient::NoError);

		QVERIFY(socket == 0);
	}

	void should_break_existing_connection_when_keepAliveTimeout_exceeded()
	{
		// Warning: timing-sensitive test.

		client->get(testUrl()); QVERIFY(server.waitForRequest());
		QPointer<QObject> socket = server.receivedSockets.last();
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse()); QCOMPARE(client->error(), Pillow::HttpClient::NoError);

		client->get(testUrl()); QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse()); QCOMPARE(client->error(), Pillow::HttpClient::NoError);

		QVERIFY(socket != 0);
		QVERIFY(server.receivedSockets.at(0) == server.receivedSockets.at(1)); // Still good.

		QTest::qWait(15); // But wait, there's more!
		client->setKeepAliveTimeout(10); // Un-oh, we're already past that!

		client->get(testUrl()); QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse()); QCOMPARE(client->error(), Pillow::HttpClient::NoError);

		QVERIFY(socket == 0); // It will have broken the initial connection.
	}

	void should_be_abortable_from_within_headersCompleted()
	{
		QSignalSpy contentReadyReadSpy(client, SIGNAL(contentReadyRead()));
		connect(client, SIGNAL(headersCompleted()), this, SLOT(abortSender()));

		client->get(testUrl()); QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeHeaders(201);
		server.receivedConnections.last()->writeContent("Hello");
		QVERIFY(waitForResponse());
		QCOMPARE(contentReadyReadSpy.size(), 0); // It should be aborted before emitting contentReadyRead.
		QCOMPARE(client->error(), Pillow::HttpClient::AbortedError);
		QCOMPARE(client->statusCode(), 201);

		// Recover
		disconnect(client, SIGNAL(headersCompleted()), this, SLOT(abortSender()));
		client->get(testUrl()); QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(404, Pillow::HttpHeaderCollection(), "Test");
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 404);
		QCOMPARE(client->content(), QByteArray("Test"));
		QCOMPARE(contentReadyReadSpy.size(), 1);

		// Wild case: abort previous and send new request from within slot.
		connect(client, SIGNAL(headersCompleted()), this, SLOT(abortSender()));
		connect(client, SIGNAL(headersCompleted()), this, SLOT(sendRequest()));
		contentReadyReadSpy.clear();

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedConnections.size(), 3);
		server.receivedConnections.last()->writeResponse(200, Pillow::HttpHeaderCollection(), "Bla bla");
		QVERIFY(waitForSignal(client, SIGNAL(finished())));
		QCOMPARE(contentReadyReadSpy.size(), 0); // It should be aborted before emitting contentReadyRead.
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QVERIFY(client->responsePending());

		// Recover
		disconnect(client, SIGNAL(headersCompleted()), this, SLOT(abortSender()));
		disconnect(client, SIGNAL(headersCompleted()), this, SLOT(sendRequest()));
		QVERIFY(waitFor([&]{ return server.receivedConnections.size() == 4; })); // At this point, we will or have already received the new request.
		server.receivedConnections.last()->writeResponse(201, Pillow::HttpHeaderCollection(), "Testing 123");
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(client->statusCode(), 201);
		QCOMPARE(client->content(), QByteArray("Testing 123"));
		QVERIFY(!client->responsePending());
	}

	void should_be_abortable_from_within_contentReadyRead()
	{
		connect(client, SIGNAL(contentReadyRead()), this, SLOT(abortSender()));

		client->get(testUrl()); QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeHeaders(201);
		server.receivedConnections.last()->writeContent("Hello");
		server.receivedConnections.last()->writeContent("World");
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::AbortedError);

		// Recover
		disconnect(client, SIGNAL(contentReadyRead()), this, SLOT(abortSender()));
		client->get(testUrl()); QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(404, Pillow::HttpHeaderCollection(), "Test");
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 404);
		QCOMPARE(client->content(), QByteArray("Test"));

		// Wild case: abort previous and send new request from within slot.
		connect(client, SIGNAL(contentReadyRead()), this, SLOT(abortSender()));
		connect(client, SIGNAL(contentReadyRead()), this, SLOT(sendRequest()));

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedConnections.size(), 3);
		server.receivedConnections.last()->writeHeaders(200);
		server.receivedConnections.last()->writeContent("Bla bla");
		QVERIFY(waitForSignal(client, SIGNAL(finished())));
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QVERIFY(client->responsePending());

		// Recover
		disconnect(client, SIGNAL(contentReadyRead()), this, SLOT(abortSender()));
		disconnect(client, SIGNAL(contentReadyRead()), this, SLOT(sendRequest()));
		QVERIFY(waitFor([&]{ return server.receivedConnections.size() == 4; })); // At this point, we will or have already received the new request.
		server.receivedConnections.last()->writeResponse(201, Pillow::HttpHeaderCollection(), "Testing 123");
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(client->statusCode(), 201);
		QCOMPARE(client->content(), QByteArray("Testing 123"));
		QVERIFY(!client->responsePending());
	}

	void should_use_port_80_by_default()
	{
		// How to test this?
	}

	void should_detect_redirects()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QVERIFY(!client->redirected());
		QCOMPARE(client->redirectionLocation(), QByteArray());

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(301, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Location", "http://new-location.example.org/"));
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 301);
		QVERIFY(client->redirected());
		QCOMPARE(client->redirectionLocation(), QByteArray("http://new-location.example.org/"));

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(201);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 201);
		QVERIFY(!client->redirected());
		QCOMPARE(client->redirectionLocation(), QByteArray());

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(302, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Location", "http://another-location.example.org/and/path"));
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 302);
		QVERIFY(client->redirected());
		QCOMPARE(client->redirectionLocation(), QByteArray("http://another-location.example.org/and/path"));
	}

	void should_allow_following_redirections()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(301, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Location", "http://127.0.0.1:4569/other/path"));
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 301);
		QVERIFY(client->redirected());
		QCOMPARE(client->redirectionLocation(), QByteArray("http://127.0.0.1:4569/other/path"));

		client->followRedirection();

		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedConnections.last()->requestPath(), QByteArray("/other/path"));
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QVERIFY(!client->redirected());
		QCOMPARE(client->redirectionLocation(), QByteArray());

		// Trying to follow redirection when none happened should do nothing.
		QTest::ignoreMessage(QtWarningMsg, "Pillow::HttpClient::followRedirection(): no redirection to follow.");
		client->followRedirection();
	}

	void should_support_gzip_content_encoding()
	{
		QByteArray gzippedData;
		{
			QFile gzipFile(":/test.gz");
			QVERIFY(gzipFile.open(QFile::ReadOnly));
			gzippedData = gzipFile.readAll();
			QCOMPARE(gzippedData.size(), 49);
		}

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Content-Encoding", "gzip"), gzippedData);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->content(), QByteArray("1234567890123456789012345678901234567890"));

		// GZip content-encoding with empty content.
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Content-Encoding", "gzip"), "");
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->content(), QByteArray());

		// Gzip content sent in multiple chunks
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeHeaders(200, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Content-Encoding", "gzip") << Pillow::HttpHeader("Transfer-Encoding", "Chunked"));
		server.receivedConnections.last()->writeContent(gzippedData.left(15));
		QTest::qWait(1);
		server.receivedConnections.last()->writeContent(gzippedData.mid(15, 15));
		QTest::qWait(1);
		server.receivedConnections.last()->writeContent(gzippedData.mid(30));
		server.receivedConnections.last()->endContent();
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->content(), QByteArray("1234567890123456789012345678901234567890"));
	}

	void should_pass_bad_gzipped_content_through()
	{
		QTest::ignoreMessage(QtWarningMsg, "Pillow::GunzipContentTransformer::transform: error inflating input stream passing original content through.");
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Content-Encoding", "gzip"), "Definitely not gzipped data");
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->content(), QByteArray("Definitely not gzipped data"));
	}
};
PILLOW_TEST_DECLARE(HttpClientTest)

class HttpRequestWriterTest : public QObject
{
	Q_OBJECT
	QBuffer *buffer;

private slots:
	void init()
	{
		buffer = new QBuffer(this);
		buffer->open(QBuffer::ReadWrite);
	}

	void cleanup()
	{
		delete buffer; buffer = 0;
	}

	QByteArray readAll()
	{
		QByteArray data = buffer->data();
		buffer->seek(0);
		return data;
	}

private slots:
	void test_initial_state()
	{
		Pillow::HttpRequestWriter w;
		QVERIFY(w.device() == 0);
		w.setDevice(buffer);
		QVERIFY(w.device() == buffer);
	}

	void test_write_get()
	{
		Pillow::HttpRequestWriter w; w.setDevice(buffer);

		w.get("/some/path", Pillow::HttpHeaderCollection());
		QCOMPARE(readAll(), QByteArray("GET /some/path HTTP/1.1\r\n\r\n"));

		w.get("/other/cool%20path", Pillow::HttpHeaderCollection() <<
			  Pillow::HttpHeader("My-Header", "Is-Cool") <<
			  Pillow::HttpHeader("X-And-Another", "Is-Better"));
		QCOMPARE(readAll(), QByteArray("GET /other/cool%20path HTTP/1.1\r\nMy-Header: Is-Cool\r\nX-And-Another: Is-Better\r\n\r\n"));
	}

	void test_write_head()
	{
		Pillow::HttpRequestWriter w; w.setDevice(buffer);

		w.head("/some/path", Pillow::HttpHeaderCollection());
		QCOMPARE(readAll(), QByteArray("HEAD /some/path HTTP/1.1\r\n\r\n"));

		w.head("/other/cool%20path", Pillow::HttpHeaderCollection() <<
			  Pillow::HttpHeader("My-Header", "Is-Cool") <<
			  Pillow::HttpHeader("X-And-Another", "Is-Better"));
		QCOMPARE(readAll(), QByteArray("HEAD /other/cool%20path HTTP/1.1\r\nMy-Header: Is-Cool\r\nX-And-Another: Is-Better\r\n\r\n"));
	}

	void test_write_post()
	{
		Pillow::HttpRequestWriter w; w.setDevice(buffer);

		w.post("/some/path.txt", Pillow::HttpHeaderCollection(), QByteArray());
		QCOMPARE(readAll(), QByteArray("POST /some/path.txt HTTP/1.1\r\n\r\n"));

		w.post("/other/path.txt", Pillow::HttpHeaderCollection() << Pillow::HttpHeader("One", "Header"), QByteArray("Some Data"));
		QCOMPARE(readAll(), QByteArray("POST /other/path.txt HTTP/1.1\r\nOne: Header\r\nContent-Length: 9\r\n\r\nSome Data"));
	}
	void test_write_put()
	{
		Pillow::HttpRequestWriter w; w.setDevice(buffer);

		w.put("/some/path.txt", Pillow::HttpHeaderCollection(), QByteArray());
		QCOMPARE(readAll(), QByteArray("PUT /some/path.txt HTTP/1.1\r\n\r\n"));

		w.put("/other/path.txt", Pillow::HttpHeaderCollection() << Pillow::HttpHeader("One", "Header"), QByteArray("Some Data"));
		QCOMPARE(readAll(), QByteArray("PUT /other/path.txt HTTP/1.1\r\nOne: Header\r\nContent-Length: 9\r\n\r\nSome Data"));
	}

	void test_write_deleteResource()
	{
		Pillow::HttpRequestWriter w; w.setDevice(buffer);

		w.deleteResource("/some/path", Pillow::HttpHeaderCollection());
		QCOMPARE(readAll(), QByteArray("DELETE /some/path HTTP/1.1\r\n\r\n"));

		w.deleteResource("/other/cool%20path", Pillow::HttpHeaderCollection() <<
			  Pillow::HttpHeader("My-Header", "Is-Cool") <<
			  Pillow::HttpHeader("X-And-Another", "Is-Better"));
		QCOMPARE(readAll(), QByteArray("DELETE /other/cool%20path HTTP/1.1\r\nMy-Header: Is-Cool\r\nX-And-Another: Is-Better\r\n\r\n"));
	}
};
PILLOW_TEST_DECLARE(HttpRequestWriterTest)

//
// Test data taken from joyent/http_parser's test.c
//
#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0

#define MAX_HEADERS 13
#define MAX_ELEMENT_SIZE 500

#define MIN(a,b) ((a) < (b) ? (a) : (b))

struct message {
  const char *name; // for debugging purposes
  enum http_parser_type type;
  const char *raw;
  int should_keep_alive;
  int message_complete_on_eof;
  unsigned short http_major;
  unsigned short http_minor;
  unsigned short status_code;
  int num_headers;
  char headers [MAX_HEADERS][2][MAX_ELEMENT_SIZE];
  size_t body_size;
  char body[MAX_ELEMENT_SIZE];


//  enum http_method method;
//  char request_path[MAX_ELEMENT_SIZE];
//  char request_url[MAX_ELEMENT_SIZE];
//  char fragment[MAX_ELEMENT_SIZE];
//  char query_string[MAX_ELEMENT_SIZE];
//  uint16_t port;
//  enum { NONE=0, FIELD, VALUE } last_header_element;

//  const char *upgrade; // upgraded body


//  int message_begin_cb_called;
//  int headers_complete_cb_called;
//  int message_complete_cb_called;
};
Q_DECLARE_METATYPE(message)

const struct message responses[] =
#define GOOGLE_301 0
{ {"google 301"
  ,HTTP_RESPONSE
  ,"HTTP/1.1 301 Moved Permanently\r\n"
		 "Location: http://www.google.com/\r\n"
		 "Content-Type: text/html; charset=UTF-8\r\n"
		 "Date: Sun, 26 Apr 2009 11:11:49 GMT\r\n"
		 "Expires: Tue, 26 May 2009 11:11:49 GMT\r\n"
		 "X-$PrototypeBI-Version: 1.6.0.3\r\n" /* $ char in header field */
		 "Cache-Control: public, max-age=2592000\r\n"
		 "Server: gws\r\n"
		 "Content-Length:  219  \r\n"
		 "\r\n"
		 "<HTML><HEAD><meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\">\n"
		 "<TITLE>301 Moved</TITLE></HEAD><BODY>\n"
		 "<H1>301 Moved</H1>\n"
		 "The document has moved\n"
		 "<A HREF=\"http://www.google.com/\">here</A>.\r\n"
		 "</BODY></HTML>\r\n"
  ,TRUE
  ,FALSE
  ,1
  ,1
  ,301
  ,8
  ,
	{ { "Location", "http://www.google.com/" }
	, { "Content-Type", "text/html; charset=UTF-8" }
	, { "Date", "Sun, 26 Apr 2009 11:11:49 GMT" }
	, { "Expires", "Tue, 26 May 2009 11:11:49 GMT" }
	, { "X-$PrototypeBI-Version", "1.6.0.3" }
	, { "Cache-Control", "public, max-age=2592000" }
	, { "Server", "gws" }
	, { "Content-Length", "219  " }
	}
  ,0
  ,"<HTML><HEAD><meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\">\n"
		  "<TITLE>301 Moved</TITLE></HEAD><BODY>\n"
		  "<H1>301 Moved</H1>\n"
		  "The document has moved\n"
		  "<A HREF=\"http://www.google.com/\">here</A>.\r\n"
		  "</BODY></HTML>\r\n"
  }

#define NO_CONTENT_LENGTH_RESPONSE 1
/* The client should wait for the server's EOF. That is, when content-length
 * is not specified, and "Connection: close", the end of body is specified
 * by the EOF.
 * Compare with APACHEBENCH_GET
 */
, {"no content-length response"
  ,HTTP_RESPONSE
  ,"HTTP/1.1 200 OK\r\n"
		 "Date: Tue, 04 Aug 2009 07:59:32 GMT\r\n"
		 "Server: Apache\r\n"
		 "X-Powered-By: Servlet/2.5 JSP/2.1\r\n"
		 "Content-Type: text/xml; charset=utf-8\r\n"
		 "Connection: close\r\n"
		 "\r\n"
		 "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		 "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\">\n"
		 "  <SOAP-ENV:Body>\n"
		 "    <SOAP-ENV:Fault>\n"
		 "       <faultcode>SOAP-ENV:Client</faultcode>\n"
		 "       <faultstring>Client Error</faultstring>\n"
		 "    </SOAP-ENV:Fault>\n"
		 "  </SOAP-ENV:Body>\n"
		 "</SOAP-ENV:Envelope>"
  ,FALSE
  ,TRUE
  ,1
  ,1
  ,200
  ,5
  ,
	{ { "Date", "Tue, 04 Aug 2009 07:59:32 GMT" }
	, { "Server", "Apache" }
	, { "X-Powered-By", "Servlet/2.5 JSP/2.1" }
	, { "Content-Type", "text/xml; charset=utf-8" }
	, { "Connection", "close" }
	}
   ,0
  ,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		  "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\">\n"
		  "  <SOAP-ENV:Body>\n"
		  "    <SOAP-ENV:Fault>\n"
		  "       <faultcode>SOAP-ENV:Client</faultcode>\n"
		  "       <faultstring>Client Error</faultstring>\n"
		  "    </SOAP-ENV:Fault>\n"
		  "  </SOAP-ENV:Body>\n"
		  "</SOAP-ENV:Envelope>"
  }

#define NO_HEADERS_NO_BODY_404 2
, {"404 no headers no body"
  ,HTTP_RESPONSE
  ,"HTTP/1.1 404 Not Found\r\n\r\n"
  ,FALSE
  ,TRUE
  ,1
  ,1
  ,404
  ,0
  , {}
  ,0
  ,""
  }

#define NO_REASON_PHRASE 3
, {"301 no response phrase"
  ,HTTP_RESPONSE
  ,"HTTP/1.1 301\r\n\r\n"
  ,FALSE
  ,TRUE
  ,1
  ,1
  ,301
  ,0
  , {}
  ,0
  ,""
  }

#define TRAILING_SPACE_ON_CHUNKED_BODY 4
, {"200 trailing space on chunked body"
  ,HTTP_RESPONSE
  ,"HTTP/1.1 200 OK\r\n"
		 "Content-Type: text/plain\r\n"
		 "Transfer-Encoding: chunked\r\n"
		 "\r\n"
		 "25  \r\n"
		 "This is the data in the first chunk\r\n"
		 "\r\n"
		 "1C\r\n"
		 "and this is the second one\r\n"
		 "\r\n"
		 "0  \r\n"
		 "\r\n"
  ,TRUE
  ,FALSE
  ,1
  ,1
  ,200
  ,2
  ,
	{ {"Content-Type", "text/plain" }
	, {"Transfer-Encoding", "chunked" }
	}
  ,37+28
  ,
		 "This is the data in the first chunk\r\n"
		 "and this is the second one\r\n"

  }

#define NO_CARRIAGE_RET 5
, {"no carriage ret"
  ,HTTP_RESPONSE
  ,"HTTP/1.1 200 OK\n"
		 "Content-Type: text/html; charset=utf-8\n"
		 "Connection: close\n"
		 "\n"
		 "these headers are from http://news.ycombinator.com/"
  ,FALSE
  ,TRUE
  ,1
  ,1
  ,200
  ,2
  ,
	{ {"Content-Type", "text/html; charset=utf-8" }
	, {"Connection", "close" }
	}
  ,0
  ,"these headers are from http://news.ycombinator.com/"
  }

#define PROXY_CONNECTION 6
, {"proxy connection"
  ,HTTP_RESPONSE
  ,"HTTP/1.1 200 OK\r\n"
		 "Content-Type: text/html; charset=UTF-8\r\n"
		 "Content-Length: 11\r\n"
		 "Proxy-Connection: close\r\n"
		 "Date: Thu, 31 Dec 2009 20:55:48 +0000\r\n"
		 "\r\n"
		 "hello world"
  ,FALSE
  ,FALSE
  ,1
  ,1
  ,200
  ,4
  ,
	{ {"Content-Type", "text/html; charset=UTF-8" }
	, {"Content-Length", "11" }
	, {"Proxy-Connection", "close" }
	, {"Date", "Thu, 31 Dec 2009 20:55:48 +0000"}
	}
  ,11
  ,"hello world"
  }

#define UNDERSTORE_HEADER_KEY 7
  // shown by
  // curl -o /dev/null -v "http://ad.doubleclick.net/pfadx/DARTSHELLCONFIGXML;dcmt=text/xml;"
, {"underscore header key"
  ,HTTP_RESPONSE
  ,"HTTP/1.1 200 OK\r\n"
		 "Server: DCLK-AdSvr\r\n"
		 "Content-Type: text/xml\r\n"
		 "Content-Length: 0\r\n"
		 "DCLK_imp: v7;x;114750856;0-0;0;17820020;0/0;21603567/21621457/1;;~okv=;dcmt=text/xml;;~cs=o\r\n\r\n"
  ,TRUE
  ,FALSE
  ,1
  ,1
  ,200
  ,4
  ,
	{ {"Server", "DCLK-AdSvr" }
	, {"Content-Type", "text/xml" }
	, {"Content-Length", "0" }
	, {"DCLK_imp", "v7;x;114750856;0-0;0;17820020;0/0;21603567/21621457/1;;~okv=;dcmt=text/xml;;~cs=o" }
	}
  ,0
  ,""
  }

#define BONJOUR_MADAME_FR 8
/* The client should not merge two headers fields when the first one doesn't
 * have a value.
 */
, {"bonjourmadame.fr"
  ,HTTP_RESPONSE
  ,"HTTP/1.0 301 Moved Permanently\r\n"
		 "Date: Thu, 03 Jun 2010 09:56:32 GMT\r\n"
		 "Server: Apache/2.2.3 (Red Hat)\r\n"
		 "Cache-Control: public\r\n"
		 "Pragma: \r\n"
		 "Location: http://www.bonjourmadame.fr/\r\n"
		 "Vary: Accept-Encoding\r\n"
		 "Content-Length: 0\r\n"
		 "Content-Type: text/html; charset=UTF-8\r\n"
		 "Connection: keep-alive\r\n"
		 "\r\n"
  ,TRUE
  ,FALSE
  ,1
  ,0
  ,301
  ,9
  ,
	{ { "Date", "Thu, 03 Jun 2010 09:56:32 GMT" }
	, { "Server", "Apache/2.2.3 (Red Hat)" }
	, { "Cache-Control", "public" }
	, { "Pragma", "" }
	, { "Location", "http://www.bonjourmadame.fr/" }
	, { "Vary",  "Accept-Encoding" }
	, { "Content-Length", "0" }
	, { "Content-Type", "text/html; charset=UTF-8" }
	, { "Connection", "keep-alive" }
	}
  ,0
  ,""
  }

#define RES_FIELD_UNDERSCORE 9
/* Should handle spaces in header fields */
, {"field underscore"
  ,HTTP_RESPONSE
  ,"HTTP/1.1 200 OK\r\n"
		 "Date: Tue, 28 Sep 2010 01:14:13 GMT\r\n"
		 "Server: Apache\r\n"
		 "Cache-Control: no-cache, must-revalidate\r\n"
		 "Expires: Mon, 26 Jul 1997 05:00:00 GMT\r\n"
		 ".et-Cookie: PlaxoCS=1274804622353690521; path=/; domain=.plaxo.com\r\n"
		 "Vary: Accept-Encoding\r\n"
		 "_eep-Alive: timeout=45\r\n" /* semantic value ignored */
		 "_onnection: Keep-Alive\r\n" /* semantic value ignored */
		 "Transfer-Encoding: chunked\r\n"
		 "Content-Type: text/html\r\n"
		 "Connection: close\r\n"
		 "\r\n"
		 "0\r\n\r\n"
  ,FALSE
  ,FALSE
  ,1
  ,1
  ,200
  ,11
  ,
	{ { "Date", "Tue, 28 Sep 2010 01:14:13 GMT" }
	, { "Server", "Apache" }
	, { "Cache-Control", "no-cache, must-revalidate" }
	, { "Expires", "Mon, 26 Jul 1997 05:00:00 GMT" }
	, { ".et-Cookie", "PlaxoCS=1274804622353690521; path=/; domain=.plaxo.com" }
	, { "Vary", "Accept-Encoding" }
	, { "_eep-Alive", "timeout=45" }
	, { "_onnection", "Keep-Alive" }
	, { "Transfer-Encoding", "chunked" }
	, { "Content-Type", "text/html" }
	, { "Connection", "close" }
	}
   ,0
  ,""
  }

#define NON_ASCII_IN_STATUS_LINE 10
/* Should handle non-ASCII in status line */
, {"non-ASCII in status line"
  ,HTTP_RESPONSE
  ,"HTTP/1.1 500 Oriëntatieprobleem\r\n"
		 "Date: Fri, 5 Nov 2010 23:07:12 GMT+2\r\n"
		 "Content-Length: 0\r\n"
		 "Connection: close\r\n"
		 "\r\n"
  ,FALSE
  ,FALSE
  ,1
  ,1
  ,500
  ,3
  ,
	{ { "Date", "Fri, 5 Nov 2010 23:07:12 GMT+2" }
	, { "Content-Length", "0" }
	, { "Connection", "close" }
	}
   ,0
  ,""
  }

#define HTTP_VERSION_0_9 11
/* Should handle HTTP/0.9 */
, {"http version 0.9"
  ,HTTP_RESPONSE
  ,"HTTP/0.9 200 OK\r\n"
		 "\r\n"
  ,FALSE
  ,TRUE
  ,0
  ,9
  ,200
  ,0
  ,
	{}
   ,0
  ,""
  }

#define NO_CONTENT_LENGTH_NO_TRANSFER_ENCODING_RESPONSE 12
/* The client should wait for the server's EOF. That is, when neither
 * content-length nor transfer-encoding is specified, the end of body
 * is specified by the EOF.
 */
, {"neither content-length nor transfer-encoding response"
  ,HTTP_RESPONSE
  ,"HTTP/1.1 200 OK\r\n"
		 "Content-Type: text/plain\r\n"
		 "\r\n"
		 "hello world"
  ,FALSE
  ,TRUE
  ,1
  ,1
  ,200
  ,1
  ,
	{ { "Content-Type", "text/plain" }
	}
   ,11
  ,"hello world"
  }

#define NO_BODY_HTTP10_KA_200 13
, {"HTTP/1.0 with keep-alive and EOF-terminated 200 status"
  ,HTTP_RESPONSE
  ,"HTTP/1.0 200 OK\r\n"
		 "Connection: keep-alive\r\n"
		 "\r\n"
  ,FALSE
  ,TRUE
  ,1
  ,0
  ,200
  ,1
  ,
	{ { "Connection", "keep-alive" }
	}
  ,0
  ,""
  }

#define NO_BODY_HTTP10_KA_204 14
, {"HTTP/1.0 with keep-alive and a 204 status"
  ,HTTP_RESPONSE
  ,"HTTP/1.0 204 No content\r\n"
		 "Connection: keep-alive\r\n"
		 "\r\n"
  ,TRUE
  ,FALSE
  ,1
  ,0
  ,204
  ,1
  ,
	{ { "Connection", "keep-alive" }
	}
  ,0
  ,""
  }

#define NO_BODY_HTTP11_KA_200 15
, {"HTTP/1.1 with an EOF-terminated 200 status"
  ,HTTP_RESPONSE
  ,"HTTP/1.1 200 OK\r\n"
		 "\r\n"
  ,FALSE
  ,TRUE
  ,1
  ,1
  ,200
  ,0
  ,{}
  ,0
  ,""
  }

#define NO_BODY_HTTP11_KA_204 16
, {"HTTP/1.1 with a 204 status"
  ,HTTP_RESPONSE
  ,"HTTP/1.1 204 No content\r\n"
		 "\r\n"
  ,TRUE
  ,FALSE
  ,1
  ,1
  ,204
  ,0
  ,{}
  ,0
  ,""
  }

#define NO_BODY_HTTP11_NOKA_204 17
, {"HTTP/1.1 with a 204 status and keep-alive disabled"
  ,HTTP_RESPONSE
  ,"HTTP/1.1 204 No content\r\n"
		 "Connection: close\r\n"
		 "\r\n"
  ,FALSE
  ,FALSE
  ,1
  ,1
  ,204
  ,1
  ,
	{ { "Connection", "close" }
	}
  ,0
  ,""
  }

#define NO_BODY_HTTP11_KA_CHUNKED_200 18
, {"HTTP/1.1 with chunked encoding and a 200 response"
  ,HTTP_RESPONSE
  ,"HTTP/1.1 200 OK\r\n"
		 "Transfer-Encoding: chunked\r\n"
		 "\r\n"
		 "0\r\n"
		 "\r\n"
  ,TRUE
  ,FALSE
  ,1
  ,1
  ,200
  ,1
  ,
	{ { "Transfer-Encoding", "chunked" }
	}
  ,0
  ,""
  }

#if !HTTP_PARSER_STRICT
#define SPACE_IN_FIELD_RES 19
/* Should handle spaces in header fields */
, {"field space"
  ,HTTP_RESPONSE
  ,"HTTP/1.1 200 OK\r\n"
		 "Server: Microsoft-IIS/6.0\r\n"
		 "X-Powered-By: ASP.NET\r\n"
		 "en-US Content-Type: text/xml\r\n" /* this is the problem */
		 "Content-Type: text/xml\r\n"
		 "Content-Length: 16\r\n"
		 "Date: Fri, 23 Jul 2010 18:45:38 GMT\r\n"
		 "Connection: keep-alive\r\n"
		 "\r\n"
		 "<xml>hello</xml>" /* fake body */
  ,TRUE
  ,FALSE
  ,1
  ,1
  ,200
  ,7
  ,
	{ { "Server",  "Microsoft-IIS/6.0" }
	, { "X-Powered-By", "ASP.NET" }
	, { "en-US Content-Type", "text/xml" }
	, { "Content-Type", "text/xml" }
	, { "Content-Length", "16" }
	, { "Date", "Fri, 23 Jul 2010 18:45:38 GMT" }
	, { "Connection", "keep-alive" }
	}
  ,"<xml>hello</xml>"
  }
#endif /* !HTTP_PARSER_STRICT */

, {NULL, HTTP_RESPONSE, 0, FALSE, FALSE, 0, 0, 0, 0, {}, 0, { 0 }} /* sentinel */
};

class ResponseParserWithCounter : public Pillow::HttpResponseParser
{
public:
	int messageBeginCount;
	int headersCompleteCount;
	int messageContentCount;
	int messageCompleteCount;
	bool pauseInMessageBegin;
	bool pauseInHeadersComplete;
	bool pauseInMessageContent;
	bool pauseInMessageComplete;

	ResponseParserWithCounter()
		: messageBeginCount(0), headersCompleteCount(0), messageContentCount(0), messageCompleteCount(0)
		, pauseInMessageBegin(false), pauseInHeadersComplete(false), pauseInMessageContent(false), pauseInMessageComplete(false)
	{}

protected:
	void messageBegin() { ++messageBeginCount; Pillow::HttpResponseParser::messageBegin(); if (pauseInMessageBegin) pause(); }
	void headersComplete() { ++headersCompleteCount;  Pillow::HttpResponseParser::headersComplete(); if (pauseInHeadersComplete) pause(); }
	void messageContent(const char *data, int length) { ++messageContentCount;  Pillow::HttpResponseParser::messageContent(data, length); if (pauseInMessageContent) pause(); }
	void messageComplete() { ++messageCompleteCount;  Pillow::HttpResponseParser::messageComplete(); if (pauseInMessageComplete) pause(); }
};

class ResponseParserWithIsParsingChecker : public Pillow::HttpResponseParser
{
public:
	bool wasParsingInMessageBegin;
	bool wasParsingInHeadersComplete;
	bool wasParsingInMessageContent;
	bool wasParsingInMessageComplete;

	ResponseParserWithIsParsingChecker()
		: wasParsingInMessageBegin(false), wasParsingInHeadersComplete(false)
		, wasParsingInMessageContent(false), wasParsingInMessageComplete(false)
	{}

protected:
	void messageBegin() { Pillow::HttpResponseParser::messageBegin(); wasParsingInMessageBegin = isParsing(); }
	void headersComplete() { Pillow::HttpResponseParser::headersComplete(); wasParsingInHeadersComplete = isParsing(); }
	void messageContent(const char *data, int length) { Pillow::HttpResponseParser::messageContent(data, length); wasParsingInMessageContent = isParsing(); }
	void messageComplete() { Pillow::HttpResponseParser::messageComplete(); wasParsingInMessageComplete = isParsing(); }
};

class ResponseParser : public Pillow::HttpResponseParser
{
public:
	std::function<void()> messageBeginCallback;
	std::function<void()> headersCompleteCallback;
	std::function<void()> messageContentCallback;
	std::function<void()> messageCompleteCallback;

protected:
	void messageBegin() { Pillow::HttpResponseParser::messageBegin(); if (messageBeginCallback) messageBeginCallback(); }
	void headersComplete() { Pillow::HttpResponseParser::headersComplete(); if (headersCompleteCallback) headersCompleteCallback(); }
	void messageContent(const char *data, int length) { Pillow::HttpResponseParser::messageContent(data, length); if (messageContentCallback) messageContentCallback(); }
	void messageComplete() { Pillow::HttpResponseParser::messageComplete(); if (messageCompleteCallback) messageCompleteCallback(); }
};

class HttpResponseParserTest : public QObject
{
	Q_OBJECT

private:
	bool compareParsedResponse(const Pillow::HttpResponseParser& p, const message& m)
	{
		bool ok; compareParsedResponse(p, m, &ok); return ok;
	}

	void compareParsedResponse(const Pillow::HttpResponseParser& p, const message& m, bool* ok)
	{
		*ok = false;
		QCOMPARE(p.statusCode(), m.status_code);
		QCOMPARE(p.httpMajor(), m.http_major);
		QCOMPARE(p.httpMinor(), m.http_minor);
		QCOMPARE(p.headers().size(), m.num_headers);
		QCOMPARE(p.content(), QByteArray(m.body));
		QCOMPARE(p.shouldKeepAlive(), m.should_keep_alive == TRUE);
		QCOMPARE(p.completesOnEof(), m.message_complete_on_eof);
		*ok = true; // All passed
		throw "Parsed response does not match message";
	}

private slots:
	void test_initial_state()
	{
		Pillow::HttpResponseParser p;

		QVERIFY(!p.hasError());
		QCOMPARE(p.error(), HPE_OK);
		QCOMPARE(p.statusCode(), 0);
		QCOMPARE(p.httpMinor(), 0);
		QCOMPARE(p.httpMajor(), 0);
		QCOMPARE(p.headers().size(), 0);
		QCOMPARE(p.shouldKeepAlive(), 0);
		QCOMPARE(p.completesOnEof(), false);
		QVERIFY(!p.isParsing());
	}

	void test_single_valid_responses_data()
	{
		QTest::addColumn<message>("testMessage");

		for (const message* m = responses; m->name != NULL; ++m)
			QTest::newRow(m->name) << *m;
	}

	void test_single_valid_responses()
	{
		QFETCH(message, testMessage);
		QVERIFY(testMessage.type = HTTP_RESPONSE);

		Pillow::HttpResponseParser p;
		size_t consumed = p.inject(testMessage.raw);
		p.injectEof();

		QCOMPARE(consumed, strlen(testMessage.raw));
		QVERIFY(!p.hasError());
		QCOMPARE(p.statusCode(), testMessage.status_code);
		QCOMPARE(p.httpMajor(), testMessage.http_major);
		QCOMPARE(p.httpMinor(), testMessage.http_minor);
		QCOMPARE(p.headers().size(), testMessage.num_headers);
		QCOMPARE(p.content(), QByteArray(testMessage.body));
		QCOMPARE(p.shouldKeepAlive(), testMessage.should_keep_alive == TRUE);
		QCOMPARE(p.completesOnEof(), testMessage.message_complete_on_eof);
		QVERIFY(!p.isParsing());
	}

	void test_keep_alive_responses()
	{
		Pillow::HttpResponseParser p;
		int count = 0;
		for (const message* m1 = responses; m1->name != NULL; ++m1)
		{
			if (m1->should_keep_alive == 0) continue;
			++count;
			p.inject(m1->raw);
			QVERIFY(!p.hasError());
			QCOMPARE(p.statusCode(), m1->status_code);
			QCOMPARE(p.httpMajor(), m1->http_major);
			QCOMPARE(p.httpMinor(), m1->http_minor);
			QCOMPARE(p.headers().size(), m1->num_headers);
			QCOMPARE(p.content(), QByteArray(m1->body));
			QCOMPARE(p.shouldKeepAlive(), m1->should_keep_alive == TRUE);
			QCOMPARE(p.completesOnEof(), m1->message_complete_on_eof);

			for (const message* m2 = responses; m2->name != NULL; ++m2)
			{
				if (m2->should_keep_alive == 0) continue;
				++count;
				p.inject(m2->raw);
				QVERIFY(!p.hasError());
				QCOMPARE(p.statusCode(), m2->status_code);
				QCOMPARE(p.httpMajor(), m2->http_major);
				QCOMPARE(p.httpMinor(), m2->http_minor);
				QCOMPARE(p.headers().size(), m2->num_headers);
				QCOMPARE(p.content(), QByteArray(m2->body));
				QCOMPARE(p.shouldKeepAlive(), m2->should_keep_alive == TRUE);
				QCOMPARE(p.completesOnEof(), m2->message_complete_on_eof);
			}
		}
		QVERIFY(count > 2);
	}

	void test_invalid_responses()
	{
		Pillow::HttpResponseParser p;
		p.inject("HTTP/1.1 BADBAD=-=-=-=-1-1-1-1-\r\nFFFFFFFUUUUUUUUUUUU\r\n\r\n");

		QVERIFY(p.hasError());
		QCOMPARE(p.error(), HPE_INVALID_STATUS);
		QCOMPARE(p.statusCode(), 0);
		QCOMPARE(p.httpMajor(), 1);
		QCOMPARE(p.httpMinor(), 1);
		QCOMPARE(p.headers().size(), 0);
		QCOMPARE(p.content(), QByteArray());
		QVERIFY(!p.shouldKeepAlive());
		QVERIFY(p.completesOnEof());
		QVERIFY(!p.isParsing());

		p.injectEof();

		QVERIFY(p.hasError());
		QCOMPARE(p.error(), HPE_INVALID_STATUS);
		QCOMPARE(p.statusCode(), 0);
		QCOMPARE(p.httpMajor(), 1);
		QCOMPARE(p.httpMinor(), 1);
		QCOMPARE(p.headers().size(), 0);
		QCOMPARE(p.content(), QByteArray());
		QVERIFY(!p.shouldKeepAlive());
		QVERIFY(p.completesOnEof());
		QVERIFY(!p.isParsing());

		// Injecting a good message should not make the parser recover.
		p.inject("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
		QVERIFY(p.hasError());
		QCOMPARE(p.error(), HPE_INVALID_STATUS);
		QCOMPARE(p.statusCode(), 0);
		QCOMPARE(p.httpMajor(), 1);
		QCOMPARE(p.httpMinor(), 1);
		QCOMPARE(p.headers().size(), 0);
		QCOMPARE(p.content(), QByteArray());
		QVERIFY(!p.shouldKeepAlive());
		QVERIFY(p.completesOnEof());
		QVERIFY(!p.isParsing());
	}

	void should_recover_from_error_when_cleared()
	{
		Pillow::HttpResponseParser p;
		p.inject("HTTP/1.1 POPOPO=-=-=-=-1-1-1-1-\r\nFFFFFFFFUUUUUUUUUUUUUU\r\n\r\n");

		QVERIFY(p.hasError());
		QCOMPARE(p.error(), HPE_INVALID_STATUS);
		QCOMPARE(p.statusCode(), 0);
		QCOMPARE(p.httpMajor(), 1);
		QCOMPARE(p.httpMinor(), 1);
		QCOMPARE(p.headers().size(), 0);
		QCOMPARE(p.content(), QByteArray());
		QVERIFY(!p.shouldKeepAlive());
		QVERIFY(p.completesOnEof());
		QVERIFY(!p.isParsing());

		p.clear();

		p.inject("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
		QVERIFY(!p.hasError());
		QCOMPARE(p.error(), HPE_OK);
		QCOMPARE(p.statusCode(), 200);
		QCOMPARE(p.httpMajor(), 1);
		QCOMPARE(p.httpMinor(), 1);
		QCOMPARE(p.headers().size(), 1);
		QCOMPARE(p.content(), QByteArray());
		QVERIFY(p.shouldKeepAlive());
		QVERIFY(!p.completesOnEof());
		QVERIFY(!p.isParsing());
	}

	void should_clear_all_fields_when_cleared()
	{
		Pillow::HttpResponseParser p;
		p.inject("HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nabc");
		QVERIFY(!p.hasError());
		QCOMPARE(p.statusCode(), 200);
		QCOMPARE(p.httpMajor(), 1);
		QCOMPARE(p.httpMinor(), 1);
		QCOMPARE(p.headers().size(), 1);
		QCOMPARE(p.content(), QByteArray("abc"));
		QVERIFY(p.shouldKeepAlive());
		QVERIFY(!p.completesOnEof());

		p.clear();

		QVERIFY(!p.hasError());
		QCOMPARE(p.error(), HPE_OK);
		QCOMPARE(p.statusCode(), 0);
		QCOMPARE(p.httpMinor(), 0);
		QCOMPARE(p.httpMajor(), 0);
		QCOMPARE(p.headers().size(), 0);
		QCOMPARE(p.shouldKeepAlive(), 0);
		QCOMPARE(p.completesOnEof(), false);
		QCOMPARE(p.content(), QByteArray());
	}

	void should_only_consume_one_response_at_a_time_for_pipelined_responses()
	{
		QByteArray threeResponses = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\nHTTP/1.1 201 OK\r\nContent-Length: 0\r\n\r\nHTTP/1.1 202 OK\r\nContent-Length: 0\r\n\r\n";

		Pillow::HttpResponseParser p;
		int consumed = p.inject(threeResponses);

		QCOMPARE(consumed, 38);
		QCOMPARE(p.statusCode(), 200);
		QVERIFY(!p.completesOnEof());

		int consumed2 = p.inject(threeResponses.mid(consumed));
		QCOMPARE(p.statusCode(), 201);
		QVERIFY(!p.completesOnEof());

		p.inject(threeResponses.mid(consumed).mid(consumed2));
		QCOMPARE(p.statusCode(), 202);
		QVERIFY(!p.completesOnEof());
	}


	void should_detect_too_much_data()
	{
		Pillow::HttpResponseParser p;
		p.inject("HTTP/1.0 200 OK\r\nContent-Length:4\r\n\r\nabcdefg"); // Too much content!

		QVERIFY(!p.hasError());
		QCOMPARE(p.statusCode(), 200);
		QCOMPARE(p.httpMajor(), 1);
		QCOMPARE(p.httpMinor(), 0);
		QCOMPARE(p.headers().size(), 1);
		QCOMPARE(p.content(), QByteArray("abcd"));
		QVERIFY(!p.shouldKeepAlive());
		QVERIFY(!p.completesOnEof());
	}

	void should_call_callbacks()
	{
		ResponseParserWithCounter p;
		QCOMPARE(p.messageBeginCount, 0);
		QCOMPARE(p.headersCompleteCount, 0);
		QCOMPARE(p.messageContentCount, 0);
		QCOMPARE(p.messageCompleteCount, 0);

		p.inject("HTTP/1.1");
		QCOMPARE(p.messageBeginCount, 1);
		QCOMPARE(p.headersCompleteCount, 0);
		QCOMPARE(p.messageContentCount, 0);
		QCOMPARE(p.messageCompleteCount, 0);

		p.inject(" 200 OK\r\nContent-Length: 4\r\n");
		QCOMPARE(p.messageBeginCount, 1);
		QCOMPARE(p.headersCompleteCount, 0);
		QCOMPARE(p.messageContentCount, 0);
		QCOMPARE(p.messageCompleteCount, 0);

		p.inject("\r\n");
		QCOMPARE(p.messageBeginCount, 1);
		QCOMPARE(p.headersCompleteCount, 1);
		QCOMPARE(p.messageContentCount, 0);
		QCOMPARE(p.messageCompleteCount, 0);

		p.inject("12");
		QCOMPARE(p.messageBeginCount, 1);
		QCOMPARE(p.headersCompleteCount, 1);
		QCOMPARE(p.messageContentCount, 1);
		QCOMPARE(p.messageCompleteCount, 0);

		p.inject("34");
		QCOMPARE(p.messageBeginCount, 1);
		QCOMPARE(p.headersCompleteCount, 1);
		QCOMPARE(p.messageContentCount, 2);
		QCOMPARE(p.messageCompleteCount, 1);

		p.inject("HTTP/1.1 302 Found\r\nLocation: somewhere\r\nContent-Length: 0\r\n\r\n");
		QCOMPARE(p.messageBeginCount, 2);
		QCOMPARE(p.headersCompleteCount, 2);
		QCOMPARE(p.messageContentCount, 2);
		QCOMPARE(p.messageCompleteCount, 2);

		p.inject("HTTP/1.1 400 Bad Request\r\nContent-Length: 2\r\n\r\n12");
		QCOMPARE(p.messageBeginCount, 3);
		QCOMPARE(p.headersCompleteCount, 3);
		QCOMPARE(p.messageContentCount, 3);
		QCOMPARE(p.messageCompleteCount, 3);
	}

	void should_be_pausable_in_callbacks()
	{
		QByteArray responseHeaders = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nContent-Type: text/plain\r\n\r\n";
		QByteArray responseContent = "1234";
		QByteArray response = responseHeaders + responseContent;

		{
			ResponseParserWithCounter p;
			p.pauseInMessageBegin = true;
			int consumed = p.inject(response);
			QCOMPARE(consumed, 1); // http_parser wants at least 1 valid byte to get started.
			QCOMPARE(p.messageBeginCount, 1);
			QCOMPARE(p.headersCompleteCount, 0);
			QCOMPARE(p.messageContentCount, 0);
			QCOMPARE(p.messageCompleteCount, 0);

			// Should allow continuing.
			p.inject(response.mid(consumed));
			QCOMPARE(p.messageBeginCount, 1);
			QCOMPARE(p.headersCompleteCount, 1);
			QCOMPARE(p.messageContentCount, 1);
			QCOMPARE(p.messageCompleteCount, 1);
			QCOMPARE(p.content(), QByteArray("1234"));
		}

		{
			ResponseParserWithCounter p;
			p.pauseInHeadersComplete = true;
			int consumed = p.inject(response);
			QCOMPARE(consumed, responseHeaders.size() - 1); // headers are complete as soon as the parser encountered "\r\n\r" (it will then silently consume the other "\n")
			QVERIFY(responseHeaders.size() > 0);
			QCOMPARE(p.messageBeginCount, 1);
			QCOMPARE(p.headersCompleteCount, 1);
			QCOMPARE(p.messageContentCount, 0);
			QCOMPARE(p.messageCompleteCount, 0);

			// Should allow continuing.
			p.inject(response.mid(consumed));
			QCOMPARE(p.messageBeginCount, 1);
			QCOMPARE(p.headersCompleteCount, 1);
			QCOMPARE(p.messageContentCount, 1);
			QCOMPARE(p.messageCompleteCount, 1);
			QCOMPARE(p.content(), QByteArray("1234"));
		}

		{
			ResponseParserWithCounter p;
			p.pauseInMessageContent = true;
			int consumed = p.inject(response);
			QCOMPARE(consumed, responseHeaders.size() + responseContent.size() - 1);
			QVERIFY(responseContent.size() > 0);
			QCOMPARE(p.messageBeginCount, 1);
			QCOMPARE(p.headersCompleteCount, 1);
			QCOMPARE(p.messageContentCount, 1);
			QCOMPARE(p.messageCompleteCount, 0);

			// Should allow continuing.
			p.inject(response.mid(consumed));
			QCOMPARE(p.messageBeginCount, 1);
			QCOMPARE(p.headersCompleteCount, 1);
			QCOMPARE(p.messageContentCount, 1);
			QCOMPARE(p.messageCompleteCount, 1);
			QCOMPARE(p.content(), QByteArray("1234"));
		}

		{
			ResponseParserWithCounter p;
			p.pauseInMessageComplete = true;
			int consumed = p.inject(response);
			QCOMPARE(consumed, response.size());
			QCOMPARE(p.messageBeginCount, 1);
			QCOMPARE(p.headersCompleteCount, 1);
			QCOMPARE(p.messageContentCount, 1);
			QCOMPARE(p.messageCompleteCount, 1);

			// Continuing should do nothing as there is nothing left.
			p.inject(response.mid(consumed));
			QCOMPARE(p.messageBeginCount, 1);
			QCOMPARE(p.headersCompleteCount, 1);
			QCOMPARE(p.messageContentCount, 1);
			QCOMPARE(p.messageCompleteCount, 1);
			QCOMPARE(p.content(), QByteArray("1234"));
		}
	}

	void should_be_parsing_while_parsing()
	{
		ResponseParserWithIsParsingChecker p;
		QVERIFY(!p.wasParsingInMessageBegin);
		QVERIFY(!p.wasParsingInHeadersComplete);
		QVERIFY(!p.wasParsingInMessageContent);
		QVERIFY(!p.wasParsingInMessageComplete);
		QVERIFY(!p.isParsing());

		p.inject("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nab");
		QVERIFY(p.wasParsingInMessageBegin);
		QVERIFY(p.wasParsingInHeadersComplete);
		QVERIFY(p.wasParsingInMessageContent);
		QVERIFY(p.wasParsingInMessageComplete);
		QVERIFY(!p.isParsing());
	}

	void should_pause_parsing_when_cleared_while_parsing()
	{
		QByteArray response = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nContent-Type: text/plain\r\n\r\nabcd";

		{
			ResponseParser p;
			p.messageBeginCallback = [&]{ p.clear(); };
			int consumed = p.inject(response);
			QCOMPARE(consumed, 1); // http_parser wants at least 1 valid byte to get started.

			// Should be reset and ready to parse a new response.
			QVERIFY(!p.hasError());
			p.messageBeginCallback = 0;

			QCOMPARE(p.inject(response), response.size());
			QVERIFY(!p.hasError());
			QCOMPARE(p.statusCode(), 200);
			QCOMPARE(p.httpMajor(), 1);
			QCOMPARE(p.httpMinor(), 1);
			QCOMPARE(p.headers().size(), 2);
			QCOMPARE(p.content(), QByteArray("abcd"));
			QVERIFY(p.shouldKeepAlive());
			QVERIFY(!p.completesOnEof());
		}

		{
			ResponseParser p;
			p.headersCompleteCallback = [&]{ p.clear(); };
			QCOMPARE(p.inject(response), 63);

			// Should be reset and ready to parse a new response.
			QVERIFY(!p.hasError());
			p.headersCompleteCallback = 0;

			QCOMPARE(p.inject(response), response.size());
			QVERIFY(!p.hasError());
			QCOMPARE(p.statusCode(), 200);
			QCOMPARE(p.httpMajor(), 1);
			QCOMPARE(p.httpMinor(), 1);
			QCOMPARE(p.headers().size(), 2);
			QCOMPARE(p.content(), QByteArray("abcd"));
			QVERIFY(p.shouldKeepAlive());
			QVERIFY(!p.completesOnEof());
		}

		{
			ResponseParser p;
			p.messageContentCallback = [&]{ p.clear(); };
			QCOMPARE(p.inject(response), 67);

			// Should be reset and ready to parse a new response.
			QVERIFY(!p.hasError());
			p.messageContentCallback = 0;

			QCOMPARE(p.inject(response), response.size());
			QVERIFY(!p.hasError());
			QCOMPARE(p.statusCode(), 200);
			QCOMPARE(p.httpMajor(), 1);
			QCOMPARE(p.httpMinor(), 1);
			QCOMPARE(p.headers().size(), 2);
			QCOMPARE(p.content(), QByteArray("abcd"));
			QVERIFY(p.shouldKeepAlive());
			QVERIFY(!p.completesOnEof());
		}

		{
			ResponseParser p;
			p.messageCompleteCallback = [&]{ p.clear(); };
			QCOMPARE(p.inject(response), 68);

			// Should be reset and ready to parse a new response.
			QVERIFY(!p.hasError());
			p.messageCompleteCallback = 0;

			QCOMPARE(p.inject(response), response.size());
			QVERIFY(!p.hasError());
			QCOMPARE(p.statusCode(), 200);
			QCOMPARE(p.httpMajor(), 1);
			QCOMPARE(p.httpMinor(), 1);
			QCOMPARE(p.headers().size(), 2);
			QCOMPARE(p.content(), QByteArray("abcd"));
			QVERIFY(p.shouldKeepAlive());
			QVERIFY(!p.completesOnEof());
		}


	}
};
PILLOW_TEST_DECLARE(HttpResponseParserTest)

#include "HttpClientTest.moc"
