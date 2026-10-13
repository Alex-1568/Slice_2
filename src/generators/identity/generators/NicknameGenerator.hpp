#pragma once

#include "generators/identity/common/IdentityGeneratorBase.hpp"
#include "generators/identity/data/NameDataProvider.hpp"

#include <memory>

namespace account_recommendations::generators::identity
{

class NicknameGenerator final : public IdentityGeneratorBase
{
public:
    explicit NicknameGenerator(std::shared_ptr<const INameDataProvider> provider = defaultNameDataProvider());
    [[nodiscard]] domain::GeneratorMetadata metadata() const override;

protected:
    [[nodiscard]] common::ValidationResult validateSpecific(
        const QJsonObject& parameters, const common::ApplicationLimits& limits) const override;
    [[nodiscard]] common::Result<domain::GeneratedRecord, domain::GenerationError>
    generateRecord(const QJsonObject& parameters, const domain::GenerationContext& context) const override;

private:
    std::shared_ptr<const INameDataProvider> m_provider;
};

} // namespace account_recommendations::generators::identity
