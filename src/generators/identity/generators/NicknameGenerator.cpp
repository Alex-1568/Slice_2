#include "generators/identity/generators/NicknameGenerator.hpp"

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
            {QStringLiteral("casual"), QStringLiteral("short"), QStringLiteral("creative"),
             QStringLiteral("fantasy"), QStringLiteral("tech"), QStringLiteral("minimal"),
             QStringLiteral("professional"), QStringLiteral("classic"), QStringLiteral("neutral"),
             QStringLiteral("custom")},
            QStringLiteral("casual"), QStringLiteral("Format")));
        fields.append(domain::FieldSpec::string(QStringLiteral("template"), QStringLiteral("Custom template"),
            QStringLiteral("{nickname}"), 256, QStringLiteral("Format")));
        fields.append(domain::FieldSpec::string(QStringLiteral("prefix"), QStringLiteral("Prefix"), QString(), 32,
                                                QStringLiteral("Format")));
        fields.append(domain::FieldSpec::string(QStringLiteral("suffix"), QStringLiteral("Suffix"), QString(), 32,
                                                QStringLiteral("Format")));
        fields.append(domain::FieldSpec::boolean(QStringLiteral("appendNumber"), QStringLiteral("Append number"),
            false, QStringLiteral("Format")));
        fields.append(domain::FieldSpec::integer(QStringLiteral("numberMin"), QStringLiteral("Minimum number"),
            0, 0, 999999, QStringLiteral("Format")));
        fields.append(domain::FieldSpec::integer(QStringLiteral("numberMax"), QStringLiteral("Maximum number"),
            9999, 0, 999999, QStringLiteral("Format")));
        fields.append(domain::FieldSpec::string(QStringLiteral("customWords"), QStringLiteral("Custom nicknames"),
            QString(), 4096, QStringLiteral("Custom data")));
        fields.append(domain::FieldSpec::integer(QStringLiteral("maxLength"), QStringLiteral("Maximum length"),
            32, 1, 128, QStringLiteral("Constraints")));
        return domain::ConfigurationSchema(std::move(fields));
    }();
    return value;
}

qint64 number(const QJsonObject& parameters, const char* key)
{
    return static_cast<qint64>(parameters.value(QLatin1String(key)).toDouble());
}
} // namespace

NicknameGenerator::NicknameGenerator(std::shared_ptr<const INameDataProvider> provider)
    : m_provider(provider ? std::move(provider) : defaultNameDataProvider())
{
}

domain::GeneratorMetadata NicknameGenerator::metadata() const
{
    domain::GeneratorMetadata md;
    md.id = QStringLiteral("nickname");
    md.displayName = QStringLiteral("Nickname");
    md.description = QStringLiteral("Generates short, casual, creative, fantasy, tech, or custom synthetic nicknames.");
    md.category = QStringLiteral("Identity");
    md.version = QStringLiteral("1.2.0");
    md.tags = {QStringLiteral("identity"), QStringLiteral("nickname"), QStringLiteral("synthetic")};
    md.supportsBatch = true;
    md.supportsPresets = true;
    md.supportsUniqueness = true;
    md.configurationSchema = schema();
    md.outputFields = {{QStringLiteral("nickname"), QStringLiteral("Nickname"), false}};
    return md;
}

common::ValidationResult NicknameGenerator::validateSpecific(
    const QJsonObject& parameters, const common::ApplicationLimits& limits) const
{
    common::ValidationResult issues;
    if (number(parameters, "numberMin") > number(parameters, "numberMax"))
        issues.addError(QStringLiteral("numberMax"), QStringLiteral("invalid_number_range"),
                        QStringLiteral("Maximum number must be at least the minimum number."));
    if (number(parameters, "maxLength") > static_cast<qint64>(limits.maxOutputLength))
        issues.addError(QStringLiteral("maxLength"), QStringLiteral("limit_exceeded"),
                        QStringLiteral("Maximum nickname length exceeds the application output limit."));
    const QStringList customWords = parseCustomWords(parameters.value(QStringLiteral("customWords")).toString(),
                                                       limits.maxCustomListSize + 1U);
    if (static_cast<std::size_t>(customWords.size()) <= limits.maxCustomListSize
        && customWords.isEmpty()
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
    if (static_cast<std::size_t>(customWords.size()) > limits.maxCustomListSize)
        issues.addError(QStringLiteral("customWords"), QStringLiteral("too_many_custom_words"),
                        QStringLiteral("Custom nickname list exceeds the application limit."));
    return issues;
}

common::Result<domain::GeneratedRecord, domain::GenerationError>
NicknameGenerator::generateRecord(const QJsonObject& parameters, const domain::GenerationContext& context) const
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
    const qint64 low = number(parameters, "numberMin");
    const qint64 high = number(parameters, "numberMax");
    const QString numberText = QString::number(low + static_cast<qint64>(random.uniformIndex(
        static_cast<std::size_t>(high - low + 1))));
    QHash<QString, QString> values = nameTemplateValues(*dataset, first, last, nickname,
                                                          numberText, QStringLiteral(""));
    values.insert(QStringLiteral("initial"), first.left(1));
    const QString style = parameters.value(QStringLiteral("style")).toString();
    QString result;
    const QStringList customWords = parseCustomWords(parameters.value(QStringLiteral("customWords")).toString(),
                                                      context.limits().maxCustomListSize);
    if (!customWords.isEmpty()) {
        result = choose(customWords, random);
    } else if (style == QLatin1String("casual")) {
        result = nickname;
    } else if (style == QLatin1String("short")) {
        result = nickname.left(4);
    } else if (style == QLatin1String("creative")) {
        result = nickname + choose({QStringLiteral("Nova"), QStringLiteral("Wave"),
                                    QStringLiteral("Spark"), QStringLiteral("Echo")}, random);
    } else if (style == QLatin1String("fantasy")) {
        result = first + choose({QStringLiteral("wyn"), QStringLiteral("ara"),
                                 QStringLiteral("orin"), QStringLiteral("eth")}, random);
    } else if (style == QLatin1String("tech")) {
        result = choose({QStringLiteral("byte"), QStringLiteral("pixel"), QStringLiteral("flux"),
                         QStringLiteral("echo"), QStringLiteral("cyber")}, random) + nickname;
    } else if (style == QLatin1String("minimal")) {
        result = first.left(1).toLower() + last.left(2).toLower();
    } else if (style == QLatin1String("professional")) {
        result = first + last.left(1);
    } else if (style == QLatin1String("classic")) {
        result = nickname + last.left(1);
    } else if (style == QLatin1String("neutral")) {
        result = first.toLower();
    } else {
        const auto rendered = TemplateEngine::render(parameters.value(QStringLiteral("template")).toString(), values,
                                                      context.limits());
        if (!rendered)
            return R::failure(domain::GenerationError::invalidConfiguration(rendered.error()));
        result = rendered.value();
    }
    result.prepend(parameters.value(QStringLiteral("prefix")).toString());
    result.append(parameters.value(QStringLiteral("suffix")).toString());
    if (parameters.value(QStringLiteral("appendNumber")).toBool())
        result.append(numberText);
    result = result.trimmed();
    const qsizetype maxLength = static_cast<qsizetype>(number(parameters, "maxLength"));
    if (result.size() > maxLength)
        result.truncate(maxLength);
    if (result.isEmpty())
        return R::failure(domain::GenerationError::constraintViolation(QStringLiteral("Nickname configuration produced an empty value.")));

    domain::GeneratedRecord record;
    record.insert(QStringLiteral("nickname"), result);
    return R::success(std::move(record));
}

} // namespace account_recommendations::generators::identity
