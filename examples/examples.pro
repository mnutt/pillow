include(../config.pri)
TEMPLATE = subdirs

SUBDIRS = fileserver simple qtscript clientbench
!pillow_no_ssl: SUBDIRS += simplessl
