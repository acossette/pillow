#include "HttpHandler.h"
#include "HttpRequest.h"
#include "HttpHelpers.h"
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QCryptographicHash>
#include <QtCore/QElapsedTimer>
#include <QtCore/QDateTime>
#include <QtCore/QStringBuilder>
#include <QtNetwork/QTcpSocket>
using namespace Pillow;

//
// HttpHandler
//

HttpHandler::HttpHandler(QObject *parent)
	: QObject(parent)
{
}

//
// HttpHandlerStack
//

HttpHandlerStack::HttpHandlerStack(QObject *parent)
    : HttpHandler(parent)
{
}

bool HttpHandlerStack::handleRequest(Pillow::HttpRequest *request)
{
	foreach (QObject* object, children())
	{
		HttpHandler* handler = qobject_cast<HttpHandler*>(object);
		
		if (handler && handler->handleRequest(request))
			return true;
	}
	
	return false;
}

QList<HttpHandler *> HttpHandlerStack::GetHandlers() const
{
	QList<HttpHandler*> handlers;

	foreach (QObject* object, children())
	{
		HttpHandler* handler = qobject_cast<HttpHandler*>(object);
		if (handler) handlers.append(handler);
	}
	
	return handlers;
}

//
// HttpHandlerFixed
//

HttpHandlerFixed::HttpHandlerFixed(int statusCode, const QByteArray& content, QObject *parent)
    :HttpHandler(parent), _statusCode(statusCode), _content(content)
{
}

bool HttpHandlerFixed::handleRequest(Pillow::HttpRequest *request)
{
	request->requestParams();
	request->writeResponse(_statusCode, HttpHeaderCollection(), _content);
	return true;
}

//
// HttpHandler404
//

HttpHandler404::HttpHandler404(QObject *parent)
    : HttpHandler(parent)
{
}

bool HttpHandler404::handleRequest(Pillow::HttpRequest *request)
{
	request->writeResponseString(404, HttpHeaderCollection(), QString("The requested resource '%1' does not exist on this server").arg(QString(request->requestPath())));
	return true;
}

//
// HttpHandlerLog
//

HttpHandlerLog::HttpHandlerLog(QObject *parent)
	: HttpHandler(parent)
{
}

HttpHandlerLog::HttpHandlerLog(QIODevice *device, QObject *parent)
	: HttpHandler(parent), _device(device)
{	
}

HttpHandlerLog::~HttpHandlerLog()
{
	foreach (QElapsedTimer* timer, requestTimerMap)
		delete timer;
}

bool HttpHandlerLog::handleRequest(Pillow::HttpRequest *request)
{
	QElapsedTimer* timer = requestTimerMap.value(request, NULL);
	if (timer == NULL)
	{
		timer = requestTimerMap[request] = new QElapsedTimer();
		connect(request, SIGNAL(completed(Pillow::HttpRequest*)), this, SLOT(requestCompleted(Pillow::HttpRequest*)));
		connect(request, SIGNAL(destroyed(QObject*)), this, SLOT(requestDestroyed(QObject*)));
	}
	timer->start();

	return false;
}

void HttpHandlerLog::requestCompleted(Pillow::HttpRequest *request)
{
	QElapsedTimer* timer = requestTimerMap.value(request, NULL);
	if (timer)
	{
		qint64 elapsed = timer->elapsed();
		QString logEntry = QString("%1 - - [%2] \"%3 %4 %5\" %6 %7 %8")
		        .arg(request->remoteAddress().toString())
		        .arg(QDateTime::currentDateTime().toString("dd/MMM/yyyy hh:mm:ss"))
		        .arg(QString(request->requestMethod())).arg(QString(request->requestUri())).arg(QString(request->requestHttpVersion()))
		        .arg(request->responseStatusCode()).arg(request->responseContentLength())
		        .arg(elapsed / 1000.0, 3, 'f', 3);

//		QString logEntry = QString()
//				% request->remoteAddress().toString()
//				% " - - [" % QDateTime::currentDateTime().toString("dd/MMM/yyyy hh:mm:ss") % "] \""
//				% request->requestMethod() % ' ' % request->requestUri() % ' ' % request->requestHttpVersion() % "\" "
//				% QString::number(request->responseStatusCode()) % ' ' 
//				% QString::number(request->responseContentLength()) % ' '
//				% QString::number(elapsed / 1000.0, 'f', 3);

		if (_device == NULL)
			qDebug() << logEntry;
		else
		{
			logEntry.append('\n');
			_device->write(logEntry.toUtf8());
 		}
	}
}

void HttpHandlerLog::requestDestroyed(QObject *r)
{
	HttpRequest* request = static_cast<HttpRequest*>(r);
	delete requestTimerMap.value(request, NULL);
	requestTimerMap.remove(request);
}

QIODevice * HttpHandlerLog::device() const
{
	return _device;
}

void Pillow::HttpHandlerLog::setDevice(QIODevice *device)
{
	if (_device == device) return;
	_device = device;
}

//
// HttpHandlerFile
//

HttpHandlerFile::HttpHandlerFile(const QString &publicPath, QObject *parent)
    : HttpHandler(parent), _bufferSize(DefaultBufferSize)
{
	setPublicPath(publicPath);
}

void HttpHandlerFile::setPublicPath(const QString &publicPath)
{
	if (_publicPath == publicPath) return;
	_publicPath = publicPath;
	
	if (!_publicPath.isEmpty())
	{
		QFileInfo pathInfo(_publicPath);
		_publicPath = pathInfo.canonicalFilePath();
		if (!(pathInfo.exists() && pathInfo.isDir() && pathInfo.isReadable()))
		{
			qWarning() << "HttpHandlerStaticFile::SetPublicPath:" << publicPath << "does not exist or is not a readable directory.";
		}
	}
}

void HttpHandlerFile::setBufferSize(int bytes)
{
	if (_bufferSize == bytes) return;
	_bufferSize = bytes;
}

bool HttpHandlerFile::handleRequest(Pillow::HttpRequest *request)
{	
	if (_publicPath.isEmpty()) { return false; } // Just don't allow access to the root filesystem unless really configured for it.

	QString requestPath = QByteArray::fromPercentEncoding(request->requestPath());
	QString resultPath = _publicPath + requestPath;
	QFileInfo resultPathInfo(resultPath);

	if (!resultPathInfo.exists())
	{
		return false;
	}
	else if (!resultPathInfo.canonicalFilePath().startsWith(_publicPath))
	{
		return false; // Somebody tried to use some ".." or has followed symlinks that escaped out of the public path. 
	}
	else if (!resultPathInfo.isFile())
	{
		return false; // This class does not serve anything else than files... No directory listings!
	}

	QFile* file = new QFile(resultPathInfo.filePath());

	if (!file->open(QIODevice::ReadOnly))
	{
		// Could not read the file?
		request->writeResponse(403, HttpHeaderCollection(), QString("The requested resource '%1' is not accessible").arg(requestPath).toUtf8());
		delete file;
	}
	else
	{
		HttpHeaderCollection headers; headers.reserve(2);
		headers << HttpHeader("Content-Type", HttpMimeHelper::getMimeTypeForFilename(requestPath));

		if (file->size() <= bufferSize())
		{
			// The file fully fits in the supported buffer size. Read it and calculate an ETag for caching.
			QByteArray content = file->readAll();
			QCryptographicHash md5sum(QCryptographicHash::Md5); md5sum.addData(content);
			QByteArray etag = md5sum.result().toHex();
			headers << HttpHeader("ETag", etag);

			if (request->getRequestHeaderValue("If-None-Match") == etag)
				request->writeResponse(304); // The client's cached file was not modified.
			else
				request->writeResponse(200, headers, content);

			delete file;
		}
		else
		{
			// The file exceeds the buffer size and must be sent incrementally. Do send the headers right away.
			headers << HttpHeader("Content-Length", QByteArray::number(file->size()));
			request->writeHeaders(200, headers);

			HttpHandlerFileTransfer* transfer = new HttpHandlerFileTransfer(file, request, bufferSize());
			file->setParent(transfer);
			//transfer->setParent(this);
			connect(transfer, SIGNAL(finished()), transfer, SLOT(deleteLater()));
			transfer->writeNextPayload();
		}
	}
	
	return true;
}

//
// HttpHandlerFile
//

HttpHandlerFileTransfer::HttpHandlerFileTransfer(QIODevice *sourceDevice, HttpRequest *targetRequest, int bufferSize)
    : _sourceDevice(sourceDevice), _targetRequest(targetRequest), _bufferSize(bufferSize)
{
	if (bufferSize < 512)
	{
		_bufferSize = 512;
		qWarning() << "HttpHandlerFileTransfer::HttpHandlerFileTransfer: requesting a buffer size of" << bufferSize << "bytes. Correcting to" << _bufferSize << "bytes.";
	}

	connect(sourceDevice, SIGNAL(destroyed()), this, SLOT(deleteLater()));
	connect(targetRequest, SIGNAL(completed(Pillow::HttpRequest*)), this, SLOT(deleteLater()));
	connect(targetRequest, SIGNAL(closed(Pillow::HttpRequest*)), this, SLOT(deleteLater()));
	connect(targetRequest, SIGNAL(destroyed()), this, SLOT(deleteLater()));
	connect(targetRequest->outputDevice(), SIGNAL(bytesWritten(qint64)), this, SLOT(writeNextPayload()), Qt::QueuedConnection);
}

void HttpHandlerFileTransfer::writeNextPayload()
{
	if (_sourceDevice == NULL || _targetRequest == NULL || _targetRequest->outputDevice() == NULL) return;

	qint64 bytesToRead = _bufferSize - _targetRequest->outputDevice()->bytesToWrite();
	if (bytesToRead <= 0) return;
	qint64 bytesAvailable = _sourceDevice->size() - _sourceDevice->pos();
	if (bytesToRead > bytesAvailable) bytesToRead = bytesAvailable;

	if (bytesToRead > 0)
	{
		_targetRequest->writeContent(_sourceDevice->read(bytesToRead));

		if (_sourceDevice->atEnd()) 
			emit finished();
	}
}

