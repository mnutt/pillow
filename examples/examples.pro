include(../config.pri)
TEMPLATE = subdirs

SUBDIRS = fileserver simple clientbench
!pillow_no_ssl: SUBDIRS += simplessl
