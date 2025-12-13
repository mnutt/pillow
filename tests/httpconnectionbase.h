#pragma once

#include <QObject>
#include <QPointer>
#include <QtNetwork/QSslError>
class QTcpServer;
class QTcpSocket;
class QLocalServer;
class QLocalSocket;
class QSignalSpy;
class QBuffer;
namespace Pillow
{
	class HttpConnection;
}

class HttpConnectionTest : public QObject
{
	Q_OBJECT

public:
	HttpConnectionTest();

protected:
	Pillow::HttpConnection* connection;
	QSignalSpy *readySpy, *completedSpy, *closedSpy;
	bool reuseConnection;

protected: // Helper methods.
	virtual void clientWrite(const QByteArray& data) = 0;
	virtual void clientFlush(bool wait = true) = 0;
	virtual QByteArray clientReadAll() = 0;
	virtual void clientClose() = 0;
	virtual bool isClientConnected() = 0;

protected slots: // Test methods.
	virtual void init();
	virtual void cleanup();

	// Behavior tests.
	void testInitialState();
	void testSimpleGet();
	void testSimplePost();
	void testIncrementalPost();
	void testHugePost();
	void testInvalidRequestHeaders();
	void testOversizedRequestHeaders();
	void testInvalidRequestContent();
	void testWriteSimpleResponse();
	void testWriteSimpleResponseString();
	void testConnectionKeepAlive();
	void testConnectionClose();
	void testPipelinedRequests();
	void testClientClosesConnectionEarly();
	void testClientExpects100Continue();
	void testHeadShouldNotSendResponseContent();
	void testWriteIncrementalResponseContent();
	void testWriteChunkedResponseContent();
	void testWriteResponseWithoutRequest();
	void testMultipacketResponse();
	void testReadsRequestParams();
	void testReuseRequest();

	void benchmarkSimpleGetClose();
	void benchmarkSimpleGetKeepAlive();
};