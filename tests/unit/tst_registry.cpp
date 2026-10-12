#include "TestSupport.hpp"
#include "domain/generator/GeneratorRegistry.hpp"

#include <QtTest>

#include <atomic>
#include <thread>
#include <vector>

using namespace account_recommendations;
using domain::GeneratorRegistry;
using domain::RegistryError;
using testing::StubGenerator;

class TstRegistry : public QObject
{
    Q_OBJECT

private slots:
    void registersAndFinds()
    {
        GeneratorRegistry registry;
        QVERIFY(registry.registerGenerator(std::make_unique<StubGenerator>(QStringLiteral("alpha"))).hasValue());
        QCOMPARE(registry.size(), 1);
        const auto found = registry.find(QStringLiteral("alpha"));
        QVERIFY(found != nullptr);
        QCOMPARE(found->metadata().id, QStringLiteral("alpha"));
        QVERIFY(registry.find(QStringLiteral("missing")) == nullptr);
        QVERIFY(registry.metadataFor(QStringLiteral("alpha")).has_value());
        QVERIFY(!registry.metadataFor(QStringLiteral("missing")).has_value());
    }

    void listPreservesRegistrationOrder()
    {
        GeneratorRegistry registry;
        for (const char* id : {"zeta", "alpha", "mid"})
            QVERIFY(registry.registerGenerator(std::make_unique<StubGenerator>(QLatin1String(id))).hasValue());
        const auto list = registry.list();
        QCOMPARE(list.size(), 3);
        QCOMPARE(list.at(0).id, QStringLiteral("zeta"));
        QCOMPARE(list.at(1).id, QStringLiteral("alpha"));
        QCOMPARE(list.at(2).id, QStringLiteral("mid"));
    }

    void rejectsDuplicateIds()
    {
        GeneratorRegistry registry;
        QVERIFY(registry.registerGenerator(std::make_unique<StubGenerator>(QStringLiteral("dup"))).hasValue());
        auto second = registry.registerGenerator(std::make_unique<StubGenerator>(QStringLiteral("dup")));
        QVERIFY(!second.hasValue());
        QCOMPARE(second.error().code, RegistryError::Code::DuplicateId);
        QCOMPARE(registry.size(), 1);
    }

    void rejectsNullGenerator()
    {
        GeneratorRegistry registry;
        auto result = registry.registerGenerator(nullptr);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, RegistryError::Code::NullGenerator);
    }

    void rejectsInvalidMetadata_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<QString>("version");
        QTest::addColumn<QString>("expectedCode");
        QTest::newRow("empty id") << "" << "1.0.0" << "invalid_id";
        QTest::newRow("uppercase id") << "Password" << "1.0.0" << "invalid_id";
        QTest::newRow("underscore id") << "my_gen" << "1.0.0" << "invalid_id";
        QTest::newRow("trailing dash") << "gen-" << "1.0.0" << "invalid_id";
        QTest::newRow("too long id") << QString(65, QLatin1Char('a')) << "1.0.0" << "invalid_id";
        QTest::newRow("bad version") << "ok-id" << "v1" << "invalid_version";
    }
    void rejectsInvalidMetadata()
    {
        QFETCH(QString, id);
        QFETCH(QString, version);
        QFETCH(QString, expectedCode);
        auto metadata = StubGenerator::makeMetadata(id);
        metadata.version = version;
        GeneratorRegistry registry;
        auto result = registry.registerGenerator(std::make_unique<StubGenerator>(metadata));
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, RegistryError::Code::InvalidMetadata);
        QVERIFY(result.error().validation.containsCode(expectedCode));
        QCOMPARE(registry.size(), 0);
    }

    void rejectsMetadataWithoutOutputFieldsOrBadSchema()
    {
        GeneratorRegistry registry;
        auto noOutputs = StubGenerator::makeMetadata(QStringLiteral("a"));
        noOutputs.outputFields.clear();
        auto result = registry.registerGenerator(std::make_unique<StubGenerator>(noOutputs));
        QVERIFY(!result.hasValue());
        QVERIFY(result.error().validation.containsCode(QStringLiteral("no_output_fields")));

        auto badSchema = StubGenerator::makeMetadata(QStringLiteral("b"));
        badSchema.configurationSchema = domain::ConfigurationSchema(
            {domain::FieldSpec::integer(QStringLiteral("n"), QStringLiteral("N"), 99, 1, 10)});
        result = registry.registerGenerator(std::make_unique<StubGenerator>(badSchema));
        QVERIFY(!result.hasValue());
        QVERIFY(result.error().validation.containsCode(QStringLiteral("invalid_default")));

        auto duplicateOutputs = StubGenerator::makeMetadata(QStringLiteral("c"));
        duplicateOutputs.outputFields.append(duplicateOutputs.outputFields.first());
        result = registry.registerGenerator(std::make_unique<StubGenerator>(duplicateOutputs));
        QVERIFY(result.error().validation.containsCode(QStringLiteral("duplicate_output_field")));
    }

    void unregister()
    {
        GeneratorRegistry registry;
        QVERIFY(registry.registerGenerator(std::make_unique<StubGenerator>(QStringLiteral("gone"))).hasValue());
        QVERIFY(registry.unregisterGenerator(QStringLiteral("gone")));
        QVERIFY(!registry.unregisterGenerator(QStringLiteral("gone")));
        QVERIFY(registry.find(QStringLiteral("gone")) == nullptr);
    }

    void foundGeneratorOutlivesUnregistration()
    {
        GeneratorRegistry registry;
        QVERIFY(registry.registerGenerator(std::make_unique<StubGenerator>(QStringLiteral("held"))).hasValue());
        const auto held = registry.find(QStringLiteral("held"));
        QVERIFY(registry.unregisterGenerator(QStringLiteral("held")));
        QCOMPARE(held->metadata().id, QStringLiteral("held")); // no dangling pointer
    }

    void concurrentReadsDuringRegistration()
    {
        GeneratorRegistry registry;
        QVERIFY(registry.registerGenerator(std::make_unique<StubGenerator>(QStringLiteral("base"))).hasValue());

        std::atomic<bool> stop{false};
        std::atomic<int> readerFailures{0};
        std::vector<std::thread> readers;
        for (int t = 0; t < 4; ++t) {
            readers.emplace_back([&] {
                while (!stop.load()) {
                    const auto found = registry.find(QStringLiteral("base"));
                    if (!found || found->metadata().id != QStringLiteral("base"))
                        ++readerFailures;
                    if (registry.list().isEmpty())
                        ++readerFailures;
                }
            });
        }
        for (int i = 0; i < 200; ++i)
            QVERIFY(registry.registerGenerator(
                        std::make_unique<StubGenerator>(QStringLiteral("gen-%1").arg(i)))
                        .hasValue());
        stop = true;
        for (auto& reader : readers)
            reader.join();
        QCOMPARE(readerFailures.load(), 0);
        QCOMPARE(registry.size(), 201);
    }

    // Shows the extensibility contract: a brand-new generator needs no framework change.
    void newGeneratorIsDiscoverableThroughMetadataOnly()
    {
        GeneratorRegistry registry;
        auto metadata = StubGenerator::makeMetadata(QStringLiteral("future-generator"));
        QVERIFY(registry.registerGenerator(std::make_unique<StubGenerator>(metadata)).hasValue());
        const auto listed = registry.list();
        QCOMPARE(listed.size(), 1);
        QCOMPARE(listed.first().category, QStringLiteral("Test"));
        QCOMPARE(listed.first().configurationSchema.fields().size(), 1);
    }
};

QTEST_APPLESS_MAIN(TstRegistry)
#include "tst_registry.moc"
