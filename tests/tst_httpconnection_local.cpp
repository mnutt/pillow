#include "httpconnectionbase.h"
#include "HttpConnection.h"
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>
#include "Helpers.h"
using namespace Pillow;

static void wait(int milliseconds = 10)
{
	QElapsedTimer t;
	t.start();
	do
	{
		QCoreApplication::processEvents(QEventLoop::AllEvents);
	}
	while (!t.hasExpired(milliseconds));
}

class HttpConnectionLocalSocketTest : public HttpConnectionTest
{
	Q_OBJECT

public:
	HttpConnectionLocalSocketTest();

private:
	QLocalServer* server;
	QLocalSocket* client;

private slots:
	void server_newConnection();

protected:
	virtual void init();
	virtual void cleanup();
	virtual void clientWrite(const QByteArray& data);
	virtual void clientFlush(bool wait = true);
	virtual QByteArray clientReadAll();
	virtual void clientClose();
	virtual bool isClientConnected();
};

HttpConnectionLocalSocketTest::HttpConnectionLocalSocketTest() : server(NULL), client(NULL) {}

void HttpConnectionLocalSocketTest::server_newConnection()
{
	QIODevice* device = server->nextPendingConnection();
	connection = new HttpConnection();
	connection->initialize(device, device);
}

void HttpConnectionLocalSocketTest::init()
{
	server = new QLocalServer();
	server->removeServer("pillowtest");
	QVERIFY(server->listen("pillowtest"));
	connect(server, SIGNAL(newConnection()), this, SLOT(server_newConnection()));
	client = new QLocalSocket();
	client->connectToServer("pillowtest");
	QVERIFY(client->waitForConnected(1000));
	while (connection == NULL)
		QCoreApplication::processEvents();
	readySpy = new QSignalSpy(connection, SIGNAL(requestReady(Pillow::HttpConnection*)));
	completedSpy = new QSignalSpy(connection, SIGNAL(requestCompleted(Pillow::HttpConnection*)));
	closedSpy = new QSignalSpy(connection, SIGNAL(closed(Pillow::HttpConnection*)));
}

void HttpConnectionLocalSocketTest::cleanup()
{
	if (connection)
		delete connection;
	connection = NULL;
	if (server)
		delete server;
	server = NULL;
	if (client)
		delete client;
	client = NULL;
	if (readySpy)
		delete readySpy;
	readySpy = NULL;
	if (completedSpy)
		delete completedSpy;
	completedSpy = NULL;
	if (closedSpy)
		delete closedSpy;
	closedSpy = NULL;
}

void HttpConnectionLocalSocketTest::clientWrite(const QByteArray& data)
{
	client->write(data);
}

void HttpConnectionLocalSocketTest::clientFlush(bool _wait /* = true */)
{
	QSignalSpy s(connection->inputDevice(), SIGNAL(readyRead()));
	client->flush();
	while (client->bytesToWrite() > 0)
		QCoreApplication::processEvents();
	QCoreApplication::processEvents();
	if (_wait)
		while (s.size() == 0)
			QCoreApplication::processEvents();
}

QByteArray HttpConnectionLocalSocketTest::clientReadAll()
{
	QElapsedTimer timer;
	timer.start();
	while (client->bytesAvailable() == 0 && !timer.hasExpired(500))
		QCoreApplication::processEvents();
	return client->readAll();
}

void HttpConnectionLocalSocketTest::clientClose()
{
	client->close();
}

bool HttpConnectionLocalSocketTest::isClientConnected()
{
	wait(20); // On Windows, we must wait quite a bit for the client connection state to be updated.
	return client->state() == QLocalSocket::ConnectedState && client->isOpen();
}

QTEST_MAIN(HttpConnectionLocalSocketTest)
#include "tst_httpconnection_local.moc"