#include "httpconnectionbase.h"
#include "HttpConnection.h"
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QSslSocket>
#include <QtNetwork/QSslKey>
#include <QtNetwork/QSslCertificate>
#include "Helpers.h"
using namespace Pillow;

#if !defined(PILLOW_NO_SSL) && !defined(QT_NO_SSL)

static void wait(int milliseconds = 10)
{
	QElapsedTimer t; t.start();
	do
	{
		QCoreApplication::processEvents(QEventLoop::AllEvents);
	}
	while (!t.hasExpired(milliseconds));
}

class HttpConnectionSslSocketTest : public HttpConnectionTest
{
	Q_OBJECT

public:
	HttpConnectionSslSocketTest();

private:
	class SslTestServer* server;
	QSslSocket* client;

private slots:
	void server_newConnection();
	void sslSocket_encrypted();
	void sslSocket_sslErrors(const QList<QSslError>& );

protected:
	virtual void init();
	virtual void cleanup();
	virtual void clientWrite(const QByteArray& data);
	virtual void clientFlush(bool wait = true);
	virtual QByteArray clientReadAll();
	virtual void clientClose();
	virtual bool isClientConnected();
};

class SslTestServer : public QTcpServer
{
public:
	HttpConnectionSslSocketTest* test;
	QSslCertificate certificate;
	QSslKey key;

protected:
	virtual void incomingConnection(int socketDescriptor)
	{
		QSslSocket* sslSocket = new QSslSocket(this);
		if (sslSocket->setSocketDescriptor(socketDescriptor))
		{
			sslSocket->setPrivateKey(key);
			sslSocket->setLocalCertificate(certificate);
			sslSocket->startServerEncryption();
			connect(sslSocket, SIGNAL(encrypted()), test, SLOT(sslSocket_encrypted()));
			connect(sslSocket, SIGNAL(sslErrors(QList<QSslError>)), test, SLOT(sslSocket_sslErrors(QList<QSslError>)));
			addPendingConnection(sslSocket);
		}
		else
		{
			delete sslSocket;
		}
	}
};

HttpConnectionSslSocketTest::HttpConnectionSslSocketTest()
	: server(NULL), client(NULL)
{
}

void HttpConnectionSslSocketTest::server_newConnection()
{
	QIODevice* device = server->nextPendingConnection();
	connection = new HttpConnection();
	connection->initialize(device, device);
}

void HttpConnectionSslSocketTest::sslSocket_encrypted()
{
}

void HttpConnectionSslSocketTest::sslSocket_sslErrors(const QList<QSslError>& )
{
}

void HttpConnectionSslSocketTest::init()
{
	QVERIFY(QFile::exists(":/test.crt"));
	QVERIFY(QFile::exists(":/test.key"));
	QFile certificateFile(":/test.crt"); QVERIFY(certificateFile.open(QIODevice::ReadOnly));
	QFile keyFile(":/test.key"); QVERIFY(keyFile.open(QIODevice::ReadOnly));
	QSslCertificate certificate(&certificateFile);
	QSslKey key(&keyFile, QSsl::Rsa);

	server = new SslTestServer();
	server->test = this;
	server->certificate = certificate;
	server->key = key;
	QVERIFY(server->listen());
	connect(server, SIGNAL(newConnection()), this, SLOT(server_newConnection()));

	client = new QSslSocket();
	client->setLocalCertificate(certificate);
	client->setPrivateKey(key);
	client->setPeerVerifyMode(QSslSocket::VerifyNone);
	client->connectToHostEncrypted("127.0.0.1", server->serverPort());
	connect(client, SIGNAL(encrypted()), this, SLOT(sslSocket_encrypted()));
	connect(client, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslSocket_sslErrors(QList<QSslError>)));
	QVERIFY(client->waitForConnected());
	while (!client->isEncrypted()) QCoreApplication::processEvents();
	QVERIFY(client->isEncrypted());

	while (connection == NULL) QCoreApplication::processEvents();
	readySpy = new QSignalSpy(connection, SIGNAL(requestReady(Pillow::HttpConnection*)));
	completedSpy = new QSignalSpy(connection, SIGNAL(requestCompleted(Pillow::HttpConnection*)));
	closedSpy = new QSignalSpy(connection, SIGNAL(closed(Pillow::HttpConnection*)));
}

void HttpConnectionSslSocketTest::cleanup()
{
	if (connection) delete connection; connection = NULL;
	if (server) delete server; server = NULL;
	if (client) delete client; client = NULL;
	if (readySpy) delete readySpy; readySpy = NULL;
	if (completedSpy) delete completedSpy; completedSpy = NULL;
	if (closedSpy) delete closedSpy; closedSpy = NULL;
}

void HttpConnectionSslSocketTest::clientWrite(const QByteArray &data)
{
	client->write(data);
}

void HttpConnectionSslSocketTest::clientFlush(bool _wait /* = true */)
{
	QSignalSpy s(connection->inputDevice(), SIGNAL(readyRead()));
	client->flush();
	while (client->bytesToWrite() > 0) QCoreApplication::processEvents();
	QCoreApplication::processEvents();
	if (_wait) while (s.size() == 0) QCoreApplication::processEvents();
}

QByteArray HttpConnectionSslSocketTest::clientReadAll()
{
	QElapsedTimer timer; timer.start();
	while (client->bytesAvailable() == 0 && !timer.hasExpired(2000)) QCoreApplication::processEvents();
	QCoreApplication::processEvents();
	return client->readAll();
}

bool HttpConnectionSslSocketTest::isClientConnected()
{
	wait();
	return client->state() == QAbstractSocket::ConnectedState && client->isOpen();
}

void HttpConnectionSslSocketTest::clientClose()
{
	client->disconnectFromHost();
	client->close();
	while (client->state() == QAbstractSocket::ConnectedState) wait();
}

#else

class HttpConnectionSslSocketTest : public QObject
{
	Q_OBJECT
};

#endif // !defined(PILLOW_NO_SSL) && !defined(QT_NO_SSL)

QTEST_MAIN(HttpConnectionSslSocketTest)
#include "tst_httpconnection_ssl.moc"