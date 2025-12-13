#include "httpserverbase.h"
#include <HttpsServer.h>
#include <HttpConnection.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtTest/QtTest>
#include <QtNetwork/QSslSocket>
#include <QtNetwork/QSslKey>
#include <QtNetwork/QSslCertificate>

#if !defined(PILLOW_NO_SSL) && !defined(QT_NO_SSL)

static QSslCertificate sslCertificate()
{
    static QSslCertificate certificate;
    if (certificate.isNull())
    {
        QFile file(":/test.crt");
        if (file.open(QIODevice::ReadOnly))
            certificate = QSslCertificate(&file);
        else
            qWarning() << "Failed to open SSL certificate file 'test.crt'";
    }
    return certificate;
}

static QSslKey sslPrivateKey()
{
    static QSslKey key;
    if (key.isNull())
    {
        QFile file(":/test.key");
        if (file.open(QIODevice::ReadOnly))
            key = QSslKey(&file, QSsl::Rsa);
        else
            qWarning() << "Failed to open SSL key file 'test.key'";
    }
    return key;
}

class HttpsServerTest : public HttpServerTestBase
{
    Q_OBJECT

private slots: // Test slots.
    void init() { HttpServerTestBase::init(); }
    void cleanup() { HttpServerTestBase::cleanup(); }

    void testInit() { HttpServerTestBase::testInit(); }
    void testHandlesConnectionsAsRequests() { HttpServerTestBase::testHandlesConnectionsAsRequests(); }
    void testHandlesConcurrentConnections() { HttpServerTestBase::testHandlesConcurrentConnections(); }
    void testReusesRequests() { HttpServerTestBase::testReusesRequests(); }
    void testDestroysRequests() { HttpServerTestBase::testDestroysRequests(); }

    // Additional HTTPS-specific tests
    void testServerProperties()
    {
        Pillow::HttpsServer* httpsServer = static_cast<Pillow::HttpsServer*>(server);
        QVERIFY(httpsServer->isListening());
        QCOMPARE(httpsServer->serverPort(), quint16(4588));
        QVERIFY(!httpsServer->certificate().isNull());
        QVERIFY(!httpsServer->privateKey().isNull());
    }

    void testSslConnectionEncrypted()
    {
        QSslSocket* client = static_cast<QSslSocket*>(createClientConnection());
        QVERIFY(client->isEncrypted());
        QVERIFY(client->mode() == QSslSocket::SslClientMode);

        // Send a request over the encrypted connection
        QByteArray request = "GET /test HTTP/1.0\r\n\r\n";
        client->write(request);
        client->flush();

        while (handledRequests.isEmpty())
            QCoreApplication::processEvents();

        QCOMPARE(handledRequests.last()->requestPath(), QByteArray("/test"));
        handledRequests.last()->writeResponse(200, Pillow::HttpHeaderCollection(), "secure response");

        while (client->bytesAvailable() == 0)
            QCoreApplication::processEvents();

        QByteArray response = client->readAll();
        QVERIFY(response.startsWith("HTTP/1.0 200 OK"));
        QVERIFY(response.endsWith("secure response"));

        delete client;
    }

    void testDefaultConstructor()
    {
        Pillow::HttpsServer defaultServer;
        QVERIFY(!defaultServer.isListening());

        // Set certificate and key before listening
        defaultServer.setCertificate(sslCertificate());
        defaultServer.setPrivateKey(sslPrivateKey());

        QVERIFY(defaultServer.listen(QHostAddress::LocalHost, 0));
        QVERIFY(defaultServer.isListening());
        QVERIFY(defaultServer.serverPort() > 0);
        defaultServer.close();
        QVERIFY(!defaultServer.isListening());
    }

    void testMultipleKeepAliveRequests()
    {
        QSslSocket* client = static_cast<QSslSocket*>(createClientConnection());
        QVERIFY(client->isEncrypted());

        // First request
        client->write("GET /first HTTP/1.1\r\nHost: localhost\r\n\r\n");
        client->flush();

        while (handledRequests.isEmpty())
            QCoreApplication::processEvents();

        handledRequests.last()->writeResponse(200, Pillow::HttpHeaderCollection(), "first");

        while (client->bytesAvailable() == 0)
            QCoreApplication::processEvents();
        QByteArray response1 = client->readAll();
        QVERIFY(response1.contains("first"));

        // Second request on same encrypted connection
        client->write("GET /second HTTP/1.1\r\nHost: localhost\r\n\r\n");
        client->flush();

        while (handledRequests.size() < 2)
            QCoreApplication::processEvents();

        handledRequests.last()->writeResponse(200, Pillow::HttpHeaderCollection(), "second");

        while (client->bytesAvailable() == 0)
            QCoreApplication::processEvents();
        QByteArray response2 = client->readAll();
        QVERIFY(response2.contains("second"));

        delete client;
    }

protected:
    virtual QObject* createServer();
    virtual QIODevice* createClientConnection();
};

QObject* HttpsServerTest::createServer()
{
    return new Pillow::HttpsServer(sslCertificate(), sslPrivateKey(), QHostAddress::Any, 4588);
}

QIODevice* HttpsServerTest::createClientConnection()
{
    QSslSocket* socket = new QSslSocket(server);
    socket->setLocalCertificate(sslCertificate());
    socket->setPrivateKey(sslPrivateKey());
    socket->setPeerVerifyMode(QSslSocket::VerifyNone);
    socket->connectToHostEncrypted("127.0.0.1", 4588);
    while (socket->state() != QAbstractSocket::ConnectedState || !socket->isEncrypted())
        QCoreApplication::processEvents();
    return socket;
}

#else

class HttpsServerTest : public QObject
{
    Q_OBJECT
};

#endif // !defined(PILLOW_NO_SSL) && !defined(QT_NO_SSL)

QTEST_MAIN(HttpsServerTest)
#include "tst_httpsserver.moc"