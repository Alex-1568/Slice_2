#include "gui/MainWindow.hpp"

#include "domain/generator/GeneratorRegistry.hpp"
#include "generators/identity/registerIdentityGenerators.hpp"
#include "generators/password/PasswordGenerator.hpp"
#include "domain/security/SensitiveDataPolicy.hpp"

#include <QCheckBox>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTreeWidget>
#include <QtTest>

#include <memory>

using namespace account_recommendations;

class GuiSmokeTests final : public QObject
{
    Q_OBJECT
private slots:
    void metadataDrivenFormGeneratesARecord()
    {
        domain::GeneratorRegistry registry;
        QVERIFY(registry.registerGenerator(std::make_unique<generators::password::PasswordGenerator>()));
        QVERIFY(generators::identity::registerIdentityGenerators(registry));
        infrastructure::SecureRandomProvider random;
        const auto probe = random.probe();
        QVERIFY2(probe, probe ? "" : qPrintable(probe.error().description));
        infrastructure::RuntimePluginManager plugins(registry);
        gui::MainWindow window(registry, random, plugins);
        window.show();

        auto* tree = window.findChild<QTreeWidget*>(QStringLiteral("generatorTree"));
        auto* results = window.findChild<QStandardItemModel*>(QStringLiteral("resultsModel"));
        QVERIFY(tree);
        QVERIFY(results);

        QTreeWidgetItem* target = nullptr;
        for (int category = 0; category < tree->topLevelItemCount(); ++category) {
            QTreeWidgetItem* parent = tree->topLevelItem(category);
            for (int row = 0; row < parent->childCount(); ++row) {
                auto* candidate = parent->child(row);
                if (candidate->data(0, Qt::UserRole + 1).toString() == QStringLiteral("account-id")) {
                    target = candidate;
                    break;
                }
            }
            if (target) break;
        }
        QVERIFY(target);
        tree->setCurrentItem(target);
        QVERIFY(QMetaObject::invokeMethod(&window, "generate", Qt::DirectConnection));
        QTRY_VERIFY_WITH_TIMEOUT(results->rowCount() > 0, 8000);
        QCOMPARE(results->columnCount(), 1);
    }

    void sensitivePasswordIsMaskedUntilExplicitReveal()
    {
        domain::GeneratorRegistry registry;
        QVERIFY(registry.registerGenerator(std::make_unique<generators::password::PasswordGenerator>()));
        QVERIFY(generators::identity::registerIdentityGenerators(registry));
        infrastructure::SecureRandomProvider random;
        const auto probe = random.probe();
        QVERIFY2(probe, probe ? "" : qPrintable(probe.error().description));
        infrastructure::RuntimePluginManager plugins(registry);
        gui::MainWindow window(registry, random, plugins);
        window.show();

        auto* tree = window.findChild<QTreeWidget*>(QStringLiteral("generatorTree"));
        auto* results = window.findChild<QStandardItemModel*>(QStringLiteral("resultsModel"));
        auto* reveal = window.findChild<QCheckBox*>(QStringLiteral("revealSensitiveValues"));
        auto* count = window.findChild<QSpinBox*>();
        QVERIFY(tree);
        QVERIFY(results);
        QVERIFY(reveal);
        QVERIFY(count);

        QTreeWidgetItem* target = nullptr;
        for (int category = 0; category < tree->topLevelItemCount(); ++category) {
            QTreeWidgetItem* parent = tree->topLevelItem(category);
            for (int row = 0; row < parent->childCount(); ++row) {
                auto* candidate = parent->child(row);
                if (candidate->data(0, Qt::UserRole + 1).toString() == QStringLiteral("password")) {
                    target = candidate;
                    break;
                }
            }
            if (target) break;
        }
        QVERIFY(target);
        tree->setCurrentItem(target);
        count->setValue(1);
        QVERIFY(QMetaObject::invokeMethod(&window, "generate", Qt::DirectConnection));
        QTRY_VERIFY_WITH_TIMEOUT(results->rowCount() == 1, 8000);
        QCOMPARE(results->columnCount(), 4);
        const QModelIndex passwordCell = results->index(0, 0);
        QCOMPARE(results->data(passwordCell).toString(), domain::SensitiveDataPolicy::maskText());

        reveal->setChecked(true);
        QTRY_VERIFY_WITH_TIMEOUT(results->data(passwordCell).toString()
                                     != domain::SensitiveDataPolicy::maskText(), 1000);
        QVERIFY(!results->data(passwordCell).toString().isEmpty());
    }
};

QTEST_MAIN(GuiSmokeTests)
#include "tst_gui_smoke.moc"
