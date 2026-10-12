#include "domain/generator/IGenerator.hpp"

#include <algorithm>
#include <utility>

namespace account_recommendations::domain
{

using common::Result;

common::Result<GeneratorResult, GenerationError>
IGenerator::generate(const GeneratorRequest& request, const GenerationContext& context) const
{
    CollectingSink sink;
    // Bounded pre-allocation: never trust `count` before it has been validated.
    sink.reserve(std::min<qsizetype>(request.count, 4096));

    auto summary = generateInto(request, context, sink);
    if (!summary)
        return Result<GeneratorResult, GenerationError>::failure(std::move(summary).error());

    GeneratorResult result;
    result.records = sink.take();
    result.summary = summary.value();
    return Result<GeneratorResult, GenerationError>::success(std::move(result));
}

common::Result<void, GenerationError>
IGenerator::checkPreconditions(const GeneratorMetadata& metadata, const GeneratorRequest& request,
                               const GenerationContext& context)
{
    using VoidResult = Result<void, GenerationError>;

    if (request.count < 1)
        return VoidResult::failure(GenerationError::invalidRequest(
            QStringLiteral("Count must be at least 1.")));
    if (static_cast<std::size_t>(request.count) > context.limits().maxBatchSize)
        return VoidResult::failure(GenerationError::limitExceeded(
            QStringLiteral("Count exceeds the maximum batch size of %1.")
                .arg(context.limits().maxBatchSize)));
    if (request.unique && !metadata.supportsUniqueness)
        return VoidResult::failure(GenerationError::invalidRequest(
            QStringLiteral("This generator does not support unique batches.")));
    if (metadata.requiresSecureRandom && !context.random().isCryptographicallySecure()
        && !context.insecureRandomPermitted())
        return VoidResult::failure(GenerationError::insecureRandomSource());
    return VoidResult::success();
}

} // namespace account_recommendations::domain
