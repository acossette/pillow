include(../examples.pri)

TEMPLATE = app

QT       += core network testlib script
QT       -= gui

CONFIG   += console
CONFIG   -= app_bundle

INCLUDEPATH = . ../../pillowcore
DEPENDPATH = . ../../pillowcore
LIBS += -L../../lib -lpillowcore

SOURCES += simplessl.cpp
 
unix: QMAKE_POST_LINK = $(COPY_FILE) "$$PWD/test.key" "$$OUT_PWD/test.key" && $(COPY_FILE) "$$PWD/test.crt" "$$OUT_PWD/test.crt"
