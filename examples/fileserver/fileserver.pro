include(../examples.pri)

TEMPLATE = app

QT       += core network testlib script
QT       -= gui

CONFIG   += console
CONFIG   -= app_bundle

INCLUDEPATH += .
DEPENDPATH += .

SOURCES += fileserver.cpp

