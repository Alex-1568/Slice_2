#pragma once

#include "common/limits/ApplicationLimits.hpp"
#include "common/validation/ValidationResult.hpp"
#include "generators/password/CharacterPools.hpp"
#include "generators/password/PasswordConfiguration.hpp"

namespace account_recommendations::generators::password
{

/// Cross-field rules that the schema cannot express.
class PasswordConfigurationValidator
{
public:
    /// Builds the pool plan itself (convenience for callers that only validate).
    [[nodiscard]] static common::ValidationResult
    validate(const PasswordConfiguration& config,
             const common::ApplicationLimits& limits = common::ApplicationLimits::defaults());

    /// Validates against an already-built plan.
    [[nodiscard]] static common::ValidationResult
    validate(const PasswordConfiguration& config, const PasswordPoolPlan& plan,
             const common::ApplicationLimits& limits = common::ApplicationLimits::defaults());
};

} // namespace account_recommendations::generators::password
