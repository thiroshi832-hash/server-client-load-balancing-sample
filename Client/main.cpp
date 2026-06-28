#include "main_window.h"

#include <QApplication>
#include <QStringList>

static QString optionValue(const QStringList &arguments, int *index, const QString &prefix, const QString &fallback)
{
    const QString current = arguments.at(*index);
    const QString withEquals = prefix + QLatin1Char('=');
    if (current.startsWith(withEquals))
        return current.mid(withEquals.length());

    if (*index + 1 < arguments.size())
        return arguments.at(++(*index));

    return fallback;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("QtDatabaseClient"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QString serverHost = QStringLiteral("127.0.0.1");
    quint16 serverPort = 7000;

    const QStringList arguments = app.arguments();
    for (int i = 1; i < arguments.size(); ++i) {
        const QString argument = arguments.at(i);
        if (argument == QLatin1String("--server-host") || argument == QLatin1String("-server-host")
                || argument == QLatin1String("-s") || argument == QLatin1String("server-host")) {
            serverHost = optionValue(arguments, &i, argument, serverHost);
        } else if (argument.startsWith(QLatin1String("--server-host="))) {
            serverHost = optionValue(arguments, &i, QStringLiteral("--server-host"), serverHost);
        } else if (argument.startsWith(QLatin1String("-server-host="))) {
            serverHost = optionValue(arguments, &i, QStringLiteral("-server-host"), serverHost);
        } else if (argument == QLatin1String("--server-port") || argument == QLatin1String("-server-port")
                   || argument == QLatin1String("-p") || argument == QLatin1String("server-port")) {
            bool ok = false;
            const int parsedPort = optionValue(arguments, &i, argument, QString::number(serverPort)).toInt(&ok);
            if (ok && parsedPort > 0 && parsedPort <= 65535)
                serverPort = static_cast<quint16>(parsedPort);
        } else if (argument.startsWith(QLatin1String("--server-port="))) {
            bool ok = false;
            const int parsedPort = optionValue(arguments, &i, QStringLiteral("--server-port"), QString::number(serverPort)).toInt(&ok);
            if (ok && parsedPort > 0 && parsedPort <= 65535)
                serverPort = static_cast<quint16>(parsedPort);
        } else if (argument.startsWith(QLatin1String("-server-port="))) {
            bool ok = false;
            const int parsedPort = optionValue(arguments, &i, QStringLiteral("-server-port"), QString::number(serverPort)).toInt(&ok);
            if (ok && parsedPort > 0 && parsedPort <= 65535)
                serverPort = static_cast<quint16>(parsedPort);
        }
    }

    MainWindow window(serverHost, serverPort);
    window.show();

    return app.exec();
}
