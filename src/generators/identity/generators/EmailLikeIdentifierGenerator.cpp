#include "generators/identity/generators/EmailLikeIdentifierGenerator.hpp"

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
        fields.append(domain::FieldSpec::string(QStringLiteral("localPartTemplate"), QStringLiteral("Local-part template"),
            QStringLiteral("{first}.{last}{number}"), 256, QStringLiteral("Format"),
            QStringLiteral("Only local-part tokens are accepted; the domain is selected separately.")));
        fields.append(domain::FieldSpec::enumeration(QStringLiteral("domain"), QStringLiteral("Reserved domain"),
            safeEmailDomains(), QStringLiteral("example.invalid"), QStringLiteral("Safety"),
            QStringLiteral("Only reserved/example domains are permitted. No network lookup is performed.")));
        fields.append(domain::FieldSpec::string(QStringLiteral("separator"), QStringLiteral("Separator"),
            QStringLiteral("."), 1, QStringLiteral("Format")));
        fields.append(domain::FieldSpec::integer(QStringLiteral("numberMin"), QStringLiteral("Minimum number"),
            0, 0, 999999, QStringLiteral("Format")));
        fields.append(domain::FieldSpec::integer(QStringLiteral("numberMax"), QStringLiteral("Maximum number"),
            9999, 0, 999999, QStringLiteral("Format")));
        return domain::ConfigurationSchema(std::move(fields));
    }();
    return value;
}

qint64 number(const QJsonObject& values, const char* key)
{
    return static_cast<qint64>(values.value(QLatin1String(key)).toDouble());
}
} // namespace

EmailLikeIdentifierGenerator::EmailLikeIdentifierGenerator(std::shared_ptr<const INameDataProvider> provider)
    : m_provider(provider ? std::move(provider) : defaultNameDataProvider())
{
}

domain::GeneratorMetadata EmailLikeIdentifierGenerator::metadata() const
{
    domain::GeneratorMetadata md;
    md.id = QStringLiteral("email-identifier");
    md.displayName = QStringLiteral("Email-like Identifier");
    md.description = QStringLiteral("Produces synthetic email-shaped identifiers on reserved domains only; no mailbox or network checks.");
    md.category = QStringLiteral("Identity");
    md.version = QStringLiteral("1.2.0");
    md.tags = {QStringLiteral("identity"), QStringLiteral("email"), QStringLiteral("synthetic"), QStringLiteral("offline")};
    md.supportsBatch = true;
    md.supportsPresets = true;
    md.supportsUniqueness = true;
    md.configurationSchema = schema();
    md.outputFields = {{QStringLiteral("emailIdentifier"), QStringLiteral("Email-like identifier"), false}};
    return md;
}

common::ValidationResult EmailLikeIdentifierGenerator::validateSpecific(
    const QJsonObject& parameters, const common::ApplicationLimits& limits) const
{
    common::ValidationResult issues;
    if (!isSafeEmailDomain(parameters.value(QStringLiteral("domain")).toString()))
        issues.addError(QStringLiteral("domain"), QStringLiteral("unsafe_domain"),
                        QStringLiteral("Email-like identifiers may use only the built-in reserved domains."));
    if (number(parameters, "numberMin") > number(parameters, "numberMax"))
        issues.addError(QStringLiteral("numberMax"), QStringLiteral("invalid_number_range"),
                        QStringLiteral("Maximum number must be at least the minimum number."));
    const QString templateValue = parameters.value(QStringLiteral("localPartTemplate")).toString();
    if (templateValue.trimmed().isEmpty())
        issues.addError(QStringLiteral("localPartTemplate"), QStringLiteral("empty_template"),
                        QStringLiteral("Local-part template must not be empty."));
    if (templateValue.contains(QLatin1Char('@')))
        issues.addError(QStringLiteral("localPartTemplate"), QStringLiteral("domain_in_template"),
                        QStringLiteral("The template must contain only the local part, not an @ or domain."));
    issues.merge(TemplateEngine::validate(templateValue,
        {QStringLiteral("first"), QStringLiteral("last"), QStringLiteral("nickname"),
         QStringLiteral("number"), QStringLiteral("separator"), QStringLiteral("initial")}));
    const QString selectedLocale = parameters.value(QStringLiteral("locale")).toString();
    const auto selectedDataset = m_provider
        ? m_provider->dataset(selectedLocale)
        : std::optional<NameDataset>{};
    if (!selectedDataset || selectedDataset->locale != selectedLocale
        || !isUsableNameDataset(*selectedDataset))
        issues.addError(QStringLiteral("locale"), QStringLiteral("unsupported_locale"),
                        QStringLiteral("The selected local name dataset is unavailable or invalid."));
    const QString selectedDomain = parameters.value(QStringLiteral("domain")).toString();
    const std::size_t minimumIdentifierLength = static_cast<std::size_t>(selectedDomain.size()) + 2U;
    if (limits.maxOutputLength < minimumIdentifierLength)
        issues.addError(QStringLiteral("localPartTemplate"), QStringLiteral("output_limit_too_small"),
                        QStringLiteral("Application output limit is too small for even a one-character local part and the selected domain."));
    return issues;
}

common::Result<domain::GeneratedRecord, domain::GenerationError>
EmailLikeIdentifierGenerator::generateRecord(const QJsonObject& parameters, const domain::GenerationContext& context) const
{
    using R = common::Result<domain::GeneratedRecord, domain::GenerationError>;
    const QString requestedLocale = parameters.value(QStringLiteral("locale")).toString();
    const auto dataset = m_provider->dataset(requestedLocale);
    if (!dataset || dataset->locale != requestedLocale || !isUsableNameDataset(*dataset))
        return R::failure(domain::GenerationError::constraintViolation(QStringLiteral("Name dataset unavailable.")));
    const QString domainName = parameters.value(QStringLiteral("domain")).toString();
    if (!isSafeEmailDomain(domainName))
        return R::failure(domain::GenerationError::constraintViolation(
            QStringLiteral("Only reserved/example domains are accepted.")));

    auto& random = context.random();
    const QString first = choose(dataset->firstNames, random);
    const QString last = choose(dataset->lastNames, random);
    const QString nickname = choose(dataset->nicknames, random);
    const qint64 low = number(parameters, "numberMin");
    const qint64 high = number(parameters, "numberMax");
    const QString numberText = QString::number(low + static_cast<qint64>(random.uniformIndex(
        static_cast<std::size_t>(high - low + 1))));
    auto values = nameTemplateValues(*dataset, first, last, nickname, numberText,
                                     parameters.value(QStringLiteral("separator")).toString());
    values.insert(QStringLiteral("initial"), first.left(1));
    const auto rendered = TemplateEngine::render(parameters.value(QStringLiteral("localPartTemplate")).toString(),
                                                  values, context.limits());
    if (!rendered)
        return R::failure(domain::GenerationError::invalidConfiguration(rendered.error()));
    QString localPart = sanitizeEmailLocalPart(rendered.value());
    if (localPart.isEmpty())
        return R::failure(domain::GenerationError::constraintViolation(
            QStringLiteral("The local-part template produced no valid email characters.")));

    const QString emailIdentifier = localPart + QLatin1Char('@') + domainName;
    if (emailIdentifier.size() > 254)
        return R::failure(domain::GenerationError::limitExceeded(
            QStringLiteral("The generated email-like identifier exceeds the 254-character limit.")));

    domain::GeneratedRecord record;
    record.insert(QStringLiteral("emailIdentifier"), emailIdentifier);
    return R::success(std::move(record));
}

} // namespace account_recommendations::generators::identity
