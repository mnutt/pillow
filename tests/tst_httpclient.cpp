#include <QtCore/QObject>
#include <QtTest/QtTest>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkCookie>
#include <QtNetwork/QNetworkCookieJar>
#include <HttpClient.h>
#include <HttpConnection.h>
#include <HttpServer.h>
#include <HttpHandlerSimpleRouter.h>
#include <HttpHelpers.h>
#include "Helpers.h"
#include <functional>

typedef QList<QByteArray> Chunks;
Q_DECLARE_METATYPE(Chunks)

//
// HttpClient test class
//

class tst_HttpClient : public QObject
{
	Q_OBJECT

	Pillow::HttpClient* client;
	TestServer server;
	TestServer server2;

private slots:
	void initTestCase()
	{
		QVERIFY(server.listen(QHostAddress::LocalHost, 4569));
		QVERIFY(server2.listen(QHostAddress::LocalHost, 4570));
		qRegisterMetaType<Pillow::HttpConnection*>("Pillow::HttpConnection*");
	}

	void init()
	{
		printf("HTTP CLIENT TEST\n");
		qDebug() << "HTTP CLIENT TEST QDEBUG";
		client = new Pillow::HttpClient();
		QVERIFY(server.receivedRequests.isEmpty());
		QVERIFY(server.receivedConnections.isEmpty());
		QVERIFY(server.receivedSockets.isEmpty());
		QVERIFY(server2.receivedRequests.isEmpty());
		QVERIFY(server2.receivedConnections.isEmpty());
		QVERIFY(server2.receivedSockets.isEmpty());
	}

	void cleanup()
	{
		delete client;
		client = 0;
		server.receivedRequests.clear();
		server.receivedConnections.clear();
		server.receivedSockets.clear();
		server2.receivedRequests.clear();
		server2.receivedConnections.clear();
		server2.receivedSockets.clear();
	}

private:
	bool waitForResponse(int maxTime = 500)
	{
		QElapsedTimer t;
		t.start();
		while (client->responsePending() && t.elapsed() < maxTime)
			QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
		if (client->responsePending())
		{
			qWarning() << "Timed out waiting for response";
			return false;
		}
		return true;
	}

	bool waitForContentReadyRead(int maxTime = 500) { return waitForSignal(client, SIGNAL(contentReadyRead()), maxTime); }

	QUrl testUrl() { return QUrl("http://127.0.0.1:4569/test"); }

protected slots:
	void abortSender() { static_cast<Pillow::HttpClient*>(sender())->abort(); }
	void sendRequest() { client->get(testUrl()); }

private slots:
	void should_be_initially_blank()
	{
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QVERIFY(!client->responsePending());
		QCOMPARE(client->headers(), Pillow::HttpHeaderCollection());
		QCOMPARE(client->statusCode(), 0);
		QCOMPARE(client->content(), QByteArray());
	}

	void should_send_valid_requests_data()
	{
		QTest::addColumn<QByteArray>("method");
		QTest::addColumn<QUrl>("url");
		QTest::addColumn<Pillow::HttpHeaderCollection>("headers");
		QTest::addColumn<QByteArray>("content");
		QTest::addColumn<HttpRequestData>("expectedRequestData");

		Pillow::HttpHeaderCollection baseExpectedHeaders;
		baseExpectedHeaders << Pillow::HttpHeader("Host", "127.0.0.1:4569");

		QTest::newRow("Simple GET") << QByteArray("GET") << QUrl("http://127.0.0.1:4569/") << Pillow::HttpHeaderCollection() << QByteArray()
		                            << HttpRequestData()
		                                   .withMethod("GET")
		                                   .withUri("/")
		                                   .withPath("/")
		                                   .withQueryString("")
		                                   .withFragment("")
		                                   .withHttpVersion("HTTP/1.1")
		                                   .withContent("")
		                                   .withHeaders(baseExpectedHeaders);

		QTest::newRow("GET with headers") << QByteArray("GET") << QUrl("http://127.0.0.1:4569/some/path?and=query")
		                                  << (Pillow::HttpHeaderCollection() << Pillow::HttpHeader("X-Some", "Header")
		                                                                     << Pillow::HttpHeader("X-And-Another", "even-better; header"))
		                                  << QByteArray()
		                                  << HttpRequestData()
		                                         .withMethod("GET")
		                                         .withUri("/some/path?and=query")
		                                         .withPath("/some/path")
		                                         .withQueryString("and=query")
		                                         .withFragment("")
		                                         .withHttpVersion("HTTP/1.1")
		                                         .withContent("")
		                                         .withHeaders(Pillow::HttpHeaderCollection(baseExpectedHeaders)
		                                                      << Pillow::HttpHeader("X-Some", "Header")
		                                                      << Pillow::HttpHeader("X-And-Another", "even-better; header"));
		QTest::newRow("GET with connection: close")
		    << QByteArray("GET") << QUrl("http://127.0.0.1:4569/")
		    << (Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Connection", "close")) << QByteArray()
		    << HttpRequestData()
		           .withMethod("GET")
		           .withUri("/")
		           .withPath("/")
		           .withQueryString("")
		           .withFragment("")
		           .withHttpVersion("HTTP/1.1")
		           .withContent("")
		           .withHeaders(Pillow::HttpHeaderCollection(baseExpectedHeaders) << Pillow::HttpHeader("Connection", "close"));
		QTest::newRow("Simple PUT") << QByteArray("PUT") << QUrl("http://127.0.0.1:4569/some/path") << (Pillow::HttpHeaderCollection())
		                            << QByteArray("Some sent data")
		                            << HttpRequestData()
		                                   .withMethod("PUT")
		                                   .withUri("/some/path")
		                                   .withPath("/some/path")
		                                   .withQueryString("")
		                                   .withFragment("")
		                                   .withHttpVersion("HTTP/1.1")
		                                   .withContent("Some sent data")
		                                   .withHeaders(Pillow::HttpHeaderCollection(baseExpectedHeaders)
		                                                << Pillow::HttpHeader("Content-Length", "14"));
		QTest::newRow("Large POST") << QByteArray("POST") << QUrl("http://127.0.0.1:4569/some/large/path")
		                            << (Pillow::HttpHeaderCollection()) << QByteArray(128 * 1024, '*')
		                            << HttpRequestData()
		                                   .withMethod("POST")
		                                   .withUri("/some/large/path")
		                                   .withPath("/some/large/path")
		                                   .withQueryString("")
		                                   .withFragment("")
		                                   .withHttpVersion("HTTP/1.1")
		                                   .withContent(QByteArray(128 * 1024, '*'))
		                                   .withHeaders(Pillow::HttpHeaderCollection(baseExpectedHeaders)
		                                                << Pillow::HttpHeader("Content-Length", "131072"));
	}

	void should_send_valid_requests()
	{
		QFETCH(QByteArray, method);
		QFETCH(QUrl, url);
		QFETCH(Pillow::HttpHeaderCollection, headers);
		QFETCH(QByteArray, content);
		QFETCH(HttpRequestData, expectedRequestData);

		client->request(method, url, headers, content);

		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedRequests.size(), 1);
		QCOMPARE(server.receivedRequests.first(), expectedRequestData);
	}

	void should_add_host_header_to_requests()
	{
		client->get(QUrl("http://127.0.0.1:4569/"));
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedConnections.size(), 1);
		QCOMPARE(server.receivedConnections.last()->requestHeaderValue("Host"), QByteArray("127.0.0.1:4569"));
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());

		// Verify that connecting to another server sends an updated host header.
		TestServer otherServer;
		QVERIFY(otherServer.listen(QHostAddress::LocalHost, 4578));

		client->get(QUrl("http://127.0.0.1:4578/"));
		QVERIFY(otherServer.waitForRequest());
		QCOMPARE(otherServer.receivedConnections.size(), 1);
		QCOMPARE(otherServer.receivedConnections.last()->requestHeaderValue("Host"), QByteArray("127.0.0.1:4578"));
	}

	void should_be_pending_response_after_sending_request()
	{
		QVERIFY(!client->responsePending());
		client->get(testUrl());
		QVERIFY(client->responsePending());
	}

	void should_receive_response()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QVERIFY(client->responsePending());
		server.receivedConnections.first()->writeResponse();
		QVERIFY(waitForResponse());
		QVERIFY(!client->responsePending());
	}

	void should_parse_received_response_data()
	{
		qRegisterMetaType<Chunks>("Chunks");
		QTest::addColumn<int>("statusCode");
		QTest::addColumn<Pillow::HttpHeaderCollection>("headers");
		QTest::addColumn<Chunks>("contentChunks");
		QTest::addColumn<Pillow::HttpHeaderCollection>("expectedHeaders");

		QTest::newRow("Simple 200") << 200 << (Pillow::HttpHeaderCollection()) << (Chunks() << "Hello World")
		                            << (Pillow::HttpHeaderCollection()
		                                << Pillow::HttpHeader("Content-Length", "11") << Pillow::HttpHeader("Content-Type", "text/plain"));
		QTest::newRow("Many headers") << 200
		                              << (Pillow::HttpHeaderCollection()
		                                  << Pillow::HttpHeader("a", "b") << Pillow::HttpHeader("c", "d") << Pillow::HttpHeader("e", "f")
		                                  << Pillow::HttpHeader("g", "hhhhhhhhhhhhhhhh"))
		                              << (Chunks() << "Hello World")
		                              << (Pillow::HttpHeaderCollection()
		                                  << Pillow::HttpHeader("a", "b") << Pillow::HttpHeader("c", "d") << Pillow::HttpHeader("e", "f")
		                                  << Pillow::HttpHeader("g", "hhhhhhhhhhhhhhhh") << Pillow::HttpHeader("Content-Length", "11")
		                                  << Pillow::HttpHeader("Content-Type", "text/plain"));
		QTest::newRow("No Content") << 304 << (Pillow::HttpHeaderCollection()) << (Chunks())
		                            << (Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Content-Length", "0"));
		QTest::newRow("Chunked Response") << 404
		                                  << (Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Transfer-Encoding", "Chunked")
		                                                                     << Pillow::HttpHeader("Content-Type", "chunky/bacon"))
		                                  << (Chunks() << "Hello" << "the" << "world!")
		                                  << (Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Content-Type", "chunky/bacon")
		                                                                     << Pillow::HttpHeader("Transfer-Encoding", "Chunked"));

		QTest::newRow("Delicious Stuff") << 400
		                                 << (Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Content-Type", "delicious-chocolate"))
		                                 << (Chunks() << "I like chocolate!")
		                                 << (Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Content-Length", "17")
		                                                                    << Pillow::HttpHeader("Content-Type", "delicious-chocolate"));
	}

	void should_parse_received_response()
	{
		QFETCH(int, statusCode);
		QFETCH(Pillow::HttpHeaderCollection, headers);
		QFETCH(Chunks, contentChunks);
		QFETCH(Pillow::HttpHeaderCollection, expectedHeaders);
		QByteArray expectedContent;

		client->get(testUrl());
		QVERIFY(server.waitForRequest());

		if (contentChunks.isEmpty())
			server.receivedConnections.first()->writeResponse(statusCode, headers, QByteArray());
		else if (contentChunks.size() == 1)
		{
			server.receivedConnections.first()->writeResponse(statusCode, headers, contentChunks.first());
			expectedContent.append(contentChunks.first());
		}
		else
		{
			server.receivedConnections.first()->writeHeaders(statusCode, headers);
			foreach (const QByteArray& chunk, contentChunks)
			{
				server.receivedConnections.first()->writeContent(chunk);
				expectedContent.append(chunk);
			}
			server.receivedConnections.first()->endContent();
		}
		QVERIFY(waitForResponse());

		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(client->statusCode(), statusCode);
		QCOMPARE(client->headers(), expectedHeaders);
		QCOMPARE(client->content(), expectedContent);
	}

	void should_allow_sending_and_receiving_multiple_requests_one_after_another()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedRequests.size(), 1);
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedRequests.size(), 2);
		server.receivedConnections.last()->writeResponse(201);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 201);

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedRequests.size(), 3);
		server.receivedConnections.last()->writeResponse(202);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 202);
	}

	void should_clear_the_previous_response_when_sending_a_new_request()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedRequests.size(), 1);
		server.receivedConnections.last()->writeResponse(404, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Hello", "World"),
		                                                 "Hello World!");
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 404);
		QVERIFY(!client->headers().isEmpty());
		QVERIFY(!client->content().isEmpty());

		client->get(testUrl());
		QCOMPARE(client->statusCode(), 0);
		QVERIFY(client->headers().isEmpty());
		QVERIFY(client->content().isEmpty());
	}

	void should_not_send_another_request_while_response_is_pending_because_it_does_not_support_pipelining()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedRequests.size(), 1);
		QVERIFY(client->responsePending());

		QTest::ignoreMessage(
		    QtWarningMsg,
		    "Pillow::HttpClient::request: cannot send new request while another one is under way. Request pipelining is not supported.");
		client->get(testUrl());
		QCOMPARE(server.receivedRequests.size(), 1);

		server.receivedConnections.first()->writeResponse(200, Pillow::HttpHeaderCollection(), "Hello from Pillow");
		QVERIFY(waitForResponse());

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedRequests.size(), 2);
	}

	void should_allow_sending_subsequent_requests_to_different_hosts()
	{
		// First request, to main server.
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QVERIFY(server.receivedConnections.size() == 1);
		QPointer<QObject> firstSocket = server.receivedSockets.first();
		QVERIFY(firstSocket != 0);
		QVERIFY(client->responsePending());
		server.receivedConnections.first()->writeResponse(200);
		QVERIFY(waitForResponse());

		// Second request, to a different server.
		QVERIFY(server2.receivedConnections.size() == 0);
		client->get(QUrl("http://127.0.0.1:4570/"));
		QVERIFY(server2.waitForRequest());
		QVERIFY(server2.receivedConnections.size() == 1);
		QVERIFY(client->responsePending());
		server2.receivedConnections.first()->writeResponse(200);
		QVERIFY(waitForResponse());
		QVERIFY(firstSocket == 0); // The first connection should have been broken by the client connecting to a different host.

		// And back to the first server.
		QVERIFY(server.receivedConnections.size() == 1);
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QVERIFY(server.receivedConnections.size() == 2);
	}

	void should_report_network_errors()
	{
		client->get(QUrl("http://popopopopopopopopopopopopo.popo:64999/should/not/work"));
		QVERIFY(client->responsePending());
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NetworkError);
		QCOMPARE(client->statusCode(), 0);
		QVERIFY(!client->responsePending());

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.first()->writeResponse(200);
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
	}

	void should_report_error_and_close_connection_on_invalid_responses()
	{
		// Else, we may try to reuse a socket where our state is bad with the server.
		client->get(testUrl());
		QVERIFY(server.waitForRequest());

		QCOMPARE(server.receivedSockets.size(), 1);
		QCOMPARE(server.receivedSockets.last()->state(), QAbstractSocket::ConnectedState);
		QPointer<QTcpSocket> serverSideSocket = server.receivedSockets.last();
		qRegisterMetaType<QAbstractSocket::SocketState>("QAbstractSocket::SocketState");
		QSignalSpy serverSocketStateSpy(serverSideSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)));

		server.receivedConnections.last()->outputDevice()->write("=-=-=-=-=-FFFFFFFUUUUUUUUUUU=-=-=-=-=-=!\r\n");
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::ResponseInvalidError);

		QVERIFY(waitFor([&] { return serverSideSocket == 0; }));
		QVERIFY(serverSideSocket == 0);
		QVERIFY(!serverSocketStateSpy.isEmpty());
		QCOMPARE(serverSocketStateSpy.last().first().value<QAbstractSocket::SocketState>(), QAbstractSocket::UnconnectedState);
	}

	void should_clear_a_previous_error_when_sending_a_new_request()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->outputDevice()->write("=-=-=-=-=-FFFFFFFUUUUUUUUUUU=-=-=-=-=-=!\r\n");
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::ResponseInvalidError);

		client->get(testUrl());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse();
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
	}

	void should_report_error_if_server_closes_connection_early()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->close();
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::RemoteHostClosedError);

		client->get(testUrl());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse();
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedSockets.last()->write("HTTP/1.1 200 OK\r\n");
		server.receivedSockets.last()->flush();
		server.receivedConnections.last()->close();
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::RemoteHostClosedError);

		client->get(testUrl());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(201);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 201);
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedSockets.last()->write(
		    "HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\nhello world"); // Missing one byte in the content!
		server.receivedSockets.last()->flush();
		QTest::qWait(50);
		server.receivedConnections.last()->close();
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::RemoteHostClosedError);
	}

	void should_not_report_error_if_server_closes_connection_after_responding()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(201);
		server.receivedConnections.last()->close();
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 201);
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
	}

	void should_silently_ignore_unexpected_data_received_while_not_waiting_for_response_and_close_connection()
	{
		// Example where server returns an extra, valid HTTP response, such as a
		// server indicating a timeout on a keep-alive connection.
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(202);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 202);

		QPointer<QTcpSocket> socket = server.receivedSockets.last();
		socket->write("HTTP/1.0 400 Bad Request\r\nConnection: close\r\n\r\nThis connection was inactive for too long!");

		waitFor([&] { return socket == 0; }); // The client should close the socket, which will close and delete it on the server.
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(client->statusCode(), 202);

		// Example where server returns extra junk.
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(202);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 202);

		socket = server.receivedSockets.last();
		socket->write("=-=-=-=-=-FFFFFFFUUUUUUUUUUUU!");

		waitFor([&] { return socket == 0; }); // The client should close the socket, which will close and delete it on the server.
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(client->statusCode(), 202);
	}

	void should_emit_finished_after_receiving_response()
	{
		QSignalSpy finishedSpy(client, SIGNAL(finished()));

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.first()->writeResponse(202);
		QVERIFY(waitForResponse());

		QCOMPARE(client->statusCode(), 202);
		QCOMPARE(finishedSpy.size(), 1);
	}

	void should_emit_finished_after_encountering_an_error()
	{
		QSignalSpy finishedSpy(client, SIGNAL(finished()));

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.first()->close();
		QVERIFY(waitForResponse());

		QCOMPARE(client->statusCode(), 0);
		QCOMPARE(finishedSpy.size(), 1);
	}

	void should_emit_finished_when_aborted()
	{
		QSignalSpy finishedSpy(client, SIGNAL(finished()));

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		client->abort();
		QVERIFY(waitForResponse());

		QCOMPARE(client->statusCode(), 0);
		QCOMPARE(finishedSpy.size(), 1);
	}

	void should_be_abortable()
	{
		client->abort(); // Should not do anything besides a warning.
		QVERIFY(client->error() == Pillow::HttpClient::NoError);

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		client->abort();
		server.receivedConnections.last()->writeResponse(202);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 0);
		QCOMPARE(client->error(), Pillow::HttpClient::AbortedError);

		// And recover from it.
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(202);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 202);
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
	}

	void should_close_connection_if_aborted_while_waiting_for_response()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QVERIFY(server.receivedSockets.last()->state() == QAbstractSocket::ConnectedState);
		QPointer<QTcpSocket> socket = server.receivedSockets.last();
		QVERIFY(socket != 0);
		client->abort();
		QVERIFY(waitForResponse());
		QVERIFY(waitFor([=] { return socket == 0; })); // The socket should get closed and deleted.
	}

	void should_close_connection_and_not_discard_previous_response_if_aborted_while_not_waiting_for_response()
	{
		QSignalSpy finishedSpy(client, SIGNAL(finished()));

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QPointer<QTcpSocket> socket = server.receivedSockets.last();
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());

		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(finishedSpy.size(), 1);
		QVERIFY(socket != 0);
		QVERIFY(!client->responsePending());

		client->abort();
		QVERIFY(waitFor([=] { return socket == 0; })); // The socket should get closed and deleted.

		// Should not have modified the results from the previous response, nor emitted finished() again.
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(finishedSpy.size(), 1);
	}

	void should_reuse_existing_connection_to_same_host_and_port()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(201);
		QVERIFY(waitForResponse());

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(202);
		QVERIFY(waitForResponse());

		QCOMPARE(server.receivedSockets.size(), 3);
		QVERIFY(server.receivedSockets.at(0) == server.receivedSockets.at(1) &&
		        server.receivedSockets.at(1) == server.receivedSockets.at(2));
	}

	void should_allow_consuming_partial_responses_to_support_streaming()
	{
		// With chunked transfer encoding.
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeHeaders(201, Pillow::HttpHeaderCollection()
		                                                         << Pillow::HttpHeader("Transfer-Encoding", "chunked"));
		server.receivedConnections.last()->writeContent("hello");
		QVERIFY(waitForContentReadyRead());
		QCOMPARE(client->statusCode(), 201);
		QCOMPARE(client->content(), QByteArray("hello"));
		QCOMPARE(client->consumeContent(), QByteArray("hello"));
		QCOMPARE(client->content(), QByteArray());
		QCOMPARE(client->consumeContent(), QByteArray());

		server.receivedConnections.last()->writeContent(" ");
		QVERIFY(waitForContentReadyRead());
		QCOMPARE(client->consumeContent(), QByteArray(" "));

		server.receivedConnections.last()->writeContent("world!");
		QVERIFY(waitForContentReadyRead());
		QCOMPARE(client->consumeContent(), QByteArray("world!"));

		server.receivedConnections.last()->endContent();
		QVERIFY(waitForResponse());

		QCOMPARE(client->content(), QByteArray());
		QCOMPARE(client->consumeContent(), QByteArray());

		// Without chunked transfer encoding.
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeHeaders(202);
		server.receivedConnections.last()->writeContent("hello");
		QVERIFY(waitForContentReadyRead());
		QCOMPARE(client->statusCode(), 202);
		QCOMPARE(client->content(), QByteArray("hello"));
		QCOMPARE(client->consumeContent(), QByteArray("hello"));
		QCOMPARE(client->content(), QByteArray());
		QCOMPARE(client->consumeContent(), QByteArray());

		server.receivedConnections.last()->writeContent(" world!");
		QVERIFY(waitForContentReadyRead());
		QCOMPARE(client->consumeContent(), QByteArray(" world!"));

		server.receivedConnections.last()->endContent();
		QVERIFY(waitForResponse());

		QCOMPARE(client->content(), QByteArray());
		QCOMPARE(client->consumeContent(), QByteArray());
	}

	void should_ignore_100_continue_responses()
	{
		QSignalSpy headersCompleteSpy(client, SIGNAL(headersCompleted()));
		QSignalSpy finishedSpy(client, SIGNAL(finished()));

		client->post(testUrl(), Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Expect", "100-continue"), "post data");
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(201, Pillow::HttpHeaderCollection(), "content");
		QVERIFY(waitForResponse(500));
		QCOMPARE(client->statusCode(), 201);
		QCOMPARE(client->content(), QByteArray("content"));
		QCOMPARE(headersCompleteSpy.size(), 1);
		QCOMPARE(finishedSpy.size(), 1);
	}

	void should_emit_headersCompleted_when_headers_have_been_received()
	{
		client->post(testUrl(), Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Expect", "100-continue"), "post data");
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeHeaders(200, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("First", "FirstValue")
		                                                                                    << Pillow::HttpHeader("Second", "SecondValue")
		                                                                                    << Pillow::HttpHeader("Content-Length", "12"));

		QCOMPARE(client->statusCode(), 0);
		QCOMPARE(client->headers(), Pillow::HttpHeaderCollection());

		QSignalSpy finishedSpy(client, SIGNAL(finished()));
		QVERIFY(waitForSignal(client, SIGNAL(headersCompleted())));
		QVERIFY(finishedSpy.isEmpty()); // Should not have finished the request yet.

		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->headers(), Pillow::HttpHeaderCollection()
		                                << Pillow::HttpHeader("First", "FirstValue") << Pillow::HttpHeader("Second", "SecondValue")
		                                << Pillow::HttpHeader("Content-Length", "12") << Pillow::HttpHeader("Content-Type", "text/plain"));
		QCOMPARE(client->content(), QByteArray());

		server.receivedConnections.last()->writeContent("Some content");
		QVERIFY(waitForSignal(client, SIGNAL(finished())));
		QCOMPARE(finishedSpy.size(), 1);

		QCOMPARE(client->content(), QByteArray("Some content"));
	}

	void should_report_error_given_an_unsupported_request() { QSKIP("Not implemented", SkipAll); }

	void should_support_head_requests()
	{
		client->head(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200, Pillow::HttpHeaderCollection(), "12345");
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->content(), QByteArray());

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(201, Pillow::HttpHeaderCollection(), "abcde");
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(client->statusCode(), 201);
		QCOMPARE(client->content(), QByteArray("abcde"));
	}

	void should_have_a_configurable_read_buffer_so_it_can_report_tcp_congestion()
	{
		const QByteArray oneMiB(1024 * 1024, '~');

		client->setReadBufferSize(32 * 1024);

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200, Pillow::HttpHeaderCollection(), oneMiB);
		QVERIFY(waitForSignal(client, SIGNAL(contentReadyRead())));

		QVERIFY(client->content().size() < 1024 * 1024);
		// Note: We don't check bytesToWrite() > 0 here because it's timing-sensitive.
		// On fast systems, the kernel may have already accepted all data into its buffers.

		QByteArray read;
		int readCount = 0;
		QVERIFY(waitFor([&]() -> bool {
			readCount++;
			read.append(client->consumeContent());
			return read.size() == 1024 * 1024;
		}));

		QVERIFY(!client->responsePending());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->content(), QByteArray());
		QVERIFY(readCount > 1); // Multiple reads required due to buffer limiting
		QCOMPARE(read, oneMiB);
	}

	void should_allow_specifying_connection_keepAliveTimeout()
	{
		QCOMPARE(client->keepAliveTimeout(), -1); // -1 Means infinite keep-alive.

		// Warning: timing-sensitive test.
		client->setKeepAliveTimeout(50);

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QPointer<QObject> socket = server.receivedSockets.last();
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);

		QTest::qWait(60); // Wait more than the keep alive timeout.

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QPointer<QObject> socket2 = server.receivedSockets.last();
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);

		QVERIFY(socket == 0); // It will have closed the initial socket and used a new one.
		QVERIFY(socket2 != 0);

		QTest::qWait(1); // Wait less than the keep alive timeout.

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);

		QVERIFY(socket2 != 0); // It will have reused the still-good connection.
	}

	void should_disable_keep_alive_when_keepAliveTimeout_is_zero()
	{
		client->setKeepAliveTimeout(0);

		client->get(testUrl());
		QVERIFY(server.waitForRequest());

		QSignalSpy connectionClosedSpy(server.receivedConnections.last(), SIGNAL(closed(Pillow::HttpConnection*)));
		QPointer<QObject> socket = server.receivedSockets.last();

		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QVERIFY(waitFor([&] { return connectionClosedSpy.size() == 1; }));

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);

		QVERIFY(socket == 0);
	}

	void should_break_existing_connection_when_keepAliveTimeout_exceeded()
	{
		// Warning: timing-sensitive test.

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QPointer<QObject> socket = server.receivedSockets.last();
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);

		QVERIFY(socket != 0);
		QVERIFY(server.receivedSockets.at(0) == server.receivedSockets.at(1)); // Still good.

		QTest::qWait(15);                // But wait, there's more!
		client->setKeepAliveTimeout(10); // Un-oh, we're already past that!

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);

		QVERIFY(socket == 0); // It will have broken the initial connection.
	}

	void should_be_abortable_even_when_has_not_sent_request()
	{
		client->get(testUrl());
		client->abort();
		client->post(testUrl(), Pillow::HttpHeaderCollection(), "hello");
		client->abort();
		client->put(testUrl(), Pillow::HttpHeaderCollection(), "world!");

		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedConnections.last()->requestMethod(), QByteArray("PUT"));
		QCOMPARE(server.receivedConnections.last()->requestContent(), QByteArray("world!"));
		server.receivedConnections.last()->writeResponse(201, Pillow::HttpHeaderCollection(), "Hello back!");

		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 201);
		QCOMPARE(client->content(), QByteArray("Hello back!"));

		// Should still work following a keep-alive request.
		client->get(testUrl());
		client->abort();
		client->post(testUrl(), Pillow::HttpHeaderCollection(), "hello");

		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedConnections.last()->requestMethod(), QByteArray("POST"));
		QCOMPARE(server.receivedConnections.last()->requestContent(), QByteArray("hello"));
		server.receivedConnections.last()->writeResponse(200, Pillow::HttpHeaderCollection(), "Hello again");

		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->content(), QByteArray("Hello again"));
	}

	void should_be_abortable_from_within_headersCompleted()
	{
		QSignalSpy contentReadyReadSpy(client, SIGNAL(contentReadyRead()));
		connect(client, SIGNAL(headersCompleted()), this, SLOT(abortSender()));

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeHeaders(201);
		server.receivedConnections.last()->writeContent("Hello");
		QVERIFY(waitForResponse());
		QCOMPARE(contentReadyReadSpy.size(), 0); // It should be aborted before emitting contentReadyRead.
		QCOMPARE(client->error(), Pillow::HttpClient::AbortedError);
		QCOMPARE(client->statusCode(), 201);

		// Recover
		disconnect(client, SIGNAL(headersCompleted()), this, SLOT(abortSender()));
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(404, Pillow::HttpHeaderCollection(), "Test");
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 404);
		QCOMPARE(client->content(), QByteArray("Test"));
		QCOMPARE(contentReadyReadSpy.size(), 1);

		// Wild case: abort previous and send new request from within slot.
		connect(client, SIGNAL(headersCompleted()), this, SLOT(abortSender()));
		connect(client, SIGNAL(headersCompleted()), this, SLOT(sendRequest()));
		contentReadyReadSpy.clear();

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedConnections.size(), 3);
		server.receivedConnections.last()->writeResponse(200, Pillow::HttpHeaderCollection(), "Bla bla");
		QVERIFY(waitForSignal(client, SIGNAL(finished())));
		QCOMPARE(contentReadyReadSpy.size(), 0); // It should be aborted before emitting contentReadyRead.
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QVERIFY(client->responsePending());

		// Recover
		disconnect(client, SIGNAL(headersCompleted()), this, SLOT(abortSender()));
		disconnect(client, SIGNAL(headersCompleted()), this, SLOT(sendRequest()));
		QVERIFY(waitFor(
		    [&] { return server.receivedConnections.size() == 4; })); // At this point, we will or have already received the new request.
		server.receivedConnections.last()->writeResponse(201, Pillow::HttpHeaderCollection(), "Testing 123");
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(client->statusCode(), 201);
		QCOMPARE(client->content(), QByteArray("Testing 123"));
		QVERIFY(!client->responsePending());
	}

	void should_be_abortable_from_within_contentReadyRead()
	{
		connect(client, SIGNAL(contentReadyRead()), this, SLOT(abortSender()));

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeHeaders(201);
		server.receivedConnections.last()->writeContent("Hello");
		server.receivedConnections.last()->writeContent("World");
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::AbortedError);

		// Recover
		disconnect(client, SIGNAL(contentReadyRead()), this, SLOT(abortSender()));
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(404, Pillow::HttpHeaderCollection(), "Test");
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 404);
		QCOMPARE(client->content(), QByteArray("Test"));

		// Wild case: abort previous and send new request from within slot.
		connect(client, SIGNAL(contentReadyRead()), this, SLOT(abortSender()));
		connect(client, SIGNAL(contentReadyRead()), this, SLOT(sendRequest()));

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedConnections.size(), 3);
		server.receivedConnections.last()->writeHeaders(200);
		server.receivedConnections.last()->writeContent("Bla bla");
		QVERIFY(waitForSignal(client, SIGNAL(finished())));
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QVERIFY(client->responsePending());

		// Recover
		disconnect(client, SIGNAL(contentReadyRead()), this, SLOT(abortSender()));
		disconnect(client, SIGNAL(contentReadyRead()), this, SLOT(sendRequest()));
		QVERIFY(waitFor(
		    [&] { return server.receivedConnections.size() == 4; })); // At this point, we will or have already received the new request.
		server.receivedConnections.last()->writeResponse(201, Pillow::HttpHeaderCollection(), "Testing 123");
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(client->statusCode(), 201);
		QCOMPARE(client->content(), QByteArray("Testing 123"));
		QVERIFY(!client->responsePending());
	}

	void should_allow_sending_new_request_from_within_finished()
	{
		connect(client, SIGNAL(finished()), this, SLOT(sendRequest()));

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200, Pillow::HttpHeaderCollection(), "123");
		waitForSignal(client, SIGNAL(finished()));

		// The previous request will already be cleared here.
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(client->statusCode(), 0);
		QCOMPARE(client->content(), QByteArray());
		QVERIFY(client->responsePending());

		QVERIFY(waitFor(
		    [&] { return server.receivedConnections.size() == 2; })); // At this point, we will or have already received the new request.

		disconnect(client, SIGNAL(finished()), this, SLOT(sendRequest()));

		server.receivedConnections.last()->writeResponse(400, Pillow::HttpHeaderCollection(), "abcd");
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(client->statusCode(), 400);
		QCOMPARE(client->content(), QByteArray("abcd"));
		QVERIFY(!client->responsePending());

		// Good. Now let's try a chain of those.
		connect(client, SIGNAL(finished()), this, SLOT(sendRequest()));

		client->get(testUrl());
		QVERIFY(waitFor([&] { return server.receivedConnections.size() == 3; }));
		server.receivedConnections.last()->writeResponse(200, Pillow::HttpHeaderCollection(), "123");
		waitForSignal(client, SIGNAL(finished()));
		QVERIFY(waitFor([&] { return server.receivedConnections.size() == 4; }));
		server.receivedConnections.last()->writeResponse(201, Pillow::HttpHeaderCollection(), QByteArray(256 * 1024 - 13, '*'));
		waitForSignal(client, SIGNAL(finished()));
		QVERIFY(waitFor([&] { return server.receivedConnections.size() == 5; }));
		server.receivedConnections.last()->writeResponse(400, Pillow::HttpHeaderCollection(), QByteArray(4 * 1024 + 17, '*'));
		waitForSignal(client, SIGNAL(finished()));
		QVERIFY(waitFor([&] { return server.receivedConnections.size() == 6; }));
		server.receivedConnections.last()->writeResponse(404, Pillow::HttpHeaderCollection(), "7");
		waitForSignal(client, SIGNAL(finished()));
		disconnect(client, SIGNAL(finished()), this, SLOT(sendRequest()));

		QVERIFY(waitFor([&] { return server.receivedConnections.size() == 7; }));
		server.receivedConnections.last()->writeResponse(500, Pillow::HttpHeaderCollection(), "hello");
		QVERIFY(waitForResponse());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
		QCOMPARE(client->statusCode(), 500);
		QCOMPARE(client->content(), QByteArray("hello"));
		QVERIFY(!client->responsePending());
	}

	void should_use_port_80_by_default()
	{
		// How to test this?
	}

	void should_detect_redirects()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QVERIFY(!client->redirected());
		QCOMPARE(client->redirectionLocation(), QByteArray());

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(301, Pillow::HttpHeaderCollection()
		                                                          << Pillow::HttpHeader("Location", "http://new-location.example.org/"));
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 301);
		QVERIFY(client->redirected());
		QCOMPARE(client->redirectionLocation(), QByteArray("http://new-location.example.org/"));

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(201);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 201);
		QVERIFY(!client->redirected());
		QCOMPARE(client->redirectionLocation(), QByteArray());

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(
		    302, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Location", "http://another-location.example.org/and/path"));
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 302);
		QVERIFY(client->redirected());
		QCOMPARE(client->redirectionLocation(), QByteArray("http://another-location.example.org/and/path"));
	}

	void should_allow_following_redirections()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(301, Pillow::HttpHeaderCollection()
		                                                          << Pillow::HttpHeader("Location", "http://127.0.0.1:4569/other/path"));
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 301);
		QVERIFY(client->redirected());
		QCOMPARE(client->redirectionLocation(), QByteArray("http://127.0.0.1:4569/other/path"));

		client->followRedirection();

		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedConnections.last()->requestPath(), QByteArray("/other/path"));
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QVERIFY(!client->redirected());
		QCOMPARE(client->redirectionLocation(), QByteArray());

		// Trying to follow redirection when none happened should do nothing.
		QTest::ignoreMessage(QtWarningMsg, "Pillow::HttpClient::followRedirection(): no redirection to follow.");
		client->followRedirection();
	}

	void should_support_gzip_content_encoding()
	{
		QByteArray gzippedData;
		{
			QFile gzipFile(":/test.gz");
			QVERIFY(gzipFile.open(QFile::ReadOnly));
			gzippedData = gzipFile.readAll();
			QCOMPARE(gzippedData.size(), 49);
		}

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(
		    200, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Content-Encoding", "gzip"), gzippedData);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->content(), QByteArray("1234567890123456789012345678901234567890"));

		// GZip content-encoding with empty content.
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(
		    200, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Content-Encoding", "gzip"), "");
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->content(), QByteArray());

		// Another request, without gzip, on the same client
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200, Pillow::HttpHeaderCollection(), "abc");
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->content(), QByteArray("abc"));

		// Gzip content sent in multiple chunks
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeHeaders(200, Pillow::HttpHeaderCollection()
		                                                         << Pillow::HttpHeader("Content-Encoding", "gzip")
		                                                         << Pillow::HttpHeader("Transfer-Encoding", "Chunked"));
		server.receivedConnections.last()->writeContent(gzippedData.left(15));
		QTest::qWait(1);
		server.receivedConnections.last()->writeContent(gzippedData.mid(15, 15));
		QTest::qWait(1);
		server.receivedConnections.last()->writeContent(gzippedData.mid(30));
		server.receivedConnections.last()->endContent();
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->content(), QByteArray("1234567890123456789012345678901234567890"));
	}

	void should_pass_bad_gzipped_content_through()
	{
		QTest::ignoreMessage(QtWarningMsg,
		                     "Pillow::GunzipContentTransformer::transform: error inflating input stream passing original content through.");
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(
		    200, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("Content-Encoding", "gzip"), "Definitely not gzipped data");
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->content(), QByteArray("Definitely not gzipped data"));
	}

	void should_close_connection_after_request_if_asked_by_server()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QPointer<QObject> s = server.receivedSockets.last();
		server.receivedSockets.last()->write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 4\r\n\r\n1234");
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->content(), QByteArray("1234"));
		QVERIFY(waitFor([&] { return s == 0; }));
	}

	void should_handle_empty_response_body()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(204); // No Content
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 204);
		QCOMPARE(client->content(), QByteArray());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
	}

	void should_handle_very_long_headers()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QByteArray longValue(4096, 'x');
		server.receivedConnections.last()->writeResponse(
		    200, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("X-Long-Header", longValue), "content");
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->content(), QByteArray("content"));

		// Verify the long header was received
		bool foundLongHeader = false;
		for (const auto& header : client->headers())
		{
			if (header.first == "X-Long-Header")
			{
				QCOMPARE(header.second, longValue);
				foundLongHeader = true;
				break;
			}
		}
		QVERIFY(foundLongHeader);
	}

	void should_handle_multiple_headers_with_same_name()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		server.receivedSockets.last()->write("HTTP/1.1 200 OK\r\n");
		server.receivedSockets.last()->write("Set-Cookie: cookie1=value1\r\n");
		server.receivedSockets.last()->write("Set-Cookie: cookie2=value2\r\n");
		server.receivedSockets.last()->write("Content-Length: 5\r\n");
		server.receivedSockets.last()->write("\r\n");
		server.receivedSockets.last()->write("hello");
		server.receivedSockets.last()->flush();
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->content(), QByteArray("hello"));

		// Count Set-Cookie headers
		int cookieCount = 0;
		for (const auto& header : client->headers())
		{
			if (header.first == "Set-Cookie")
				cookieCount++;
		}
		QCOMPARE(cookieCount, 2);
	}

	void should_handle_response_with_no_content_length_and_close()
	{
		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		// HTTP/1.0 style response with no content-length, connection closed to indicate end
		server.receivedSockets.last()->write("HTTP/1.0 200 OK\r\n\r\n");
		server.receivedSockets.last()->write("response without content length");
		server.receivedSockets.last()->flush();
		server.receivedSockets.last()->close();
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->content(), QByteArray("response without content length"));
	}

	void should_handle_delete_and_patch_methods()
	{
		// DELETE request
		client->deleteResource(testUrl());
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedConnections.last()->requestMethod(), QByteArray("DELETE"));
		server.receivedConnections.last()->writeResponse(204);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 204);
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);

		// PATCH request
		client->request("PATCH", testUrl(), Pillow::HttpHeaderCollection(), "patch data");
		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedConnections.last()->requestMethod(), QByteArray("PATCH"));
		QCOMPARE(server.receivedConnections.last()->requestContent(), QByteArray("patch data"));
		server.receivedConnections.last()->writeResponse(200, Pillow::HttpHeaderCollection(), "patched");
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->content(), QByteArray("patched"));
	}

	void should_handle_read_buffer_size_zero()
	{
		// A read buffer size of 0 should mean unlimited
		client->setReadBufferSize(0);

		client->get(testUrl());
		QVERIFY(server.waitForRequest());
		QByteArray largeContent(512 * 1024, 'A');
		server.receivedConnections.last()->writeResponse(200, Pillow::HttpHeaderCollection(), largeContent);
		QVERIFY(waitForResponse(2000));
		QCOMPARE(client->statusCode(), 200);
		QCOMPARE(client->content().size(), largeContent.size());
		QCOMPARE(client->error(), Pillow::HttpClient::NoError);
	}

	void should_handle_url_with_userinfo()
	{
		// URLs with username/password should work (userinfo is stripped before sending)
		client->get(QUrl("http://user:pass@127.0.0.1:4569/test"));
		QVERIFY(server.waitForRequest());
		// The request should still reach the server
		QCOMPARE(server.receivedConnections.last()->requestPath(), QByteArray("/test"));
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForResponse());
		QCOMPARE(client->statusCode(), 200);
	}
};

QTEST_MAIN(tst_HttpClient)
#include "tst_httpclient.moc"
#include "moc_Helpers.cpp" // Include moc for TestServer from Helpers.h