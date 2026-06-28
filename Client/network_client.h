#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

class NetworkClient : public QObject
{
    Q_OBJECT

public:
    explicit NetworkClient(QObject *parent = nullptr);

    void start(const QString &hostName, quint16 port);
    void stop();
    bool isConnected() const;
    quint64 sendRequest(const QString &type, const QJsonObject &payload = QJsonObject());

signals:
    void connected();
    void disconnected();
    void stateChanged(const QString &state);
    void responseReceived(quint64 requestId, const QString &type, bool ok, const QJsonObject &payload, const QString &error);
    void binaryResponseReceived(quint64 requestId, const QString &type, bool ok, const QJsonObject &payload, const QByteArray &data, const QString &error);

private slots:
    void connectNow();
    void socketConnected();
    void socketDisconnected();
    void socketError(QAbstractSocket::SocketError error);
    void readAvailable();
    void sendHeartbeat();

private:
    QTcpSocket m_socket;
    QByteArray m_buffer;
    QTimer m_reconnectTimer;
    QTimer m_heartbeatTimer;
    QString m_hostName;
    quint16 m_port = 0;
    quint64 m_nextRequestId = 1;
    bool m_shouldReconnect = false;
};

#endif
