#include <QtCore/QObject>
#include <QtCore/QRandomGenerator>
#include <QtTest/QtTest>
#include <HttpClient.h>
#include "Helpers.h"
#include "../pillowcore/parser/http_parser.h"

// Test data structures from http-parser tests
#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0

#define MAX_HEADERS 13
#define MAX_ELEMENT_SIZE 500

struct message
{
	const char* name; // for debugging purposes
	enum http_parser_type type;
	const char* raw;
	int should_keep_alive;
	int message_complete_on_eof;
	unsigned short http_major;
	unsigned short http_minor;
	unsigned short status_code;
	int num_headers;
	char headers[MAX_HEADERS][2][MAX_ELEMENT_SIZE];
	size_t body_size;
	char body[MAX_ELEMENT_SIZE];
};
Q_DECLARE_METATYPE(message)

const struct message responses[] =
#define GOOGLE_301 0
    {
        {"google 301",
         HTTP_RESPONSE,
         "HTTP/1.1 301 Moved Permanently\r\n"
         "Location: http://www.google.com/\r\n"
         "Content-Type: text/html; charset=UTF-8\r\n"
         "Date: Sun, 26 Apr 2009 11:11:49 GMT\r\n"
         "Expires: Tue, 26 May 2009 11:11:49 GMT\r\n"
         "X-$PrototypeBI-Version: 1.6.0.3\r\n" /* $ char in header field */
         "Cache-Control: public, max-age=2592000\r\n"
         "Server: gws\r\n"
         "Content-Length:  219  \r\n"
         "\r\n"
         "<HTML><HEAD><meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\">\n"
         "<TITLE>301 Moved</TITLE></HEAD><BODY>\n"
         "<H1>301 Moved</H1>\n"
         "The document has moved\n"
         "<A HREF=\"http://www.google.com/\">here</A>.\r\n"
         "</BODY></HTML>\r\n",
         TRUE,
         FALSE,
         1,
         1,
         301,
         8,
         {{"Location", "http://www.google.com/"},
          {"Content-Type", "text/html; charset=UTF-8"},
          {"Date", "Sun, 26 Apr 2009 11:11:49 GMT"},
          {"Expires", "Tue, 26 May 2009 11:11:49 GMT"},
          {"X-$PrototypeBI-Version", "1.6.0.3"},
          {"Cache-Control", "public, max-age=2592000"},
          {"Server", "gws"},
          {"Content-Length", "219  "}},
         0,
         "<HTML><HEAD><meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\">\n"
         "<TITLE>301 Moved</TITLE></HEAD><BODY>\n"
         "<H1>301 Moved</H1>\n"
         "The document has moved\n"
         "<A HREF=\"http://www.google.com/\">here</A>.\r\n"
         "</BODY></HTML>\r\n"}

#define NO_CONTENT_LENGTH_RESPONSE 1
        /* The client should wait for the server's EOF. That is, when content-length
         * is not specified, and "Connection: close", the end of body is specified
         * by the EOF.
         * Compare with APACHEBENCH_GET
         */
        ,
        {"no content-length response",
         HTTP_RESPONSE,
         "HTTP/1.1 200 OK\r\n"
         "Date: Tue, 04 Aug 2009 07:59:32 GMT\r\n"
         "Server: Apache\r\n"
         "X-Powered-By: Servlet/2.5 JSP/2.1\r\n"
         "Content-Type: text/xml; charset=utf-8\r\n"
         "Connection: close\r\n"
         "\r\n"
         "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\">\n"
         "  <SOAP-ENV:Body>\n"
         "    <SOAP-ENV:Fault>\n"
         "       <faultcode>SOAP-ENV:Client</faultcode>\n"
         "       <faultstring>Client Error</faultstring>\n"
         "    </SOAP-ENV:Fault>\n"
         "  </SOAP-ENV:Body>\n"
         "</SOAP-ENV:Envelope>",
         FALSE,
         TRUE,
         1,
         1,
         200,
         5,
         {{"Date", "Tue, 04 Aug 2009 07:59:32 GMT"},
          {"Server", "Apache"},
          {"X-Powered-By", "Servlet/2.5 JSP/2.1"},
          {"Content-Type", "text/xml; charset=utf-8"},
          {"Connection", "close"}},
         0,
         "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\">\n"
         "  <SOAP-ENV:Body>\n"
         "    <SOAP-ENV:Fault>\n"
         "       <faultcode>SOAP-ENV:Client</faultcode>\n"
         "       <faultstring>Client Error</faultstring>\n"
         "    </SOAP-ENV:Fault>\n"
         "  </SOAP-ENV:Body>\n"
         "</SOAP-ENV:Envelope>"}

#define NO_HEADERS_NO_BODY_404 2
        ,
        {"404 no headers no body", HTTP_RESPONSE, "HTTP/1.1 404 Not Found\r\n\r\n", FALSE, TRUE, 1, 1, 404, 0, {}, 0, ""}

#define NO_REASON_PHRASE 3
        ,
        {"301 no response phrase", HTTP_RESPONSE, "HTTP/1.1 301\r\n\r\n", FALSE, TRUE, 1, 1, 301, 0, {}, 0, ""}

#define TRAILING_SPACE_ON_CHUNKED_BODY 4
        ,
        {"200 trailing space on chunked body",
         HTTP_RESPONSE,
         "HTTP/1.1 200 OK\r\n"
         "Content-Type: text/plain\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "25  \r\n"
         "This is the data in the first chunk\r\n"
         "\r\n"
         "1C\r\n"
         "and this is the second one\r\n"
         "\r\n"
         "0  \r\n"
         "\r\n",
         TRUE,
         FALSE,
         1,
         1,
         200,
         2,
         {{"Content-Type", "text/plain"}, {"Transfer-Encoding", "chunked"}},
         37 + 28,
         "This is the data in the first chunk\r\n"
         "and this is the second one\r\n"

        }

#define NO_CARRIAGE_RET 5
        ,
        {"no carriage ret",
         HTTP_RESPONSE,
         "HTTP/1.1 200 OK\n"
         "Content-Type: text/html; charset=utf-8\n"
         "Connection: close\n"
         "\n"
         "these headers are from http://news.ycombinator.com/",
         FALSE,
         TRUE,
         1,
         1,
         200,
         2,
         {{"Content-Type", "text/html; charset=utf-8"}, {"Connection", "close"}},
         0,
         "these headers are from http://news.ycombinator.com/"}

#define PROXY_CONNECTION 6
        ,
        {"proxy connection",
         HTTP_RESPONSE,
         "HTTP/1.1 200 OK\r\n"
         "Content-Type: text/html; charset=UTF-8\r\n"
         "Content-Length: 11\r\n"
         "Proxy-Connection: close\r\n"
         "Date: Thu, 31 Dec 2009 20:55:48 +0000\r\n"
         "\r\n"
         "hello world",
         FALSE,
         FALSE,
         1,
         1,
         200,
         4,
         {{"Content-Type", "text/html; charset=UTF-8"},
          {"Content-Length", "11"},
          {"Proxy-Connection", "close"},
          {"Date", "Thu, 31 Dec 2009 20:55:48 +0000"}},
         11,
         "hello world"}

#define UNDERSTORE_HEADER_KEY 7
        // shown by
        // curl -o /dev/null -v "http://ad.doubleclick.net/pfadx/DARTSHELLCONFIGXML;dcmt=text/xml;"
        ,
        {"underscore header key",
         HTTP_RESPONSE,
         "HTTP/1.1 200 OK\r\n"
         "Server: DCLK-AdSvr\r\n"
         "Content-Type: text/xml\r\n"
         "Content-Length: 0\r\n"
         "DCLK_imp: v7;x;114750856;0-0;0;17820020;0/0;21603567/21621457/1;;~okv=;dcmt=text/xml;;~cs=o\r\n\r\n",
         TRUE,
         FALSE,
         1,
         1,
         200,
         4,
         {{"Server", "DCLK-AdSvr"},
          {"Content-Type", "text/xml"},
          {"Content-Length", "0"},
          {"DCLK_imp", "v7;x;114750856;0-0;0;17820020;0/0;21603567/21621457/1;;~okv=;dcmt=text/xml;;~cs=o"}},
         0,
         ""}

#define BONJOUR_MADAME_FR 8
        /* The client should not merge two headers fields when the first one doesn't
         * have a value.
         */
        ,
        {"bonjourmadame.fr",
         HTTP_RESPONSE,
         "HTTP/1.0 301 Moved Permanently\r\n"
         "Date: Thu, 03 Jun 2010 09:56:32 GMT\r\n"
         "Server: Apache/2.2.3 (Red Hat)\r\n"
         "Cache-Control: public\r\n"
         "Pragma: \r\n"
         "Location: http://www.bonjourmadame.fr/\r\n"
         "Vary: Accept-Encoding\r\n"
         "Content-Length: 0\r\n"
         "Content-Type: text/html; charset=UTF-8\r\n"
         "Connection: keep-alive\r\n"
         "\r\n",
         TRUE,
         FALSE,
         1,
         0,
         301,
         9,
         {{"Date", "Thu, 03 Jun 2010 09:56:32 GMT"},
          {"Server", "Apache/2.2.3 (Red Hat)"},
          {"Cache-Control", "public"},
          {"Pragma", ""},
          {"Location", "http://www.bonjourmadame.fr/"},
          {"Vary", "Accept-Encoding"},
          {"Content-Length", "0"},
          {"Content-Type", "text/html; charset=UTF-8"},
          {"Connection", "keep-alive"}},
         0,
         ""}

#define RES_FIELD_UNDERSCORE 9
        /* Should handle spaces in header fields */
        ,
        {"field underscore",
         HTTP_RESPONSE,
         "HTTP/1.1 200 OK\r\n"
         "Date: Tue, 28 Sep 2010 01:14:13 GMT\r\n"
         "Server: Apache\r\n"
         "Cache-Control: no-cache, must-revalidate\r\n"
         "Expires: Mon, 26 Jul 1997 05:00:00 GMT\r\n"
         ".et-Cookie: PlaxoCS=1274804622353690521; path=/; domain=.plaxo.com\r\n"
         "Vary: Accept-Encoding\r\n"
         "_eep-Alive: timeout=45\r\n" /* semantic value ignored */
         "_onnection: Keep-Alive\r\n" /* semantic value ignored */
         "Transfer-Encoding: chunked\r\n"
         "Content-Type: text/html\r\n"
         "Connection: close\r\n"
         "\r\n"
         "0\r\n\r\n",
         FALSE,
         FALSE,
         1,
         1,
         200,
         11,
         {{"Date", "Tue, 28 Sep 2010 01:14:13 GMT"},
          {"Server", "Apache"},
          {"Cache-Control", "no-cache, must-revalidate"},
          {"Expires", "Mon, 26 Jul 1997 05:00:00 GMT"},
          {".et-Cookie", "PlaxoCS=1274804622353690521; path=/; domain=.plaxo.com"},
          {"Vary", "Accept-Encoding"},
          {"_eep-Alive", "timeout=45"},
          {"_onnection", "Keep-Alive"},
          {"Transfer-Encoding", "chunked"},
          {"Content-Type", "text/html"},
          {"Connection", "close"}},
         0,
         ""}

#define NON_ASCII_IN_STATUS_LINE 10
        /* Should handle non-ASCII in status line */
        ,
        {"non-ASCII in status line",
         HTTP_RESPONSE,
         "HTTP/1.1 500 Oriëntatieprobleem\r\n"
         "Date: Fri, 5 Nov 2010 23:07:12 GMT+2\r\n"
         "Content-Length: 0\r\n"
         "Connection: close\r\n"
         "\r\n",
         FALSE,
         FALSE,
         1,
         1,
         500,
         3,
         {{"Date", "Fri, 5 Nov 2010 23:07:12 GMT+2"}, {"Content-Length", "0"}, {"Connection", "close"}},
         0,
         ""}

#define HTTP_VERSION_0_9 11
        /* Should handle HTTP/0.9 */
        ,
        {"http version 0.9",
         HTTP_RESPONSE,
         "HTTP/0.9 200 OK\r\n"
         "\r\n",
         FALSE,
         TRUE,
         0,
         9,
         200,
         0,
         {},
         0,
         ""}

#define NO_CONTENT_LENGTH_NO_TRANSFER_ENCODING_RESPONSE 12
        /* The client should wait for the server's EOF. That is, when neither
         * content-length nor transfer-encoding is specified, the end of body
         * is specified by the EOF.
         */
        ,
        {"neither content-length nor transfer-encoding response",
         HTTP_RESPONSE,
         "HTTP/1.1 200 OK\r\n"
         "Content-Type: text/plain\r\n"
         "\r\n"
         "hello world",
         FALSE,
         TRUE,
         1,
         1,
         200,
         1,
         {{"Content-Type", "text/plain"}},
         11,
         "hello world"}

#define NO_BODY_HTTP10_KA_200 13
        ,
        {"HTTP/1.0 with keep-alive and EOF-terminated 200 status",
         HTTP_RESPONSE,
         "HTTP/1.0 200 OK\r\n"
         "Connection: keep-alive\r\n"
         "\r\n",
         FALSE,
         TRUE,
         1,
         0,
         200,
         1,
         {{"Connection", "keep-alive"}},
         0,
         ""}

#define NO_BODY_HTTP10_KA_204 14
        ,
        {"HTTP/1.0 with keep-alive and a 204 status",
         HTTP_RESPONSE,
         "HTTP/1.0 204 No content\r\n"
         "Connection: keep-alive\r\n"
         "\r\n",
         TRUE,
         FALSE,
         1,
         0,
         204,
         1,
         {{"Connection", "keep-alive"}},
         0,
         ""}

#define NO_BODY_HTTP11_KA_200 15
        ,
        {"HTTP/1.1 with an EOF-terminated 200 status",
         HTTP_RESPONSE,
         "HTTP/1.1 200 OK\r\n"
         "\r\n",
         FALSE,
         TRUE,
         1,
         1,
         200,
         0,
         {},
         0,
         ""}

#define NO_BODY_HTTP11_KA_204 16
        ,
        {"HTTP/1.1 with a 204 status",
         HTTP_RESPONSE,
         "HTTP/1.1 204 No content\r\n"
         "\r\n",
         TRUE,
         FALSE,
         1,
         1,
         204,
         0,
         {},
         0,
         ""}

#define NO_BODY_HTTP11_NOKA_204 17
        ,
        {"HTTP/1.1 with a 204 status and keep-alive disabled",
         HTTP_RESPONSE,
         "HTTP/1.1 204 No content\r\n"
         "Connection: close\r\n"
         "\r\n",
         FALSE,
         FALSE,
         1,
         1,
         204,
         1,
         {{"Connection", "close"}},
         0,
         ""}

#define NO_BODY_HTTP11_KA_CHUNKED_200 18
        ,
        {"HTTP/1.1 with chunked encoding and a 200 response",
         HTTP_RESPONSE,
         "HTTP/1.1 200 OK\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "0\r\n"
         "\r\n",
         TRUE,
         FALSE,
         1,
         1,
         200,
         1,
         {{"Transfer-Encoding", "chunked"}},
         0,
         ""}

#if !HTTP_PARSER_STRICT
#define SPACE_IN_FIELD_RES 19
        /* Should handle spaces in header fields */
        ,
        {"field space",
         HTTP_RESPONSE,
         "HTTP/1.1 200 OK\r\n"
         "Server: Microsoft-IIS/6.0\r\n"
         "X-Powered-By: ASP.NET\r\n"
         "en-US Content-Type: text/xml\r\n" /* this is the problem */
         "Content-Type: text/xml\r\n"
         "Content-Length: 16\r\n"
         "Date: Fri, 23 Jul 2010 18:45:38 GMT\r\n"
         "Connection: keep-alive\r\n"
         "\r\n"
         "<xml>hello</xml>" /* fake body */
         ,
         TRUE,
         FALSE,
         1,
         1,
         200,
         7,
         {{"Server", "Microsoft-IIS/6.0"},
          {"X-Powered-By", "ASP.NET"},
          {"en-US Content-Type", "text/xml"},
          {"Content-Type", "text/xml"},
          {"Content-Length", "16"},
          {"Date", "Fri, 23 Jul 2010 18:45:38 GMT"},
          {"Connection", "keep-alive"}},
         "<xml>hello</xml>"}
#endif /* !HTTP_PARSER_STRICT */

        ,
        {NULL, HTTP_RESPONSE, 0, FALSE, FALSE, 0, 0, 0, 0, {}, 0, {0}} /* sentinel */
};

class ResponseParserWithCounter : public Pillow::HttpResponseParser
{
public:
	int messageBeginCount;
	int headersCompleteCount;
	int messageContentCount;
	int messageCompleteCount;
	bool pauseInMessageBegin;
	bool pauseInHeadersComplete;
	bool pauseInMessageContent;
	bool pauseInMessageComplete;

	ResponseParserWithCounter()
	    : messageBeginCount(0),
	      headersCompleteCount(0),
	      messageContentCount(0),
	      messageCompleteCount(0),
	      pauseInMessageBegin(false),
	      pauseInHeadersComplete(false),
	      pauseInMessageContent(false),
	      pauseInMessageComplete(false)
	{}

protected:
	void messageBegin()
	{
		++messageBeginCount;
		Pillow::HttpResponseParser::messageBegin();
		if (pauseInMessageBegin)
			pause();
	}
	void headersComplete()
	{
		++headersCompleteCount;
		Pillow::HttpResponseParser::headersComplete();
		if (pauseInHeadersComplete)
			pause();
	}
	void messageContent(const char* data, int length)
	{
		++messageContentCount;
		Pillow::HttpResponseParser::messageContent(data, length);
		if (pauseInMessageContent)
			pause();
	}
	void messageComplete()
	{
		++messageCompleteCount;
		Pillow::HttpResponseParser::messageComplete();
		if (pauseInMessageComplete)
			pause();
	}
};

class ResponseParserWithIsParsingChecker : public Pillow::HttpResponseParser
{
public:
	bool wasParsingInMessageBegin;
	bool wasParsingInHeadersComplete;
	bool wasParsingInMessageContent;
	bool wasParsingInMessageComplete;

	ResponseParserWithIsParsingChecker()
	    : wasParsingInMessageBegin(false),
	      wasParsingInHeadersComplete(false),
	      wasParsingInMessageContent(false),
	      wasParsingInMessageComplete(false)
	{}

protected:
	void messageBegin()
	{
		Pillow::HttpResponseParser::messageBegin();
		wasParsingInMessageBegin = isParsing();
	}
	void headersComplete()
	{
		Pillow::HttpResponseParser::headersComplete();
		wasParsingInHeadersComplete = isParsing();
	}
	void messageContent(const char* data, int length)
	{
		Pillow::HttpResponseParser::messageContent(data, length);
		wasParsingInMessageContent = isParsing();
	}
	void messageComplete()
	{
		Pillow::HttpResponseParser::messageComplete();
		wasParsingInMessageComplete = isParsing();
	}
};

class ResponseParser : public Pillow::HttpResponseParser
{
public:
	std::function<void()> messageBeginCallback;
	std::function<void()> headersCompleteCallback;
	std::function<void()> messageContentCallback;
	std::function<void()> messageCompleteCallback;

protected:
	void messageBegin()
	{
		Pillow::HttpResponseParser::messageBegin();
		if (messageBeginCallback)
			messageBeginCallback();
	}
	void headersComplete()
	{
		Pillow::HttpResponseParser::headersComplete();
		if (headersCompleteCallback)
			headersCompleteCallback();
	}
	void messageContent(const char* data, int length)
	{
		Pillow::HttpResponseParser::messageContent(data, length);
		if (messageContentCallback)
			messageContentCallback();
	}
	void messageComplete()
	{
		Pillow::HttpResponseParser::messageComplete();
		if (messageCompleteCallback)
			messageCompleteCallback();
	}
};

class tst_HttpResponseParser : public QObject
{
	Q_OBJECT

private:
	bool compareParsedResponse(const Pillow::HttpResponseParser& p, const message& m)
	{
		bool ok;
		compareParsedResponse(p, m, &ok);
		return ok;
	}

	void compareParsedResponse(const Pillow::HttpResponseParser& p, const message& m, bool* ok)
	{
		*ok = false;
		QCOMPARE(p.statusCode(), m.status_code);
		QCOMPARE(p.httpMajor(), m.http_major);
		QCOMPARE(p.httpMinor(), m.http_minor);
		QCOMPARE(p.headers().size(), m.num_headers);
		QCOMPARE(p.content(), QByteArray(m.body));
		QCOMPARE(p.shouldKeepAlive(), m.should_keep_alive == TRUE);
		QCOMPARE(p.completesOnEof(), m.message_complete_on_eof);
		*ok = true; // All passed
		throw "Parsed response does not match message";
	}

private slots:
	void test_initial_state()
	{
		Pillow::HttpResponseParser p;

		QVERIFY(!p.hasError());
		QCOMPARE(p.error(), HPE_OK);
		QCOMPARE(p.statusCode(), 0);
		QCOMPARE(p.httpMinor(), 0);
		QCOMPARE(p.httpMajor(), 0);
		QCOMPARE(p.headers().size(), 0);
		QCOMPARE(p.shouldKeepAlive(), 0);
		QCOMPARE(p.completesOnEof(), false);
		QVERIFY(!p.isParsing());
	}

	void test_single_valid_responses_data()
	{
		QTest::addColumn<message>("testMessage");

		for (const message* m = responses; m->name != NULL; ++m)
			QTest::newRow(m->name) << *m;
	}

	void test_single_valid_responses()
	{
		QFETCH(message, testMessage);
		QVERIFY(testMessage.type = HTTP_RESPONSE);

		Pillow::HttpResponseParser p;
		size_t consumed = p.inject(testMessage.raw);
		p.injectEof();

		QCOMPARE(consumed, strlen(testMessage.raw));
		QVERIFY(!p.hasError());
		QCOMPARE(p.statusCode(), testMessage.status_code);
		QCOMPARE(p.httpMajor(), testMessage.http_major);
		QCOMPARE(p.httpMinor(), testMessage.http_minor);
		QCOMPARE(p.headers().size(), testMessage.num_headers);
		QCOMPARE(p.content(), QByteArray(testMessage.body));
		QCOMPARE(p.shouldKeepAlive(), testMessage.should_keep_alive == TRUE);
		QCOMPARE(p.completesOnEof(), testMessage.message_complete_on_eof);
		QVERIFY(!p.isParsing());
	}

	void test_keep_alive_responses()
	{
		Pillow::HttpResponseParser p;
		int count = 0;
		for (const message* m1 = responses; m1->name != NULL; ++m1)
		{
			if (m1->should_keep_alive == 0)
				continue;
			++count;
			p.inject(m1->raw);
			QVERIFY(!p.hasError());
			QCOMPARE(p.statusCode(), m1->status_code);
			QCOMPARE(p.httpMajor(), m1->http_major);
			QCOMPARE(p.httpMinor(), m1->http_minor);
			QCOMPARE(p.headers().size(), m1->num_headers);
			QCOMPARE(p.content(), QByteArray(m1->body));
			QCOMPARE(p.shouldKeepAlive(), m1->should_keep_alive == TRUE);
			QCOMPARE(p.completesOnEof(), m1->message_complete_on_eof);

			for (const message* m2 = responses; m2->name != NULL; ++m2)
			{
				if (m2->should_keep_alive == 0)
					continue;
				++count;
				p.inject(m2->raw);
				QVERIFY(!p.hasError());
				QCOMPARE(p.statusCode(), m2->status_code);
				QCOMPARE(p.httpMajor(), m2->http_major);
				QCOMPARE(p.httpMinor(), m2->http_minor);
				QCOMPARE(p.headers().size(), m2->num_headers);
				QCOMPARE(p.content(), QByteArray(m2->body));
				QCOMPARE(p.shouldKeepAlive(), m2->should_keep_alive == TRUE);
				QCOMPARE(p.completesOnEof(), m2->message_complete_on_eof);
			}
		}
		QVERIFY(count > 2);
	}

	void test_invalid_responses()
	{
		Pillow::HttpResponseParser p;
		p.inject("HTTP/1.1 BADBAD=-=-=-=-1-1-1-1-\r\nFFFFFFFUUUUUUUUUUUU\r\n\r\n");

		QVERIFY(p.hasError());
		QCOMPARE(p.error(), HPE_INVALID_STATUS);
		QCOMPARE(p.statusCode(), 0);
		QCOMPARE(p.httpMajor(), 1);
		QCOMPARE(p.httpMinor(), 1);
		QCOMPARE(p.headers().size(), 0);
		QCOMPARE(p.content(), QByteArray());
		QVERIFY(!p.shouldKeepAlive());
		QVERIFY(p.completesOnEof());
		QVERIFY(!p.isParsing());

		p.injectEof();

		QVERIFY(p.hasError());
		QCOMPARE(p.error(), HPE_INVALID_STATUS);
		QCOMPARE(p.statusCode(), 0);
		QCOMPARE(p.httpMajor(), 1);
		QCOMPARE(p.httpMinor(), 1);
		QCOMPARE(p.headers().size(), 0);
		QCOMPARE(p.content(), QByteArray());
		QVERIFY(!p.shouldKeepAlive());
		QVERIFY(p.completesOnEof());
		QVERIFY(!p.isParsing());

		// Injecting a good message should not make the parser recover.
		p.inject("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
		QVERIFY(p.hasError());
		QCOMPARE(p.error(), HPE_INVALID_STATUS);
		QCOMPARE(p.statusCode(), 0);
		QCOMPARE(p.httpMajor(), 1);
		QCOMPARE(p.httpMinor(), 1);
		QCOMPARE(p.headers().size(), 0);
		QCOMPARE(p.content(), QByteArray());
		QVERIFY(!p.shouldKeepAlive());
		QVERIFY(p.completesOnEof());
		QVERIFY(!p.isParsing());
	}

	void should_recover_from_error_when_cleared()
	{
		Pillow::HttpResponseParser p;
		p.inject("HTTP/1.1 POPOPO=-=-=-=-1-1-1-1-\r\nFFFFFFFFUUUUUUUUUUUUUU\r\n\r\n");

		QVERIFY(p.hasError());
		QCOMPARE(p.error(), HPE_INVALID_STATUS);
		QCOMPARE(p.statusCode(), 0);
		QCOMPARE(p.httpMajor(), 1);
		QCOMPARE(p.httpMinor(), 1);
		QCOMPARE(p.headers().size(), 0);
		QCOMPARE(p.content(), QByteArray());
		QVERIFY(!p.shouldKeepAlive());
		QVERIFY(p.completesOnEof());
		QVERIFY(!p.isParsing());

		p.clear();

		p.inject("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
		QVERIFY(!p.hasError());
		QCOMPARE(p.error(), HPE_OK);
		QCOMPARE(p.statusCode(), 200);
		QCOMPARE(p.httpMajor(), 1);
		QCOMPARE(p.httpMinor(), 1);
		QCOMPARE(p.headers().size(), 1);
		QCOMPARE(p.content(), QByteArray());
		QVERIFY(p.shouldKeepAlive());
		QVERIFY(!p.completesOnEof());
		QVERIFY(!p.isParsing());
	}

	void should_clear_all_fields_when_cleared()
	{
		Pillow::HttpResponseParser p;
		p.inject("HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nabc");
		QVERIFY(!p.hasError());
		QCOMPARE(p.statusCode(), 200);
		QCOMPARE(p.httpMajor(), 1);
		QCOMPARE(p.httpMinor(), 1);
		QCOMPARE(p.headers().size(), 1);
		QCOMPARE(p.content(), QByteArray("abc"));
		QVERIFY(p.shouldKeepAlive());
		QVERIFY(!p.completesOnEof());

		p.clear();

		QVERIFY(!p.hasError());
		QCOMPARE(p.error(), HPE_OK);
		QCOMPARE(p.statusCode(), 0);
		QCOMPARE(p.httpMinor(), 0);
		QCOMPARE(p.httpMajor(), 0);
		QCOMPARE(p.headers().size(), 0);
		QCOMPARE(p.shouldKeepAlive(), 0);
		QCOMPARE(p.completesOnEof(), false);
		QCOMPARE(p.content(), QByteArray());
	}

	void should_only_consume_one_response_at_a_time_for_pipelined_responses()
	{
		QByteArray threeResponses = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\nHTTP/1.1 201 OK\r\nContent-Length: 0\r\n\r\nHTTP/1.1 202 "
		                            "OK\r\nContent-Length: 0\r\n\r\n";

		Pillow::HttpResponseParser p;
		int consumed = p.inject(threeResponses);

		QCOMPARE(consumed, 38);
		QCOMPARE(p.statusCode(), 200);
		QVERIFY(!p.completesOnEof());

		int consumed2 = p.inject(threeResponses.mid(consumed));
		QCOMPARE(p.statusCode(), 201);
		QVERIFY(!p.completesOnEof());

		p.inject(threeResponses.mid(consumed).mid(consumed2));
		QCOMPARE(p.statusCode(), 202);
		QVERIFY(!p.completesOnEof());
	}

	void should_detect_too_much_data()
	{
		Pillow::HttpResponseParser p;
		p.inject("HTTP/1.0 200 OK\r\nContent-Length:4\r\n\r\nabcdefg"); // Too much content!

		QVERIFY(!p.hasError());
		QCOMPARE(p.statusCode(), 200);
		QCOMPARE(p.httpMajor(), 1);
		QCOMPARE(p.httpMinor(), 0);
		QCOMPARE(p.headers().size(), 1);
		QCOMPARE(p.content(), QByteArray("abcd"));
		QVERIFY(!p.shouldKeepAlive());
		QVERIFY(!p.completesOnEof());
	}

	void should_call_callbacks()
	{
		ResponseParserWithCounter p;
		QCOMPARE(p.messageBeginCount, 0);
		QCOMPARE(p.headersCompleteCount, 0);
		QCOMPARE(p.messageContentCount, 0);
		QCOMPARE(p.messageCompleteCount, 0);

		p.inject("HTTP/1.1");
		QCOMPARE(p.messageBeginCount, 1);
		QCOMPARE(p.headersCompleteCount, 0);
		QCOMPARE(p.messageContentCount, 0);
		QCOMPARE(p.messageCompleteCount, 0);

		p.inject(" 200 OK\r\nContent-Length: 4\r\n");
		QCOMPARE(p.messageBeginCount, 1);
		QCOMPARE(p.headersCompleteCount, 0);
		QCOMPARE(p.messageContentCount, 0);
		QCOMPARE(p.messageCompleteCount, 0);

		p.inject("\r\n");
		QCOMPARE(p.messageBeginCount, 1);
		QCOMPARE(p.headersCompleteCount, 1);
		QCOMPARE(p.messageContentCount, 0);
		QCOMPARE(p.messageCompleteCount, 0);

		p.inject("12");
		QCOMPARE(p.messageBeginCount, 1);
		QCOMPARE(p.headersCompleteCount, 1);
		QCOMPARE(p.messageContentCount, 1);
		QCOMPARE(p.messageCompleteCount, 0);

		p.inject("34");
		QCOMPARE(p.messageBeginCount, 1);
		QCOMPARE(p.headersCompleteCount, 1);
		QCOMPARE(p.messageContentCount, 2);
		QCOMPARE(p.messageCompleteCount, 1);

		p.inject("HTTP/1.1 302 Found\r\nLocation: somewhere\r\nContent-Length: 0\r\n\r\n");
		QCOMPARE(p.messageBeginCount, 2);
		QCOMPARE(p.headersCompleteCount, 2);
		QCOMPARE(p.messageContentCount, 2);
		QCOMPARE(p.messageCompleteCount, 2);

		p.inject("HTTP/1.1 400 Bad Request\r\nContent-Length: 2\r\n\r\n12");
		QCOMPARE(p.messageBeginCount, 3);
		QCOMPARE(p.headersCompleteCount, 3);
		QCOMPARE(p.messageContentCount, 3);
		QCOMPARE(p.messageCompleteCount, 3);
	}

	void should_be_pausable_in_callbacks()
	{
		QByteArray responseHeaders = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nContent-Type: text/plain\r\n\r\n";
		QByteArray responseContent = "1234";
		QByteArray response = responseHeaders + responseContent;

		{
			ResponseParserWithCounter p;
			p.pauseInMessageBegin = true;
			int consumed = p.inject(response);
			QCOMPARE(consumed, 1); // http_parser wants at least 1 valid byte to get started.
			QCOMPARE(p.messageBeginCount, 1);
			QCOMPARE(p.headersCompleteCount, 0);
			QCOMPARE(p.messageContentCount, 0);
			QCOMPARE(p.messageCompleteCount, 0);

			// Should allow continuing.
			p.inject(response.mid(consumed));
			QCOMPARE(p.messageBeginCount, 1);
			QCOMPARE(p.headersCompleteCount, 1);
			QCOMPARE(p.messageContentCount, 1);
			QCOMPARE(p.messageCompleteCount, 1);
			QCOMPARE(p.content(), QByteArray("1234"));
		}

		{
			ResponseParserWithCounter p;
			p.pauseInHeadersComplete = true;
			int consumed = p.inject(response);
			QCOMPARE(
			    consumed,
			    responseHeaders.size() -
			        1); // headers are complete as soon as the parser encountered "\r\n\r" (it will then silently consume the other "\n")
			QVERIFY(responseHeaders.size() > 0);
			QCOMPARE(p.messageBeginCount, 1);
			QCOMPARE(p.headersCompleteCount, 1);
			QCOMPARE(p.messageContentCount, 0);
			QCOMPARE(p.messageCompleteCount, 0);

			// Should allow continuing.
			p.inject(response.mid(consumed));
			QCOMPARE(p.messageBeginCount, 1);
			QCOMPARE(p.headersCompleteCount, 1);
			QCOMPARE(p.messageContentCount, 1);
			QCOMPARE(p.messageCompleteCount, 1);
			QCOMPARE(p.content(), QByteArray("1234"));
		}

		{
			ResponseParserWithCounter p;
			p.pauseInMessageContent = true;
			int consumed = p.inject(response);
			QCOMPARE(consumed, responseHeaders.size() + responseContent.size() - 1);
			QVERIFY(responseContent.size() > 0);
			QCOMPARE(p.messageBeginCount, 1);
			QCOMPARE(p.headersCompleteCount, 1);
			QCOMPARE(p.messageContentCount, 1);
			QCOMPARE(p.messageCompleteCount, 0);

			// Should allow continuing.
			p.inject(response.mid(consumed));
			QCOMPARE(p.messageBeginCount, 1);
			QCOMPARE(p.headersCompleteCount, 1);
			QCOMPARE(p.messageContentCount, 1);
			QCOMPARE(p.messageCompleteCount, 1);
			QCOMPARE(p.content(), QByteArray("1234"));
		}

		{
			ResponseParserWithCounter p;
			p.pauseInMessageComplete = true;
			int consumed = p.inject(response);
			QCOMPARE(consumed, response.size());
			QCOMPARE(p.messageBeginCount, 1);
			QCOMPARE(p.headersCompleteCount, 1);
			QCOMPARE(p.messageContentCount, 1);
			QCOMPARE(p.messageCompleteCount, 1);

			// Continuing should do nothing as there is nothing left.
			p.inject(response.mid(consumed));
			QCOMPARE(p.messageBeginCount, 1);
			QCOMPARE(p.headersCompleteCount, 1);
			QCOMPARE(p.messageContentCount, 1);
			QCOMPARE(p.messageCompleteCount, 1);
			QCOMPARE(p.content(), QByteArray("1234"));
		}
	}

	void should_be_parsing_while_parsing()
	{
		ResponseParserWithIsParsingChecker p;
		QVERIFY(!p.wasParsingInMessageBegin);
		QVERIFY(!p.wasParsingInHeadersComplete);
		QVERIFY(!p.wasParsingInMessageContent);
		QVERIFY(!p.wasParsingInMessageComplete);
		QVERIFY(!p.isParsing());

		p.inject("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nab");
		QVERIFY(p.wasParsingInMessageBegin);
		QVERIFY(p.wasParsingInHeadersComplete);
		QVERIFY(p.wasParsingInMessageContent);
		QVERIFY(p.wasParsingInMessageComplete);
		QVERIFY(!p.isParsing());
	}

	void should_pause_parsing_when_cleared_while_parsing()
	{
		QByteArray response = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nContent-Type: text/plain\r\n\r\nabcd";

		{
			ResponseParser p;
			p.messageBeginCallback = [&] { p.clear(); };
			int consumed = p.inject(response);
			QCOMPARE(consumed, 1); // http_parser wants at least 1 valid byte to get started.

			// Should be reset and ready to parse a new response.
			QVERIFY(!p.hasError());
			p.messageBeginCallback = 0;

			QCOMPARE(p.inject(response), response.size());
			QVERIFY(!p.hasError());
			QCOMPARE(p.statusCode(), 200);
			QCOMPARE(p.httpMajor(), 1);
			QCOMPARE(p.httpMinor(), 1);
			QCOMPARE(p.headers().size(), 2);
			QCOMPARE(p.content(), QByteArray("abcd"));
			QVERIFY(p.shouldKeepAlive());
			QVERIFY(!p.completesOnEof());
		}

		{
			ResponseParser p;
			p.headersCompleteCallback = [&] { p.clear(); };
			QCOMPARE(p.inject(response), 63);

			// Should be reset and ready to parse a new response.
			QVERIFY(!p.hasError());
			p.headersCompleteCallback = 0;

			QCOMPARE(p.inject(response), response.size());
			QVERIFY(!p.hasError());
			QCOMPARE(p.statusCode(), 200);
			QCOMPARE(p.httpMajor(), 1);
			QCOMPARE(p.httpMinor(), 1);
			QCOMPARE(p.headers().size(), 2);
			QCOMPARE(p.content(), QByteArray("abcd"));
			QVERIFY(p.shouldKeepAlive());
			QVERIFY(!p.completesOnEof());
		}

		{
			ResponseParser p;
			p.messageContentCallback = [&] { p.clear(); };
			QCOMPARE(p.inject(response), 67);

			// Should be reset and ready to parse a new response.
			QVERIFY(!p.hasError());
			p.messageContentCallback = 0;

			QCOMPARE(p.inject(response), response.size());
			QVERIFY(!p.hasError());
			QCOMPARE(p.statusCode(), 200);
			QCOMPARE(p.httpMajor(), 1);
			QCOMPARE(p.httpMinor(), 1);
			QCOMPARE(p.headers().size(), 2);
			QCOMPARE(p.content(), QByteArray("abcd"));
			QVERIFY(p.shouldKeepAlive());
			QVERIFY(!p.completesOnEof());
		}

		{
			ResponseParser p;
			p.messageCompleteCallback = [&] { p.clear(); };
			QCOMPARE(p.inject(response), 68);

			// Should be reset and ready to parse a new response.
			QVERIFY(!p.hasError());
			p.messageCompleteCallback = 0;

			QCOMPARE(p.inject(response), response.size());
			QVERIFY(!p.hasError());
			QCOMPARE(p.statusCode(), 200);
			QCOMPARE(p.httpMajor(), 1);
			QCOMPARE(p.httpMinor(), 1);
			QCOMPARE(p.headers().size(), 2);
			QCOMPARE(p.content(), QByteArray("abcd"));
			QVERIFY(p.shouldKeepAlive());
			QVERIFY(!p.completesOnEof());
		}
	}
};

QTEST_MAIN(tst_HttpResponseParser)
#include "tst_httpresponseparser.moc"