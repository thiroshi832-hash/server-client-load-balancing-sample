#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QByteArray>
#include <QIODevice>
#include <QJsonObject>
#include <QString>

namespace Protocol {

static const quint32 MaxFrameSize = 16 * 1024 * 1024;

namespace Field {
extern const char Id[];
extern const char Type[];
extern const char Ok[];
extern const char Payload[];
extern const char Error[];
}

namespace Type {
extern const char Auth[];
extern const char Heartbeat[];
extern const char ListDatabases[];
extern const char ListTables[];
extern const char GetTableSchema[];
extern const char GetTableRows[];
extern const char GetBlob[];
extern const char InsertRow[];
extern const char UpdateRow[];
extern const char DeleteRow[];
}

enum FrameKind {
    NoFrame,
    JsonFrame,
    BinaryFrame,
    InvalidFrame
};

QJsonObject makeRequest(quint64 id, const QString &type, const QJsonObject &payload = QJsonObject());
QJsonObject makeResponse(quint64 id, const QString &type, bool ok, const QJsonObject &payload = QJsonObject(), const QString &error = QString());

QByteArray encodeFrame(const QJsonObject &message);
QByteArray encodeBinaryFrame(const QJsonObject &header, const QByteArray &payload);
FrameKind tryDecodeAnyFrame(QByteArray *buffer, QJsonObject *message, QByteArray *payload, QString *error);
bool tryDecodeFrame(QByteArray *buffer, QJsonObject *message, QString *error);
bool writeMessage(QIODevice *device, const QJsonObject &message, QString *error = nullptr);
bool writeBinaryMessage(QIODevice *device, const QJsonObject &header, const QByteArray &payload, QString *error = nullptr);

QString messageType(const QJsonObject &message);
quint64 messageId(const QJsonObject &message);
QJsonObject messagePayload(const QJsonObject &message);

}

#endif
