#include "HttpHandlerProxy.h"
#include "HttpConnection.h"
#include <QtCore/QBuffer>
#include <QtNetwork/QNetworkReply>

//
// Pillow::HttpHandlerProxy
//

Pillow::HttpHandlerProxy::HttpHandlerProxy(QObject *parent) 
	: Pillow::HttpHandler(parent)
{
	networkAccessManager = new ElasticNetworkAccessManager(this);
}

Pillow::HttpHandlerProxy::HttpHandlerProxy(const QUrl& proxiedUrl, QObject *parent) 
	: Pillow::HttpHandler(parent), _proxiedUrl(proxiedUrl)
{
	networkAccessManager = new ElasticNetworkAccessManager(this);
}

void Pillow::HttpHandlerProxy::setProxiedUrl(const QUrl &proxiedUrl)
{
	if (_proxiedUrl == proxiedUrl) return;
	_proxiedUrl = proxiedUrl;
}

bool Pillow::HttpHandlerProxy::handleRequest(Pillow::HttpConnection *request)
{
	if (_proxiedUrl.isEmpty()) return false;
	
	QUrl targetUrl = _proxiedUrl;
	targetUrl.setEncodedPath(request->requestPath());
	if (!request->requestQueryString().isEmpty()) targetUrl.setEncodedQuery(request->requestQueryString());
	if (!request->requestFragment().isEmpty()) targetUrl.setEncodedFragment(request->requestFragment());
		
	QNetworkRequest proxiedRequest(targetUrl);
	foreach (const Pillow::HttpHeader& header, request->requestHeaders())
		proxiedRequest.setRawHeader(header.first, header.second);
	
	QBuffer* requestContentBuffer = NULL;
	if (request->requestContent().size() > 0)
	{
		requestContentBuffer = new QBuffer(&(const_cast<QByteArray&>(request->requestContent())));
		requestContentBuffer->open(QIODevice::ReadOnly);
	}
	
	QNetworkReply* proxiedReply = networkAccessManager->sendCustomRequest(proxiedRequest, request->requestMethod(), requestContentBuffer);
	
	if (requestContentBuffer) requestContentBuffer->setParent(proxiedReply);	
	
	new Pillow::HttpHandlerProxyPipe(request, proxiedReply);
	
	return true;
}

//
// Pillow::HttpHandlerProxyPipe
//

Pillow::HttpHandlerProxyPipe::HttpHandlerProxyPipe(Pillow::HttpConnection *request, QNetworkReply *proxiedRequest)
	: _request(request), _proxiedRequest(proxiedRequest), _headersSent(false), _broken(false)
{
	// Make sure we stop piping data if the client request finishes early or the proxied request sends too much.
	connect(request, SIGNAL(requestCompleted(Pillow::HttpConnection*)), this, SLOT(teardown()));
	connect(request, SIGNAL(closed(Pillow::HttpConnection*)), this, SLOT(teardown()));
	connect(request, SIGNAL(destroyed()), this, SLOT(teardown()));
	connect(proxiedRequest, SIGNAL(readyRead()), this, SLOT(proxiedRequest_readyRead()));
	connect(proxiedRequest, SIGNAL(finished()), this, SLOT(proxiedRequest_finished()));
	connect(proxiedRequest, SIGNAL(destroyed()), this, SLOT(teardown()));
}

Pillow::HttpHandlerProxyPipe::~HttpHandlerProxyPipe()
{
}

void Pillow::HttpHandlerProxyPipe::teardown()
{
	_broken = true;
	
	if (_request)
	{
		disconnect(_request, NULL, this, NULL);
		_request = NULL;
	}
	
	if (_proxiedRequest)
	{
		disconnect(_proxiedRequest, NULL, this, NULL);
		if (qobject_cast<QNetworkReply*>(_proxiedRequest))
		{
			_proxiedRequest->abort();
			_proxiedRequest->deleteLater();
		}
		_proxiedRequest = NULL;
	}
	
	deleteLater();
}

void Pillow::HttpHandlerProxyPipe::sendHeaders()
{
	if (_headersSent || _broken) return;
	_headersSent = true;	

	// Headers have not been sent yet. Do so now.
	int statusCode = _proxiedRequest->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	QList<Pillow::HttpHeader> headerList = _proxiedRequest->rawHeaderPairs();
	Pillow::HttpHeaderCollection headers; headers.reserve(headerList.size());
	foreach (const Pillow::HttpHeader& header, headerList)
		headers << header;
	_request->writeHeaders(statusCode, headers);
}

void Pillow::HttpHandlerProxyPipe::proxiedRequest_readyRead()
{	
	sendHeaders();
	if (!_broken) _request->writeContent(_proxiedRequest->readAll());
}

void Pillow::HttpHandlerProxyPipe::proxiedRequest_finished()
{	
	if (_proxiedRequest->error() == QNetworkReply::NoError)
	{
		sendHeaders(); // Make sure headers have been sent; can cause the pipe to tear down.
		
		if (!_broken && _request->state() == Pillow::HttpConnection::SendingContent)
		{
			// The client request will still be in this state if the content-length was not specified. We must
			// close the connection to indicate the end of the content stream.
			_request->close();
		}
	}
	else
	{
		if (!_broken && _request->state() == Pillow::HttpConnection::SendingHeaders)
		{
			// Finishing before sending headers means that we have a network or transport error. Let the client know about this.
			_request->writeResponse(503);
		}
	}
}

//
// Pillow::ElasticNetworkAccessManager
//

Pillow::ElasticNetworkAccessManager::ElasticNetworkAccessManager(QObject *parent)
	: QNetworkAccessManager(parent)
{
}
