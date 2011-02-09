include (../config.pri)

INCLUDEPATH += ../../pillowcore
DEPENDPATH += ../../pillowcore

LIBS += -L../../lib -l$${PILLOWCORE_LIB_NAME}
unix: POST_TARGETDEPS += ../../lib/lib$${PILLOWCORE_LIB_NAME}.a
win32: POST_TARGETDEPS += ../../lib/$${PILLOWCORE_LIB_NAME}.lib
