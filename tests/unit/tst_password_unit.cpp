#include "TestSupport.hpp"
#include "domain/generator/GeneratorMetadata.hpp"
#include "generators/password/EntropyEstimator.hpp"
#include "generators/password/PasswordAlgorithm.hpp"
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
PasswordConfiguration parseOrFail(const QJsonObject& params)
{
    auto parsed = parsePasswordConfiguration(params);
    if (!parsed)
        qFatal("test setup: parse failed: %s", qPrintable(parsed.error().summary()));
    return std::move(parsed).value();
}

QJsonObject customOnly(const QString& custom, bool allowDuplicates = true, int length = 8)
{
    return {{"uppercase", false}, {"lowercase", false}, {"digits", false}, {"symbols", false},
            {"customCharacters", custom}, {"allowDuplicates", allowDuplicates}, {"length", length}};
}
} // namespace

class TstPasswordUnit : public QObject
{
    Q_OBJECT

private slots:
    void metadataIsValidAndFlagsSensitiveOutput()
    {
        const PasswordGenerator generator;
        const auto md = generator.metadata();
        QVERIFY2(domain::validateMetadata(md).isValid(), qPrintable(domain::validateMetadata(md).summary()));
        QCOMPARE(md.id, QStringLiteral("password"));
        QVERIFY(md.requiresSecureRandom);
        QVERIFY(md.supportsUniqueness);
        const auto it = std::find_if(md.outputFields.begin(), md.outputFields.end(),
                                     [](const auto& f) { return f.key == QLatin1String("password"); });
        QVERIFY(it != md.outputFields.end());
        QVERIFY(it->sensitive);
    }

    void schemaIsWellFormedAndDescribesDefaults()
    {
        const auto& schema = passwordConfigurationSchema();
        QVERIFY(schema.validateDefinition().isValid());
        const PasswordConfiguration config = parseOrFail({});
        QCOMPARE(config.length, std::size_t{24});
        QVERIFY(config.useUppercase && config.useLowercase && config.useDigits && config.useSymbols);
        QVERIFY(config.allowDuplicates);
        QCOMPARE(config.totalMinimum(), std::size_t{0});
    }

    void parsesProvidedValues()
    {
        const PasswordConfiguration config = parseOrFail({{"length", 16}, {"symbols", false},
                                                          {"minimumDigits", 3}, {"allowDuplicates", false},
                                                          {"excludedCharacters", "Il1O0"}});
        QCOMPARE(config.length, std::size_t{16});
        QVERIFY(!config.useSymbols);
        QCOMPARE(config.minimumDigits, std::size_t{3});
        QVERIFY(!config.allowDuplicates);
        QCOMPARE(config.excludedCharacters, QStringLiteral("Il1O0"));
    }

    void standardPoolSizes()
    {
        const auto pools = standardPools();
        QCOMPARE(pools.uppercase.size(), std::size_t{26});
        QCOMPARE(pools.lowercase.size(), std::size_t{26});
        QCOMPARE(pools.digits.size(), std::size_t{10});
        QCOMPARE(pools.symbols.size(), std::size_t{25});
    }

    // ---------- semantic validation ----------
    void rejectsInvalidConfigurations_data()
    {
        QTest::addColumn<QJsonObject>("params");
        QTest::addColumn<QString>("field");
        QTest::addColumn<QString>("code");

        QTest::newRow("length too small") << QJsonObject{{"length", 3}} << "length" << "out_of_range";
        QTest::newRow("length too large") << QJsonObject{{"length", 513}} << "length" << "out_of_range";
        QTest::newRow("length wrong type") << QJsonObject{{"length", "24"}} << "length" << "invalid_type";
        QTest::newRow("length fractional") << QJsonObject{{"length", 24.5}} << "length" << "invalid_value";
        QTest::newRow("length huge") << QJsonObject{{"length", 1e300}} << "length" << "invalid_value";
        QTest::newRow("unknown key") << QJsonObject{{"lenght", 24}} << "lenght" << "unknown_field";
        QTest::newRow("negative minimum") << QJsonObject{{"minimumDigits", -1}} << "minimumDigits" << "out_of_range";
        QTest::newRow("nothing enabled")
            << QJsonObject{{"uppercase", false}, {"lowercase", false}, {"digits", false}, {"symbols", false}}
            << "length" << "no_allowed_characters";
        QTest::newRow("minimum for disabled category")
            << QJsonObject{{"digits", false}, {"minimumDigits", 2}} << "minimumDigits" << "category_disabled";
        QTest::newRow("minimums exceed length")
            << QJsonObject{{"length", 4}, {"minimumUppercase", 2}, {"minimumLowercase", 2}, {"minimumDigits", 1}}
            << "length" << "minimums_exceed_length";
        QTest::newRow("minimum category fully excluded")
            << QJsonObject{{"excludedCharacters", "0123456789"}, {"minimumDigits", 1}}
            << "minimumDigits" << "pool_empty";
        QTest::newRow("custom has whitespace") << customOnly(QStringLiteral("ab c")) << "customCharacters" << "invalid_character";
        QTest::newRow("custom has control char") << customOnly(QStringLiteral("ab\tc")) << "customCharacters" << "invalid_character";
        QTest::newRow("custom has lone combining mark") << customOnly(QString(QChar(0x0301))) << "customCharacters" << "invalid_character";
        QTest::newRow("custom has lone surrogate") << customOnly(QString(QChar(0xD800))) << "customCharacters" << "invalid_encoding";
        QTest::newRow("custom too long") << customOnly(QString(300, QLatin1Char('a'))) << "customCharacters" << "too_long";
        QTest::newRow("excluded too long")
            << QJsonObject{{"excludedCharacters", QString(300, QLatin1Char('a'))}} << "excludedCharacters" << "too_long";
        QTest::newRow("excluded lone surrogate")
            << QJsonObject{{"excludedCharacters", QString(QChar(0xDC00))}} << "excludedCharacters" << "invalid_encoding";
        QTest::newRow("no-duplicates length exceeds unique pool (spec example ABC123, length 8)")
            << customOnly(QStringLiteral("ABC123"), false, 8) << "length" << "insufficient_unique_characters";
        QTest::newRow("no-duplicates category minimum exceeds its pool")
            << QJsonObject{{"uppercase", false}, {"lowercase", false}, {"symbols", false},
                           {"length", 12}, {"minimumDigits", 11}, {"allowDuplicates", false}}
            << "minimumDigits" << "insufficient_unique_characters";
    }
    void rejectsInvalidConfigurations()
    {
        QFETCH(QJsonObject, params);
        QFETCH(QString, field);
        QFETCH(QString, code);
        const PasswordGenerator generator;
        const auto result = generator.validate(testing::passwordRequest(params));
        QVERIFY(!result.isValid());
        QVERIFY2(result.contains(field, code), qPrintable(result.summary()));
    }

    void uniqueCharacterErrorUsesTheSpecifiedMessage()
    {
        const PasswordGenerator generator;
        const auto result = generator.validate(
            testing::passwordRequest(customOnly(QStringLiteral("ABC123"), false, 8)));
        QVERIFY(result.summary().contains(
            QStringLiteral("Password length exceeds the number of available unique characters.")));
        // Exactly 6 unique characters are available, so length 6 is the boundary that must pass.
        QVERIFY(generator.validate(
                    testing::passwordRequest(customOnly(QStringLiteral("ABC123"), false, 6)))
                    .isValid());
    }

    void warnsWhenAnEnabledCategoryIsFullyExcluded()
    {
        const PasswordGenerator generator;
        const auto result = generator.validate(testing::passwordRequest(
            {{"excludedCharacters", "ABCDEFGHIJKLMNOPQRSTUVWXYZ"}}));
        QVERIFY(result.isValid());
        QVERIFY(result.hasWarnings());
        QVERIFY(result.contains(QStringLiteral("minimumUppercase"), QStringLiteral("pool_emptied")));
    }

    void wrongGeneratorIdIsRejected()
    {
        const PasswordGenerator generator;
        auto request = testing::passwordRequest({});
        request.generatorId = QStringLiteral("username");
        QVERIFY(generator.validate(request).contains(QStringLiteral("generatorId"), QStringLiteral("wrong_generator")));
    }

    void countMustBePositive()
    {
        const PasswordGenerator generator;
        QVERIFY(!generator.validate(testing::passwordRequest({}, 0)).isValid());
        QVERIFY(!generator.validate(testing::passwordRequest({}, -5)).isValid());
    }

    void uniqueBatchLargerThanSearchSpaceIsRejectedUpFront()
    {
        const PasswordGenerator generator;
        // 10^4 = 10 000 possible strings; asking for 20 000 unique is impossible.
        const auto request = testing::passwordRequest(
            {{"uppercase", false}, {"lowercase", false}, {"symbols", false}, {"length", 4}}, 20000, true);
        QVERIFY(generator.validate(request).contains(QStringLiteral("count"), QStringLiteral("unique_space_too_small")));
    }

    // ---------- pool plan ----------
    void customCharactersOverlappingEnabledPoolsAreNotCountedTwice()
    {
        const PasswordConfiguration config = parseOrFail(
            {{"uppercase", false}, {"lowercase", false}, {"symbols", false}, {"digits", true},
             {"customCharacters", "ABC123"}});
        const auto plan = buildPoolPlan(config).value();
        QCOMPARE(plan.pool(CharacterCategory::Digits).size(), std::size_t{10});
        QCOMPARE(plan.pool(CharacterCategory::Custom), std::u32string(U"ABC")); // 1,2,3 live in digits
        QCOMPARE(plan.allowed.size(), std::size_t{13});
    }

    void exclusionsApplyToEveryPoolIncludingCustom()
    {
        const PasswordConfiguration config = parseOrFail(
            {{"uppercase", false}, {"lowercase", false}, {"symbols", false}, {"digits", true},
             {"customCharacters", "ABC123"}, {"excludedCharacters", "B1"}});
        const auto plan = buildPoolPlan(config).value();
        QCOMPARE(plan.pool(CharacterCategory::Digits).size(), std::size_t{9});
        QCOMPARE(plan.pool(CharacterCategory::Custom), std::u32string(U"AC"));
        QCOMPARE(plan.allowed.size(), std::size_t{11});
        QVERIFY(!plan.isAllowed(U'B'));
        QVERIFY(plan.isForbidden(U'1'));
    }

    void customCharactersMatchingDisabledCategoryStayCustom()
    {
        const PasswordConfiguration config = parseOrFail(customOnly(QStringLiteral("A1")));
        const auto plan = buildPoolPlan(config).value();
        QCOMPARE(plan.pool(CharacterCategory::Custom), std::u32string(U"1A"));
        QVERIFY(plan.pool(CharacterCategory::Digits).empty());
    }

    void poolsAreDisjointSortedAndDuplicateFree()
    {
        const PasswordConfiguration config = parseOrFail(
            {{"customCharacters", "zZ9!!@@é😀é"}, {"excludedCharacters", "aq"}});
        const auto plan = buildPoolPlan(config).value();
        std::u32string joined;
        for (const auto& pool : plan.pools) {
            QVERIFY(std::is_sorted(pool.begin(), pool.end()));
            QVERIFY(std::adjacent_find(pool.begin(), pool.end()) == pool.end());
            joined += pool;
        }
        std::sort(joined.begin(), joined.end());
        QVERIFY(std::adjacent_find(joined.begin(), joined.end()) == joined.end()); // disjoint
        QCOMPARE(joined.size(), plan.allowed.size());
    }

    void nonBmpCustomCharactersStayAtomic()
    {
        const PasswordConfiguration config = parseOrFail(customOnly(QString::fromUtf8("😀🙂"), true, 6));
        const auto plan = buildPoolPlan(config).value();
        QCOMPARE(plan.allowed.size(), std::size_t{2});
        DeterministicRandomProvider rng(5);
        const auto password = generatePasswordCodePoints(config, plan, rng).value();
        QCOMPARE(password.size(), std::size_t{6});
        for (char32_t c : password)
            QVERIFY(c == 0x1F600 || c == 0x1F642);
    }

    void decodeCharactersNormalizesToNfc()
    {
        std::u32string out;
        QVERIFY(decodeCharacters(QString::fromUtf8("e\xCC\x81"), out)); // e + combining acute
        QCOMPARE(out.size(), std::size_t{1});
        QCOMPARE(static_cast<uint>(out[0]), 0x00E9U);
    }

    // ---------- algorithm ----------
    void satisfiesConstraintsDetectsEveryViolation()
    {
        const PasswordConfiguration config = parseOrFail(
            {{"length", 8}, {"symbols", false}, {"minimumUppercase", 2}, {"minimumDigits", 2},
             {"excludedCharacters", "x"}, {"allowDuplicates", false}});
        const auto plan = buildPoolPlan(config).value();
        QVERIFY(satisfiesConstraints(U"ABab12cd", config, plan));
        QVERIFY(!satisfiesConstraints(U"ABab12c", config, plan));     // wrong length
        QVERIFY(!satisfiesConstraints(U"ABab12cx", config, plan));    // forbidden char
        QVERIFY(!satisfiesConstraints(U"ABab12c!", config, plan));    // not allowed (symbols off)
        QVERIFY(!satisfiesConstraints(U"Aabcdefg", config, plan));    // only 1 uppercase, 0 digits
        QVERIFY(!satisfiesConstraints(U"ABab12cc", config, plan));    // duplicate
    }

    void shuffleIsAPermutation()
    {
        DeterministicRandomProvider rng(11);
        std::u32string text = U"0123456789abcdef";
        std::u32string original = text;
        secureShuffle(text, rng);
        std::u32string sortedText = text;
        std::sort(sortedText.begin(), sortedText.end());
        std::u32string sortedOriginal = original;
        std::sort(sortedOriginal.begin(), sortedOriginal.end());
        QVERIFY(sortedText == sortedOriginal);
        QVERIFY(text != original); // 1 / 16! chance of a false failure
    }

    void shuffleIsRoughlyUniformOverPermutations()
    {
        DeterministicRandomProvider rng(77);
        std::map<std::u32string, int> counts;
        constexpr int kRuns = 60000;
        for (int i = 0; i < kRuns; ++i) {
            std::u32string text = U"abc";
            secureShuffle(text, rng);
            ++counts[text];
        }
        QCOMPARE(counts.size(), std::size_t{6});
        for (const auto& [permutation, count] : counts)
            QVERIFY(count > 9300 && count < 10700);
    }

    // Required characters are drawn first; the shuffle must not leave them clustered.
    void requiredCharactersAreNotPositionallyBiased()
    {
        const PasswordConfiguration config = parseOrFail(
            {{"length", 8}, {"lowercase", false}, {"digits", false}, {"symbols", false},
             {"uppercase", true}, {"customCharacters", "xyz"}, {"minimumUppercase", 1}});
        const auto plan = buildPoolPlan(config).value();
        DeterministicRandomProvider rng(8);
        std::array<int, 8> uppercaseAt{};
        for (int i = 0; i < 4000; ++i) {
            const auto pw = generatePasswordCodePoints(config, plan, rng).value();
            for (std::size_t p = 0; p < pw.size(); ++p)
                uppercaseAt[p] += (pw[p] >= U'A' && pw[p] <= U'Z') ? 1 : 0;
        }
        // Expected ~ (1 + 7*26/29) / 8 = 90.9% per position (~3636 of 4000). Without the
        // final shuffle position 0 would always be uppercase (4000).
        for (int count : uppercaseAt)
            QVERIFY2(count > 3450 && count < 3850, qPrintable(QString::number(count)));
    }

    // ---------- entropy ----------
    void searchSpaceMatchesClosedForms()
    {
        QVERIFY(std::abs(searchSpaceBits(10, 10, true) - 10.0 * std::log2(10.0)) < 1e-9);
        QVERIFY(std::abs(searchSpaceBits(10, 10, false) - std::log2(3628800.0)) < 1e-9); // 10!
        QVERIFY(std::abs(searchSpaceBits(94, 1, true) - std::log2(94.0)) < 1e-9);
        QCOMPARE(searchSpaceBits(0, 10, true), 0.0);
        QCOMPARE(searchSpaceBits(1, 10, true), 0.0);
        QCOMPARE(searchSpaceBits(10, 11, false), 0.0); // impossible, not negative or NaN
    }

    // ---------- entropy semantics (see EntropyEstimator.hpp) ----------
    void unconstrainedConfigurationHasExactEntropy()
    {
        const PasswordConfiguration config = parseOrFail({});
        const auto estimate = estimateGeneratorEntropy(config, buildPoolPlan(config).value());
        QCOMPARE(estimate.quality, EntropyQuality::Exact);
        QVERIFY(std::abs(estimate.lowerBoundBits - 24.0 * std::log2(87.0)) < 1e-9);
        QCOMPARE(estimate.lowerBoundBits, estimate.upperBoundBits);
    }

    void constrainedConfigurationReportsAnIntervalNeverASinglePretendExactNumber()
    {
        // 36-symbol pool (digits + lowercase), length 8, at least 2 digits.
        const PasswordConfiguration config = parseOrFail(
            {{"uppercase", false}, {"symbols", false}, {"length", 8}, {"minimumDigits", 2}});
        const auto estimate = estimateGeneratorEntropy(config, buildPoolPlan(config).value());
        QCOMPARE(estimate.quality, EntropyQuality::Bounded);
        QVERIFY(std::abs(estimate.upperBoundBits - 8.0 * std::log2(36.0)) < 1e-9);
        QVERIFY(std::abs(estimate.lowerBoundBits - (2.0 * std::log2(10.0) + 6.0 * std::log2(36.0))) < 1e-9);
        QVERIFY(estimate.lowerBoundBits < estimate.upperBoundBits);
        QCOMPARE(estimate.conservativeBits(), estimate.lowerBoundBits);
    }

    void noDuplicatesBoundsUseFallingFactorials()
    {
        const PasswordConfiguration config = parseOrFail(
            {{"uppercase", false}, {"symbols", false}, {"length", 8}, {"minimumDigits", 2}, {"allowDuplicates", false}});
        const auto estimate = estimateGeneratorEntropy(config, buildPoolPlan(config).value());
        // upper: log2(36!/28!) ; lower: log2(10*9) + log2(34!/28!)
        const auto log2FallingFactorial = [](double n, double k) {
            double bits = 0.0;
            for (double i = 0; i < k; ++i)
                bits += std::log2(n - i);
            return bits;
        };
        QVERIFY(std::abs(estimate.upperBoundBits - log2FallingFactorial(36, 8)) < 1e-9);
        QVERIFY(std::abs(estimate.lowerBoundBits - (log2FallingFactorial(10, 2) + log2FallingFactorial(34, 6))) < 1e-9);
    }

    void minimumThatCoversWholePoolDoesNotLoseEntropy()
    {
        // Only digits exist, so "at least 1 digit" excludes nothing: the estimate is exact.
        const PasswordConfiguration config = parseOrFail(
            {{"uppercase", false}, {"lowercase", false}, {"symbols", false}, {"minimumDigits", 1}});
        QCOMPARE(estimateGeneratorEntropy(config, buildPoolPlan(config).value()).quality, EntropyQuality::Exact);
    }

    void unusableConfigurationYieldsZeroBits()
    {
        PasswordConfiguration config = parseOrFail({});
        config.length = 3;
        config.minimumDigits = 9; // more than the length
        const auto estimate = estimateGeneratorEntropy(config, buildPoolPlan(config).value());
        QCOMPARE(estimate.lowerBoundBits, 0.0);
        QCOMPARE(estimate.upperBoundBits, 0.0);
    }

    void bandIsDerivedFromTheConservativeBoundOnly()
    {
        GeneratorEntropyEstimate estimate;
        estimate.lowerBoundBits = 59.0;
        estimate.upperBoundBits = 99.0;
        estimate.quality = EntropyQuality::Bounded;
        // The optimistic bound alone would say "strong"; the honest label must not.
        QCOMPARE(classifyBand(estimate), EntropyBand::Fair);
        QCOMPARE(classifyBand(estimate.upperBoundBits), EntropyBand::Strong); // what the upper bound would have claimed
    }

    void bandBoundaries()
    {
        QCOMPARE(classifyBand(0.0), EntropyBand::Weak);
        QCOMPARE(classifyBand(39.99), EntropyBand::Weak);
        QCOMPARE(classifyBand(40.0), EntropyBand::Fair);
        QCOMPARE(classifyBand(60.0), EntropyBand::Good);
        QCOMPARE(classifyBand(80.0), EntropyBand::Strong);
        QCOMPARE(classifyBand(100.0), EntropyBand::VeryStrong);
        QCOMPARE(classifyBand(500.0), EntropyBand::VeryStrong);
    }

    void bandThresholdsAreConfigurable()
    {
        EntropyBandThresholds custom;
        custom.fair = 10;
        custom.good = 20;
        custom.strong = 30;
        custom.veryStrong = 40;
        QVERIFY(custom.isValid());
        QCOMPARE(classifyBand(25.0, custom), EntropyBand::Good);

        EntropyBandThresholds broken;
        broken.good = 5; // below fair
        QVERIFY(!broken.isValid());
    }

    void idsAreStable()
    {
        QCOMPARE(bandId(EntropyBand::VeryStrong), QStringLiteral("very-strong"));
        QCOMPARE(bandId(EntropyBand::Weak), QStringLiteral("weak"));
        QCOMPARE(qualityId(EntropyQuality::Exact), QStringLiteral("exact"));
        QCOMPARE(qualityId(EntropyQuality::Bounded), QStringLiteral("bounded"));
    }

    // Regression guard for the semantic split: nothing may call a configuration
    // estimate a password "strength".
    void metadataNeverClaimsPerPasswordStrength()
    {
        const auto md = PasswordGenerator().metadata();
        for (const auto& field : md.outputFields) {
            QVERIFY2(!field.key.contains(QStringLiteral("strength"), Qt::CaseInsensitive), qPrintable(field.key));
            QVERIFY2(!field.label.contains(QStringLiteral("strength"), Qt::CaseInsensitive), qPrintable(field.label));
        }
        const auto upper = std::find_if(md.outputFields.begin(), md.outputFields.end(),
                                        [](const auto& f) { return f.key == QLatin1String("searchSpaceBits"); });
        QVERIFY(upper != md.outputFields.end());
        QVERIFY(upper->label.contains(QStringLiteral("upper bound")));
    }
};

QTEST_APPLESS_MAIN(TstPasswordUnit)
#include "tst_password_unit.moc"
