#include "HttpConnection.h"
#include "HttpHelpers.h"
#include "ByteArrayHelpers.h"
#include "parser/parser.h"
#include <QtCore/QIODevice>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtCore/QStringBuilder>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QLocalSocket>
#include <QtCore/QVarLengthArray>
#include <QtCore/QThread>
#include <QtCore/QDebug>

//
// Helpers
//

// Helper function to append HttpHeader to QByteArray (replacing custom ByteArray functionality)
inline QByteArray& appendHttpHeader(QByteArray& ba, const Pillow::HttpHeader& header)
{
	const int requiredSpace = header.first.size() + 2 + header.second.size() + 2;
	if (ba.capacity() >= ba.size() + requiredSpace)
	{
		// Common case where there is enough space.
		int oldSize = ba.size();
		ba.resize(oldSize + requiredSpace);
		char* d = ba.data() + oldSize;
		memcpy(d, header.first.constData(), header.first.size()); d += header.first.size();
		d[0] = ':'; d[1] = ' '; d += 2;
		memcpy(d, header.second.constData(), header.second.size()); d += header.second.size();
		d[0] = '\r'; d[1] = '\n';
	}
	else
		ba.append(header.first).append(": ", 2).append(header.second).append("\r\n", 2);
	return ba;
}

namespace Pillow
{
	namespace Tokens
	{
		#define DEFINE_TOKEN(tokenName, tokenValue) static const Pillow::Token tokenName##Token(tokenValue)
		#define DEFINE_LOWERCASE_TOKEN(tokenName, tokenValue)  static const Pillow::LowerCaseToken tokenName##Token(tokenValue)
		DEFINE_TOKEN(crLf, "\r\n");
		DEFINE_TOKEN(contentLengthOut, "Content-Length: ");
		DEFINE_TOKEN(contentTypeTextPlainTokenHeader, "Content-Type: text/plain\r\n");
		DEFINE_TOKEN(connectionKeepAliveHeader, "Connection: keep-alive\r\n");
		DEFINE_TOKEN(connectionCloseHeader, "Connection: close\r\n");
		DEFINE_TOKEN(httpSlash11, "HTTP/1.1");
		DEFINE_TOKEN(head, "HEAD");
		DEFINE_TOKEN(colonSpace, ": ");
		DEFINE_LOWERCASE_TOKEN(connection, "connection");
		DEFINE_LOWERCASE_TOKEN(contentLength, "content-length");
		DEFINE_LOWERCASE_TOKEN(contentType, "content-type");
		DEFINE_LOWERCASE_TOKEN(expect, "expect");
		DEFINE_LOWERCASE_TOKEN(hundredDashContinue, "100-continue");
		DEFINE_LOWERCASE_TOKEN(keepAlive, "keep-alive");
		DEFINE_LOWERCASE_TOKEN(close, "close");
		DEFINE_LOWERCASE_TOKEN(transferEncoding, "transfer-encoding");
		DEFINE_LOWERCASE_TOKEN(chunked, "chunked");
		#undef DEFINE_TOKEN
		#undef DEFINE_LOWERCASE_TOKEN
	}

	struct HttpHeaderRef
	{
		int fieldPos, fieldLength, valuePos, valueLength;
		inline HttpHeaderRef(int fieldPos, int fieldLength, int valuePos, int valueLength)
			: fieldPos(fieldPos), fieldLength(fieldLength), valuePos(valuePos), valueLength(valueLength) {}
		inline HttpHeaderRef() {}
	};
}
Q_DECLARE_TYPEINFO(Pillow::HttpHeaderRef, Q_PRIMITIVE_TYPE);

using namespace Pillow::Tokens;
using namespace Pillow::ByteArrayHelpers;

//
// HttpConnectionPrivate
//

#define PERCENT_DECODABLE(fieldName) \
	QByteArray _##fieldName; \
	mutable QString _##fieldName##Decoded; \
	inline const QString& fieldName##Decoded() const { if (_##fieldName##Decoded.isEmpty() && !_##fieldName.isEmpty()) _##fieldName##Decoded = Pillow::ByteArrayHelpers::percentDecode(_##fieldName); return _##fieldName##Decoded; }
#define FORWARD_PERCENT_DECODABLE(fieldName) \
	const QByteArray& Pillow::HttpConnection::fieldName() const { return d_ptr->_##fieldName; } \
	const QString& Pillow::HttpConnection::fieldName##Decoded() const { return d_ptr->fieldName##Decoded(); }

namespace Pillow
{
	class HttpConnectionPrivate
	{
		Q_DECLARE_PUBLIC(HttpConnection)
		HttpConnection* q_ptr;

	public:
		HttpConnectionPrivate(HttpConnection* connection);

	public:
		Pillow::HttpConnection::State _state;
		QIODevice* _inputDevice,* _outputDevice;
		http_parser _parser;

		// Request fields.
		QByteArray _requestBuffer;
		QByteArray _requestMethod, _requestHttpVersion, _requestContent;
		PERCENT_DECODABLE(requestUri)
		PERCENT_DECODABLE(requestFragment)
		PERCENT_DECODABLE(requestPath)
		PERCENT_DECODABLE(requestQueryString)
		QVarLengthArray<Pillow::HttpHeaderRef, 32> _requestHeadersRef;
		Pillow::HttpHeaderCollection _requestHeaders;
		int _requestContentLength; int _requestContentLengthHeaderIndex;
		bool _requestHttp11;
		Pillow::HttpParamCollection _requestParams;

		// Response fields.
		QByteArray _responseHeadersBuffer;
		int _responseStatusCode;
		qint64 _responseContentLength, _responseContentBytesSent;
		bool _responseConnectionKeepAlive;
		bool _responseChunkedTransferEncoding;

	public:
		void initialize();
		void processInput();
		void setupRequestHeaders();
		void transitionToReceivingHeaders();
		void transitionToReceivingContent();
		void transitionToSendingHeaders();
		void transitionToSendingContent();
		void transitionToCompleted();
		void flush();
		void drain();
		void transitionToFlushing();
		void transitionToClosed();
		void writeRequestErrorResponse(int statusCode = 400); // Used internally when an error happens while receiving a request. It sends an error response to the client and closes the connection right away.

		static void parser_http_field(void *data, const char *field, size_t flen, const char *value, size_t vlen);

	public: // From public interface.
		void writeResponse(int statusCode = 200, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection(), const QByteArray& content = QByteArray());
		void writeResponseString(int statusCode = 200, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection(), const QString& content = QString());
		void writeHeaders(int statusCode = 200, const Pillow::HttpHeaderCollection& headers = Pillow::HttpHeaderCollection());
		void writeContent(const QByteArray& content);
		void endContent();
		void close();
	};
}

Pillow::HttpConnectionPrivate::HttpConnectionPrivate(HttpConnection *connection)
	: q_ptr(connection), _state(Pillow::HttpConnection::Uninitialized), _inputDevice(0), _outputDevice(0)
{
	// Detach bytearrays we're going to write to from global shared null as we'll be thinkering with their internal data with the assumption that they are never shared.
	_requestBuffer.detach();
	_requestContent.detach();
}

inline void Pillow::HttpConnectionPrivate::initialize()
{
	memset(&_parser, 0, sizeof(http_parser));
	_parser.data = this;
	_parser.http_field = &HttpConnectionPrivate::parser_http_field;

	// Clear any leftover data from a previous potentially failed request (that would not have gone though "transitionToCompleted")
	if (_requestBuffer.capacity() <= Pillow::HttpConnection::MaximumRequestHeaderLength
		&& _requestBuffer.size() != 0) _requestBuffer.resize(0);
	else _requestBuffer.clear();
	_requestHeadersRef.clear();
	if (_requestParams.capacity() > 16) _requestParams.clear();
	else while(!_requestParams.isEmpty()) _requestParams.pop_back();

	// Enter the initial working state and schedule processing of any data already available on the device.
	transitionToReceivingHeaders();
	if (_inputDevice && _inputDevice->bytesAvailable() > 0) {
		qDebug() << "HttpConnection::initialize() - scheduling processInput() via QTimer::singleShot - bytesAvailable:" << _inputDevice->bytesAvailable();
		QTimer::singleShot(0, q_ptr, SLOT(processInput()));
	}
}

inline void Pillow::HttpConnectionPrivate::processInput()
{
	// Add detailed logging to understand call patterns
	static int callCount = 0;
	callCount++;
	qDebug() << "HttpConnection::processInput() - call #" << callCount << "- state:" << _state << "- thread:" << QThread::currentThread();
	
	// Log the call stack by checking if we're in an event vs direct call
	QObject* sender = q_ptr->sender();
	if (sender) {
		qDebug() << "  Called from signal, sender:" << sender << "sender type:" << sender->metaObject()->className();
	} else {
		qDebug() << "  Called directly (not from signal)";
	}
	
	if (_state != Pillow::HttpConnection::ReceivingHeaders && _state != Pillow::HttpConnection::ReceivingContent) {
		qDebug() << "  Ignoring processInput - wrong state";
		return;
	}

	qint64 bytesAvailable = _inputDevice->bytesAvailable();
	qDebug() << "  bytesAvailable:" << bytesAvailable << "- bufferSize:" << _requestBuffer.size();
	
	// Prevent infinite loop: only proceed if there's new data to read OR existing data in buffer to process
	if (bytesAvailable == 0 && _requestBuffer.isEmpty()) {
		qDebug() << "  No data to process - returning early";
		return;
	}
	
	if (bytesAvailable > 0)
	{
		// Use a temporary buffer and append - simpler and safer
		QByteArray tempBuffer(bytesAvailable, '\0');
		const qint64 bytesRead = _inputDevice->read(tempBuffer.data(), bytesAvailable);
		qDebug() << "  bytesRead:" << bytesRead;
		if (bytesRead > 0) {
			tempBuffer.resize(bytesRead);
			_requestBuffer.append(tempBuffer);
			qDebug() << "  Buffer after append: size=" << _requestBuffer.size();
			qDebug() << "  Buffer content (first 200 chars):" << QString::fromLatin1(_requestBuffer.left(200));
		}
	}

	if (_state == Pillow::HttpConnection::ReceivingHeaders)
	{
		if (!_requestBuffer.isEmpty()) {
			qDebug() << "  Executing parser on buffer size:" << _requestBuffer.size() << "nread:" << _parser.nread;
			thin_http_parser_execute(&_parser, _requestBuffer.constData(), _requestBuffer.size(), _parser.nread);
			qDebug() << "  After parser - nread:" << _parser.nread << "has_error:" << thin_http_parser_has_error(&_parser) << "is_finished:" << thin_http_parser_is_finished(&_parser);
		}

		if (_parser.nread > Pillow::HttpConnection::MaximumRequestHeaderLength || thin_http_parser_has_error(&_parser)) {
			qDebug() << "  PARSER ERROR - nread:" << _parser.nread << "has_error:" << thin_http_parser_has_error(&_parser);
			return writeRequestErrorResponse(400); // Bad client Request!
		}
		else if (thin_http_parser_is_finished(&_parser))
			transitionToReceivingContent();
	}
	else if (_state == Pillow::HttpConnection::ReceivingContent)
	{
		if (_requestBuffer.size() - int(_parser.body_start) >= _requestContentLength) {
			qDebug() << "  CALLER: processInput() -> transitionToSendingHeaders() (content received)";
			transitionToSendingHeaders(); // Finished receiving the content.
		}
	}
}

inline void Pillow::HttpConnectionPrivate::transitionToReceivingHeaders()
{
	if (_state == Pillow::HttpConnection::ReceivingHeaders) return;
	_state = Pillow::HttpConnection::ReceivingHeaders;

	thin_http_parser_init(&_parser);
	_requestContentLength = 0;
	_requestContentLengthHeaderIndex = -1;
	_requestHttp11 = false;
}

inline void Pillow::HttpConnectionPrivate::setupRequestHeaders()
{
	char* data = _requestBuffer.data();

	while (_requestHeaders.size() > _requestHeadersRef.size()) _requestHeaders.pop_back();
	if (_requestHeaders.capacity() == 0 && _requestHeadersRef.size() > 0) _requestHeaders.resize(_requestHeadersRef.size());
	else while (_requestHeaders.size() < _requestHeadersRef.size()) _requestHeaders.push_back(HttpHeader());

	auto header = _requestHeaders.begin();
	for (const HttpHeaderRef* ref = _requestHeadersRef.data(), *refE = _requestHeadersRef.data() + _requestHeadersRef.size(); ref < refE; ++ref, ++header)
	{
		setFromRawDataAndNullterm((*header).first, data, ref->fieldPos, ref->fieldLength);
		setFromRawDataAndNullterm((*header).second, data, ref->valuePos, ref->valueLength);
	}
}

inline void Pillow::HttpConnectionPrivate::transitionToReceivingContent()
{
	if (_state == Pillow::HttpConnection::ReceivingContent) return;
	_state = Pillow::HttpConnection::ReceivingContent;

	setupRequestHeaders();

	bool contentLengthParseOk = true;
	if (_requestContentLengthHeaderIndex >= 0)
		_requestContentLength = _requestHeaders.at(_requestContentLengthHeaderIndex).second.toInt(&contentLengthParseOk);

	// Exit early if the client sent an incorrect or unacceptable content-length.
	if (_requestContentLength < 0)
		return writeRequestErrorResponse(400); // Invalid request: negative content length does not make sense.
	else if (_requestContentLength > Pillow::HttpConnection::MaximumRequestContentLength || !contentLengthParseOk)
		return writeRequestErrorResponse(413); // Request entity too large.

	if (_requestContentLength > 0)
	{
		if (asciiEqualsCaseInsensitive(_requestHeaders.getFieldValue(expectToken), hundredDashContinueToken))
			_outputDevice->write("HTTP/1.1 100 Continue\r\n\r\n");// The client politely wanted to know if it could proceed with his payload. All clear!

		// Resize the request buffer right away to avoid too many reallocs later.
		// NOTE: This invalidates the request headers QByteArrays if the reallocation
		// changes the buffer's address (very likely unless the content-length is tiny).
		_requestBuffer.reserve(int(_parser.body_start + _requestContentLength + 1));

		// So do invalidate the request headers.
		if (_requestHeaders.size() > 0) _requestHeaders.pop_back();

		// Pump; the content may already be sitting in the buffers.
		qDebug() << "HttpConnection::transitionToReceivingContent() - calling processInput() directly - requestContentLength:" << _requestContentLength << "buffer size:" << _requestBuffer.size();
		processInput();
	}
	else
	{
		qDebug() << "  CALLER: transitionToReceivingContent() -> transitionToSendingHeaders() (no content to receive)";
		transitionToSendingHeaders(); // No content to receive. Go straight to sending headers.
	}
}

inline void Pillow::HttpConnectionPrivate::transitionToSendingHeaders()
{
	static int sendingHeadersCallCount = 0;
	sendingHeadersCallCount++;
	qDebug() << "HttpConnection::transitionToSendingHeaders() - call #" << sendingHeadersCallCount << "- current state:" << _state;
	
	if (_state == Pillow::HttpConnection::SendingHeaders) {
		qDebug() << "  Already in SendingHeaders state, returning early";
		return;
	}
	_state = Pillow::HttpConnection::SendingHeaders;

	// Prepare and null terminate the request fields.

	if (_requestHeaders.size() != _requestHeadersRef.size())
		setupRequestHeaders();

	char* data = _requestBuffer.data();

	if (_parser.query_string_len == 0)
	{
		// The request uri has no query string, we can use the data straight because it means that inserting null after the path
		// will not destroy the uri (which does not include the fragment).
		setFromRawDataAndNullterm(_requestUri, data, _parser.request_uri_start, _parser.request_uri_len);
	}
	else
	{
		// Create deep copy for requestUri; it would otherwise get a null inserted after the path.
		_requestUri = QByteArray(data + _parser.request_uri_start, _parser.request_uri_len);
	}
	setFromRawDataAndNullterm(_requestMethod, data, _parser.request_method_start, _parser.request_method_len);
	setFromRawDataAndNullterm(_requestFragment, data, _parser.fragment_start, _parser.fragment_len);
	setFromRawDataAndNullterm(_requestPath, data, _parser.request_path_start, _parser.request_path_len);
	setFromRawDataAndNullterm(_requestQueryString, data, _parser.query_string_start, _parser.query_string_len);
	setFromRawDataAndNullterm(_requestHttpVersion, data, _parser.http_version_start, _parser.http_version_len);

	if (!_requestUriDecoded.isEmpty()) _requestUriDecoded = QString();
	if (!_requestFragmentDecoded.isEmpty())_requestFragmentDecoded = QString();
	if (!_requestPathDecoded.isEmpty())_requestPathDecoded = QString();
	if (!_requestQueryStringDecoded.isEmpty())_requestQueryStringDecoded = QString();

	_requestHttp11 = _requestHttpVersion == httpSlash11Token;

	setFromRawData(_requestContent, _requestBuffer.constData(), static_cast<int>(_parser.body_start), _requestContentLength);

	// Reset our known information about the response.
	_responseContentLength = -1;   // The response content-length is initially unknown.
	_responseContentBytesSent = 0; // No content bytes transfered yet.
	_responseConnectionKeepAlive = true;
	_responseChunkedTransferEncoding = false;
	qDebug() << "  About to emit requestReady signal - connection ptr:" << q_ptr;
	emit q_ptr->requestReady(q_ptr);
	qDebug() << "  Finished emitting requestReady signal - state after signal:" << _state;
}

inline void Pillow::HttpConnectionPrivate::transitionToSendingContent()
{
	if (_state == Pillow::HttpConnection::SendingContent) return;
	_state = Pillow::HttpConnection::SendingContent;

	if (_responseHeadersBuffer.capacity() > 4096)
		_responseHeadersBuffer.clear();
	else
		_responseHeadersBuffer.resize(0);

	if (_responseContentLength == 0 || _requestMethod == headToken) {
		qDebug() << "HttpConnection::transitionToSendingContent() - calling transitionToCompleted() because responseContentLength == 0 or HEAD request - responseContentLength:" << _responseContentLength << "requestMethod:" << _requestMethod;
		qDebug() << "  CALLER: transitionToSendingContent() -> transitionToCompleted()";
		transitionToCompleted();
	}

	if (_responseContentLength < 0 && !_responseChunkedTransferEncoding)
	{
		// The response content length needs to be specified or chunked transfer encoding used to support keep-alive. Forcefully disable it.
		_responseConnectionKeepAlive = false;
	}
}

inline void Pillow::HttpConnectionPrivate::transitionToCompleted()
{
	// Guard against redundant calls - check state first before any logging or processing
	if (_state == Pillow::HttpConnection::Completed) {
		qDebug() << "HttpConnection::transitionToCompleted() - Already in Completed state, ignoring redundant call";
		return;
	}
	if (_state == Pillow::HttpConnection::Closed) {
		qWarning() << "HttpConnection::transitionToCompleted called while the request is in the closed state, ignoring call";
		return;
	}
	if (_state == Pillow::HttpConnection::Flushing) {
		qDebug() << "HttpConnection::transitionToCompleted called while in Flushing state, ignoring call";
		return;
	}
	
	// Add detailed logging to track call patterns and detect infinite loops
	static int callCount = 0;
	callCount++;
	qDebug() << "HttpConnection::transitionToCompleted() - call #" << callCount << "- current state:" << _state << "- thread:" << QThread::currentThread();
	
	// Log the calling method by examining the stack trace info available
	QObject* sender = q_ptr->sender();
	if (sender) {
		qDebug() << "  Called from signal, sender:" << sender << "sender type:" << sender->metaObject()->className();
	} else {
		qDebug() << "  Called directly (not from signal)";
	}
	
	// Log additional context about the current request state
	qDebug() << "  Response info: contentLength=" << _responseContentLength << "bytesSent=" << _responseContentBytesSent << "keepAlive=" << _responseConnectionKeepAlive;
	qDebug() << "  Request buffer size:" << _requestBuffer.size() << "parser body_start:" << _parser.body_start << "contentLength:" << _requestContentLength;
	
	// Transition to Completed state
	qDebug() << "  Step 1: Transitioning state to Completed";
	_state = Pillow::HttpConnection::Completed;
	
	// Emit the requestCompleted signal - this could trigger handlers that call back into this connection
	qDebug() << "  Step 2: About to emit requestCompleted signal - connection ptr:" << q_ptr;
	emit q_ptr->requestCompleted(q_ptr);
	qDebug() << "  Step 2: Finished emitting requestCompleted signal - state after signal:" << _state;

	// Clean up request data
	qDebug() << "  Step 3: Cleaning up request buffer and data";
	int remainingBytes = _requestBuffer.size() - int(_parser.body_start) - _requestContentLength;
	qDebug() << "    remainingBytes calculation:" << remainingBytes << "= bufferSize(" << _requestBuffer.size() << ") - bodyStart(" << int(_parser.body_start) << ") - contentLength(" << _requestContentLength << ")";
	
	if (remainingBytes > 0) {
		qDebug() << "    Preserving" << remainingBytes << "remaining bytes in buffer";
		_requestBuffer = _requestBuffer.right(remainingBytes);
	} else if (_requestBuffer.capacity() <= Pillow::HttpConnection::MaximumRequestHeaderLength) {
		qDebug() << "    Resizing buffer to 0 (capacity=" << _requestBuffer.capacity() << ")";
		_requestBuffer.resize(0);
	} else {
		qDebug() << "    Clearing buffer (capacity=" << _requestBuffer.capacity() << ")";
		_requestBuffer.clear();
	}

	qDebug() << "  Step 4: Clearing request headers and params";
	_requestHeadersRef.clear();

	if (_requestParams.capacity() > 16) _requestParams.clear();
	else while(!_requestParams.isEmpty()) _requestParams.pop_back();

	if (_requestContent.size() > 0)	_requestContent.resize(0);

	// Handle connection keep-alive or close
	qDebug() << "  Step 5: Handling connection keep-alive decision - keepAlive:" << _responseConnectionKeepAlive;
	if (_responseConnectionKeepAlive)
	{
		qDebug() << "    Step 5a: Keep-alive path - flushing output";
		flush(); // Done writing for this request, make sure the data is pushed right away to the client.
		
		qDebug() << "    Step 5b: Transitioning to ReceivingHeaders for next request";
		transitionToReceivingHeaders();
		
		qDebug() << "    Step 5c: About to call processInput() for keep-alive - buffer size:" << _requestBuffer.size();
		qDebug() << "      CALLER: transitionToCompleted() -> processInput() (keep-alive path)";
		// This processInput() call might be causing the loop if there's data that triggers another completed transition
		processInput();
		qDebug() << "    Step 5c: Finished processInput() call for keep-alive - final state:" << _state;
	}
	else
	{
		qDebug() << "    Step 5d: Close path - transitioning to flushing";
		transitionToFlushing();
		qDebug() << "    Step 5d: Finished transitioning to flushing";
	}
	
	qDebug() << "HttpConnection::transitionToCompleted() - call #" << callCount << "finished - final state:" << _state;
}

inline void Pillow::HttpConnectionPrivate::flush()
{
	if (_outputDevice != 0 && _outputDevice->bytesToWrite() > 0)
	{
		if (qobject_cast<QTcpSocket*>(_outputDevice))
			static_cast<QTcpSocket*>(_outputDevice)->flush();
		else if (qobject_cast<QLocalSocket*>(_outputDevice))
			static_cast<QLocalSocket*>(_outputDevice)->flush();
	}
}

inline void Pillow::HttpConnectionPrivate::drain()
{
	if (_state != Pillow::HttpConnection::Flushing) return;
	flush();
	if (_outputDevice != 0 && _outputDevice->bytesToWrite() == 0) transitionToClosed();
}

inline void Pillow::HttpConnectionPrivate::transitionToFlushing()
{
	// This is a transient state that is meant to fully drain the write buffers and
	// wait for all the data to make it to the kernel before closing the connection.
	if (_state == Pillow::HttpConnection::Flushing) return;
	_state = Pillow::HttpConnection::Flushing;

	drain(); // Will transition to closed also if there was no data at all to flush.
	if (_state == Pillow::HttpConnection::Flushing) // A first flush was not enough. Schedule more flushes.
		QObject::connect(_outputDevice, SIGNAL(bytesWritten(qint64)), q_ptr, SLOT(drain()));
}

inline void Pillow::HttpConnectionPrivate::transitionToClosed()
{
	if (_state == Pillow::HttpConnection::Closed) return;
	_state = Pillow::HttpConnection::Closed;

	if (_inputDevice && _inputDevice->isOpen()) _inputDevice->close();
	if (_outputDevice && (_inputDevice != _outputDevice) && _outputDevice->isOpen()) _outputDevice->close();
	emit q_ptr->closed(q_ptr);

	QObject::disconnect(_inputDevice, 0, q_ptr, 0);
	if (_inputDevice != _outputDevice) QObject::disconnect(_outputDevice, 0, q_ptr, 0);
	_inputDevice = 0;
	_outputDevice = 0;
}

void Pillow::HttpConnectionPrivate::writeRequestErrorResponse(int statusCode)
{
	if (_state == Pillow::HttpConnection::Closed)
	{
		qWarning() << "HttpConnection::writeRequestErrorResponse called while state is already 'Closed'";
		return;
	}

	qDebug() << "HttpConnection: request error. Sending http status" << statusCode << "and closing connection.";

	QByteArray _responseHeadersBuffer; _responseHeadersBuffer.reserve(1024);
	const char* status = HttpProtocol::StatusCodes::getStatusCodeAndMessage(statusCode);
	_responseHeadersBuffer.append("HTTP/1.0 ").append(status, qstrlen(status)).append(crLfToken);
	_responseHeadersBuffer.append("Connection: close").append(crLfToken);
	_responseHeadersBuffer.append(crLfToken); // End of headers.
	_outputDevice->write(_responseHeadersBuffer);
	transitionToFlushing();
}

inline void Pillow::HttpConnectionPrivate::parser_http_field(void *data, const char *field, size_t flen, const char *value, size_t vlen)
{
	Pillow::HttpConnectionPrivate* request = reinterpret_cast<Pillow::HttpConnectionPrivate*>(data);

	// Find the one request header that interest us, fast.
	if (flen == 14 && asciiEqualsCaseInsensitive(field, 14, "content-length", 14))
		request->_requestContentLengthHeaderIndex = request->_requestHeadersRef.size();

	const char* begin = request->_requestBuffer.constData();
	request->_requestHeadersRef.append(HttpHeaderRef(field - begin, static_cast<int>(flen), value - begin, static_cast<int>(vlen)));
}

inline void Pillow::HttpConnectionPrivate::writeResponse(int statusCode, const HttpHeaderCollection &headers, const QByteArray &content)
{
	if (_state != Pillow::HttpConnection::SendingHeaders)
	{
		qWarning() << "HttpConnection::writeResponse called while state is not 'SendingHeaders', not proceeding with sending headers.";
		return;
	}

	// Calculate the Content-Length header so it can be set in WriteHeaders, unless it is already present.
	_responseContentLength = content.size();
	writeHeaders(statusCode, headers);
	if (!content.isEmpty() && _requestMethod != headToken) writeContent(content);
}

inline void Pillow::HttpConnectionPrivate::writeHeaders(int statusCode, const HttpHeaderCollection &headers)
{
	if (_state != Pillow::HttpConnection::SendingHeaders)
	{
		qWarning() << "HttpConnection::writeHeaders called while state is not 'SendingHeaders', not proceeding with sending headers.";
		return;
	}

	const char* statusCodeAndMessage = HttpProtocol::StatusCodes::getStatusCodeAndMessage(statusCode);
	if (statusCodeAndMessage == NULL)
	{
		// Huh? Trying to send a bad status code...
		qWarning() << "HttpConnection::writeHeaders:" << statusCode << "is not a valid Http status code. Using 500 Internal Server Error instead.";
		statusCodeAndMessage = HttpProtocol::StatusCodes::getStatusMessage(500); // Internal server error.
	}
	_responseStatusCode = statusCode;

	if (_responseHeadersBuffer.capacity() == 0)
		_responseHeadersBuffer.reserve(1024);

	_responseHeadersBuffer.append(_requestHttpVersion).append(' ').append(statusCodeAndMessage, qstrlen(statusCodeAndMessage)).append(crLfToken);

	const HttpHeader* contentTypeHeader = 0;
	const HttpHeader* connectionHeader = 0;
	const HttpHeader* transferEncodingHeader = 0;

	// Grab headers that are important to us so we can check their values and consistency.
	for (const auto& header : headers)
	{
		if (asciiEqualsCaseInsensitive(header.first, contentLengthToken))
		{
			bool ok = false;
			_responseContentLength = header.second.toLongLong(&ok);
			if (!ok)
			{
				// Somebody trying to be a bad server? Pretend we don't know the length.
				qWarning() << "HttpConnection::writeHeaders: Invalid content-length header specified. Sending response as if content-length was unknown.";
				_responseContentLength = -1;
			}
		}
		else if (asciiEqualsCaseInsensitive(header.first, contentTypeToken))
		{
			contentTypeHeader = &header;
		}
		else if (asciiEqualsCaseInsensitive(header.first, connectionToken))
		{
			connectionHeader = &header;
		}
		else if (asciiEqualsCaseInsensitive(header.first, transferEncodingToken))
		{
			transferEncodingHeader = &header;
		}
		else
		{
			// Not a special header for us. Write it out to the buffer.
			appendHttpHeader(_responseHeadersBuffer, header);
		}
	}

	if (transferEncodingHeader && asciiEqualsCaseInsensitive(transferEncodingHeader->second, chunkedToken))
	{
		if (_requestHttp11)
		{
			if (_responseContentLength == -1)
				_responseChunkedTransferEncoding = true;
			else
			{
				qWarning() << "HttpConnection::writeHeaders: Using chunked transfer encoding with a known content-lenght does not make sense. Not using chunked transfer encoding.";
				transferEncodingHeader = 0; // Suppress header
			}
		}
		else
		{
			// Chunked transfer encoding is not supported on Http below 1.1.
			transferEncodingHeader = 0;
		}
	}

	// Negotiate keep-alive between client and server.
	bool clientWantsKeepAlive;

	if (_requestHttp11)
	{
		// Keep-Alive by default, unless "close" is specified.
		clientWantsKeepAlive = !asciiEqualsCaseInsensitive(_requestHeaders.getFieldValue(connectionToken), closeToken);
	}
	else
	{
		// Close by default, unless "keep-alive" is specified.
		clientWantsKeepAlive = asciiEqualsCaseInsensitive(_requestHeaders.getFieldValue(connectionToken), keepAliveToken);
	}

	if (clientWantsKeepAlive)
	{
		// To be able to keep the connection alive, the response length needs to be known, or chunked encoding be used.
		bool serverWantsKeepAlive = _responseContentLength >= 0 || _responseChunkedTransferEncoding;

		if (serverWantsKeepAlive && connectionHeader)
		{
			// Server is Keep-Alive by default, unless "close" is specified.
			serverWantsKeepAlive = !asciiEqualsCaseInsensitive(connectionHeader->second, closeToken);
		}

		_responseConnectionKeepAlive = serverWantsKeepAlive;
	}
	else
		_responseConnectionKeepAlive = false;

	if (_outputDevice != 0 && _outputDevice->parent() != nullptr) {
		QTcpServer* server = qobject_cast<QTcpServer*>(_outputDevice->parent());

		if (server != nullptr && !server->isListening()) {
			// The server has stopped listening for new connections, so disable keep-alive
			_responseConnectionKeepAlive = false;
		}
	}

	// Automatically add essential headers.
	if (_responseContentLength != -1) { _responseHeadersBuffer.append(contentLengthOutToken); appendNumber<int, 10>(_responseHeadersBuffer, _responseContentLength); _responseHeadersBuffer.append(crLfToken); }
	if (contentTypeHeader) { appendHttpHeader(_responseHeadersBuffer, *contentTypeHeader); } else if (_responseContentLength > 0) { _responseHeadersBuffer.append(contentTypeTextPlainTokenHeaderToken); }
	if (!_requestHttp11 || !_responseConnectionKeepAlive) _responseHeadersBuffer.append(_responseConnectionKeepAlive ? connectionKeepAliveHeaderToken : connectionCloseHeaderToken);
	if (transferEncodingHeader) { appendHttpHeader(_responseHeadersBuffer, *transferEncodingHeader); }
	_responseHeadersBuffer.append(crLfToken); // End of headers.
	_outputDevice->write(_responseHeadersBuffer);
	transitionToSendingContent();
}

inline void Pillow::HttpConnectionPrivate::writeContent(const QByteArray &content)
{
	if (_state != Pillow::HttpConnection::SendingContent)
	{
		qWarning() << "HttpConnection::writeContent called while state is not 'SendingContent'. Not proceeding with sending content of size" << content.size() << "bytes.";
		return;
	}
	else if (_responseContentLength > 0 && _responseContentBytesSent >= _responseContentLength)
	{
		qWarning() << "HttpConnection::writeContent called but all content has already been sent (" << _responseContentBytesSent << "of" << _responseContentLength << "bytes). Ignoring.";
		return;
	}
	else if (_responseContentLength == 0)
	{
		qWarning() << "HttpConnection::writeContent called while the specified response content-length is 0. Not proceeding with sending content of size" << content.size() << "bytes.";
		return;
	}
	else if (_responseContentLength > 0 && content.size() + _responseContentBytesSent > _responseContentLength)
	{
		qWarning() << "HttpConnection::writeContent called trying to send more data (" << (content.size() + _responseContentBytesSent) << "bytes) than the specified response content-length of" << _responseContentLength << "bytes.";
		return;
	}

	if (content.size() > 0 && _requestMethod != headToken)
	{
		_responseContentBytesSent += content.size();
		if (_responseChunkedTransferEncoding)
		{
			QByteArray buffer; appendNumber<int, 16>(buffer, content.size()); buffer.append("\r\n", 2);
			_outputDevice->write(buffer);
		}
		_outputDevice->write(content);

		if (_responseChunkedTransferEncoding)
			_outputDevice->write("\r\n", 2);

		if (_responseContentBytesSent == _responseContentLength && _state == Pillow::HttpConnection::SendingContent) {
			qDebug() << "HttpConnection::writeContent() - calling transitionToCompleted() because all content sent - responseContentBytesSent:" << _responseContentBytesSent << "responseContentLength:" << _responseContentLength;
			qDebug() << "  CALLER: writeContent() -> transitionToCompleted()";
			transitionToCompleted();
		}
	}
}

inline void Pillow::HttpConnectionPrivate::endContent()
{
	if (_state != Pillow::HttpConnection::SendingContent)
	{
		qWarning() << "HttpConnection::endContent called while state is not 'SendingContent'. Not proceeding.";
		return;
	}

	if (_responseContentLength >= 0)
	{
		qWarning() << "HttpConnection::endContent called while the response content-length is specified. Call the close() method to forcibly end the connection without sending enough data.";
		return;
	}

	if (_responseChunkedTransferEncoding)
		_outputDevice->write("0\r\n\r\n", 5);
	else
		_responseConnectionKeepAlive = false;

	qDebug() << "HttpConnection::endContent() - calling transitionToCompleted() - responseChunkedTransferEncoding:" << _responseChunkedTransferEncoding << "responseConnectionKeepAlive:" << _responseConnectionKeepAlive;
	qDebug() << "  CALLER: endContent() -> transitionToCompleted()";
	transitionToCompleted();
}

inline void Pillow::HttpConnectionPrivate::close()
{
	transitionToClosed();
}

//
// HttpConnection
//

Pillow::HttpConnection::HttpConnection(QObject* parent /*= 0*/)
	: QObject(parent), d_ptr(new Pillow::HttpConnectionPrivate(this))
{
}

Pillow::HttpConnection::~HttpConnection()
{
	delete d_ptr;
}

void Pillow::HttpConnection::initialize(QIODevice* inputDevice, QIODevice* outputDevice)
{
	if (inputDevice != d_ptr->_inputDevice)
	{
		d_ptr->_inputDevice = inputDevice;

		connect(d_ptr->_inputDevice, SIGNAL(readyRead()), this, SLOT(processInput()));

		if (qobject_cast<QAbstractSocket*>(d_ptr->_inputDevice))
			connect(d_ptr->_inputDevice, SIGNAL(disconnected()), this, SLOT(close()));
		else if (qobject_cast<QLocalSocket*>(d_ptr->_inputDevice))
		{
			connect(d_ptr->_inputDevice, SIGNAL(disconnected()), this, SLOT(close()));
			connect(d_ptr->_inputDevice, SIGNAL(aboutToClose()), this, SLOT(close()));
			connect(d_ptr->_inputDevice, SIGNAL(readChannelFinished()), this, SLOT(close()));

		}
		else
			connect(d_ptr->_inputDevice, SIGNAL(aboutToClose()), this, SLOT(close()));
	}

	d_ptr->_outputDevice = outputDevice;
	d_ptr->initialize();
}

Pillow::HttpConnection::State Pillow::HttpConnection::state() const
{
	return d_ptr->_state;
}

QIODevice *Pillow::HttpConnection::inputDevice() const
{
	return d_ptr->_inputDevice;
}

QIODevice *Pillow::HttpConnection::outputDevice() const
{
	return d_ptr->_outputDevice;
}

void Pillow::HttpConnection::processInput()
{
	d_ptr->processInput();
}

void Pillow::HttpConnection::flush()
{
	d_ptr->flush();
}

void Pillow::HttpConnection::drain()
{
	d_ptr->drain();
}

void Pillow::HttpConnection::writeResponse(int statusCode, const HttpHeaderCollection& headers, const QByteArray& content)
{
	d_ptr->writeResponse(statusCode, headers, content);
}

void Pillow::HttpConnection::writeResponseString(int statusCode, const HttpHeaderCollection& headers, const QString& content)
{
	d_ptr->	writeResponse(statusCode, headers, content.toUtf8());
}

void Pillow::HttpConnection::writeHeaders(int statusCode, const HttpHeaderCollection& headers)
{
	d_ptr->writeHeaders(statusCode, headers);
}

void Pillow::HttpConnection::writeContent(const QByteArray& content)
{
	d_ptr->writeContent(content);
}

void Pillow::HttpConnection::endContent()
{
	d_ptr->endContent();
}

void Pillow::HttpConnection::close()
{
	d_ptr->close();
}

int Pillow::HttpConnection::responseStatusCode() const
{
	return d_ptr->_responseStatusCode;
}

qint64 Pillow::HttpConnection::responseContentLength() const
{
	return d_ptr->_responseContentLength;
}

const QByteArray & Pillow::HttpConnection::requestHeaderValue(const QByteArray &field)
{
	return d_ptr->_requestHeaders.getFieldValue(field);
}

const Pillow::HttpParamCollection& Pillow::HttpConnection::requestParams()
{
	if (d_ptr->_requestParams.isEmpty() && !d_ptr->_requestQueryString.isEmpty())
	{
		// The params have not yet been initialized. Parse them.
		const char paramDelimiter = '&', keyValueDelimiter = '=';
		for (const char* c = d_ptr->_requestQueryString.constBegin(), *cE = d_ptr->_requestQueryString.constEnd(); c < cE;)
		{
			const char *paramEnd, *keyEnd;
			for (paramEnd = c; paramEnd < cE; ++paramEnd) if (*paramEnd == paramDelimiter) break; // Find the param delimiter, or the end of string.
			for (keyEnd = c; keyEnd < paramEnd; ++keyEnd) if (*keyEnd == keyValueDelimiter) break; // Find the key value delimiter, or the end of the param.

			if (keyEnd < paramEnd)
			{
				// Key-value pair.
				d_ptr->_requestParams << HttpParam(percentDecode(c, keyEnd - c), percentDecode(keyEnd + 1, paramEnd - (keyEnd + 1)));
			}
			else
			{
				// Key without value.
				d_ptr->_requestParams << HttpParam(percentDecode(c, paramEnd - c), QString());
			}
			c = paramEnd + 1;
		}
	}
	return d_ptr->_requestParams;
}

QString Pillow::HttpConnection::requestParamValue(const QString &name)
{
	requestParams();
	for (int i = 0, iE = d_ptr->_requestParams.size(); i < iE; ++i)
	{
		const HttpParam& param = d_ptr->_requestParams.at(i);
		if (param.first.compare(name, Qt::CaseInsensitive) == 0)
			return param.second;
	}
	return QString();
}

void Pillow::HttpConnection::setRequestParam(const QString &name, const QString &value)
{
	requestParams();
	for (int i = 0, iE = d_ptr->_requestParams.size(); i < iE; ++i)
	{
		const HttpParam& param = d_ptr->_requestParams.at(i);
		if (param.first.compare(name, Qt::CaseInsensitive) == 0)
		{
			d_ptr->_requestParams[i] = HttpParam(name, value);
			return;
		}
	}
	d_ptr->_requestParams << HttpParam(name, value);
}

QHostAddress Pillow::HttpConnection::remoteAddress() const
{
	return qobject_cast<QAbstractSocket*>(d_ptr->_inputDevice) ? static_cast<QAbstractSocket*>(d_ptr->_inputDevice)->peerAddress() : QHostAddress();
}

const QByteArray &Pillow::HttpConnection::requestMethod() const
{
	return d_ptr->_requestMethod;
}

FORWARD_PERCENT_DECODABLE(requestUri)
FORWARD_PERCENT_DECODABLE(requestFragment)
FORWARD_PERCENT_DECODABLE(requestPath)
FORWARD_PERCENT_DECODABLE(requestQueryString)

const QByteArray &Pillow::HttpConnection::requestHttpVersion() const
{
	return d_ptr->_requestHttpVersion;
}

const QByteArray &Pillow::HttpConnection::requestContent() const
{
	return d_ptr->_requestContent;
}

const Pillow::HttpHeaderCollection &Pillow::HttpConnection::requestHeaders() const
{
	return d_ptr->_requestHeaders;
}
