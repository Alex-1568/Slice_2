#include "generators/identity/generators/UsernameGenerator.hpp"

#include "generators/identity/common/IdentityUtilities.hpp"
#include "generators/identity/common/TemplateEngine.hpp"

#include <QJsonObject>

#include <algorithm>
#include <utility>

namespace account_recommendations::generators::identity
{
namespace
{
using domain::ConfigurationSchema;
using domain::FieldSpec;

const ConfigurationSchema& schema()
{
    static const ConfigurationSchema value = [] {
        QList<FieldSpec> fields;
        fields.append(FieldSpec::enumeration(QStringLiteral("locale"), QStringLiteral("Locale"),
            builtInNameLocales(), QStringLiteral("en-US"), QStringLiteral("Name data")));
        fields.append(FieldSpec::enumeration(QStringLiteral("pattern"), QStringLiteral("Pattern"),
            {QStringLiteral("first-last"), QStringLiteral("firstlast"), QStringLiteral("initial-last"),
             QStringLiteral("nickname-number"), QStringLiteral("custom")},
            QStringLiteral("first-last"), QStringLiteral("Format")));
        fields.append(FieldSpec::string(QStringLiteral("template"), QStringLiteral("Custom template"),
            QStringLiteral("{first}{separator}{last}"), 256, QStringLiteral("Format"),
            QStringLiteral("Tokens: {first}, {last}, {nickname}, {number}, {separator}, {initial}.")));
        fields.append(FieldSpec::enumeration(QStringLiteral("separator"), QStringLiteral("Separator"),
            {QStringLiteral("dot"), QStringLiteral("underscore"), QStringLiteral("hyphen"), QStringLiteral("none")},
            QStringLiteral("dot"), QStringLiteral("Format")));
        fields.append(FieldSpec::string(QStringLiteral("prefix"), QStringLiteral("Prefix"), QString(), 32, QStringLiteral("Format")));
        fields.append(FieldSpec::string(QStringLiteral("suffix"), QStringLiteral("Suffix"), QString(), 32, QStringLiteral("Format")));
        fields.append(FieldSpec::boolean(QStringLiteral("appendNumber"), QStringLiteral("Append number"), false,
                                         QStringLiteral("Format")));
        fields.append(FieldSpec::integer(QStringLiteral("numberMin"), QStringLiteral("Minimum number"), 0, 0, 999999,
                                         QStringLiteral("Format")));
        fields.append(FieldSpec::integer(QStringLiteral("numberMax"), QStringLiteral("Maximum number"), 9999, 0, 999999,
                                         QStringLiteral("Format")));
        fields.append(FieldSpec::integer(QStringLiteral("minLength"), QStringLiteral("Minimum length"), 4, 2, 128,
                                         QStringLiteral("Constraints")));
        fields.append(FieldSpec::integer(QStringLiteral("maxLength"), QStringLiteral("Maximum length"), 24, 2, 128,
                                         QStringLiteral("Constraints")));
        fields.append(FieldSpec::string(QStringLiteral("allowedCharacters"), QStringLiteral("Allowed characters"),
            QStringLiteral("abcdefghijklmnopqrstuvwxyz0123456789._-"), 96, QStringLiteral("Constraints")));
        fields.append(FieldSpec::string(QStringLiteral("blockedCharacters"), QStringLiteral("Blocked characters"),
            QString(), 96, QStringLiteral("Constraints")));
        fields.append(FieldSpec::string(QStringLiteral("customWords"), QStringLiteral("Custom words"),
            QString(), 4096, QStringLiteral("Custom data"),
            QStringLiteral("Optional newline/semicolon-separated synthetic words.")));
        return ConfigurationSchema(std::move(fields));
    }();
    return value;
}

QString separatorValue(const QString& value)
{
    if (value == QLatin1String("dot")) return QStringLiteral(".");
    if (value == QLatin1String("underscore")) return QStringLiteral("_");
    if (value == QLatin1String("hyphen")) return QStringLiteral("-");
    return {};
}

qint64 number(const QJsonObject& values, const char* key)
{
    return static_cast<qint64>(values.value(QLatin1String(key)).toDouble());
}
} // namespace

UsernameGenerator::UsernameGenerator(std::shared_ptr<const INameDataProvider> provider)
    : m_provider(provider ? std::move(provider) : defaultNameDataProvider())
{
}

domain::GeneratorMetadata UsernameGenerator::metadata() const
{
    domain::GeneratorMetadata md;
    md.id = QStringLiteral("username");
    md.displayName = QStringLiteral("Username");
    md.description = QStringLiteral("Creates synthetic usernames from local name data, templates, custom words, and character rules.");
    md.category = QStringLiteral("Identity");
    md.version = QStringLiteral("1.2.0");
    md.tags = {QStringLiteral("identity"), QStringLiteral("username"), QStringLiteral("synthetic")};
    md.supportsBatch = true;
    md.supportsPresets = true;
    md.supportsUniqueness = true;
    md.configurationSchema = schema();
    md.outputFields = {{QStringLiteral("username"), QStringLiteral("Username"), false}};
    return md;
}

common::ValidationResult UsernameGenerator::validateSpecific(
    const QJsonObject& parameters, const common::ApplicationLimits& limits) const
{
    common::ValidationResult issues;
    const qint64 minLength = number(parameters, "minLength");
    const qint64 maxLength = number(parameters, "maxLength");
    if (minLength > maxLength)
        issues.addError(QStringLiteral("maxLength"), QStringLiteral("invalid_length_range"),
                        QStringLiteral("Maximum length must be at least the minimum length."));
    if (maxLength > static_cast<qint64>(limits.maxOutputLength))
        issues.addError(QStringLiteral("maxLength"), QStringLiteral("limit_exceeded"),
                        QStringLiteral("Maximum length exceeds the application output limit."));

    const QString allowed = parameters.value(QStringLiteral("allowedCharacters")).toString();
    const QString blocked = parameters.value(QStringLiteral("blockedCharacters")).toString();
    if (!isPrintableNonWhitespaceAscii(allowed))
        issues.addError(QStringLiteral("allowedCharacters"), QStringLiteral("invalid_charset"),
                        QStringLiteral("Allowed characters must be non-empty printable ASCII without whitespace."));
    else {
        bool hasAlphanumeric = false;
        for (const QChar c : allowed.toLower()) {
            if ((c >= QLatin1Char('a') && c <= QLatin1Char('z'))
                || (c >= QLatin1Char('0') && c <= QLatin1Char('9'))) {
                if (!blocked.toLower().contains(c)) {
                    hasAlphanumeric = true;
                    break;
                }
            }
        }
        if (!hasAlphanumeric)
            issues.addError(QStringLiteral("allowedCharacters"), QStringLiteral("no_usable_characters"),
                            QStringLiteral("At least one unblocked letter or digit must be allowed."));
    }
    if (!blocked.isEmpty() && !isPrintableNonWhitespaceAscii(blocked))
        issues.addError(QStringLiteral("blockedCharacters"), QStringLiteral("invalid_blocklist"),
                        QStringLiteral("Blocked characters must be printable ASCII without whitespace."));
    if (number(parameters, "numberMin") > number(parameters, "numberMax"))
        issues.addError(QStringLiteral("numberMax"), QStringLiteral("invalid_number_range"),
                        QStringLiteral("Maximum number must be at least the minimum number."));

    const QStringList custom = parseCustomWords(parameters.value(QStringLiteral("customWords")).toString(),
                                                 limits.maxCustomListSize + 1U);
    if (static_cast<std::size_t>(custom.size()) > limits.maxCustomListSize)
        issues.addError(QStringLiteral("customWords"), QStringLiteral("too_many_custom_words"),
                        QStringLiteral("Custom word list exceeds the application limit."));
    const QString pattern = parameters.value(QStringLiteral("pattern")).toString();
    if (custom.isEmpty() && pattern == QLatin1String("custom")) {
        const auto templateIssues = TemplateEngine::validate(parameters.value(QStringLiteral("template")).toString(),
            {QStringLiteral("first"), QStringLiteral("last"), QStringLiteral("nickname"),
             QStringLiteral("number"), QStringLiteral("separator"), QStringLiteral("initial")});
        issues.merge(templateIssues);
    }
    const QString selectedLocale = parameters.value(QStringLiteral("locale")).toString();
    const auto selectedDataset = m_provider
        ? m_provider->dataset(selectedLocale)
        : std::optional<NameDataset>{};
    if (!selectedDataset || selectedDataset->locale != selectedLocale
        || !isUsableNameDataset(*selectedDataset))
        issues.addError(QStringLiteral("locale"), QStringLiteral("unsupported_locale"),
                        QStringLiteral("The selected local name dataset is unavailable or invalid."));
    return issues;
}

common::Result<domain::GeneratedRecord, domain::GenerationError>
UsernameGenerator::generateRecord(const QJsonObject& parameters, const domain::GenerationContext& context) const
{
    using R = common::Result<domain::GeneratedRecord, domain::GenerationError>;
    const QString requestedLocale = parameters.value(QStringLiteral("locale")).toString();
    const auto dataset = m_provider->dataset(requestedLocale);
    if (!dataset || dataset->locale != requestedLocale || !isUsableNameDataset(*dataset))
        return R::failure(domain::GenerationError::constraintViolation(QStringLiteral("Name dataset unavailable.")));

    auto& random = context.random();
    const QString first = choose(dataset->firstNames, random);
    const QString last = choose(dataset->lastNames, random);
    const QString nickname = choose(dataset->nicknames, random);
    const qint64 numberMin = number(parameters, "numberMin");
    const qint64 numberMax = number(parameters, "numberMax");
    const QString numberText = QString::number(numberMin + static_cast<qint64>(random.uniformIndex(
        static_cast<std::size_t>(numberMax - numberMin + 1))));
    const QString separator = separatorValue(parameters.value(QStringLiteral("separator")).toString());

    QString candidate;
    const QStringList customWords = parseCustomWords(parameters.value(QStringLiteral("customWords")).toString(),
                                                      context.limits().maxCustomListSize);
    if (!customWords.isEmpty()) {
        candidate = choose(customWords, random);
    } else {
        QString pattern;
        const QString mode = parameters.value(QStringLiteral("pattern")).toString();
        if (mode == QLatin1String("first-last")) pattern = QStringLiteral("{first}{separator}{last}");
        else if (mode == QLatin1String("firstlast")) pattern = QStringLiteral("{first}{last}");
        else if (mode == QLatin1String("initial-last")) pattern = QStringLiteral("{initial}{separator}{last}");
        else if (mode == QLatin1String("nickname-number")) pattern = QStringLiteral("{nickname}{number}");
        else pattern = parameters.value(QStringLiteral("template")).toString();
        auto values = nameTemplateValues(*dataset, first, last, nickname, numberText, separator);
        values.insert(QStringLiteral("initial"), first.left(1));
        const auto rendered = TemplateEngine::render(pattern, values, context.limits());
        if (!rendered)
            return R::failure(domain::GenerationError::invalidConfiguration(rendered.error()));
        candidate = rendered.value();
    }

    candidate.prepend(parameters.value(QStringLiteral("prefix")).toString());
    candidate.append(parameters.value(QStringLiteral("suffix")).toString());
    const bool numberAlreadyInTemplate = parameters.value(QStringLiteral("pattern")).toString() == QLatin1String("nickname-number")
        || (parameters.value(QStringLiteral("pattern")).toString() == QLatin1String("custom")
            && TemplateEngine::containsToken(parameters.value(QStringLiteral("template")).toString(),
                                             QStringLiteral("number")));
    if (parameters.value(QStringLiteral("appendNumber")).toBool() && !numberAlreadyInTemplate)
        candidate.append(numberText);

    const QString allowed = parameters.value(QStringLiteral("allowedCharacters")).toString();
    const QString blocked = parameters.value(QStringLiteral("blockedCharacters")).toString();
    candidate = sanitizeUsername(candidate, allowed, blocked);
    const qsizetype maxLength = static_cast<qsizetype>(number(parameters, "maxLength"));
    if (candidate.size() > maxLength)
        candidate.truncate(maxLength);
    while (!candidate.isEmpty() && (candidate.endsWith(QLatin1Char('.'))
           || candidate.endsWith(QLatin1Char('_')) || candidate.endsWith(QLatin1Char('-'))))
        candidate.chop(1);

    const qsizetype minLength = static_cast<qsizetype>(number(parameters, "minLength"));
    QString paddingAlphabet;
    for (const QChar c : allowed.toLower()) {
        const bool alphanumeric = (c >= QLatin1Char('a') && c <= QLatin1Char('z'))
                               || (c >= QLatin1Char('0') && c <= QLatin1Char('9'));
        if (alphanumeric && !blocked.toLower().contains(c) && !paddingAlphabet.contains(c))
            paddingAlphabet.append(c);
    }
    while (candidate.size() < minLength && !paddingAlphabet.isEmpty() && candidate.size() < maxLength)
        candidate.append(paddingAlphabet.at(static_cast<qsizetype>(random.uniformIndex(static_cast<std::size_t>(paddingAlphabet.size())))));
    if (candidate.size() < minLength)
        return R::failure(domain::GenerationError::constraintViolation(
            QStringLiteral("The configured character rules cannot satisfy the minimum username length.")));

    domain::GeneratedRecord record;
    record.insert(QStringLiteral("username"), candidate);
    return R::success(std::move(record));
}

} // namespace account_recommendations::generators::identity
