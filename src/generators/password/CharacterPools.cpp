#include "generators/password/CharacterPools.hpp"

#include <QChar>

#include <algorithm>
#include <iterator>

namespace account_recommendations::generators::password
{

using common::Result;
using common::ValidationResult;

CharacterPools standardPools()
{
    CharacterPools pools;
    pools.uppercase = U"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    pools.lowercase = U"abcdefghijklmnopqrstuvwxyz";
    pools.digits = U"0123456789";
    pools.symbols = U"!@#$%^&*()-_=+[]{}:;,.?/~";
    return pools;
}

namespace
{

void sortUnique(std::u32string& text)
{
    std::sort(text.begin(), text.end());
    text.erase(std::unique(text.begin(), text.end()), text.end());
}

std::u32string difference(const std::u32string& a, const std::u32string& b)
{
    std::u32string result;
    std::set_difference(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(result));
    return result;
}

std::u32string unite(const std::u32string& a, const std::u32string& b)
{
    std::u32string result;
    std::set_union(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(result));
    return result;
}

std::u32string filtered(std::u32string source, bool enabled, const std::u32string& excluded)
{
    if (!enabled)
        return {};
    sortUnique(source);
    return difference(source, excluded);
}

bool containsSorted(const std::u32string& sorted, char32_t c) noexcept
{
    return std::binary_search(sorted.begin(), sorted.end(), c);
}

} // namespace

bool PasswordPoolPlan::isAllowed(char32_t c) const noexcept
{
    return containsSorted(allowed, c);
}

bool PasswordPoolPlan::isForbidden(char32_t c) const noexcept
{
    return containsSorted(forbidden, c);
}

bool PasswordPoolPlan::inPool(CharacterCategory category, char32_t c) const noexcept
{
    return containsSorted(pool(category), c);
}

bool decodeCharacters(const QString& text, std::u32string& out)
{
    out.clear();
    const QString normalized = text.normalized(QString::NormalizationForm_C);
    const qsizetype size = normalized.size();
    out.reserve(static_cast<std::size_t>(size));
    for (qsizetype i = 0; i < size; ++i) {
        const QChar unit = normalized.at(i);
        if (unit.isHighSurrogate()) {
            if (i + 1 >= size || !normalized.at(i + 1).isLowSurrogate())
                return false;
            out.push_back(QChar::surrogateToUcs4(unit, normalized.at(i + 1)));
            ++i;
        } else if (unit.isLowSurrogate()) {
            return false;
        } else {
            out.push_back(unit.unicode());
        }
    }
    return true;
}

bool isAcceptableCustomCharacter(char32_t c)
{
    switch (QChar::category(c)) {
    case QChar::Mark_NonSpacing:
    case QChar::Mark_SpacingCombining:
    case QChar::Mark_Enclosing:
    case QChar::Separator_Space:
    case QChar::Separator_Line:
    case QChar::Separator_Paragraph:
    case QChar::Other_Control:
    case QChar::Other_Format:
    case QChar::Other_Surrogate:
    case QChar::Other_PrivateUse:
    case QChar::Other_NotAssigned:
        return false;
    default:
        return true;
    }
}

Result<PasswordPoolPlan, ValidationResult> buildPoolPlan(const PasswordConfiguration& config)
{
    using PlanResult = Result<PasswordPoolPlan, ValidationResult>;

    ValidationResult issues;
    std::u32string custom;
    std::u32string excluded;

    if (static_cast<std::size_t>(config.customCharacters.size()) > PasswordLimits::kMaxCustomCharacters) {
        issues.addError(QStringLiteral("customCharacters"), QStringLiteral("too_long"),
                        QStringLiteral("Custom characters must be at most %1 characters.")
                            .arg(PasswordLimits::kMaxCustomCharacters));
    } else if (!decodeCharacters(config.customCharacters, custom)) {
        issues.addError(QStringLiteral("customCharacters"), QStringLiteral("invalid_encoding"),
                        QStringLiteral("Custom characters contain invalid text."));
    } else if (!std::all_of(custom.begin(), custom.end(), isAcceptableCustomCharacter)) {
        issues.addError(QStringLiteral("customCharacters"), QStringLiteral("invalid_character"),
                        QStringLiteral("Custom characters must not contain whitespace, control "
                                       "or combining characters."));
    }

    if (static_cast<std::size_t>(config.excludedCharacters.size()) > PasswordLimits::kMaxExcludedCharacters) {
        issues.addError(QStringLiteral("excludedCharacters"), QStringLiteral("too_long"),
                        QStringLiteral("Excluded characters must be at most %1 characters.")
                            .arg(PasswordLimits::kMaxExcludedCharacters));
    } else if (!decodeCharacters(config.excludedCharacters, excluded)) {
        issues.addError(QStringLiteral("excludedCharacters"), QStringLiteral("invalid_encoding"),
                        QStringLiteral("Excluded characters contain invalid text."));
    }

    if (!issues.isValid())
        return PlanResult::failure(std::move(issues));

    sortUnique(custom);
    sortUnique(excluded);

    const CharacterPools base = standardPools();
    PasswordPoolPlan plan;
    plan.forbidden = excluded;
    plan.pools[static_cast<std::size_t>(CharacterCategory::Uppercase)] =
        filtered(base.uppercase, config.useUppercase, excluded);
    plan.pools[static_cast<std::size_t>(CharacterCategory::Lowercase)] =
        filtered(base.lowercase, config.useLowercase, excluded);
    plan.pools[static_cast<std::size_t>(CharacterCategory::Digits)] =
        filtered(base.digits, config.useDigits, excluded);
    plan.pools[static_cast<std::size_t>(CharacterCategory::Symbols)] =
        filtered(base.symbols, config.useSymbols, excluded);

    // Make the custom pool disjoint from the enabled standard pools.
    std::u32string standardUnion;
    for (std::size_t i = 0; i < kStandardCategoryCount; ++i)
        standardUnion = unite(standardUnion, plan.pools[i]);
    plan.pools[static_cast<std::size_t>(CharacterCategory::Custom)] =
        difference(difference(custom, excluded), standardUnion);

    for (const std::u32string& pool : plan.pools)
        plan.allowed = unite(plan.allowed, pool);

    return PlanResult::success(std::move(plan));
}

} // namespace account_recommendations::generators::password
