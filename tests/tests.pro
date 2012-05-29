include(../config.pri)
TEMPLATE = app

QT       += core network testlib script gui

CONFIG   += console precompile_header
CONFIG   -= app_bundle

PRECOMPILED_HEADER = pch.h

INCLUDEPATH = . ../pillowcore
DEPENDPATH = . ../pillowcore
LIBS += -L../lib -l$${PILLOWCORE_LIB_NAME}
unix: LIBS += -lz
POST_TARGETDEPS += ../lib/$$PILLOWCORE_LIB_FILE

SOURCES += main.cpp \
	HttpServerTest.cpp \
	HttpConnectionTest.cpp \
	HttpHandlerTest.cpp \
	HttpsServerTest.cpp \
	HttpHandlerProxyTest.cpp \
	ByteArrayHelpersTest.cpp \
	HttpClientTest.cpp

HEADERS += \
	HttpServerTest.h \
	HttpConnectionTest.h \
	HttpHandlerTest.h \
	HttpsServerTest.h \
	HttpHandlerProxyTest.h \
	Helpers.h \
	pch.h

RESOURCES += tests.qrc
