#include <QtTest/QtTest>
#include <QtCore/QBuffer>
#include "httphandlerbase.h"
#include "HttpHandler.h"
using namespace Pillow;

class MockHandler : public Pillow::HttpHandler
{
public:
    MockHandler(const QByteArray& acceptPath, int statusCode, QObject* parent)
        : Pillow::HttpHandler(parent), acceptPath(acceptPath), statusCode(statusCode), handleRequestCount(0)
    {}

    QByteArray acceptPath;
    int statusCode;
    int handleRequestCount;

    bool handleRequest(Pillow::HttpConnection* connection)
    {
        ++handleRequestCount;

        if (acceptPath == connection->requestPath())
        {
            connection->writeResponse(statusCode);
            return true;
        }
        return false;
    }
};

class tst_HttpHandler : public HttpHandlerTestBase
{
    Q_OBJECT

private slots:
    void testHandlerStack()
    {
        HttpHandlerStack handler;
        MockHandler* mock1 = new MockHandler("/1", 200, &handler);
        MockHandler* mock1_1 = new MockHandler("/", 403, mock1);
        new QObject(&handler); // Some dummy object, also child of handler.
        MockHandler* mock2 = new MockHandler("/2", 302, &handler);
        MockHandler* mock3 = new MockHandler("/", 500, &handler);
        MockHandler* mock4 = new MockHandler("/", 200, &handler);

        bool handled = handler.handleRequest(createGetRequest("/"));
        QVERIFY(handled);
        QVERIFY(response.startsWith("HTTP/1.0 500"));
        QCOMPARE(mock1->handleRequestCount, 1);
        QCOMPARE(mock1_1->handleRequestCount, 0);
        QCOMPARE(mock2->handleRequestCount, 1);
        QCOMPARE(mock3->handleRequestCount, 1);
        QCOMPARE(mock4->handleRequestCount, 0);

        handled = handler.handleRequest(createGetRequest("/2"));
        QVERIFY(handled);
        QVERIFY(response.startsWith("HTTP/1.0 302"));
        QCOMPARE(mock1->handleRequestCount, 2);
        QCOMPARE(mock1_1->handleRequestCount, 0);
        QCOMPARE(mock2->handleRequestCount, 2);
        QCOMPARE(mock3->handleRequestCount, 1);
        QCOMPARE(mock4->handleRequestCount, 0);
    }

    void testHandlerFixed()
    {
        bool handled = HttpHandlerFixed(403, "Fixed test").handleRequest(createGetRequest());
        QVERIFY(handled);
        QVERIFY(response.startsWith("HTTP/1.0 403"));
        QVERIFY(response.endsWith("\r\n\r\nFixed test"));
    }

    void testHandler404()
    {
        bool handled = HttpHandler404().handleRequest(createGetRequest("/some_path"));
        QVERIFY(handled);
        QVERIFY(response.startsWith("HTTP/1.0 404"));
        QVERIFY(response.contains("The requested resource does not exist on this server"));
    }

    void testHandlerFunction()
    {
#ifdef Q_COMPILER_LAMBDA
        HttpHandlerFunction handler(
            [](Pillow::HttpConnection* request) { request->writeResponse(200, Pillow::HttpHeaderCollection(), "hello from lambda"); });

        QVERIFY(handler.handleRequest(createGetRequest("/some/random/path")));
        QVERIFY(response.startsWith("HTTP/1.0 200 OK"));
        QVERIFY(response.endsWith("hello from lambda"));
#else
        QSKIP("Compiler does not support lambdas or C++0x support is not enabled.", SkipSingle);
#endif
    }

    void testHandlerLog()
    {
        QBuffer buffer;
        buffer.open(QIODevice::ReadWrite);
        Pillow::HttpConnection* request1 = createGetRequest("/first");
        Pillow::HttpConnection* request2 = createGetRequest("/second");
        Pillow::HttpConnection* request3 = createGetRequest("/third");

        HttpHandlerLog handler(&buffer);
        QVERIFY(!handler.handleRequest(request1));
        QVERIFY(!handler.handleRequest(request2));
        QVERIFY(!handler.handleRequest(request3));
        QVERIFY(buffer.data().isEmpty());
        request3->writeResponse(302);
        request1->writeResponse(200);
        request2->writeResponse(500);

        // The log handler should write the log entries as they are completed.
        buffer.seek(0);
        QVERIFY(buffer.readLine().contains("GET /third"));
        QVERIFY(buffer.readLine().contains("GET /first"));
        QVERIFY(buffer.readLine().contains("GET /second"));
        QVERIFY(buffer.readLine().isEmpty());
    }

    void testHandlerLogTrace()
    {
        QBuffer buffer;
        buffer.open(QIODevice::ReadWrite);
        Pillow::HttpConnection* request1 = createGetRequest("/first", "1.1");
        Pillow::HttpConnection* request2 = createGetRequest("/second", "1.1");
        Pillow::HttpConnection* request3 = createGetRequest("/third", "1.1");

        HttpHandlerLog handler(&buffer);
        handler.setMode(HttpHandlerLog::TraceRequests);

        QVERIFY(!handler.handleRequest(request1));
        QVERIFY(!buffer.data().isEmpty());
        QVERIFY(buffer.data().contains("[BEGIN]"));
        QVERIFY(!buffer.data().contains("[ END ]"));
        QVERIFY(!handler.handleRequest(request2));
        QVERIFY(!handler.handleRequest(request3));
        request3->writeResponse(302);
        QVERIFY(buffer.data().contains("[ END ]"));
        request1->writeResponse(200);
        request2->writeResponse(500);
        QVERIFY(!buffer.data().contains("[CLOSE]"));
        request1->close();
        QVERIFY(buffer.data().contains("[CLOSE]"));

        buffer.seek(0);
        QVERIFY(buffer.readLine().contains("GET /first"));
        QVERIFY(buffer.readLine().contains("GET /second"));
        QVERIFY(buffer.readLine().contains("GET /third"));
        QVERIFY(buffer.readLine().contains("GET /third"));  // END
        QVERIFY(buffer.readLine().contains("GET /first"));  // END
        QVERIFY(buffer.readLine().contains("GET /second")); // END
        QVERIFY(buffer.readLine().contains("GET /first"));  // CLOSE
        QVERIFY(buffer.readLine().isEmpty());
    }
};

QTEST_MAIN(tst_HttpHandler)
#include "tst_httphandler.moc"