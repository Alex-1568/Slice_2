#pragma once

#include "common/limits/ApplicationLimits.hpp"
#include "common/result/Result.hpp"
#include "common/validation/ValidationResult.hpp"
#include "domain/generator/ConfigurationSchema.hpp"

#include <QJsonObject>
#include <QString>

#include <cstddef>

namespace account_recommendations::generators::password
{

/// Generator-intrinsic bounds. The MAXIMUM length is not here: it comes from
/// ApplicationLimits::maxOutputLength so that all resource ceilings live in one place.
struct PasswordLimits
{
    static constexpr std::size_t kMinLength = 4;
    static constexpr std::size_t kMaxCustomCharacters = 256;   // UTF-16 units
    static constexpr std::size_t kMaxExcludedCharacters = 256; // UTF-16 units
};

/// Disjoint character categories. The first four are the "standard" categories
/// that can carry minimum-count requirements; Custom has none.
enum class CharacterCategory : std::size_t
{
    Uppercase = 0,
    Lowercase,
    Digits,
    Symbols,
    Custom
};

inline constexpr std::size_t kCategoryCount = 5;
inline constexpr std::size_t kStandardCategoryCount = 4;

/// Strongly typed configuration, produced by parsePasswordConfiguration() after
/// schema validation. Cross-field rules are checked by PasswordConfigurationValidator.
struct PasswordConfiguration
{
    std::size_t length = 24;

    bool useUppercase = true;
    bool useLowercase = true;
    bool useDigits = true;
    bool useSymbols = true;

    QString customCharacters;
    QString excludedCharacters;

    std::size_t minimumUppercase = 0;
    std::size_t minimumLowercase = 0;
    std::size_t minimumDigits = 0;
    std::size_t minimumSymbols = 0;

    bool allowDuplicates = true;

    [[nodiscard]] bool isEnabled(CharacterCategory category) const noexcept;
    [[nodiscard]] std::size_t minimumFor(CharacterCategory category) const noexcept;
    [[nodiscard]] std::size_t totalMinimum() const noexcept;
};

/// Application-specific schema driving the dynamic GUI form and generic validation.
[[nodiscard]] const domain::ConfigurationSchema& passwordConfigurationSchema();

/// Schema validation (types, ranges, unknown keys) + defaults + conversion.
/// Does NOT perform cross-field validation.
[[nodiscard]] common::Result<PasswordConfiguration, common::ValidationResult>
parsePasswordConfiguration(const QJsonObject& parameters,
                           const common::ApplicationLimits& limits = common::ApplicationLimits::defaults());

} // namespace account_recommendations::generators::password
