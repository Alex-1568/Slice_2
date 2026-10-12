#include "generators/password/PasswordConfiguration.hpp"

namespace account_recommendations::generators::password
{

using common::Result;
using common::ValidationResult;
using domain::ConfigurationSchema;
using domain::FieldSpec;

bool PasswordConfiguration::isEnabled(CharacterCategory category) const noexcept
{
    switch (category) {
    case CharacterCategory::Uppercase: return useUppercase;
    case CharacterCategory::Lowercase: return useLowercase;
    case CharacterCategory::Digits: return useDigits;
    case CharacterCategory::Symbols: return useSymbols;
    case CharacterCategory::Custom: return !customCharacters.isEmpty();
    }
    return false;
}

std::size_t PasswordConfiguration::minimumFor(CharacterCategory category) const noexcept
{
    switch (category) {
    case CharacterCategory::Uppercase: return minimumUppercase;
    case CharacterCategory::Lowercase: return minimumLowercase;
    case CharacterCategory::Digits: return minimumDigits;
    case CharacterCategory::Symbols: return minimumSymbols;
    case CharacterCategory::Custom: return 0;
    }
    return 0;
}

std::size_t PasswordConfiguration::totalMinimum() const noexcept
{
    return minimumUppercase + minimumLowercase + minimumDigits + minimumSymbols;
}

const ConfigurationSchema& passwordConfigurationSchema()
{
    static const ConfigurationSchema schema = [] {
        const auto maxLen = static_cast<qint64>(common::ApplicationLimits::defaults().maxOutputLength);
        constexpr auto minLen = static_cast<qint64>(PasswordLimits::kMinLength);
        const QString lengthGroup = QStringLiteral("Length");
        const QString setsGroup = QStringLiteral("Character sets");
        const QString minGroup = QStringLiteral("Minimum requirements");
        const QString advancedGroup = QStringLiteral("Advanced");

        QList<FieldSpec> fields;
        fields.append(FieldSpec::integer(QStringLiteral("length"), QStringLiteral("Length"), 24,
                                         minLen, maxLen, lengthGroup,
                                         QStringLiteral("Number of characters in each password.")));
        fields.append(FieldSpec::boolean(QStringLiteral("uppercase"), QStringLiteral("Uppercase"),
                                         true, setsGroup));
        fields.append(FieldSpec::boolean(QStringLiteral("lowercase"), QStringLiteral("Lowercase"),
                                         true, setsGroup));
        fields.append(FieldSpec::boolean(QStringLiteral("digits"), QStringLiteral("Numbers"), true,
                                         setsGroup));
        fields.append(FieldSpec::boolean(QStringLiteral("symbols"), QStringLiteral("Symbols"), true,
                                         setsGroup));
        fields.append(FieldSpec::string(
            QStringLiteral("customCharacters"), QStringLiteral("Custom characters"), QString(),
            static_cast<qsizetype>(PasswordLimits::kMaxCustomCharacters), setsGroup,
            QStringLiteral("Extra characters to allow. Whitespace, control and combining "
                           "characters are not accepted.")));
        fields.append(FieldSpec::integer(QStringLiteral("minimumUppercase"),
                                         QStringLiteral("Minimum uppercase"), 0, 0, maxLen,
                                         minGroup));
        fields.append(FieldSpec::integer(QStringLiteral("minimumLowercase"),
                                         QStringLiteral("Minimum lowercase"), 0, 0, maxLen,
                                         minGroup));
        fields.append(FieldSpec::integer(QStringLiteral("minimumDigits"),
                                         QStringLiteral("Minimum numbers"), 0, 0, maxLen,
                                         minGroup));
        fields.append(FieldSpec::integer(QStringLiteral("minimumSymbols"),
                                         QStringLiteral("Minimum symbols"), 0, 0, maxLen,
                                         minGroup));
        fields.append(FieldSpec::string(
            QStringLiteral("excludedCharacters"), QStringLiteral("Excluded characters"), QString(),
            static_cast<qsizetype>(PasswordLimits::kMaxExcludedCharacters), advancedGroup,
            QStringLiteral("Characters that must never appear (applies to every set).")));
        fields.append(FieldSpec::boolean(QStringLiteral("allowDuplicates"),
                                         QStringLiteral("Allow duplicate characters"), true,
                                         advancedGroup,
                                         QStringLiteral("When off, every character appears at most once.")));
        return ConfigurationSchema(std::move(fields));
    }();
    return schema;
}

Result<PasswordConfiguration, ValidationResult>
parsePasswordConfiguration(const QJsonObject& parameters, const common::ApplicationLimits& limits)
{
    using ParseResult = Result<PasswordConfiguration, ValidationResult>;

    const ConfigurationSchema& schema = passwordConfigurationSchema();
    ValidationResult issues = schema.validate(parameters, limits);
    if (!issues.isValid())
        return ParseResult::failure(std::move(issues));

    // Schema validation guarantees presence/type/range of every field after defaults.
    const QJsonObject merged = schema.applyDefaults(parameters);
    const auto size = [&](const char* key) {
        return static_cast<std::size_t>(merged.value(QLatin1String(key)).toDouble());
    };
    const auto flag = [&](const char* key) { return merged.value(QLatin1String(key)).toBool(); };
    const auto text = [&](const char* key) { return merged.value(QLatin1String(key)).toString(); };

    PasswordConfiguration config;
    config.length = size("length");
    config.useUppercase = flag("uppercase");
    config.useLowercase = flag("lowercase");
    config.useDigits = flag("digits");
    config.useSymbols = flag("symbols");
    config.customCharacters = text("customCharacters");
    config.excludedCharacters = text("excludedCharacters");
    config.minimumUppercase = size("minimumUppercase");
    config.minimumLowercase = size("minimumLowercase");
    config.minimumDigits = size("minimumDigits");
    config.minimumSymbols = size("minimumSymbols");
    config.allowDuplicates = flag("allowDuplicates");
    return ParseResult::success(std::move(config));
}

} // namespace account_recommendations::generators::password
