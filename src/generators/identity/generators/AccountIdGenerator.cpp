#include "generators/identity/generators/AccountIdGenerator.hpp"

#include "generators/identity/common/IdentityUtilities.hpp"
#include "generators/identity/common/TemplateEngine.hpp"

#include <QSet>

#include <utility>

namespace account_recommendations::generators::identity
{
namespace
{
const domain::ConfigurationSchema& schema()
{
    static const domain::ConfigurationSchema value = [] {
        QList<domain::FieldSpec> fields;
        fields.append(domain::FieldSpec::enumeration(QStringLiteral("mode"), QStringLiteral("ID format"),
            {QStringLiteral("uuid"), QStringLiteral("numeric"), QStringLiteral("alphanumeric"),
             QStringLiteral("prefixed"), QStringLiteral("custom")},
            QStringLiteral("uuid"), QStringLiteral("Format")));
        fields.append(domain::FieldSpec::integer(QStringLiteral("length"), QStringLiteral("Length"), 16, 1, 128,
                                                 QStringLiteral("Format")));
        fields.append(domain::FieldSpec::string(QStringLiteral("prefix"), QStringLiteral("Prefix"),
                                                QStringLiteral("acct_"), 32, QStringLiteral("Format")));
        fields.append(domain::FieldSpec::string(QStringLiteral("alphabet"), QStringLiteral("Alphabet"),
            QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"),
            128, QStringLiteral("Format")));
        fields.append(domain::FieldSpec::string(QStringLiteral("customTemplate"), QStringLiteral("Custom template"),
            QStringLiteral("{prefix}{random}"), 256, QStringLiteral("Format"),
            QStringLiteral("Tokens: {prefix}, {number}, {random}, {uuid}.")));
        return domain::ConfigurationSchema(std::move(fields));
    }();
    return value;
}

qint64 number(const QJsonObject& values, const char* key)
{
    return static_cast<qint64>(values.value(QLatin1String(key)).toDouble());
}

bool hasUnescapedToken(const QString& pattern, const QString& name)
{
    return TemplateEngine::containsToken(pattern, name);
}
} // namespace

domain::GeneratorMetadata AccountIdGenerator::metadata() const
{
    domain::GeneratorMetadata md;
    md.id = QStringLiteral("account-id");
    md.displayName = QStringLiteral("Account ID");
    md.description = QStringLiteral("Generates synthetic UUID v4, numeric, alphanumeric, prefixed, or templated account identifiers.");
    md.category = QStringLiteral("Identity");
    md.version = QStringLiteral("1.2.0");
    md.tags = {QStringLiteral("identity"), QStringLiteral("id"), QStringLiteral("synthetic")};
    md.supportsBatch = true;
    md.supportsPresets = true;
    md.supportsUniqueness = true;
    md.configurationSchema = schema();
    md.outputFields = {{QStringLiteral("accountId"), QStringLiteral("Account ID"), false}};
    return md;
}

common::ValidationResult AccountIdGenerator::validateSpecific(
    const QJsonObject& parameters, const common::ApplicationLimits& limits) const
{
    common::ValidationResult issues;
    const QString mode = parameters.value(QStringLiteral("mode")).toString();
    const QString pattern = parameters.value(QStringLiteral("customTemplate")).toString();
    const bool usesLength = mode == QLatin1String("numeric") || mode == QLatin1String("alphanumeric")
        || mode == QLatin1String("prefixed")
        || (mode == QLatin1String("custom") && hasUnescapedToken(pattern, QStringLiteral("random")));
    if (usesLength && number(parameters, "length") > static_cast<qint64>(limits.maxOutputLength))
        issues.addError(QStringLiteral("length"), QStringLiteral("limit_exceeded"),
                        QStringLiteral("Random ID length exceeds the application output limit."));
    if (mode == QLatin1String("uuid") && limits.maxOutputLength < 36U)
        issues.addError(QStringLiteral("length"), QStringLiteral("limit_exceeded"),
                        QStringLiteral("The output limit is too small for a UUID v4 (36 characters)."));

    const QString alphabet = parameters.value(QStringLiteral("alphabet")).toString();
    const bool usesAlphabet = mode == QLatin1String("alphanumeric") || mode == QLatin1String("prefixed")
        || (mode == QLatin1String("custom") && hasUnescapedToken(pattern, QStringLiteral("random")));
    if (usesAlphabet) {
        if (!isPrintableNonWhitespaceAscii(alphabet))
            issues.addError(QStringLiteral("alphabet"), QStringLiteral("invalid_alphabet"),
                            QStringLiteral("Alphabet must contain printable non-whitespace ASCII characters."));
        else if (hasDuplicateCharacters(alphabet))
            issues.addError(QStringLiteral("alphabet"), QStringLiteral("duplicate_alphabet_character"),
                            QStringLiteral("Alphabet must not contain duplicate characters."));
    }

    if (mode == QLatin1String("custom")) {
        issues.merge(TemplateEngine::validate(pattern,
            {QStringLiteral("prefix"), QStringLiteral("number"), QStringLiteral("random"), QStringLiteral("uuid")}));
        if (!hasUnescapedToken(pattern, QStringLiteral("random"))
            && !hasUnescapedToken(pattern, QStringLiteral("number"))
            && !hasUnescapedToken(pattern, QStringLiteral("uuid"))) {
            issues.addError(QStringLiteral("customTemplate"), QStringLiteral("constant_custom_template"),
                            QStringLiteral("A custom ID template must include {random}, {number}, or {uuid}."));
        }
    }
    if (mode == QLatin1String("prefixed")
        && parameters.value(QStringLiteral("prefix")).toString().size() >= number(parameters, "length"))
        issues.addError(QStringLiteral("prefix"), QStringLiteral("prefix_too_long"),
                        QStringLiteral("Prefix length must be smaller than total ID length."));
    return issues;
}

common::Result<domain::GeneratedRecord, domain::GenerationError>
AccountIdGenerator::generateRecord(const QJsonObject& parameters, const domain::GenerationContext& context) const
{
    using R = common::Result<domain::GeneratedRecord, domain::GenerationError>;
    auto& random = context.random();
    const QString mode = parameters.value(QStringLiteral("mode")).toString();
    const qsizetype length = static_cast<qsizetype>(number(parameters, "length"));
    const QString prefix = parameters.value(QStringLiteral("prefix")).toString();
    const QString alphabet = parameters.value(QStringLiteral("alphabet")).toString();
    QString id;
    if (mode == QLatin1String("uuid")) {
        id = randomUuidV4(random);
    } else if (mode == QLatin1String("numeric")) {
        id = randomDigits(random, length);
    } else if (mode == QLatin1String("alphanumeric")) {
        id = randomFromAlphabet(random, alphabet, length);
    } else if (mode == QLatin1String("prefixed")) {
        const qsizetype prefixSize = prefix.size();
        if (prefixSize >= length)
            return R::failure(domain::GenerationError::constraintViolation(
                QStringLiteral("Prefix length must be smaller than total ID length.")));
        id = prefix + randomFromAlphabet(random, alphabet, length - prefixSize);
    } else {
        const QString pattern = parameters.value(QStringLiteral("customTemplate")).toString();
        QHash<QString, QString> values;
        values.insert(QStringLiteral("prefix"), prefix);
        if (hasUnescapedToken(pattern, QStringLiteral("number")))
            values.insert(QStringLiteral("number"), QString::number(static_cast<qulonglong>(random.uniformIndex(1'000'000'000U))));
        if (hasUnescapedToken(pattern, QStringLiteral("random")))
            values.insert(QStringLiteral("random"), randomFromAlphabet(random, alphabet, length));
        if (hasUnescapedToken(pattern, QStringLiteral("uuid")))
            values.insert(QStringLiteral("uuid"), randomUuidV4(random));
        const auto rendered = TemplateEngine::render(pattern, values, context.limits());
        if (!rendered)
            return R::failure(domain::GenerationError::invalidConfiguration(rendered.error()));
        id = rendered.value();
    }

    if (id.isEmpty())
        return R::failure(domain::GenerationError::constraintViolation(QStringLiteral("ID configuration produced an empty identifier.")));
    domain::GeneratedRecord record;
    record.insert(QStringLiteral("accountId"), id);
    return R::success(std::move(record));
}

} // namespace account_recommendations::generators::identity
