#include "tcp_server.h"

#include "client_session.h"
#include "database_dispatcher.h"
#include "protocol.h"

#include <QDateTime>
#include <QDebug>
#include <QTcpSocket>

TcpServer::TcpServer(DatabaseDispatcher *dispatcher, QObject *parent)
    : QObject(parent),
      m_dispatcher(dispatcher)
{
    connect(&m_server, &QTcpServer::newConnection, this, &TcpServer::acceptConnection);
    connect(m_dispatcher, &DatabaseDispatcher::responseReady, this, &TcpServer::dispatcherResponse);
}

bool TcpServer::listen(const QHostAddress &address, quint16 port)
{
    if (!m_server.listen(address, port)) {
        qCritical() << "Listen failed:" << m_server.errorString();
        return false;
    }

    qInfo() << "Server listening on" << m_server.serverAddress().toString() << m_server.serverPort();
    return true;
}

void TcpServer::acceptConnection()
{
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        const quint64 sessionId = m_nextSessionId++;
        ClientSession *session = new ClientSession(sessionId, socket, this);
        m_sessions.insert(sessionId, session);

        connect(session, &ClientSession::messageReceived, this, &TcpServer::sessionMessage);
        connect(session, &ClientSession::closed, this, &TcpServer::sessionClosed);

        qInfo() << "Client connected" << sessionId << session->peerName();
    }
}

void TcpServer::sessionMessage(quint64 sessionId, const QJsonObject &message)
{
    ClientSession *session = m_sessions.value(sessionId, nullptr);
    if (!session)
        return;

    const QString type = Protocol::messageType(message);
    const quint64 requestId = Protocol::messageId(message);

    if (type == QLatin1String(Protocol::Type::Heartbeat)) {
        QJsonObject payload;
        payload.insert(QStringLiteral("serverTimeUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        payload.insert(QStringLiteral("sessionId"), QString::number(sessionId));
        session->sendMessage(Protocol::makeResponse(requestId, type, true, payload));
        return;
    }

    if (type == QLatin1String(Protocol::Type::Auth)) {
        QJsonObject payload;
        payload.insert(QStringLiteral("sessionId"), QString::number(sessionId));
        payload.insert(QStringLiteral("message"), QStringLiteral("Authenticated"));
        session->sendMessage(Protocol::makeResponse(requestId, type, true, payload));
        return;
    }

    if (isDatabaseRequest(type)) {
        m_dispatcher->dispatch(sessionId, message);
        return;
    }

    sendError(session, requestId, type, QStringLiteral("Unsupported request type: %1").arg(type));
}

void TcpServer::sessionClosed(quint64 sessionId)
{
    ClientSession *session = m_sessions.take(sessionId);
    if (!session)
        return;

    qInfo() << "Client disconnected" << sessionId;
    session->deleteLater();
}

void TcpServer::dispatcherResponse(quint64 sessionId, const QJsonObject &response)
{
    ClientSession *session = m_sessions.value(sessionId, nullptr);
    if (!session)
        return;

    session->sendMessage(response);
}

void TcpServer::sendError(ClientSession *session, quint64 requestId, const QString &type, const QString &error)
{
    if (!session)
        return;
    session->sendMessage(Protocol::makeResponse(requestId, type, false, QJsonObject(), error));
}

bool TcpServer::isDatabaseRequest(const QString &type) const
{
    return type == QLatin1String(Protocol::Type::ListDatabases)
        || type == QLatin1String(Protocol::Type::ListTables)
        || type == QLatin1String(Protocol::Type::GetTableSchema)
        || type == QLatin1String(Protocol::Type::GetTableRows)
        || type == QLatin1String(Protocol::Type::InsertRow)
        || type == QLatin1String(Protocol::Type::UpdateRow)
        || type == QLatin1String(Protocol::Type::DeleteRow);
}
