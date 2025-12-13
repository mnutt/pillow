#include <QtTest/QtTest>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>
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

        // Create subdirectory for testing
        QDir(testPath + "/subdir").mkpath(".");

        QByteArray bigData(16 * 1024 * 1024, '-');

        {
            QFile f(testPath + "/first");
            f.open(QIODevice::WriteOnly);
            f.write("first content");
            f.flush();
            f.close();
        }
        {
            QFile f(testPath + "/second");
            f.open(QIODevice::WriteOnly);
            f.write("second content");
            f.flush();
            f.close();
        }
        {
            QFile f(testPath + "/large");
            f.open(QIODevice::WriteOnly);
            f.write(bigData);
            f.flush();
            f.close();
        }
        {
            QFile f(testPath + "/first");
            f.open(QIODevice::ReadOnly);
            QCOMPARE(f.readAll(), QByteArray("first content"));
        }
        {
            QFile f(testPath + "/second");
            f.open(QIODevice::ReadOnly);
            QCOMPARE(f.readAll(), QByteArray("second content"));
        }
        {
            QFile f(testPath + "/large");
            f.open(QIODevice::ReadOnly);
            QCOMPARE(f.readAll(), bigData);
        }

        // Create files for edge case tests
        {
            QFile f(testPath + "/empty");
            f.open(QIODevice::WriteOnly);
            f.close();
        }
        {
            QFile f(testPath + "/test.html");
            f.open(QIODevice::WriteOnly);
            f.write("<html></html>");
            f.close();
        }
        {
            QFile f(testPath + "/style.css");
            f.open(QIODevice::WriteOnly);
            f.write("body {}");
            f.close();
        }
        {
            QFile f(testPath + "/no_extension");
            f.open(QIODevice::WriteOnly);
            f.write("no ext content");
            f.close();
        }
        {
            QFile f(testPath + "/file with spaces.txt");
            f.open(QIODevice::WriteOnly);
            f.write("spaces content");
            f.close();
        }
        {
            QFile f(testPath + "/special%char.txt");
            f.open(QIODevice::WriteOnly);
            f.write("special content");
            f.close();
        }
        {
            QFile f(testPath + "/subdir/nested.txt");
            f.open(QIODevice::WriteOnly);
            f.write("nested content");
            f.close();
        }
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

    void test404ForNonExistentFile()
    {
        HttpHandlerFile handler(testPath);

        // Handler returns false for non-existent files (allowing other handlers to try)
        QVERIFY(!handler.handleRequest(createGetRequest("/does_not_exist")));
        QVERIFY(!handler.handleRequest(createGetRequest("/also_missing.txt")));
        QVERIFY(!handler.handleRequest(createGetRequest("/subdir/missing.txt")));
    }

    void testDirectoryTraversalPrevention()
    {
        HttpHandlerFile handler(testPath);

        // Attempts to escape the public path should fail
        QVERIFY(!handler.handleRequest(createGetRequest("/../etc/passwd")));
        QVERIFY(!handler.handleRequest(createGetRequest("/subdir/../../etc/passwd")));
        QVERIFY(!handler.handleRequest(createGetRequest("/..%2F..%2Fetc%2Fpasswd")));
        QVERIFY(!handler.handleRequest(createGetRequest("/%2e%2e/%2e%2e/etc/passwd")));
    }

    void testCorrectMimeTypeInHeader()
    {
        HttpHandlerFile handler(testPath);

        // HTML file
        response.clear();
        QVERIFY(handler.handleRequest(createGetRequest("/test.html")));
        QVERIFY(response.contains("Content-Type: text/html"));

        // CSS file
        response.clear();
        QVERIFY(handler.handleRequest(createGetRequest("/style.css")));
        QVERIFY(response.contains("Content-Type: text/css"));

        // File without extension
        response.clear();
        QVERIFY(handler.handleRequest(createGetRequest("/no_extension")));
        QVERIFY(response.contains("Content-Type: application/octet-stream"));
    }

    void testEmptyFile()
    {
        HttpHandlerFile handler(testPath);

        response.clear();
        QVERIFY(handler.handleRequest(createGetRequest("/empty")));
        QVERIFY(response.startsWith("HTTP/1.0 200 OK"));
        // Empty file should have Content-Length: 0 or just end after headers
        QVERIFY(response.contains("Content-Length: 0") || response.endsWith("\r\n\r\n"));
    }

    void testUrlEncodedFilenames()
    {
        HttpHandlerFile handler(testPath);

        // File with spaces - URL encoded
        response.clear();
        QVERIFY(handler.handleRequest(createGetRequest("/file%20with%20spaces.txt")));
        QVERIFY(response.startsWith("HTTP/1.0 200 OK"));
        QVERIFY(response.endsWith("spaces content"));
    }

    void testNestedDirectories()
    {
        HttpHandlerFile handler(testPath);

        response.clear();
        QVERIFY(handler.handleRequest(createGetRequest("/subdir/nested.txt")));
        QVERIFY(response.startsWith("HTTP/1.0 200 OK"));
        QVERIFY(response.endsWith("nested content"));
    }

    void testDirectoryRequestReturns404()
    {
        HttpHandlerFile handler(testPath);

        // Requesting a directory should fail (not serve directory listing)
        QVERIFY(!handler.handleRequest(createGetRequest("/subdir")));
        QVERIFY(!handler.handleRequest(createGetRequest("/subdir/")));
    }

    void testEmptyPublicPath()
    {
        HttpHandlerFile handler("");

        // Empty public path should not serve anything
        QVERIFY(!handler.handleRequest(createGetRequest("/first")));
        QVERIFY(!handler.handleRequest(createGetRequest("/etc/passwd")));
    }

    void testEtagCaching()
    {
        HttpHandlerFile handler(testPath);

        // First request should return 200 with ETag
        response.clear();
        QVERIFY(handler.handleRequest(createGetRequest("/first")));
        QVERIFY(response.startsWith("HTTP/1.0 200 OK"));
        QVERIFY(response.contains("ETag: "));

        // Extract ETag from response
        int etagStart = response.indexOf("ETag: ") + 6;
        int etagEnd = response.indexOf("\r\n", etagStart);
        QByteArray etag = response.mid(etagStart, etagEnd - etagStart);
        QVERIFY(!etag.isEmpty());

        // Calculate expected ETag (MD5 of content)
        QCryptographicHash md5(QCryptographicHash::Md5);
        md5.addData("first content");
        QByteArray expectedEtag = md5.result().toHex();
        QCOMPARE(etag, expectedEtag);
    }

    void testBufferSizeConfiguration()
    {
        HttpHandlerFile handler(testPath);

        // Default buffer size
        QCOMPARE(handler.bufferSize(), static_cast<int>(HttpHandlerFile::DefaultBufferSize));

        // Set custom buffer size
        handler.setBufferSize(1024);
        QCOMPARE(handler.bufferSize(), 1024);
    }

    void testPublicPathConfiguration()
    {
        HttpHandlerFile handler;

        // Empty initially if not set
        QVERIFY(handler.publicPath().isEmpty());

        // Set public path
        handler.setPublicPath(testPath);
        QCOMPARE(handler.publicPath(), testPath);

        // Now it should serve files
        response.clear();
        QVERIFY(handler.handleRequest(createGetRequest("/first")));
        QVERIFY(response.startsWith("HTTP/1.0 200 OK"));
    }
};

QTEST_MAIN(tst_HttpHandlerFile)
#include "tst_httphandlerfile.moc"