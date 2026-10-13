#pragma once

#include "domain/generator/CancellationToken.hpp"
#include "domain/generator/GeneratorRegistry.hpp"
#include "domain/security/SensitiveDataPolicy.hpp"
#include "infrastructure/plugins/RuntimePluginManager.hpp"
#include "infrastructure/random/SecureRandomProvider.hpp"

#include <QMainWindow>
#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QFutureWatcher>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QMenu;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QStandardItemModel;
class QTreeWidget;
class QTreeWidgetItem;
class QTableView;
class QWidget;

namespace account_recommendations::gui
{

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(domain::GeneratorRegistry& registry,
               infrastructure::SecureRandomProvider& random,
               infrastructure::RuntimePluginManager& plugins,
               QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void selectGenerator(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void generate();
    void cancelGeneration();
    void copySelectedRow();
    void exportResults();
    void clearResults();
    void loadPluginFile();
    void showLoadedPlugins();
    void setLightTheme();
    void setDarkTheme();
    void setSystemTheme();

private:
    using BatchResult = common::Result<domain::GeneratorResult, domain::GenerationError>;

    void buildUi();
    void rebuildGeneratorTree(const QString& preserveId = {});
    void showGenerator(const QString& id);
    void rebuildConfigurationForm();
    [[nodiscard]] QWidget* makeEditor(const domain::FieldSpec& field);
    [[nodiscard]] QJsonObject parametersFromForm() const;
    void renderRecords(const QList<domain::GeneratedRecord>& records);
    void setBusy(bool busy);
    void showStatus(const QString& message, int timeoutMs = 5000);
    void populateResultHeaders();

    domain::GeneratorRegistry& m_registry;
    infrastructure::SecureRandomProvider& m_random;
    infrastructure::RuntimePluginManager& m_plugins;
    domain::GeneratorMetadata m_selectedMetadata;
    std::shared_ptr<const domain::IGenerator> m_selectedGenerator;

    QTreeWidget* m_generatorTree = nullptr;
    QLabel* m_generatorTitle = nullptr;
    QLabel* m_generatorDescription = nullptr;
    QWidget* m_formContainer = nullptr;
    QFormLayout* m_formLayout = nullptr;
    QScrollArea* m_formScroll = nullptr;
    QSpinBox* m_countSpin = nullptr;
    QCheckBox* m_uniqueCheck = nullptr;
    QCheckBox* m_revealSensitive = nullptr;
    QTableView* m_resultsTable = nullptr;
    QStandardItemModel* m_resultsModel = nullptr;
    QLabel* m_resultSummary = nullptr;
    QPushButton* m_generateButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
    QPushButton* m_copyButton = nullptr;
    QHash<QString, QWidget*> m_fieldEditors;

    QList<domain::GeneratedRecord> m_lastRawRecords;
    domain::CancellationSource m_cancellation;
    QFutureWatcher<BatchResult>* m_watcher = nullptr;
};

} // namespace account_recommendations::gui
