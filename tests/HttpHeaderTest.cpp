#include <QtTest/QtTest>
#include "Helpers.h"
#include <HttpConnection.h>
#include <ByteArrayHelpers.h>

class HttpHeaderTest : public QObject
{
	Q_OBJECT

private slots:
	void should_be_initially_blank()
	{
		Pillow::HttpHeader h;
		QVERIFY(h.first.isEmpty());
		QVERIFY(h.second.isEmpty());
	}

	void should_be_constructible_from_key_value_literals()
	{
		{
			Pillow::HttpHeader h("Accept",  "some/stuff");
			QCOMPARE(h.first, QByteArray("Accept"));
			QCOMPARE(h.second, QByteArray("some/stuff"));
		}
		{
			Pillow::HttpHeader h("Accept-More",  "");
			QCOMPARE(h.first, QByteArray("Accept-More"));
			QCOMPARE(h.second, QByteArray());
		}
		{
			Pillow::HttpHeader h("",  "");
			QCOMPARE(h.first, QByteArray());
			QCOMPARE(h.second, QByteArray());
		}
		{
			Pillow::HttpHeader h("   ",  " 1234 ");
			QCOMPARE(h.first, QByteArray("   "));
			QCOMPARE(h.second, QByteArray(" 1234 "));
		}

	}

	void should_be_constructible_from_raw_header_literal_including_colon()
	{
		{
			Pillow::HttpHeader h("Accept: some/stuff");
			QCOMPARE(h.first, QByteArray("Accept"));
			QCOMPARE(h.second, QByteArray("some/stuff"));
		}
		{
			Pillow::HttpHeader h("Accept-More:some/otherstuff");
			QCOMPARE(h.first, QByteArray("Accept-More"));
			QCOMPARE(h.second, QByteArray("some/otherstuff"));
		}
		{
			Pillow::HttpHeader h("Has-No-Value:");
			QCOMPARE(h.first, QByteArray("Has-No-Value"));
			QCOMPARE(h.second, QByteArray());
		}
		{
			Pillow::HttpHeader h("Has-No-Value-Either");
			QCOMPARE(h.first, QByteArray("Has-No-Value-Either"));
			QCOMPARE(h.second, QByteArray());
		}
		{
			Pillow::HttpHeader h("Has-No-Value-Again:           ");
			QCOMPARE(h.first, QByteArray("Has-No-Value-Again"));
			QCOMPARE(h.second, QByteArray());
		}
		{
			Pillow::HttpHeader h("");
			QCOMPARE(h.first, QByteArray());
			QCOMPARE(h.second, QByteArray());
		}
		{
			Pillow::HttpHeader h(":");
			QCOMPARE(h.first, QByteArray());
			QCOMPARE(h.second, QByteArray());
		}
	}

	void should_be_constructible_from_qpair_of_bytearrays()
	{
		QPair<QByteArray, QByteArray> pair("Hello", "World");
		Pillow::HttpHeader h(pair);
		QCOMPARE(h.first, QByteArray("Hello"));
		QCOMPARE(h.second, QByteArray("World"));
		Pillow::HttpHeader h2(h);
		QCOMPARE(h2.first, QByteArray("Hello"));
		QCOMPARE(h2.second, QByteArray("World"));

	}

	void should_be_convertible_to_qpair_of_bytearrays()
	{
		Pillow::HttpHeader h;
		h.first = "SomeField";
		h.second = "SomeValue";
		QPair<QByteArray, QByteArray> pair = h;
		QCOMPARE(pair.first, QByteArray("SomeField"));
		QCOMPARE(pair.second, QByteArray("SomeValue"));
	}

	void should_be_convertible_to_a_ref_to_a_const_pair_of_bytearrays()
	{
		Pillow::HttpHeader h;
		h.first = "a";
		h.second = "b";
		const QPair<QByteArray, QByteArray> &pair = h;
		QCOMPARE(pair.first, QByteArray("a"));
		QCOMPARE(pair.second, QByteArray("b"));

		Pillow::HttpHeader h2 = pair;
		QCOMPARE(h2.first, QByteArray("a"));
		QCOMPARE(h2.second, QByteArray("b"));
	}
};
PILLOW_TEST_DECLARE(HttpHeaderTest)

class HttpHeaderCollectionTest : public QObject
{
	Q_OBJECT

private slots:
	void should_be_initially_blank()
	{
		Pillow::HttpHeaderCollection c;
		QCOMPARE(c.size(), 0);
		QVERIFY(c.isEmpty());
	}

	void should_be_a_vector_of_http_headers()
	{
		Pillow::HttpHeaderCollection c;
		c.append(Pillow::HttpHeader("Hello", "World"));
		Pillow::HttpHeaderCollection &c2 = c;
		QVector<Pillow::HttpHeader> &c3 = c2;
		QCOMPARE(c3.size(), 1);
		QCOMPARE(c3.at(0).first, QByteArray("Hello"));
		QCOMPARE(c3.at(0).second, QByteArray("World"));
	}

	void should_allow_adding_headers()
	{
		// Just testing the methods reimplemented from QVector<T>.
		Pillow::HttpHeaderCollection c;
		c << Pillow::HttpHeader("a", "1") << Pillow::HttpHeader("b", "2");
		QCOMPARE(c.size(), 2);
		QCOMPARE(c.at(0).first, QByteArray("a"));
		QCOMPARE(c.at(0).second, QByteArray("1"));
		QCOMPARE(c.at(1).first, QByteArray("b"));
		QCOMPARE(c.at(1).second, QByteArray("2"));

		Pillow::HttpHeaderCollection c2 = Pillow::HttpHeaderCollection() << Pillow::HttpHeader("c", "3");
		c2 += c;
		QCOMPARE(c2.size(), 3);
		QCOMPARE(c2.at(0).first, QByteArray("c"));
		QCOMPARE(c2.at(0).second, QByteArray("3"));
		QCOMPARE(c2.at(1).first, QByteArray("a"));
		QCOMPARE(c2.at(1).second, QByteArray("1"));
		QCOMPARE(c2.at(2).first, QByteArray("b"));
		QCOMPARE(c2.at(2).second, QByteArray("2"));
	}

	void should_allow_inserting_raw_headers_literals()
	{
		Pillow::HttpHeaderCollection c;
		c << "Accept: */*" << "Content-type: text/plain";
		c.append("User-Agent: Pillow/1.0");
		QCOMPARE(c.size(), 3);
		QCOMPARE(c.at(0).first, QByteArray("Accept"));
		QCOMPARE(c.at(0).second, QByteArray("*/*"));
		QCOMPARE(c.at(1).first, QByteArray("Content-type"));
		QCOMPARE(c.at(1).second, QByteArray("text/plain"));
		QCOMPARE(c.at(2).first, QByteArray("User-Agent"));
		QCOMPARE(c.at(2).second, QByteArray("Pillow/1.0"));

		c.prepend("Connection: close");
		QCOMPARE(c.size(), 4);
		QCOMPARE(c.at(0).first, QByteArray("Connection"));
		QCOMPARE(c.at(0).second, QByteArray("close"));
		QCOMPARE(c.at(1).first, QByteArray("Accept"));
		QCOMPARE(c.at(1).second, QByteArray("*/*"));
		QCOMPARE(c.at(2).first, QByteArray("Content-type"));
		QCOMPARE(c.at(2).second, QByteArray("text/plain"));
		QCOMPARE(c.at(3).first, QByteArray("User-Agent"));
		QCOMPARE(c.at(3).second, QByteArray("Pillow/1.0"));
	}

	void should_be_a_vector_of_bytearray_pairs()
	{
		QVector<QPair<QByteArray, QByteArray> > c3;

		{
			Pillow::HttpHeaderCollection c;
			c << "A: 1" << "B: 2";

			const QVector<QPair<QByteArray, QByteArray> > &c2 = c;
			QCOMPARE(c2.size(), 2);
			QCOMPARE(c2.at(0).first, QByteArray("A"));
			QCOMPARE(c2.at(0).second, QByteArray("1"));
			QCOMPARE(c2.at(1).first, QByteArray("B"));
			QCOMPARE(c2.at(1).second, QByteArray("2"));

			c3 = c2;
		}

		QCOMPARE(c3.size(), 2);
		QCOMPARE(c3.at(0).first, QByteArray("A"));
		QCOMPARE(c3.at(0).second, QByteArray("1"));
		QCOMPARE(c3.at(1).first, QByteArray("B"));
		QCOMPARE(c3.at(1).second, QByteArray("2"));

	}

	void should_allow_finding_field_value_case_insensitively()
	{
		Pillow::HttpHeaderCollection c;
		c << "Some-Field: some/value"
		  << "ANOTHER-FIELD: ANOTHER/VALUE"
		  << "third-field: third/VALUE"
		  << "abcde-field: abcde/VALUE"
		  << "and more: value";

		QCOMPARE(c.getFieldValue("Some-Field"), QByteArray("some/value"));
		QCOMPARE(c.getFieldValue("some-field"), QByteArray("some/value"));
		QCOMPARE(c.getFieldValue("SOME-Field"), QByteArray("some/value"));
		QCOMPARE(c.getFieldValue("ANOTHER-field"), QByteArray("ANOTHER/VALUE"));
		QCOMPARE(c.getFieldValue("THIRD-FIELD"), QByteArray("third/VALUE"));
		QCOMPARE(c.getFieldValue("AND more"), QByteArray("value"));
		QCOMPARE(c.getFieldValue("does not exist"), QByteArray());
		QCOMPARE(c.getFieldValue(""), QByteArray());
	}

	void should_allow_finding_multiple_field_values_case_insensitively()
	{
		Pillow::HttpHeaderCollection c;
		c << "Repeated-Field: some/value"
		  << "ANOTHER-FIELD: ANOTHER/VALUE"
		  << "Repeated-Field: some different value";

		QCOMPARE(c.getFieldValues("repeated-FiELD"), QVector<QByteArray>() << "some/value" << "some different value");
		QCOMPARE(c.getFieldValues("Repeated-Field"), QVector<QByteArray>() << "some/value" << "some different value");
		QCOMPARE(c.getFieldValues("Another-Field"), QVector<QByteArray>() << "ANOTHER/VALUE");
		QCOMPARE(c.getFieldValues("does not exist"), QVector<QByteArray>());
		QCOMPARE(c.getFieldValues(""), QVector<QByteArray>());
	}

	void benchmark_getFieldValue()
	{
		Pillow::HttpHeaderCollection c;
		c << "Some-Field: some/value"
		  << "ANOTHER-FIELD: ANOTHER/VALUE"
		  << "third-field: third/VALUE"
		  << "and more: value";

		qint64 dummy = 0;
		const QByteArray am("AND more");
		const char am2[9] = "AND more";
		Pillow::LowerCaseToken am3("and more");
		static const char* and_more = "AND more";

		QBENCHMARK
		{
			for (int i = 0; i < 1000000; ++i)
			{
				//dummy += c.getFieldValue("and more").size();
				//dummy += c.getFieldValue(am).size();
				//dummy += c.getFieldValue(am2).size();
				dummy += c.getFieldValue(am3).size();
				//dummy += c.getFieldValue(and_more, strlen(and_more)).size();
				//dummy += c.getFieldValue(Pillow::LowerCaseToken("and more")).size();
			}
		}

		QVERIFY(dummy > 0);
	}

	void should_allow_testing_whether_the_specified_field_has_the_specified_value_in_any_header_case_insensitively()
	{
		Pillow::HttpHeaderCollection c;
		c << "A: Hello"
		  << "B: World"
		  << "B: !"
		  << "Hello: World";

		QVERIFY(c.testFieldValue("A", "Hello"));
		QVERIFY(c.testFieldValue("A", "heLlO"));
		QVERIFY(c.testFieldValue(QByteArray("B"), "World"));
		QVERIFY(c.testFieldValue(QByteArray("B"), QByteArray("!")));
		QVERIFY(c.testFieldValue("Hello", QByteArray("World")));
		QVERIFY(c.testFieldValue("heLLo", QByteArray("World")));
		QVERIFY(c.testFieldValue("heLLo", QByteArray("WorLD")));
		QVERIFY(c.testFieldValue(Pillow::LowerCaseToken("hello"), QByteArray("WorLD")));

		QVERIFY(!c.testFieldValue("A", "World"));
		QVERIFY(!c.testFieldValue("B", "does not match"));
		QVERIFY(!c.testFieldValue("B", ""));
		QVERIFY(!c.testFieldValue("B", QByteArray()));
		QVERIFY(!c.testFieldValue("does not exist", "World"));
		QVERIFY(!c.testFieldValue("", "World"));
		QVERIFY(!c.testFieldValue(QByteArray(), QByteArray()));
	}

	void benchmark_testFieldValue()
	{
		Pillow::HttpHeaderCollection c;
		c << "Some-Field: some/value"
		  << "ANOTHER-FIELD: ANOTHER/VALUE"
		  << "and more: hello"
		  << "third-field: third/VALUE"
		  << "and more: world!";

		qint64 dummy = 0;
		const QByteArray am("AND more");
		const char am2[9] = "AND more";
		Pillow::LowerCaseToken am3("and more");
		static const char* and_more = "AND more";

		QByteArray world("world!");

		QBENCHMARK
		{
			for (int i = 0; i < 1000000; ++i)
			{
				//dummy += c.getFieldValue("and more").size();
				//dummy += c.testFieldValue("and more", "world!");
				dummy += c.testFieldValue(am, "world!");
				//dummy += c.testFieldValue(am, world);
				//dummy += c.getFieldValue(am2).size();
				//dummy += c.getFieldValue(am3).size();
				//dummy += c.getFieldValue(and_more, strlen(and_more)).size();
				//dummy += c.getFieldValue(Pillow::LowerCaseToken("and more")).size();
			}
		}

		QVERIFY(dummy > 0);
	}

};
PILLOW_TEST_DECLARE(HttpHeaderCollectionTest)

#include "HttpHeaderTest.moc"

