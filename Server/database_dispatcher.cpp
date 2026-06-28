#include "database_dispatcher.h"

#include "database_task_worker.h"

#include <QDebug>
#include <QMetaObject>
#include <QThread>

DatabaseDispatcher::DatabaseDispatcher(const DatabaseConfig &config, QObject *parent)
    : QObject(parent)
{
    const int workerCount = qMax(1, config.workerCount);
    m_threads.reserve(workerCount);
    m_workers.reserve(workerCount);

    for (int i = 0; i < workerCount; ++i) {
        QThread *thread = new QThread(this);
        DatabaseTaskWorker *worker = new DatabaseTaskWorker(config, i);
        worker->moveToThread(thread);

        connect(thread, &QThread::finished, worker, &QObject::deleteLater);
        connect(worker, &DatabaseTaskWorker::responseReady, this, &DatabaseDispatcher::responseReady);

        m_threads.append(thread);
        m_workers.append(worker);
        thread->start();
    }

    qInfo() << "Started database worker threads:" << workerCount;
}

DatabaseDispatcher::~DatabaseDispatcher()
{
    for (QThread *thread : m_threads)
        thread->quit();

    for (QThread *thread : m_threads)
        thread->wait();
}

void DatabaseDispatcher::dispatch(quint64 sessionId, const QJsonObject &message)
{
    if (m_workers.isEmpty())
        return;

    DatabaseTaskWorker *worker = m_workers.at(m_nextWorker);
    m_nextWorker = (m_nextWorker + 1) % m_workers.size();

    QMetaObject::invokeMethod(worker,
                              "process",
                              Qt::QueuedConnection,
                              Q_ARG(quint64, sessionId),
                              Q_ARG(QJsonObject, message));
}
