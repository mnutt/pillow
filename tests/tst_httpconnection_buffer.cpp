#include "httpconnectionbase.h"
#include "HttpConnection.h"
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QtCore/QBuffer>
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

class HttpConnectionBufferTest : public HttpConnectionTest
{
	Q_OBJECT

public:
	HttpConnectionBufferTest();

private:
	QBuffer* inputBuffer;
	QBuffer* outputBuffer;

protected:
	virtual void init();
	virtual void cleanup();
	virtual void clientWrite(const QByteArray& data);
	virtual void clientFlush(bool wait = true);
	virtual QByteArray clientReadAll();
	virtual void clientClose();
	virtual bool isClientConnected();
};

HttpConnectionBufferTest::HttpConnectionBufferTest() : inputBuffer(NULL), outputBuffer(NULL) {}

void HttpConnectionBufferTest::init()
{
	inputBuffer = new QBuffer();
	inputBuffer->open(QIODevice::ReadWrite);
	outputBuffer = new QBuffer();
	outputBuffer->open(QIODevice::ReadWrite);

	connection = new HttpConnection(NULL);
	connection->initialize(inputBuffer, outputBuffer);

	readySpy = new QSignalSpy(connection, SIGNAL(requestReady(Pillow::HttpConnection*)));
	completedSpy = new QSignalSpy(connection, SIGNAL(requestCompleted(Pillow::HttpConnection*)));
	closedSpy = new QSignalSpy(connection, SIGNAL(closed(Pillow::HttpConnection*)));
}

void HttpConnectionBufferTest::cleanup()
{
	if (connection)
		delete connection;
	connection = NULL;
	if (inputBuffer)
		delete inputBuffer;
	inputBuffer = NULL;
	if (outputBuffer)
		delete outputBuffer;
	outputBuffer = NULL;
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

void HttpConnectionBufferTest::clientWrite(const QByteArray& data)
{
	inputBuffer->write(data);
}

void HttpConnectionBufferTest::clientFlush(bool _wait /* = true */)
{
	if (inputBuffer->isOpen())
		inputBuffer->seek(0);
	QCoreApplication::processEvents();
	if (_wait)
		wait();
	if (inputBuffer->isOpen())
		inputBuffer->seek(0);
	inputBuffer->buffer().clear();
}

QByteArray HttpConnectionBufferTest::clientReadAll()
{
	QCoreApplication::processEvents();
	if (outputBuffer->isOpen())
		outputBuffer->seek(0);
	QByteArray data = outputBuffer->buffer();
	if (outputBuffer->isOpen())
		outputBuffer->seek(0);
	outputBuffer->buffer().clear();
	return data;
}

void HttpConnectionBufferTest::clientClose()
{
	inputBuffer->close();
	outputBuffer->close();
}

bool HttpConnectionBufferTest::isClientConnected()
{
	return inputBuffer->isOpen();
}

QTEST_MAIN(HttpConnectionBufferTest)
#include "tst_httpconnection_buffer.moc"