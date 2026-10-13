#include "domain/generator/GenerationContext.hpp"
#include "domain/generator/GeneratorRegistry.hpp"
#include "domain/generator/IGenerator.hpp"
#include "infrastructure/plugins/RuntimePluginManager.hpp"

#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace account_recommendations;

namespace
{
class TestRandom final : public domain::IRandomProvider
{
public:
    std::uint64_t nextUInt64() override { return 7; }
};

class DuplicateTagGenerator final : public domain::IGenerator
{
public:
    domain::GeneratorMetadata metadata() const override
    {
        domain::GeneratorMetadata md;
        md.id = QStringLiteral("example-tag");
        md.displayName = QStringLiteral("Pre-existing duplicate");
        md.description = QStringLiteral("Test collision.");
        md.category = QStringLiteral("Tests");
        md.version = QStringLiteral("1.0.0");
        md.configurationSchema = domain::ConfigurationSchema{};
        md.outputFields = {{QStringLiteral("tag"), QStringLiteral("Tag"), false}};
        return md;
    }
    common::ValidationResult validate(const domain::GeneratorRequest&) const override { return {}; }
protected:
    common::Result<domain::GenerationSummary, domain::GenerationError>
    generateIntoImpl(const domain::GeneratorRequest&, const domain::GenerationContext&,
                     domain::IResultSink&) const override
    {
        return common::Result<domain::GenerationSummary, domain::GenerationError>::success({});
    }
};
}

class RuntimePluginTests final : public QObject
{
    Q_OBJECT
private slots:
    void loadingIsDisabledByDefault()
    {
        domain::GeneratorRegistry registry;
        infrastructure::RuntimePluginManager manager(registry);
        const auto result = manager.loadFile(QStringLiteral(AR_EXAMPLE_PLUGIN_PATH));
        QVERIFY(!result);
        QCOMPARE(result.error().code, infrastructure::PluginLoadError::Code::Disabled);
        QCOMPARE(registry.size(), qsizetype{0});
    }

    void pathMustBeInsideAnApprovedDirectory()
    {
        QTemporaryDir untrustedDirectory;
        QVERIFY(untrustedDirectory.isValid());
        const QString pluginPath = QStringLiteral(AR_EXAMPLE_PLUGIN_PATH);
        domain::GeneratorRegistry registry;
        infrastructure::RuntimePluginManager manager(registry);
        manager.enableForSession({untrustedDirectory.path()});
        const auto result = manager.loadFile(pluginPath);
        QVERIFY(!result);
        QCOMPARE(result.error().code, infrastructure::PluginLoadError::Code::OutsideTrustedDirectory);
        QCOMPARE(registry.size(), qsizetype{0});
    }

    void rejectsLibraryBeforeInstantiatingWrongIid()
    {
        const QString corePath = QStringLiteral(AR_CORE_LIBRARY_PATH);
        QVERIFY(QFileInfo::exists(corePath));
        domain::GeneratorRegistry registry;
        infrastructure::RuntimePluginManager manager(registry);
        manager.enableForSession({QFileInfo(corePath).absolutePath()});
        const auto result = manager.loadFile(corePath);
        QVERIFY(!result);
        QCOMPARE(result.error().code, infrastructure::PluginLoadError::Code::MissingInterface);
        QCOMPARE(registry.size(), qsizetype{0});
    }

    void pluginRegistrationIsAtomicOnConflict()
    {
        domain::GeneratorRegistry registry;
        QVERIFY(registry.registerGenerator(std::make_unique<DuplicateTagGenerator>()));
        const QString pluginPath = QStringLiteral(AR_EXAMPLE_PLUGIN_PATH);
        infrastructure::RuntimePluginManager manager(registry);
        manager.enableForSession({QFileInfo(pluginPath).absolutePath()});
        const auto result = manager.loadFile(pluginPath);
        QVERIFY(!result);
        QCOMPARE(result.error().code, infrastructure::PluginLoadError::Code::RegistrationFailed);
        QCOMPARE(registry.size(), qsizetype{1});
        QVERIFY(!registry.find(QStringLiteral("example-counter")));
    }

    void loadApprovedExamplePluginAndGenerate()
    {
        domain::GeneratorRegistry registry;
        infrastructure::RuntimePluginManager manager(registry);
        const QString pluginPath = QStringLiteral(AR_EXAMPLE_PLUGIN_PATH);
        manager.enableForSession({QFileInfo(pluginPath).absolutePath()});
        const auto loaded = manager.loadFile(pluginPath);
        QVERIFY2(loaded, loaded ? "" : qPrintable(loaded.error().message));
        QCOMPARE(loaded.value().id, QStringLiteral("example-prefix-plugin"));
        QVERIFY(loaded.value().generatorIds.contains(QStringLiteral("example-tag")));
        QVERIFY(loaded.value().generatorIds.contains(QStringLiteral("example-counter")));

        const auto generator = registry.find(QStringLiteral("example-tag"));
        QVERIFY(generator);
        TestRandom random;
        domain::GenerationContext context(random);
        domain::GeneratorRequest request;
        request.generatorId = QStringLiteral("example-tag");
        request.parameters = QJsonObject{{QStringLiteral("prefix"), QStringLiteral("test-")},
                                         {QStringLiteral("startNumber"), 5}};
        request.count = 3;
        const auto result = generator->generate(request, context);
        QVERIFY2(result, result ? "" : qPrintable(result.error().message));
        QCOMPARE(result.value().records.size(), qsizetype{3});
        QCOMPARE(result.value().records.at(0).value(QStringLiteral("exampleTag")).toString(), QStringLiteral("test-5"));
        QCOMPARE(result.value().records.at(2).value(QStringLiteral("exampleTag")).toString(), QStringLiteral("test-7"));
    }
};

QTEST_MAIN(RuntimePluginTests)
#include "tst_runtime_plugin.moc"
