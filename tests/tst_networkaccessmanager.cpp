#include <QtCore/QObject>
#include <QtCore/QTimeZone>
#include <QtTest/QtTest>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkCookie>
#include <QtNetwork/QNetworkCookieJar>
#include <HttpClient.h>
#include <HttpConnection.h>
#include <HttpServer.h>
#include <HttpHelpers.h>
#include "Helpers.h"

//
// NetworkAccessManager test class
//

class tst_NetworkAccessManager : public QObject
{
	Q_OBJECT
	Pillow::NetworkAccessManager *nam;
	TestServer server;

private slots:
	void initTestCase()
	{
		QVERIFY(server.listen(QHostAddress::LocalHost, 4571));
	}

	void init()
	{
		nam = new Pillow::NetworkAccessManager();
		QVERIFY(server.receivedRequests.isEmpty());
		QVERIFY(server.receivedConnections.isEmpty());
		QVERIFY(server.receivedSockets.isEmpty());
	}

	void cleanup()
	{
		delete nam; nam = 0;
		server.receivedRequests.clear();
		server.receivedConnections.clear();
		server.receivedSockets.clear();
	}

private:
	QUrl testUrl() const { return QUrl("http://127.0.0.1:4571/test/path"); }

private slots:
	void should_send_valid_requests_data()
	{
		QTest::addColumn<QByteArray>("method");
		QTest::addColumn<QUrl>("url");
		QTest::addColumn<Pillow::HttpHeaderCollection>("headers");
		QTest::addColumn<QByteArray>("content");
		QTest::addColumn<HttpRequestData>("expectedRequestData");

		Pillow::HttpHeaderCollection baseExpectedHeaders;
		baseExpectedHeaders << Pillow::HttpHeader("Host", "127.0.0.1:4571");

		QTest::newRow("Simple GET") << QByteArray("GET")
									<< QUrl("http://127.0.0.1:4571/")
									<< Pillow::HttpHeaderCollection()
									<< QByteArray()
									<< HttpRequestData()
									   .withMethod("GET")
									   .withUri("/").withPath("/").withQueryString("").withFragment("")
									   .withHttpVersion("HTTP/1.1")
									   .withContent("")
									   .withHeaders(baseExpectedHeaders);

		QTest::newRow("GET with headers") << QByteArray("GET")
									<< QUrl("http://127.0.0.1:4571/some/path?and=query")
									<< (Pillow::HttpHeaderCollection()
										<< Pillow::HttpHeader("X-Some", "Header")
										<< Pillow::HttpHeader("X-And-Another", "even-better; header"))
									<< QByteArray()
									<< HttpRequestData()
									   .withMethod("GET")
									   .withUri("/some/path?and=query").withPath("/some/path").withQueryString("and=query").withFragment("")
									   .withHttpVersion("HTTP/1.1")
									   .withContent("")
									   .withHeaders(Pillow::HttpHeaderCollection(baseExpectedHeaders)
													<< Pillow::HttpHeader("x-some", "Header")
													<< Pillow::HttpHeader("x-and-another", "even-better; header"));
		QTest::newRow("GET with connection: close") << QByteArray("GET")
									<< QUrl("http://127.0.0.1:4571/")
									<< (Pillow::HttpHeaderCollection()
										<< Pillow::HttpHeader("Connection", "close"))
									<< QByteArray()
									<< HttpRequestData()
									   .withMethod("GET")
									   .withUri("/").withPath("/").withQueryString("").withFragment("")
									   .withHttpVersion("HTTP/1.1")
									   .withContent("")
									   .withHeaders(Pillow::HttpHeaderCollection(baseExpectedHeaders)
													<< Pillow::HttpHeader("connection", "close"));
		QTest::newRow("Simple PUT") << QByteArray("PUT")
									<< QUrl("http://127.0.0.1:4571/some/path")
									<< (Pillow::HttpHeaderCollection())
									<< QByteArray("Some sent data")
									<< HttpRequestData()
									   .withMethod("PUT")
									   .withUri("/some/path").withPath("/some/path").withQueryString("").withFragment("")
									   .withHttpVersion("HTTP/1.1")
									   .withContent("Some sent data")
									   .withHeaders(Pillow::HttpHeaderCollection(baseExpectedHeaders)
													<< Pillow::HttpHeader("Content-Length", "14"));
		QTest::newRow("Large POST") << QByteArray("POST")
									<< QUrl("http://127.0.0.1:4571/some/large/path")
									<< (Pillow::HttpHeaderCollection())
									<< QByteArray(128 * 1024, '*')
									<< HttpRequestData()
									   .withMethod("POST")
									   .withUri("/some/large/path").withPath("/some/large/path").withQueryString("").withFragment("")
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

		QNetworkRequest request;
		request.setUrl(url);
		foreach (const Pillow::HttpHeader &header, headers) request.setRawHeader(header.first, header.second);
		QBuffer requestContent(&content); requestContent.open(QIODevice::ReadOnly);

		nam->sendCustomRequest(request, method, &requestContent);

		QVERIFY(server.waitForRequest());
		QCOMPARE(server.receivedRequests.size(), 1);
		QCOMPARE(server.receivedRequests.first(), expectedRequestData);
	}

	void should_receive_response()
	{
		QNetworkReply *r = nam->get(QNetworkRequest(testUrl()));
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(201, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("X-Some", "Header"), "Hello World!");

		QVERIFY(waitForSignal(r, SIGNAL(finished())));

		QCOMPARE(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 201);
		QCOMPARE(r->readAll(), QByteArray("Hello World!"));
		QCOMPARE(r->rawHeaderPairs(), Pillow::HttpHeaderCollection()
				 << Pillow::HttpHeader("x-some", "Header")
				 << Pillow::HttpHeader("content-length", "12")
				 << Pillow::HttpHeader("content-type", "text/plain"));
	}

	void should_set_cooked_headers_from_received_headers()
	{
		QNetworkReply *r = nam->get(QNetworkRequest(testUrl()));
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(
					302,
					Pillow::HttpHeaderCollection()
					<< Pillow::HttpHeader("Location", "http://example.org")
					<< Pillow::HttpHeader("Content-Type", "some/type")
					<< Pillow::HttpHeader("Last-Modified", Pillow::HttpProtocol::Dates::getHttpDate(QDateTime(QDate(2012, 4, 30), QTime(8, 9, 0), QTimeZone::UTC)))
					<< Pillow::HttpHeader("Set-Cookie", "ChocolateCookie=Very Delicious")
					, "Hello!");

		QVERIFY(waitForSignal(r, SIGNAL(finished())));
		QCOMPARE(r->readAll(), QByteArray("Hello!"));
		QCOMPARE(r->rawHeaderPairs(), Pillow::HttpHeaderCollection()
				 << Pillow::HttpHeader("location", "http://example.org")
				 << Pillow::HttpHeader("last-modified", "Mon, 30 Apr 2012 08:09:00 GMT")
				 << Pillow::HttpHeader("content-length", "6")
				 << Pillow::HttpHeader("content-type", "some/type")
				 << Pillow::HttpHeader("set-cookie", "ChocolateCookie=Very Delicious")
				 );

		QCOMPARE(r->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl(), QUrl("http://example.org"));
		QCOMPARE(r->header(QNetworkRequest::ContentTypeHeader).toByteArray(), QByteArray("some/type"));
		QCOMPARE(r->header(QNetworkRequest::ContentLengthHeader).toInt(), 6);
		QCOMPARE(r->header(QNetworkRequest::LastModifiedHeader).toDateTime(), QDateTime(QDate(2012, 4, 30), QTime(8, 9, 0), QTimeZone::UTC));

		QList<QNetworkCookie> cookies;
		cookies << QNetworkCookie("ChocolateCookie", "Very Delicious");
		QCOMPARE(r->header(QNetworkRequest::SetCookieHeader).value<QList<QNetworkCookie> >(), cookies);
	}

	void should_store_received_cookies_in_the_jar()
	{
		class JarOpener : public QNetworkCookieJar
		{
		public:
			QList<QNetworkCookie> getAllCookies() const { return QNetworkCookieJar::allCookies(); }
		};

		QCOMPARE(static_cast<JarOpener*>(nam->cookieJar())->getAllCookies().size(), 0);

		QNetworkReply *r = nam->get(QNetworkRequest(testUrl()));
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200,
					Pillow::HttpHeaderCollection()
					<< Pillow::HttpHeader("Set-Cookie", "SomeCookie=Super Chocolate")
					<< Pillow::HttpHeader("seT-CoOKie", "AnotherCookie=Mega Caramel")
					, "Hello!");

		QVERIFY(waitForSignal(r, SIGNAL(finished())));
		QCOMPARE(static_cast<JarOpener*>(nam->cookieJar())->getAllCookies().size(), 2);

		QList<QNetworkCookie> cookies;
		cookies << QNetworkCookie("SomeCookie", "Super Chocolate");
		cookies << QNetworkCookie("AnotherCookie", "Mega Caramel");
		QCOMPARE(r->header(QNetworkRequest::SetCookieHeader).value<QList<QNetworkCookie> >(), cookies);
	}

	void should_send_cookies_from_the_jar()
	{
		QNetworkCookieJar jar;
		QNetworkCookie c1("First", "FirstValue");
		QNetworkCookie c2("Second", "SecondValue");
		QNetworkCookie c3("Third", "ThirdValue");
		jar.setCookiesFromUrl(QList<QNetworkCookie>() << c1 << c3, QUrl("http://127.0.0.1:4571/"));
		jar.setCookiesFromUrl(QList<QNetworkCookie>() << c2, QUrl("http://127.0.1.1:7777/"));

		nam->setCookieJar(&jar); jar.setParent(0);
		nam->get(QNetworkRequest(testUrl()));
		QVERIFY(server.waitForRequest());

		int cookieHeaderCount = 0;
		Pillow::HttpHeaderCollection headers = server.receivedConnections.last()->requestHeaders();
		foreach (const Pillow::HttpHeader &h, headers)
		{
			if (h.first.toLower() == "cookie")
				cookieHeaderCount++;
		}

		QCOMPARE(cookieHeaderCount, 1);

		QByteArray cookieValue = server.receivedConnections.last()->requestHeaderValue("Cookie");
		QCOMPARE(cookieValue, QByteArray("First=FirstValue; Third=ThirdValue"));
	}

	void should_set_headers_as_soon_as_they_are_received()
	{
		QNetworkReply *r = nam->get(QNetworkRequest(testUrl()));
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeHeaders(200, Pillow::HttpHeaderCollection() << Pillow::HttpHeader("X-Some", "Header") << Pillow::HttpHeader("Content-Length", "12") << Pillow::HttpHeader("Content-Type", "text/plain"));

		QSignalSpy finishedSpy(r, SIGNAL(finished()));
		QVERIFY(waitForSignal(r, SIGNAL(metaDataChanged())));
		QVERIFY(finishedSpy.isEmpty()); // The request should not be finished yet even though headers have been received

		QCOMPARE(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
		QCOMPARE(r->rawHeaderPairs(), Pillow::HttpHeaderCollection()
				 << Pillow::HttpHeader("x-some", "Header")
				 << Pillow::HttpHeader("content-length", "12")
				 << Pillow::HttpHeader("content-type", "text/plain"));
		QCOMPARE(r->header(QNetworkRequest::ContentTypeHeader).toByteArray(), QByteArray("text/plain"));
		QCOMPARE(r->header(QNetworkRequest::ContentLengthHeader).toInt(), 12);
		QCOMPARE(r->readAll(), QByteArray());

		server.receivedConnections.last()->writeContent("Hello World!");

		QVERIFY(waitForSignal(r, SIGNAL(finished())));

		QCOMPARE(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
		QCOMPARE(r->rawHeaderPairs(), Pillow::HttpHeaderCollection()
				 << Pillow::HttpHeader("x-some", "Header")
				 << Pillow::HttpHeader("content-length", "12")
				 << Pillow::HttpHeader("content-type", "text/plain"));
		QCOMPARE(r->header(QNetworkRequest::ContentTypeHeader).toByteArray(), QByteArray("text/plain"));
		QCOMPARE(r->header(QNetworkRequest::ContentLengthHeader).toInt(), 12);
		QCOMPARE(r->readAll(), QByteArray("Hello World!"));
	}

	void should_make_request_accessible()
	{
		QNetworkRequest originalRequest;
		originalRequest.setUrl(testUrl());
		originalRequest.setRawHeader("One-Header", "OneValue");
		originalRequest.setRawHeader("Another-Header", "AnotherValue");
		originalRequest.setPriority(QNetworkRequest::HighPriority);

		QNetworkReply *r = nam->post(originalRequest, QByteArray("hello"));
		QCOMPARE(r->request(), originalRequest);
		QCOMPARE(r->url(), testUrl());
		QCOMPARE(r->operation(), QNetworkAccessManager::PostOperation);
	}

	void should_allow_aborting_reply()
	{
		QNetworkReply *r = nam->get(QNetworkRequest(testUrl()));
		QVERIFY(server.waitForRequest());
		QVERIFY(server.receivedSockets.last()->state() == QAbstractSocket::ConnectedState);
		QPointer<QTcpSocket> socket = server.receivedSockets.last();
		QVERIFY(socket != 0);
		QSignalSpy errorSpy(r, SIGNAL(errorOccurred(QNetworkReply::NetworkError)));
		QSignalSpy finishedSpy(r, SIGNAL(finished()));
		r->abort();
		QCOMPARE(finishedSpy.size(), 1);
		QCOMPARE(errorSpy.size(), 1);
		QCOMPARE(errorSpy.last().first().value<QNetworkReply::NetworkError>(), QNetworkReply::OperationCanceledError);
		QVERIFY(waitFor([=]{ return socket == 0; })); // The socket should get closed and deleted.
	}

	void should_emit_readyRead_when_new_content_is_available()
	{
		QNetworkReply *r = nam->get(QNetworkRequest(testUrl()));
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeHeaders(200);

		QCOMPARE(r->bytesAvailable(), qint64(0));

		server.receivedConnections.last()->writeContent("Hello");
		QVERIFY(waitForSignal(r, SIGNAL(readyRead())));
		QCOMPARE(r->bytesAvailable(), qint64(5));
		QCOMPARE(r->pos(), qint64(0));
		QCOMPARE(r->read(3), QByteArray("Hel"));
		QCOMPARE(r->bytesAvailable(), qint64(2));
		QCOMPARE(r->pos(), qint64(0));

		server.receivedConnections.last()->writeContent("the");
		QVERIFY(waitForSignal(r, SIGNAL(readyRead())));
		QCOMPARE(r->bytesAvailable(), qint64(5));
		QCOMPARE(r->read(4), QByteArray("loth"));
		QCOMPARE(r->bytesAvailable(), qint64(1));
		QCOMPARE(r->pos(), qint64(0));

		server.receivedConnections.last()->writeContent("world!");
		QVERIFY(waitForSignal(r, SIGNAL(readyRead())));
		QCOMPARE(r->bytesAvailable(), qint64(7));
		QCOMPARE(r->read(5), QByteArray("eworl"));
		QCOMPARE(r->bytesAvailable(), qint64(2));
		QCOMPARE(r->read(2), QByteArray("d!"));
		QCOMPARE(r->bytesAvailable(), qint64(0));
	}

	void should_report_errors()
	{
		{
			QNetworkReply *r = nam->get(QNetworkRequest(QUrl("http://4230948234.423.423.423.423.32.423.4/bad")));
			QSignalSpy errorSpy(r, SIGNAL(errorOccurred(QNetworkReply::NetworkError)));
			QVERIFY(waitFor([&]{ return errorSpy.size() > 0; }));
			QCOMPARE(errorSpy.last().first().value<QNetworkReply::NetworkError>(), QNetworkReply::UnknownNetworkError);
		}

		{
			QNetworkReply *r = nam->get(QNetworkRequest(testUrl()));
			QSignalSpy errorSpy(r, SIGNAL(errorOccurred(QNetworkReply::NetworkError)));
			QVERIFY(server.waitForRequest());
			server.receivedConnections.last()->outputDevice()->write("=-=-=-=-=-FFFFFFFUUUUUUUUUUU=-=-=-=-=-=!\r\n");
			QVERIFY(waitFor([&]{ return errorSpy.size() > 0; }));
			QCOMPARE(errorSpy.last().first().value<QNetworkReply::NetworkError>(), QNetworkReply::ProtocolUnknownError);
		}
	}

	void should_use_default_QNetworkAccessManager_implementation_for_non_http_schemes()
	{
		QByteArray url = "data:text/plain;base64," + QByteArray("Hello World!").toBase64();

		QNetworkReply *r = nam->get(QNetworkRequest(QUrl(url)));
		QVERIFY(waitForSignal(r, SIGNAL(finished())));

		QCOMPARE(r->readAll(), QByteArray("Hello World!"));
	}

	void should_reuse_existing_connection_to_same_server_when_available()
	{
		QNetworkReply *r = nam->get(QNetworkRequest(testUrl()));
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(200);
		QVERIFY(waitForSignal(r, SIGNAL(finished())));

		r = nam->get(QNetworkRequest(testUrl()));
		QVERIFY(server.waitForRequest());
		server.receivedConnections.last()->writeResponse(201);
		QVERIFY(waitForSignal(r, SIGNAL(finished())));

		QCOMPARE(server.receivedSockets.size(), 2);
		QVERIFY(server.receivedSockets.at(0) == server.receivedSockets.at(1));
		QVERIFY(server.receivedSockets.at(0) != 0);

		r = nam->get(QNetworkRequest(testUrl()));
		QNetworkReply *r4 = nam->get(QNetworkRequest(testUrl()));
		QNetworkReply *r5 = nam->get(QNetworkRequest(testUrl()));

		QVERIFY(server.waitForRequest());
		QVERIFY(waitFor([&]{ return server.receivedConnections.size() == 5; }));
		QVERIFY(server.receivedSockets.at(2) == server.receivedSockets.at(1));
		QVERIFY(server.receivedSockets.at(3) != server.receivedSockets.at(2));
		QVERIFY(server.receivedSockets.at(4) != server.receivedSockets.at(2));
		QVERIFY(server.receivedSockets.at(4) != server.receivedSockets.at(3));

		server.receivedConnections.at(2)->writeResponse(400);
		server.receivedConnections.at(3)->writeResponse(404);
		server.receivedConnections.at(4)->writeResponse(500);

		QVERIFY(waitFor([&]{ return !r->isRunning() && !r4->isRunning() && !r5->isRunning(); }));
	}
};

QTEST_MAIN(tst_NetworkAccessManager)
#include "tst_networkaccessmanager.moc"
#include "moc_Helpers.cpp"