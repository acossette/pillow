#include <QtCore/QtCore>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <HttpClient.h>

class Bench : public QObject
{
	Q_OBJECT

public:
	Bench(QObject *parent = 0)
		: QObject(parent), m_requestCount(1), m_concurrency(1)
	{}

	int requestCount() const { return m_requestCount; }
	void setRequestCount(int arg) { if (arg < 1) arg = 1; m_requestCount = arg; }

	int concurrency() const { return m_concurrency; }
	void setConcurrency(int arg) { if (arg < 1) arg = 1; m_concurrency = arg; }

	QUrl url() const { return m_url; }
	void setUrl(QUrl arg) { m_url = arg; }

public:
	virtual void start()
	{
		m_remainingRequests = m_requestCount;
		m_elapsedTimer.start();
	}

protected slots:
	void finish()
	{
		qint64 elapsed = m_elapsedTimer.elapsed();
		int requestsPerSecond = m_requestCount / (float(elapsed) / 1000.0f);
		qint64 usecsPerReq = elapsed * 1000 / m_requestCount;
		qDebug() << m_requestCount << "completed in" << elapsed << "ms" << requestsPerSecond << "reqs/sec." << usecsPerReq << "usecs/reqs";
		::exit(0);
	}

	void fail()
	{
		qDebug() << "Encountered error after" << (m_requestCount - m_remainingRequests) << "completed requests. Aborting.";
		::exit(1);
	}

protected:
	int m_requestCount;
	int m_concurrency;
	QUrl m_url;

	int m_remainingRequests;
	QElapsedTimer m_elapsedTimer;
};

class ClientBench : public Bench
{
	Q_OBJECT

public:
	ClientBench(QObject *parent = 0)
		: Bench(parent)
	{}

	void start()
	{
		Bench::start();

		int clientCount = qMin(m_concurrency, m_requestCount);

		qDebug() << "Pillow clientbench to" << qPrintable(url().toString());
		qDebug() << requestCount() << "request with concurency of" << clientCount;

		for (int i = 0; i < clientCount; ++i)
		{
			Pillow::HttpClient *client = new Pillow::HttpClient(this);
			connect(client, SIGNAL(finished()), this, SLOT(client_finished()));
			client->get(url());
		}
	}

private slots:
	void client_finished()
	{
		Pillow::HttpClient *client = static_cast<Pillow::HttpClient*>(sender());

		if (client->error() != Pillow::HttpClient::NoError)
			fail();
		else
		{
			if (--m_remainingRequests == 0)
				finish();
			else
				client->get(url());
		}
	}
};

class NamBench : public Bench
{
	Q_OBJECT

public:
	NamBench(QObject *parent = 0)
		: Bench(parent)
	{
		m_nam = new QNetworkAccessManager(this);
	}

	void start()
	{
		Bench::start();

		int clientCount = qMin(6, qMin(m_concurrency, m_requestCount));

		qDebug() << "Pillow clientbench for QNetworkAccessManager to" << qPrintable(url().toString());
		qDebug() << requestCount() << "request with concurency of" << clientCount;

		for (int i = 0; i < clientCount; ++i)
		{
			QNetworkReply *reply = m_nam->get(QNetworkRequest(url()));
			connect(reply, SIGNAL(finished()), this, SLOT(client_finished()));
		}
	}

private slots:
	void client_finished()
	{
		QNetworkReply *reply = static_cast<QNetworkReply*>(sender());

		if (reply->error() != QNetworkReply::NoError)
			fail();
		else
		{
			if (--m_remainingRequests == 0)
				finish();
			else
			{
				reply->deleteLater();
				reply = m_nam->get(QNetworkRequest(url()));
				connect(reply, SIGNAL(finished()), this, SLOT(client_finished()));
			}
		}
	}

private:
	QNetworkAccessManager * m_nam;
};

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	// Support similar arguments as apachebench
	// clientbench -n <number of requests> -c <concurency> <url>

	ClientBench bench;
	//NamBench bench;

	for (int i = 0, iE = a.arguments().size(); i < iE; ++i)
	{
		QString arg = a.arguments().at(i);
		QString value = (i + 1) < iE ? a.arguments().at(i + 1) : QString();

		if (arg == "-n")
			bench.setRequestCount(value.toInt());
		else if (arg == "-c")
			bench.setConcurrency(value.toInt());
		else
			bench.setUrl(arg);
	}

	bench.start();

	return a.exec();
}

#include "clientbench.moc"
