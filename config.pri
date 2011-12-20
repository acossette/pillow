PILLOWCORE_LIB_NAME = pillowcore

# Enable both debug and release build generation by default and append a "d" suffix on debug libs.
CONFIG += debug_and_release
CONFIG(debug, debug|release) {
	TARGET = $${TARGET}d
	PILLOWCORE_LIB_NAME = $${PILLOWCORE_LIB_NAME}d
}

contains(QMAKE_CC, cl) {
	PILLOWCORE_LIB_FILE = $${PILLOWCORE_LIB_NAME}.lib
} else {
	PILLOWCORE_LIB_FILE = lib$${PILLOWCORE_LIB_NAME}.a
	QMAKE_CXXFLAGS += --std=c++0x
}

# Uncomment the following to disable SSL support in Pillow.
#CONFIG += pillow_no_ssl

pillow_no_ssl {
	DEFINES += PILLOW_NO_SSL
}

