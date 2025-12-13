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

// Qt6 provides isDetached() method
#define IS_SHARED(ba) (!ba.isDetached())
#define IS_NOT_SHARED(ba) (ba.isDetached())

class tst_ByteArrayHelpers : public QObject
{
    Q_OBJECT
private slots:
    void test_setFromRawDataAndNullterm()
    {
        char rawData[] = "hello world!";

        QByteArray ba;
        const char* oldData = ba.constData();
        // An empty QByteArray might already be detached, so we need to ensure it's shared
        QByteArray ba2 = ba;    // Create a copy to ensure sharing
        QVERIFY(IS_SHARED(ba)); // Should be the multi-referenced shared null.
        QVERIFY(rawData[5] != '\0');

        // Calling setFromRawDataAndNullterm sets ba to point directly into rawData (zero-copy).
        Pillow::ByteArrayHelpers::setFromRawDataAndNullterm(ba, rawData, 0, 5);
        QVERIFY(ba.constData() != oldData); // Data pointer changed from empty string
        QVERIFY(ba.constData() == rawData); // Zero-copy: points directly to rawData
        // In Qt6, setRawData creates a QByteArray that is "shared" with external data
        // (isDetached() returns false), which is correct behavior.
        QVERIFY(IS_SHARED(ba));
        // Verify null termination happened in the source buffer
        QVERIFY(rawData[5] == '\0');
        QCOMPARE(ba, QByteArray("hello"));

        // Calling setFromRawDataAndNullterm again points to a different part of rawData.
        // In Qt6, this creates a new reference (no data block reuse optimization).
        Pillow::ByteArrayHelpers::setFromRawDataAndNullterm(ba, rawData, 6, 5);
        QVERIFY(ba.constData() == rawData + 6); // Zero-copy: points to rawData + 6
        QVERIFY(IS_SHARED(ba));                 // Still shared with external buffer
        QVERIFY(rawData[11] == '\0');           // Null termination happened
        QCOMPARE(ba, QByteArray("world"));

        // Set from a different starting point
        Pillow::ByteArrayHelpers::setFromRawDataAndNullterm(ba, rawData, 0, 2);
        QVERIFY(ba.constData() == rawData); // Zero-copy
        QVERIFY(rawData[2] == '\0');        // Null termination
        QCOMPARE(ba, QByteArray("he"));

        // Empty case
        Pillow::ByteArrayHelpers::setFromRawDataAndNullterm(ba, rawData, 0, 0);
        QCOMPARE(ba, QByteArray());
    }

    void test_setFromRawData()
    {
        char rawData[] = "hello world!";

        // First, verify Qt's setRawData behavior - each call points to new data
        QByteArray ba;
        const char* oldData = ba.constData();
        ba.setRawData(rawData, 5);
        QVERIFY(oldData != ba.constData());
        QVERIFY(ba.constData() == rawData); // Points directly to rawData
        oldData = ba.constData();
        ba.setRawData(rawData + 6, 5);
        QVERIFY(oldData != ba.constData());     // Qt6 creates new reference each time
        QVERIFY(ba.constData() == rawData + 6); // Points to new location

        // Test our setFromRawData wrapper - should have same zero-copy behavior
        ba = QByteArray();
        QByteArray ba2 = ba;    // Create a copy to ensure sharing
        QVERIFY(IS_SHARED(ba)); // Should be the multi-referenced shared null.

        Pillow::ByteArrayHelpers::setFromRawData(ba, rawData, 0, 5);
        QVERIFY(ba.constData() == rawData); // Zero-copy: points directly to rawData
        // In Qt6, a QByteArray using setRawData is "shared" with external buffer
        QVERIFY(IS_SHARED(ba));
        QVERIFY(rawData[5] != '\0'); // setFromRawData does NOT null-terminate
        QCOMPARE(ba, QByteArray("hello"));

        // Calling setFromRawData again points to a different location
        Pillow::ByteArrayHelpers::setFromRawData(ba, rawData, 6, 5);
        QVERIFY(ba.constData() == rawData + 6); // Zero-copy
        QVERIFY(IS_SHARED(ba));                 // Still shared with external buffer
        QVERIFY(rawData[11] != '\0');           // No null termination
        QCOMPARE(ba, QByteArray("world"));
    }

    void test_appendNumber()
    {
        QByteArray ba;

        ba = QByteArray();
        Pillow::ByteArrayHelpers::appendNumber<int, 10>(ba, 0);
        QCOMPARE(ba, QByteArray("0"));
        ba = QByteArray();
        Pillow::ByteArrayHelpers::appendNumber<int, 10>(ba, Q_UINT64_C(9876));
        QCOMPARE(ba, QByteArray("9876"));
        ba = QByteArray();
        Pillow::ByteArrayHelpers::appendNumber<quint64, 10>(ba, Q_UINT64_C(12345678901234));
        QCOMPARE(ba, QByteArray("12345678901234"));
        ba = QByteArray();
        Pillow::ByteArrayHelpers::appendNumber<int, 10>(ba, -23456);
        QCOMPARE(ba, QByteArray("-23456"));
        ba = QByteArray();
        Pillow::ByteArrayHelpers::appendNumber<qint64, 10>(ba, Q_INT64_C(-12345678901234));
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
        QByteArray ba;
        ba = QByteArray("Some-string");
        QVERIFY(ba == QLatin1String("Some-string"));
        QVERIFY(!(ba == QLatin1String("Some-other-string")));
        QVERIFY(ba != QLatin1String("Some-other-string"));
        QVERIFY(!(ba != QLatin1String("Some-string")));
    }

    void test_byteArray_equals_pillowToken()
    {
        QByteArray ba;
        ba = QByteArray("Some-string");
        QVERIFY(ba == Pillow::Token("Some-string"));
        QVERIFY(!(ba == Pillow::Token("Some-other-string")));
        QVERIFY(ba != Pillow::Token("Some-other-string"));
        QVERIFY(!(ba != Pillow::Token("Some-string")));
    }

    void test_byteArray_equals_pillowLowerCaseToken()
    {
        QByteArray ba;
        ba = QByteArray("some-string");
        QVERIFY(ba == Pillow::LowerCaseToken("some-string"));
        QVERIFY(!(ba == Pillow::LowerCaseToken("some-other-string")));
        QVERIFY(ba != Pillow::LowerCaseToken("some-other-string"));
        QVERIFY(!(ba != Pillow::LowerCaseToken("some-string")));
    }

    void test_byteArray_append_Header()
    {
        QByteArray ba;

        // When there is not enough reserved space, append should work but the buffer might change.
        const char* d = ba.constData();
        appendHttpHeader(ba, Pillow::HttpHeader("Hello", "World"));
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("Hello: World\r\n"));
        QVERIFY(ba.constData() != d);

        // When there is enough reserved space
        ba.clear();
        ba.reserve(128);
        d = ba.constData();
        appendHttpHeader(ba, Pillow::HttpHeader("Some", "Test"));
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("Some: Test\r\n"));
        QVERIFY(ba.constData() == d);
    }

    void test_byteArray_append_latin1Literal()
    {
        QByteArray ba;

        // When there is not enough reserved space, append should work but the buffer might change.
        const char* d = ba.constData();
        ba.append(QLatin1String("12345678"));
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("12345678"));
        QVERIFY(ba.constData() != d);

        // When there is enough reserved space
        ba.clear();
        ba.reserve(10);
        QVERIFY(ba.capacity() == 10);
        d = ba.constData();
        ba.append(QLatin1String("abcdefgh"));
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh"));
        QVERIFY(ba.constData() == d);

        // Trigger a realloc
        ba.append(QLatin1String("12345678"));
        QVERIFY(ba.capacity() > 10);
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh12345678"));
    }

    void test_byteArray_append_token()
    {
        QByteArray ba;

        // When there is not enough reserved space, append should work but the buffer might change.
        const char* d = ba.constData();
        ba.append(Pillow::Token("12345678"));
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("12345678"));
        QVERIFY(ba.constData() != d);

        // When there is enough reserved space
        ba.clear();
        ba.reserve(10);
        QVERIFY(ba.capacity() == 10);
        d = ba.constData();
        ba.append(Pillow::Token("abcdefgh"));
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh"));
        QVERIFY(ba.constData() == d);

        // Trigger a realloc
        ba.append(Pillow::Token("12345678"));
        QVERIFY(ba.capacity() > 10);
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh12345678"));
    }

    void test_byteArray_append_lowerCaseToken()
    {
        QByteArray ba;

        // When there is not enough reserved space, append should work but the buffer might change.
        const char* d = ba.constData();
        ba.append(Pillow::LowerCaseToken("12345678"));
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("12345678"));
        QVERIFY(ba.constData() != d);

        // When there is enough reserved space
        ba.clear();
        ba.reserve(10);
        QVERIFY(ba.capacity() == 10);
        d = ba.constData();
        ba.append(Pillow::LowerCaseToken("abcdefgh"));
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh"));
        QVERIFY(ba.constData() == d);

        // Trigger a realloc
        ba.append(Pillow::LowerCaseToken("12345678"));
        QVERIFY(ba.capacity() > 10);
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh12345678"));
    }

    void test_byteArray_append_constChar()
    {
        QByteArray ba;

        // When there is not enough reserved space, append should work but the buffer might change.
        const char* d = ba.constData();
        ba.append("12345678");
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("12345678"));
        QVERIFY(ba.constData() != d);

        // When there is enough reserved space
        ba.clear();
        ba.reserve(10);
        QVERIFY(ba.capacity() == 10);
        d = ba.constData();
        ba.append("abcdefgh");
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh"));
        QVERIFY(ba.constData() == d);

        // Trigger a realloc
        ba.append("12345678");
        QVERIFY(ba.capacity() > 10);
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abcdefgh12345678"));
    }

    void test_byteArray_append_constCharPtr_len()
    {
        QByteArray ba;

        // When there is not enough reserved space, append should work but the buffer might change.
        const char* d = ba.constData();
        ba.append("12345678", 4);
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("1234"));
        QVERIFY(ba.constData() != d);

        // When there is enough reserved space
        ba.clear();
        ba.reserve(10);
        QVERIFY(ba.capacity() == 10);
        d = ba.constData();
        ba.append("abcdefgh", 3);
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abc"));
        QVERIFY(ba.constData() == d);

        // Trigger a realloc
        ba.append("12345678", 8);
        QVERIFY(ba.capacity() > 10);
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("abc12345678"));
    }

    void test_byteArray_append_char()
    {
        QByteArray ba;

        // When there is not enough reserved space, append should work but the buffer might change.
        const char* d = ba.constData();
        ba.append('a');
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("a"));
        QVERIFY(ba.constData() != d);

        // When there is enough reserved space
        ba.clear();
        ba.reserve(2);
        QVERIFY(ba.capacity() == 2);
        d = ba.constData();
        ba.append('1');
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("1"));
        QVERIFY(ba.constData() == d);
        ba.append('2');
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("12"));
        QVERIFY(ba.constData() == d);

        // Trigger a realloc
        ba.append('3');
        QVERIFY(ba.capacity() > 2);
        QCOMPARE(static_cast<QByteArray&>(ba), QByteArray("123"));
    }
};

QTEST_MAIN(tst_ByteArrayHelpers)
#include "tst_bytearrayhelpers.moc"
