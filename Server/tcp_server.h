#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <QTcpServer>

class ClientSession;
class DatabaseDispatcher;

class TcpServer : public QObject
{
    Q_OBJECT

public:
    explicit TcpServer(DatabaseDispatcher *dispatcher, QObject *parent = nullptr);

    bool listen(const QHostAddress &address, quint16 port);

private slots:
    void acceptConnection();
    void sessionMessage(quint64 sessionId, const QJsonObject &message);
    void sessionClosed(quint64 sessionId);
    void dispatcherResponse(quint64 sessionId, const QJsonObject &response);

private:
    void sendError(ClientSession *session, quint64 requestId, const QString &type, const QString &error);
    bool isDatabaseRequest(const QString &type) const;

    QTcpServer m_server;
    DatabaseDispatcher *m_dispatcher = nullptr;
    QHash<quint64, ClientSession *> m_sessions;
    quint64 m_nextSessionId = 1;
};

#endif
