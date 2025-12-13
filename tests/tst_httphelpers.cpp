#include <QtTest/QTest>
#include <QtCore/QObject>
#include <QtCore/QDateTime>
#include <QtCore/QTimeZone>
#include "HttpHelpers.h"

class tst_HttpHelpers : public QObject
{
	Q_OBJECT

private slots:
	// HttpMimeHelper tests
	void testMimeType_html()
	{
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("test.html"), "text/html");
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("test.htm"), "text/html");
	}

	void testMimeType_css() { QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("style.css"), "text/css"); }

	void testMimeType_javascript() { QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("app.js"), "text/javascript"); }

	void testMimeType_json() { QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("data.json"), "application/json"); }

	void testMimeType_xml() { QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("config.xml"), "text/xml"); }

	void testMimeType_images()
	{
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("photo.png"), "image/png");
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("photo.jpg"), "image/jpeg");
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("photo.jpeg"), "image/jpeg");
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("animation.gif"), "image/gif");
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("favicon.ico"), "image/x-icon");
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("modern.webp"), "image/webp");
	}

	void testMimeType_svg() { QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("icon.svg"), "image/svg+xml"); }

	void testMimeType_fonts()
	{
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("font.woff"), "font/woff");
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("font.woff2"), "font/woff2");
	}

	void testMimeType_text() { QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("readme.txt"), "text/plain"); }

	void testMimeType_unknownExtension()
	{
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("file.xyz"), "application/octet-stream");
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("file.unknown"), "application/octet-stream");
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("file.bin"), "application/octet-stream");
	}

	void testMimeType_noExtension()
	{
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("Makefile"), "application/octet-stream");
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("LICENSE"), "application/octet-stream");
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("somefile"), "application/octet-stream");
	}

	void testMimeType_caseInsensitivity()
	{
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("FILE.HTML"), "text/html");
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("FILE.Html"), "text/html");
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("IMAGE.PNG"), "image/png");
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("STYLE.CSS"), "text/css");
	}

	void testMimeType_pathWithExtension()
	{
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("/path/to/file.html"), "text/html");
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("some/dir/style.css"), "text/css");
	}

	void testMimeType_multipleDotsInFilename()
	{
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("file.min.js"), "text/javascript");
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("archive.tar.gz"), "application/octet-stream");
		QCOMPARE(Pillow::HttpMimeHelper::getMimeTypeForFilename("config.backup.json"), "application/json");
	}

	// HttpProtocol::StatusCodes tests
	void testStatusCodes_informational()
	{
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(100), "100 Continue");
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(101), "101 Switching Protocols");
	}

	void testStatusCodes_success()
	{
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(200), "200 OK");
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(201), "201 Created");
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(204), "204 No Content");
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(206), "206 Partial Content");
	}

	void testStatusCodes_redirection()
	{
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(301), "301 Moved Permanently");
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(302), "302 Found");
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(304), "304 Not Modified");
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(307), "307 Temporary Redirect");
	}

	void testStatusCodes_clientErrors()
	{
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(400), "400 Bad Request");
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(401), "401 Unauthorized");
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(403), "403 Forbidden");
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(404), "404 Not Found");
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(405), "405 Method Not Allowed");
	}

	void testStatusCodes_serverErrors()
	{
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(500), "500 Internal Server Error");
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(502), "502 Bad Gateway");
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(503), "503 Service Unavailable");
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(504), "504 Gateway Timeout");
	}

	void testStatusCodes_unknownCode()
	{
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(999), nullptr);
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(0), nullptr);
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(-1), nullptr);
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusCodeAndMessage(299), nullptr);
	}

	void testStatusMessage_returnsMessageOnly()
	{
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusMessage(200), "OK");
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusMessage(404), "Not Found");
		QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusMessage(500), "Internal Server Error");
	}

	void testStatusMessage_unknownCode() { QCOMPARE(Pillow::HttpProtocol::StatusCodes::getStatusMessage(999), nullptr); }

	// HttpProtocol::Dates tests
	void testHttpDate_format()
	{
		// Test with a known date: Wed, 21 Oct 2015 07:28:00 GMT
		QDateTime dt = QDateTime(QDate(2015, 10, 21), QTime(7, 28, 0), QTimeZone::utc());
		QByteArray httpDate = Pillow::HttpProtocol::Dates::getHttpDate(dt);
		QCOMPARE(httpDate, QByteArray("Wed, 21 Oct 2015 07:28:00 GMT"));
	}

	void testHttpDate_allDays()
	{
		// Monday
		QDateTime mon = QDateTime(QDate(2015, 10, 19), QTime(12, 0, 0), QTimeZone::utc());
		QVERIFY(Pillow::HttpProtocol::Dates::getHttpDate(mon).startsWith("Mon"));

		// Tuesday
		QDateTime tue = QDateTime(QDate(2015, 10, 20), QTime(12, 0, 0), QTimeZone::utc());
		QVERIFY(Pillow::HttpProtocol::Dates::getHttpDate(tue).startsWith("Tue"));

		// Wednesday
		QDateTime wed = QDateTime(QDate(2015, 10, 21), QTime(12, 0, 0), QTimeZone::utc());
		QVERIFY(Pillow::HttpProtocol::Dates::getHttpDate(wed).startsWith("Wed"));

		// Thursday
		QDateTime thu = QDateTime(QDate(2015, 10, 22), QTime(12, 0, 0), QTimeZone::utc());
		QVERIFY(Pillow::HttpProtocol::Dates::getHttpDate(thu).startsWith("Thu"));

		// Friday
		QDateTime fri = QDateTime(QDate(2015, 10, 23), QTime(12, 0, 0), QTimeZone::utc());
		QVERIFY(Pillow::HttpProtocol::Dates::getHttpDate(fri).startsWith("Fri"));

		// Saturday
		QDateTime sat = QDateTime(QDate(2015, 10, 24), QTime(12, 0, 0), QTimeZone::utc());
		QVERIFY(Pillow::HttpProtocol::Dates::getHttpDate(sat).startsWith("Sat"));

		// Sunday
		QDateTime sun = QDateTime(QDate(2015, 10, 25), QTime(12, 0, 0), QTimeZone::utc());
		QVERIFY(Pillow::HttpProtocol::Dates::getHttpDate(sun).startsWith("Sun"));
	}

	void testHttpDate_allMonths()
	{
		const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
		for (int m = 1; m <= 12; ++m)
		{
			QDateTime dt = QDateTime(QDate(2015, m, 15), QTime(12, 0, 0), QTimeZone::utc());
			QByteArray httpDate = Pillow::HttpProtocol::Dates::getHttpDate(dt);
			QVERIFY2(
			    httpDate.contains(months[m - 1]),
			    qPrintable(QString("Month %1 should contain '%2', got '%3'").arg(m).arg(months[m - 1]).arg(QString::fromUtf8(httpDate))));
		}
	}

	void testHttpDate_endsWithGMT()
	{
		QDateTime dt = QDateTime::currentDateTimeUtc();
		QByteArray httpDate = Pillow::HttpProtocol::Dates::getHttpDate(dt);
		QVERIFY(httpDate.endsWith(" GMT"));
	}

	void testHttpDate_convertsToUTC()
	{
		// Create a datetime in a non-UTC timezone
		// Passing local time should produce the same result as the equivalent UTC time
		QDateTime localDt = QDateTime(QDate(2015, 10, 21), QTime(10, 28, 0), QTimeZone("Europe/Paris"));
		QDateTime utcDt = localDt.toUTC();
		QCOMPARE(Pillow::HttpProtocol::Dates::getHttpDate(localDt), Pillow::HttpProtocol::Dates::getHttpDate(utcDt));
	}

	void testHttpDate_paddedDays()
	{
		// Single digit day should be padded
		QDateTime dt = QDateTime(QDate(2015, 1, 5), QTime(12, 0, 0), QTimeZone::utc());
		QByteArray httpDate = Pillow::HttpProtocol::Dates::getHttpDate(dt);
		QVERIFY(httpDate.contains(", 05 Jan"));

		// Double digit day
		dt = QDateTime(QDate(2015, 1, 25), QTime(12, 0, 0), QTimeZone::utc());
		httpDate = Pillow::HttpProtocol::Dates::getHttpDate(dt);
		QVERIFY(httpDate.contains(", 25 Jan"));
	}

	void testHttpDate_paddedTime()
	{
		// Test single digit hours, minutes, seconds are padded
		QDateTime dt = QDateTime(QDate(2015, 1, 15), QTime(5, 8, 3), QTimeZone::utc());
		QByteArray httpDate = Pillow::HttpProtocol::Dates::getHttpDate(dt);
		QVERIFY(httpDate.contains("05:08:03"));
	}
};

QTEST_MAIN(tst_HttpHelpers)
#include "tst_httphelpers.moc"
