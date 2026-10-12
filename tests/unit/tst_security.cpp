#include "TestSupport.hpp"
#include "domain/security/SensitiveDataPolicy.hpp"
#include "generators/password/PasswordGenerator.hpp"
#include "infrastructure/random/SecureRandomProvider.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QtTest>

#include <limits>

using namespace account_recommendations;
using domain::GenerationContext;
using domain::GenerationError;
using generators::password::PasswordGenerator;
using testing::passwordRequest;

namespace
{
struct SourceFile
{
    QString relativePath;
    QString text;
};

QList<SourceFile> sourcesUnder(const QString& subdirectory)
{
    QList<SourceFile> files;
    const QString root = QStringLiteral(AR_SOURCE_DIR "/src");
    QDirIterator it(root + QLatin1Char('/') + subdirectory,
                    {QStringLiteral("*.cpp"), QStringLiteral("*.hpp")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            files.append({QDir(root).relativeFilePath(path), QString::fromUtf8(file.readAll())});
    }
    return files;
}
} // namespace

class TstSecurity : public QObject
{
    Q_OBJECT

private slots:
    // ---- randomness hygiene ------------------------------------------------
    void weakRandomSourcesAppearNowhereInProductionCode()
    {
        const auto files = sourcesUnder(QString());
        QVERIFY2(files.size() > 20, "source scan found too few files; AR_SOURCE_DIR wrong?");
        const QRegularExpression cRand(QStringLiteral(R"((^|[^A-Za-z0-9_])s?rand\s*\()"));
        const QRegularExpression stdEngines(QStringLiteral(R"(mt19937|minstd_rand|default_random_engine|random_device|QRandomGenerator64?::global)"));
        for (const auto& file : files) {
            QVERIFY2(!cRand.match(file.text).hasMatch(), qPrintable(file.relativePath + ": C rand()/srand()"));
            QVERIFY2(!stdEngines.match(file.text).hasMatch(), qPrintable(file.relativePath + ": non-CSPRNG engine"));
        }
    }

    // Static portability guard: no platform headers or compiler extensions in src/.
    void sourcesAvoidPlatformSpecificHeadersAndCompilerExtensions()
    {
        const QRegularExpression platformHeaders(QStringLiteral(
            R"(#\s*include\s*<(unistd|sys/[a-z_/]+|pthread|windows|dlfcn|fcntl|mach/[a-z_/]+|linux/[a-z_/]+)\.h>)"));
        const QRegularExpression extensions(QStringLiteral(
            R"(__attribute__|__builtin_|__declspec|#\s*pragma\s+(GCC|clang|warning)|__int128|\btypeof\s*\()"));
        for (const auto& file : sourcesUnder(QString())) {
            // The OS entropy adapter is the single place where platform headers belong.
            const bool isEntropyAdapter = file.relativePath.endsWith(QStringLiteral("infrastructure/random/OperatingSystemEntropy.cpp"));
            QVERIFY2(isEntropyAdapter || !platformHeaders.match(file.text).hasMatch(), qPrintable(file.relativePath + ": platform header"));
            QVERIFY2(!extensions.match(file.text).hasMatch(), qPrintable(file.relativePath + ": compiler extension"));
        }
    }

    // The Qt system generator can silently fall back to a non-cryptographic engine (docs/RANDOMNESS_AUDIT.md).
    void qtRandomClassesAppearNowhereInSrc()
    {
        for (const auto& file : sourcesUnder(QString()))
            QVERIFY2(!file.text.contains(QStringLiteral("QRandomGenerator")), qPrintable(file.relativePath));
    }

    void generatorsAndDomainNeverDependOnInfrastructure()
    {
        for (const char* layer : {"generators", "domain", "common"}) {
            for (const auto& file : sourcesUnder(QLatin1String(layer)))
                QVERIFY2(!file.text.contains(QStringLiteral("#include \"infrastructure/")), qPrintable(file.relativePath));
        }
    }

    void domainAndGeneratorsDoNotUseGuiTypes()
    {
        for (const char* layer : {"generators", "domain", "common"}) {
            for (const auto& file : sourcesUnder(QLatin1String(layer))) {
                QVERIFY2(!file.text.contains(QStringLiteral("QtWidgets")) && !file.text.contains(QStringLiteral("<QWidget>")),
                         qPrintable(file.relativePath));
            }
        }
    }

    // Production composition must not be able to pick the deterministic provider up by accident.
    void productionTargetsDoNotReferenceTheDeterministicProvider()
    {
        const QString root = QStringLiteral(AR_SOURCE_DIR);
        for (const char* directory : {"tools", "app"}) {
            QDirIterator it(root + QLatin1Char('/') + QLatin1String(directory),
                            {QStringLiteral("*.cpp"), QStringLiteral("*.hpp"), QStringLiteral("CMakeLists.txt")},
                            QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                QFile file(path);
                QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
                const QString text = QString::fromUtf8(file.readAll());
                QVERIFY2(!text.contains(QStringLiteral("DeterministicRandomProvider")), qPrintable(path));
                QVERIFY2(!text.contains(QStringLiteral("ar_random_testing")), qPrintable(path));
            }
        }
        QFile cmake(root + QStringLiteral("/CMakeLists.txt"));
        QVERIFY(cmake.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString rootCmake = QString::fromUtf8(cmake.readAll());
        const QRegularExpression coreLinksTesting(
            QStringLiteral(R"(target_link_libraries\((ar_core|ar_generators|ar_random)\s[^)]*ar_random_testing)"));
        QVERIFY(!coreLinksTesting.match(rootCmake).hasMatch());
        const QRegularExpression generatorsLinkRandom(
            QStringLiteral(R"(target_link_libraries\(ar_generators\s[^)]*ar_random)"));
        QVERIFY(!generatorsLinkRandom.match(rootCmake).hasMatch());
    }

    // Compile-time boundary: production code cannot see testsupport/ at all, and the words that
    // unlock sensitive export/clipboard or insecure randomness appear only where they belong.
    void testingFacilitiesAreNotReachableFromProductionCode()
    {
        const QString root = QStringLiteral(AR_SOURCE_DIR);
        const auto scan = [&](const QString& directory) {
            QList<SourceFile> files;
            QDirIterator it(root + QLatin1Char('/') + directory,
                            {QStringLiteral("*.cpp"), QStringLiteral("*.hpp")}, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                QFile file(path);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text))
                    files.append({QDir(root).relativeFilePath(path), QString::fromUtf8(file.readAll())});
            }
            return files;
        };
        for (const char* directory : {"src", "tools", "app"}) {
            for (const auto& file : scan(QLatin1String(directory))) {
                QVERIFY2(!file.text.contains(QStringLiteral("#include \"testing/")), qPrintable(file.relativePath + ": includes testsupport"));
                QVERIFY2(!file.text.contains(QStringLiteral("insecureContext")), qPrintable(file.relativePath + ": insecureContext"));
                QVERIFY2(!file.text.contains(QStringLiteral("forTesting")), qPrintable(file.relativePath + ": forTesting"));
                if (!file.relativePath.endsWith(QStringLiteral("domain/generator/GenerationContext.hpp")))
                    QVERIFY2(!file.text.contains(QStringLiteral("InsecureContextAccess")), qPrintable(file.relativePath));
            }
        }
        for (const char* directory : {"src", "tools", "app"}) {
            for (const auto& file : scan(QLatin1String(directory))) {
                const bool allowed = file.relativePath.endsWith(QStringLiteral("domain/security/SensitiveDataPolicy.hpp"))
                                     || file.relativePath.startsWith(QStringLiteral("src/gui/"));
                QVERIFY2(allowed || !file.text.contains(QStringLiteral("userConfirmed(")), qPrintable(file.relativePath + ": userConfirmed"));
                QVERIFY2(!QString(file.relativePath).startsWith(QStringLiteral("tools")) || !file.text.contains(QStringLiteral("ISystemEntropy")),
                         qPrintable(file.relativePath + ": entropy injection seam in a tool"));
                // The RNG trust boundary: injection and attestation words live only where designed.
                QVERIFY2(!file.text.contains(QStringLiteral("providerWithInjectedSource")), qPrintable(file.relativePath + ": injected provider in production code"));
                QVERIFY2(!file.text.contains(QStringLiteral("EntropyInjectionAccess"))
                             || file.relativePath.endsWith(QStringLiteral("infrastructure/random/SecureRandomProvider.hpp")),
                         qPrintable(file.relativePath + ": EntropyInjectionAccess"));
                const bool mayMentionAttestation = file.relativePath.endsWith(QStringLiteral("domain/random/IRandomProvider.hpp"))
                                                   || file.relativePath.endsWith(QStringLiteral("infrastructure/random/SecureRandomProvider.hpp"))
                                                   || file.relativePath.endsWith(QStringLiteral("infrastructure/random/SecureRandomProvider.cpp"));
                QVERIFY2(mayMentionAttestation || !file.text.contains(QStringLiteral("SecureAttestation")), qPrintable(file.relativePath + ": SecureAttestation"));
            }
        }
        QFile cmake(root + QStringLiteral("/CMakeLists.txt"));
        QVERIFY(cmake.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString rootCmake = QString::fromUtf8(cmake.readAll());
        // Only ar_random_testing may export the testsupport include directory.
        const QRegularExpression exportsTestsupport(
            QStringLiteral(R"(target_include_directories\(\s*(\w+)[^)]*testsupport)"));
        QStringList exporters;
        QRegularExpressionMatchIterator it = exportsTestsupport.globalMatch(rootCmake);
        while (it.hasNext())
            exporters << it.next().captured(1);
        QCOMPARE(exporters, QStringList{QStringLiteral("ar_random_testing")});
        QVERIFY(rootCmake.contains(QStringLiteral("ar_add_library(ar_random_testing testsupport/testing/")));
    }

    void failingRandomSourceNeverFallsBack()
    {
        testing::FailingRandomProvider failing;
        const auto context = testing::insecureContext(failing);
        const auto result = PasswordGenerator().generate(passwordRequest({}, 3), context);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, GenerationError::Code::RandomSourceFailure);
    }

    void nonSecureProviderRejectedByDefault()
    {
        testing::ConstantRandomProvider constant(42);
        const GenerationContext context(constant);
        const auto result = PasswordGenerator().generate(passwordRequest({}), context);
        QCOMPARE(result.error().code, GenerationError::Code::InsecureRandomSource);
    }

    // ---- sensitive-data handling ------------------------------------------
    void generatedPasswordsNeverSurviveLogSanitisation()
    {
        infrastructure::SecureRandomProvider secure;
        const GenerationContext context(secure);
        const PasswordGenerator generator;
        const auto result = generator.generate(passwordRequest({{"length", 32}}, 50), context);
        QVERIFY(result.hasValue());

        const auto policy = domain::SensitiveDataPolicy::forGenerator(generator.metadata());
        const auto logged = policy.sanitize(result.value().records, domain::OutputChannel::Log);
        const auto persisted = policy.sanitize(result.value().records, domain::OutputChannel::Persistence);
        for (qsizetype i = 0; i < logged.size(); ++i) {
            const QString password = result.value().records.at(i).value(QStringLiteral("password")).toString();
            const QString loggedText = QString::fromUtf8(QJsonDocument(logged.at(i)).toJson());
            const QString persistedText = QString::fromUtf8(QJsonDocument(persisted.at(i)).toJson());
            QVERIFY(!loggedText.contains(password));
            QVERIFY(!persistedText.contains(password));
        }
    }

    void passwordGeneratorDeclaresItsOutputAsSensitive()
    {
        const auto policy = domain::SensitiveDataPolicy::forGenerator(PasswordGenerator().metadata());
        QVERIFY(policy.isSensitive(QStringLiteral("password")));
        QVERIFY(!policy.isSensitive(QStringLiteral("guaranteedEntropyBits")));
        QVERIFY(policy.isSensitive(QStringLiteral("strength"))); // undeclared => fail-closed
        QVERIFY(policy.requiresConfirmation(QJsonObject{{QStringLiteral("password"), QStringLiteral("x")}}, domain::OutputChannel::Export));
    }

    void errorMessagesDoNotEchoUserSuppliedSecrets()
    {
        const QString secretLooking = QStringLiteral("S3cr3t-Value");
        const auto validation = PasswordGenerator().validate(
            passwordRequest({{"customCharacters", secretLooking + QStringLiteral(" with space")}}));
        QVERIFY(!validation.isValid());
        for (const auto& issue : validation.issues())
            QVERIFY(!issue.message.contains(secretLooking));
    }

    // ---- invalid / hostile input ---------------------------------------------
    void hostileParameterValuesAreRejectedWithoutCrashing_data()
    {
        QTest::addColumn<QJsonObject>("params");
        QTest::newRow("nested object") << QJsonObject{{"length", QJsonObject{{"a", QJsonObject{{"b", 1}}}}}};
        QTest::newRow("array") << QJsonObject{{"uppercase", QJsonArray{true, false}}};
        QTest::newRow("null") << QJsonObject{{"customCharacters", QJsonValue::Null}};
        QTest::newRow("negative huge") << QJsonObject{{"length", -1e308}};
        QTest::newRow("max double") << QJsonObject{{"minimumDigits", std::numeric_limits<double>::max()}};
        QTest::newRow("megabyte custom set") << QJsonObject{{"customCharacters", QString(1'000'000, QLatin1Char('x'))}};
        QTest::newRow("megabyte excluded set") << QJsonObject{{"excludedCharacters", QString(1'000'000, QLatin1Char('x'))}};
        QTest::newRow("wrong type everywhere")
            << QJsonObject{{"length", "x"}, {"uppercase", 1}, {"customCharacters", 5}, {"allowDuplicates", "no"}};
    }
    void hostileParameterValuesAreRejectedWithoutCrashing()
    {
        QFETCH(QJsonObject, params);
        infrastructure::SecureRandomProvider secure;
        const GenerationContext context(secure);
        const PasswordGenerator generator;
        QVERIFY(!generator.validate(passwordRequest(params)).isValid());
        const auto result = generator.generate(passwordRequest(params), context);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, GenerationError::Code::InvalidConfiguration);
    }

    void absurdBatchSizesAreRefusedBeforeAllocation()
    {
        infrastructure::SecureRandomProvider secure;
        const GenerationContext context(secure);
        for (qsizetype count : {qsizetype{1'000'001}, std::numeric_limits<qsizetype>::max() / 2, std::numeric_limits<qsizetype>::max()}) {
            const auto result = PasswordGenerator().generate(passwordRequest({}, count), context);
            QVERIFY(!result.hasValue());
            QCOMPARE(result.error().code, GenerationError::Code::LimitExceeded);
        }
        const auto negative = PasswordGenerator().generate(passwordRequest({}, std::numeric_limits<qsizetype>::min()), context);
        QCOMPARE(negative.error().code, GenerationError::Code::InvalidRequest);
    }

    void uniqueRequestsAgainstUnsupportedGeneratorsAreRejected()
    {
        testing::StubGenerator stub(QStringLiteral("stub"));
        testing::ScriptedRandomProvider scripted({});
        const auto context = testing::insecureContext(scripted);
        domain::GeneratorRequest request;
        request.generatorId = QStringLiteral("stub");
        request.unique = true;
        const auto result = stub.generate(request, context);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, GenerationError::Code::InvalidRequest);
    }
};

QTEST_APPLESS_MAIN(TstSecurity)
#include "tst_security.moc"
