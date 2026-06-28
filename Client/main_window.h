#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "network_client.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMainWindow>
#include <QStandardItemModel>

class QLabel;
class QPushButton;
class QStackedWidget;
class QTableView;
class QTreeView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(const QString &serverHost, quint16 serverPort, QWidget *parent = nullptr);

private slots:
    void handleConnected();
    void handleDisconnected();
    void handleConnectionState(const QString &state);
    void handleResponse(quint64 requestId, const QString &type, bool ok, const QJsonObject &payload, const QString &error);
    void handleTreeDoubleClick(const QModelIndex &index);
    void handleTableListDoubleClick(const QModelIndex &index);
    void handleRowChanged(QStandardItem *item);
    void addRow();
    void saveRows();
    void deleteSelectedRow();
    void previousPage();
    void nextPage();

private:
    enum CustomRole {
        NodeKindRole = Qt::UserRole + 1,
        DatabaseRole,
        TableRole,
        RowStateRole,
        OriginalKeyRole
    };

    enum NodeKind {
        DatabaseNode = 1,
        TableNode = 2
    };

    struct PendingRequest {
        QString action;
        QString databaseName;
        QString tableName;
    };

    void buildUi();
    void requestDatabaseList();
    void requestTableList(const QString &databaseName);
    void requestTableSchema(const QString &databaseName, const QString &tableName);
    void requestTableRows(int offset);
    void openTable(const QString &databaseName, const QString &tableName);
    void populateDatabases(const QJsonArray &databases);
    void populateTables(const QString &databaseName, const QJsonArray &tables);
    void applySchema(const QJsonArray &columns);
    void applyRows(const QJsonObject &payload);
    void updateActions();
    void registerPending(quint64 requestId, const QString &action, const QString &databaseName = QString(), const QString &tableName = QString());

    QStandardItem *findDatabaseItem(const QString &databaseName) const;
    QJsonObject valuesForRow(int row) const;
    QJsonObject originalKeyForRow(int row) const;
    bool validateRequiredFields(const QJsonObject &values, QString *error) const;
    QStringList primaryKeyColumns() const;
    QString displayText(const QJsonValue &value) const;
    QJsonValue valueFromEditorText(const QString &text) const;

    NetworkClient *m_client = nullptr;
    QTreeView *m_treeView = nullptr;
    QTableView *m_tableListView = nullptr;
    QTableView *m_rowsView = nullptr;
    QStackedWidget *m_stack = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_pageLabel = nullptr;
    QPushButton *m_addButton = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QPushButton *m_prevButton = nullptr;
    QPushButton *m_nextButton = nullptr;

    QStandardItemModel m_treeModel;
    QStandardItemModel m_tableListModel;
    QStandardItemModel m_rowsModel;
    QHash<quint64, PendingRequest> m_pending;

    QString m_currentDatabase;
    QString m_currentTable;
    QStringList m_currentColumns;
    QJsonArray m_schema;
    int m_pageOffset = 0;
    int m_pageSize = 100;
    bool m_hasMore = false;
    bool m_loadingRows = false;
    int m_pendingMutations = 0;
};

#endif
