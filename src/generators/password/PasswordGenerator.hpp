#pragma once

#include "domain/generator/IGenerator.hpp"
#include "generators/password/EntropyEstimator.hpp"

namespace account_recommendations::generators::password
{

/// Security-sensitive generator. Depends only on domain abstractions: randomness
/// arrives through GenerationContext::random() (an IRandomProvider), never from a
/// concrete source. Stateless after construction, hence safe to share across workers.
///
/// Output record (all numeric fields describe the CONFIGURATION, not the password):
///   password              sensitive
///   guaranteedEntropyBits lower bound on min-entropy (use this for display)
///   searchSpaceBits       upper bound (never present as "strength")
///   entropyQuality        "exact" | "bounded"
class PasswordGenerator final : public domain::IGenerator
{
public:
    explicit PasswordGenerator(EntropyBandThresholds thresholds = {});

    [[nodiscard]] static QString generatorId();

    [[nodiscard]] domain::GeneratorMetadata metadata() const override;

    [[nodiscard]] common::ValidationResult
    validate(const domain::GeneratorRequest& request) const override;

    [[nodiscard]] common::Result<domain::GenerationSummary, domain::GenerationError>
    generateInto(const domain::GeneratorRequest& request, const domain::GenerationContext& context,
                 domain::IResultSink& sink) const override;

private:
    struct Prepared
    {
        PasswordConfiguration config;
        PasswordPoolPlan plan;
        GeneratorEntropyEstimate entropy;
        common::ValidationResult validation; // warnings only (errors => failure)
    };

    [[nodiscard]] common::Result<Prepared, common::ValidationResult>
    prepare(const domain::GeneratorRequest& request, const common::ApplicationLimits& limits) const;

    EntropyBandThresholds m_thresholds;
};

} // namespace account_recommendations::generators::password
