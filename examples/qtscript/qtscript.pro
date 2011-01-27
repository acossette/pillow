include(../examples.pri)

TEMPLATE = app

QT       += core network testlib script
QT       -= gui

CONFIG   += console
CONFIG   -= app_bundle

INCLUDEPATH = . ../../pillowcore
DEPENDPATH = . ../../pillowcore
LIBS += -L../../lib -lpillowcore

SOURCES += qtscript.cpp

OTHER_FILES += test.js

!contains(PWD, $$OUT_PWD) {
	unix: QMAKE_POST_LINK = $(COPY_FILE) "$$PWD/test.js" "$$OUT_PWD"
	win32: QMAKE_POST_LINK = xcopy /Y /D \"$$PWD/test.js\" \"$$OUT_PWD\"
}
