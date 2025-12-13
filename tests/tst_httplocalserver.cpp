#include "httpserverbase.h"
#include <HttpServer.h>
#include <HttpConnection.h>
#include <QtTest/QtTest>
#include <QtTest/QSignalSpy>
#include <QtNetwork/QLocalSocket>

class HttpLocalServerTest : public HttpServerTestBase
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

	// Additional HttpLocalServer-specific tests
	void testServerProperties()
	{
		Pillow::HttpLocalServer* localServer = static_cast<Pillow::HttpLocalServer*>(server);
		QVERIFY(localServer->isListening());
		QCOMPARE(localServer->serverName(), QString("Pillow_HttpLocalServerTest"));
	}

	void testDefaultConstructor()
	{
		Pillow::HttpLocalServer defaultServer;
		QVERIFY(!defaultServer.isListening());
		QVERIFY(defaultServer.listen("Pillow_TestDefaultServer"));
		QVERIFY(defaultServer.isListening());
		QCOMPARE(defaultServer.serverName(), QString("Pillow_TestDefaultServer"));
		defaultServer.close();
		QVERIFY(!defaultServer.isListening());
	}

	void testMultipleRequestsSameSocket()
	{
		QLocalSocket* client = new QLocalSocket();
		client->connectToServer("Pillow_HttpLocalServerTest");
		QVERIFY(client->waitForConnected(1000));

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

		// Second request on same socket
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

	void testServerSignals()
	{
		QSignalSpy requestReadySpy(server, SIGNAL(requestReady(Pillow::HttpConnection*)));

		QLocalSocket* client = new QLocalSocket();
		client->connectToServer("Pillow_HttpLocalServerTest");
		QVERIFY(client->waitForConnected(1000));

		client->write("GET / HTTP/1.0\r\n\r\n");
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

QObject* HttpLocalServerTest::createServer()
{
	return new Pillow::HttpLocalServer("Pillow_HttpLocalServerTest");
}

QIODevice * HttpLocalServerTest::createClientConnection()
{
	QSignalSpy spy(server, SIGNAL(newConnection()));
	QLocalSocket* socket = new QLocalSocket(server);
	socket->connectToServer("Pillow_HttpLocalServerTest");
	while (spy.isEmpty() && socket->error() == QLocalSocket::UnknownSocketError)
		QCoreApplication::processEvents();

	if (socket->error() != QLocalSocket::UnknownSocketError)
		qDebug() << "Unexpected QLocalSocket error:" << socket->errorString();

	return socket;
}

QTEST_MAIN(HttpLocalServerTest)
#include "tst_httplocalserver.moc"