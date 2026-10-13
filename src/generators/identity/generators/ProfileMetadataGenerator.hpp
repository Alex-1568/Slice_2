#pragma once

#include "generators/identity/common/IdentityGeneratorBase.hpp"
#include "generators/identity/data/NameDataProvider.hpp"

#include <memory>

namespace account_recommendations::generators::identity
{

/// Emits a flat set of synthetic profile fields. Every field is scalar to comply with the
/// framework's fail-closed flat-record contract; this generator does not create real PII.
class ProfileMetadataGenerator final : public IdentityGeneratorBase
{
public:
    explicit ProfileMetadataGenerator(std::shared_ptr<const INameDataProvider> provider = defaultNameDataProvider());
    [[nodiscard]] domain::GeneratorMetadata metadata() const override;

protected:
    [[nodiscard]] common::ValidationResult validateSpecific(
        const QJsonObject& parameters, const common::ApplicationLimits& limits) const override;
    [[nodiscard]] common::Result<domain::GeneratedRecord, domain::GenerationError>
    generateRecord(const QJsonObject& parameters, const domain::GenerationContext& context) const override;
    [[nodiscard]] QString uniqueKey(const domain::GeneratedRecord& record) const override;

private:
    std::shared_ptr<const INameDataProvider> m_provider;
};

} // namespace account_recommendations::generators::identity
