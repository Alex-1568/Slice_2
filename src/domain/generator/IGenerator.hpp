#pragma once

#include "common/result/Result.hpp"
#include "common/validation/ValidationResult.hpp"
#include "domain/generator/GenerationContext.hpp"
#include "domain/generator/GenerationError.hpp"
#include "domain/generator/GeneratorMetadata.hpp"
#include "domain/generator/GeneratorRequest.hpp"
#include "domain/generator/GeneratorResult.hpp"
#include "domain/generator/IResultSink.hpp"

namespace account_recommendations::domain
{

/// Generator contract.
///
/// Implementations are expected to be stateless/immutable after construction
/// (all methods are const) so one instance can serve concurrent workers.
class IGenerator
{
public:
    virtual ~IGenerator() = default;

    [[nodiscard]] virtual GeneratorMetadata metadata() const = 0;

    /// Structural + semantic validation without generating anything. Independent of
    /// runtime limits (those come from the GenerationContext).
    [[nodiscard]] virtual common::ValidationResult validate(const GeneratorRequest& request) const = 0;

    /// Streaming primitive. Pushes `request.count` records into `sink`, checking
    /// cancellation between records. Cancellation and sink-stop are reported through
    /// the summary status, not as errors.
    [[nodiscard]] virtual common::Result<GenerationSummary, GenerationError>
    generateInto(const GeneratorRequest& request, const GenerationContext& context,
                 IResultSink& sink) const = 0;

    /// Convenience for callers that want everything in memory. On error no partial
    /// records are returned (important for sensitive data).
    [[nodiscard]] common::Result<GeneratorResult, GenerationError>
    generate(const GeneratorRequest& request, const GenerationContext& context) const;

protected:
    IGenerator() = default;
    IGenerator(const IGenerator&) = default;
    IGenerator& operator=(const IGenerator&) = default;

    /// Checks shared by all generators: count >= 1, count <= limits, uniqueness
    /// support, and secure-randomness policy.
    [[nodiscard]] static common::Result<void, GenerationError>
    checkPreconditions(const GeneratorMetadata& metadata, const GeneratorRequest& request,
                       const GenerationContext& context);
};

} // namespace account_recommendations::domain
