#ifndef DATABASE_CONFIG_H
#define DATABASE_CONFIG_H

#include <QString>

struct DatabaseConfig
{
    QString driver = QStringLiteral("QMYSQL");
    QString hostName = QStringLiteral("127.0.0.1");
    QString databaseName;
    QString userName = QStringLiteral("root");
    QString password;
    int port = 3306;
    int workerCount = 8;
};

#endif
