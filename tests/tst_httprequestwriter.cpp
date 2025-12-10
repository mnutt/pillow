#include <QtCore/QObject>
#include <QtTest/QtTest>
#include <QtCore/QBuffer>
#include <HttpClient.h>
#include "Helpers.h"

//
// HttpRequestWriter test class
//

class tst_HttpRequestWriter : public QObject
{
	Q_OBJECT
	QBuffer *buffer;

private slots:
	void init()
	{
		buffer = new QBuffer(this);
		buffer->open(QBuffer::ReadWrite);
	}

	void cleanup()
	{
		delete buffer; buffer = 0;
	}

	QByteArray readAll()
	{
		QByteArray data = buffer->data();
		buffer->seek(0);
		return data;
	}

private slots:
	void test_initial_state()
	{
		Pillow::HttpRequestWriter w;
		QVERIFY(w.device() == 0);
		w.setDevice(buffer);
		QVERIFY(w.device() == buffer);
	}

	void test_write_get()
	{
		Pillow::HttpRequestWriter w; w.setDevice(buffer);

		w.get("/some/path", Pillow::HttpHeaderCollection());
		QCOMPARE(readAll(), QByteArray("GET /some/path HTTP/1.1\r\n\r\n"));

		w.get("/other/cool%20path", Pillow::HttpHeaderCollection() <<
			  Pillow::HttpHeader("My-Header", "Is-Cool") <<
			  Pillow::HttpHeader("X-And-Another", "Is-Better"));
		QCOMPARE(readAll(), QByteArray("GET /other/cool%20path HTTP/1.1\r\nMy-Header: Is-Cool\r\nX-And-Another: Is-Better\r\n\r\n"));
	}

	void test_write_head()
	{
		Pillow::HttpRequestWriter w; w.setDevice(buffer);

		w.head("/some/path", Pillow::HttpHeaderCollection());
		QCOMPARE(readAll(), QByteArray("HEAD /some/path HTTP/1.1\r\n\r\n"));

		w.head("/other/cool%20path", Pillow::HttpHeaderCollection() <<
			  Pillow::HttpHeader("My-Header", "Is-Cool") <<
			  Pillow::HttpHeader("X-And-Another", "Is-Better"));
		QCOMPARE(readAll(), QByteArray("HEAD /other/cool%20path HTTP/1.1\r\nMy-Header: Is-Cool\r\nX-And-Another: Is-Better\r\n\r\n"));
	}

	void test_write_post()
	{
		Pillow::HttpRequestWriter w; w.setDevice(buffer);

		w.post("/some/path.txt", Pillow::HttpHeaderCollection(), QByteArray());
		QCOMPARE(readAll(), QByteArray("POST /some/path.txt HTTP/1.1\r\n\r\n"));

		w.post("/other/path.txt", Pillow::HttpHeaderCollection() << Pillow::HttpHeader("One", "Header"), QByteArray("Some Data"));
		QCOMPARE(readAll(), QByteArray("POST /other/path.txt HTTP/1.1\r\nOne: Header\r\nContent-Length: 9\r\n\r\nSome Data"));
	}
	void test_write_put()
	{
		Pillow::HttpRequestWriter w; w.setDevice(buffer);

		w.put("/some/path.txt", Pillow::HttpHeaderCollection(), QByteArray());
		QCOMPARE(readAll(), QByteArray("PUT /some/path.txt HTTP/1.1\r\n\r\n"));

		w.put("/other/path.txt", Pillow::HttpHeaderCollection() << Pillow::HttpHeader("One", "Header"), QByteArray("Some Data"));
		QCOMPARE(readAll(), QByteArray("PUT /other/path.txt HTTP/1.1\r\nOne: Header\r\nContent-Length: 9\r\n\r\nSome Data"));
	}

	void test_write_deleteResource()
	{
		Pillow::HttpRequestWriter w; w.setDevice(buffer);

		w.deleteResource("/some/path", Pillow::HttpHeaderCollection());
		QCOMPARE(readAll(), QByteArray("DELETE /some/path HTTP/1.1\r\n\r\n"));

		w.deleteResource("/other/cool%20path", Pillow::HttpHeaderCollection() <<
			  Pillow::HttpHeader("My-Header", "Is-Cool") <<
			  Pillow::HttpHeader("X-And-Another", "Is-Better"));
		QCOMPARE(readAll(), QByteArray("DELETE /other/cool%20path HTTP/1.1\r\nMy-Header: Is-Cool\r\nX-And-Another: Is-Better\r\n\r\n"));
	}
};

QTEST_MAIN(tst_HttpRequestWriter)
#include "tst_httprequestwriter.moc"