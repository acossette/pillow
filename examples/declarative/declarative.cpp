#include <QtCore/QRegExp>
#include <QtCore/QStringList>
#include <QtGui/QApplication>
#include <QtDeclarative/qdeclarative.h>
#include <QtDeclarative/QDeclarativeEngine>
#include <QtDeclarative/QDeclarativeView>
#include <QtScript/QScriptValueIterator>

#include "HttpServer.h"
#include "HttpConnection.h"
#include "HttpHandler.h"

namespace Pillow
{
	class BaseObject : public QObject
	{
		Q_OBJECT
		Q_PROPERTY(QDeclarativeListProperty<QObject> data READ data DESIGNABLE false)
		Q_CLASSINFO("DefaultProperty", "data")

	public:
		inline BaseObject(QObject *parent = 0) : QObject(parent) {}

	private:
		QDeclarativeListProperty<QObject> data()
		{
			return QDeclarativeListProperty<QObject>(this, 0, BaseObject::data_append, BaseObject::data_count, BaseObject::data_at, BaseObject::data_clear );
		}

		static void data_append(QDeclarativeListProperty<QObject> *property, QObject *obj)
		{
			if (!obj) return;
			obj->setParent(property->object);
		}

		static int data_count(QDeclarativeListProperty<QObject> *property)
		{
			return property->object->children().size();
		}

		static QObject *data_at(QDeclarativeListProperty<QObject> *property, int index)
		{
			const QObjectList& children = property->object->children();
			if (index < 0 || index > children.size()) return 0;
			return children.at(index);
		}

		static void data_clear(QDeclarativeListProperty<QObject> *property)
		{
			foreach (QObject* obj, property->object->children())
				obj->setParent(0);
		}
	};

	class DeclarativeHttpServer : public BaseObject, public QDeclarativeParserStatus
	{
		Q_OBJECT
		Q_INTERFACES(QDeclarativeParserStatus)
		Q_PROPERTY(QString address READ address WRITE setAddress NOTIFY addressChanged)
		Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
		Q_PROPERTY(bool listening READ isListening NOTIFY listeningChanged)

	public:
		DeclarativeHttpServer() : _server(NULL), _port(0), _componentComplete(false)
		{
			_server = new Pillow::HttpServer(this);
			connect(_server, SIGNAL(requestReady(Pillow::HttpConnection*)), this, SIGNAL(request(Pillow::HttpConnection*)));
		}

		~DeclarativeHttpServer()
		{
			qDebug() << "Server is going away.";
		}

		const QString& address() const { return _address; }
		void setAddress(const QString& address)
		{
			if (address == _address) return;
			_address = address;
			rebind();
			emit addressChanged();
		}

		int port() const { return _port; }
		void setPort(int port)
		{
			if (port == _port) return;
			_port = port;
			rebind();
			emit portChanged();
		}

		bool isListening() const { return _server->isListening(); }

		void classBegin() {}
		void componentComplete()
		{
			_componentComplete = true;
			rebind();
		}

		Q_INVOKABLE Pillow::HttpHeaderCollection emptyHeaders() const { return Pillow::HttpHeaderCollection(); }

		Q_INVOKABLE Pillow::HttpHeaderCollection makeHeaders(const QScriptValue& object) const
		{
			Pillow::HttpHeaderCollection headers;
			for (QScriptValueIterator it(object); it.hasNext();)
			{
				it.next();
				headers << Pillow::HttpHeader(it.name().toLatin1(), it.value().toString().toLatin1());
			}
			return headers;
		}

	signals:
		void addressChanged();
		void portChanged();
		void listeningChanged();
		void request(Pillow::HttpConnection* request);

	private:
		void rebind()
		{
			if (!_componentComplete) return;
			if (_server->isListening()) _server->close();
			QHostAddress addr = _address.isEmpty() ? QHostAddress(QHostAddress::Any) : QHostAddress(_address);
			_server->listen(addr, _port);
			emit listeningChanged();
		}

	private:
		Pillow::HttpServer* _server;
		QString _address;
		int _port;
		bool _componentComplete;
	};

	class RouteMatcher : public BaseObject
	{
		Q_OBJECT
		Q_PROPERTY(QString path READ path WRITE setPath NOTIFY changed)
		Q_PROPERTY(QStringList matchParamNames READ matchParamNames NOTIFY changed)

	public:
		RouteMatcher(QObject* parent = 0) : BaseObject(parent) {}

		const QString& path() const { return _path; }
		const QStringList& matchParamNames() const { return _matchParamNames; }

	public slots:
		void setPath(const QString& path)
		{
			if (_path == path) return;
			_path = path;
			rebuild();
			emit changed();
		}

		bool match(Pillow::HttpConnection* request)
		{
			if (request == NULL) return false;

			if (_matchRegExp.indexIn(request->requestPathDecoded()) != -1)
				return true;

			return false;
		}

	signals:
		void changed();

	private:
		void rebuild()
		{
			QString path = _path;
			QStringList paramNames;

			QRegExp paramRegex(":(\\w+)"); QString paramReplacement("([\\w_-]+)");
			int pos = 0; while (pos >= 0)
			{
				if ((pos = paramRegex.indexIn(path, pos)) >= 0)
				{
					paramNames << paramRegex.cap(1);
					pos += paramRegex.matchedLength();
				}
			}
			path.replace(paramRegex, paramReplacement);

			QRegExp splatRegex("\\*(\\w+)"); QString splatReplacement("(.*)");
			pos = 0; while (pos >= 0)
			{
				if ((pos = splatRegex.indexIn(path, pos)) >= 0)
				{
					paramNames << splatRegex.cap(1);
					pos += splatRegex.matchedLength();
				}
			}
			path.replace(splatRegex, splatReplacement);

			_matchRegExp = QRegExp("^" + path + "$");
			_matchParamNames = paramNames;
		}

	private:
		QString _path;
		QRegExp _matchRegExp;
		QStringList _matchParamNames;
	};
}

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	qRegisterMetaType<Pillow::HttpHeaderCollection>("Pillow::HttpHeaderCollection");
	qmlRegisterType<Pillow::DeclarativeHttpServer>("Pillow", 1, 0, "HttpServer");
	qmlRegisterType<Pillow::RouteMatcher>("Pillow", 1, 0, "RouteMatcher");
	qmlRegisterType<Pillow::HttpConnection>("Pillow", 1, 0, "HttpConnection");
	qmlRegisterType<Pillow::HttpHandler>();
	qmlRegisterType<Pillow::HttpHandlerStack>("Pillow", 1, 0, "HttpHandlerStack");
	qmlRegisterType<Pillow::HttpHandlerFile>("Pillow", 1, 0, "HttpHandlerFile");
	qmlRegisterType<Pillow::HttpHandlerFixed>("Pillow", 1, 0, "HttpHandlerFixed");
	qmlRegisterType<Pillow::HttpHandlerLog>("Pillow", 1, 0, "HttpHandlerLog");
	qmlRegisterType<Pillow::HttpHandler404>("Pillow", 1, 0, "HttpHandler404");

	QDeclarativeView view;
	view.setResizeMode(QDeclarativeView::SizeRootObjectToView);
	view.setSource(QString("declarative.qml"));
	view.resize(400, 400);
	view.show();

	return a.exec();
}

#include "declarative.moc"
