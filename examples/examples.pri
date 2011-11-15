include (../config.pri)

INCLUDEPATH += ../../pillowcore
DEPENDPATH += ../../pillowcore

LIBS += -L../../lib -l$${PILLOWCORE_LIB_NAME}
POST_TARGETDEPS += ../../lib/$$PILLOWCORE_LIB_FILE
