include(config.pri)

TEMPLATE = subdirs
SUBDIRS = pillowcore examples

tests.depends = pillowcore
examples.depends = pillowcore

OTHER_FILES += README
