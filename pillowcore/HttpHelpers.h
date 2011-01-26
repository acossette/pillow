#ifndef _PILLOW_HTTPHELPERS_H_
#define _PILLOW_HTTPHELPERS_H_

#ifndef QSTRING_H
#include <QString>
#endif // QSTRING_H
#ifndef QBYTEARRAY_H
#include <QByteArray>
#endif // QBYTEARRAY_H
#ifndef QDATETIME_H
#include <QDateTime>
#endif // QDATETIME_H

namespace Pillow
{
	namespace HttpMimeHelper
	{
		const char* getMimeTypeForFilename(const QString& filename);
	}

	namespace HttpProtocol
	{
		namespace StatusCodes
		{
			const char* getStatusCodeAndMessage(int statusCode);
			const char* getStatusMessage(int statusCode);
		}

		namespace Dates
		{
			QByteArray GetHttpDate(const QDateTime& dateTime = QDateTime::currentDateTime());
		}
	}
}

#endif // _PILLOW_HTTPHELPERS_H_
