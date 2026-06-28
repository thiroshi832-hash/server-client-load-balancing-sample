#include "network_client.h"

#include "protocol.h"

#include <QAbstractSocket>
#include <QDebug>

NetworkClient::NetworkClient(QObject *parent)
    : QObject(parent)
{
    m_reconnectTimer.setInterval(2000);
    m_reconnectTimer.setSingleShot(true);
    m_heartbeatTimer.setInterval(5000);

    connect(&m_socket, &QTcpSocket::connected, this, &NetworkClient::socketConnected);
    connect(&m_socket, &QTcpSocket::disconnected, this, &NetworkClient::socketDisconnected);
    connect(&m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error), this, &NetworkClient::socketError);
    connect(&m_socket, &QTcpSocket::readyRead, this, &NetworkClient::readAvailable);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &NetworkClient::connectNow);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &NetworkClient::sendHeartbeat);
}

void NetworkClient::start(const QString &hostName, quint16 port)
{
    m_hostName = hostName;
    m_port = port;
    m_shouldReconnect = true;
    connectNow();
}

void NetworkClient::stop()
{
    m_shouldReconnect = false;
    m_reconnectTimer.stop();
    m_heartbeatTimer.stop();
    m_socket.disconnectFromHost();
}

bool NetworkClient::isConnected() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

quint64 NetworkClient::sendRequest(const QString &type, const QJsonObject &payload)
{
    const quint64 requestId = m_nextRequestId++;
    if (!isConnected()) {
        emit responseReceived(requestId, type, false, QJsonObject(), QStringLiteral("Client is not connected"));
        return requestId;
    }

    QString error;
    const bool ok = Protocol::writeMessage(&m_socket, Protocol::makeRequest(requestId, type, payload), &error);
    if (!ok)
        emit responseReceived(requestId, type, false, QJsonObject(), error);

    return requestId;
}

void NetworkClient::connectNow()
{
    if (m_hostName.isEmpty() || m_port == 0)
        return;

    if (m_socket.state() == QAbstractSocket::ConnectedState || m_socket.state() == QAbstractSocket::ConnectingState)
        return;

    emit stateChanged(QStringLiteral("Connecting"));
    m_socket.connectToHost(m_hostName, m_port);
}

void NetworkClient::socketConnected()
{
    m_buffer.clear();
    emit stateChanged(QStringLiteral("Connected"));
    emit connected();
    sendRequest(QLatin1String(Protocol::Type::Auth));
    m_heartbeatTimer.start();
}

void NetworkClient::socketDisconnected()
{
    m_heartbeatTimer.stop();
    emit stateChanged(m_shouldReconnect ? QStringLiteral("Reconnecting") : QStringLiteral("Disconnected"));
    emit disconnected();

    if (m_shouldReconnect)
        m_reconnectTimer.start();
}

void NetworkClient::socketError(QAbstractSocket::SocketError)
{
    emit stateChanged(QStringLiteral("Error: %1").arg(m_socket.errorString()));
}

void NetworkClient::readAvailable()
{
    m_buffer.append(m_socket.readAll());

    while (true) {
        QJsonObject message;
        QString error;
        const bool decoded = Protocol::tryDecodeFrame(&m_buffer, &message, &error);
        if (!decoded) {
            if (!error.isEmpty()) {
                emit stateChanged(QStringLiteral("Protocol error: %1").arg(error));
                m_socket.disconnectFromHost();
            }
            return;
        }

        const quint64 requestId = Protocol::messageId(message);
        const QString type = Protocol::messageType(message);
        const bool ok = message.value(Protocol::Field::Ok).toBool(false);
        const QJsonObject payload = Protocol::messagePayload(message);
        const QString responseError = message.value(Protocol::Field::Error).toString();
        emit responseReceived(requestId, type, ok, payload, responseError);
    }
}

void NetworkClient::sendHeartbeat()
{
    sendRequest(QLatin1String(Protocol::Type::Heartbeat));
}
