#include "TestSupport.hpp"
#include "testing/DeterministicRandomProvider.hpp"
#include "infrastructure/random/SecureRandomProvider.hpp"

#include <QtTest>

#include <limits>
#include <set>
#include <vector>

using namespace account_recommendations;
using testing::DeterministicRandomProvider;
using infrastructure::SecureRandomProvider;
using testing::ConstantRandomProvider;
using testing::ScriptedRandomProvider;

class TstRandom : public QObject
{
    Q_OBJECT

private slots:
    void deterministicProviderIsReproducible()
    {
        DeterministicRandomProvider a(123);
        DeterministicRandomProvider b(123);
        DeterministicRandomProvider c(124);
        bool differs = false;
        for (int i = 0; i < 100; ++i) {
            const auto x = a.nextUInt64();
            QCOMPARE(x, b.nextUInt64());
            differs = differs || x != c.nextUInt64();
        }
        QVERIFY(differs);
    }

    // Reference values produced by an independent Python implementation of
    // xoshiro256** + SplitMix64 seeding. They pin the sequence across compilers,
    // standard libraries and platforms: a change here silently breaks reproducibility.
    void deterministicProviderMatchesIndependentReferenceVectors()
    {
        DeterministicRandomProvider seed0(0);
        for (std::uint64_t expected : {0x99ec5f36cb75f2b4ULL, 0xbf6e1f784956452aULL, 0x1a5f849d4933e6e0ULL, 0x6aa594f1262d2d2cULL})
            QCOMPARE(seed0.nextUInt64(), expected);
        DeterministicRandomProvider seed42(42);
        for (std::uint64_t expected : {0x15780b2e0c2ec716ULL, 0x6104d9866d113a7eULL, 0xae17533239e499a1ULL, 0xecb8ad4703b360a1ULL})
            QCOMPARE(seed42.nextUInt64(), expected);
    }

    void uniformIndexMatchesIndependentReferenceSequence()
    {
        DeterministicRandomProvider rng(42);
        const std::size_t expected[] = {9, 0, 26, 11, 25, 51, 10, 0};
        for (std::size_t value : expected)
            QCOMPARE(rng.uniformIndex(87), value);
    }

    void providersAdvertiseSecurityHonestly()
    {
        DeterministicRandomProvider deterministic(1);
        SecureRandomProvider secure;
        QVERIFY(!deterministic.isCryptographicallySecure());
        QVERIFY(secure.isCryptographicallySecure());
    }

    void secureProviderProducesVaryingOutput()
    {
        SecureRandomProvider secure;
        std::set<std::uint64_t> seen;
        for (int i = 0; i < 1000; ++i)
            seen.insert(secure.nextUInt64());
        QCOMPARE(seen.size(), std::size_t{1000}); // 64-bit collisions: ~2^-45
    }

    void nextUInt32UsesHighBits()
    {
        ScriptedRandomProvider scripted({0xAABBCCDD11223344ULL});
        QCOMPARE(scripted.nextUInt32(), std::uint32_t{0xAABBCCDDU});
    }

    void uniformIndexStaysInRange_data()
    {
        QTest::addColumn<quint64>("bound");
        for (quint64 bound : {2ULL, 3ULL, 7ULL, 10ULL, 26ULL, 87ULL, 255ULL, 256ULL, 1000ULL,
                              1ULL << 40U, (1ULL << 63U) + 1ULL})
            QTest::newRow(qPrintable(QString::number(bound))) << bound;
    }
    void uniformIndexStaysInRange()
    {
        QFETCH(quint64, bound);
        DeterministicRandomProvider rng(99);
        SecureRandomProvider secure;
        for (int i = 0; i < 2000; ++i) {
            QVERIFY(rng.uniformIndex(static_cast<std::size_t>(bound)) < bound);
            QVERIFY(secure.uniformIndex(static_cast<std::size_t>(bound)) < bound);
        }
    }

    void uniformIndexRejectsZeroBound()
    {
        DeterministicRandomProvider rng(1);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, static_cast<void>(rng.uniformIndex(0)));
    }

    void uniformIndexOfOneConsumesNothing()
    {
        ScriptedRandomProvider scripted({});
        QCOMPARE(scripted.uniformIndex(1), std::size_t{0});
        QCOMPARE(scripted.consumed(), std::size_t{0});
    }

    // 2^64 mod 3 == 1, so the word 0 lies in the biased region and MUST be rejected.
    void rejectionSamplingDiscardsBiasedWords()
    {
        ScriptedRandomProvider scripted({0ULL, 5ULL});
        QCOMPARE(scripted.uniformIndex(3), std::size_t{2}); // 5 % 3
        QCOMPARE(scripted.consumed(), std::size_t{2});
    }

    void rejectionSamplingAcceptsThresholdWord()
    {
        ScriptedRandomProvider scripted({1ULL});
        QCOMPARE(scripted.uniformIndex(3), std::size_t{1});
        QCOMPARE(scripted.consumed(), std::size_t{1});
    }

    void rejectionSamplingAcceptsMaximumWord()
    {
        ScriptedRandomProvider scripted({std::numeric_limits<std::uint64_t>::max()});
        // (2^64 - 1) % 10 == 5
        QCOMPARE(scripted.uniformIndex(10), std::size_t{5});
    }

    void brokenSourceCannotLoopForever()
    {
        ConstantRandomProvider constantZero(0); // always inside the rejected region for bound 3
        QVERIFY_THROWS_EXCEPTION(domain::RandomSourceError, static_cast<void>(constantZero.uniformIndex(3)));
    }

    void exhaustedScriptSurfacesAsSourceError()
    {
        ScriptedRandomProvider scripted({});
        QVERIFY_THROWS_EXCEPTION(domain::RandomSourceError, static_cast<void>(scripted.uniformIndex(10)));
    }

    // Statistical sanity check only. It is NOT evidence of cryptographic security.
    void uniformIndexIsRoughlyUniform_data()
    {
        QTest::addColumn<int>("bound");
        QTest::addColumn<int>("samples");
        QTest::newRow("6") << 6 << 60000;
        QTest::newRow("10") << 10 << 100000;
    }
    void uniformIndexIsRoughlyUniform()
    {
        QFETCH(int, bound);
        QFETCH(int, samples);
        DeterministicRandomProvider rng(2024);
        std::vector<int> buckets(static_cast<std::size_t>(bound), 0);
        for (int i = 0; i < samples; ++i)
            ++buckets[rng.uniformIndex(static_cast<std::size_t>(bound))];
        const double expected = static_cast<double>(samples) / bound;
        for (int count : buckets)
            QVERIFY2(count > expected * 0.95 && count < expected * 1.05,
                     qPrintable(QStringLiteral("bucket count %1, expected ~%2").arg(count).arg(expected)));
    }
};

QTEST_APPLESS_MAIN(TstRandom)
#include "tst_random.moc"
