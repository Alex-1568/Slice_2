// Verifies the entropy interval against the EXACT output distribution of the
// specified algorithm (computed by brute-force enumeration, independent of the
// production code) and against what the real generator actually emits.
#include "TestSupport.hpp"
#include "generators/password/EntropyEstimator.hpp"
#include "generators/password/PasswordGenerator.hpp"
#include "generators/password/PasswordValidator.hpp"
#include "testing/DeterministicRandomProvider.hpp"

#include <QtTest>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

using namespace account_recommendations;
using namespace account_recommendations::generators::password;
using testing::DeterministicRandomProvider;

namespace
{
// Alphabet {A,B} (upper) + {0,1} (digits): every other character is excluded.
constexpr const char* kTinyExclusions = "CDEFGHIJKLMNOPQRSTUVWXYZ23456789";

QJsonObject tinyConfig(bool allowDuplicates)
{
    return {{"uppercase", true}, {"lowercase", false}, {"digits", true}, {"symbols", false},
            {"excludedCharacters", kTinyExclusions}, {"length", 4}, {"minimumUppercase", 1},
            {"minimumDigits", 1}, {"allowDuplicates", allowDuplicates}};
}

struct Distribution
{
    std::map<QString, double> probability;
    double shannonBits = 0.0;
    double minEntropyBits = 0.0;
};

// Enumerates "draw required upper, required digit, fill the rest, shuffle uniformly"
// exactly as specified, with duplicates allowed.
Distribution exactDistributionWithDuplicates()
{
    const QStringList allowed{QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("A"), QStringLiteral("B")};
    const QStringList uppers{QStringLiteral("A"), QStringLiteral("B")};
    const QStringList digits{QStringLiteral("0"), QStringLiteral("1")};

    Distribution d;
    std::size_t outcomes = 0;
    std::map<QString, std::size_t> counts;
    for (const QString& u : uppers)
        for (const QString& g : digits)
            for (const QString& f1 : allowed)
                for (const QString& f2 : allowed) {
                    const std::array<QString, 4> pre{u, g, f1, f2};
                    std::array<int, 4> perm{0, 1, 2, 3};
                    do {
                        std::array<QString, 4> out;
                        for (int i = 0; i < 4; ++i)
                            out[static_cast<std::size_t>(perm[static_cast<std::size_t>(i)])] = pre[static_cast<std::size_t>(i)];
                        ++counts[out[0] + out[1] + out[2] + out[3]];
                        ++outcomes;
                    } while (std::next_permutation(perm.begin(), perm.end()));
                }
    double maxP = 0.0;
    for (const auto& [text, n] : counts) {
        const double p = static_cast<double>(n) / static_cast<double>(outcomes);
        d.probability[text] = p;
        d.shannonBits -= p * std::log2(p);
        maxP = std::max(maxP, p);
    }
    d.minEntropyBits = -std::log2(maxP);
    return d;
}
} // namespace

class TstEntropySemantics : public QObject
{
    Q_OBJECT

private slots:
    void estimateMatchesHandComputedBoundsForTheTinyConfiguration()
    {
        auto config = parsePasswordConfiguration(tinyConfig(true)).value();
        const auto estimate = estimateGeneratorEntropy(config, buildPoolPlan(config).value());
        QCOMPARE(estimate.quality, EntropyQuality::Bounded);
        QVERIFY(std::abs(estimate.lowerBoundBits - 6.0) < 1e-9); // 1 + 1 + 2*log2(4)
        QVERIFY(std::abs(estimate.upperBoundBits - 8.0) < 1e-9); // 4*log2(4)
    }

    void lowerBoundIsBelowTheTrueMinEntropyAndUpperBoundAboveTheTrueShannonEntropy()
    {
        const Distribution exact = exactDistributionWithDuplicates();
        auto config = parsePasswordConfiguration(tinyConfig(true)).value();
        const auto estimate = estimateGeneratorEntropy(config, buildPoolPlan(config).value());

        QVERIFY2(estimate.lowerBoundBits <= exact.minEntropyBits + 1e-9,
                 qPrintable(QStringLiteral("lower %1 > true min-entropy %2").arg(estimate.lowerBoundBits).arg(exact.minEntropyBits)));
        QVERIFY(exact.minEntropyBits <= exact.shannonBits + 1e-9);
        QVERIFY(exact.shannonBits <= estimate.upperBoundBits + 1e-9);
        // The point of the whole exercise: the true value is strictly inside the interval,
        // so quoting the upper bound as "the entropy" would have been wrong.
        QVERIFY(exact.shannonBits < estimate.upperBoundBits - 0.1);
        QVERIFY(exact.minEntropyBits > estimate.lowerBoundBits);
    }

    void generatorOutputFollowsTheSpecifiedDistribution()
    {
        const Distribution exact = exactDistributionWithDuplicates();
        DeterministicRandomProvider rng(2026);
        const auto context = testing::insecureContext(rng);
        constexpr qsizetype kSamples = 200000;
        const auto result = PasswordGenerator().generate(testing::passwordRequest(tinyConfig(true), kSamples), context);
        QVERIFY(result.hasValue());

        std::map<QString, double> observed;
        for (const auto& record : result.value().records)
            observed[record.value(QStringLiteral("password")).toString()] += 1.0;

        for (const auto& [text, count] : observed)
            QVERIFY2(exact.probability.count(text) == 1, qPrintable(QStringLiteral("generator emitted impossible string ") + text));
        for (const auto& [text, p] : exact.probability) {
            const double expected = p * static_cast<double>(kSamples);
            const double got = observed.count(text) ? observed[text] : 0.0;
            QVERIFY2(std::abs(got - expected) <= 6.0 * std::sqrt(expected) + 3.0,
                     qPrintable(QStringLiteral("%1: observed %2, expected %3").arg(text).arg(got).arg(expected)));
        }
    }

    void noDuplicatesBoundIsAlsoSound()
    {
        // 4 distinct characters, 2 of them forced from fixed categories: every
        // arrangement of {A,B,0,1} is reachable and equally likely (1/24).
        auto config = parsePasswordConfiguration(tinyConfig(false)).value();
        const auto estimate = estimateGeneratorEntropy(config, buildPoolPlan(config).value());
        QVERIFY(std::abs(estimate.lowerBoundBits - 3.0) < 1e-9);               // 1 + 1 + log2(2!)
        QVERIFY(std::abs(estimate.upperBoundBits - std::log2(24.0)) < 1e-9);    // log2(4!)
        QVERIFY(estimate.lowerBoundBits <= std::log2(24.0) + 1e-9);             // true min-entropy

        DeterministicRandomProvider rng(5);
        const auto context = testing::insecureContext(rng);
        const auto result = PasswordGenerator().generate(testing::passwordRequest(tinyConfig(false), 24000), context);
        QVERIFY(result.hasValue());
        std::map<QString, int> observed;
        for (const auto& record : result.value().records)
            ++observed[record.value(QStringLiteral("password")).toString()];
        QCOMPARE(observed.size(), std::size_t{24});
        for (const auto& [text, count] : observed)
            QVERIFY2(count > 1000 - 6 * 32 && count < 1000 + 6 * 32, qPrintable(text)); // 24 000 / 24 = 1000
    }

    void recordsExposeBothBoundsAndTheQualityLabel()
    {
        DeterministicRandomProvider rng(1);
        const auto context = testing::insecureContext(rng);
        const auto result = PasswordGenerator().generate(
            testing::passwordRequest({{"minimumDigits", 2}, {"minimumSymbols", 2}}, 1), context);
        QVERIFY(result.hasValue());
        const auto record = result.value().records.first();
        const double lower = record.value(QStringLiteral("guaranteedEntropyBits")).toDouble();
        const double upper = record.value(QStringLiteral("searchSpaceBits")).toDouble();
        QVERIFY(lower < upper);
        QCOMPARE(record.value(QStringLiteral("entropyQuality")).toString(), QStringLiteral("bounded"));
    }

    void boundsAreOrderedForRandomConfigurations()
    {
        DeterministicRandomProvider rng(99);
        for (int i = 0; i < 400; ++i) {
            QJsonObject params;
            params.insert(QStringLiteral("length"), static_cast<int>(4 + rng.uniformIndex(60)));
            params.insert(QStringLiteral("allowDuplicates"), rng.uniformIndex(2) == 0);
            for (const char* key : {"minimumUppercase", "minimumLowercase", "minimumDigits", "minimumSymbols"})
                params.insert(QLatin1String(key), static_cast<int>(rng.uniformIndex(4)));
            auto parsed = parsePasswordConfiguration(params);
            if (!parsed)
                continue;
            auto plan = buildPoolPlan(parsed.value());
            if (!plan || !PasswordConfigurationValidator::validate(parsed.value(), plan.value()).isValid())
                continue;
            const auto estimate = estimateGeneratorEntropy(parsed.value(), plan.value());
            QVERIFY(estimate.lowerBoundBits >= 0.0);
            QVERIFY(estimate.lowerBoundBits <= estimate.upperBoundBits + 1e-9);
            if (estimate.quality == EntropyQuality::Exact)
                QCOMPARE(estimate.lowerBoundBits, estimate.upperBoundBits);
        }
    }
};

QTEST_APPLESS_MAIN(TstEntropySemantics)
#include "tst_entropy_semantics.moc"
