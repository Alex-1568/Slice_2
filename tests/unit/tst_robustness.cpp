// Robustness / DoS-oriented tests: extreme, malformed and hostile configurations.
// Every ceiling asserted here must originate from ApplicationLimits (or a documented
// generator-intrinsic bound), never from a number buried in the code.
#include "TestSupport.hpp"
#include "generators/password/PasswordGenerator.hpp"
#include "infrastructure/random/SecureRandomProvider.hpp"
#include "testing/DeterministicRandomProvider.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

#include <chrono>
#include <limits>
#include <set>
#include <thread>

using namespace account_recommendations;
using domain::GenerationContext;
using domain::GenerationError;
using generators::password::PasswordGenerator;
using testing::passwordRequest;

namespace
{
using common::ApplicationLimits;

QString joinCodePoints(char32_t first, int count)
{
    QString text;
    for (int i = 0; i < count; ++i) {
        const char32_t c = first + static_cast<char32_t>(i);
        text += QString::fromUcs4(&c, 1);
    }
    return text;
}

std::u32string toU32(const QString& text)
{
    const QList<uint> ucs4 = text.toUcs4();
    return std::u32string(ucs4.begin(), ucs4.end());
}
} // namespace

class TstRobustness : public QObject
{
    Q_OBJECT

private slots:
    // ---------------- length extremes ----------------
    void lengthZeroAndNegativeAreRejected()
    {
        const PasswordGenerator generator;
        for (int length : {0, -1, -1000}) {
            const auto result = generator.validate(passwordRequest({{"length", length}}));
            QVERIFY(result.contains(QStringLiteral("length"), QStringLiteral("out_of_range")));
        }
        QCOMPARE(generator.validate(passwordRequest({{"length", 0}})).summary(),
                 QStringLiteral("Length must be between 4 and 512."));
    }

    void absurdlyLargeLengthsAreRejectedWithoutAllocating_data()
    {
        QTest::addColumn<double>("length");
        QTest::addColumn<QString>("code");
        QTest::newRow("513") << 513.0 << "out_of_range";
        QTest::newRow("INT_MAX") << 2147483647.0 << "out_of_range";
        QTest::newRow("2^40") << 1099511627776.0 << "out_of_range";
        QTest::newRow("2^53") << 9007199254740992.0 << "out_of_range";
        QTest::newRow("2^63 (beyond exact doubles)") << 9223372036854775808.0 << "invalid_value";
        QTest::newRow("1e300") << 1e300 << "invalid_value";
    }
    void absurdlyLargeLengthsAreRejectedWithoutAllocating()
    {
        QFETCH(double, length);
        QFETCH(QString, code);
        infrastructure::SecureRandomProvider secure;
        const GenerationContext context(secure);
        const auto result = PasswordGenerator().generate(passwordRequest({{"length", length}}), context);
        QVERIFY(!result.hasValue());
        QVERIFY(result.error().validation.contains(QStringLiteral("length"), code));
    }

    void minimumAndMaximumSupportedLengthsWork()
    {
        infrastructure::SecureRandomProvider secure;
        const GenerationContext context(secure);
        for (int length : {4, 512}) {
            const auto result = PasswordGenerator().generate(passwordRequest({{"length", length}}, 3), context);
            QVERIFY(result.hasValue());
            for (const auto& record : result.value().records)
                QCOMPARE(toU32(record.value(QStringLiteral("password")).toString()).size(), static_cast<std::size_t>(length));
        }
    }

    // ---------------- minimum counts ----------------
    void minimumsLargerThanLengthAreRejected()
    {
        const PasswordGenerator generator;
        QVERIFY(generator.validate(passwordRequest({{"length", 10}, {"minimumDigits", 11}}))
                    .contains(QStringLiteral("length"), QStringLiteral("minimums_exceed_length")));
        QVERIFY(generator.validate(passwordRequest({{"length", 10}, {"minimumUppercase", 4}, {"minimumLowercase", 4}, {"minimumDigits", 4}}))
                    .contains(QStringLiteral("length"), QStringLiteral("minimums_exceed_length")));
        QVERIFY(generator.validate(passwordRequest({{"minimumDigits", 513}}))
                    .contains(QStringLiteral("minimumDigits"), QStringLiteral("out_of_range")));
    }

    void minimumsExactlyEqualToLengthProduceExactCategoryCounts()
    {
        infrastructure::SecureRandomProvider secure;
        const GenerationContext context(secure);
        const auto result = PasswordGenerator().generate(
            passwordRequest({{"length", 8}, {"minimumUppercase", 2}, {"minimumLowercase", 2}, {"minimumDigits", 2}, {"minimumSymbols", 2}}, 200),
            context);
        QVERIFY(result.hasValue());
        const QString symbols = QStringLiteral("!@#$%^&*()-_=+[]{}:;,.?/~");
        for (const auto& record : result.value().records) {
            const QString pw = record.value(QStringLiteral("password")).toString();
            int upper = 0, lower = 0, digit = 0, symbol = 0;
            for (QChar c : pw) {
                upper += (c >= QLatin1Char('A') && c <= QLatin1Char('Z'));
                lower += (c >= QLatin1Char('a') && c <= QLatin1Char('z'));
                digit += (c >= QLatin1Char('0') && c <= QLatin1Char('9'));
                symbol += symbols.contains(c);
            }
            QCOMPARE(upper, 2);
            QCOMPARE(lower, 2);
            QCOMPARE(digit, 2);
            QCOMPARE(symbol, 2);
        }
    }

    // ---------------- character sets ----------------
    void emptyCustomCharsetWithEverythingElseDisabledIsRejected()
    {
        const auto result = PasswordGenerator().validate(passwordRequest(
            {{"uppercase", false}, {"lowercase", false}, {"digits", false}, {"symbols", false}, {"customCharacters", ""}}));
        QVERIFY(result.contains(QStringLiteral("length"), QStringLiteral("no_allowed_characters")));
    }

    void fullyExcludedCharsetIsRejected()
    {
        // Exclude every built-in character; nothing custom is supplied.
        QString everything = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()-_=+[]{}:;,.?/~");
        const auto result = PasswordGenerator().validate(passwordRequest({{"excludedCharacters", everything}}));
        QVERIFY(result.contains(QStringLiteral("length"), QStringLiteral("no_allowed_characters")));
    }

    void customCharactersSurviveWhenEverythingBuiltInIsExcluded()
    {
        const QString everything = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()-_=+[]{}:;,.?/~");
        infrastructure::SecureRandomProvider secure;
        const GenerationContext context(secure);
        const auto result = PasswordGenerator().generate(
            passwordRequest({{"excludedCharacters", everything}, {"customCharacters", "xyz"}, {"length", 12}}, 5), context);
        // 'x','y','z' are excluded too (they are in the lowercase set), so nothing remains.
        QVERIFY(!result.hasValue());
        const auto ok = PasswordGenerator().generate(
            passwordRequest({{"excludedCharacters", everything}, {"customCharacters", "éñü"}, {"length", 12}}, 5), context);
        QVERIFY(ok.hasValue());
        for (const auto& record : ok.value().records)
            for (char32_t c : toU32(record.value(QStringLiteral("password")).toString()))
                QVERIFY(c == U'é' || c == U'ñ' || c == U'ü');
    }

    void duplicateRestrictionBoundary()
    {
        infrastructure::SecureRandomProvider secure;
        const GenerationContext context(secure);
        const PasswordGenerator generator;
        const QJsonObject digitsOnly{{"uppercase", false}, {"lowercase", false}, {"symbols", false}, {"allowDuplicates", false}};

        auto tooLong = digitsOnly;
        tooLong.insert(QStringLiteral("length"), 11);
        QVERIFY(generator.validate(passwordRequest(tooLong)).contains(QStringLiteral("length"), QStringLiteral("insufficient_unique_characters")));

        auto exact = digitsOnly;
        exact.insert(QStringLiteral("length"), 10); // a permutation of 0-9
        const auto result = generator.generate(passwordRequest(exact, 50), context);
        QVERIFY(result.hasValue());
        for (const auto& record : result.value().records) {
            const auto pw = toU32(record.value(QStringLiteral("password")).toString());
            QCOMPARE(std::set<char32_t>(pw.begin(), pw.end()).size(), std::size_t{10});
        }
    }

    // Required characters are drawn first and must be removed from the fill pool; if they
    // were not, a full-length permutation would contain duplicates.
    void duplicateRestrictionBoundaryWithMinimumsStillYieldsAPermutation()
    {
        infrastructure::SecureRandomProvider secure;
        const GenerationContext context(secure);
        const auto result = PasswordGenerator().generate(
            passwordRequest({{"uppercase", false}, {"lowercase", false}, {"symbols", false}, {"length", 10},
                             {"minimumDigits", 6}, {"allowDuplicates", false}},
                            200),
            context);
        QVERIFY(result.hasValue());
        for (const auto& record : result.value().records) {
            const auto pw = toU32(record.value(QStringLiteral("password")).toString());
            QCOMPARE(std::set<char32_t>(pw.begin(), pw.end()).size(), std::size_t{10});
        }
    }

    // ---------------- maximum supported configuration ----------------
    void maximumSupportedConfigurationWorks()
    {
        const QString custom = joinCodePoints(0x4E00, 256); // 256 distinct CJK ideographs
        const QString excluded = joinCodePoints(0x0100, 256); // 256 Latin Extended code points (mostly unused)
        infrastructure::SecureRandomProvider secure;
        const GenerationContext context(secure);

        const auto duplicates = PasswordGenerator().generate(
            passwordRequest({{"length", 512}, {"customCharacters", custom}, {"excludedCharacters", excluded},
                             {"minimumUppercase", 100}, {"minimumLowercase", 100}, {"minimumDigits", 100}, {"minimumSymbols", 100}},
                            20),
            context);
        QVERIFY2(duplicates.hasValue(), qPrintable(duplicates.hasValue() ? QString() : duplicates.error().message));
        for (const auto& record : duplicates.value().records)
            QCOMPARE(toU32(record.value(QStringLiteral("password")).toString()).size(), std::size_t{512});

        // 87 built-in + 256 custom = 343 distinct characters: length 343 without duplicates is the exact limit.
        const auto permutation = PasswordGenerator().generate(
            passwordRequest({{"length", 343}, {"customCharacters", custom}, {"allowDuplicates", false}}, 5), context);
        QVERIFY(permutation.hasValue());
        for (const auto& record : permutation.value().records) {
            const auto pw = toU32(record.value(QStringLiteral("password")).toString());
            QCOMPARE(std::set<char32_t>(pw.begin(), pw.end()).size(), std::size_t{343});
        }
        const auto overLimit = PasswordGenerator().validate(
            passwordRequest({{"length", 344}, {"customCharacters", custom}, {"allowDuplicates", false}}));
        QVERIFY(overLimit.contains(QStringLiteral("length"), QStringLiteral("insufficient_unique_characters")));
    }

    // ---------------- batch / uniqueness ----------------
    void batchCeilingComesFromApplicationLimits()
    {
        infrastructure::SecureRandomProvider secure;
        for (std::size_t ceiling : {std::size_t{1}, std::size_t{7}, std::size_t{100}}) {
            ApplicationLimits limits;
            limits.maxBatchSize = ceiling;
            const GenerationContext context(secure, {}, limits);
            QVERIFY(PasswordGenerator().generate(passwordRequest({}, static_cast<qsizetype>(ceiling)), context).hasValue());
            const auto over = PasswordGenerator().generate(passwordRequest({}, static_cast<qsizetype>(ceiling + 1)), context);
            QVERIFY(!over.hasValue());
            QCOMPARE(over.error().code, GenerationError::Code::LimitExceeded);
        }
    }

    void defaultBatchCeilingIsOneMillion()
    {
        infrastructure::SecureRandomProvider secure;
        const GenerationContext context(secure);
        const auto over = PasswordGenerator().generate(
            passwordRequest({}, static_cast<qsizetype>(ApplicationLimits::defaults().maxBatchSize + 1)), context);
        QCOMPARE(over.error().code, GenerationError::Code::LimitExceeded);
    }

    void impossibleUniquenessIsRejectedUpFront()
    {
        const PasswordGenerator generator;
        const QJsonObject digits4{{"uppercase", false}, {"lowercase", false}, {"symbols", false}, {"length", 4}};
        QVERIFY(generator.validate(passwordRequest(digits4, 10001, true)).contains(QStringLiteral("count"), QStringLiteral("unique_space_too_small")));
        // Exactly the size of the space is not provably impossible, so it passes validation.
        QVERIFY(generator.validate(passwordRequest(digits4, 10000, true)).isValid());
    }

    void uniquenessAtTheEdgeOfTheSpaceFailsCleanlyNotByHanging()
    {
        testing::DeterministicRandomProvider rng(17);
        const auto context = testing::insecureContext(rng); // default retry budget
        const auto started = std::chrono::steady_clock::now();
        const auto result = PasswordGenerator().generate(
            passwordRequest({{"uppercase", false}, {"lowercase", false}, {"symbols", false}, {"length", 4}}, 10000, true), context);
        // Coupon-collector behaviour: the last values are astronomically unlikely to be hit
        // within the retry budget, so the generator must report exhaustion, not spin.
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, GenerationError::Code::UniquenessExhausted);
        QVERIFY(std::chrono::steady_clock::now() - started < std::chrono::seconds(30));
    }

    // ---------------- Unicode ----------------
    void dangerousOrInvalidUnicodeInCustomCharactersIsRejected_data()
    {
        QTest::addColumn<QString>("custom");
        QTest::addColumn<QString>("code");
        QTest::newRow("lone high surrogate") << QString(QChar(0xD800)) << "invalid_encoding";
        QTest::newRow("lone low surrogate") << QString(QChar(0xDC00)) << "invalid_encoding";
        QTest::newRow("reversed surrogate pair") << (QString(QChar(0xDC00)) + QString(QChar(0xD800))) << "invalid_encoding";
        QTest::newRow("NUL") << QString(QChar(0)) << "invalid_character";
        QTest::newRow("tab") << QStringLiteral("a\tb") << "invalid_character";
        QTest::newRow("no-break space") << QString(QChar(0x00A0)) << "invalid_character";
        QTest::newRow("zero-width joiner") << QString(QChar(0x200D)) << "invalid_character";
        QTest::newRow("right-to-left override") << QString(QChar(0x202E)) << "invalid_character";
        QTest::newRow("private use") << QString(QChar(0xE000)) << "invalid_character";
        QTest::newRow("lone combining acute") << QString(QChar(0x0301)) << "invalid_character";
        QTest::newRow("line separator") << QString(QChar(0x2028)) << "invalid_character";
    }
    void dangerousOrInvalidUnicodeInCustomCharactersIsRejected()
    {
        QFETCH(QString, custom);
        QFETCH(QString, code);
        const auto result = PasswordGenerator().validate(passwordRequest({{"customCharacters", custom}}));
        QVERIFY2(result.contains(QStringLiteral("customCharacters"), code), qPrintable(result.summary()));
    }

    // ---------------- malformed / oversized configuration ----------------
    void malformedJsonYieldsNullDocumentNotAConfiguration()
    {
        // Callers (a future persistence layer) MUST check QJsonParseError: an empty object
        // from a failed parse would otherwise silently mean "use defaults".
        for (const char* text : {"{", "{\"length\":}", "[1,2,3]", "not json", "{\"length\": 24,}", ""}) {
            QJsonParseError error{};
            const auto document = QJsonDocument::fromJson(QByteArray(text), &error);
            QVERIFY2(error.error != QJsonParseError::NoError || !document.isObject(), text);
        }
    }

    void wellFormedJsonWithWrongTypesIsRejectedBySchema()
    {
        const QByteArray text = R"({"length":"24","uppercase":null,"symbols":[true],"minimumDigits":{"x":1},"customCharacters":7})";
        QJsonParseError error{};
        const auto document = QJsonDocument::fromJson(text, &error);
        QCOMPARE(error.error, QJsonParseError::NoError);
        const auto result = PasswordGenerator().validate(passwordRequest(document.object()));
        for (const char* field : {"length", "uppercase", "symbols", "minimumDigits", "customCharacters"})
            QVERIFY2(result.contains(QLatin1String(field), QStringLiteral("invalid_type")), field);
    }

    void tooManyParametersAreRejectedBeforePerKeyWork()
    {
        QJsonObject huge;
        for (int i = 0; i < 100000; ++i)
            huge.insert(QStringLiteral("k%1").arg(i), i);
        const auto started = std::chrono::steady_clock::now();
        const auto result = PasswordGenerator().validate(passwordRequest(huge));
        QVERIFY(!result.isValid());
        QCOMPARE(result.issues().size(), 1); // one error, not 100 000
        QVERIFY(result.containsCode(QStringLiteral("too_many_parameters")));
        QVERIFY(std::chrono::steady_clock::now() - started < std::chrono::seconds(5));
    }

    void oversizedStringsAreRejected()
    {
        QCOMPARE(PasswordGenerator().validate(passwordRequest({{"customCharacters", QString(10'000'000, QLatin1Char('x'))}}))
                     .issues().size(), 1);
    }

    // ---------------- every ceiling is driven by ApplicationLimits ----------------
    void tighterLimitsChangeBehaviour()
    {
        infrastructure::SecureRandomProvider secure;
        const PasswordGenerator generator;

        ApplicationLimits shortOutput;
        shortOutput.maxOutputLength = 64;
        const auto tooLong = generator.generate(passwordRequest({{"length", 100}}), GenerationContext(secure, {}, shortOutput));
        QVERIFY(!tooLong.hasValue());
        QCOMPARE(tooLong.error().validation.summary(), QStringLiteral("Length must be between 4 and 64."));
        QVERIFY(generator.generate(passwordRequest({{"length", 64}}), GenerationContext(secure, {}, shortOutput)).hasValue());

        ApplicationLimits fewParameters;
        fewParameters.maxParameterCount = 3;
        const auto crowded = generator.generate(
            passwordRequest({{"length", 10}, {"digits", true}, {"symbols", true}, {"uppercase", true}}), GenerationContext(secure, {}, fewParameters));
        QVERIFY(crowded.error().validation.containsCode(QStringLiteral("too_many_parameters")));

        ApplicationLimits shortStrings;
        shortStrings.maxStringLength = 5;
        const auto longText = generator.generate(passwordRequest({{"customCharacters", "abcdefg"}}), GenerationContext(secure, {}, shortStrings));
        QVERIFY(longText.error().validation.contains(QStringLiteral("customCharacters"), QStringLiteral("too_long")));
    }

    // ---------------- cancellation ----------------
    void cancellationFromAnotherThreadStopsALargeBatch()
    {
        infrastructure::SecureRandomProvider secure;
        domain::CancellationSource source;
        const GenerationContext context(secure, source.token());
        std::thread canceller([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            source.requestCancel();
        });
        const auto result = PasswordGenerator().generate(passwordRequest({{"length", 64}}, 1'000'000), context);
        canceller.join();
        QVERIFY(result.hasValue());
        QCOMPARE(result.value().summary.status, domain::CompletionStatus::Cancelled);
        QVERIFY(result.value().summary.produced < 1'000'000);
        QCOMPARE(result.value().records.size(), result.value().summary.produced);
    }

    void cancellationDoesNotLeakIntoOtherRuns()
    {
        infrastructure::SecureRandomProvider secure;
        domain::CancellationSource cancelled;
        cancelled.requestCancel();
        const auto stopped = PasswordGenerator().generate(passwordRequest({}, 10), GenerationContext(secure, cancelled.token()));
        QCOMPARE(stopped.value().summary.status, domain::CompletionStatus::Cancelled);
        const auto fresh = PasswordGenerator().generate(passwordRequest({}, 10), GenerationContext(secure));
        QCOMPARE(fresh.value().summary.status, domain::CompletionStatus::Completed);
    }

    void uniformIndexZeroIsAContractViolationNotUndefinedBehaviour()
    {
        testing::DeterministicRandomProvider rng(1);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, static_cast<void>(rng.uniformIndex(0)));
        infrastructure::SecureRandomProvider secure;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, static_cast<void>(secure.uniformIndex(0)));
    }
};

QTEST_APPLESS_MAIN(TstRobustness)
#include "tst_robustness.moc"
