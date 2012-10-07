#include <QtTest/QTest>
#include <QtCore/QObject>
#include "ByteArrayHelpers.h"
#include "HttpConnection.h"
#include "private/ByteArray.h"
#include "Helpers.h"

#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#define REFCOUNT ref.atomic._q_value
#else
#define REFCOUNT ref
#endif


class ByteArrayHelpersTest : public QObject
{
	Q_OBJECT
private slots:
	void test_setFromRawDataAndNullterm()
	{
		char rawData[] = "hello world!";

		QByteArray ba;
		QByteArray::DataPtr oldDataPtr = ba.data_ptr();
		QVERIFY(ba.data_ptr() == oldDataPtr);
		QVERIFY(ba.data_ptr()->REFCOUNT > 1); // Should be the multi-referenced shared null.
		QVERIFY(rawData[5] != '\0');

		// Calling setFromRawDataAndNullterm on a shared byte array should detach it.
		Pillow::ByteArrayHelpers::setFromRawDataAndNullterm(ba, rawData, 0, 5);
		QVERIFY(ba.data_ptr() != oldDataPtr);
		QVERIFY(ba.data_ptr()->REFCOUNT == 1);
		QVERIFY(rawData[5] == '\0');
		QCOMPARE(ba, QByteArray("hello"));

		// Calling setFromRawDataAndNullterm on a non-shared byte array should reuse its data block.
		oldDataPtr = ba.data_ptr();
		Pillow::ByteArrayHelpers::setFromRawDataAndNullterm(ba, rawData, 6, 5);
		QVERIFY(ba.data_ptr() == oldDataPtr);
		QVERIFY(ba.data_ptr()->REFCOUNT == 1);
		QVERIFY(rawData[11] == '\0');
		QCOMPARE(ba, QByteArray("world"));

		// Set it again from a brand new byte array data block that came from another QByteArray.
		QByteArray temp("New Value");
		ba = temp; temp = QByteArray();
		ba = QByteArray("New Value");
		QVERIFY(ba.data_ptr()->REFCOUNT == 1);
		QVERIFY(ba.data_ptr() != oldDataPtr);
		oldDataPtr = ba.data_ptr();
		Pillow::ByteArrayHelpers::setFromRawDataAndNullterm(ba, rawData, 0, 2);
		QVERIFY(ba.data_ptr() == oldDataPtr);
		QVERIFY(ba.data_ptr()->REFCOUNT == 1);
		QVERIFY(rawData[2] == '\0');
		QCOMPARE(ba, QByteArray("he"));

		Pillow::ByteArrayHelpers::setFromRawDataAndNullterm(ba, rawData, 0, 0);
		QVERIFY(ba.data_ptr() == oldDataPtr);
		QVERIFY(ba.data_ptr()->REFCOUNT == 1);
		QVERIFY(rawData[0] != '\0');
		QCOMPARE(ba, QByteArray());
	}

	void test_setFromRawData()
	{
		char rawData[] = "hello world!";

		QByteArray ba;
		QByteArray::DataPtr oldDataPtr = ba.data_ptr();
		ba.setRawData(rawData, 5);
		QVERIFY(oldDataPtr != ba.data_ptr());
		oldDataPtr = ba.data_ptr();
		ba.setRawData(rawData + 6, 5);
		// Start using QByteArray::setRawData directly if the following QVERIFY fails.
		// This would mean that the QByteArray::setRawData behavior has been changed to the
		// desired one where switching from one raw data to another does not cause a
		// QByteArray data block reallocation. Qt5 maybe?
		QVERIFY(oldDataPtr != ba.data_ptr());

		// Go ahead and do tests on our replacement setFromRawData.
		ba = QByteArray();
		oldDataPtr = ba.data_ptr();
		QVERIFY(ba.data_ptr()->REFCOUNT > 1); // Should be the multi-referenced shared null.
		Pillow::ByteArrayHelpers::setFromRawData(ba, rawData, 0, 5);
		QVERIFY(ba.data_ptr() != oldDataPtr);
		QVERIFY(ba.data_ptr()->REFCOUNT == 1);
		QVERIFY(rawData[5] != '\0');
		QCOMPARE(ba, QByteArray("hello"));

		// Calling out setFromRawData on a string that is already detached should not reallocate the
		// QByteArray data block, as opposed to QByteArray::setRawData.
		oldDataPtr = ba.data_ptr();
		Pillow::ByteArrayHelpers::setFromRawData(ba, rawData, 6, 5);
		QVERIFY(ba.data_ptr() == oldDataPtr);
		QVERIFY(ba.data_ptr()->REFCOUNT == 1);
		QVERIFY(rawData[11] != '\0');
		QCOMPARE(ba, QByteArray("world"));
	}

	void test_appendNumber()
	{
		QByteArray ba;

		ba = QByteArray(); Pillow::ByteArrayHelpers::appendNumber<int, 10>(ba, 0);
		QCOMPARE(ba, QByteArray("0"));
		ba = QByteArray(); Pillow::ByteArrayHelpers::appendNumber<int, 10>(ba, Q_UINT64_C(9876));
		QCOMPARE(ba, QByteArray("9876"));
		ba = QByteArray(); Pillow::ByteArrayHelpers::appendNumber<quint64, 10>(ba, Q_UINT64_C(12345678901234));
		QCOMPARE(ba, QByteArray("12345678901234"));
		ba = QByteArray(); Pillow::ByteArrayHelpers::appendNumber<int, 10>(ba, -23456);
		QCOMPARE(ba, QByteArray("-23456"));
		ba = QByteArray(); Pillow::ByteArrayHelpers::appendNumber<qint64, 10>(ba, Q_INT64_C(-12345678901234));
		QCOMPARE(ba, QByteArray("-12345678901234"));

		ba = QByteArray();
		ba.reserve(1024);
		Pillow::ByteArrayHelpers::appendNumber<int, 10>(ba, 0);
		QCOMPARE(ba, QByteArray("0"));
		Pillow::ByteArrayHelpers::appendNumber<int, 10>(ba, Q_UINT64_C(9876));
		QCOMPARE(ba, QByteArray("09876"));
		Pillow::ByteArrayHelpers::appendNumber<quint64, 10>(ba, Q_UINT64_C(12345678901234));
		QCOMPARE(ba, QByteArray("0987612345678901234"));
		Pillow::ByteArrayHelpers::appendNumber<int, 10>(ba, -23456);
		QCOMPARE(ba, QByteArray("0987612345678901234-23456"));
		Pillow::ByteArrayHelpers::appendNumber<qint64, 10>(ba, Q_INT64_C(-12345678901234));
		QCOMPARE(ba, QByteArray("0987612345678901234-23456-12345678901234"));

		ba = QByteArray();
		Pillow::ByteArrayHelpers::appendNumber<int, 16>(ba, 15);
		QCOMPARE(ba, QByteArray("f"));
		Pillow::ByteArrayHelpers::appendNumber<int, 16>(ba, 16);
		QCOMPARE(ba, QByteArray("f10"));
		Pillow::ByteArrayHelpers::appendNumber<int, 16>(ba, 255);
		QCOMPARE(ba, QByteArray("f10ff"));
		Pillow::ByteArrayHelpers::appendNumber<int, 16>(ba, 256);
		QCOMPARE(ba, QByteArray("f10ff100"));
	}

	void test_asciiEqualsCaseInsensitive()
	{
		QVERIFY(Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("", QByteArray("")));
		QVERIFY(Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("hello", QByteArray("hello")));
		QVERIFY(Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("hello", QByteArray("hElLo")));
		QVERIFY(Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("hello123", QByteArray("hElLo123")));
		QVERIFY(Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("12345", QByteArray("12345")));
		QVERIFY(!Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("hello123", QByteArray("hElLo1234")));
		QVERIFY(!Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("hello", QByteArray("World")));
		QVERIFY(!Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("hello", QByteArray("hello\1")));
		QVERIFY(Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("", QLatin1Literal("")));
		QVERIFY(Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("hello", QLatin1Literal("hello")));
		QVERIFY(Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("hello", QLatin1Literal("hElLo")));
		QVERIFY(Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("hello123", QLatin1Literal("hElLo123")));
		QVERIFY(Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("12345", QLatin1Literal("12345")));
		QVERIFY(!Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("hello123", QLatin1Literal("hElLo1234")));
		QVERIFY(!Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("hello", QLatin1Literal("World")));
		QVERIFY(!Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("hello", QLatin1Literal("hello\1")));
	}

	void test_unhex()
	{
		QCOMPARE(Pillow::ByteArrayHelpers::unhex('0'), char(0));
		QCOMPARE(Pillow::ByteArrayHelpers::unhex('6'), char(6));
		QCOMPARE(Pillow::ByteArrayHelpers::unhex('a'), char(10));
		QCOMPARE(Pillow::ByteArrayHelpers::unhex('F'), char(15));
		QCOMPARE(Pillow::ByteArrayHelpers::unhex('g'), char(0));
		QCOMPARE(Pillow::ByteArrayHelpers::unhex('O'), char(0));
		QCOMPARE(Pillow::ByteArrayHelpers::unhex('&'), char(0));
		QCOMPARE(Pillow::ByteArrayHelpers::unhex(0), char(0));
		QCOMPARE(Pillow::ByteArrayHelpers::unhex(1), char(0));
	}

	void test_percentDecode()
	{
		QCOMPARE(Pillow::ByteArrayHelpers::percentDecode(""), QString());
		QCOMPARE(Pillow::ByteArrayHelpers::percentDecode("hello"), QString("hello"));
		QCOMPARE(Pillow::ByteArrayHelpers::percentDecode("hello%20world%3F"), QString("hello world?"));
		QCOMPARE(Pillow::ByteArrayHelpers::percentDecode("hello%20%20world100%25"), QString("hello  world100%"));
		QCOMPARE(Pillow::ByteArrayHelpers::percentDecode("hello%20%20world%2f%2F!"), QString("hello  world//!"));
	}

	void test_byteArray_equals_latin1Literal()
	{
		Pillow::ByteArray ba; ba = QByteArray("Some-string");
		QVERIFY(ba == QLatin1Literal("Some-string"));
		QVERIFY(!(ba == QLatin1Literal("Some-other-string")));
		QVERIFY(ba != QLatin1Literal("Some-other-string"));
		QVERIFY(!(ba != QLatin1Literal("Some-string")));
	}

	void test_byteArray_equals_pillowToken()
	{
		Pillow::ByteArray ba; ba = QByteArray("Some-string");
		QVERIFY(ba == Pillow::Token("Some-string"));
		QVERIFY(!(ba == Pillow::Token("Some-other-string")));
		QVERIFY(ba != Pillow::Token("Some-other-string"));
		QVERIFY(!(ba != Pillow::Token("Some-string")));
	}

	void test_byteArray_equals_pillowLowerCaseToken()
	{
		Pillow::ByteArray ba; ba = QByteArray("some-string");
		QVERIFY(ba == Pillow::LowerCaseToken("some-string"));
		QVERIFY(!(ba == Pillow::LowerCaseToken("some-other-string")));
		QVERIFY(ba != Pillow::LowerCaseToken("some-other-string"));
		QVERIFY(!(ba != Pillow::LowerCaseToken("some-string")));
	}

	void test_byteArray_append_Header()
	{
		Pillow::ByteArray ba;

		// When there is not enough reserved space, append should work but the buffer might change.
		QByteArray::DataPtr d = ba.data_ptr();
		ba.append(Pillow::HttpHeader("Hello", "World"));
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("Hello: World\r\n"));
		QVERIFY(ba.data_ptr() != d);

		// When there is enough reserved space
		ba.clear(); ba.reserve(128);
		d = ba.data_ptr();
		ba.append(Pillow::HttpHeader("Some", "Test"));
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("Some: Test\r\n"));
		QVERIFY(ba.data_ptr() == d);
	}

	void test_byteArray_append_latin1Literal()
	{
		Pillow::ByteArray ba;

		// When there is not enough reserved space, append should work but the buffer might change.
		QByteArray::DataPtr d = ba.data_ptr();
		ba.append(QLatin1Literal("12345678"));
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("12345678"));
		QVERIFY(ba.data_ptr() != d);

		// When there is enough reserved space
		ba.clear(); ba.reserve(10); QVERIFY(ba.capacity() == 10);
		d = ba.data_ptr();
		ba.append(QLatin1Literal("abcdefgh"));
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh"));
		QVERIFY(ba.data_ptr() == d);

		// Trigger a realloc
		ba.append(QLatin1Literal("12345678"));
		QVERIFY(ba.capacity() > 10);
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh12345678"));
	}

	void test_byteArray_append_token()
	{
		Pillow::ByteArray ba;

		// When there is not enough reserved space, append should work but the buffer might change.
		QByteArray::DataPtr d = ba.data_ptr();
		ba.append(Pillow::Token("12345678"));
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("12345678"));
		QVERIFY(ba.data_ptr() != d);


		// When there is enough reserved space
		ba.clear(); ba.reserve(10); QVERIFY(ba.capacity() == 10);
		d = ba.data_ptr();
		ba.append(Pillow::Token("abcdefgh"));
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh"));
		QVERIFY(ba.data_ptr() == d);

		// Trigger a realloc
		ba.append(Pillow::Token("12345678"));
		QVERIFY(ba.capacity() > 10);
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh12345678"));
	}

	void test_byteArray_append_lowerCaseToken()
	{
		Pillow::ByteArray ba;

		// When there is not enough reserved space, append should work but the buffer might change.
		QByteArray::DataPtr d = ba.data_ptr();
		ba.append(Pillow::LowerCaseToken("12345678"));
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("12345678"));
		QVERIFY(ba.data_ptr() != d);

		// When there is enough reserved space
		ba.clear(); ba.reserve(10); QVERIFY(ba.capacity() == 10);
		d = ba.data_ptr();
		ba.append(Pillow::LowerCaseToken("abcdefgh"));
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh"));
		QVERIFY(ba.data_ptr() == d);

		// Trigger a realloc
		ba.append(Pillow::LowerCaseToken("12345678"));
		QVERIFY(ba.capacity() > 10);
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh12345678"));
	}

	void test_byteArray_append_constChar()
	{
		Pillow::ByteArray ba;

		// When there is not enough reserved space, append should work but the buffer might change.
		QByteArray::DataPtr d = ba.data_ptr();
		ba.append("12345678");
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("12345678"));
		QVERIFY(ba.data_ptr() != d);

		// When there is enough reserved space
		ba.clear(); ba.reserve(10); QVERIFY(ba.capacity() == 10);
		d = ba.data_ptr();
		ba.append("abcdefgh");
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh"));
		QVERIFY(ba.data_ptr() == d);

		// Trigger a realloc
		ba.append("12345678");
		QVERIFY(ba.capacity() > 10);
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh12345678"));
	}

	void test_byteArray_append_constCharPtr_len()
	{
		Pillow::ByteArray ba;

		// When there is not enough reserved space, append should work but the buffer might change.
		QByteArray::DataPtr d = ba.data_ptr();
		ba.append("12345678", 4);
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("1234"));
		QVERIFY(ba.data_ptr() != d);

		// When there is enough reserved space
		ba.clear(); ba.reserve(10); QVERIFY(ba.capacity() == 10);
		d = ba.data_ptr();
		ba.append("abcdefgh", 3);
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abc"));
		QVERIFY(ba.data_ptr() == d);

		// Trigger a realloc
		ba.append("12345678", 8);
		QVERIFY(ba.capacity() > 10);
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abc12345678"));
	}

	void test_byteArray_append_char()
	{
		Pillow::ByteArray ba;

		// When there is not enough reserved space, append should work but the buffer might change.
		QByteArray::DataPtr d = ba.data_ptr();
		ba.append('a');
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("a"));
		QVERIFY(ba.data_ptr() != d);

		// When there is enough reserved space
		ba.clear(); ba.reserve(2); QVERIFY(ba.capacity() == 2);
		d = ba.data_ptr();
		ba.append('1');
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("1"));
		QVERIFY(ba.data_ptr() == d);
		ba.append('2');
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("12"));
		QVERIFY(ba.data_ptr() == d);

		// Trigger a realloc
		ba.append('3');
		QVERIFY(ba.capacity() > 2);
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("123"));
	}

};
PILLOW_TEST_DECLARE(ByteArrayHelpersTest)

#include "ByteArrayHelpersTest.moc"
