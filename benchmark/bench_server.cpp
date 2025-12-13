#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QDebug>

#include "HttpServer.h"
#include "HttpHandler.h"
#include "HttpConnection.h"

static const QByteArray jsonContentType("application/json");
static const QByteArray textContentType("text/plain");
static const QByteArray helloResponse("Hello, World!");
static const QByteArray jsonResponse("{\"message\":\"Hello, World!\"}");

class BenchHandler : public Pillow::HttpHandler
{
    Q_OBJECT

public:
    enum ResponseType
    {
        PlainText,
        Json,
        Empty
    };

    BenchHandler(ResponseType type, QObject* parent = nullptr) : Pillow::HttpHandler(parent), m_type(type) {}

    bool handleRequest(Pillow::HttpConnection* connection) override
    {
        Pillow::HttpHeaderCollection headers;

        switch (m_type)
        {
        case PlainText:
            headers.append(Pillow::HttpHeader("Content-Type", textContentType));
            connection->writeResponse(200, headers, helloResponse);
            break;
        case Json:
            headers.append(Pillow::HttpHeader("Content-Type", jsonContentType));
            connection->writeResponse(200, headers, jsonResponse);
            break;
        case Empty:
            connection->writeResponse(200, Pillow::HttpHeaderCollection(), QByteArray());
            break;
        }

        return true;
    }

private:
    ResponseType m_type;
};

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("pillow-benchmark");
    app.setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Pillow HTTP benchmark server");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption portOption(QStringList() << "p" << "port", "Port to listen on (default: 8080)", "port", "8080");
    parser.addOption(portOption);

    QCommandLineOption typeOption(QStringList() << "t" << "type", "Response type: text, json, empty (default: text)", "type", "text");
    parser.addOption(typeOption);

    parser.process(app);

    quint16 port = parser.value(portOption).toUShort();
    QString typeStr = parser.value(typeOption);

    BenchHandler::ResponseType responseType = BenchHandler::PlainText;
    if (typeStr == "json")
    {
        responseType = BenchHandler::Json;
    }
    else if (typeStr == "empty")
    {
        responseType = BenchHandler::Empty;
    }

    Pillow::HttpServer server(QHostAddress::Any, port);
    if (!server.isListening())
    {
        qCritical() << "Failed to start server on port" << port;
        return 1;
    }

    BenchHandler* handler = new BenchHandler(responseType, &server);
    QObject::connect(&server, &Pillow::HttpServer::requestReady, handler, &BenchHandler::handleRequest);

    qDebug() << "Pillow benchmark server listening on port" << port;
    qDebug() << "Response type:" << typeStr;
    qDebug() << "Press Ctrl+C to stop";

    return app.exec();
}

#include "bench_server.moc"
