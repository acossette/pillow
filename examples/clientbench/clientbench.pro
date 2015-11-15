include(../examples.pri)

TEMPLATE = app

QT       += core network testlib
QT       -= gui

CONFIG   += console
CONFIG   -= app_bundle

INCLUDEPATH += .
DEPENDPATH += .

SOURCES += clientbench.cpp

