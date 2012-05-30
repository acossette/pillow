#ifndef HELPERS_H
#define HELPERS_H

#include <QtCore/qdebug.h>
#include <QtCore/qelapsedtimer.h>
#include <QtNetwork/qtcpsocket.h>
#include <QtTest/qtest.h>
#include <QtTest/QSignalSpy>
#include <HttpConnection.h>
#include <HttpServer.h>

#define PILLOW_TEST_DECLARE(TestClass) \
	int exec_##TestClass() \
	{ \
		TestClass t; return QTest::qExec(&t, QCoreApplication::arguments()); \
	}

#define PILLOW_TEST_RUN(TestClass, resultVar) \
	extern int exec_##TestClass(); resultVar += exec_##TestClass();

struct HttpRequestData
{
	QByteArray _method;
	QByteArray _uri;
	QByteArray _path;
	QByteArray _queryString;
	QByteArray _fragment;
	QByteArray _httpVersion;
	QByteArray _content;
	Pillow::HttpHeaderCollection _headers;

	inline HttpRequestData() {}

	HttpRequestData& withMethod(const QByteArray& arg) { _method = arg; return *this; }
	HttpRequestData& withUri(const QByteArray& arg) { _uri = arg; return *this; }
	HttpRequestData& withPath(const QByteArray& arg) { _path = arg; return *this; }
	HttpRequestData& withQueryString(const QByteArray& arg) { _queryString = arg; return *this; }
	HttpRequestData& withFragment(const QByteArray& arg) { _fragment = arg; return *this; }
	HttpRequestData& withHttpVersion(const QByteArray& arg) { _httpVersion = arg; return *this; }
	HttpRequestData& withContent(const QByteArray& arg) { _content = arg; return *this; }
	HttpRequestData& withHeaders(const Pillow::HttpHeaderCollection& arg) { _headers = arg; return *this; }

	static HttpRequestData fromHttpConnection(Pillow::HttpConnection* c)
	{
		HttpRequestData d;
		d._method = c->requestMethod();
		d._uri = c->requestUri();
		d._path = c->requestPath();
		d._queryString = c->requestQueryString();
		d._fragment = c->requestFragment();
		d._httpVersion = c->requestHttpVersion();
		d._content = c->requestContent();
		d._headers = c->requestHeaders();
		d._headers.detach();
		for (int i = 0; i < d._headers.size(); ++i)
		{
			d._headers[i].first.detach();
			d._headers[i].second.detach();
		}
		return d;
	}
};
Q_DECLARE_METATYPE(HttpRequestData)

class TestServer : public Pillow::HttpServer
{
	Q_OBJECT

public:
	inline TestServer()
	{
		connect(this, SIGNAL(requestReady(Pillow::HttpConnection*)), this, SLOT(self_requestReady(Pillow::HttpConnection*)));
	}

	QList<HttpRequestData> receivedRequests;
	QList<Pillow::HttpConnection*> receivedConnections;
	QList<QTcpSocket*> receivedSockets;

	inline bool waitForRequest(int maxTime = 500)
	{
		QElapsedTimer t; t.start();
		int initialRequests = receivedRequests.size();
		while (receivedRequests.size() == initialRequests && t.elapsed() < maxTime) QCoreApplication::processEvents();
		if (receivedRequests.size() == initialRequests)
		{
			qWarning() << "Timed out waiting for request";
			return false;
		}
		return true;
	}

private slots:
	inline void self_requestReady(Pillow::HttpConnection* request)
	{
		receivedRequests << HttpRequestData::fromHttpConnection(request);
		receivedConnections << request;
		receivedSockets << qobject_cast<QTcpSocket*>(request->inputDevice());
	}
};

namespace PillowTest
{
	template <typename T1, typename T2>
	bool pCompare(T1 const & t1, T2 const & t2, const char * actual, const char * expected, const char * file, int line)
	{
		return QTest::qCompare(t1, t2, actual, expected, file, line);
	}

	template <>
	inline bool pCompare(unsigned short const &t1, int const &t2, const char *actual, const char *expected, const char *file, int line)
	{
		return QTest::qCompare(int(t1), t2, actual, expected, file, line);
	}

	template <>
	inline bool pCompare(QByteArray const &t1, QByteArray const &t2, const char *actual, const char *expected, const char *file, int line)
	{
		if (!QTest::qCompare(t1, t2, actual, expected, file, line))
		{
			if (t1.size() != t2.size())
			{
				qWarning() << "QByteArray sizes are different:";
				qWarning() << "  Actual:" << t1.size();
				qWarning() << "  Expected:" << t2.size();
			}

			for (int i = 0, l = qMin(t1.size(), t2.size()); i < l; ++i)
			{
				if (t1.at(i) != t2.at(i))
				{
					qWarning() << "First difference at index: " << i;
					qWarning() << "  Actual:" << t1.mid(i, 128);
					qWarning() << "  Expected:" << t2.mid(i, 128);
					break;
				}
			}
			return false;
		}
		else
			return true;
	}
	template <>
	inline bool pCompare(Pillow::HttpHeaderCollection const &t1, Pillow::HttpHeaderCollection const &t2, const char *actual, const char *expected, const char *file, int line)
	{
		if (t1 != t2)
		{
			qWarning() << "Headers do not match";
			qWarning() << "  Actual:" << t1.size();
			foreach (const Pillow::HttpHeader& h, t1)
				qWarning() << "    " << h.first << ":" << h.second;
			qWarning() << "  Expected:" << t2.size();
			foreach (const Pillow::HttpHeader& h, t2)
				qWarning() << "    " << h.first << ":" << h.second;

			return QTest::qCompare(t1, t2, actual, expected, file, line); // Let qCompare report the failure.
		}
		return true;
	}

	inline bool pCompare(QList<QPair<QByteArray, QByteArray> > const &t1, Pillow::HttpHeaderCollection const &t2, const char *actual, const char *expected, const char *file, int line)
	{
		Pillow::HttpHeaderCollection _t1;
		for (int i = 0; i < t1.size(); ++i) _t1.append(t1.at(i));
		return pCompare(const_cast<const Pillow::HttpHeaderCollection&>(_t1), t2, actual, expected, file, line);
	}


	template <>
	inline bool pCompare(HttpRequestData const &t1, HttpRequestData const &t2, const char *actual, const char *expected, const char *file, int line)
	{
		if (!PillowTest::pCompare(t1._method, t2._method, actual, expected, file, line)) return false;
		if (!PillowTest::pCompare(t1._uri, t2._uri, actual, expected, file, line)) return false;
		if (!PillowTest::pCompare(t1._path, t2._path, actual, expected, file, line)) return false;
		if (!PillowTest::pCompare(t1._queryString, t2._queryString, actual, expected, file, line)) return false;
		if (!PillowTest::pCompare(t1._fragment, t2._fragment, actual, expected, file, line)) return false;
		if (!PillowTest::pCompare(t1._httpVersion, t2._httpVersion, actual, expected, file, line)) return false;
		if (!PillowTest::pCompare(t1._content, t2._content, actual, expected, file, line)) return false;

		if (t1._headers != t2._headers)
		{
			qWarning() << "Request does not have the expected headers";
			qWarning() << "  Actual:" << t1._headers.size();
			foreach (const Pillow::HttpHeader& h, t1._headers)
				qWarning() << "         " << h.first << ": " << h.second;
			qWarning() << "  Expected:" << t2._headers.size();
			foreach (const Pillow::HttpHeader& h, t2._headers)
				qWarning() << "         " << h.first << ": " << h.second;
			QTest::qFail("Request does not have the expected headers", file, line);
			return false;
		}

		return true;
	}

}

#undef QCOMPARE
#define QCOMPARE(actual, expected) \
do {\
	if (!PillowTest::pCompare(actual, expected, #actual, #expected, __FILE__, __LINE__))\
		return;\
} while (0)

template <typename Pred> bool waitFor(const Pred& predicate, int maxTime = 500)
{
	QElapsedTimer t; t.start();
	bool result = false;
	while (!(result = predicate()) && !t.hasExpired(maxTime))
		QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
	return result;
}

inline bool waitForSignal(QObject *obj, const char* signal, int maxTime = 500)
{
	QSignalSpy spy(obj, signal);
	return waitFor([&]{ return spy.size() > 0; }, maxTime);
}

#endif // HELPERS_H
