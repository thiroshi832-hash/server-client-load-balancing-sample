#include "database_task_worker.h"

#include "protocol.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QSqlError>
#include <QSqlField>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTime>
#include <QVariant>

DatabaseTaskWorker::DatabaseTaskWorker(const DatabaseConfig &config, int workerIndex, QObject *parent)
    : QObject(parent),
      m_config(config),
      m_connectionName(QStringLiteral("server_worker_%1").arg(workerIndex))
{
}

DatabaseTaskWorker::~DatabaseTaskWorker()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName);
            if (db.isOpen())
                db.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

void DatabaseTaskWorker::process(quint64 sessionId, const QJsonObject &message)
{
    const QString type = Protocol::messageType(message);
    const quint64 requestId = Protocol::messageId(message);
    const QJsonObject request = Protocol::messagePayload(message);

    QJsonObject payload;
    QString error;
    bool ok = false;

    if (!ensureOpen(&error)) {
        ok = false;
    } else if (type == QLatin1String(Protocol::Type::ListDatabases)) {
        ok = runListDatabases(&payload, &error);
    } else if (type == QLatin1String(Protocol::Type::ListTables)) {
        ok = runListTables(request, &payload, &error);
    } else if (type == QLatin1String(Protocol::Type::GetTableSchema)) {
        ok = runGetTableSchema(request, &payload, &error);
    } else if (type == QLatin1String(Protocol::Type::GetTableRows)) {
        ok = runGetTableRows(request, &payload, &error);
    } else if (type == QLatin1String(Protocol::Type::InsertRow)) {
        ok = runInsertRow(request, &payload, &error);
    } else if (type == QLatin1String(Protocol::Type::UpdateRow)) {
        ok = runUpdateRow(request, &payload, &error);
    } else if (type == QLatin1String(Protocol::Type::DeleteRow)) {
        ok = runDeleteRow(request, &payload, &error);
    } else {
        error = QStringLiteral("Unsupported request type: %1").arg(type);
    }

    emit responseReady(sessionId, Protocol::makeResponse(requestId, type, ok, payload, error));
}

bool DatabaseTaskWorker::ensureOpen(QString *error)
{
    if (!QSqlDatabase::isDriverAvailable(m_config.driver)) {
        if (error) {
            error->append(QStringLiteral("SQL driver %1 is not available. Available drivers: %2")
                          .arg(m_config.driver, QSqlDatabase::drivers().join(QStringLiteral(", "))));
        }
        return false;
    }

    QSqlDatabase db;
    if (QSqlDatabase::contains(m_connectionName)) {
        db = QSqlDatabase::database(m_connectionName);
    } else {
        db = QSqlDatabase::addDatabase(m_config.driver, m_connectionName);
        db.setHostName(m_config.hostName);
        db.setPort(m_config.port);
        db.setUserName(m_config.userName);
        db.setPassword(m_config.password);
        if (!m_config.databaseName.isEmpty())
            db.setDatabaseName(m_config.databaseName);
    }

    if (db.isOpen())
        return true;

    if (!db.open()) {
        if (error)
            *error = QStringLiteral("Database connection failed: %1").arg(db.lastError().text());
        return false;
    }

    return true;
}

bool DatabaseTaskWorker::validateIdentifier(const QString &identifier, QString *error) const
{
    static const QRegularExpression safeIdentifier(QStringLiteral("^[A-Za-z0-9_$]+$"));
    if (identifier.isEmpty() || !safeIdentifier.match(identifier).hasMatch()) {
        if (error)
            *error = QStringLiteral("Unsafe or empty identifier: %1").arg(identifier);
        return false;
    }
    return true;
}

QString DatabaseTaskWorker::quotedIdentifier(const QString &identifier, QString *error) const
{
    if (!validateIdentifier(identifier, error))
        return QString();
    return QStringLiteral("`%1`").arg(identifier);
}

QString DatabaseTaskWorker::qualifiedTableName(const QString &databaseName, const QString &tableName, QString *error) const
{
    const QString databasePart = quotedIdentifier(databaseName, error);
    if (databasePart.isEmpty())
        return QString();

    const QString tablePart = quotedIdentifier(tableName, error);
    if (tablePart.isEmpty())
        return QString();

    return databasePart + QLatin1Char('.') + tablePart;
}

bool DatabaseTaskWorker::runListDatabases(QJsonObject *payload, QString *error)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(QStringLiteral("SHOW DATABASES"))) {
        if (error)
            *error = lastSqlError(QStringLiteral("SHOW DATABASES failed"));
        return false;
    }

    QJsonArray databases;
    while (query.next())
        databases.append(query.value(0).toString());

    payload->insert(QStringLiteral("databases"), databases);
    return true;
}

bool DatabaseTaskWorker::runListTables(const QJsonObject &request, QJsonObject *payload, QString *error)
{
    const QString databaseName = request.value(QStringLiteral("database")).toString();
    const QString databaseSql = quotedIdentifier(databaseName, error);
    if (databaseSql.isEmpty())
        return false;

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(QStringLiteral("SHOW FULL TABLES FROM %1").arg(databaseSql))) {
        if (error)
            *error = lastSqlError(QStringLiteral("SHOW TABLES failed"));
        return false;
    }

    QJsonArray tables;
    while (query.next()) {
        QJsonObject table;
        table.insert(QStringLiteral("name"), query.value(0).toString());
        table.insert(QStringLiteral("type"), query.value(1).toString());
        tables.append(table);
    }

    payload->insert(QStringLiteral("database"), databaseName);
    payload->insert(QStringLiteral("tables"), tables);
    return true;
}

bool DatabaseTaskWorker::runGetTableSchema(const QJsonObject &request, QJsonObject *payload, QString *error)
{
    const QString databaseName = request.value(QStringLiteral("database")).toString();
    const QString tableName = request.value(QStringLiteral("table")).toString();

    QJsonArray columns;
    if (!loadSchema(databaseName, tableName, &columns, error))
        return false;

    payload->insert(QStringLiteral("database"), databaseName);
    payload->insert(QStringLiteral("table"), tableName);
    payload->insert(QStringLiteral("columns"), columns);
    return true;
}

bool DatabaseTaskWorker::runGetTableRows(const QJsonObject &request, QJsonObject *payload, QString *error)
{
    const QString databaseName = request.value(QStringLiteral("database")).toString();
    const QString tableName = request.value(QStringLiteral("table")).toString();
    const QString tableSql = qualifiedTableName(databaseName, tableName, error);
    if (tableSql.isEmpty())
        return false;

    const int offset = qMax(0, request.value(QStringLiteral("offset")).toInt(0));
    const int requestedLimit = request.value(QStringLiteral("limit")).toInt(100);
    const int limit = qBound(1, requestedLimit, 1000);
    const int queryLimit = limit + 1;

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    const QString sql = QStringLiteral("SELECT * FROM %1 LIMIT %2 OFFSET %3")
                            .arg(tableSql)
                            .arg(queryLimit)
                            .arg(offset);
    if (!query.exec(sql)) {
        if (error)
            *error = lastSqlError(QStringLiteral("SELECT rows failed"));
        return false;
    }

    const QSqlRecord record = query.record();
    QJsonArray columns;
    for (int i = 0; i < record.count(); ++i)
        columns.append(record.fieldName(i));

    QJsonArray rows;
    int count = 0;
    bool hasMore = false;
    while (query.next()) {
        if (count >= limit) {
            hasMore = true;
            break;
        }

        QJsonObject row;
        for (int i = 0; i < record.count(); ++i)
            row.insert(record.fieldName(i), variantToJson(query.value(i)));
        rows.append(row);
        ++count;
    }

    payload->insert(QStringLiteral("database"), databaseName);
    payload->insert(QStringLiteral("table"), tableName);
    payload->insert(QStringLiteral("offset"), offset);
    payload->insert(QStringLiteral("limit"), limit);
    payload->insert(QStringLiteral("hasMore"), hasMore);
    payload->insert(QStringLiteral("columns"), columns);
    payload->insert(QStringLiteral("rows"), rows);
    return true;
}

bool DatabaseTaskWorker::runInsertRow(const QJsonObject &request, QJsonObject *payload, QString *error)
{
    const QString databaseName = request.value(QStringLiteral("database")).toString();
    const QString tableName = request.value(QStringLiteral("table")).toString();
    const QString tableSql = qualifiedTableName(databaseName, tableName, error);
    if (tableSql.isEmpty())
        return false;

    const QJsonObject values = request.value(QStringLiteral("values")).toObject();
    QJsonArray schema;
    if (!loadSchema(databaseName, tableName, &schema, error))
        return false;

    QSet<QString> knownColumns;
    QJsonArray insertColumns;
    QStringList sqlColumns;
    QStringList sqlValues;

    for (const QJsonValue &columnValue : schema) {
        const QJsonObject column = columnValue.toObject();
        const QString name = column.value(QStringLiteral("name")).toString();
        const bool nullable = column.value(QStringLiteral("nullable")).toBool();
        const bool hasDefault = !column.value(QStringLiteral("default")).isNull();
        const bool autoIncrement = column.value(QStringLiteral("extra")).toString().contains(QStringLiteral("auto_increment"), Qt::CaseInsensitive);
        knownColumns.insert(name);

        const QJsonValue provided = values.value(name);
        const bool missing = provided.isUndefined() || provided.isNull() || (provided.isString() && provided.toString().isEmpty());
        if (!nullable && !hasDefault && !autoIncrement && missing) {
            if (error)
                *error = QStringLiteral("Required field is missing: %1").arg(name);
            return false;
        }

        if (!provided.isUndefined() && !(autoIncrement && missing)) {
            insertColumns.append(name);
            sqlColumns.append(quotedIdentifier(name, error));
            sqlValues.append(QStringLiteral(":v%1").arg(sqlValues.count()));
        }
    }

    for (const QString &name : values.keys()) {
        if (!knownColumns.contains(name)) {
            if (error)
                *error = QStringLiteral("Unknown column: %1").arg(name);
            return false;
        }
    }

    QString sql;
    if (sqlColumns.isEmpty()) {
        sql = QStringLiteral("INSERT INTO %1 () VALUES ()").arg(tableSql);
    } else {
        sql = QStringLiteral("INSERT INTO %1 (%2) VALUES (%3)")
                  .arg(tableSql, sqlColumns.join(QStringLiteral(", ")), sqlValues.join(QStringLiteral(", ")));
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.prepare(sql)) {
        if (error)
            *error = lastSqlError(QStringLiteral("Prepare insert failed"));
        return false;
    }

    for (int i = 0; i < insertColumns.count(); ++i) {
        const QString name = insertColumns.at(i).toString();
        query.bindValue(QStringLiteral(":v%1").arg(i), jsonToVariant(values.value(name)));
    }

    if (!query.exec()) {
        if (error)
            *error = lastSqlError(QStringLiteral("Insert failed"));
        return false;
    }

    payload->insert(QStringLiteral("affectedRows"), query.numRowsAffected());
    payload->insert(QStringLiteral("lastInsertId"), variantToJson(query.lastInsertId()));
    return true;
}

bool DatabaseTaskWorker::runUpdateRow(const QJsonObject &request, QJsonObject *payload, QString *error)
{
    const QString databaseName = request.value(QStringLiteral("database")).toString();
    const QString tableName = request.value(QStringLiteral("table")).toString();
    const QString tableSql = qualifiedTableName(databaseName, tableName, error);
    if (tableSql.isEmpty())
        return false;

    const QJsonObject values = request.value(QStringLiteral("values")).toObject();
    const QJsonObject key = request.value(QStringLiteral("key")).toObject();
    if (values.isEmpty() || key.isEmpty()) {
        if (error)
            *error = QStringLiteral("Update requires values and primary key");
        return false;
    }

    QJsonArray schema;
    if (!loadSchema(databaseName, tableName, &schema, error))
        return false;

    QSet<QString> knownColumns;
    for (const QJsonValue &columnValue : schema)
        knownColumns.insert(columnValue.toObject().value(QStringLiteral("name")).toString());

    QStringList setParts;
    QStringList whereParts;
    for (const QString &name : values.keys()) {
        if (!knownColumns.contains(name)) {
            if (error)
                *error = QStringLiteral("Unknown column: %1").arg(name);
            return false;
        }
        setParts.append(QStringLiteral("%1 = :s%2").arg(quotedIdentifier(name, error)).arg(setParts.count()));
    }

    for (const QString &name : key.keys()) {
        if (!knownColumns.contains(name)) {
            if (error)
                *error = QStringLiteral("Unknown key column: %1").arg(name);
            return false;
        }
        whereParts.append(QStringLiteral("%1 = :k%2").arg(quotedIdentifier(name, error)).arg(whereParts.count()));
    }

    const QString sql = QStringLiteral("UPDATE %1 SET %2 WHERE %3")
                            .arg(tableSql, setParts.join(QStringLiteral(", ")), whereParts.join(QStringLiteral(" AND ")));
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.prepare(sql)) {
        if (error)
            *error = lastSqlError(QStringLiteral("Prepare update failed"));
        return false;
    }

    int index = 0;
    for (const QString &name : values.keys())
        query.bindValue(QStringLiteral(":s%1").arg(index++), jsonToVariant(values.value(name)));

    index = 0;
    for (const QString &name : key.keys())
        query.bindValue(QStringLiteral(":k%1").arg(index++), jsonToVariant(key.value(name)));

    if (!query.exec()) {
        if (error)
            *error = lastSqlError(QStringLiteral("Update failed"));
        return false;
    }

    payload->insert(QStringLiteral("affectedRows"), query.numRowsAffected());
    return true;
}

bool DatabaseTaskWorker::runDeleteRow(const QJsonObject &request, QJsonObject *payload, QString *error)
{
    const QString databaseName = request.value(QStringLiteral("database")).toString();
    const QString tableName = request.value(QStringLiteral("table")).toString();
    const QString tableSql = qualifiedTableName(databaseName, tableName, error);
    if (tableSql.isEmpty())
        return false;

    const QJsonObject key = request.value(QStringLiteral("key")).toObject();
    if (key.isEmpty()) {
        if (error)
            *error = QStringLiteral("Delete requires primary key values");
        return false;
    }

    QJsonArray schema;
    if (!loadSchema(databaseName, tableName, &schema, error))
        return false;

    QSet<QString> knownColumns;
    for (const QJsonValue &columnValue : schema)
        knownColumns.insert(columnValue.toObject().value(QStringLiteral("name")).toString());

    QStringList whereParts;
    for (const QString &name : key.keys()) {
        if (!knownColumns.contains(name)) {
            if (error)
                *error = QStringLiteral("Unknown key column: %1").arg(name);
            return false;
        }
        whereParts.append(QStringLiteral("%1 = :k%2").arg(quotedIdentifier(name, error)).arg(whereParts.count()));
    }

    const QString sql = QStringLiteral("DELETE FROM %1 WHERE %2").arg(tableSql, whereParts.join(QStringLiteral(" AND ")));
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.prepare(sql)) {
        if (error)
            *error = lastSqlError(QStringLiteral("Prepare delete failed"));
        return false;
    }

    int index = 0;
    for (const QString &name : key.keys())
        query.bindValue(QStringLiteral(":k%1").arg(index++), jsonToVariant(key.value(name)));

    if (!query.exec()) {
        if (error)
            *error = lastSqlError(QStringLiteral("Delete failed"));
        return false;
    }

    payload->insert(QStringLiteral("affectedRows"), query.numRowsAffected());
    return true;
}

bool DatabaseTaskWorker::loadSchema(const QString &databaseName, const QString &tableName, QJsonArray *columns, QString *error)
{
    if (!columns)
        return false;

    if (!validateIdentifier(databaseName, error) || !validateIdentifier(tableName, error))
        return false;

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, IS_NULLABLE, COLUMN_KEY, COLUMN_DEFAULT, EXTRA "
        "FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = ? AND TABLE_NAME = ? "
        "ORDER BY ORDINAL_POSITION"));
    query.addBindValue(databaseName);
    query.addBindValue(tableName);

    if (!query.exec()) {
        if (error)
            *error = lastSqlError(QStringLiteral("Schema lookup failed"));
        return false;
    }

    while (query.next()) {
        QJsonObject column;
        const QString key = query.value(4).toString();
        column.insert(QStringLiteral("name"), query.value(0).toString());
        column.insert(QStringLiteral("dataType"), query.value(1).toString());
        column.insert(QStringLiteral("columnType"), query.value(2).toString());
        column.insert(QStringLiteral("nullable"), query.value(3).toString() == QLatin1String("YES"));
        column.insert(QStringLiteral("key"), key);
        column.insert(QStringLiteral("primaryKey"), key == QLatin1String("PRI"));
        column.insert(QStringLiteral("default"), variantToJson(query.value(5)));
        column.insert(QStringLiteral("extra"), query.value(6).toString());
        columns->append(column);
    }

    if (columns->isEmpty()) {
        if (error)
            *error = QStringLiteral("Table was not found or has no columns: %1.%2").arg(databaseName, tableName);
        return false;
    }

    return true;
}

QVariant DatabaseTaskWorker::jsonToVariant(const QJsonValue &value) const
{
    if (value.isUndefined() || value.isNull())
        return QVariant();
    return value.toVariant();
}

QJsonValue DatabaseTaskWorker::variantToJson(const QVariant &value) const
{
    if (!value.isValid() || value.isNull())
        return QJsonValue();

    switch (value.type()) {
    case QVariant::Date:
        return value.toDate().toString(Qt::ISODate);
    case QVariant::Time:
        return value.toTime().toString(Qt::ISODate);
    case QVariant::DateTime:
        return value.toDateTime().toString(Qt::ISODate);
    case QVariant::ByteArray:
        return QString::fromLatin1(value.toByteArray().toBase64());
    default:
        return QJsonValue::fromVariant(value);
    }
}

QString DatabaseTaskWorker::lastSqlError(const QString &prefix) const
{
    const QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    const QString dbError = db.lastError().text();
    if (!dbError.isEmpty())
        return prefix + QStringLiteral(": ") + dbError;
    return prefix;
}
