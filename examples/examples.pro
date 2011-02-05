include(../config.pri)
TEMPLATE = subdirs

SUBDIRS = fileserver simple qtscript
!pillow_no_ssl: SUBDIRS += simplessl
