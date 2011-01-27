include(../examples.pri)

TEMPLATE = app

QT       += core network testlib
QT       -= gui

CONFIG   += console
CONFIG   -= app_bundle

INCLUDEPATH = . ../../pillowcore
DEPENDPATH = . ../../pillowcore
LIBS += -L../../lib -lpillowcore

SOURCES += simple.cpp
