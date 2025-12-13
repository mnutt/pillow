#include "HttpsServer.h"
#include "HttpConnection.h"
#include <QtNetwork/QSslSocket>
using namespace Pillow;

#if !defined(PILLOW_NO_SSL) && !defined(QT_NO_SSL)

//
// HttpsServer
//

HttpsServer::HttpsServer(QObject* parent) : HttpServer(parent) {}

HttpsServer::HttpsServer(const QSslCertificate& certificate, const QSslKey& privateKey, const QHostAddress& serverAddress,
                         quint16 serverPort, QObject* parent)
    : HttpServer(serverAddress, serverPort, parent), _certificate(certificate), _privateKey(privateKey)
{}

void HttpsServer::setCertificate(const QSslCertificate& certificate)
{
	_certificate = certificate;
}

void HttpsServer::setPrivateKey(const QSslKey& privateKey)
{
	_privateKey = privateKey;
}

void HttpsServer::incomingConnection(qintptr socketDescriptor)
{
	QSslSocket* sslSocket = new QSslSocket(this);
	if (sslSocket->setSocketDescriptor(socketDescriptor))
	{
		sslSocket->setPrivateKey(privateKey());
		sslSocket->setLocalCertificate(certificate());
		connect(sslSocket, &QSslSocket::sslErrors, this, &HttpsServer::sslSocket_sslErrors);
		connect(sslSocket, &QSslSocket::encrypted, this, &HttpsServer::sslSocket_encrypted);
		sslSocket->startServerEncryption();
		// Don't initialize the HttpConnection yet - wait for encrypted() signal
	}
	else
	{
		qWarning() << "HttpsServer::incomingConnection: failed to set socket descriptor '" << socketDescriptor << "' on ssl socket.";
		delete sslSocket;
	}
}

void HttpsServer::sslSocket_sslErrors(const QList<QSslError>&) {}

void HttpsServer::sslSocket_encrypted()
{
	QSslSocket* sslSocket = qobject_cast<QSslSocket*>(sender());
	if (sslSocket)
	{
		addPendingConnection(sslSocket);
		nextPendingConnection();
		createHttpConnection()->initialize(sslSocket, sslSocket);
	}
}

#endif // !defined(PILLOW_NO_SSL) && !defined(QT_NO_SSL)
