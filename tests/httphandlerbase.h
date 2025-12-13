#pragma once

#include <QtCore/QObject>
#include <HttpConnection.h>

namespace Pillow
{
	class HttpConnection;
}

class HttpHandlerTestBase : public QObject
{
	Q_OBJECT

protected slots:
	void outputBuffer_bytesWritten();
	void requestCompleted(Pillow::HttpConnection* request);

protected:
	QByteArray responseBuffer;
	QByteArray response;
	Pillow::HttpParamCollection requestParams;

protected:
	Pillow::HttpConnection* createGetRequest(const QByteArray& path = "/", const QByteArray& httpVersion = "1.0");
	Pillow::HttpConnection* createPostRequest(const QByteArray& path = "/", const QByteArray& content = QByteArray(),
	                                          const QByteArray& httpVersion = "1.0");
	Pillow::HttpConnection* createRequest(const QByteArray& method, const QByteArray& path = "/", const QByteArray& content = QByteArray(),
	                                      const QByteArray& httpVersion = "1.0");
};
