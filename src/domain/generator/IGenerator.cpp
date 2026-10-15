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
    ValidatingSink(const GeneratorMetadata& metadata, IResultSink& destination,
                   qsizetype maximumRecords, std::size_t maximumStringLength)
        : m_metadata(metadata), m_destination(destination), m_maximumRecords(maximumRecords),
          m_maximumStringLength(maximumStringLength)
    {
    }

    [[nodiscard]] SinkResponse accept(GeneratedRecord&& record) override
    {
        if (m_error)
            return SinkResponse::Stop;

        // A sink's Stop response is a contract boundary. A buggy or third-party generator
        // must not be able to ignore it and keep forwarding records downstream.
        if (m_stopRequested) {
            m_error = GenerationError::internal(
                QStringLiteral("Generator continued after the result sink requested a stop."));
            return SinkResponse::Stop;
        }

        // Bound the number of records independently of generator implementation. This is
        // especially important for runtime plugins that accidentally ignore request.count.
        if (m_submittedRecords >= m_maximumRecords) {
            m_error = GenerationError::limitExceeded(
                QStringLiteral("Generator emitted more records than the request count."));
            return SinkResponse::Stop;
        }

        common::ValidationResult issues = validateRecord(m_metadata, record);
        if (!issues.isValid()) {
            m_error = GenerationError::invalidOutputRecord(std::move(issues));
            return SinkResponse::Stop; // invalid record is never passed to destination
        }
        for (auto it = record.constBegin(); it != record.constEnd(); ++it) {
            if (it.value().isString()
                && static_cast<std::size_t>(it.value().toString().size()) > m_maximumStringLength) {
                m_error = GenerationError::limitExceeded(
                    QStringLiteral("Generated output exceeds the configured string-length limit."));
                return SinkResponse::Stop;
            }
        }

        // `produced` means submitted to the sink, including the final record on which the
        // sink returns Stop. All built-in and example generators follow this convention.
        ++m_submittedRecords;
        const SinkResponse response = m_destination.accept(std::move(record));
        if (response == SinkResponse::Stop)
            m_stopRequested = true;
        return response;
    }

    [[nodiscard]] const std::optional<GenerationError>& error() const noexcept { return m_error; }
    [[nodiscard]] bool stopRequested() const noexcept { return m_stopRequested; }
    [[nodiscard]] qsizetype submittedRecords() const noexcept { return m_submittedRecords; }

private:
    const GeneratorMetadata& m_metadata;
    IResultSink& m_destination;
    qsizetype m_maximumRecords;
    std::size_t m_maximumStringLength;
    qsizetype m_submittedRecords = 0;
    bool m_stopRequested = false;
    std::optional<GenerationError> m_error;
};
} // namespace

using common::Result;

Result<GenerationSummary, GenerationError>
IGenerator::generateInto(const GeneratorRequest& request, const GenerationContext& context,
                         IResultSink& sink) const
{
    using SummaryResult = Result<GenerationSummary, GenerationError>;

    GeneratorMetadata md;
    try {
        md = metadata();
    } catch (...) {
        return SummaryResult::failure(GenerationError::internal(
            QStringLiteral("The generator threw an exception while describing its metadata.")));
    }
    if (request.generatorId != md.id)
        return SummaryResult::failure(GenerationError::invalidRequest(
            QStringLiteral("Request generatorId does not match this generator.")));

    const common::ValidationResult metadataIssues = validateMetadata(md);
    if (!metadataIssues.isValid())
        return SummaryResult::failure(GenerationError::constraintViolation(
            QStringLiteral("Generator metadata is invalid; generation was refused.")));

    if (auto precondition = checkPreconditions(md, request, context); !precondition)
        return SummaryResult::failure(std::move(precondition).error());

    // Validate at the public boundary instead of relying on every implementation (especially
    // runtime plugins) to remember to call its own validator. Built-ins may revalidate while
    // preparing their runtime-specific plans; this first pass guarantees invalid requests do not
    // reach a generator implementation at all.
    common::ValidationResult requestIssues;
    try {
        requestIssues = validate(request);
    } catch (...) {
        return SummaryResult::failure(GenerationError::internal(
            QStringLiteral("The generator threw an exception while validating the request.")));
    }
    if (!requestIssues.isValid())
        return SummaryResult::failure(GenerationError::invalidConfiguration(std::move(requestIssues)));

    ValidatingSink validatingSink(md, sink, request.count, context.limits().maxOutputLength);
    SummaryResult result = [&]() -> SummaryResult {
        try {
            return generateIntoImpl(request, context, validatingSink);
        } catch (const RandomSourceError&) {
            return SummaryResult::failure(GenerationError::randomSourceFailure());
        } catch (...) {
            // Do not surface arbitrary exception messages: plugin exceptions may contain
            // generated values or other sensitive data. Streaming rows already delivered
            // cannot be rolled back; the batch convenience API discards its local sink.
            return SummaryResult::failure(GenerationError::internal(
                QStringLiteral("The generator or result sink threw an exception.")));
        }
    }();

    // An invalid record takes precedence over a normal completion or sink-stop response.
    // The offending record has not reached the downstream sink.
    if (validatingSink.error())
        return SummaryResult::failure(*validatingSink.error());

    if (!result)
        return result;

    const GenerationSummary summary = result.value();
    if (summary.produced != validatingSink.submittedRecords())
        return SummaryResult::failure(GenerationError::internal(
            QStringLiteral("Generator summary does not match the records submitted to the sink.")));

    if (validatingSink.stopRequested()) {
        if (summary.status != CompletionStatus::StoppedBySink)
            return SummaryResult::failure(GenerationError::internal(
                QStringLiteral("Generator did not report the sink's stop request.")));
        return result;
    }

    if (summary.status == CompletionStatus::StoppedBySink)
        return SummaryResult::failure(GenerationError::internal(
            QStringLiteral("Generator reported a sink stop that did not occur.")));

    if (summary.status == CompletionStatus::Completed
        && validatingSink.submittedRecords() != request.count)
        return SummaryResult::failure(GenerationError::internal(
            QStringLiteral("Generator completed without producing the requested number of records.")));

    return result;
}

Result<GeneratorResult, GenerationError>
IGenerator::generate(const GeneratorRequest& request, const GenerationContext& context) const
{
    using BatchResult = Result<GeneratorResult, GenerationError>;

    CollectingSink sink;
    // Bounded pre-allocation: never trust `count` before it has been validated. In particular,
    // do not pass a negative capacity to QList::reserve for malformed requests.
    if (request.count > 0)
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
    if (static_cast<std::size_t>(request.parameters.size()) > context.limits().maxParameterCount)
        return VoidResult::failure(GenerationError::limitExceeded(
            QStringLiteral("Parameter count exceeds the configured limit.")));

    // Current ConfigurationSchema fields are scalar. Enforce that public contract centrally
    // before invoking plugin-controlled validation code, so a plugin cannot accidentally bypass
    // request size/type ceilings by omitting its own schema checks.
    common::ValidationResult parameterIssues;
    for (auto it = request.parameters.constBegin(); it != request.parameters.constEnd(); ++it) {
        if (it.value().isObject() || it.value().isArray()) {
            parameterIssues.addError(it.key(), QStringLiteral("nested_parameter_value"),
                QStringLiteral("Nested object and array parameters are not supported by the flat configuration contract."));
        } else if (it.value().isString()
                   && static_cast<std::size_t>(it.value().toString().size()) > context.limits().maxStringLength) {
            parameterIssues.addError(it.key(), QStringLiteral("too_long"),
                QStringLiteral("Parameter string exceeds the configured length limit."));
        }
        if (static_cast<std::size_t>(it.key().size()) > context.limits().maxStringLength) {
            parameterIssues.addError(QStringLiteral("parameters"), QStringLiteral("key_too_long"),
                QStringLiteral("Parameter key exceeds the configured length limit."));
        }
    }
    if (!parameterIssues.isValid())
        return VoidResult::failure(GenerationError::invalidConfiguration(std::move(parameterIssues)));

    if (metadata.requiresSecureRandom && !context.random().isCryptographicallySecure()
        && !context.insecureRandomPermitted())
        return VoidResult::failure(GenerationError::insecureRandomSource());
    return VoidResult::success();
}

} // namespace account_recommendations::domain
