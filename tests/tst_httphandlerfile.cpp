#include <QtTest/QtTest>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QCoreApplication>
#include "httphandlerbase.h"
#include "HttpHandler.h"
using namespace Pillow;

class tst_HttpHandlerFile : public HttpHandlerTestBase
{
	Q_OBJECT
	QString testPath;

private slots:
	void initTestCase()
	{
		testPath = QDir::tempPath() + "/HttpHandlerFileTest";
		QDir(testPath).mkpath(".");
		QVERIFY(QFile::exists(testPath));

		QByteArray bigData(16 * 1024 * 1024, '-');

		{ QFile f(testPath + "/first"); f.open(QIODevice::WriteOnly); f.write("first content"); f.flush(); f.close(); }
		{ QFile f(testPath + "/second"); f.open(QIODevice::WriteOnly); f.write("second content"); f.flush(); f.close(); }
		{ QFile f(testPath + "/large"); f.open(QIODevice::WriteOnly); f.write(bigData); f.flush(); f.close(); }
		{ QFile f(testPath + "/first"); f.open(QIODevice::ReadOnly); QCOMPARE(f.readAll(), QByteArray("first content")); }
		{ QFile f(testPath + "/second"); f.open(QIODevice::ReadOnly); QCOMPARE(f.readAll(), QByteArray("second content")); }
		{ QFile f(testPath + "/large"); f.open(QIODevice::ReadOnly); QCOMPARE(f.readAll(), bigData); }
	}

	void testServesFiles()
	{
		HttpHandlerFile handler(testPath);
		QVERIFY(!handler.handleRequest(createGetRequest("/")));
		QVERIFY(!handler.handleRequest(createGetRequest("/bad_path")));
		QVERIFY(!handler.handleRequest(createGetRequest("/another_bad")));

		Pillow::HttpConnection* request = createGetRequest("/first");
		QVERIFY(handler.handleRequest(request));
		QVERIFY(response.startsWith("HTTP/1.0 200 OK"));
		QVERIFY(response.endsWith("first content"));

		response.clear();

		// Note: the large files test currently fails when the output device is a QBuffer.
		request = createGetRequest("/large");
		QVERIFY(handler.handleRequest(request));
		while (response.isEmpty())
			QCoreApplication::processEvents();
		QVERIFY(response.size() > 16 * 1024 * 1024);
		QVERIFY(response.startsWith("HTTP/1.0 200 OK"));
		QVERIFY(response.endsWith(QByteArray(16 * 1024 * 1024, '-')));
	}
};

QTEST_MAIN(tst_HttpHandlerFile)
#include "tst_httphandlerfile.moc"