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
	void testHandlesConcurrentConnectionsSimultaneousResponses() { HttpServerTestBase::testHandlesConcurrentConnectionsSimultaneousResponses(); }
	void testReusesRequests() { HttpServerTestBase::testReusesRequests(); }
	void testDestroysRequests() { HttpServerTestBase::testDestroysRequests(); }

protected:
	virtual QObject* createServer();
	virtual QIODevice* createClientConnection();
};

QObject* HttpServerTest::createServer()
{
	return new Pillow::HttpServer(QHostAddress::Any, 4577);
}

QIODevice * HttpServerTest::createClientConnection()
{
	QTcpSocket* socket = new QTcpSocket(server);
	socket->connectToHost(QHostAddress::LocalHost, 4577);
	Q_ASSERT(socket->waitForConnected(50));
	return socket;
}

QTEST_MAIN(HttpServerTest)
#include "tst_httpserver.moc"