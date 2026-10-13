#include "generators/identity/common/IdentityGeneratorBase.hpp"

#include <QSet>

#include <algorithm>
#include <utility>

namespace account_recommendations::generators::identity
{

common::ValidationResult IdentityGeneratorBase::validate(const domain::GeneratorRequest& request) const
{
    return validateRequest(request, common::ApplicationLimits::defaults());
}

common::ValidationResult IdentityGeneratorBase::validateSpecific(
    const QJsonObject&, const common::ApplicationLimits&) const
{
    return {};
}

QString IdentityGeneratorBase::uniqueKey(const domain::GeneratedRecord& record) const
{
    const auto fields = metadata().outputFields;
    if (fields.isEmpty())
        return {};
    return record.value(fields.first().key).toVariant().toString().toCaseFolded();
}

common::ValidationResult IdentityGeneratorBase::validateRequest(
    const domain::GeneratorRequest& request, const common::ApplicationLimits& limits) const
{
    const domain::GeneratorMetadata md = metadata();
    common::ValidationResult issues;
    if (request.generatorId != md.id)
        issues.addError(QStringLiteral("generatorId"), QStringLiteral("wrong_generator"),
                        QStringLiteral("The request is addressed to a different generator."));
    if (request.count < 1)
        issues.addError(QStringLiteral("count"), QStringLiteral("out_of_range"),
                        QStringLiteral("Count must be at least 1."));
    if (request.count >= 1 && static_cast<std::size_t>(request.count) > limits.maxBatchSize)
        issues.addError(QStringLiteral("count"), QStringLiteral("limit_exceeded"),
                        QStringLiteral("Count exceeds the configured batch limit."));

    issues.merge(md.configurationSchema.validate(request.parameters, limits));
    if (!issues.isValid())
        return issues;

    const QJsonObject parameters = md.configurationSchema.applyDefaults(request.parameters);
    issues.merge(validateSpecific(parameters, limits));
    return issues;
}

common::Result<domain::GenerationSummary, domain::GenerationError>
IdentityGeneratorBase::generateIntoImpl(const domain::GeneratorRequest& request,
                                        const domain::GenerationContext& context,
                                        domain::IResultSink& validatedSink) const
{
    using R = common::Result<domain::GenerationSummary, domain::GenerationError>;
    const domain::GeneratorMetadata md = metadata();
    const common::ValidationResult issues = validateRequest(request, context.limits());
    if (!issues.isValid())
        return R::failure(domain::GenerationError::invalidConfiguration(issues));

    const QJsonObject parameters = md.configurationSchema.applyDefaults(request.parameters);
    QSet<QString> seen;
    if (request.unique)
        seen.reserve(std::min<qsizetype>(request.count, 65'536));

    domain::GenerationSummary summary;
    const std::size_t attemptsAllowed = request.unique ? context.limits().maxUniquenessAttempts : 1U;
    if (request.unique && attemptsAllowed == 0U)
        return R::failure(domain::GenerationError::uniquenessExhausted());

    try {
        for (qsizetype index = 0; index < request.count; ++index) {
            if (context.isCancelled()) {
                summary.status = domain::CompletionStatus::Cancelled;
                return R::success(summary);
            }

            domain::GeneratedRecord record;
            bool accepted = false;
            for (std::size_t attempt = 0; attempt < attemptsAllowed; ++attempt) {
                if (context.isCancelled()) {
                    summary.status = domain::CompletionStatus::Cancelled;
                    return R::success(summary);
                }
                auto generated = generateRecord(parameters, context);
                if (!generated)
                    return R::failure(std::move(generated).error());
                record = std::move(generated).value();

                bool withinLimit = true;
                for (auto it = record.constBegin(); it != record.constEnd(); ++it) {
                    if (it.value().isString()
                        && static_cast<std::size_t>(it.value().toString().size()) > context.limits().maxOutputLength) {
                        withinLimit = false;
                        break;
                    }
                }
                if (!withinLimit)
                    return R::failure(domain::GenerationError::limitExceeded(
                        QStringLiteral("Generated output exceeds the configured string-length limit.")));

                if (!request.unique) {
                    accepted = true;
                    break;
                }
                const QString key = uniqueKey(record);
                if (!key.isEmpty() && !seen.contains(key)) {
                    seen.insert(key);
                    accepted = true;
                    break;
                }
            }
            if (!accepted)
                return R::failure(domain::GenerationError::uniquenessExhausted());

            if (validatedSink.accept(std::move(record)) == domain::SinkResponse::Stop) {
                ++summary.produced;
                summary.status = domain::CompletionStatus::StoppedBySink;
                return R::success(summary);
            }
            ++summary.produced;
        }
    } catch (const domain::RandomSourceError&) {
        return R::failure(domain::GenerationError::randomSourceFailure());
    }

    summary.status = domain::CompletionStatus::Completed;
    return R::success(summary);
}

} // namespace account_recommendations::generators::identity
