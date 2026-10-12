#include "generators/password/PasswordValidator.hpp"

namespace account_recommendations::generators::password
{

using common::ValidationResult;

namespace
{

struct CategoryText
{
    CharacterCategory category;
    const char* minimumKey;
    const char* noun; // plural, lowercase
};

constexpr CategoryText kStandardCategories[] = {
    {CharacterCategory::Uppercase, "minimumUppercase", "uppercase characters"},
    {CharacterCategory::Lowercase, "minimumLowercase", "lowercase characters"},
    {CharacterCategory::Digits, "minimumDigits", "numbers"},
    {CharacterCategory::Symbols, "minimumSymbols", "symbols"},
};

} // namespace

ValidationResult PasswordConfigurationValidator::validate(const PasswordConfiguration& config,
                                                          const common::ApplicationLimits& limits)
{
    auto plan = buildPoolPlan(config);
    if (!plan)
        return plan.error();
    return validate(config, plan.value(), limits);
}

ValidationResult PasswordConfigurationValidator::validate(const PasswordConfiguration& config,
                                                          const PasswordPoolPlan& plan,
                                                          const common::ApplicationLimits& limits)
{
    ValidationResult result;

    if (config.length < PasswordLimits::kMinLength || config.length > limits.maxOutputLength)
        result.addError(QStringLiteral("length"), QStringLiteral("out_of_range"),
                        QStringLiteral("Length must be between %1 and %2.")
                            .arg(PasswordLimits::kMinLength)
                            .arg(limits.maxOutputLength));

    for (const CategoryText& entry : kStandardCategories) {
        const QString key = QLatin1String(entry.minimumKey);
        const QString noun = QLatin1String(entry.noun);
        const std::size_t minimum = config.minimumFor(entry.category);
        const std::size_t poolSize = plan.pool(entry.category).size();

        if (minimum == 0) {
            if (config.isEnabled(entry.category) && poolSize == 0)
                result.addWarning(key, QStringLiteral("pool_emptied"),
                                  QStringLiteral("All %1 are excluded, so none will be used.")
                                      .arg(noun));
            continue;
        }
        if (!config.isEnabled(entry.category)) {
            result.addError(key, QStringLiteral("category_disabled"),
                            QStringLiteral("At least %1 %2 are required, but %2 are turned off.")
                                .arg(minimum)
                                .arg(noun));
        } else if (poolSize == 0) {
            result.addError(key, QStringLiteral("pool_empty"),
                            QStringLiteral("At least %1 %2 are required, but none remain after "
                                           "exclusions.")
                                .arg(minimum)
                                .arg(noun));
        } else if (!config.allowDuplicates && minimum > poolSize) {
            result.addError(key, QStringLiteral("insufficient_unique_characters"),
                            QStringLiteral("At least %1 %2 are required, but only %3 unique ones "
                                           "are available and duplicates are not allowed.")
                                .arg(minimum)
                                .arg(noun)
                                .arg(poolSize));
        }
    }

    if (config.totalMinimum() > config.length)
        result.addError(QStringLiteral("length"), QStringLiteral("minimums_exceed_length"),
                        QStringLiteral("The minimum requirements add up to %1, which is more than "
                                       "the password length (%2).")
                            .arg(config.totalMinimum())
                            .arg(config.length));

    if (plan.allowed.empty()) {
        result.addError(QStringLiteral("length"), QStringLiteral("no_allowed_characters"),
                        QStringLiteral("No characters are available. Turn on a character set or "
                                       "add custom characters, and check the exclusions."));
    } else if (!config.allowDuplicates && config.length > plan.allowed.size()) {
        result.addError(QStringLiteral("length"), QStringLiteral("insufficient_unique_characters"),
                        QStringLiteral("Password length exceeds the number of available unique "
                                       "characters."));
    }

    return result;
}

} // namespace account_recommendations::generators::password
