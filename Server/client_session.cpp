#include "client_session.h"

#include "protocol.h"

#include <QDebug>
#include <QHostAddress>
#include <QTcpSocket>

ClientSession::ClientSession(quint64 sessionId, QTcpSocket *socket, QObject *parent)
    : QObject(parent),
      m_sessionId(sessionId),
      m_socket(socket)
{
    m_socket->setParent(this);
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientSession::readAvailable);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientSession::socketDisconnected);
}

quint64 ClientSession::sessionId() const
{
    return m_sessionId;
}

QString ClientSession::peerName() const
{
    if (!m_socket)
        return QString();
    return QStringLiteral("%1:%2").arg(m_socket->peerAddress().toString()).arg(m_socket->peerPort());
}

void ClientSession::sendMessage(const QJsonObject &message)
{
    if (!m_socket)
        return;

    QString error;
    if (!Protocol::writeMessage(m_socket, message, &error)) {
        qWarning() << "Failed to send response to session" << m_sessionId << error;
        m_socket->disconnectFromHost();
    }
}

void ClientSession::readAvailable()
{
    m_buffer.append(m_socket->readAll());

    while (true) {
        QJsonObject message;
        QString error;
        const bool decoded = Protocol::tryDecodeFrame(&m_buffer, &message, &error);
        if (!decoded) {
            if (!error.isEmpty()) {
                qWarning() << "Protocol error from session" << m_sessionId << error;
                m_socket->disconnectFromHost();
            }
            return;
        }

        emit messageReceived(m_sessionId, message);
    }
}

void ClientSession::socketDisconnected()
{
    emit closed(m_sessionId);
}
