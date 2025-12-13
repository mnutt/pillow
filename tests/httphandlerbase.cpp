#include "httphandlerbase.h"
#include "HttpHandler.h"
#include "HttpHandlerSimpleRouter.h"
#include "HttpConnection.h"
#include <QtCore/QBuffer>
#include <QtCore/QCoreApplication>
using namespace Pillow;

Pillow::HttpConnection* HttpHandlerTestBase::createGetRequest(const QByteArray& path, const QByteArray& httpVersion)
{
	return createRequest("GET", path, QByteArray(), httpVersion);
}

Pillow::HttpConnection* HttpHandlerTestBase::createPostRequest(const QByteArray& path, const QByteArray& content,
                                                               const QByteArray& httpVersion)
{
	return createRequest("POST", path, content, httpVersion);
}

Pillow::HttpConnection* HttpHandlerTestBase::createRequest(const QByteArray& method, const QByteArray& path, const QByteArray& content,
                                                           const QByteArray& httpVersion)
{
	QByteArray data = QByteArray().append(method).append(" ").append(path).append(" HTTP/").append(httpVersion).append("\r\n");
	if (content.size() > 0)
	{
		data.append("Content-Length: ").append(QByteArray::number(content.size())).append("\r\n");
		data.append("Content-Type: text/plain\r\n");
	}
	data.append("\r\n").append(content);

	QBuffer* inputBuffer = new QBuffer();
	inputBuffer->open(QIODevice::ReadWrite);
	QBuffer* outputBuffer = new QBuffer();
	outputBuffer->open(QIODevice::ReadWrite);
	connect(outputBuffer, SIGNAL(bytesWritten(qint64)), this, SLOT(outputBuffer_bytesWritten()));

	Pillow::HttpConnection* connection = new Pillow::HttpConnection(this);
	connect(connection, SIGNAL(requestCompleted(Pillow::HttpConnection*)), this, SLOT(requestCompleted(Pillow::HttpConnection*)));
	connection->initialize(inputBuffer, outputBuffer);
	inputBuffer->setParent(connection);
	outputBuffer->setParent(connection);

	inputBuffer->write(data);
	inputBuffer->seek(0);

	while (connection->state() != Pillow::HttpConnection::SendingHeaders)
		QCoreApplication::processEvents();

	return connection;
}

void HttpHandlerTestBase::requestCompleted(Pillow::HttpConnection* connection)
{
	QCoreApplication::processEvents();
	response = responseBuffer;
	responseBuffer = QByteArray();
	requestParams = connection->requestParams();
}

void HttpHandlerTestBase::outputBuffer_bytesWritten()
{
	QBuffer* buffer = static_cast<QBuffer*>(sender());
	responseBuffer.append(buffer->data());
	if (buffer->isOpen())
		buffer->seek(0);
}

class MockHandler : public Pillow::HttpHandler
{
public:
	MockHandler(const QByteArray& acceptPath, int statusCode, QObject* parent)
	    : Pillow::HttpHandler(parent), acceptPath(acceptPath), statusCode(statusCode), handleRequestCount(0)
	{}

	QByteArray acceptPath;
	int statusCode;
	int handleRequestCount;

	bool handleRequest(Pillow::HttpConnection* connection)
	{
		++handleRequestCount;

		if (acceptPath == connection->requestPath())
		{
			connection->writeResponse(statusCode);
			return true;
		}
		return false;
	}
};
