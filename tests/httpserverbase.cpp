#include "httpserverbase.h"
#include <HttpServer.h>
#include <HttpConnection.h>
#include <QtTest/QtTest>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QLocalSocket>

ulong qHash(const QPointer<Pillow::HttpConnection>& ptr)
{
    Pillow::HttpConnection* c = static_cast<Pillow::HttpConnection*>(ptr);
    return qHash(c);
}

// Qt6 compatibility helper - replaces the removed QList::toSet()
template<typename T>
int uniqueCount(const QList<T>& list)
{
    return QSet<T>(list.begin(), list.end()).size();
}

void HttpServerTestBase::init()
{
    server = createServer();
    connect(server, SIGNAL(requestReady(Pillow::HttpConnection*)), this, SLOT(requestReady(Pillow::HttpConnection*)));
}

void HttpServerTestBase::cleanup()
{
    delete server;
    handledRequests.clear();
    guardedHandledRequests.clear();
}

void HttpServerTestBase::requestReady(Pillow::HttpConnection* request)
{
    handledRequests << request;
    guardedHandledRequests << request;
}

void HttpServerTestBase::sendRequest(QIODevice* device, const QByteArray& content)
{
    int oldHandledRequestsCount = handledRequests.size();

    QByteArray request;
    request.append("GET / HTTP/1.0\r\nContent-Length: ").append(QByteArray::number(content.size())).append("\r\n\r\n").append(content);
    device->write(request);

    while (oldHandledRequestsCount == handledRequests.size())
        QCoreApplication::processEvents();
}

void HttpServerTestBase::sendResponses()
{
    foreach (Pillow::HttpConnection* request, guardedHandledRequests)
    {
        if (request && request->state() == Pillow::HttpConnection::SendingHeaders)
        {
            // Echo the request content as the response content.
            request->writeResponse(200, Pillow::HttpHeaderCollection(), request->requestContent());
        }
    }
}

void HttpServerTestBase::sendConcurrentRequests(int concurrencyLevel)
{
    int startingHandledRequests = handledRequests.size();
    QVector<QIODevice*> clients;

    for (int j = 0; j < concurrencyLevel; ++j)
        clients << createClientConnection();
    for (int j = 0; j < concurrencyLevel; ++j)
        sendRequest(clients.at(j), QByteArray("Hello").append(QByteArray::number(j)));

    while (handledRequests.size() < startingHandledRequests + concurrencyLevel)
        QCoreApplication::processEvents();

    sendResponses();

    foreach (QIODevice* client, clients)
    {
        while (client->bytesAvailable() == 0)
            QCoreApplication::processEvents();
        client->readAll(), delete client;
    }
}

void HttpServerTestBase::testHandlesConnectionsAsRequests()
{
    QIODevice* client = createClientConnection();
    sendRequest(client, "Hello");
    sendResponses();
    while (client->bytesAvailable() == 0)
        QCoreApplication::processEvents();
    QCOMPARE(handledRequests.size(), 1);
    QByteArray response = client->readAll();
    QVERIFY(response.startsWith("HTTP/1.0 200 OK"));
    QVERIFY(response.endsWith("Hello"));
}

void HttpServerTestBase::testHandlesConcurrentConnections()
{
    const int clientCount =
        10; // Note: on Windows the maximum number of concurrent QLocalSocket clients is 62, so putting this well below the limit.
    QVector<QIODevice*> clients;
    for (int i = 0; i < clientCount; ++i)
        clients << createClientConnection();

    for (int i = 0; i < clientCount; ++i)
        sendRequest(clients.at(i), QByteArray("Hello").append(QByteArray::number(i)));

    sendResponses();
    QCOMPARE(handledRequests.size(), clientCount);

    for (int i = 0; i < clientCount; ++i)
    {
        QIODevice* client = clients.at(i);
        while (client->bytesAvailable() == 0)
            QCoreApplication::processEvents();
        QByteArray response = client->readAll();
        QVERIFY(response.startsWith("HTTP/1.0 200 OK"));
        QVERIFY(response.endsWith(QByteArray("Hello").append(QByteArray::number(i))));
    }
}

void HttpServerTestBase::testHandlesConcurrentConnectionsSimultaneousResponses()
{
    const int clientCount = 10;
    QVector<QIODevice*> clients;
    QMap<Pillow::HttpConnection*, int> connectionToClientIndex;

    // Create connections and send unique requests
    for (int i = 0; i < clientCount; ++i)
        clients << createClientConnection();

    for (int i = 0; i < clientCount; ++i)
        sendRequest(clients.at(i), QByteArray("Hello").append(QByteArray::number(i)));

    // Ensure we have all requests before responding
    QCOMPARE(handledRequests.size(), clientCount);

    // Map each connection to its expected client index based on request content
    for (int i = 0; i < handledRequests.size(); ++i)
    {
        Pillow::HttpConnection* request = handledRequests.at(i);
        QByteArray content = request->requestContent();
        QString contentStr = QString::fromLatin1(content);

        // Extract client index from "Hello{i}" content
        bool ok;
        int clientIndex = contentStr.mid(5).toInt(&ok); // Remove "Hello" prefix
        QVERIFY(ok);
        QVERIFY(clientIndex >= 0 && clientIndex < clientCount);

        connectionToClientIndex[request] = clientIndex;
    }

    // Send ALL responses SIMULTANEOUSLY (like the proxy test does)
    // This mimics the behavior that triggers the infinite loop in the proxy test
    foreach (Pillow::HttpConnection* request, guardedHandledRequests)
    {
        if (request && request->state() == Pillow::HttpConnection::SendingHeaders)
        {
            int clientIndex = connectionToClientIndex[request];
            QByteArray responseContent = QByteArray("Response").append(QByteArray::number(clientIndex));
            // Write response immediately without waiting - this should trigger simultaneous completion
            request->writeResponse(200, Pillow::HttpHeaderCollection(), responseContent);
        }
    }

    // Verify each client gets exactly the correct response
    QMap<int, QByteArray> actualResponses;
    for (int i = 0; i < clientCount; ++i)
    {
        QIODevice* client = clients.at(i);
        while (client->bytesAvailable() == 0)
            QCoreApplication::processEvents();
        QByteArray response = client->readAll();

        // Store actual response for this client
        actualResponses[i] = response;

        QVERIFY(response.startsWith("HTTP/1.0 200 OK"));

        // Verify this client gets the response for its own request
        QByteArray expectedContent = QByteArray("Response").append(QByteArray::number(i));
        if (!response.endsWith(expectedContent))
        {
            qDebug() << "CROSS-CONTAMINATION DETECTED!";
            qDebug() << "Client" << i << "expected:" << expectedContent;
            qDebug() << "Client" << i << "got response:" << response;

            // Show what all clients got to help debug cross-contamination
            for (int j = 0; j < clientCount; ++j)
            {
                if (actualResponses.contains(j))
                    qDebug() << "  Client" << j << "response:" << actualResponses[j];
            }
            QFAIL("Client received wrong response - possible cross-contamination!");
        }
    }

    // Clean up
    foreach (QIODevice* client, clients)
        delete client;
}

void HttpServerTestBase::testReusesRequests()
{
    const int iterations = 3;
    const int clientCount = 25;
    for (int i = 0; i < iterations; ++i)
        sendConcurrentRequests(clientCount);

    QCOMPARE(handledRequests.size(), iterations * clientCount);
    QCOMPARE(uniqueCount(handledRequests), 25);

    // Handling way more request should reuse those unique requests.
    handledRequests.clear();
    guardedHandledRequests.clear();
    for (int i = 0; i < iterations * 2; ++i)
        sendConcurrentRequests(clientCount + 5);
    QCOMPARE(handledRequests.size(), iterations * 2 * (clientCount + 5));
    QVERIFY(uniqueCount(handledRequests) > uniqueCount(guardedHandledRequests));
    QCOMPARE(uniqueCount(guardedHandledRequests),
             26); // The pooled request objects still alive + a NULL pointer for all requests that were collected.
}

void HttpServerTestBase::testDestroysRequests()
{
    const int iterations = 2;
    const int clientCount = 27;
    for (int i = 0; i < iterations; ++i)
        sendConcurrentRequests(clientCount);

    QCOMPARE(handledRequests.size(), iterations * clientCount);
    QVERIFY(uniqueCount(handledRequests) >= uniqueCount(guardedHandledRequests));
    QCOMPARE(uniqueCount(guardedHandledRequests), 26); // There should remain the internally pooled objects. + 1 for the NULL pointer.

    delete server;
    server = NULL;
    QCOMPARE(uniqueCount(guardedHandledRequests), 1); // All requests should now have been destroyed. Only NULL is remaining.
}
