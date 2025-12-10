#ifndef HTTPSERVERBASE_H
#define HTTPSERVERBASE_H

#include <QObject>
#include <QPointer>

namespace Pillow
{
	class HttpServer;
	class HttpConnection;
}
class QIODevice;

ulong qHash(const QPointer<Pillow::HttpConnection>& ptr);

class HttpServerTestBase : public QObject
{
    Q_OBJECT

protected slots: // Test slots.
	void init();
	void cleanup();
	
	void testInit() { cleanup(); init(); }
	void testHandlesConnectionsAsRequests();
	void testHandlesConcurrentConnections();
	void testHandlesConcurrentConnectionsSimultaneousResponses();
	void testReusesRequests();
	void testDestroysRequests();
	
protected:
	QObject* server;
	QList<Pillow::HttpConnection*> handledRequests;
	QList<QPointer<Pillow::HttpConnection> > guardedHandledRequests;

	void sendRequest(QIODevice* device, const QByteArray& content);
	void sendResponses();
	void sendConcurrentRequests(int concurrencyLevel);
	
protected slots:
	void requestReady(Pillow::HttpConnection* request);
	
protected:
	virtual QObject* createServer() = 0;
	virtual QIODevice* createClientConnection() = 0;	
};

#endif // HTTPSERVERBASE_H