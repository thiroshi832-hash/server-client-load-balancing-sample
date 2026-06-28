#include "protocol.h"

#include <QDataStream>
#include <QJsonDocument>

namespace Protocol {

namespace Field {
const char Id[] = "id";
const char Type[] = "type";
const char Ok[] = "ok";
const char Payload[] = "payload";
const char Error[] = "error";
}

namespace Type {
const char Auth[] = "AUTH";
const char Heartbeat[] = "HEARTBEAT";
const char ListDatabases[] = "LIST_DATABASES";
const char ListTables[] = "LIST_TABLES";
const char GetTableSchema[] = "GET_TABLE_SCHEMA";
const char GetTableRows[] = "GET_TABLE_ROWS";
const char InsertRow[] = "INSERT_ROW";
const char UpdateRow[] = "UPDATE_ROW";
const char DeleteRow[] = "DELETE_ROW";
}

QJsonObject makeRequest(quint64 id, const QString &type, const QJsonObject &payload)
{
    QJsonObject message;
    message.insert(Field::Id, QString::number(id));
    message.insert(Field::Type, type);
    message.insert(Field::Payload, payload);
    return message;
}

QJsonObject makeResponse(quint64 id, const QString &type, bool ok, const QJsonObject &payload, const QString &error)
{
    QJsonObject message;
    message.insert(Field::Id, QString::number(id));
    message.insert(Field::Type, type);
    message.insert(Field::Ok, ok);
    message.insert(Field::Payload, payload);
    if (!error.isEmpty())
        message.insert(Field::Error, error);
    return message;
}

QByteArray encodeFrame(const QJsonObject &message)
{
    const QByteArray json = QJsonDocument(message).toJson(QJsonDocument::Compact);
    QByteArray frame;
    frame.reserve(static_cast<int>(json.size() + sizeof(quint32)));

    QDataStream stream(&frame, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << static_cast<quint32>(json.size());
    frame.append(json);
    return frame;
}

bool tryDecodeFrame(QByteArray *buffer, QJsonObject *message, QString *error)
{
    if (!buffer || !message)
        return false;

    if (buffer->size() < static_cast<int>(sizeof(quint32)))
        return false;

    QDataStream stream(buffer->left(static_cast<int>(sizeof(quint32))));
    stream.setByteOrder(QDataStream::BigEndian);
    quint32 frameSize = 0;
    stream >> frameSize;

    if (frameSize == 0 || frameSize > MaxFrameSize) {
        if (error)
            *error = QStringLiteral("Invalid frame size: %1").arg(frameSize);
        buffer->clear();
        return false;
    }

    const int totalSize = static_cast<int>(sizeof(quint32) + frameSize);
    if (buffer->size() < totalSize)
        return false;

    const QByteArray json = buffer->mid(static_cast<int>(sizeof(quint32)), static_cast<int>(frameSize));
    buffer->remove(0, totalSize);

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = QStringLiteral("Invalid JSON frame: %1").arg(parseError.errorString());
        return false;
    }

    *message = doc.object();
    return true;
}

bool writeMessage(QIODevice *device, const QJsonObject &message, QString *error)
{
    if (!device || !device->isWritable()) {
        if (error)
            *error = QStringLiteral("Socket is not writable");
        return false;
    }

    const QByteArray frame = encodeFrame(message);
    const qint64 written = device->write(frame);
    if (written != frame.size()) {
        if (error)
            *error = QStringLiteral("Failed to write complete frame");
        return false;
    }

    return true;
}

QString messageType(const QJsonObject &message)
{
    return message.value(Field::Type).toString();
}

quint64 messageId(const QJsonObject &message)
{
    bool ok = false;
    const quint64 stringId = message.value(Field::Id).toString().toULongLong(&ok);
    if (ok)
        return stringId;
    return static_cast<quint64>(message.value(Field::Id).toDouble());
}

QJsonObject messagePayload(const QJsonObject &message)
{
    return message.value(Field::Payload).toObject();
}

}
