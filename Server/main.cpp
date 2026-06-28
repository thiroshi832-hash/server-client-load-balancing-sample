#include "database_config.h"
#include "database_dispatcher.h"
#include "tcp_server.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QHostAddress>
#include <QJsonObject>
#include <QThread>
#include <QTimer>

static QString envOrDefault(const char *name, const QString &fallback)
{
    const QByteArray value = qgetenv(name);
    return value.isEmpty() ? fallback : QString::fromLocal8Bit(value);
}

static int envIntOrDefault(const char *name, int fallback)
{
    bool ok = false;
    const int value = QString::fromLocal8Bit(qgetenv(name)).toInt(&ok);
    return ok ? value : fallback;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("QtDatabaseServer"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    qRegisterMetaType<QJsonObject>("QJsonObject");
    qRegisterMetaType<quint64>("quint64");

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Qt database server for TCP client applications."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption listenAddress(QStringList() << QStringLiteral("a") << QStringLiteral("listen-address"),
                                     QStringLiteral("Address to listen on."),
                                     QStringLiteral("address"),
                                     envOrDefault("SERVER_LISTEN_ADDRESS", QStringLiteral("0.0.0.0")));
    QCommandLineOption listenPort(QStringList() << QStringLiteral("p") << QStringLiteral("listen-port"),
                                  QStringLiteral("Port to listen on."),
                                  QStringLiteral("port"),
                                  QString::number(envIntOrDefault("SERVER_LISTEN_PORT", 7100)));
    QCommandLineOption dbHost(QStringLiteral("db-host"),
                              QStringLiteral("MySQL host."),
                              QStringLiteral("host"),
                              envOrDefault("DB_HOST", QStringLiteral("127.0.0.1")));
    QCommandLineOption dbPort(QStringLiteral("db-port"),
                              QStringLiteral("MySQL port."),
                              QStringLiteral("port"),
                              QString::number(envIntOrDefault("DB_PORT", 3306)));
    QCommandLineOption dbUser(QStringLiteral("db-user"),
                              QStringLiteral("MySQL user."),
                              QStringLiteral("user"),
                              envOrDefault("DB_USER", QStringLiteral("root")));
    QCommandLineOption dbPassword(QStringLiteral("db-password"),
                                  QStringLiteral("MySQL password."),
                                  QStringLiteral("password"),
                                  envOrDefault("DB_PASSWORD", QString()));
    QCommandLineOption dbName(QStringLiteral("db-name"),
                              QStringLiteral("Optional default database name."),
                              QStringLiteral("database"),
                              envOrDefault("DB_NAME", QString()));
    QCommandLineOption dbDriver(QStringLiteral("db-driver"),
                                QStringLiteral("Qt SQL driver."),
                                QStringLiteral("driver"),
                                envOrDefault("DB_DRIVER", QStringLiteral("QMYSQL")));
    QCommandLineOption workerCount(QStringLiteral("workers"),
                                   QStringLiteral("Database worker count."),
                                   QStringLiteral("count"),
                                   QString::number(envIntOrDefault("DB_WORKERS", qMax(4, QThread::idealThreadCount()))));

    parser.addOption(listenAddress);
    parser.addOption(listenPort);
    parser.addOption(dbHost);
    parser.addOption(dbPort);
    parser.addOption(dbUser);
    parser.addOption(dbPassword);
    parser.addOption(dbName);
    parser.addOption(dbDriver);
    parser.addOption(workerCount);
    parser.process(app);

    DatabaseConfig dbConfig;
    dbConfig.driver = parser.value(dbDriver);
    dbConfig.hostName = parser.value(dbHost);
    dbConfig.port = parser.value(dbPort).toInt();
    dbConfig.userName = parser.value(dbUser);
    dbConfig.password = parser.value(dbPassword);
    dbConfig.databaseName = parser.value(dbName);
    dbConfig.workerCount = qMax(1, parser.value(workerCount).toInt());

    DatabaseDispatcher dispatcher(dbConfig);
    TcpServer server(&dispatcher);

    const QHostAddress address(parser.value(listenAddress));
    const quint16 port = static_cast<quint16>(parser.value(listenPort).toUShort());
    if (!server.listen(address, port))
        return 1;

    return app.exec();
}
