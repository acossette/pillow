include(../config.pri)
TEMPLATE = subdirs

SUBDIRS = fileserver simple qtscript declarative
!pillow_no_ssl: SUBDIRS += simplessl
