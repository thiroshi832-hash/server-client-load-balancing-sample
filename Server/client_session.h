#ifndef CLIENT_SESSION_H
#define CLIENT_SESSION_H

#include <QObject>
#include <QByteArray>
#include <QJsonObject>

class QTcpSocket;

class ClientSession : public QObject
{
    Q_OBJECT

public:
    ClientSession(quint64 sessionId, QTcpSocket *socket, QObject *parent = nullptr);

    quint64 sessionId() const;
    QString peerName() const;
    void sendMessage(const QJsonObject &message);

signals:
    void messageReceived(quint64 sessionId, const QJsonObject &message);
    void closed(quint64 sessionId);

private slots:
    void readAvailable();
    void socketDisconnected();

private:
    quint64 m_sessionId = 0;
    QTcpSocket *m_socket = nullptr;
    QByteArray m_buffer;
};

#endif
