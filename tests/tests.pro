TEMPLATE = app

QT       += core network testlib script
QT       -= gui

CONFIG   += console
CONFIG   -= app_bundle

INCLUDEPATH = . ../pillowcore
DEPENDPATH = . ../pillowcore
LIBS += -L../lib -lpillowcore

SOURCES += main.cpp \
    HttpServerTest.cpp \
    HttpRequestTest.cpp \
    HttpHandlerTest.cpp

HEADERS += \
    HttpServerTest.h \
    HttpRequestTest.h \
    HttpHandlerTest.h

RESOURCES += tests.qrc

unix: POST_TARGETDEPS += ../lib/libpillowcore.a
win32: POST_TARGETDEPS += ../lib/pillowcore.lib
