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