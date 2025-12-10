#include <QtTest/QtTest>
#include "httphandlerbase.h"
#include "HttpHandler.h"
#include "HttpHandlerSimpleRouter.h"
using namespace Pillow;

class tst_HttpHandlerSimpleRouter : public HttpHandlerTestBase
{
	Q_OBJECT

protected:
	Q_INVOKABLE void handleRequest1(Pillow::HttpConnection* request)
	{
		request->writeResponse(403, Pillow::HttpHeaderCollection(), "Hello");
	}

protected slots:
	void handleRequest2(Pillow::HttpConnection* request)
	{
		request->writeResponse(200, Pillow::HttpHeaderCollection(), "World");
	}

private slots:
	void testHandlerRoute()
	{
		HttpHandlerSimpleRouter handler;
		handler.addRoute("/some_path", new HttpHandlerFixed(303, "Hello"));
		handler.addRoute("/other/path", new HttpHandlerFixed(404, "World"));
		handler.addRoute("/some_path/even/deeper", new HttpHandlerFixed(200, "!"));

		QVERIFY(!handler.handleRequest(createGetRequest("/should_not_match")));
		QVERIFY(!handler.handleRequest(createGetRequest("/should/not/match/either")));
		QVERIFY(!handler.handleRequest(createGetRequest("/some_path/should_not_match")));

		QVERIFY(handler.handleRequest(createGetRequest("/other/path")));
		QVERIFY(response.startsWith("HTTP/1.0 404"));
		QVERIFY(response.endsWith("World"));
		response.clear();

		QVERIFY(handler.handleRequest(createGetRequest("/some_path/even/deeper?with=query_string")));
		QVERIFY(response.startsWith("HTTP/1.0 200"));
		QVERIFY(response.endsWith("!"));
		response.clear();
	}

	void testQObjectMetaCallRoute()
	{
		HttpHandlerSimpleRouter handler;
		handler.addRoute("/first", this, "handleRequest1");
		handler.addRoute("/first/second", this, "handleRequest2");

		QVERIFY(!handler.handleRequest(createGetRequest("/should_not_match")));
		QVERIFY(!handler.handleRequest(createGetRequest("/should/not/match/either")));
		QVERIFY(!handler.handleRequest(createGetRequest("/first/should_not_match")));
		QVERIFY(!handler.handleRequest(createGetRequest("/first/second/should_not_match")));

		QVERIFY(handler.handleRequest(createGetRequest("/first")));
		QVERIFY(response.startsWith("HTTP/1.0 403"));
		QVERIFY(response.endsWith("Hello"));
		response.clear();

		QVERIFY(handler.handleRequest(createGetRequest("/first/second?with=query_string")));
		QVERIFY(response.startsWith("HTTP/1.0 200"));
		QVERIFY(response.endsWith("World"));
		response.clear();
	}

	void testQObjectSlotCallRoute()
	{
		HttpHandlerSimpleRouter handler;
		handler.addRoute("/route", this, SLOT(handleRequest2(Pillow::HttpConnection*)));

		QVERIFY(handler.handleRequest(createGetRequest("/route")));
		QVERIFY(response.startsWith("HTTP/1.0 200"));
		QVERIFY(response.endsWith("World"));
		response.clear();
	}

	void testStaticRoute()
	{
		HttpHandlerSimpleRouter handler;
		handler.addRoute("/first", 200, Pillow::HttpHeaderCollection(), "First Route");
		handler.addRoute("/first/second", 404, Pillow::HttpHeaderCollection(), "Second Route");
		handler.addRoute("/third", 500, Pillow::HttpHeaderCollection(), "Third Route");

		QVERIFY(!handler.handleRequest(createGetRequest("/should_not_match")));
		QVERIFY(!handler.handleRequest(createGetRequest("/should/not/match/either")));
		QVERIFY(!handler.handleRequest(createGetRequest("/first/should_not_match")));
		QVERIFY(!handler.handleRequest(createGetRequest("/first/second/should_not_match")));

		QVERIFY(handler.handleRequest(createGetRequest("/first")));
		QVERIFY(response.startsWith("HTTP/1.0 200"));
		QVERIFY(response.endsWith("First Route"));
		response.clear();

		QVERIFY(handler.handleRequest(createGetRequest("/third?with=query_string#and_fragment")));
		QVERIFY(response.startsWith("HTTP/1.0 500"));
		QVERIFY(response.endsWith("Third Route"));
		response.clear();
	}

	void testFuncRoute()
	{
#ifdef Q_COMPILER_LAMBDA
		HttpHandlerSimpleRouter handler;
		handler.addRoute("/a_route", [](Pillow::HttpConnection* request) { request->writeResponse(200, Pillow::HttpHeaderCollection(), "Amazing First Route"); });
		handler.addRoute("/a_route/and_another", [](Pillow::HttpConnection* request) { request->writeResponse(400, Pillow::HttpHeaderCollection(), "Delicious Second Route"); });

		QVERIFY(!handler.handleRequest(createGetRequest("/should_not_match")));
		QVERIFY(!handler.handleRequest(createGetRequest("/should/not/match/either")));
		QVERIFY(!handler.handleRequest(createGetRequest("/a_route/should_not_match")));
		QVERIFY(!handler.handleRequest(createGetRequest("/a_route/and_another/should_not_match")));

		QVERIFY(handler.handleRequest(createGetRequest("/a_route")));
		QVERIFY(response.startsWith("HTTP/1.0 200"));
		QVERIFY(response.endsWith("Amazing First Route"));
		response.clear();

		QVERIFY(handler.handleRequest(createGetRequest("/a_route/and_another?with=query_string#and_fragment")));
		QVERIFY(response.startsWith("HTTP/1.0 400"));
		QVERIFY(response.endsWith("Delicious Second Route"));
		response.clear();
#else
		QSKIP("Compiler does not support lambdas or C++0x support is not enabled.", SkipSingle);
#endif
	}

	void testPathParams()
	{
		HttpHandlerSimpleRouter handler;
		handler.addRoute("/first/:with_param", 200, Pillow::HttpHeaderCollection(), "First Route");
		handler.addRoute("/second/:with_param/and/:another", 200, Pillow::HttpHeaderCollection(), "Second Route");
		handler.addRoute("/third/:with/:many/:params", 200, Pillow::HttpHeaderCollection(), "Third Route");

		QVERIFY(handler.handleRequest(createGetRequest("/first/some_param-value")));
		QVERIFY(response.startsWith("HTTP/1.0 200"));
		QVERIFY(response.endsWith("First Route"));
		QCOMPARE(requestParams.size(), 1);
		QCOMPARE(requestParams.at(0).first, QString("with_param"));
		QCOMPARE(requestParams.at(0).second, QString("some_param-value"));
		response.clear();

		QVERIFY(handler.handleRequest(createGetRequest("/second/some_param-value/and/another_value")));
		QVERIFY(response.startsWith("HTTP/1.0 200"));
		QVERIFY(response.endsWith("Second Route"));
		QCOMPARE(requestParams.size(), 2);
		QCOMPARE(requestParams.at(0).first, QString("with_param"));
		QCOMPARE(requestParams.at(0).second, QString("some_param-value"));
		QCOMPARE(requestParams.at(1).first, QString("another"));
		QCOMPARE(requestParams.at(1).second, QString("another_value"));
		response.clear();

		QVERIFY(handler.handleRequest(createGetRequest("/third/some_param-value/another_value/and_a_last_one?with=overriden&extra=bonus_query_param#and_fragment")));
		QVERIFY(response.startsWith("HTTP/1.0 200"));
		QVERIFY(response.endsWith("Third Route"));
		QCOMPARE(requestParams.size(), 4);
		QCOMPARE(requestParams.at(0).first, QString("with"));
		QCOMPARE(requestParams.at(0).second, QString("some_param-value")); // The route param should have overriden the query string param.
		QCOMPARE(requestParams.at(1).first, QString("extra"));
		QCOMPARE(requestParams.at(1).second, QString("bonus_query_param"));
		QCOMPARE(requestParams.at(2).first, QString("many"));
		QCOMPARE(requestParams.at(2).second, QString("another_value"));
		QCOMPARE(requestParams.at(3).first, QString("params"));
		QCOMPARE(requestParams.at(3).second, QString("and_a_last_one"));
		response.clear();

		QVERIFY(!handler.handleRequest(createGetRequest("/first/some_param-value/and_extra_stuff")));
		QVERIFY(!handler.handleRequest(createGetRequest("/second/some_param-value/bad_part/another_value")));
		QVERIFY(!handler.handleRequest(createGetRequest("/third/some_param-value/another_value/and_a_last_one/and_extra_stuff")));
	}

	void testPathSplats()
	{
		HttpHandlerSimpleRouter handler;
		handler.addRoute("/first/*with_splat", 200, Pillow::HttpHeaderCollection(), "First Route");
		handler.addRoute("/second/:with_param/and/*splat", 200, Pillow::HttpHeaderCollection(), "Second Route");
		handler.addRoute("/third/*with/two/*splats", 200, Pillow::HttpHeaderCollection(), "Third Route");

		QVERIFY(handler.handleRequest(createGetRequest("/first/")));
		QVERIFY(response.startsWith("HTTP/1.0 200"));
		QVERIFY(response.endsWith("First Route"));
		QCOMPARE(requestParams.size(), 1);
		QCOMPARE(requestParams.at(0).first, QString("with_splat"));
		QCOMPARE(requestParams.at(0).second, QString(""));
		response.clear();

		QVERIFY(handler.handleRequest(createGetRequest("/first/with/anything-after.that/really_I_tell_you.html")));
		QVERIFY(response.startsWith("HTTP/1.0 200"));
		QVERIFY(response.endsWith("First Route"));
		QCOMPARE(requestParams.size(), 1);
		QCOMPARE(requestParams.at(0).first, QString("with_splat"));
		QCOMPARE(requestParams.at(0).second, QString("with/anything-after.that/really_I_tell_you.html"));
		response.clear();

		QVERIFY(handler.handleRequest(createGetRequest("/second/some-param-value/and/")));
		QVERIFY(response.startsWith("HTTP/1.0 200"));
		QVERIFY(response.endsWith("Second Route"));
		QCOMPARE(requestParams.size(), 2);
		QCOMPARE(requestParams.at(0).first, QString("with_param"));
		QCOMPARE(requestParams.at(0).second, QString("some-param-value"));
		QCOMPARE(requestParams.at(1).first, QString("splat"));
		QCOMPARE(requestParams.at(1).second, QString(""));
		response.clear();

		QVERIFY(handler.handleRequest(createGetRequest("/second/some-param-value/and/extra/stuff/splatted.at/the.end?with=bonus_query_param#and_fragment")));
		QVERIFY(response.startsWith("HTTP/1.0 200"));
		QVERIFY(response.endsWith("Second Route"));
		QCOMPARE(requestParams.size(), 3);
		QCOMPARE(requestParams.at(0).first, QString("with"));
		QCOMPARE(requestParams.at(0).second, QString("bonus_query_param"));
		QCOMPARE(requestParams.at(1).first, QString("with_param"));
		QCOMPARE(requestParams.at(1).second, QString("some-param-value"));
		QCOMPARE(requestParams.at(2).first, QString("splat"));
		QCOMPARE(requestParams.at(2).second, QString("extra/stuff/splatted.at/the.end"));
		response.clear();

		QVERIFY(handler.handleRequest(createGetRequest("/third/some/path/two/and/another/path%20with%20spaces.txt")));
		QVERIFY(response.startsWith("HTTP/1.0 200"));
		QVERIFY(response.endsWith("Third Route"));
		QCOMPARE(requestParams.size(), 2);
		QCOMPARE(requestParams.at(0).first, QString("with"));
		QCOMPARE(requestParams.at(0).second, QString("some/path"));
		QCOMPARE(requestParams.at(1).first, QString("splats"));
		QCOMPARE(requestParams.at(1).second, QString("and/another/path with spaces.txt"));
		response.clear();

		QVERIFY(!handler.handleRequest(createGetRequest("/first")));
		QVERIFY(!handler.handleRequest(createGetRequest("/second/some_param-value/")));
		QVERIFY(!handler.handleRequest(createGetRequest("/second/some_param-value/and")));
		QVERIFY(!handler.handleRequest(createGetRequest("/second/some_param-value/bad_part/splat/splat/splat")));
	}

	void testMatchesMethod()
	{
		HttpHandlerSimpleRouter handler;
		handler.addRoute("GET", "/get", 200, Pillow::HttpHeaderCollection(), "First Route");
		handler.addRoute("POST", "/post", 200, Pillow::HttpHeaderCollection(), "Second Route");
		handler.addRoute("GET", "/both", 200, Pillow::HttpHeaderCollection(), "Third Route (GET)");
		handler.addRoute("POST", "/both", 200, Pillow::HttpHeaderCollection(), "Third Route (POST)");

		QVERIFY(handler.handleRequest(createGetRequest("/get")));
		QVERIFY(!handler.handleRequest(createPostRequest("/get")));
		QVERIFY(!handler.handleRequest(createGetRequest("/post")));
		QVERIFY(handler.handleRequest(createPostRequest("/post")));

		QVERIFY(handler.handleRequest(createGetRequest("/both")));
		QVERIFY(response.startsWith("HTTP/1.0 200"));
		QVERIFY(response.endsWith("Third Route (GET)"));
		response.clear();

		QVERIFY(handler.handleRequest(createPostRequest("/both")));
		QVERIFY(response.startsWith("HTTP/1.0 200"));
		QVERIFY(response.endsWith("Third Route (POST)"));
		response.clear();
	}

	void testUnmatchedRequestAction()
	{
		HttpHandlerSimpleRouter handler;
		QVERIFY(handler.unmatchedRequestAction() == HttpHandlerSimpleRouter::Passthrough);
		handler.setUnmatchedRequestAction(HttpHandlerSimpleRouter::Return4xxResponse);
		handler.addRoute("GET", "/a", 200, Pillow::HttpHeaderCollection(), "First Route (GET)");
		handler.addRoute("DELETE", "/a", 200, Pillow::HttpHeaderCollection(), "First Route (DELETE)");

		QVERIFY(handler.handleRequest(createGetRequest("/unmatched/route")));
		QVERIFY(response.startsWith("HTTP/1.0 404"));
		response.clear();
	}

	void testMethodMismatchAction()
	{
		HttpHandlerSimpleRouter handler;
		QVERIFY(handler.methodMismatchAction() == HttpHandlerSimpleRouter::Passthrough);
		handler.setMethodMismatchAction(HttpHandlerSimpleRouter::Return4xxResponse);
		handler.addRoute("GET", "/a", 200, Pillow::HttpHeaderCollection(), "First Route (GET)");
		handler.addRoute("DELETE", "/a", 200, Pillow::HttpHeaderCollection(), "First Route (DELETE)");

		QVERIFY(handler.handleRequest(createPostRequest("/a")));
		QVERIFY(response.startsWith("HTTP/1.0 405"));
		QVERIFY(response.contains("Allow: GET, DELETE"));
		response.clear();
	}

	void testSupportsMethodParam()
	{
		HttpHandlerSimpleRouter handler;
		handler.addRoute("POST", "/a", 200, Pillow::HttpHeaderCollection(), "Route");
		handler.addRoute("DELETE", "/b", 200, Pillow::HttpHeaderCollection(), "Route");

		QVERIFY(handler.acceptsMethodParam() == false);
		QVERIFY(!handler.handleRequest(createGetRequest("/a")));
		QVERIFY(handler.handleRequest(createPostRequest("/a")));
		QVERIFY(!handler.handleRequest(createGetRequest("/a?_method=post")));
		QVERIFY(!handler.handleRequest(createGetRequest("/b?_method=delete")));
		QVERIFY(!handler.handleRequest(createPostRequest("/b?_method=delete")));

		handler.setAcceptsMethodParam(true);
		QVERIFY(!handler.handleRequest(createGetRequest("/a")));
		QVERIFY(handler.handleRequest(createPostRequest("/a")));
		QVERIFY(handler.handleRequest(createGetRequest("/a?_method=POST")));
		QVERIFY(handler.handleRequest(createGetRequest("/b?_method=DELETE")));
		QVERIFY(handler.handleRequest(createPostRequest("/b?_method=DELETE")));
		QVERIFY(handler.handleRequest(createGetRequest("/b?_method=delete")));
		QVERIFY(handler.handleRequest(createPostRequest("/b?_method=delete")));
	}
};

QTEST_MAIN(tst_HttpHandlerSimpleRouter)
#include "tst_httphandlersimplerouter.moc"