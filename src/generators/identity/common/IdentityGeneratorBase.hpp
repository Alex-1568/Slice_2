#pragma once

#include "domain/generator/IGenerator.hpp"

namespace account_recommendations::generators::identity
{

/// Shared lifecycle for synthetic identity generators. It centralizes schema validation,
/// cancellation, bounded uniqueness, random-source failure handling, output-size limits, and
/// streaming semantics. Concrete generators only implement metadata and record construction.
class IdentityGeneratorBase : public domain::IGenerator
{
public:
    [[nodiscard]] common::ValidationResult validate(const domain::GeneratorRequest& request) const final;

protected:
    IdentityGeneratorBase() = default;

    [[nodiscard]] virtual common::ValidationResult validateSpecific(
        const QJsonObject& parameters, const common::ApplicationLimits& limits) const;

    [[nodiscard]] virtual common::Result<domain::GeneratedRecord, domain::GenerationError>
    generateRecord(const QJsonObject& parameters, const domain::GenerationContext& context) const = 0;

    /// Unique identity key within a batch. The default is the first declared output field.
    [[nodiscard]] virtual QString uniqueKey(const domain::GeneratedRecord& record) const;

private:
    [[nodiscard]] common::ValidationResult validateRequest(
        const domain::GeneratorRequest& request, const common::ApplicationLimits& limits) const;

    [[nodiscard]] common::Result<domain::GenerationSummary, domain::GenerationError>
    generateIntoImpl(const domain::GeneratorRequest& request, const domain::GenerationContext& context,
                     domain::IResultSink& validatedSink) const final;
};

} // namespace account_recommendations::generators::identity
