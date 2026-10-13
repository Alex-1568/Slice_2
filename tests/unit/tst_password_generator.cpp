#include "TestSupport.hpp"
#include "domain/generator/GeneratorRegistry.hpp"
#include "generators/password/PasswordGenerator.hpp"
#include "testing/DeterministicRandomProvider.hpp"
#include "infrastructure/random/SecureRandomProvider.hpp"

#include <QtTest>

#include <atomic>
#include <set>
#include <thread>
#include <vector>

using namespace account_recommendations;
using domain::CompletionStatus;
using domain::GenerationContext;
using domain::GenerationError;
using generators::password::PasswordGenerator;
using testing::DeterministicRandomProvider;
using infrastructure::SecureRandomProvider;
using testing::passwordRequest;

namespace
{
std::u32string codePoints(const QJsonObject& record)
{
    const QList<uint> ucs4 = record.value(QStringLiteral("password")).toString().toUcs4();
    return std::u32string(ucs4.begin(), ucs4.end());
}
} // namespace

class TstPasswordGenerator : public QObject
{
    Q_OBJECT

private slots:
    void endToEndThroughTheRegistry()
    {
        domain::GeneratorRegistry registry;
        QVERIFY(registry.registerGenerator(std::make_unique<PasswordGenerator>()).hasValue());
        const auto generator = registry.find(QStringLiteral("password"));
        QVERIFY(generator != nullptr);

        SecureRandomProvider secure;
        const GenerationContext context(secure);
        const auto result = generator->generate(passwordRequest({}, 5), context);
        QVERIFY2(result.hasValue(), qPrintable(result.hasValue() ? QString() : result.error().message));
        const auto& records = result.value().records;
        QCOMPARE(records.size(), 5);
        QCOMPARE(result.value().summary.produced, qsizetype{5});
        QCOMPARE(result.value().summary.status, CompletionStatus::Completed);
        const auto md = PasswordGenerator().metadata();
        for (const auto& record : records) {
            QVERIFY2(domain::validateRecord(md, record).isValid(), "password records must be flat and fully declared");
            QCOMPARE(codePoints(record).size(), std::size_t{24});
            QVERIFY(record.value(QStringLiteral("guaranteedEntropyBits")).toDouble() > 100.0);
            QCOMPARE(record.value(QStringLiteral("entropyQuality")).toString(), QStringLiteral("exact"));
            QVERIFY(!record.contains(QStringLiteral("strength"))); // a configuration is not a password verdict
            QCOMPARE(record.keys().size(), 4);
        }
    }

    void specificationExampleWithMinimumsOfTwo()
    {
        SecureRandomProvider secure;
        const GenerationContext context(secure);
        const PasswordGenerator generator;
        const auto result = generator.generate(
            passwordRequest({{"length", 20}, {"minimumUppercase", 2}, {"minimumLowercase", 2},
                             {"minimumDigits", 2}, {"minimumSymbols", 2}},
                            3),
            context);
        QVERIFY(result.hasValue());
        QCOMPARE(result.value().records.size(), 3);
        for (const auto& record : result.value().records) {
            const QString pw = record.value(QStringLiteral("password")).toString();
            int upper = 0, lower = 0, digits = 0, symbols = 0;
            for (QChar c : pw) {
                upper += (c >= QLatin1Char('A') && c <= QLatin1Char('Z'));
                lower += (c >= QLatin1Char('a') && c <= QLatin1Char('z'));
                digits += (c >= QLatin1Char('0') && c <= QLatin1Char('9'));
                symbols += QStringLiteral("!@#$%^&*()-_=+[]{}:;,.?/~").contains(c);
            }
            QCOMPARE(pw.size(), 20);
            QVERIFY(upper >= 2 && lower >= 2 && digits >= 2 && symbols >= 2);
            QCOMPARE(upper + lower + digits + symbols, 20);
        }
    }

    void batchCountIsExact_data()
    {
        QTest::addColumn<int>("count");
        for (int count : {1, 2, 100, 1000, 10000})
            QTest::newRow(qPrintable(QString::number(count))) << count;
    }
    void batchCountIsExact()
    {
        QFETCH(int, count);
        SecureRandomProvider secure;
        const GenerationContext context(secure);
        const auto result = PasswordGenerator().generate(passwordRequest({{"length", 12}}, count), context);
        QVERIFY(result.hasValue());
        QCOMPARE(result.value().records.size(), count);
    }

    void uniqueBatchHasNoDuplicates()
    {
        SecureRandomProvider secure;
        const GenerationContext context(secure);
        // Space of 10^4 strings, 3000 requested: collisions are certain without enforcement.
        const auto result = PasswordGenerator().generate(
            passwordRequest({{"uppercase", false}, {"lowercase", false}, {"symbols", false}, {"length", 4}}, 3000, true),
            context);
        QVERIFY2(result.hasValue(), qPrintable(result.hasValue() ? QString() : result.error().message));
        std::set<QString> seen;
        for (const auto& record : result.value().records)
            seen.insert(record.value(QStringLiteral("password")).toString());
        QCOMPARE(seen.size(), std::size_t{3000});
    }

    void uniquenessRetryBudgetIsBounded()
    {
        DeterministicRandomProvider rng(3);
        common::ApplicationLimits limits;
        limits.maxUniquenessAttempts = 1; // collisions become fatal almost immediately
        const auto context = testing::insecureContext(rng, {}, limits);
        // 5040 distinct strings exist (10*9*8*7) and 5000 are requested.
        const auto result = PasswordGenerator().generate(
            passwordRequest({{"uppercase", false}, {"lowercase", false}, {"symbols", false},
                             {"length", 4}, {"allowDuplicates", false}},
                            5000, true),
            context);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, GenerationError::Code::UniquenessExhausted);
    }

    void cancellationBeforeStartProducesNothing()
    {
        SecureRandomProvider secure;
        domain::CancellationSource source;
        source.requestCancel();
        const GenerationContext context(secure, source.token());
        const auto result = PasswordGenerator().generate(passwordRequest({}, 1000), context);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, GenerationError::Code::Cancelled);
    }

    void cancellationDuringBatchStopsPromptly()
    {
        SecureRandomProvider secure;
        domain::CancellationSource source;
        const GenerationContext context(secure, source.token());
        int received = 0;
        testing::LambdaSink sink([&](domain::GeneratedRecord&&) {
            if (++received == 10)
                source.requestCancel();
            return domain::SinkResponse::Continue;
        });
        const auto summary = PasswordGenerator().generateInto(passwordRequest({}, 100000), context, sink);
        QVERIFY(summary.hasValue());
        QCOMPARE(summary.value().status, CompletionStatus::Cancelled);
        QCOMPARE(summary.value().produced, qsizetype{10});
        QCOMPARE(received, 10);
    }

    void sinkCanStopGeneration()
    {
        SecureRandomProvider secure;
        const GenerationContext context(secure);
        int received = 0;
        testing::LambdaSink sink([&](domain::GeneratedRecord&&) {
            return ++received == 3 ? domain::SinkResponse::Stop : domain::SinkResponse::Continue;
        });
        const auto summary = PasswordGenerator().generateInto(passwordRequest({}, 50), context, sink);
        QVERIFY(summary.hasValue());
        QCOMPARE(summary.value().status, CompletionStatus::StoppedBySink);
        QCOMPARE(summary.value().produced, qsizetype{3});
    }

    void streamingDeliversRecordsIncrementally()
    {
        SecureRandomProvider secure;
        const GenerationContext context(secure);
        qsizetype total = 0;
        qsizetype malformed = 0;
        testing::LambdaSink sink([&](domain::GeneratedRecord&& record) {
            if (!record.contains(QStringLiteral("password")))
                ++malformed;
            ++total;
            return domain::SinkResponse::Continue;
        });
        QVERIFY(PasswordGenerator().generateInto(passwordRequest({{"length", 8}}, 500), context, sink).hasValue());
        QCOMPARE(total, qsizetype{500});
        QCOMPARE(malformed, qsizetype{0});
    }

    void limitsAreEnforced()
    {
        SecureRandomProvider secure;
        common::ApplicationLimits limits;
        limits.maxBatchSize = 10;
        const GenerationContext context(secure, {}, limits);
        const PasswordGenerator generator;

        auto tooMany = generator.generate(passwordRequest({}, 11), context);
        QVERIFY(!tooMany.hasValue());
        QCOMPARE(tooMany.error().code, GenerationError::Code::LimitExceeded);

        QVERIFY(generator.generate(passwordRequest({}, 10), context).hasValue());

        auto zero = generator.generate(passwordRequest({}, 0), context);
        QCOMPARE(zero.error().code, GenerationError::Code::InvalidRequest);
        auto negative = generator.generate(passwordRequest({}, -1), context);
        QCOMPARE(negative.error().code, GenerationError::Code::InvalidRequest);
    }

    void insecureProviderIsRejectedOutsideTestMode()
    {
        DeterministicRandomProvider deterministic(1);
        const GenerationContext production(deterministic);
        const auto rejected = PasswordGenerator().generate(passwordRequest({}), production);
        QVERIFY(!rejected.hasValue());
        QCOMPARE(rejected.error().code, GenerationError::Code::InsecureRandomSource);

        const auto testing = testing::insecureContext(deterministic);
        QVERIFY(PasswordGenerator().generate(passwordRequest({}), testing).hasValue());
    }

    void failingRandomSourceYieldsAnErrorNeverAFallbackValue()
    {
        testing::FailingRandomProvider failing;
        const auto context = testing::insecureContext(failing);
        const auto result = PasswordGenerator().generate(passwordRequest({}, 10), context);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, GenerationError::Code::RandomSourceFailure);
        QVERIFY(failing.calls() >= 1);
    }

    void failureMidBatchDiscardsPartialSecrets()
    {
        // Enough words for a couple of passwords, then the source dies.
        DeterministicRandomProvider seedSource(9);
        std::vector<std::uint64_t> words;
        for (int i = 0; i < 40; ++i)
            words.push_back(seedSource.nextUInt64());
        testing::ScriptedRandomProvider scripted(words);
        const auto context = testing::insecureContext(scripted);
        const auto result = PasswordGenerator().generate(passwordRequest({}, 100), context);
        QVERIFY(!result.hasValue()); // error only: no partial password list is returned
        QCOMPARE(result.error().code, GenerationError::Code::RandomSourceFailure);
    }

    void generatorDrawsOnlyFromTheInjectedProvider()
    {
        DeterministicRandomProvider inner(4);
        testing::CountingRandomProvider counting(inner);
        const auto context = testing::insecureContext(counting);
        QVERIFY(PasswordGenerator().generate(passwordRequest({{"length", 10}}, 5), context).hasValue());
        QVERIFY(counting.calls() >= 5 * 10); // at least one draw per character
    }

    void sameSeedGivesIdenticalOutputInTestMode()
    {
        DeterministicRandomProvider a(777);
        DeterministicRandomProvider b(777);
        const auto first = PasswordGenerator().generate(passwordRequest({}, 20), testing::insecureContext(a));
        const auto second = PasswordGenerator().generate(passwordRequest({}, 20), testing::insecureContext(b));
        QVERIFY(first.hasValue() && second.hasValue());
        QCOMPARE(first.value().records, second.value().records);
    }

    // Reference password computed by an independent Python re-implementation of the
    // documented algorithm (draw required, fill, shuffle) driven by the same RNG.
    void goldenPasswordsMatchIndependentReferenceImplementation()
    {
        const QJsonObject pools{{"uppercase", false}, {"symbols", false}, {"length", 8}, {"minimumDigits", 2}};
        DeterministicRandomProvider withDuplicates(7);
        const auto first = PasswordGenerator().generate(passwordRequest(pools, 1), testing::insecureContext(withDuplicates));
        QVERIFY(first.hasValue());
        QCOMPARE(first.value().records.first().value(QStringLiteral("password")).toString(), QStringLiteral("4g6w4gt4"));

        QJsonObject noDuplicates = pools;
        noDuplicates.insert(QStringLiteral("allowDuplicates"), false);
        DeterministicRandomProvider withoutDuplicates(7);
        const auto second = PasswordGenerator().generate(passwordRequest(noDuplicates, 1), testing::insecureContext(withoutDuplicates));
        QVERIFY(second.hasValue());
        QCOMPARE(second.value().records.first().value(QStringLiteral("password")).toString(), QStringLiteral("j2uo5xg4"));
    }

    void secureOutputDiffersBetweenRuns()
    {
        SecureRandomProvider secure;
        const GenerationContext context(secure);
        const auto a = PasswordGenerator().generate(passwordRequest({}, 5), context);
        const auto b = PasswordGenerator().generate(passwordRequest({}, 5), context);
        QVERIFY(a.value().records != b.value().records);
    }

    void invalidConfigurationReturnsDetailedIssues()
    {
        SecureRandomProvider secure;
        const GenerationContext context(secure);
        const auto result = PasswordGenerator().generate(
            passwordRequest({{"length", 5}, {"minimumUppercase", 4}, {"minimumDigits", 4}}), context);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, GenerationError::Code::InvalidConfiguration);
        QVERIFY(result.error().validation.contains(QStringLiteral("length"), QStringLiteral("minimums_exceed_length")));
    }

    void noDuplicatesModeProducesDistinctCharacters()
    {
        SecureRandomProvider secure;
        const GenerationContext context(secure);
        const auto result = PasswordGenerator().generate(
            passwordRequest({{"length", 60}, {"allowDuplicates", false}, {"minimumUppercase", 5},
                             {"minimumDigits", 5}, {"minimumSymbols", 5}},
                            200),
            context);
        QVERIFY(result.hasValue());
        for (const auto& record : result.value().records) {
            const auto pw = codePoints(record);
            const std::set<char32_t> distinct(pw.begin(), pw.end());
            QCOMPARE(distinct.size(), pw.size());
        }
    }

    void forbiddenAndCustomCharactersAreHonoured()
    {
        SecureRandomProvider secure;
        const GenerationContext context(secure);
        const auto result = PasswordGenerator().generate(
            passwordRequest({{"excludedCharacters", "Il1O0o"}, {"customCharacters", "éñ😀"}, {"length", 64}}, 100),
            context);
        QVERIFY(result.hasValue());
        const QString forbidden = QStringLiteral("Il1O0o");
        for (const auto& record : result.value().records) {
            const QString pw = record.value(QStringLiteral("password")).toString();
            for (QChar c : forbidden)
                QVERIFY(!pw.contains(c));
        }
    }

    void sharedGeneratorIsSafeAcrossThreads()
    {
        const PasswordGenerator generator; // one stateless instance
        SecureRandomProvider secure;       // stateless, thread-safe provider
        constexpr int kThreads = 4;
        std::atomic<int> failures{0};
        std::atomic<int> produced{0};
        std::vector<std::thread> workers;
        for (int t = 0; t < kThreads; ++t) {
            workers.emplace_back([&] {
                const GenerationContext context(secure);
                const auto result = generator.generate(
                    passwordRequest({{"length", 16}, {"minimumDigits", 2}, {"minimumSymbols", 2}}, 2000), context);
                if (!result.hasValue()) {
                    ++failures;
                    return;
                }
                produced += static_cast<int>(result.value().records.size());
                for (const auto& record : result.value().records) {
                    if (record.value(QStringLiteral("password")).toString().size() != 16)
                        ++failures;
                }
            });
        }
        for (auto& worker : workers)
            worker.join();
        QCOMPARE(failures.load(), 0);
        QCOMPARE(produced.load(), kThreads * 2000);
    }
};

QTEST_APPLESS_MAIN(TstPasswordGenerator)
#include "tst_password_generator.moc"
