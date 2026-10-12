#include "TestSupport.hpp"
#include "generators/password/PasswordGenerator.hpp"
#include "testing/DeterministicRandomProvider.hpp"
#include "infrastructure/random/SecureRandomProvider.hpp"

#include <QtTest>

#include <array>
#include <cmath>
#include <set>

using namespace account_recommendations;
using domain::GenerationContext;
using generators::password::PasswordGenerator;
using testing::DeterministicRandomProvider;
using testing::passwordRequest;

namespace
{
const QString kAlphabet = QString::fromUtf8("ABCxyz0189!@#é😀-_aZ");

std::set<char32_t> toSet(const QString& text)
{
    std::set<char32_t> result;
    for (uint c : text.toUcs4())
        result.insert(c);
    return result;
}

// Independent oracle: classifies characters without using the production pool code.
bool isUpper(char32_t c) { return c >= U'A' && c <= U'Z'; }
bool isLower(char32_t c) { return c >= U'a' && c <= U'z'; }
bool isDigit(char32_t c) { return c >= U'0' && c <= U'9'; }
bool isSymbol(char32_t c) { return std::u32string_view(U"!@#$%^&*()-_=+[]{}:;,.?/~").find(c) != std::u32string_view::npos; }

QString randomSubset(DeterministicRandomProvider& rng, std::size_t maxChars)
{
    QString text;
    const QList<uint> units = kAlphabet.toUcs4();
    const std::size_t n = rng.uniformIndex(maxChars + 1);
    for (std::size_t i = 0; i < n; ++i) {
        const auto pick = static_cast<char32_t>(units.at(static_cast<qsizetype>(rng.uniformIndex(static_cast<std::size_t>(units.size())))));
        text += QString::fromUcs4(&pick, 1);
    }
    return text;
}
} // namespace

// Randomised property tests. Every iteration is reproducible from its seed, which
// is included in any failure message.
class TstPasswordProperties : public QObject
{
    Q_OBJECT

private slots:
    void randomConfigurationsSatisfyAllProperties()
    {
        constexpr int kIterations = 600;
        int validCount = 0;
        int invalidCount = 0;
        const PasswordGenerator generator;

        for (int iteration = 0; iteration < kIterations; ++iteration) {
            const auto seed = static_cast<std::uint64_t>(iteration) * 7919U + 1U;
            DeterministicRandomProvider configRng(seed);
            const QString trace = QStringLiteral("seed=%1").arg(seed);

            const auto flag = [&](std::size_t percentTrue) { return configRng.uniformIndex(100) < percentTrue; };
            const auto minimum = [&] { return flag(50) ? 0 : static_cast<int>(configRng.uniformIndex(4)); };

            QJsonObject params;
            params.insert(QStringLiteral("length"), static_cast<int>(4 + configRng.uniformIndex(40)));
            params.insert(QStringLiteral("uppercase"), flag(80));
            params.insert(QStringLiteral("lowercase"), flag(80));
            params.insert(QStringLiteral("digits"), flag(80));
            params.insert(QStringLiteral("symbols"), flag(80));
            params.insert(QStringLiteral("customCharacters"), randomSubset(configRng, 6));
            params.insert(QStringLiteral("excludedCharacters"), randomSubset(configRng, 8));
            params.insert(QStringLiteral("minimumUppercase"), minimum());
            params.insert(QStringLiteral("minimumLowercase"), minimum());
            params.insert(QStringLiteral("minimumDigits"), minimum());
            params.insert(QStringLiteral("minimumSymbols"), minimum());
            params.insert(QStringLiteral("allowDuplicates"), flag(70));
            const auto count = static_cast<qsizetype>(1 + configRng.uniformIndex(5));
            const auto request = passwordRequest(params, count);

            const auto validation = generator.validate(request);
            DeterministicRandomProvider generationRng(seed ^ 0xABCDEFULL);
            const auto context = testing::insecureContext(generationRng);
            const auto result = generator.generate(request, context);

            if (!validation.isValid()) {
                ++invalidCount;
                QVERIFY2(!result.hasValue(), qPrintable(trace));
                QCOMPARE(result.error().code, domain::GenerationError::Code::InvalidConfiguration);
                continue;
            }
            ++validCount;
            QVERIFY2(result.hasValue(),
                     qPrintable(trace + QStringLiteral(" ") + (result.hasValue() ? QString() : result.error().message)));
            QVERIFY2(result.value().records.size() == count, qPrintable(trace));

            const auto length = static_cast<std::size_t>(params.value("length").toInt());
            const auto excluded = toSet(params.value("excludedCharacters").toString());
            const auto custom = toSet(params.value("customCharacters").toString());

            std::set<char32_t> allowed;
            const auto addIf = [&](bool enabled, auto predicate, char32_t first, char32_t last) {
                if (!enabled)
                    return;
                for (char32_t c = first; c <= last; ++c) {
                    if (predicate(c))
                        allowed.insert(c);
                }
            };
            addIf(params.value("uppercase").toBool(), isUpper, U'A', U'Z');
            addIf(params.value("lowercase").toBool(), isLower, U'a', U'z');
            addIf(params.value("digits").toBool(), isDigit, U'0', U'9');
            addIf(params.value("symbols").toBool(), isSymbol, U'!', U'~');
            allowed.insert(custom.begin(), custom.end());
            for (char32_t c : excluded)
                allowed.erase(c);

            for (const auto& record : result.value().records) {
                const QList<uint> ucs4 = record.value(QStringLiteral("password")).toString().toUcs4();
                const std::u32string pw(ucs4.begin(), ucs4.end());

                QVERIFY2(pw.size() == length, qPrintable(trace + QStringLiteral(" length")));
                std::size_t upper = 0, lower = 0, digits = 0, symbols = 0;
                for (char32_t c : pw) {
                    QVERIFY2(allowed.count(c) == 1, qPrintable(trace + QStringLiteral(" char outside allowed set")));
                    QVERIFY2(excluded.count(c) == 0, qPrintable(trace + QStringLiteral(" forbidden char")));
                    upper += isUpper(c);
                    lower += isLower(c);
                    digits += isDigit(c);
                    symbols += isSymbol(c);
                }
                QVERIFY2(upper >= static_cast<std::size_t>(params.value("minimumUppercase").toInt()), qPrintable(trace + " minU"));
                QVERIFY2(lower >= static_cast<std::size_t>(params.value("minimumLowercase").toInt()), qPrintable(trace + " minL"));
                QVERIFY2(digits >= static_cast<std::size_t>(params.value("minimumDigits").toInt()), qPrintable(trace + " minD"));
                QVERIFY2(symbols >= static_cast<std::size_t>(params.value("minimumSymbols").toInt()), qPrintable(trace + " minS"));

                if (!params.value("allowDuplicates").toBool()) {
                    const std::set<char32_t> distinct(pw.begin(), pw.end());
                    QVERIFY2(distinct.size() == pw.size(), qPrintable(trace + QStringLiteral(" duplicates")));
                }

                const double lowerBits = record.value(QStringLiteral("guaranteedEntropyBits")).toDouble();
                const double upperBits = record.value(QStringLiteral("searchSpaceBits")).toDouble();
                const double ceiling = static_cast<double>(length) * std::log2(static_cast<double>(allowed.size())) + 1e-9;
                QVERIFY2(lowerBits >= 0.0 && lowerBits <= upperBits + 1e-9 && upperBits <= ceiling, qPrintable(trace + QStringLiteral(" entropy bounds")));
                const bool constrained = params.value("minimumUppercase").toInt() + params.value("minimumLowercase").toInt()
                                         + params.value("minimumDigits").toInt() + params.value("minimumSymbols").toInt() > 0;
                if (!constrained)
                    QVERIFY2(record.value(QStringLiteral("entropyQuality")).toString() == QLatin1String("exact"), qPrintable(trace + QStringLiteral(" quality")));
            }
        }
        // Both branches must be exercised or the test proves little.
        QVERIFY2(validCount > 150, qPrintable(QStringLiteral("only %1 valid configurations").arg(validCount)));
        QVERIFY2(invalidCount > 30, qPrintable(QStringLiteral("only %1 invalid configurations").arg(invalidCount)));
    }

    void batchProperties()
    {
        const PasswordGenerator generator;
        for (int count : {1, 7, 250}) {
            DeterministicRandomProvider rng(static_cast<std::uint64_t>(count));
            const auto context = testing::insecureContext(rng);
            const auto result = generator.generate(passwordRequest({{"length", 10}}, count), context);
            QVERIFY(result.hasValue());
            QCOMPARE(result.value().records.size(), count);
        }
    }

    // Sanity check only (NOT a proof of randomness quality): each digit should appear
    // about equally often when sampling digits uniformly.
    void characterFrequenciesAreRoughlyUniform()
    {
        infrastructure::SecureRandomProvider secure;
        const GenerationContext context(secure);
        const auto result = PasswordGenerator().generate(
            passwordRequest({{"uppercase", false}, {"lowercase", false}, {"symbols", false}, {"length", 10}}, 20000),
            context);
        QVERIFY(result.hasValue());
        std::array<int, 10> counts{};
        for (const auto& record : result.value().records) {
            for (QChar c : record.value(QStringLiteral("password")).toString())
                ++counts[static_cast<std::size_t>(c.unicode() - u'0')];
        }
        // 200 000 samples, expectation 20 000, sigma ~134: +-5 sigma window.
        for (int count : counts)
            QVERIFY2(count > 19300 && count < 20700, qPrintable(QString::number(count)));
    }
};

QTEST_APPLESS_MAIN(TstPasswordProperties)
#include "tst_password_properties.moc"
