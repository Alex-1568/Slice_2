#include "TestSupport.hpp"

#include "generators/identity/common/TemplateEngine.hpp"
#include "generators/identity/data/NameDataProvider.hpp"
#include "generators/identity/generators/AccountIdGenerator.hpp"
#include "generators/identity/generators/DisplayNameGenerator.hpp"
#include "generators/identity/generators/EmailLikeIdentifierGenerator.hpp"
#include "generators/identity/generators/NicknameGenerator.hpp"
#include "generators/identity/generators/ProfileMetadataGenerator.hpp"
#include "generators/identity/generators/UsernameGenerator.hpp"
#include "generators/identity/registerIdentityGenerators.hpp"

#include <QRegularExpression>
#include <QtTest>

#include <memory>

using namespace account_recommendations;
using namespace account_recommendations::generators::identity;

namespace
{
domain::GeneratorRequest requestFor(const domain::IGenerator& generator, qsizetype count = 1)
{
    domain::GeneratorRequest request;
    request.generatorId = generator.metadata().id;
    request.count = count;
    return request;
}
}

class TstIdentityGenerators final : public QObject
{
    Q_OBJECT
private slots:
    void builtinDataProviderIsOfflineAndHasLocalePresets()
    {
        BuiltinNameDataProvider provider;
        const QStringList locales = provider.supportedLocales();
        QCOMPARE(locales.size(), 4);
        for (const QString& locale : locales) {
            const auto dataset = provider.dataset(locale);
            QVERIFY(dataset.has_value());
            QVERIFY(!dataset->firstNames.isEmpty());
            QVERIFY(!dataset->lastNames.isEmpty());
            QVERIFY(!dataset->nicknames.isEmpty());
            QVERIFY(isUsableNameDataset(*dataset));
        }
        QVERIFY(!provider.dataset(QStringLiteral("not-a-locale")).has_value());
    }

    void localDatasetAcceptsValidNonBmpTextAndRejectsUnpairedSurrogates()
    {
        BuiltinNameDataProvider provider;
        auto dataset = provider.dataset(QStringLiteral("en-US"));
        QVERIFY(dataset.has_value());
        dataset->firstNames[0] = QString::fromUcs4(U"🙂");
        QVERIFY(isUsableNameDataset(*dataset));

        QString malformed;
        malformed.append(QChar(static_cast<ushort>(0xD800)));
        dataset->firstNames[0] = malformed;
        QVERIFY(!isUsableNameDataset(*dataset));
    }

    void allMetadataIsValidAndRegistryRegistrationIsModular()
    {
        domain::GeneratorRegistry registry;
        auto registered = registerIdentityGenerators(registry);
        QVERIFY(registered.hasValue());
        QCOMPARE(registry.size(), qsizetype{6});
        for (const auto& metadata : registry.list())
            QVERIFY2(domain::validateMetadata(metadata).isValid(), qPrintable(metadata.id));
        QVERIFY(registry.find(QStringLiteral("username")) != nullptr);
        QVERIFY(registry.find(QStringLiteral("display-name")) != nullptr);
        QVERIFY(registry.find(QStringLiteral("nickname")) != nullptr);
        QVERIFY(registry.find(QStringLiteral("email-identifier")) != nullptr);
        QVERIFY(registry.find(QStringLiteral("account-id")) != nullptr);
        QVERIFY(registry.find(QStringLiteral("profile")) != nullptr);
    }

    void usernameSupportsCustomWordsAndUniqueness()
    {
        UsernameGenerator generator;
        testing::DeterministicRandomProvider random(701);
        const domain::GenerationContext context(random);
        auto request = requestFor(generator, 3);
        request.unique = true;
        request.parameters.insert(QStringLiteral("customWords"), QStringLiteral("Alpha\nBeta\nGamma"));
        const auto result = generator.generate(request, context);
        QVERIFY2(result.hasValue(), result.hasValue() ? "" : qPrintable(result.error().message));
        QCOMPARE(result.value().records.size(), qsizetype{3});
        QSet<QString> usernames;
        for (const auto& record : result.value().records) {
            const QString name = record.value(QStringLiteral("username")).toString();
            QVERIFY(!name.isEmpty());
            QVERIFY(name == name.toLower());
            QVERIFY(!usernames.contains(name));
            usernames.insert(name);
            QVERIFY(domain::validateRecord(generator.metadata(), record).isValid());
        }
    }

    void inactiveUsernameTemplateDoesNotInvalidateOtherPatterns()
    {
        UsernameGenerator generator;
        auto request = requestFor(generator);
        request.parameters.insert(QStringLiteral("pattern"), QStringLiteral("first-last"));
        request.parameters.insert(QStringLiteral("template"), QStringLiteral("{this-is-not-used"));
        QVERIFY2(generator.validate(request).isValid(), "An unused custom template should not block a built-in pattern.");
        request.parameters.insert(QStringLiteral("pattern"), QStringLiteral("custom"));
        QVERIFY(!generator.validate(request).isValid());
    }

    void usernameSupportsEveryBuiltInPattern()
    {
        UsernameGenerator generator;
        testing::DeterministicRandomProvider random(7001);
        const domain::GenerationContext context(random);
        const QStringList patterns{QStringLiteral("first-last"), QStringLiteral("firstlast"),
            QStringLiteral("initial-last"), QStringLiteral("nickname-number"), QStringLiteral("custom")};
        for (const QString& pattern : patterns) {
            auto request = requestFor(generator);
            request.parameters.insert(QStringLiteral("pattern"), pattern);
            request.parameters.insert(QStringLiteral("appendNumber"), true);
            if (pattern == QLatin1String("custom"))
                request.parameters.insert(QStringLiteral("template"), QStringLiteral("{initial}{separator}{last}{number}"));
            const auto result = generator.generate(request, context);
            QVERIFY2(result.hasValue(), result.hasValue() ? "" : qPrintable(result.error().message));
            const QString value = result.value().records.first().value(QStringLiteral("username")).toString();
            QVERIFY(value.size() >= 4);
            QVERIFY(value.size() <= 24);
            QVERIFY(QRegularExpression(QStringLiteral("^[a-z0-9._-]+$")).match(value).hasMatch());
        }
    }

    void usernameRejectsBadTemplateAndImpossibleRanges()
    {
        UsernameGenerator generator;
        auto request = requestFor(generator);
        request.parameters.insert(QStringLiteral("pattern"), QStringLiteral("custom"));
        request.parameters.insert(QStringLiteral("template"), QStringLiteral("{notAllowed}"));
        QVERIFY(!generator.validate(request).isValid());
        request.parameters = {{QStringLiteral("minLength"), 20}, {QStringLiteral("maxLength"), 5}};
        QVERIFY(!generator.validate(request).isValid());
    }

    void displayNameSupportsEachPreset()
    {
        DisplayNameGenerator generator;
        testing::DeterministicRandomProvider random(702);
        const domain::GenerationContext context(random);
        const QStringList styles{QStringLiteral("professional"), QStringLiteral("casual"),
            QStringLiteral("short"), QStringLiteral("stylized"), QStringLiteral("creative"),
            QStringLiteral("minimal"), QStringLiteral("classic"), QStringLiteral("neutral"),
            QStringLiteral("custom")};
        for (const QString& style : styles) {
            auto request = requestFor(generator);
            request.parameters.insert(QStringLiteral("style"), style);
            const auto result = generator.generate(request, context);
            QVERIFY2(result.hasValue(), result.hasValue() ? "" : qPrintable(result.error().message));
            const QString value = result.value().records.first().value(QStringLiteral("displayName")).toString();
            QVERIFY(!value.trimmed().isEmpty());
            QVERIFY(domain::validateRecord(generator.metadata(), result.value().records.first()).isValid());
        }
    }

    void inactiveDisplayAndNicknameTemplatesAreIgnored()
    {
        DisplayNameGenerator display;
        auto displayRequest = requestFor(display);
        displayRequest.parameters.insert(QStringLiteral("style"), QStringLiteral("professional"));
        displayRequest.parameters.insert(QStringLiteral("template"), QStringLiteral("{broken"));
        QVERIFY(display.validate(displayRequest).isValid());
        displayRequest.parameters.insert(QStringLiteral("style"), QStringLiteral("classic"));
        QVERIFY(display.validate(displayRequest).isValid());
        const testing::DeterministicRandomProvider styleRandom(704);
        const domain::GenerationContext styleContext(styleRandom);
        QVERIFY(display.generate(displayRequest, styleContext).hasValue());
        displayRequest.parameters.insert(QStringLiteral("style"), QStringLiteral("neutral"));
        QVERIFY(display.validate(displayRequest).isValid());
        QVERIFY(display.generate(displayRequest, styleContext).hasValue());
        displayRequest.parameters.insert(QStringLiteral("style"), QStringLiteral("custom"));
        QVERIFY(!display.validate(displayRequest).isValid());

        NicknameGenerator nickname;
        auto nicknameRequest = requestFor(nickname);
        nicknameRequest.parameters.insert(QStringLiteral("style"), QStringLiteral("casual"));
        nicknameRequest.parameters.insert(QStringLiteral("template"), QStringLiteral("{broken"));
        QVERIFY(nickname.validate(nicknameRequest).isValid());
        nicknameRequest.parameters.insert(QStringLiteral("style"), QStringLiteral("custom"));
        QVERIFY(!nickname.validate(nicknameRequest).isValid());
    }

    void nicknameSupportsCustomWordsAndTemplates()
    {
        NicknameGenerator generator;
        testing::DeterministicRandomProvider random(703);
        const domain::GenerationContext context(random);
        auto request = requestFor(generator, 3);
        request.unique = true;
        request.parameters.insert(QStringLiteral("customWords"), QStringLiteral("Rin\nMika\nRen"));
        const auto result = generator.generate(request, context);
        QVERIFY(result.hasValue());
        QCOMPARE(result.value().records.size(), qsizetype{3});

        const QStringList styles{QStringLiteral("casual"), QStringLiteral("short"),
            QStringLiteral("creative"), QStringLiteral("fantasy"), QStringLiteral("tech"),
            QStringLiteral("minimal"), QStringLiteral("professional"), QStringLiteral("classic"),
            QStringLiteral("neutral"), QStringLiteral("custom")};
        for (const QString& style : styles) {
            auto styleRequest = requestFor(generator);
            styleRequest.parameters.insert(QStringLiteral("style"), style);
            const auto styleResult = generator.generate(styleRequest, context);
            QVERIFY2(styleResult.hasValue(), styleResult.hasValue() ? "" : qPrintable(styleResult.error().message));
            QVERIFY(!styleResult.value().records.first().value(QStringLiteral("nickname")).toString().isEmpty());
        }

        auto invalid = requestFor(generator);
        invalid.parameters.insert(QStringLiteral("style"), QStringLiteral("custom"));
        invalid.parameters.insert(QStringLiteral("template"), QStringLiteral("{system.exec}"));
        QVERIFY(!generator.validate(invalid).isValid());
    }

    void emailIdentifiersUseReservedDomainsOnly()
    {
        EmailLikeIdentifierGenerator generator;
        testing::DeterministicRandomProvider random(704);
        const domain::GenerationContext context(random);
        const QStringList domains{QStringLiteral("example.com"), QStringLiteral("example.net"),
            QStringLiteral("example.org"), QStringLiteral("example.test"),
            QStringLiteral("example.invalid"), QStringLiteral("demo.test"),
            QStringLiteral("sample.invalid")};
        for (const QString& domainName : domains) {
            auto request = requestFor(generator, 5);
            request.parameters.insert(QStringLiteral("domain"), domainName);
            const auto result = generator.generate(request, context);
            QVERIFY2(result.hasValue(), result.hasValue() ? "" : qPrintable(result.error().message));
            for (const auto& record : result.value().records) {
                const QString email = record.value(QStringLiteral("emailIdentifier")).toString();
                QVERIFY(email.endsWith(QStringLiteral("@") + domainName));
                QVERIFY(!email.contains(QLatin1Char(' ')));
                QVERIFY(email.size() <= 254);
                QVERIFY(domain::validateRecord(generator.metadata(), record).isValid());
            }
        }
        auto unsafe = requestFor(generator);
        unsafe.parameters.insert(QStringLiteral("domain"), QStringLiteral("real-company.example"));
        QVERIFY(!generator.validate(unsafe).isValid());
        auto networkAttempt = requestFor(generator);
        networkAttempt.parameters.insert(QStringLiteral("localPartTemplate"), QStringLiteral("x@example.com"));
        QVERIFY(!generator.validate(networkAttempt).isValid());

        common::ApplicationLimits tinyLimits;
        tinyLimits.maxOutputLength = 10;
        const domain::GenerationContext tinyContext(random, {}, tinyLimits);
        const auto tooSmall = generator.generate(requestFor(generator), tinyContext);
        QVERIFY(!tooSmall.hasValue());
        QCOMPARE(tooSmall.error().code, domain::GenerationError::Code::InvalidConfiguration);
    }

    void accountIdModesRespectTheirFormat()
    {
        AccountIdGenerator generator;
        testing::DeterministicRandomProvider random(705);
        const domain::GenerationContext context(random);
        const QRegularExpression uuidPattern(QStringLiteral("^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$"));
        const QStringList modes{QStringLiteral("uuid"), QStringLiteral("numeric"),
            QStringLiteral("alphanumeric"), QStringLiteral("prefixed"), QStringLiteral("custom")};
        for (const QString& mode : modes) {
            auto request = requestFor(generator);
            request.parameters.insert(QStringLiteral("mode"), mode);
            const auto result = generator.generate(request, context);
            QVERIFY2(result.hasValue(), result.hasValue() ? "" : qPrintable(result.error().message));
            const QString id = result.value().records.first().value(QStringLiteral("accountId")).toString();
            QVERIFY(!id.isEmpty());
            if (mode == QLatin1String("uuid")) QVERIFY(uuidPattern.match(id).hasMatch());
            if (mode == QLatin1String("numeric")) QVERIFY(QRegularExpression(QStringLiteral("^[0-9]+$")).match(id).hasMatch());
            if (mode == QLatin1String("prefixed")) QVERIFY(id.startsWith(QStringLiteral("acct_")));
            QVERIFY(domain::validateRecord(generator.metadata(), result.value().records.first()).isValid());
        }
        auto invalid = requestFor(generator);
        invalid.parameters.insert(QStringLiteral("mode"), QStringLiteral("custom"));
        invalid.parameters.insert(QStringLiteral("customTemplate"), QStringLiteral("{prefix}"));
        QVERIFY(!generator.validate(invalid).isValid());
        invalid.parameters.insert(QStringLiteral("customTemplate"), QStringLiteral("{{random}}"));
        QVERIFY(!generator.validate(invalid).isValid());
    }

    void accountIdValidatesOnlyFieldsUsedByTheSelectedMode()
    {
        AccountIdGenerator generator;
        auto request = requestFor(generator);
        request.parameters.insert(QStringLiteral("mode"), QStringLiteral("uuid"));
        request.parameters.insert(QStringLiteral("alphabet"), QString());
        request.parameters.insert(QStringLiteral("length"), 128);
        QVERIFY(generator.validate(request).isValid());

        auto alphabetRequired = requestFor(generator);
        alphabetRequired.parameters.insert(QStringLiteral("mode"), QStringLiteral("alphanumeric"));
        alphabetRequired.parameters.insert(QStringLiteral("alphabet"), QString());
        QVERIFY(!generator.validate(alphabetRequired).isValid());

        testing::DeterministicRandomProvider random(7051);
        common::ApplicationLimits limits;
        limits.maxOutputLength = 35;
        const domain::GenerationContext context(random, {}, limits);
        const auto tooSmall = generator.generate(request, context);
        QVERIFY(!tooSmall.hasValue());
        QCOMPARE(tooSmall.error().code, domain::GenerationError::Code::InvalidConfiguration);
    }

    void profileMetadataIsFlatSyntheticAndUniqueByUsername()
    {
        ProfileMetadataGenerator generator;
        testing::DeterministicRandomProvider random(706);
        const domain::GenerationContext context(random);
        auto request = requestFor(generator, 8);
        request.unique = true;
        request.parameters.insert(QStringLiteral("locale"), QStringLiteral("id-ID"));
        request.parameters.insert(QStringLiteral("countryCode"), QStringLiteral("ID"));
        const auto result = generator.generate(request, context);
        QVERIFY2(result.hasValue(), result.hasValue() ? "" : qPrintable(result.error().message));
        QCOMPARE(result.value().records.size(), qsizetype{8});
        QSet<QString> usernames;
        for (const auto& record : result.value().records) {
            QCOMPARE(record.size(), generator.metadata().outputFields.size());
            QVERIFY(domain::validateRecord(generator.metadata(), record).isValid());
            QVERIFY(record.value(QStringLiteral("locale")).toString() == QStringLiteral("id-ID"));
            QVERIFY(record.value(QStringLiteral("countryCode")).toString() == QStringLiteral("ID"));
            const QString username = record.value(QStringLiteral("username")).toString();
            QVERIFY(!usernames.contains(username));
            usernames.insert(username);
            for (auto it = record.constBegin(); it != record.constEnd(); ++it)
                QVERIFY(!it.value().isObject() && !it.value().isArray());
        }
    }

    void profileRespectsRuntimeOutputLengthLimit()
    {
        ProfileMetadataGenerator generator;
        testing::DeterministicRandomProvider random(707);
        common::ApplicationLimits limits;
        limits.maxOutputLength = 1;
        const domain::GenerationContext context(random, {}, limits);
        const auto result = generator.generate(requestFor(generator), context);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, domain::GenerationError::Code::LimitExceeded);
    }

    void generatorBoundaryRejectsUnknownConfigurationKeys()
    {
        UsernameGenerator generator;
        auto request = requestFor(generator);
        request.parameters.insert(QStringLiteral("enableCodeExecution"), true);
        QVERIFY(!generator.validate(request).isValid());
    }
};

QTEST_GUILESS_MAIN(TstIdentityGenerators)
#include "tst_identity_generators.moc"
