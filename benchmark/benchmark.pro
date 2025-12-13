include(../examples/examples.pri)

TEMPLATE = app
TARGET = bench_server

QT += core network
QT -= gui

CONFIG += console
CONFIG -= app_bundle

INCLUDEPATH += .
DEPENDPATH += .

SOURCES += bench_server.cpp
