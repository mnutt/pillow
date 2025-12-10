#include "httpconnectionbase.h"
#include "HttpConnection.h"
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include "Helpers.h"
using namespace Pillow;

static void wait(int milliseconds = 10)
{
	QElapsedTimer t; t.start();
	do
	{
		QCoreApplication::processEvents(QEventLoop::AllEvents);
	}
	while (!t.hasExpired(milliseconds));
}

class HttpConnectionTcpSocketTest : public HttpConnectionTest
{
	Q_OBJECT

public:
	HttpConnectionTcpSocketTest();

private:
	QTcpServer* server;
	QTcpSocket* client;

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

HttpConnectionTcpSocketTest::HttpConnectionTcpSocketTest()
	: server(NULL), client(NULL)
{
}

void HttpConnectionTcpSocketTest::server_newConnection()
{
	QIODevice* device = server->nextPendingConnection();
	if (!reuseConnection || connection == 0)
	{
		connection = new HttpConnection();
	}
	connection->initialize(device, device);
}

void HttpConnectionTcpSocketTest::init()
{
	server = new QTcpServer();
	QVERIFY(server->listen());
	connect(server, SIGNAL(newConnection()), this, SLOT(server_newConnection()));
	client = new QTcpSocket();
	client->connectToHost(QHostAddress::LocalHost, server->serverPort());
	QVERIFY(client->waitForConnected(1000));
	while (connection == NULL || connection->inputDevice() == NULL)
		QCoreApplication::processEvents();
	readySpy = new QSignalSpy(connection, SIGNAL(requestReady(Pillow::HttpConnection*)));
	completedSpy = new QSignalSpy(connection, SIGNAL(requestCompleted(Pillow::HttpConnection*)));
	closedSpy = new QSignalSpy(connection, SIGNAL(closed(Pillow::HttpConnection*)));
}

void HttpConnectionTcpSocketTest::cleanup()
{
	if (connection && !reuseConnection) { delete connection; connection = NULL; }
	if (server) delete server; server = NULL;
	if (client) delete client; client = NULL;
	if (readySpy) delete readySpy; readySpy = NULL;
	if (completedSpy) delete completedSpy; completedSpy = NULL;
	if (closedSpy) delete closedSpy; closedSpy = NULL;
}

void HttpConnectionTcpSocketTest::clientWrite(const QByteArray &data)
{
	client->write(data);
}

void HttpConnectionTcpSocketTest::clientFlush(bool _wait /* = true */)
{
	QSignalSpy s(connection->inputDevice(), SIGNAL(readyRead()));
	client->flush();
	while (client->bytesToWrite() > 0) QCoreApplication::processEvents();
	QCoreApplication::processEvents();
	if (_wait) while (s.size() == 0) QCoreApplication::processEvents();
}

QByteArray HttpConnectionTcpSocketTest::clientReadAll()
{
	QElapsedTimer timer; timer.start();
	while (client->bytesAvailable() == 0 && !timer.hasExpired(500)) QCoreApplication::processEvents();
	return client->readAll();
}

bool HttpConnectionTcpSocketTest::isClientConnected()
{
	wait();
	return client->state() == QAbstractSocket::ConnectedState && client->isOpen();
}

void HttpConnectionTcpSocketTest::clientClose()
{
	client->disconnectFromHost();
	client->close();
	while (client->state() == QAbstractSocket::ConnectedState) wait();
}

QTEST_MAIN(HttpConnectionTcpSocketTest)
#include "tst_httpconnection_tcp.moc"