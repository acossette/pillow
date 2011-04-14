#ifndef _PILLOW_HTTPHANDLERQTSCRIPT_H_
#define _PILLOW_HTTPHANDLERQTSCRIPT_H_

#include "HttpHandler.h"
#include <QtScript/QScriptValue>
#include<QtCore/QDateTime>

class QScriptEngine;

namespace Pillow
{
	class HttpHandlerQtScript : public Pillow::HttpHandler
	{
		Q_OBJECT
		QScriptValue _scriptFunction;
	
	public:
		HttpHandlerQtScript(QScriptValue scriptFunction, QObject *parent = 0);
		HttpHandlerQtScript(QObject *parent = 0);
	
		const QScriptValue& scriptFunction() const { return _scriptFunction; }
	
	public:
		void setScriptFunction(const QScriptValue& scriptFunction);
	
		virtual bool handleRequest(Pillow::HttpConnection *request);
	};
	
	class HttpHandlerQtScriptFile : public HttpHandlerQtScript
	{
		Q_OBJECT
		QString _fileName;
		QString _functionName;
		bool _autoReload;
		QScriptValue _scriptObject;
		QDateTime _lastModified;
	
	public:
		HttpHandlerQtScriptFile(QScriptEngine* engine, const QString& fileName, const QString& functionName, bool autoReload = true, QObject* parent = 0);
	
		QScriptEngine* engine() const { return _scriptObject.engine(); }
		const QString& fileName() const { return _fileName; }
		const QString& functionName() const { return _functionName; }
		bool autoReload() const { return _autoReload; }
	
	public:
		void setFileName(const QString& fileName);
		void setFunctionName(const QString& functionName);
		void setAutoReload(bool autoReload);
	
		virtual bool handleRequest(Pillow::HttpConnection *request);
	};
}

#endif // _PILLOW_HTTPHANDLERQTSCRIPT_H_
