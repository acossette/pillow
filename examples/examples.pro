include(../config.pri)
TEMPLATE = subdirs

SUBDIRS = fileserver simple qtscript declarative clientbench
!pillow_no_ssl: SUBDIRS += simplessl
