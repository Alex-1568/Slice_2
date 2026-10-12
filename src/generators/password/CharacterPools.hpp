#pragma once

#include "common/result/Result.hpp"
#include "common/validation/ValidationResult.hpp"
#include "generators/password/PasswordConfiguration.hpp"

#include <QString>

#include <array>
#include <string>

namespace account_recommendations::generators::password
{

/// Raw, unfiltered character sets.
struct CharacterPools
{
    std::u32string uppercase;
    std::u32string lowercase;
    std::u32string digits;
    std::u32string symbols;
    std::u32string custom;
};

/// The built-in sets (symbols are a conservative, widely accepted subset).
[[nodiscard]] CharacterPools standardPools();

/// Filtered, normalised pools ready for sampling.
///
/// Invariants (established by buildPoolPlan):
///  - every pool is sorted and duplicate-free;
///  - pools are pairwise DISJOINT, so a character is never counted twice;
///  - no pool contains a forbidden character;
///  - disabled categories have an empty pool;
///  - `allowed` is the sorted union of all pools.
/// A custom character that already belongs to an enabled standard category lives
/// in that category (e.g. custom "ABC123" with digits enabled adds only "ABC" to
/// the custom pool when uppercase is also enabled -> it adds nothing).
struct PasswordPoolPlan
{
    std::array<std::u32string, kCategoryCount> pools;
    std::u32string allowed;
    std::u32string forbidden;

    [[nodiscard]] const std::u32string& pool(CharacterCategory category) const noexcept
    {
        return pools[static_cast<std::size_t>(category)];
    }
    [[nodiscard]] bool isAllowed(char32_t c) const noexcept;
    [[nodiscard]] bool isForbidden(char32_t c) const noexcept;
    [[nodiscard]] bool inPool(CharacterCategory category, char32_t c) const noexcept;
};

/// Decodes text to NFC-normalised code points. Returns false on invalid UTF-16
/// (lone surrogates). Working in code points keeps non-BMP characters atomic.
[[nodiscard]] bool decodeCharacters(const QString& text, std::u32string& out);

/// Custom characters must be single, visible, standalone code points.
[[nodiscard]] bool isAcceptableCustomCharacter(char32_t c);

/// Builds the plan or reports why the configuration's character text is unusable.
[[nodiscard]] common::Result<PasswordPoolPlan, common::ValidationResult>
buildPoolPlan(const PasswordConfiguration& config);

} // namespace account_recommendations::generators::password
