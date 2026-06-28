#ifndef DATABASE_TASK_WORKER_H
#define DATABASE_TASK_WORKER_H

#include "database_config.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QSqlDatabase>

class DatabaseTaskWorker : public QObject
{
    Q_OBJECT

public:
    DatabaseTaskWorker(const DatabaseConfig &config, int workerIndex, QObject *parent = nullptr);
    ~DatabaseTaskWorker();

public slots:
    void process(quint64 sessionId, const QJsonObject &message);

signals:
    void responseReady(quint64 sessionId, const QJsonObject &response);

private:
    bool ensureOpen(QString *error);
    bool validateIdentifier(const QString &identifier, QString *error) const;
    QString quotedIdentifier(const QString &identifier, QString *error) const;
    QString qualifiedTableName(const QString &databaseName, const QString &tableName, QString *error) const;

    bool runListDatabases(QJsonObject *payload, QString *error);
    bool runListTables(const QJsonObject &request, QJsonObject *payload, QString *error);
    bool runGetTableSchema(const QJsonObject &request, QJsonObject *payload, QString *error);
    bool runGetTableRows(const QJsonObject &request, QJsonObject *payload, QString *error);
    bool runInsertRow(const QJsonObject &request, QJsonObject *payload, QString *error);
    bool runUpdateRow(const QJsonObject &request, QJsonObject *payload, QString *error);
    bool runDeleteRow(const QJsonObject &request, QJsonObject *payload, QString *error);

    bool loadSchema(const QString &databaseName, const QString &tableName, QJsonArray *columns, QString *error);
    QVariant jsonToVariant(const QJsonValue &value) const;
    QJsonValue variantToJson(const QVariant &value) const;
    QString lastSqlError(const QString &prefix) const;

    DatabaseConfig m_config;
    QString m_connectionName;
};

#endif
