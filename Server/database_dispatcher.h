#ifndef DATABASE_DISPATCHER_H
#define DATABASE_DISPATCHER_H

#include "database_config.h"

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QVector>

class DatabaseTaskWorker;
class QThread;

class DatabaseDispatcher : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseDispatcher(const DatabaseConfig &config, QObject *parent = nullptr);
    ~DatabaseDispatcher();

    void dispatch(quint64 sessionId, const QJsonObject &message);

signals:
    void responseReady(quint64 sessionId, const QJsonObject &response);
    void binaryResponseReady(quint64 sessionId, const QJsonObject &header, const QByteArray &payload);

private:
    QVector<QThread *> m_threads;
    QVector<DatabaseTaskWorker *> m_workers;
    int m_nextWorker = 0;
};

#endif
