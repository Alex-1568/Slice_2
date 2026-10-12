#include "generators/password/PasswordAlgorithm.hpp"

#include "common/security/SecureWipe.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace account_recommendations::generators::password
{

using common::Result;
using domain::GenerationError;
using domain::IRandomProvider;

namespace
{

// Uniformly picks one element, removes it from `candidates` (swap-remove keeps the
// remaining set intact, so subsequent picks stay uniform over what is left).
char32_t takeRandom(std::u32string& candidates, IRandomProvider& random)
{
    const std::size_t index = random.uniformIndex(candidates.size());
    std::swap(candidates[index], candidates.back());
    const char32_t picked = candidates.back();
    candidates.pop_back();
    return picked;
}

char32_t pickRandom(const std::u32string& pool, IRandomProvider& random)
{
    return pool[random.uniformIndex(pool.size())];
}

void removeValue(std::u32string& candidates, char32_t value)
{
    const auto it = std::find(candidates.begin(), candidates.end(), value);
    if (it != candidates.end()) {
        std::swap(*it, candidates.back());
        candidates.pop_back();
    }
}

} // namespace

void secureShuffle(std::u32string& text, IRandomProvider& random)
{
    for (std::size_t i = text.size(); i > 1; --i) {
        const std::size_t j = random.uniformIndex(i);
        std::swap(text[i - 1], text[j]);
    }
}

Result<std::u32string, GenerationError>
generatePasswordCodePoints(const PasswordConfiguration& config, const PasswordPoolPlan& plan,
                           IRandomProvider& random)
{
    using PasswordResult = Result<std::u32string, GenerationError>;

    if (plan.allowed.empty() || config.length == 0)
        return PasswordResult::failure(GenerationError::constraintViolation(
            QStringLiteral("The configuration leaves no characters to choose from.")));

    std::u32string password;
    password.reserve(config.length);

    if (config.allowDuplicates) {
        for (std::size_t c = 0; c < kStandardCategoryCount; ++c) {
            const auto category = static_cast<CharacterCategory>(c);
            const std::u32string& pool = plan.pool(category);
            for (std::size_t i = 0; i < config.minimumFor(category); ++i) {
                if (pool.empty()) {
                    common::secureWipe(password);
                    return PasswordResult::failure(GenerationError::constraintViolation(
                        QStringLiteral("A required character category has no characters.")));
                }
                password.push_back(pickRandom(pool, random));
            }
        }
        while (password.size() < config.length)
            password.push_back(pickRandom(plan.allowed, random));
    } else {
        std::u32string remaining = plan.allowed;
        for (std::size_t c = 0; c < kStandardCategoryCount; ++c) {
            const auto category = static_cast<CharacterCategory>(c);
            const std::size_t minimum = config.minimumFor(category);
            if (minimum == 0)
                continue;
            std::u32string candidates = plan.pool(category);
            if (candidates.size() < minimum) {
                common::secureWipe(password);
                return PasswordResult::failure(GenerationError::constraintViolation(
                    QStringLiteral("Not enough unique characters for a required category.")));
            }
            for (std::size_t i = 0; i < minimum; ++i) {
                const char32_t picked = takeRandom(candidates, random);
                password.push_back(picked);
                removeValue(remaining, picked);
            }
        }
        if (config.length > password.size() + remaining.size()) {
            common::secureWipe(password);
            return PasswordResult::failure(GenerationError::constraintViolation(
                QStringLiteral("Password length exceeds the number of available unique "
                               "characters.")));
        }
        while (password.size() < config.length)
            password.push_back(takeRandom(remaining, random));
    }

    secureShuffle(password, random);

    if (!satisfiesConstraints(password, config, plan)) {
        common::secureWipe(password);
        return PasswordResult::failure(GenerationError::constraintViolation(
            QStringLiteral("The generated value failed its own constraint check.")));
    }
    return PasswordResult::success(std::move(password));
}

bool satisfiesConstraints(const std::u32string& password, const PasswordConfiguration& config,
                          const PasswordPoolPlan& plan)
{
    if (password.size() != config.length)
        return false;

    std::array<std::size_t, kStandardCategoryCount> counts{};
    for (const char32_t c : password) {
        if (plan.isForbidden(c) || !plan.isAllowed(c))
            return false;
        for (std::size_t i = 0; i < kStandardCategoryCount; ++i) {
            if (plan.inPool(static_cast<CharacterCategory>(i), c))
                ++counts[i];
        }
    }
    for (std::size_t i = 0; i < kStandardCategoryCount; ++i) {
        if (counts[i] < config.minimumFor(static_cast<CharacterCategory>(i)))
            return false;
    }

    if (!config.allowDuplicates) {
        std::u32string sorted = password;
        std::sort(sorted.begin(), sorted.end());
        const bool hasDuplicate = std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end();
        common::secureWipe(sorted);
        if (hasDuplicate)
            return false;
    }
    return true;
}

} // namespace account_recommendations::generators::password
