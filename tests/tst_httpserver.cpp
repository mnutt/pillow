#include "httpserverbase.h"
#include <HttpServer.h>
#include <HttpConnection.h>
#include <QtTest/QtTest>
#include <QtNetwork/QTcpSocket>

class HttpServerTest : public HttpServerTestBase
{
    Q_OBJECT

private slots: // Test slots.
    void init() { HttpServerTestBase::init(); }
    void cleanup() { HttpServerTestBase::cleanup(); }

    void testInit() { HttpServerTestBase::testInit(); }
    void testHandlesConnectionsAsRequests() { HttpServerTestBase::testHandlesConnectionsAsRequests(); }
    void testHandlesConcurrentConnections() { HttpServerTestBase::testHandlesConcurrentConnections(); }
    void testHandlesConcurrentConnectionsSimultaneousResponses()
    {
        HttpServerTestBase::testHandlesConcurrentConnectionsSimultaneousResponses();
    }
    void testReusesRequests() { HttpServerTestBase::testReusesRequests(); }
    void testDestroysRequests() { HttpServerTestBase::testDestroysRequests(); }

    // Additional tests
    void testServerProperties()
    {
        Pillow::HttpServer* httpServer = static_cast<Pillow::HttpServer*>(server);
        QVERIFY(httpServer->isListening());
        QCOMPARE(httpServer->serverPort(), quint16(4577));
        QCOMPARE(httpServer->serverAddress(), QHostAddress(QHostAddress::Any));
    }

    void testMultipleSequentialRequestsSameConnection()
    {
        // Test keep-alive behavior with multiple sequential requests
        QTcpSocket* client = new QTcpSocket(server);
        client->connectToHost(QHostAddress::LocalHost, 4577);
        QVERIFY(client->waitForConnected(50));

        // First request with keep-alive
        QByteArray request1 = "GET /first HTTP/1.1\r\nHost: localhost\r\n\r\n";
        client->write(request1);
        client->flush();

        while (handledRequests.size() < 1)
            QCoreApplication::processEvents();

        handledRequests.last()->writeResponse(200, Pillow::HttpHeaderCollection(), "first response");

        while (client->bytesAvailable() == 0)
            QCoreApplication::processEvents();
        QByteArray response1 = client->readAll();
        QVERIFY(response1.startsWith("HTTP/1.1 200 OK"));
        QVERIFY(response1.endsWith("first response"));

        // Second request on same connection
        QByteArray request2 = "GET /second HTTP/1.1\r\nHost: localhost\r\n\r\n";
        client->write(request2);
        client->flush();

        while (handledRequests.size() < 2)
            QCoreApplication::processEvents();

        handledRequests.last()->writeResponse(200, Pillow::HttpHeaderCollection(), "second response");

        while (client->bytesAvailable() == 0)
            QCoreApplication::processEvents();
        QByteArray response2 = client->readAll();
        QVERIFY(response2.startsWith("HTTP/1.1 200 OK"));
        QVERIFY(response2.endsWith("second response"));

        delete client;
    }

    void testMalformedRequest()
    {
        QTcpSocket* client = new QTcpSocket(server);
        client->connectToHost(QHostAddress::LocalHost, 4577);
        QVERIFY(client->waitForConnected(50));

        // Send malformed HTTP request
        QByteArray malformed = "GARBAGE NOT HTTP\r\n\r\n";
        client->write(malformed);
        client->flush();

        // Wait for server to respond with error
        while (client->bytesAvailable() == 0 && client->state() == QAbstractSocket::ConnectedState)
            QCoreApplication::processEvents();

        if (client->bytesAvailable() > 0)
        {
            QByteArray response = client->readAll();
            QVERIFY(response.startsWith("HTTP/1.0 400")); // Bad Request
        }

        delete client;
    }

    void testConnectionCloseHeader()
    {
        QTcpSocket* client = new QTcpSocket(server);
        client->connectToHost(QHostAddress::LocalHost, 4577);
        QVERIFY(client->waitForConnected(50));

        // Send request with Connection: close
        QByteArray request = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
        client->write(request);
        client->flush();

        while (handledRequests.isEmpty())
            QCoreApplication::processEvents();

        handledRequests.last()->writeResponse(200, Pillow::HttpHeaderCollection(), "goodbye");

        // Wait for connection to close
        while (client->state() == QAbstractSocket::ConnectedState && client->bytesToWrite() > 0)
            QCoreApplication::processEvents();

        while (client->bytesAvailable() == 0)
            QCoreApplication::processEvents();

        QByteArray response = client->readAll();
        QVERIFY(response.startsWith("HTTP/1.1 200 OK"));
        QVERIFY(response.contains("Connection: close"));

        // Connection should be closed after response
        while (client->state() == QAbstractSocket::ConnectedState)
            QCoreApplication::processEvents();

        QVERIFY(client->state() != QAbstractSocket::ConnectedState);

        delete client;
    }

    void testDefaultConstructor()
    {
        Pillow::HttpServer defaultServer;
        QVERIFY(!defaultServer.isListening());
        QVERIFY(defaultServer.listen(QHostAddress::LocalHost, 0));
        QVERIFY(defaultServer.isListening());
        QVERIFY(defaultServer.serverPort() > 0);
        defaultServer.close();
        QVERIFY(!defaultServer.isListening());
    }

    void testServerSignals()
    {
        QSignalSpy requestReadySpy(server, SIGNAL(requestReady(Pillow::HttpConnection*)));

        QTcpSocket* client = new QTcpSocket(server);
        client->connectToHost(QHostAddress::LocalHost, 4577);
        QVERIFY(client->waitForConnected(50));

        QByteArray request = "GET / HTTP/1.0\r\n\r\n";
        client->write(request);
        client->flush();

        while (requestReadySpy.isEmpty())
            QCoreApplication::processEvents();

        QCOMPARE(requestReadySpy.size(), 1);
        QVERIFY(requestReadySpy.at(0).at(0).value<Pillow::HttpConnection*>() != nullptr);

        delete client;
    }

protected:
    virtual QObject* createServer();
    virtual QIODevice* createClientConnection();
};

QObject* HttpServerTest::createServer()
{
    return new Pillow::HttpServer(QHostAddress::Any, 4577);
}

QIODevice* HttpServerTest::createClientConnection()
{
    QTcpSocket* socket = new QTcpSocket(server);
    socket->connectToHost(QHostAddress::LocalHost, 4577);
    Q_ASSERT(socket->waitForConnected(50));
    return socket;
}

QTEST_MAIN(HttpServerTest)
#include "tst_httpserver.moc"