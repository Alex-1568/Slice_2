#pragma once

#include "common/result/Result.hpp"
#include "domain/generator/GenerationError.hpp"
#include "domain/random/IRandomProvider.hpp"
#include "generators/password/CharacterPools.hpp"
#include "generators/password/PasswordConfiguration.hpp"

#include <string>

namespace account_recommendations::generators::password
{

/// In-place Fisher-Yates shuffle driven by unbiased uniformIndex().
/// May throw domain::RandomSourceError.
void secureShuffle(std::u32string& text, domain::IRandomProvider& random);

/// Generates one password as code points.
///
/// Algorithm: (1) draw each category's required minimum, (2) fill the remaining
/// positions from the combined allowed pool, (3) Fisher-Yates shuffle, (4)
/// re-verify every constraint. With duplicates disabled all draws are made without
/// replacement. Preconditions: config validated, plan built from the same config.
///
/// May throw domain::RandomSourceError (callers translate it). The caller owns the
/// returned buffer and should wipe it after use.
[[nodiscard]] common::Result<std::u32string, domain::GenerationError>
generatePasswordCodePoints(const PasswordConfiguration& config, const PasswordPoolPlan& plan,
                           domain::IRandomProvider& random);

/// Independent post-condition check used by the generator and by tests.
[[nodiscard]] bool satisfiesConstraints(const std::u32string& password,
                                        const PasswordConfiguration& config,
                                        const PasswordPoolPlan& plan);

} // namespace account_recommendations::generators::password
