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
const char GetBlob[] = "GET_BLOB";
const char InsertRow[] = "INSERT_ROW";
const char UpdateRow[] = "UPDATE_ROW";
const char DeleteRow[] = "DELETE_ROW";
}

static const char BinaryFrameMarker = 'B';

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

QByteArray encodeBinaryFrame(const QJsonObject &header, const QByteArray &payload)
{
    const QByteArray json = QJsonDocument(header).toJson(QJsonDocument::Compact);
    const quint32 bodySize = static_cast<quint32>(1 + sizeof(quint32) + json.size() + payload.size());

    QByteArray frame;
    frame.reserve(static_cast<int>(sizeof(quint32) + bodySize));

    QDataStream stream(&frame, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << bodySize;
    frame.append(BinaryFrameMarker);

    QByteArray headerSizeBytes;
    QDataStream headerSizeStream(&headerSizeBytes, QIODevice::WriteOnly);
    headerSizeStream.setByteOrder(QDataStream::BigEndian);
    headerSizeStream << static_cast<quint32>(json.size());
    frame.append(headerSizeBytes);
    frame.append(json);
    frame.append(payload);
    return frame;
}

FrameKind tryDecodeAnyFrame(QByteArray *buffer, QJsonObject *message, QByteArray *payload, QString *error)
{
    if (!buffer || !message)
        return InvalidFrame;

    if (payload)
        payload->clear();

    if (buffer->size() < static_cast<int>(sizeof(quint32)))
        return NoFrame;

    QDataStream stream(buffer->left(static_cast<int>(sizeof(quint32))));
    stream.setByteOrder(QDataStream::BigEndian);
    quint32 frameSize = 0;
    stream >> frameSize;

    if (frameSize == 0 || frameSize > MaxFrameSize) {
        if (error)
            *error = QStringLiteral("Invalid frame size: %1").arg(frameSize);
        buffer->clear();
        return InvalidFrame;
    }

    const int totalSize = static_cast<int>(sizeof(quint32) + frameSize);
    if (buffer->size() < totalSize)
        return NoFrame;

    const QByteArray body = buffer->mid(static_cast<int>(sizeof(quint32)), static_cast<int>(frameSize));
    buffer->remove(0, totalSize);

    if (!body.isEmpty() && body.at(0) == BinaryFrameMarker) {
        if (body.size() < static_cast<int>(1 + sizeof(quint32))) {
            if (error)
                *error = QStringLiteral("Invalid binary frame header");
            return InvalidFrame;
        }

        QDataStream headerStream(body.mid(1, static_cast<int>(sizeof(quint32))));
        headerStream.setByteOrder(QDataStream::BigEndian);
        quint32 headerSize = 0;
        headerStream >> headerSize;

        const int headerOffset = static_cast<int>(1 + sizeof(quint32));
        if (headerSize == 0 || headerOffset + static_cast<int>(headerSize) > body.size()) {
            if (error)
                *error = QStringLiteral("Invalid binary frame JSON header size");
            return InvalidFrame;
        }

        QJsonParseError parseError;
        const QByteArray headerJson = body.mid(headerOffset, static_cast<int>(headerSize));
        const QJsonDocument doc = QJsonDocument::fromJson(headerJson, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            if (error)
                *error = QStringLiteral("Invalid binary frame JSON header: %1").arg(parseError.errorString());
            return InvalidFrame;
        }

        *message = doc.object();
        if (payload)
            *payload = body.mid(headerOffset + static_cast<int>(headerSize));
        return BinaryFrame;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = QStringLiteral("Invalid JSON frame: %1").arg(parseError.errorString());
        return InvalidFrame;
    }

    *message = doc.object();
    return JsonFrame;
}

bool tryDecodeFrame(QByteArray *buffer, QJsonObject *message, QString *error)
{
    QByteArray payload;
    const FrameKind kind = tryDecodeAnyFrame(buffer, message, &payload, error);
    return kind == JsonFrame;
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

bool writeBinaryMessage(QIODevice *device, const QJsonObject &header, const QByteArray &payload, QString *error)
{
    if (!device || !device->isWritable()) {
        if (error)
            *error = QStringLiteral("Socket is not writable");
        return false;
    }

    const QByteArray frame = encodeBinaryFrame(header, payload);
    const qint64 written = device->write(frame);
    if (written != frame.size()) {
        if (error)
            *error = QStringLiteral("Failed to write complete binary frame");
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
