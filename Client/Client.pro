QT += core gui widgets network

CONFIG += c++11

TEMPLATE = app
TARGET = QtDatabaseClient

include(../Common/Common.pri)

SOURCES += \
    main.cpp \
    main_window.cpp \
    network_client.cpp

HEADERS += \
    main_window.h \
    network_client.h
