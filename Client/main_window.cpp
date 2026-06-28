#include "main_window.h"

#include "protocol.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QModelIndex>
#include <QPixmap>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTableView>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

MainWindow::MainWindow(const QString &serverHost, quint16 serverPort, QWidget *parent)
    : QMainWindow(parent),
      m_client(new NetworkClient(this))
{
    buildUi();

    connect(m_client, &NetworkClient::connected, this, &MainWindow::handleConnected);
    connect(m_client, &NetworkClient::disconnected, this, &MainWindow::handleDisconnected);
    connect(m_client, &NetworkClient::stateChanged, this, &MainWindow::handleConnectionState);
    connect(m_client, &NetworkClient::responseReceived, this, &MainWindow::handleResponse);
    connect(m_client, &NetworkClient::binaryResponseReceived, this, &MainWindow::handleBinaryResponse);

    m_client->start(serverHost, serverPort);
    updateActions();
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("Qt Database Client"));
    resize(1180, 760);

    m_treeModel.setHorizontalHeaderLabels(QStringList() << QStringLiteral("Databases"));
    m_tableListModel.setHorizontalHeaderLabels(QStringList() << QStringLiteral("Table") << QStringLiteral("Type"));

    m_treeView = new QTreeView(this);
    m_treeView->setModel(&m_treeModel);
    m_treeView->setHeaderHidden(false);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_treeView, &QTreeView::doubleClicked, this, &MainWindow::handleTreeDoubleClick);

    m_tableListView = new QTableView(this);
    m_tableListView->setModel(&m_tableListModel);
    m_tableListView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableListView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableListView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableListView->horizontalHeader()->setStretchLastSection(true);
    connect(m_tableListView, &QTableView::doubleClicked, this, &MainWindow::handleTableListDoubleClick);

    m_rowsView = new QTableView(this);
    m_rowsView->setModel(&m_rowsModel);
    m_rowsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_rowsView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_rowsView->horizontalHeader()->setStretchLastSection(false);
    m_rowsView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    connect(m_rowsView, &QTableView::doubleClicked, this, &MainWindow::handleRowsDoubleClick);
    connect(&m_rowsModel, &QStandardItemModel::itemChanged, this, &MainWindow::handleRowChanged);
    connect(m_rowsView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        updateActions();
    });

    QWidget *rowsPage = new QWidget(this);
    QVBoxLayout *rowsLayout = new QVBoxLayout(rowsPage);
    rowsLayout->setContentsMargins(0, 0, 0, 0);

    QToolBar *rowToolbar = new QToolBar(rowsPage);
    rowToolbar->setMovable(false);

    m_addButton = new QPushButton(style()->standardIcon(QStyle::SP_FileDialogNewFolder), QStringLiteral("Add Row"), rowsPage);
    m_saveButton = new QPushButton(style()->standardIcon(QStyle::SP_DialogSaveButton), QStringLiteral("Save"), rowsPage);
    m_deleteButton = new QPushButton(style()->standardIcon(QStyle::SP_TrashIcon), QStringLiteral("Delete Row"), rowsPage);
    m_prevButton = new QPushButton(style()->standardIcon(QStyle::SP_ArrowLeft), QStringLiteral("Previous"), rowsPage);
    m_nextButton = new QPushButton(style()->standardIcon(QStyle::SP_ArrowRight), QStringLiteral("Next"), rowsPage);
    m_pageLabel = new QLabel(rowsPage);

    rowToolbar->addWidget(m_addButton);
    rowToolbar->addWidget(m_saveButton);
    rowToolbar->addWidget(m_deleteButton);
    rowToolbar->addSeparator();
    rowToolbar->addWidget(m_prevButton);
    rowToolbar->addWidget(m_nextButton);
    rowToolbar->addWidget(m_pageLabel);

    rowsLayout->addWidget(rowToolbar);
    rowsLayout->addWidget(m_rowsView);

    connect(m_addButton, &QPushButton::clicked, this, &MainWindow::addRow);
    connect(m_saveButton, &QPushButton::clicked, this, &MainWindow::saveRows);
    connect(m_deleteButton, &QPushButton::clicked, this, &MainWindow::deleteSelectedRow);
    connect(m_prevButton, &QPushButton::clicked, this, &MainWindow::previousPage);
    connect(m_nextButton, &QPushButton::clicked, this, &MainWindow::nextPage);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_tableListView);
    m_stack->addWidget(rowsPage);

    QSplitter *splitter = new QSplitter(this);
    splitter->addWidget(m_treeView);
    splitter->addWidget(m_stack);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes(QList<int>() << 280 << 900);

    setCentralWidget(splitter);

    m_statusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_statusLabel);
}

void MainWindow::handleConnected()
{
    requestDatabaseList();
}

void MainWindow::handleDisconnected()
{
    updateActions();
}

void MainWindow::handleConnectionState(const QString &state)
{
    m_statusLabel->setText(QStringLiteral("Server: %1").arg(state));
    updateActions();
}

void MainWindow::handleResponse(quint64 requestId, const QString &type, bool ok, const QJsonObject &payload, const QString &error)
{
    PendingRequest pending = m_pending.take(requestId);

    if (type == QLatin1String(Protocol::Type::Heartbeat) || type == QLatin1String(Protocol::Type::Auth))
        return;

    if (!ok) {
        if (pending.action == QLatin1String("mutation") && m_pendingMutations > 0)
            --m_pendingMutations;
        statusBar()->showMessage(error, 8000);
        QMessageBox::warning(this, QStringLiteral("Request Failed"), error);
        updateActions();
        return;
    }

    if (type == QLatin1String(Protocol::Type::ListDatabases)) {
        populateDatabases(payload.value(QStringLiteral("databases")).toArray());
    } else if (type == QLatin1String(Protocol::Type::ListTables)) {
        populateTables(payload.value(QStringLiteral("database")).toString(), payload.value(QStringLiteral("tables")).toArray());
    } else if (type == QLatin1String(Protocol::Type::GetTableSchema)) {
        applySchema(payload.value(QStringLiteral("columns")).toArray());
        requestTableRows(0);
    } else if (type == QLatin1String(Protocol::Type::GetTableRows)) {
        applyRows(payload);
    } else if (type == QLatin1String(Protocol::Type::GetBlob)) {
        m_pendingBlobs.remove(requestId);
        if (payload.value(QStringLiteral("isNull")).toBool(false))
            QMessageBox::information(this, QStringLiteral("BLOB"), QStringLiteral("This BLOB value is NULL."));
    } else if (pending.action == QLatin1String("mutation")) {
        if (m_pendingMutations > 0)
            --m_pendingMutations;
        if (m_pendingMutations == 0)
            requestTableRows(m_pageOffset);
    }

    updateActions();
}

void MainWindow::handleBinaryResponse(quint64 requestId, const QString &type, bool ok, const QJsonObject &payload, const QByteArray &data, const QString &error)
{
    PendingRequest pending = m_pending.value(requestId);
    if (type != QLatin1String(Protocol::Type::GetBlob))
        return;

    if (!ok) {
        m_pending.remove(requestId);
        m_pendingBlobs.remove(requestId);
        QMessageBox::warning(this, QStringLiteral("BLOB Request Failed"), error);
        return;
    }

    PendingBlob blob = m_pendingBlobs.value(requestId);
    if (blob.columnName.isEmpty()) {
        blob.databaseName = pending.databaseName;
        blob.tableName = pending.tableName;
        blob.columnName = pending.columnName;
        blob.totalSize = payload.value(QStringLiteral("totalSize")).toInt(-1);
    }

    blob.data.append(data);
    if (blob.totalSize < 0)
        blob.totalSize = payload.value(QStringLiteral("totalSize")).toInt(-1);

    const bool finalChunk = payload.value(QStringLiteral("final")).toBool(false);
    if (finalChunk) {
        m_pending.remove(requestId);
        m_pendingBlobs.remove(requestId);
        const QString title = QStringLiteral("%1.%2.%3 (%4)")
                                  .arg(blob.databaseName, blob.tableName, blob.columnName, formatBlobSize(blob.data.size()));
        showBlobViewer(title, blob.data);
        statusBar()->showMessage(QStringLiteral("Loaded BLOB %1").arg(formatBlobSize(blob.data.size())), 4000);
    } else {
        m_pendingBlobs.insert(requestId, blob);
        if (blob.totalSize > 0) {
            statusBar()->showMessage(QStringLiteral("Loading BLOB %1 / %2")
                                     .arg(formatBlobSize(blob.data.size()), formatBlobSize(blob.totalSize)), 1000);
        }
    }
}

void MainWindow::handleTreeDoubleClick(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    QStandardItem *item = m_treeModel.itemFromIndex(index);
    if (!item)
        return;

    const int kind = item->data(NodeKindRole).toInt();
    const QString databaseName = item->data(DatabaseRole).toString();
    const QString tableName = item->data(TableRole).toString();

    if (kind == DatabaseNode) {
        requestTableList(databaseName);
    } else if (kind == TableNode) {
        openTable(databaseName, tableName);
    }
}

void MainWindow::handleTableListDoubleClick(const QModelIndex &index)
{
    if (!index.isValid() || m_currentDatabase.isEmpty())
        return;

    QStandardItem *item = m_tableListModel.item(index.row(), 0);
    if (!item)
        return;

    openTable(m_currentDatabase, item->text());
}

void MainWindow::handleRowsDoubleClick(const QModelIndex &index)
{
    requestBlob(index);
}

void MainWindow::handleRowChanged(QStandardItem *item)
{
    if (m_loadingRows || !item)
        return;

    QStandardItem *first = m_rowsModel.item(item->row(), 0);
    if (!first)
        return;

    const QString state = first->data(RowStateRole).toString();
    if (state != QLatin1String("new"))
        first->setData(QStringLiteral("dirty"), RowStateRole);

    updateActions();
}

void MainWindow::addRow()
{
    if (m_currentColumns.isEmpty())
        return;

    QList<QStandardItem *> items;
    for (const QString &column : m_currentColumns) {
        QStandardItem *item = new QStandardItem();
        item->setEditable(true);
        item->setData(column, TableRole);
        items.append(item);
    }

    items.first()->setData(QStringLiteral("new"), RowStateRole);
    m_rowsModel.appendRow(items);
    m_rowsView->scrollToBottom();
    updateActions();
}

void MainWindow::saveRows()
{
    if (!m_client->isConnected())
        return;

    int mutationCount = 0;
    for (int row = 0; row < m_rowsModel.rowCount(); ++row) {
        QStandardItem *first = m_rowsModel.item(row, 0);
        if (!first)
            continue;

        const QString state = first->data(RowStateRole).toString();
        if (state != QLatin1String("new") && state != QLatin1String("dirty"))
            continue;

        const QJsonObject values = valuesForRow(row);
        QString validationError;
        if (state == QLatin1String("new") && !validateRequiredFields(values, &validationError)) {
            QMessageBox::warning(this, QStringLiteral("Validation Failed"), validationError);
            return;
        }

        QJsonObject payload;
        payload.insert(QStringLiteral("database"), m_currentDatabase);
        payload.insert(QStringLiteral("table"), m_currentTable);
        payload.insert(QStringLiteral("values"), values);

        quint64 requestId = 0;
        if (state == QLatin1String("new")) {
            requestId = m_client->sendRequest(QLatin1String(Protocol::Type::InsertRow), payload);
        } else {
            const QJsonObject key = originalKeyForRow(row);
            if (key.isEmpty()) {
                QMessageBox::warning(this, QStringLiteral("Cannot Save"), QStringLiteral("This table has no primary key values for the selected row."));
                return;
            }
            payload.insert(QStringLiteral("key"), key);
            requestId = m_client->sendRequest(QLatin1String(Protocol::Type::UpdateRow), payload);
        }

        registerPending(requestId, QStringLiteral("mutation"), m_currentDatabase, m_currentTable);
        ++mutationCount;
    }

    m_pendingMutations = mutationCount;
    if (mutationCount == 0)
        statusBar()->showMessage(QStringLiteral("No changes to save"), 3000);

    updateActions();
}

void MainWindow::deleteSelectedRow()
{
    const QModelIndex index = m_rowsView->currentIndex();
    if (!index.isValid())
        return;

    const int row = index.row();
    QStandardItem *first = m_rowsModel.item(row, 0);
    if (!first)
        return;

    if (first->data(RowStateRole).toString() == QLatin1String("new")) {
        m_rowsModel.removeRow(row);
        updateActions();
        return;
    }

    const QJsonObject key = originalKeyForRow(row);
    if (key.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Cannot Delete"), QStringLiteral("This table has no primary key values for the selected row."));
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("database"), m_currentDatabase);
    payload.insert(QStringLiteral("table"), m_currentTable);
    payload.insert(QStringLiteral("key"), key);

    const quint64 requestId = m_client->sendRequest(QLatin1String(Protocol::Type::DeleteRow), payload);
    registerPending(requestId, QStringLiteral("mutation"), m_currentDatabase, m_currentTable);
    m_pendingMutations = 1;
    updateActions();
}

void MainWindow::previousPage()
{
    if (m_pageOffset <= 0)
        return;
    requestTableRows(qMax(0, m_pageOffset - m_pageSize));
}

void MainWindow::nextPage()
{
    if (!m_hasMore)
        return;
    requestTableRows(m_pageOffset + m_pageSize);
}

void MainWindow::requestDatabaseList()
{
    const quint64 requestId = m_client->sendRequest(QLatin1String(Protocol::Type::ListDatabases));
    registerPending(requestId, QStringLiteral("databases"));
}

void MainWindow::requestTableList(const QString &databaseName)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("database"), databaseName);
    const quint64 requestId = m_client->sendRequest(QLatin1String(Protocol::Type::ListTables), payload);
    registerPending(requestId, QStringLiteral("tables"), databaseName);
}

void MainWindow::requestTableSchema(const QString &databaseName, const QString &tableName)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("database"), databaseName);
    payload.insert(QStringLiteral("table"), tableName);
    const quint64 requestId = m_client->sendRequest(QLatin1String(Protocol::Type::GetTableSchema), payload);
    registerPending(requestId, QStringLiteral("schema"), databaseName, tableName);
}

void MainWindow::requestTableRows(int offset)
{
    if (m_currentDatabase.isEmpty() || m_currentTable.isEmpty())
        return;

    QJsonObject payload;
    payload.insert(QStringLiteral("database"), m_currentDatabase);
    payload.insert(QStringLiteral("table"), m_currentTable);
    payload.insert(QStringLiteral("offset"), offset);
    payload.insert(QStringLiteral("limit"), m_pageSize);
    const quint64 requestId = m_client->sendRequest(QLatin1String(Protocol::Type::GetTableRows), payload);
    registerPending(requestId, QStringLiteral("rows"), m_currentDatabase, m_currentTable);
}

void MainWindow::requestBlob(const QModelIndex &index)
{
    if (!index.isValid() || !m_client->isConnected())
        return;

    QStandardItem *item = m_rowsModel.item(index.row(), index.column());
    if (!item || !item->data(BlobMetaRole).toBool())
        return;

    const QJsonObject key = originalKeyForRow(index.row());
    if (key.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Cannot Load BLOB"), QStringLiteral("This row has no primary key values."));
        return;
    }

    const QString columnName = m_currentColumns.value(index.column());
    QJsonObject payload;
    payload.insert(QStringLiteral("database"), m_currentDatabase);
    payload.insert(QStringLiteral("table"), m_currentTable);
    payload.insert(QStringLiteral("column"), columnName);
    payload.insert(QStringLiteral("key"), key);

    const quint64 requestId = m_client->sendRequest(QLatin1String(Protocol::Type::GetBlob), payload);
    registerPending(requestId, QStringLiteral("blob"), m_currentDatabase, m_currentTable, columnName);

    PendingBlob pendingBlob;
    pendingBlob.databaseName = m_currentDatabase;
    pendingBlob.tableName = m_currentTable;
    pendingBlob.columnName = columnName;
    m_pendingBlobs.insert(requestId, pendingBlob);
    statusBar()->showMessage(QStringLiteral("Loading BLOB..."), 1000);
}

void MainWindow::openTable(const QString &databaseName, const QString &tableName)
{
    m_currentDatabase = databaseName;
    m_currentTable = tableName;
    m_stack->setCurrentIndex(1);
    requestTableSchema(databaseName, tableName);
}

void MainWindow::populateDatabases(const QJsonArray &databases)
{
    m_treeModel.removeRows(0, m_treeModel.rowCount());
    for (const QJsonValue &value : databases) {
        QStandardItem *item = new QStandardItem(value.toString());
        item->setEditable(false);
        item->setData(DatabaseNode, NodeKindRole);
        item->setData(value.toString(), DatabaseRole);
        m_treeModel.appendRow(item);
    }
}

void MainWindow::populateTables(const QString &databaseName, const QJsonArray &tables)
{
    m_currentDatabase = databaseName;
    m_stack->setCurrentIndex(0);

    QStandardItem *databaseItem = findDatabaseItem(databaseName);
    if (databaseItem)
        databaseItem->removeRows(0, databaseItem->rowCount());

    m_tableListModel.removeRows(0, m_tableListModel.rowCount());
    for (const QJsonValue &value : tables) {
        const QJsonObject table = value.toObject();
        const QString tableName = table.value(QStringLiteral("name")).toString();
        const QString tableType = table.value(QStringLiteral("type")).toString();

        QList<QStandardItem *> row;
        row << new QStandardItem(tableName) << new QStandardItem(tableType);
        for (QStandardItem *item : row)
            item->setEditable(false);
        m_tableListModel.appendRow(row);

        if (databaseItem) {
            QStandardItem *tableItem = new QStandardItem(tableName);
            tableItem->setEditable(false);
            tableItem->setData(TableNode, NodeKindRole);
            tableItem->setData(databaseName, DatabaseRole);
            tableItem->setData(tableName, TableRole);
            databaseItem->appendRow(tableItem);
        }
    }

    if (databaseItem)
        m_treeView->expand(databaseItem->index());
}

void MainWindow::applySchema(const QJsonArray &columns)
{
    m_schema = columns;
    m_currentColumns.clear();

    for (const QJsonValue &value : columns)
        m_currentColumns.append(value.toObject().value(QStringLiteral("name")).toString());
}

void MainWindow::applyRows(const QJsonObject &payload)
{
    m_loadingRows = true;
    m_rowsModel.clear();
    m_rowsModel.setHorizontalHeaderLabels(m_currentColumns);

    const QJsonArray rows = payload.value(QStringLiteral("rows")).toArray();
    const QStringList primaryKeys = primaryKeyColumns();

    for (const QJsonValue &rowValue : rows) {
        const QJsonObject rowObject = rowValue.toObject();
        QList<QStandardItem *> items;
        for (const QString &column : m_currentColumns) {
            const QJsonValue cellValue = rowObject.value(column);
            QStandardItem *item = new QStandardItem(displayText(cellValue));
            item->setEditable(!isBlobValue(cellValue));
            if (isBlobValue(cellValue))
                item->setData(true, BlobMetaRole);
            items.append(item);
        }

        if (!items.isEmpty()) {
            QJsonObject key;
            for (const QString &primaryKey : primaryKeys)
                key.insert(primaryKey, rowObject.value(primaryKey));
            items.first()->setData(QStringLiteral("clean"), RowStateRole);
            items.first()->setData(key.toVariantMap(), OriginalKeyRole);
        }

        m_rowsModel.appendRow(items);
    }

    m_pageOffset = payload.value(QStringLiteral("offset")).toInt(0);
    m_hasMore = payload.value(QStringLiteral("hasMore")).toBool(false);
    m_pageLabel->setText(QStringLiteral("Rows %1-%2").arg(m_pageOffset + 1).arg(m_pageOffset + m_rowsModel.rowCount()));
    m_loadingRows = false;
}

void MainWindow::updateActions()
{
    const bool connected = m_client->isConnected();
    const bool tableOpen = connected && !m_currentDatabase.isEmpty() && !m_currentTable.isEmpty() && !m_currentColumns.isEmpty();
    const bool busyMutating = m_pendingMutations > 0;

    m_addButton->setEnabled(tableOpen && !busyMutating);
    m_saveButton->setEnabled(tableOpen && !busyMutating);
    m_deleteButton->setEnabled(tableOpen && !busyMutating && m_rowsView->currentIndex().isValid());
    m_prevButton->setEnabled(tableOpen && m_pageOffset > 0);
    m_nextButton->setEnabled(tableOpen && m_hasMore);
}

void MainWindow::registerPending(quint64 requestId, const QString &action, const QString &databaseName, const QString &tableName)
{
    PendingRequest pending;
    pending.action = action;
    pending.databaseName = databaseName;
    pending.tableName = tableName;
    m_pending.insert(requestId, pending);
}

void MainWindow::registerPending(quint64 requestId, const QString &action, const QString &databaseName, const QString &tableName, const QString &columnName)
{
    PendingRequest pending;
    pending.action = action;
    pending.databaseName = databaseName;
    pending.tableName = tableName;
    pending.columnName = columnName;
    m_pending.insert(requestId, pending);
}

QStandardItem *MainWindow::findDatabaseItem(const QString &databaseName) const
{
    for (int row = 0; row < m_treeModel.rowCount(); ++row) {
        QStandardItem *item = m_treeModel.item(row);
        if (item && item->data(DatabaseRole).toString() == databaseName)
            return item;
    }
    return nullptr;
}

QJsonObject MainWindow::valuesForRow(int row) const
{
    QJsonObject values;
    for (int column = 0; column < m_currentColumns.count(); ++column) {
        QStandardItem *item = m_rowsModel.item(row, column);
        if (item && item->data(BlobMetaRole).toBool())
            continue;
        values.insert(m_currentColumns.at(column), valueFromEditorText(item ? item->text() : QString()));
    }
    return values;
}

QJsonObject MainWindow::originalKeyForRow(int row) const
{
    QStandardItem *first = m_rowsModel.item(row, 0);
    if (!first)
        return QJsonObject();
    return QJsonObject::fromVariantMap(first->data(OriginalKeyRole).toMap());
}

bool MainWindow::validateRequiredFields(const QJsonObject &values, QString *error) const
{
    for (const QJsonValue &columnValue : m_schema) {
        const QJsonObject column = columnValue.toObject();
        const QString name = column.value(QStringLiteral("name")).toString();
        const bool nullable = column.value(QStringLiteral("nullable")).toBool();
        const bool hasDefault = !column.value(QStringLiteral("default")).isNull();
        const bool autoIncrement = column.value(QStringLiteral("extra")).toString().contains(QStringLiteral("auto_increment"), Qt::CaseInsensitive);

        const QJsonValue value = values.value(name);
        const bool missing = value.isUndefined() || value.isNull() || (value.isString() && value.toString().isEmpty());
        if (!nullable && !hasDefault && !autoIncrement && missing) {
            if (error)
                *error = QStringLiteral("Required field is missing: %1").arg(name);
            return false;
        }
    }
    return true;
}

QStringList MainWindow::primaryKeyColumns() const
{
    QStringList columns;
    for (const QJsonValue &columnValue : m_schema) {
        const QJsonObject column = columnValue.toObject();
        if (column.value(QStringLiteral("primaryKey")).toBool())
            columns.append(column.value(QStringLiteral("name")).toString());
    }
    return columns;
}

QString MainWindow::formatBlobSize(int size) const
{
    if (size < 1024)
        return QStringLiteral("%1 bytes").arg(size);
    if (size < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(QString::number(size / 1024.0, 'f', 1));
    return QStringLiteral("%1 MB").arg(QString::number(size / (1024.0 * 1024.0), 'f', 1));
}

bool MainWindow::isBlobValue(const QJsonValue &value) const
{
    return value.isObject() && value.toObject().value(QStringLiteral("_blob")).toBool(false);
}

void MainWindow::showBlobViewer(const QString &title, const QByteArray &data)
{
    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.resize(760, 560);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *summary = new QLabel(QStringLiteral("Size: %1").arg(formatBlobSize(data.size())), &dialog);
    layout->addWidget(summary);

    QPixmap pixmap;
    if (pixmap.loadFromData(data)) {
        QLabel *imageLabel = new QLabel(&dialog);
        imageLabel->setAlignment(Qt::AlignCenter);
        imageLabel->setPixmap(pixmap.scaled(720, 430, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        layout->addWidget(imageLabel, 1);
    } else {
        QTextEdit *textEdit = new QTextEdit(&dialog);
        textEdit->setReadOnly(true);

        const int sampleSize = qMin(data.size(), 4096);
        int printable = 0;
        for (int i = 0; i < sampleSize; ++i) {
            const uchar ch = static_cast<uchar>(data.at(i));
            if (ch == '\n' || ch == '\r' || ch == '\t' || (ch >= 32 && ch < 127))
                ++printable;
        }

        if (sampleSize > 0 && printable > sampleSize * 8 / 10) {
            textEdit->setPlainText(QString::fromUtf8(data));
        } else {
            const QByteArray preview = data.left(8192).toHex();
            QString hexText;
            for (int i = 0; i < preview.size(); i += 2) {
                if (i > 0) {
                    hexText.append(QLatin1Char(' '));
                    if ((i / 2) % 16 == 0)
                        hexText.append(QLatin1Char('\n'));
                }
                hexText.append(QString::fromLatin1(preview.mid(i, 2)));
            }
            if (data.size() > 8192)
                hexText.append(QStringLiteral("\n\n... preview truncated ..."));
            textEdit->setPlainText(hexText);
        }

        layout->addWidget(textEdit, 1);
    }

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Save), &QPushButton::clicked, &dialog, [this, &data]() {
        const QString fileName = QFileDialog::getSaveFileName(this, QStringLiteral("Save BLOB"));
        if (fileName.isEmpty())
            return;

        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this, QStringLiteral("Save Failed"), file.errorString());
            return;
        }
        file.write(data);
    });
    layout->addWidget(buttons);

    dialog.exec();
}

QString MainWindow::displayText(const QJsonValue &value) const
{
    if (value.isUndefined() || value.isNull())
        return QString();
    if (isBlobValue(value)) {
        const QJsonObject blob = value.toObject();
        if (blob.value(QStringLiteral("isNull")).toBool(false))
            return QStringLiteral("[BLOB NULL]");
        return QStringLiteral("[BLOB %1]").arg(formatBlobSize(blob.value(QStringLiteral("size")).toInt()));
    }
    if (value.isString())
        return value.toString();
    if (value.isBool())
        return value.toBool() ? QStringLiteral("1") : QStringLiteral("0");
    if (value.isDouble())
        return QString::number(value.toDouble(), 'g', 15);
    if (value.isArray())
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    if (value.isObject())
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    return QString();
}

QJsonValue MainWindow::valueFromEditorText(const QString &text) const
{
    if (text.isNull())
        return QJsonValue();
    return text;
}
