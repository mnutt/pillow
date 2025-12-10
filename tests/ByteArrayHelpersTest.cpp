#include <QtTest/QTest>
#include <QtCore/QObject>
#include "ByteArrayHelpers.h"
#include "HttpConnection.h"
#include "HttpHeader.h"
#include "Helpers.h"

// Helper function for tests to append HttpHeader to QByteArray
inline QByteArray& appendHttpHeader(QByteArray& ba, const Pillow::HttpHeader& header)
{
	return ba.append(header.first).append(": ", 2).append(header.second).append("\r\n", 2);
}

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
// Qt6 provides isDetached() method
#define IS_SHARED(ba) (!ba.isDetached())
#define IS_NOT_SHARED(ba) (ba.isDetached())
#elif (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#define REFCOUNT ref.atomic._q_value
#define IS_SHARED(ba) (ba.data_ptr()->REFCOUNT > 1)
#define IS_NOT_SHARED(ba) (ba.data_ptr()->REFCOUNT == 1)
#else
#define REFCOUNT ref
#define IS_SHARED(ba) (ba.data_ptr()->REFCOUNT > 1)
#define IS_NOT_SHARED(ba) (ba.data_ptr()->REFCOUNT == 1)
#endif


class ByteArrayHelpersTest : public QObject
{
	Q_OBJECT
private slots:
	void test_setFromRawDataAndNullterm()
	{
		char rawData[] = "hello world!";

		QByteArray ba;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		const char* oldData = ba.constData();
#else
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		const char* oldData = ba.constData();
#else
		auto oldDataPtr = ba.data_ptr();
#endif
#endif
		QVERIFY(IS_SHARED(ba)); // Should be the multi-referenced shared null.
		QVERIFY(rawData[5] != '\0');

		// Calling setFromRawDataAndNullterm on a shared byte array should detach it.
		Pillow::ByteArrayHelpers::setFromRawDataAndNullterm(ba, rawData, 0, 5);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() != oldData); // Data pointer changed
#else
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() != oldData);
#else
		QVERIFY(ba.data_ptr() != oldDataPtr);
#endif
#endif
		QVERIFY(IS_NOT_SHARED(ba));
		QVERIFY(rawData[5] == '\0');
		QCOMPARE(ba, QByteArray("hello"));

		// Calling setFromRawDataAndNullterm on a non-shared byte array should reuse its data block.
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		oldData = ba.constData();
#else
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		oldData = ba.constData();
#else
		oldDataPtr = ba.data_ptr();
#endif
#endif
		Pillow::ByteArrayHelpers::setFromRawDataAndNullterm(ba, rawData, 6, 5);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() == oldData); // Data pointer should be reused
#else
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() == oldData);
#else
		QVERIFY(ba.data_ptr() == oldDataPtr);
#endif
#endif
		QVERIFY(IS_NOT_SHARED(ba));
		QVERIFY(rawData[11] == '\0');
		QCOMPARE(ba, QByteArray("world"));

		// Set it again from a brand new byte array data block that came from another QByteArray.
		QByteArray temp("New Value");
		ba = temp; temp = QByteArray();
		ba = QByteArray("New Value");
		QVERIFY(IS_NOT_SHARED(ba));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() != oldData);
#else
		QVERIFY(ba.data_ptr() != oldDataPtr);
#endif
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		oldData = ba.constData();
#else
		oldDataPtr = ba.data_ptr();
#endif
		Pillow::ByteArrayHelpers::setFromRawDataAndNullterm(ba, rawData, 0, 2);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() == oldData);
#else
		QVERIFY(ba.data_ptr() == oldDataPtr);
#endif
		QVERIFY(IS_NOT_SHARED(ba));
		QVERIFY(rawData[2] == '\0');
		QCOMPARE(ba, QByteArray("he"));

		Pillow::ByteArrayHelpers::setFromRawDataAndNullterm(ba, rawData, 0, 0);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() == oldData);
#else
		QVERIFY(ba.data_ptr() == oldDataPtr);
#endif
		QVERIFY(IS_NOT_SHARED(ba));
		QVERIFY(rawData[0] != '\0');
		QCOMPARE(ba, QByteArray());
	}

	void test_setFromRawData()
	{
		char rawData[] = "hello world!";

		QByteArray ba;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		const char* oldData = ba.constData();
#else
		auto oldDataPtr = ba.data_ptr();
#endif
		ba.setRawData(rawData, 5);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(oldData != ba.constData());
#else
		QVERIFY(oldDataPtr != ba.data_ptr());
#endif
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		oldData = ba.constData();
#else
		oldDataPtr = ba.data_ptr();
#endif
		ba.setRawData(rawData + 6, 5);
		// Start using QByteArray::setRawData directly if the following QVERIFY fails.
		// This would mean that the QByteArray::setRawData behavior has been changed to the
		// desired one where switching from one raw data to another does not cause a
		// QByteArray data block reallocation. Qt5 maybe?
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(oldData != ba.constData());
#else
		QVERIFY(oldDataPtr != ba.data_ptr());
#endif

		// Go ahead and do tests on our replacement setFromRawData.
		ba = QByteArray();
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		oldData = ba.constData();
#else
		oldDataPtr = ba.data_ptr();
#endif
		QVERIFY(IS_SHARED(ba)); // Should be the multi-referenced shared null.
		Pillow::ByteArrayHelpers::setFromRawData(ba, rawData, 0, 5);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() != oldData);
#else
		QVERIFY(ba.data_ptr() != oldDataPtr);
#endif
		QVERIFY(IS_NOT_SHARED(ba));
		QVERIFY(rawData[5] != '\0');
		QCOMPARE(ba, QByteArray("hello"));

		// Calling out setFromRawData on a string that is already detached should not reallocate the
		// QByteArray data block, as opposed to QByteArray::setRawData.
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		oldData = ba.constData();
#else
		oldDataPtr = ba.data_ptr();
#endif
		Pillow::ByteArrayHelpers::setFromRawData(ba, rawData, 6, 5);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() == oldData);
#else
		QVERIFY(ba.data_ptr() == oldDataPtr);
#endif
		QVERIFY(IS_NOT_SHARED(ba));
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
		QVERIFY(Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("", QLatin1String("")));
		QVERIFY(Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("hello", QLatin1String("hello")));
		QVERIFY(Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("hello", QLatin1String("hElLo")));
		QVERIFY(Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("hello123", QLatin1String("hElLo123")));
		QVERIFY(Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("12345", QLatin1String("12345")));
		QVERIFY(!Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("hello123", QLatin1String("hElLo1234")));
		QVERIFY(!Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("hello", QLatin1String("World")));
		QVERIFY(!Pillow::ByteArrayHelpers::asciiEqualsCaseInsensitive("hello", QLatin1String("hello\1")));
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
		QByteArray ba; ba = QByteArray("Some-string");
		QVERIFY(ba == QLatin1String("Some-string"));
		QVERIFY(!(ba == QLatin1String("Some-other-string")));
		QVERIFY(ba != QLatin1String("Some-other-string"));
		QVERIFY(!(ba != QLatin1String("Some-string")));
	}

	void test_byteArray_equals_pillowToken()
	{
		QByteArray ba; ba = QByteArray("Some-string");
		QVERIFY(ba == Pillow::Token("Some-string"));
		QVERIFY(!(ba == Pillow::Token("Some-other-string")));
		QVERIFY(ba != Pillow::Token("Some-other-string"));
		QVERIFY(!(ba != Pillow::Token("Some-string")));
	}

	void test_byteArray_equals_pillowLowerCaseToken()
	{
		QByteArray ba; ba = QByteArray("some-string");
		QVERIFY(ba == Pillow::LowerCaseToken("some-string"));
		QVERIFY(!(ba == Pillow::LowerCaseToken("some-other-string")));
		QVERIFY(ba != Pillow::LowerCaseToken("some-other-string"));
		QVERIFY(!(ba != Pillow::LowerCaseToken("some-string")));
	}

	void test_byteArray_append_Header()
	{
		QByteArray ba;

		// When there is not enough reserved space, append should work but the buffer might change.
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		const char* d = ba.constData();
#else
		auto d = ba.data_ptr();
#endif
		appendHttpHeader(ba, Pillow::HttpHeader("Hello", "World"));
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("Hello: World\r\n"));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() != d);
#else
		QVERIFY(ba.data_ptr() != d);
#endif

		// When there is enough reserved space
		ba.clear(); ba.reserve(128);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		d = ba.constData();
#else
		d = ba.data_ptr();
#endif
		appendHttpHeader(ba, Pillow::HttpHeader("Some", "Test"));
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("Some: Test\r\n"));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() == d);
#else
		QVERIFY(ba.data_ptr() == d);
#endif
	}

	void test_byteArray_append_latin1Literal()
	{
		QByteArray ba;

		// When there is not enough reserved space, append should work but the buffer might change.
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		const char* d = ba.constData();
#else
		auto d = ba.data_ptr();
#endif
		ba.append(QLatin1String("12345678"));
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("12345678"));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() != d);
#else
		QVERIFY(ba.data_ptr() != d);
#endif

		// When there is enough reserved space
		ba.clear(); ba.reserve(10); QVERIFY(ba.capacity() == 10);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		d = ba.constData();
#else
		d = ba.data_ptr();
#endif
		ba.append(QLatin1String("abcdefgh"));
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh"));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() == d);
#else
		QVERIFY(ba.data_ptr() == d);
#endif

		// Trigger a realloc
		ba.append(QLatin1String("12345678"));
		QVERIFY(ba.capacity() > 10);
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh12345678"));
	}

	void test_byteArray_append_token()
	{
		QByteArray ba;

		// When there is not enough reserved space, append should work but the buffer might change.
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		const char* d = ba.constData();
#else
		auto d = ba.data_ptr();
#endif
		ba.append(Pillow::Token("12345678"));
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("12345678"));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() != d);
#else
		QVERIFY(ba.data_ptr() != d);
#endif


		// When there is enough reserved space
		ba.clear(); ba.reserve(10); QVERIFY(ba.capacity() == 10);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		d = ba.constData();
#else
		d = ba.data_ptr();
#endif
		ba.append(Pillow::Token("abcdefgh"));
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh"));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() == d);
#else
		QVERIFY(ba.data_ptr() == d);
#endif

		// Trigger a realloc
		ba.append(Pillow::Token("12345678"));
		QVERIFY(ba.capacity() > 10);
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh12345678"));
	}

	void test_byteArray_append_lowerCaseToken()
	{
		QByteArray ba;

		// When there is not enough reserved space, append should work but the buffer might change.
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		const char* d = ba.constData();
#else
		auto d = ba.data_ptr();
#endif
		ba.append(Pillow::LowerCaseToken("12345678"));
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("12345678"));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() != d);
#else
		QVERIFY(ba.data_ptr() != d);
#endif

		// When there is enough reserved space
		ba.clear(); ba.reserve(10); QVERIFY(ba.capacity() == 10);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		d = ba.constData();
#else
		d = ba.data_ptr();
#endif
		ba.append(Pillow::LowerCaseToken("abcdefgh"));
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh"));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() == d);
#else
		QVERIFY(ba.data_ptr() == d);
#endif

		// Trigger a realloc
		ba.append(Pillow::LowerCaseToken("12345678"));
		QVERIFY(ba.capacity() > 10);
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh12345678"));
	}

	void test_byteArray_append_constChar()
	{
		QByteArray ba;

		// When there is not enough reserved space, append should work but the buffer might change.
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		const char* d = ba.constData();
#else
		auto d = ba.data_ptr();
#endif
		ba.append("12345678");
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("12345678"));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() != d);
#else
		QVERIFY(ba.data_ptr() != d);
#endif

		// When there is enough reserved space
		ba.clear(); ba.reserve(10); QVERIFY(ba.capacity() == 10);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		d = ba.constData();
#else
		d = ba.data_ptr();
#endif
		ba.append("abcdefgh");
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh"));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() == d);
#else
		QVERIFY(ba.data_ptr() == d);
#endif

		// Trigger a realloc
		ba.append("12345678");
		QVERIFY(ba.capacity() > 10);
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh12345678"));
	}

	void test_byteArray_append_constCharPtr_len()
	{
		QByteArray ba;

		// When there is not enough reserved space, append should work but the buffer might change.
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		const char* d = ba.constData();
#else
		auto d = ba.data_ptr();
#endif
		ba.append("12345678", 4);
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("1234"));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() != d);
#else
		QVERIFY(ba.data_ptr() != d);
#endif

		// When there is enough reserved space
		ba.clear(); ba.reserve(10); QVERIFY(ba.capacity() == 10);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		d = ba.constData();
#else
		d = ba.data_ptr();
#endif
		ba.append("abcdefgh", 3);
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abc"));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() == d);
#else
		QVERIFY(ba.data_ptr() == d);
#endif

		// Trigger a realloc
		ba.append("12345678", 8);
		QVERIFY(ba.capacity() > 10);
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abc12345678"));
	}

	void test_byteArray_append_char()
	{
		QByteArray ba;

		// When there is not enough reserved space, append should work but the buffer might change.
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		const char* d = ba.constData();
#else
		auto d = ba.data_ptr();
#endif
		ba.append('a');
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("a"));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() != d);
#else
		QVERIFY(ba.data_ptr() != d);
#endif

		// When there is enough reserved space
		ba.clear(); ba.reserve(2); QVERIFY(ba.capacity() == 2);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		d = ba.constData();
#else
		d = ba.data_ptr();
#endif
		ba.append('1');
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("1"));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() == d);
#else
		QVERIFY(ba.data_ptr() == d);
#endif
		ba.append('2');
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("12"));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QVERIFY(ba.constData() == d);
#else
		QVERIFY(ba.data_ptr() == d);
#endif

		// Trigger a realloc
		ba.append('3');
		QVERIFY(ba.capacity() > 2);
		QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("123"));
	}

};
PILLOW_TEST_DECLARE(ByteArrayHelpersTest)

#include "ByteArrayHelpersTest.moc"
