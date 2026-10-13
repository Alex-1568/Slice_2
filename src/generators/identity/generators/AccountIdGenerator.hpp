#pragma once

#include "generators/identity/common/IdentityGeneratorBase.hpp"

namespace account_recommendations::generators::identity
{

/// Generates synthetic identifiers. IDs are identifiers, not authentication secrets; this
/// generator does not claim cryptographic strength and accepts the ordinary injected RNG.
class AccountIdGenerator final : public IdentityGeneratorBase
{
public:
    [[nodiscard]] domain::GeneratorMetadata metadata() const override;

protected:
    [[nodiscard]] common::ValidationResult validateSpecific(
        const QJsonObject& parameters, const common::ApplicationLimits& limits) const override;
    [[nodiscard]] common::Result<domain::GeneratedRecord, domain::GenerationError>
    generateRecord(const QJsonObject& parameters, const domain::GenerationContext& context) const override;
};

} // namespace account_recommendations::generators::identity
