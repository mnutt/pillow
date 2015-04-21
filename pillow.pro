include(config.pri)

TEMPLATE = subdirs
SUBDIRS = pillowcore

tests.depends = pillowcore
examples.depends = pillowcore

OTHER_FILES += README
