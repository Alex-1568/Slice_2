#include "generators/identity/generators/DisplayNameGenerator.hpp"

#include "generators/identity/common/IdentityUtilities.hpp"
#include "generators/identity/common/TemplateEngine.hpp"

#include <utility>

namespace account_recommendations::generators::identity
{
namespace
{
const domain::ConfigurationSchema& schema()
{
    static const domain::ConfigurationSchema value = [] {
        QList<domain::FieldSpec> fields;
        fields.append(domain::FieldSpec::enumeration(QStringLiteral("locale"), QStringLiteral("Locale"),
            builtInNameLocales(), QStringLiteral("en-US"), QStringLiteral("Name data")));
        fields.append(domain::FieldSpec::enumeration(QStringLiteral("style"), QStringLiteral("Style"),
            {QStringLiteral("professional"), QStringLiteral("casual"), QStringLiteral("short"),
             QStringLiteral("stylized"), QStringLiteral("creative"), QStringLiteral("minimal"),
             QStringLiteral("classic"), QStringLiteral("neutral"), QStringLiteral("custom")},
            QStringLiteral("professional"), QStringLiteral("Format")));
        fields.append(domain::FieldSpec::string(QStringLiteral("template"), QStringLiteral("Custom template"),
            QStringLiteral("{first} {last}"), 256, QStringLiteral("Format")));
        fields.append(domain::FieldSpec::string(QStringLiteral("prefix"), QStringLiteral("Prefix"), QString(),
                                                48, QStringLiteral("Format")));
        fields.append(domain::FieldSpec::string(QStringLiteral("suffix"), QStringLiteral("Suffix"), QString(),
                                                48, QStringLiteral("Format")));
        fields.append(domain::FieldSpec::string(QStringLiteral("customWords"), QStringLiteral("Custom names"),
            QString(), 4096, QStringLiteral("Custom data")));
        fields.append(domain::FieldSpec::integer(QStringLiteral("maxLength"), QStringLiteral("Maximum length"),
            64, 1, 256, QStringLiteral("Constraints")));
        fields.append(domain::FieldSpec::boolean(QStringLiteral("capitalize"), QStringLiteral("Capitalize words"),
            false, QStringLiteral("Format")));
        return domain::ConfigurationSchema(std::move(fields));
    }();
    return value;
}

qint64 number(const QJsonObject& parameters, const char* key)
{
    return static_cast<qint64>(parameters.value(QLatin1String(key)).toDouble());
}

QString titleCaseFirst(QString value)
{
    if (!value.isEmpty())
        value[0] = value.at(0).toUpper();
    return value;
}
} // namespace

DisplayNameGenerator::DisplayNameGenerator(std::shared_ptr<const INameDataProvider> provider)
    : m_provider(provider ? std::move(provider) : defaultNameDataProvider())
{
}

domain::GeneratorMetadata DisplayNameGenerator::metadata() const
{
    domain::GeneratorMetadata md;
    md.id = QStringLiteral("display-name");
    md.displayName = QStringLiteral("Display Name");
    md.description = QStringLiteral("Generates synthetic display names in professional, casual, short, stylized, or custom formats.");
    md.category = QStringLiteral("Identity");
    md.version = QStringLiteral("1.2.0");
    md.tags = {QStringLiteral("identity"), QStringLiteral("display-name"), QStringLiteral("synthetic")};
    md.supportsBatch = true;
    md.supportsPresets = true;
    md.supportsUniqueness = true;
    md.configurationSchema = schema();
    md.outputFields = {{QStringLiteral("displayName"), QStringLiteral("Display name"), false}};
    return md;
}

common::ValidationResult DisplayNameGenerator::validateSpecific(
    const QJsonObject& parameters, const common::ApplicationLimits& limits) const
{
    common::ValidationResult issues;
    const qint64 maxLength = number(parameters, "maxLength");
    if (maxLength > static_cast<qint64>(limits.maxOutputLength))
        issues.addError(QStringLiteral("maxLength"), QStringLiteral("limit_exceeded"),
                        QStringLiteral("Maximum display-name length exceeds the application limit."));
    const QStringList customNames = parseCustomWords(parameters.value(QStringLiteral("customWords")).toString(),
                                                        limits.maxCustomListSize + 1U);
    if (static_cast<std::size_t>(customNames.size()) <= limits.maxCustomListSize
        && customNames.isEmpty()
        && parameters.value(QStringLiteral("style")).toString() == QLatin1String("custom")) {
        issues.merge(TemplateEngine::validate(parameters.value(QStringLiteral("template")).toString(),
            {QStringLiteral("first"), QStringLiteral("last"), QStringLiteral("nickname"),
             QStringLiteral("number"), QStringLiteral("separator"), QStringLiteral("initial"),
             QStringLiteral("locale"), QStringLiteral("language")}));
    }
    const QString selectedLocale = parameters.value(QStringLiteral("locale")).toString();
    const auto selectedDataset = m_provider
        ? m_provider->dataset(selectedLocale)
        : std::optional<NameDataset>{};
    if (!selectedDataset || selectedDataset->locale != selectedLocale
        || !isUsableNameDataset(*selectedDataset))
        issues.addError(QStringLiteral("locale"), QStringLiteral("unsupported_locale"),
                        QStringLiteral("The selected local name dataset is unavailable or invalid."));
    if (static_cast<std::size_t>(customNames.size()) > limits.maxCustomListSize)
        issues.addError(QStringLiteral("customWords"), QStringLiteral("too_many_custom_words"),
                        QStringLiteral("Custom name list exceeds the application limit."));
    return issues;
}

common::Result<domain::GeneratedRecord, domain::GenerationError>
DisplayNameGenerator::generateRecord(const QJsonObject& parameters, const domain::GenerationContext& context) const
{
    using R = common::Result<domain::GeneratedRecord, domain::GenerationError>;
    const QString requestedLocale = parameters.value(QStringLiteral("locale")).toString();
    const auto dataset = m_provider->dataset(requestedLocale);
    if (!dataset || dataset->locale != requestedLocale || !isUsableNameDataset(*dataset))
        return R::failure(domain::GenerationError::constraintViolation(QStringLiteral("Name dataset unavailable.")));

    auto& random = context.random();
    QString first = choose(dataset->firstNames, random);
    const QString last = choose(dataset->lastNames, random);
    const QString nickname = choose(dataset->nicknames, random);
    const QString numberText = randomDigits(random, 2);
    QHash<QString, QString> values = nameTemplateValues(*dataset, first, last, nickname, numberText, QStringLiteral(" "));
    values.insert(QStringLiteral("initial"), first.left(1));
    const QString style = parameters.value(QStringLiteral("style")).toString();
    QString result;
    const QStringList customNames = parseCustomWords(parameters.value(QStringLiteral("customWords")).toString(),
                                                      context.limits().maxCustomListSize);
    if (!customNames.isEmpty()) {
        result = choose(customNames, random);
    } else if (style == QLatin1String("professional")) result = first + QLatin1Char(' ') + last;
    else if (style == QLatin1String("casual")) result = nickname + QLatin1Char(' ') + last;
    else if (style == QLatin1String("short")) result = nickname;
    else if (style == QLatin1String("stylized")) result = first + QStringLiteral(" · ") + last;
    else if (style == QLatin1String("creative")) result = nickname + QLatin1Char(' ') + choose(
        {QStringLiteral("Nova"), QStringLiteral("River"), QStringLiteral("Blue"), QStringLiteral("Sky")}, random);
    else if (style == QLatin1String("minimal")) result = first.left(1) + QLatin1Char('.') + QLatin1Char(' ') + last;
    else if (style == QLatin1String("classic")) result = last + QStringLiteral(", ") + first;
    else if (style == QLatin1String("neutral")) result = first + QLatin1Char(' ') + last;
    else {
        const auto rendered = TemplateEngine::render(parameters.value(QStringLiteral("template")).toString(), values,
                                                      context.limits());
        if (!rendered)
            return R::failure(domain::GenerationError::invalidConfiguration(rendered.error()));
        result = rendered.value();
    }

    if (parameters.value(QStringLiteral("capitalize")).toBool()) {
        const QStringList parts = result.split(QLatin1Char(' '), Qt::KeepEmptyParts);
        QStringList capitalized;
        capitalized.reserve(parts.size());
        for (QString part : parts)
            capitalized.append(titleCaseFirst(std::move(part)));
        result = capitalized.join(QLatin1Char(' '));
    }
    result = parameters.value(QStringLiteral("prefix")).toString() + result
           + parameters.value(QStringLiteral("suffix")).toString();
    result = result.simplified();
    const qsizetype maxLength = static_cast<qsizetype>(number(parameters, "maxLength"));
    if (result.size() > maxLength)
        result.truncate(maxLength);
    if (result.isEmpty())
        return R::failure(domain::GenerationError::constraintViolation(QStringLiteral("Display-name configuration produced an empty value.")));

    domain::GeneratedRecord record;
    record.insert(QStringLiteral("displayName"), result);
    return R::success(std::move(record));
}

} // namespace account_recommendations::generators::identity
