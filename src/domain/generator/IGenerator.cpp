#include "domain/generator/IGenerator.hpp"

#include <algorithm>
#include <optional>
#include <utility>

namespace account_recommendations::domain
{

namespace
{
/// Stops invalid records at the single public streaming boundary. Once an error occurs,
/// subsequent calls remain stopped even if a buggy implementation ignores SinkResponse::Stop.
class ValidatingSink final : public IResultSink
{
public:
    ValidatingSink(const GeneratorMetadata& metadata, IResultSink& destination)
        : m_metadata(metadata), m_destination(destination)
    {
    }

    [[nodiscard]] SinkResponse accept(GeneratedRecord&& record) override
    {
        if (m_error)
            return SinkResponse::Stop;

        common::ValidationResult issues = validateRecord(m_metadata, record);
        if (!issues.isValid()) {
            m_error = GenerationError::invalidOutputRecord(std::move(issues));
            return SinkResponse::Stop; // invalid record is never passed to destination
        }
        return m_destination.accept(std::move(record));
    }

    [[nodiscard]] const std::optional<GenerationError>& error() const noexcept { return m_error; }

private:
    const GeneratorMetadata& m_metadata;
    IResultSink& m_destination;
    std::optional<GenerationError> m_error;
};
} // namespace

using common::Result;

Result<GenerationSummary, GenerationError>
IGenerator::generateInto(const GeneratorRequest& request, const GenerationContext& context,
                         IResultSink& sink) const
{
    using SummaryResult = Result<GenerationSummary, GenerationError>;

    const GeneratorMetadata md = metadata();
    if (request.generatorId != md.id)
        return SummaryResult::failure(GenerationError::invalidRequest(
            QStringLiteral("Request generatorId does not match this generator.")));

    const common::ValidationResult metadataIssues = validateMetadata(md);
    if (!metadataIssues.isValid())
        return SummaryResult::failure(GenerationError::constraintViolation(
            QStringLiteral("Generator metadata is invalid; generation was refused.")));

    if (auto precondition = checkPreconditions(md, request, context); !precondition)
        return SummaryResult::failure(std::move(precondition).error());

    ValidatingSink validatingSink(md, sink);
    auto result = generateIntoImpl(request, context, validatingSink);

    // An invalid record takes precedence over a normal completion or sink-stop response.
    // The offending record has not reached the downstream sink.
    if (validatingSink.error())
        return SummaryResult::failure(*validatingSink.error());

    return result;
}

Result<GeneratorResult, GenerationError>
IGenerator::generate(const GeneratorRequest& request, const GenerationContext& context) const
{
    using BatchResult = Result<GeneratorResult, GenerationError>;

    CollectingSink sink;
    // Bounded pre-allocation: never trust `count` before it has been validated.
    sink.reserve(std::min<qsizetype>(request.count, 4096));

    auto summary = generateInto(request, context, sink);
    if (!summary)
        return BatchResult::failure(std::move(summary).error());

    // Batch generation is atomic at the API boundary. If cancellation happens after
    // some records were collected, discard the local sink and return only Cancelled.
    // Streaming calls intentionally keep already-delivered records (no rollback claim).
    if (summary.value().status == CompletionStatus::Cancelled)
        return BatchResult::failure(GenerationError::cancelled());
    if (summary.value().status == CompletionStatus::StoppedBySink)
        return BatchResult::failure(GenerationError::internal(
            QStringLiteral("The in-memory batch sink stopped unexpectedly.")));

    GeneratorResult result;
    result.records = sink.take();
    result.summary = summary.value();
    return BatchResult::success(std::move(result));
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
