#include "gui/MainWindow.hpp"

#include "domain/generator/ConfigurationSchema.hpp"

#include <QAction>
#include <QActionGroup>
#include <QAbstractItemView>
#include <QColor>
#include <QFileInfo>
#include <QFont>
#include <QItemSelectionModel>
#include <QStyle>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIODevice>
#include <QPalette>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTableView>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <limits>
#include <exception>

namespace account_recommendations::gui
{
namespace
{
constexpr int kGeneratorIdRole = Qt::UserRole + 1;
constexpr int kMaxInteractivePreviewRows = 10000;

QString jsonValueText(const QJsonValue& value)
{
    if (value.isString()) return value.toString();
    if (value.isBool()) return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (value.isNull()) return QStringLiteral("null");
    if (value.isDouble()) return QString::number(value.toDouble(), 'g', 15);
    return domain::SensitiveDataPolicy::redactionText();
}


} // namespace

MainWindow::MainWindow(domain::GeneratorRegistry& registry,
                       infrastructure::SecureRandomProvider& random,
                       infrastructure::RuntimePluginManager& plugins,
                       QWidget* parent)
    : QMainWindow(parent), m_registry(registry), m_random(random), m_plugins(plugins)
{
    setWindowTitle(QStringLiteral("Account Recommendations"));
    resize(1280, 800);
    buildUi();
    rebuildGeneratorTree();
}

MainWindow::~MainWindow()
{
    // The window owns the watcher while the task captures references to services whose
    // lifetimes are managed by the composition root. Drain the task even if it has finished
    // in the worker but its queued finished() signal has not yet been delivered.
    if (m_watcher) {
        m_cancellation.requestCancel();
        if (m_watcher->isRunning())
            m_watcher->waitForFinished();
        QObject::disconnect(m_watcher, nullptr, this, nullptr);
        delete m_watcher;
        m_watcher = nullptr;
    }
}

void MainWindow::buildUi()
{
    auto* fileMenu = menuBar()->addMenu(tr("&Application"));
    QAction* exportAction = fileMenu->addAction(tr("Export results…"));
    connect(exportAction, &QAction::triggered, this, &MainWindow::exportResults);
    QAction* clearAction = fileMenu->addAction(tr("Clear results"));
    connect(clearAction, &QAction::triggered, this, &MainWindow::clearResults);
    fileMenu->addSeparator();
    QAction* quitAction = fileMenu->addAction(tr("Quit"));
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    auto* pluginsMenu = menuBar()->addMenu(tr("&Plugins"));
    QAction* loadPluginAction = pluginsMenu->addAction(tr("Load native plugin…"));
    connect(loadPluginAction, &QAction::triggered, this, &MainWindow::loadPluginFile);
    QAction* loadedPluginsAction = pluginsMenu->addAction(tr("Loaded plugins…"));
    connect(loadedPluginsAction, &QAction::triggered, this, &MainWindow::showLoadedPlugins);

    auto* viewMenu = menuBar()->addMenu(tr("&Appearance"));
    auto* themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);
    auto* systemTheme = viewMenu->addAction(tr("System theme"));
    systemTheme->setCheckable(true);
    systemTheme->setChecked(true);
    themeGroup->addAction(systemTheme);
    auto* lightTheme = viewMenu->addAction(tr("Light theme"));
    lightTheme->setCheckable(true);
    themeGroup->addAction(lightTheme);
    auto* darkTheme = viewMenu->addAction(tr("Dark theme"));
    darkTheme->setCheckable(true);
    themeGroup->addAction(darkTheme);
    connect(systemTheme, &QAction::triggered, this, &MainWindow::setSystemTheme);
    connect(lightTheme, &QAction::triggered, this, &MainWindow::setLightTheme);
    connect(darkTheme, &QAction::triggered, this, &MainWindow::setDarkTheme);

    auto* root = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(root);
    auto* splitter = new QSplitter(Qt::Horizontal, root);
    splitter->setChildrenCollapsible(false);

    auto* left = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(left);
    auto* sideTitle = new QLabel(tr("Generators"), left);
    QFont titleFont = sideTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    sideTitle->setFont(titleFont);
    m_generatorTree = new QTreeWidget(left);
    m_generatorTree->setObjectName(QStringLiteral("generatorTree"));
    m_generatorTree->setHeaderHidden(true);
    m_generatorTree->setMinimumWidth(210);
    connect(m_generatorTree, &QTreeWidget::currentItemChanged,
            this, &MainWindow::selectGenerator);
    leftLayout->addWidget(sideTitle);
    leftLayout->addWidget(m_generatorTree, 1);

    auto* right = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(right);
    m_generatorTitle = new QLabel(tr("Choose a generator"), right);
    QFont headingFont = m_generatorTitle->font();
    headingFont.setBold(true);
    headingFont.setPointSize(headingFont.pointSize() + 4);
    m_generatorTitle->setFont(headingFont);
    m_generatorDescription = new QLabel(tr("Select a generator from the sidebar to configure it."), right);
    m_generatorDescription->setWordWrap(true);

    auto* configPanel = new QWidget(right);
    auto* configLayout = new QVBoxLayout(configPanel);
    auto* configHeading = new QLabel(tr("Configuration"), configPanel);
    configHeading->setStyleSheet(QStringLiteral("font-weight: 600;"));
    m_formScroll = new QScrollArea(configPanel);
    m_formScroll->setWidgetResizable(true);
    m_formContainer = new QWidget(m_formScroll);
    m_formLayout = new QFormLayout(m_formContainer);
    m_formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_formScroll->setWidget(m_formContainer);

    auto* batchRow = new QWidget(configPanel);
    auto* batchLayout = new QHBoxLayout(batchRow);
    batchLayout->setContentsMargins(0, 0, 0, 0);
    batchLayout->addWidget(new QLabel(tr("Quantity"), batchRow));
    m_countSpin = new QSpinBox(batchRow);
    m_countSpin->setRange(1, kMaxInteractivePreviewRows);
    m_countSpin->setValue(10);
    m_countSpin->setMaximumWidth(130);
    m_countSpin->setToolTip(tr("Interactive preview is limited to %1 records to keep the desktop responsive.").arg(kMaxInteractivePreviewRows));
    batchLayout->addWidget(m_countSpin);
    m_uniqueCheck = new QCheckBox(tr("Unique within batch"), batchRow);
    batchLayout->addWidget(m_uniqueCheck);
    batchLayout->addStretch(1);

    configLayout->addWidget(configHeading);
    configLayout->addWidget(m_formScroll, 1);
    configLayout->addWidget(batchRow);

    auto* resultsPanel = new QWidget(right);
    auto* resultsLayout = new QVBoxLayout(resultsPanel);
    resultsLayout->setContentsMargins(0, 0, 0, 0);
    auto* resultsHeader = new QWidget(resultsPanel);
    auto* resultsHeaderLayout = new QHBoxLayout(resultsHeader);
    resultsHeaderLayout->setContentsMargins(0, 0, 0, 0);
    auto* resultsTitle = new QLabel(tr("Results"), resultsHeader);
    resultsTitle->setStyleSheet(QStringLiteral("font-weight: 600;"));
    m_revealSensitive = new QCheckBox(tr("Reveal sensitive values"), resultsHeader);
    m_revealSensitive->setObjectName(QStringLiteral("revealSensitiveValues"));
    m_revealSensitive->setChecked(false);
    connect(m_revealSensitive, &QCheckBox::toggled, this, [this] {
        renderRecords(m_lastRawRecords);
    });
    resultsHeaderLayout->addWidget(resultsTitle);
    resultsHeaderLayout->addStretch(1);
    resultsHeaderLayout->addWidget(m_revealSensitive);

    m_resultsTable = new QTableView(resultsPanel);
    m_resultsModel = new QStandardItemModel(this);
    m_resultsModel->setObjectName(QStringLiteral("resultsModel"));
    m_resultsTable->setObjectName(QStringLiteral("resultsTable"));
    m_resultsTable->setModel(m_resultsModel);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTable->setAlternatingRowColors(true);
    m_resultsTable->setSortingEnabled(false); // row indexes correspond to m_lastRawRecords
    m_resultsTable->horizontalHeader()->setStretchLastSection(true);
    m_resultsTable->verticalHeader()->setVisible(false);

    auto* actionRow = new QWidget(resultsPanel);
    auto* actionLayout = new QHBoxLayout(actionRow);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    m_generateButton = new QPushButton(tr("Generate"), actionRow);
    m_generateButton->setObjectName(QStringLiteral("generateButton"));
    m_generateButton->setDefault(true);
    m_cancelButton = new QPushButton(tr("Cancel"), actionRow);
    m_cancelButton->setEnabled(false);
    m_copyButton = new QPushButton(tr("Copy selected row…"), actionRow);
    auto* clearResultsButton = new QPushButton(tr("Clear"), actionRow);
    connect(m_generateButton, &QPushButton::clicked, this, &MainWindow::generate);
    connect(m_cancelButton, &QPushButton::clicked, this, &MainWindow::cancelGeneration);
    connect(m_copyButton, &QPushButton::clicked, this, &MainWindow::copySelectedRow);
    connect(clearResultsButton, &QPushButton::clicked, this, &MainWindow::clearResults);
    actionLayout->addWidget(m_generateButton);
    actionLayout->addWidget(m_cancelButton);
    actionLayout->addWidget(m_copyButton);
    actionLayout->addStretch(1);
    actionLayout->addWidget(clearResultsButton);

    m_resultSummary = new QLabel(tr("No results yet."), resultsPanel);
    resultsLayout->addWidget(resultsHeader);
    resultsLayout->addWidget(m_resultsTable, 1);
    resultsLayout->addWidget(m_resultSummary);
    resultsLayout->addWidget(actionRow);

    auto* vertical = new QSplitter(Qt::Vertical, right);
    vertical->addWidget(configPanel);
    vertical->addWidget(resultsPanel);
    vertical->setStretchFactor(0, 3);
    vertical->setStretchFactor(1, 4);

    rightLayout->addWidget(m_generatorTitle);
    rightLayout->addWidget(m_generatorDescription);
    rightLayout->addWidget(vertical, 1);
    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({260, 1000});
    rootLayout->addWidget(splitter);
    setCentralWidget(root);
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::rebuildGeneratorTree(const QString& preserveId)
{
    m_generatorTree->clear();
    QHash<QString, QTreeWidgetItem*> categories;
    QTreeWidgetItem* restoreItem = nullptr;
    for (const domain::GeneratorMetadata& metadata : m_registry.list()) {
        QTreeWidgetItem* category = categories.value(metadata.category, nullptr);
        if (!category) {
            category = new QTreeWidgetItem(m_generatorTree, {metadata.category});
            category->setFlags(category->flags() & ~Qt::ItemIsSelectable);
            categories.insert(metadata.category, category);
        }
        auto* item = new QTreeWidgetItem(category, {metadata.displayName});
        item->setData(0, kGeneratorIdRole, metadata.id);
        item->setToolTip(0, metadata.description);
        if (metadata.id == preserveId)
            restoreItem = item;
    }
    m_generatorTree->expandAll();
    if (!restoreItem) {
        for (int i = 0; i < m_generatorTree->topLevelItemCount() && !restoreItem; ++i) {
            if (m_generatorTree->topLevelItem(i)->childCount() > 0)
                restoreItem = m_generatorTree->topLevelItem(i)->child(0);
        }
    }
    if (restoreItem)
        m_generatorTree->setCurrentItem(restoreItem);
}

void MainWindow::selectGenerator(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    Q_UNUSED(previous)
    if (!current)
        return;
    const QString id = current->data(0, kGeneratorIdRole).toString();
    if (id.isEmpty())
        return;
    showGenerator(id);
}

void MainWindow::showGenerator(const QString& id)
{
    auto generator = m_registry.find(id);
    auto metadata = m_registry.metadataFor(id);
    if (!generator || !metadata)
        return;
    m_selectedGenerator = std::move(generator);
    m_selectedMetadata = *metadata;
    m_generatorTitle->setText(metadata->displayName);
    m_generatorDescription->setText(metadata->description);
    m_countSpin->setEnabled(metadata->supportsBatch);
    m_countSpin->setValue(metadata->supportsBatch ? 10 : 1);
    m_uniqueCheck->setEnabled(metadata->supportsUniqueness);
    m_uniqueCheck->setChecked(false);
    m_revealSensitive->setChecked(false);
    rebuildConfigurationForm();
    populateResultHeaders();
    clearResults();
    showStatus(tr("Selected %1 (%2)").arg(metadata->displayName, metadata->id));
}

void MainWindow::rebuildConfigurationForm()
{
    auto* old = m_formScroll->takeWidget();
    delete old;
    m_formContainer = new QWidget(m_formScroll);
    m_formLayout = new QFormLayout(m_formContainer);
    m_formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_fieldEditors.clear();

    QString currentGroup;
    for (const domain::FieldSpec& field : m_selectedMetadata.configurationSchema.fields()) {
        if (!field.group.isEmpty() && field.group != currentGroup) {
            currentGroup = field.group;
            auto* label = new QLabel(currentGroup, m_formContainer);
            label->setStyleSheet(QStringLiteral("font-weight: 600; margin-top: 8px;"));
            m_formLayout->addRow(label);
        }
        QWidget* editor = makeEditor(field);
        if (!field.description.isEmpty())
            editor->setToolTip(field.description);
        m_fieldEditors.insert(field.key, editor);
        m_formLayout->addRow(field.label, editor);
    }
    if (m_fieldEditors.isEmpty())
        m_formLayout->addRow(new QLabel(tr("This generator has no configurable fields."), m_formContainer));
    m_formScroll->setWidget(m_formContainer);
}

QWidget* MainWindow::makeEditor(const domain::FieldSpec& field)
{
    switch (field.type) {
    case domain::FieldType::Integer: {
        auto* editor = new QSpinBox(m_formContainer);
        const qint64 lower = field.minimum.value_or(std::numeric_limits<int>::min());
        const qint64 upper = field.maximum.value_or(std::numeric_limits<int>::max());
        editor->setRange(static_cast<int>(std::clamp<qint64>(lower, std::numeric_limits<int>::min(), std::numeric_limits<int>::max())),
                         static_cast<int>(std::clamp<qint64>(upper, std::numeric_limits<int>::min(), std::numeric_limits<int>::max())));
        editor->setValue(static_cast<int>(field.defaultValue.toInteger()));
        return editor;
    }
    case domain::FieldType::Boolean: {
        auto* editor = new QCheckBox(m_formContainer);
        editor->setChecked(field.defaultValue.toBool());
        return editor;
    }
    case domain::FieldType::Enumeration: {
        auto* editor = new QComboBox(m_formContainer);
        editor->addItems(field.allowedValues);
        const int defaultIndex = editor->findText(field.defaultValue.toString());
        if (defaultIndex >= 0)
            editor->setCurrentIndex(defaultIndex);
        return editor;
    }
    case domain::FieldType::String: {
        auto* editor = new QLineEdit(field.defaultValue.toString(), m_formContainer);
        editor->setMaxLength(static_cast<int>(std::min<qsizetype>(field.maxLength, std::numeric_limits<int>::max())));
        return editor;
    }
    }
    return new QLineEdit(m_formContainer);
}

QJsonObject MainWindow::parametersFromForm() const
{
    QJsonObject parameters;
    for (const domain::FieldSpec& field : m_selectedMetadata.configurationSchema.fields()) {
        QWidget* editor = m_fieldEditors.value(field.key, nullptr);
        if (!editor)
            continue;
        switch (field.type) {
        case domain::FieldType::Integer:
            if (auto* spin = qobject_cast<QSpinBox*>(editor))
                parameters.insert(field.key, spin->value());
            break;
        case domain::FieldType::Boolean:
            if (auto* check = qobject_cast<QCheckBox*>(editor))
                parameters.insert(field.key, check->isChecked());
            break;
        case domain::FieldType::Enumeration:
            if (auto* combo = qobject_cast<QComboBox*>(editor))
                parameters.insert(field.key, combo->currentText());
            break;
        case domain::FieldType::String:
            if (auto* line = qobject_cast<QLineEdit*>(editor))
                parameters.insert(field.key, line->text());
            break;
        }
    }
    return parameters;
}

void MainWindow::generate()
{
    if (!m_selectedGenerator)
        return;
    // Keep the guard until the GUI thread has processed finished(), not merely until
    // the worker thread stops. Otherwise a second job can overwrite the watcher pointer.
    if (m_watcher) {
        showStatus(tr("A generation job is already running or awaiting result delivery."));
        return;
    }

    domain::GeneratorRequest request;
    request.generatorId = m_selectedMetadata.id;
    request.parameters = parametersFromForm();
    request.count = m_selectedMetadata.supportsBatch ? m_countSpin->value() : 1;
    request.unique = m_uniqueCheck->isEnabled() && m_uniqueCheck->isChecked();

    common::ValidationResult validation;
    try {
        validation = m_selectedGenerator->validate(request);
    } catch (...) {
        QMessageBox::warning(this, tr("Generator error"),
                             tr("The selected generator threw an exception during validation."));
        return;
    }
    if (!validation.isValid()) {
        QMessageBox::warning(this, tr("Invalid configuration"), validation.summary());
        return;
    }

    m_cancellation = domain::CancellationSource{};
    const auto token = m_cancellation.token();
    const auto generator = m_selectedGenerator;
    auto* watcher = new QFutureWatcher<BatchResult>(this);
    m_watcher = watcher;
    connect(watcher, &QFutureWatcher<BatchResult>::finished, this, [this, watcher] {
        const BatchResult result = watcher->result();
        watcher->deleteLater();
        m_watcher = nullptr;
        setBusy(false);
        if (!result) {
            showStatus(result.error().message, 10000);
            if (result.error().code != domain::GenerationError::Code::Cancelled)
                QMessageBox::warning(this, tr("Generation failed"), result.error().message);
            return;
        }
        m_lastRawRecords = result.value().records;
        renderRecords(m_lastRawRecords);
        m_resultSummary->setText(tr("Generated %1 record(s).")
                                      .arg(result.value().summary.produced));
        showStatus(tr("Generation completed."));
    });
    setBusy(true);
    m_resultSummary->setText(tr("Generating…"));
    auto limits = common::ApplicationLimits::defaults();
    // The desktop results table is an interactive preview, not a bulk export pipeline.
    limits.maxBatchSize = std::min<std::size_t>(limits.maxBatchSize,
                                                static_cast<std::size_t>(kMaxInteractivePreviewRows));
    auto* random = &m_random;
    watcher->setFuture(QtConcurrent::run([generator, request, token, limits, random] {
        try {
            const domain::GenerationContext context(*random, token, limits);
            return generator->generate(request, context);
        } catch (...) {
            // Third-party generators are native code. Catch ordinary C++ exceptions at the task
            // boundary, without displaying exception text that might contain generated data.
            return BatchResult::failure(domain::GenerationError::internal(
                QStringLiteral("The generator threw an exception. No results were released.")));
        }
    }));
}

void MainWindow::cancelGeneration()
{
    if (m_watcher && m_watcher->isRunning()) {
        m_cancellation.requestCancel();
        m_cancelButton->setEnabled(false);
        showStatus(tr("Cancellation requested…"));
    }
}

void MainWindow::renderRecords(const QList<domain::GeneratedRecord>& records)
{
    m_resultsModel->clear();
    populateResultHeaders();
    const domain::SensitiveDataSettings settings{false, m_revealSensitive && m_revealSensitive->isChecked()};
    const auto policy = domain::SensitiveDataPolicy::forGenerator(m_selectedMetadata, settings);
    for (const domain::GeneratedRecord& raw : records) {
        const QJsonObject shown = policy.sanitize(raw, domain::OutputChannel::Display);
        QList<QStandardItem*> items;
        for (const domain::OutputFieldSpec& field : m_selectedMetadata.outputFields) {
            auto* item = new QStandardItem(jsonValueText(shown.value(field.key)));
            item->setEditable(false);
            if (field.sensitive && !settings.revealOnDisplay)
                item->setToolTip(tr("Sensitive value hidden. Use the reveal control to display it."));
            items.append(item);
        }
        m_resultsModel->appendRow(items);
    }
    m_resultsTable->resizeColumnsToContents();
    m_resultSummary->setText(records.isEmpty() ? tr("No results yet.")
                                               : tr("%1 record(s) available.").arg(records.size()));
}

void MainWindow::populateResultHeaders()
{
    m_resultsModel->setColumnCount(static_cast<int>(m_selectedMetadata.outputFields.size()));
    QStringList headers;
    for (const domain::OutputFieldSpec& field : m_selectedMetadata.outputFields)
        headers.append(field.label);
    m_resultsModel->setHorizontalHeaderLabels(headers);
}

void MainWindow::setBusy(bool busy)
{
    m_generateButton->setEnabled(!busy);
    m_cancelButton->setEnabled(busy);
    m_generatorTree->setEnabled(!busy);
    m_countSpin->setEnabled(!busy && m_selectedMetadata.supportsBatch);
    m_uniqueCheck->setEnabled(!busy && m_selectedMetadata.supportsUniqueness);
}

void MainWindow::copySelectedRow()
{
    if (!m_selectedMetadata.id.size() || m_lastRawRecords.isEmpty())
        return;
    const QModelIndexList rows = m_resultsTable->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        QMessageBox::information(this, tr("Copy results"), tr("Select a result row first."));
        return;
    }
    const int row = rows.front().row();
    if (row < 0 || static_cast<qsizetype>(row) >= m_lastRawRecords.size())
        return;

    const QJsonObject raw = m_lastRawRecords.at(row);
    const auto policy = domain::SensitiveDataPolicy::forGenerator(m_selectedMetadata);
    domain::ReleaseAuthorization authorization;
    if (policy.requiresConfirmation(raw, domain::OutputChannel::Clipboard)) {
        const auto answer = QMessageBox::warning(
            this, tr("Copy sensitive data?"),
            tr("The selected result contains sensitive or undeclared fields. Copying it exposes the value to the system clipboard and other applications. The clipboard will be cleared after 30 seconds if it has not changed. Continue?"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes)
            return;
        authorization = domain::ReleaseAuthorization::userConfirmed(domain::OutputChannel::Clipboard);
    }
    const QJsonObject clipboardRecord = policy.sanitize(raw, domain::OutputChannel::Clipboard, authorization);
    QStringList values;
    for (const domain::OutputFieldSpec& field : m_selectedMetadata.outputFields)
        values.append(jsonValueText(clipboardRecord.value(field.key)));
    const QString text = values.join(QLatin1Char('\t'));
    QApplication::clipboard()->setText(text);
    QPointer<QClipboard> clipboard = QApplication::clipboard();
    QTimer::singleShot(30000, this, [clipboard, text] {
        if (clipboard && clipboard->text() == text)
            clipboard->clear();
    });
    showStatus(tr("Selected row copied. Clipboard auto-clear is best effort."));
}

void MainWindow::exportResults()
{
    if (m_lastRawRecords.isEmpty()) {
        QMessageBox::information(this, tr("Export results"), tr("There are no results to export."));
        return;
    }
    QString selectedFilter;
    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Export results"), QDir::homePath() + QStringLiteral("/generated-results.json"),
        tr("JSON (*.json);;CSV (*.csv);;Text / TSV (*.txt *.tsv)"), &selectedFilter);
    if (filePath.isEmpty())
        return;
    if (QFileInfo(filePath).suffix().isEmpty()) {
        if (selectedFilter.startsWith(QStringLiteral("JSON")))
            filePath += QStringLiteral(".json");
        else if (selectedFilter.startsWith(QStringLiteral("CSV")))
            filePath += QStringLiteral(".csv");
        else
            filePath += QStringLiteral(".txt");
    }
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix != QStringLiteral("json") && suffix != QStringLiteral("csv")
        && suffix != QStringLiteral("txt") && suffix != QStringLiteral("tsv")) {
        QMessageBox::warning(this, tr("Unsupported format"),
                             tr("Choose a .json, .csv, .txt, or .tsv file extension."));
        return;
    }

    const auto policy = domain::SensitiveDataPolicy::forGenerator(m_selectedMetadata);
    domain::ReleaseAuthorization authorization;
    if (policy.requiresConfirmation(m_lastRawRecords, domain::OutputChannel::Export)) {
        const auto answer = QMessageBox::warning(
            this, tr("Export sensitive data?"),
            tr("The results contain sensitive or undeclared fields. Exporting may store plaintext in a file accessible to other applications or users. Continue?"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes)
            return;
        authorization = domain::ReleaseAuthorization::userConfirmed(domain::OutputChannel::Export);
    }
    const auto records = policy.sanitize(m_lastRawRecords, domain::OutputChannel::Export, authorization);
    QByteArray bytes;
    if (suffix == QStringLiteral("json")) {
        QJsonArray array;
        for (const QJsonObject& record : records)
            array.append(record);
        bytes = QJsonDocument(array).toJson(QJsonDocument::Indented);
    } else {
        QStringList headers;
        for (const domain::OutputFieldSpec& field : m_selectedMetadata.outputFields)
            headers.append(field.label);
        QStringList lines;
        const auto escapeCsv = [](QString cell) {
            cell.replace(QLatin1Char('"'), QStringLiteral("\"\""));
            if (cell.contains(QLatin1Char(',')) || cell.contains(QLatin1Char('"'))
                || cell.contains(QLatin1Char('\n')) || cell.contains(QLatin1Char('\r')))
                cell = QLatin1Char('"') + cell + QLatin1Char('"');
            return cell;
        };
        if (suffix == QStringLiteral("csv")) {
            for (QString& header : headers) header = escapeCsv(header);
            lines.append(headers.join(QLatin1Char(',')));
            for (const QJsonObject& record : records) {
                QStringList cells;
                for (const domain::OutputFieldSpec& field : m_selectedMetadata.outputFields)
                    cells.append(escapeCsv(jsonValueText(record.value(field.key))));
                lines.append(cells.join(QLatin1Char(',')));
            }
        } else {
            lines.append(headers.join(QLatin1Char('\t')));
            for (const QJsonObject& record : records) {
                QStringList cells;
                for (const domain::OutputFieldSpec& field : m_selectedMetadata.outputFields)
                    cells.append(jsonValueText(record.value(field.key)).replace(QLatin1Char('\t'), QLatin1Char(' '))
                                      .replace(QLatin1Char('\r'), QLatin1Char(' '))
                                      .replace(QLatin1Char('\n'), QLatin1Char(' ')));
                lines.append(cells.join(QLatin1Char('\t')));
            }
        }
        bytes = lines.join(QLatin1Char('\n')).toUtf8();
    }

    QSaveFile file(filePath);
    const bool opened = file.open(QIODevice::WriteOnly);
    const qint64 written = opened ? file.write(bytes) : qint64{-1};
    if (!opened || written < 0 || written != static_cast<qint64>(bytes.size()) || !file.commit()) {
        QMessageBox::warning(this, tr("Export failed"),
                             tr("The result file could not be written atomically. Check the path and available disk space."));
        return;
    }
    showStatus(tr("Exported %1 record(s). Sensitive values were released only after confirmation.")
                   .arg(records.size()), 8000);
}

void MainWindow::clearResults()
{
    m_lastRawRecords.clear();
    m_resultsModel->clear();
    populateResultHeaders();
    m_resultSummary->setText(tr("No results yet."));
}

void MainWindow::loadPluginFile()
{
    const QString selected = QFileDialog::getOpenFileName(
        this, tr("Select native generator plugin"), QDir::homePath(),
        tr("Shared libraries (*.dll *.so *.dylib);;All files (*)"));
    if (selected.isEmpty())
        return;

    const auto answer = QMessageBox::warning(
        this, tr("Load native code?"),
        tr("A native plugin runs with the same privileges as Account Recommendations. It can read files, access the network, and execute arbitrary code. Only load a plugin from a source you trust. This permission applies for this application session; the plugin will remain loaded until the process exits.\n\nLoad:\n%1")
            .arg(QFileInfo(selected).fileName()),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes)
        return;

    m_plugins.enableForSession({QFileInfo(selected).absolutePath()});
    const auto loaded = m_plugins.loadFile(selected);
    if (!loaded) {
        QMessageBox::warning(this, tr("Plugin load failed"), loaded.error().message);
        showStatus(tr("Plugin load failed."));
        return;
    }
    rebuildGeneratorTree(m_selectedMetadata.id);
    QMessageBox::information(this, tr("Plugin loaded"),
        tr("Loaded %1 v%2. Registered generators: %3\n\nThe library remains loaded until the application exits.")
            .arg(loaded.value().id, loaded.value().version,
                 loaded.value().generatorIds.join(QStringLiteral(", "))));
    showStatus(tr("Plugin loaded: %1").arg(loaded.value().id));
}

void MainWindow::showLoadedPlugins()
{
    const auto plugins = m_plugins.loadedPlugins();
    if (plugins.isEmpty()) {
        QMessageBox::information(this, tr("Loaded plugins"), tr("No runtime plugins have been loaded."));
        return;
    }
    QStringList lines;
    for (const auto& plugin : plugins) {
        lines.append(QStringLiteral("%1 v%2\n  %3\n  Generators: %4")
                         .arg(plugin.id, plugin.version, plugin.absolutePath,
                              plugin.generatorIds.join(QStringLiteral(", "))));
    }
    QMessageBox::information(this, tr("Loaded plugins"), lines.join(QStringLiteral("\n\n")));
}

void MainWindow::setLightTheme()
{
    qApp->setPalette(style()->standardPalette());
}

void MainWindow::setDarkTheme()
{
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(38, 40, 44));
    palette.setColor(QPalette::WindowText, QColor(235, 235, 235));
    palette.setColor(QPalette::Base, QColor(27, 29, 32));
    palette.setColor(QPalette::AlternateBase, QColor(45, 48, 53));
    palette.setColor(QPalette::ToolTipBase, QColor(45, 48, 53));
    palette.setColor(QPalette::ToolTipText, QColor(245, 245, 245));
    palette.setColor(QPalette::Text, QColor(235, 235, 235));
    palette.setColor(QPalette::Button, QColor(51, 54, 60));
    palette.setColor(QPalette::ButtonText, QColor(245, 245, 245));
    palette.setColor(QPalette::Highlight, QColor(75, 115, 175));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    qApp->setPalette(palette);
}

void MainWindow::setSystemTheme()
{
    qApp->setPalette(style()->standardPalette());
}

void MainWindow::showStatus(const QString& message, int timeoutMs)
{
    statusBar()->showMessage(message, timeoutMs);
}

} // namespace account_recommendations::gui
