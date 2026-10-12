#include "TestSupport.hpp"
#include "domain/plugin/IGeneratorPlugin.hpp"

#include <QtTest>

#include <stdexcept>

using namespace account_recommendations;
using domain::GeneratorRegistry;
using domain::IGeneratorPlugin;
using domain::RegistryError;
using testing::StubGenerator;

namespace
{
class FakePlugin final : public IGeneratorPlugin
{
public:
    enum class Mode { Good, Throwing, Duplicate };
    FakePlugin(Mode mode, int api = domain::kGeneratorApiVersion) : m_mode(mode), m_api(api) {}

    QString pluginId() const override { return QStringLiteral("fake"); }
    QString pluginVersion() const override { return QStringLiteral("0.1.0"); }
    int apiVersion() const noexcept override { return m_api; }
    common::Result<void, RegistryError> registerGenerators(GeneratorRegistry& registry) override
    {
        if (m_mode == Mode::Throwing)
            throw std::runtime_error("boom");
        auto first = registry.registerGenerator(std::make_unique<StubGenerator>(QStringLiteral("from-plugin")));
        if (!first)
            return first;
        if (m_mode == Mode::Duplicate)
            return registry.registerGenerator(std::make_unique<StubGenerator>(QStringLiteral("from-plugin")));
        return common::Result<void, RegistryError>::success();
    }

private:
    Mode m_mode;
    int m_api;
};
} // namespace

class TstPluginBoundary : public QObject
{
    Q_OBJECT

private slots:
    void compatiblePluginRegistersGenerators()
    {
        GeneratorRegistry registry;
        FakePlugin plugin(FakePlugin::Mode::Good);
        QVERIFY(domain::registerPluginGenerators(plugin, registry).hasValue());
        QVERIFY(registry.find(QStringLiteral("from-plugin")) != nullptr);
    }

    void incompatibleApiVersionIsRejectedBeforeRegistration()
    {
        GeneratorRegistry registry;
        FakePlugin plugin(FakePlugin::Mode::Good, domain::kGeneratorApiVersion + 1);
        const auto result = domain::registerPluginGenerators(plugin, registry);
        QVERIFY(!result.hasValue());
        QVERIFY(result.error().message.contains(QStringLiteral("API")));
        QCOMPARE(registry.size(), 0);
    }

    void throwingPluginIsIsolated()
    {
        GeneratorRegistry registry;
        FakePlugin plugin(FakePlugin::Mode::Throwing);
        const auto result = domain::registerPluginGenerators(plugin, registry);
        QVERIFY(!result.hasValue());
        QVERIFY(result.error().message.contains(QStringLiteral("boom")));
    }

    void pluginIdConflictsSurfaceAsRegistryErrors()
    {
        GeneratorRegistry registry;
        FakePlugin plugin(FakePlugin::Mode::Duplicate);
        const auto result = domain::registerPluginGenerators(plugin, registry);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, RegistryError::Code::DuplicateId);
        QCOMPARE(registry.size(), 1); // first registration stays: no transactions (documented)
    }
};

QTEST_APPLESS_MAIN(TstPluginBoundary)
#include "tst_plugin_boundary.moc"
