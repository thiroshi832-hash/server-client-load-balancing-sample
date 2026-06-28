QT += core core-private sql sql-private

TEMPLATE = lib
TARGET = qsqlmysql
CONFIG += plugin release

MYSQL_CONNECTOR = $$PWD/../../ThirdParty/mysql-connector-c-6.1.11-win32
QT_MYSQL_SRC = $$clean_path($$[QT_INSTALL_PREFIX]/../Src/qtbase/src/plugins/sqldrivers/mysql)
!exists($$QT_MYSQL_SRC/qsql_mysql.cpp): QT_MYSQL_SRC = C:/Qt/Qt5.14.0/5.14.0/Src/qtbase/src/plugins/sqldrivers/mysql

DESTDIR = $$PWD/../../Build/qsqlmysql-plugin

DEFINES += QT_PLUGIN QT_NO_CAST_TO_ASCII QT_NO_CAST_FROM_ASCII

INCLUDEPATH += \
    $$QT_MYSQL_SRC \
    $$MYSQL_CONNECTOR/include

SOURCES += \
    $$QT_MYSQL_SRC/qsql_mysql.cpp \
    $$QT_MYSQL_SRC/main.cpp

HEADERS += \
    $$QT_MYSQL_SRC/qsql_mysql_p.h

OTHER_FILES += \
    $$QT_MYSQL_SRC/mysql.json

LIBS += \
    -L$$MYSQL_CONNECTOR/lib \
    -lmysql
