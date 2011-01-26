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

HEADERS += \
    HttpServerTest.h \
    HttpRequestTest.h

POST_TARGETDEPS += ../lib/libpillowcore.a

unix: QMAKE_POST_LINK = $(COPY_FILE) "$$PWD/test.key" "$$OUT_PWD/test.key" && $(COPY_FILE) "$$PWD/test.crt" "$$OUT_PWD/test.crt"
