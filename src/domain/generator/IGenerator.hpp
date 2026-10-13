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

    /// Validated streaming boundary. This method is final so implementations cannot
    /// bypass metadata/record validation. A valid record is forwarded to the caller's
    /// sink; an invalid record is withheld and returned as InvalidOutputRecord.
    ///
    /// Streaming is not transactional: valid records delivered before a later error or
    /// cancellation cannot be rolled back. See docs/API_AUDIT.md.
    [[nodiscard]] virtual common::Result<GenerationSummary, GenerationError>
    generateInto(const GeneratorRequest& request, const GenerationContext& context,
                 IResultSink& sink) const final;

    /// Convenience for callers that want everything in memory. On error no partial
    /// records are returned (important for sensitive data).
    [[nodiscard]] common::Result<GeneratorResult, GenerationError>
    generate(const GeneratorRequest& request, const GenerationContext& context) const;

protected:
    IGenerator() = default;
    IGenerator(const IGenerator&) = default;
    IGenerator& operator=(const IGenerator&) = default;

    /// Implementations produce records only through this protected hook. The public
    /// final generateInto() wraps the sink and enforces the declared output contract.
    [[nodiscard]] virtual common::Result<GenerationSummary, GenerationError>
    generateIntoImpl(const GeneratorRequest& request, const GenerationContext& context,
                     IResultSink& validatedSink) const = 0;

    /// Checks shared by all generators: request id, count/limits, uniqueness support,
    /// and secure-randomness policy. Called by the non-bypassable public boundary.
    [[nodiscard]] static common::Result<void, GenerationError>
    checkPreconditions(const GeneratorMetadata& metadata, const GeneratorRequest& request,
                       const GenerationContext& context);
};

} // namespace account_recommendations::domain
