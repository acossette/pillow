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
    HttpRequest.cpp \
    HttpHelpers.cpp

HEADERS += \
	parser/parser.h \
    HttpServer.h \
    HttpHandler.h \
    HttpHandlerQtScript.h \
    HttpRequest.h \
    HttpHelpers.h

QMAKE_CXXFLAGS = -O1
