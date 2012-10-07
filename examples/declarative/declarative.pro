include(../examples.pri)

TEMPLATE = app

QT       += core network gui script

contains(QT_MAJOR_VERSION, 4): QT += declarative
contains(QT_MAJOR_VERSION, 5): QT += quick1


CONFIG   += console
CONFIG   -= app_bundle

INCLUDEPATH += .
DEPENDPATH += .

SOURCES += declarative.cpp

OTHER_FILES += \
	declarative.qml \
	Route.qml

!contains(PWD, $$OUT_PWD) {
	unix: QMAKE_POST_LINK = ln -sf "$$PWD/declarative.qml" "$$OUT_PWD"
	win32: QMAKE_POST_LINK = xcopy /Y /D \"$$PWD/declarative.qml\" \"$$OUT_PWD\"
}

