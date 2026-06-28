QT += core network sql
QT -= gui

CONFIG += console c++11
CONFIG -= app_bundle

TEMPLATE = app
TARGET = QtDatabaseServer

include(../Common/Common.pri)

SOURCES += \
    main.cpp \
    client_session.cpp \
    database_dispatcher.cpp \
    database_task_worker.cpp \
    tcp_server.cpp

HEADERS += \
    client_session.h \
    database_config.h \
    database_dispatcher.h \
    database_task_worker.h \
    tcp_server.h
