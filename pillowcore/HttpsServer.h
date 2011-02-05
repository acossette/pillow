#ifndef _PILLOW_HTTPSSERVER_H_
#define _PILLOW_HTTPSSERVER_H_

#ifndef PILLOW_NO_SSL

#include "HttpServer.h"
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslKey>
#include <QtNetwork/QSslError>

namespace Pillow
{
	//
	// HttpsServer
	//

	class HttpsServer : public Pillow::HttpServer
	{
		Q_OBJECT
		QSslCertificate _certificate;
		QSslKey _privateKey;

	public slots:
		void sslSocket_encrypted();
		void sslSocket_sslErrors(const QList<QSslError>& sslErrors);

	protected:
		virtual void incomingConnection(int socketDescriptor);

	public:
		HttpsServer(QObject* parent = 0);
		HttpsServer(const QSslCertificate& certificate, const QSslKey& privateKey, const QHostAddress& serverAddress, quint16 serverPort, QObject *parent = 0);

		const QSslCertificate& certificate() const { return _certificate; }
		const QSslKey& privateKey() const { return _privateKey; }

	public slots:
		void setCertificate(const QSslCertificate& certificate);
		void setPrivateKey(const QSslKey& privateKey);
	};
}

#endif // !PILLOW_NO_SSL

#endif // _PILLOW_HTTPSSERVER_H_
