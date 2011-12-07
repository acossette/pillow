include(../config.pri)

TEMPLATE = lib
DESTDIR = ../lib

QT       += core network script
QT       -= gui

CONFIG   += static

DEPENDPATH = .
INCLUDEPATH = .

SOURCES += \
	parser/parser.c \
	HttpServer.cpp \
	HttpHandler.cpp \
	HttpHandlerQtScript.cpp \
	HttpHelpers.cpp \
	HttpsServer.cpp \
	HttpHandlerSimpleRouter.cpp \
	HttpConnection.cpp \
	HttpHandlerProxy.cpp

HEADERS += \
	parser/parser.h \
	HttpServer.h \
	HttpHandler.h \
	HttpHandlerQtScript.h \
	HttpHelpers.h \
	HttpsServer.h \
	HttpHandlerSimpleRouter.h \
	HttpConnection.h \
	HttpHandlerProxy.h \
    ByteArrayHelpers.h

OTHER_FILES +=
