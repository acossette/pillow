include(../examples.pri)

TEMPLATE = app

QT       += core network testlib
QT       -= gui
QT       += svg # For zlib symbols on Qt5

CONFIG   += console
CONFIG   -= app_bundle

INCLUDEPATH += .
DEPENDPATH += .

SOURCES += clientbench.cpp

unix: LIBS += -lz
