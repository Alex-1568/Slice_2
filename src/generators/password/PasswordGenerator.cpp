#include "generators/password/PasswordGenerator.hpp"

#include "common/security/SecureWipe.hpp"
#include "generators/password/PasswordAlgorithm.hpp"
#include "generators/password/PasswordValidator.hpp"

#include <QSet>

#include <cmath>
#include <utility>

namespace account_recommendations::generators::password
{

using common::Result;
using common::ValidationResult;
using domain::CompletionStatus;
using domain::GenerationError;
using domain::GenerationSummary;

namespace
{
// Slack for floating-point comparison of search-space sizes (in bits).
constexpr double kBitsTolerance = 1e-9;
}

PasswordGenerator::PasswordGenerator(EntropyBandThresholds thresholds) : m_thresholds(thresholds)
{
    if (!m_thresholds.isValid())
        m_thresholds = EntropyBandThresholds{};
}

QString PasswordGenerator::generatorId()
{
    return QStringLiteral("password");
}

domain::GeneratorMetadata PasswordGenerator::metadata() const
{
    domain::GeneratorMetadata md;
    md.id = generatorId();
    md.displayName = QStringLiteral("Password");
    md.description = QStringLiteral(
        "Generates random passwords from a cryptographically secure source, with category "
        "minimums, exclusions and optional duplicate prevention.");
    md.category = QStringLiteral("Security");
    md.version = QStringLiteral("1.1.0");
    md.tags = {QStringLiteral("security"), QStringLiteral("credential"), QStringLiteral("random")};
    md.supportsBatch = true;
    md.supportsPresets = true;
    md.supportsUniqueness = true;
    md.requiresSecureRandom = true;
    md.configurationSchema = passwordConfigurationSchema();
    md.outputFields = {
        {QStringLiteral("password"), QStringLiteral("Password"), /*sensitive=*/true},
        {QStringLiteral("guaranteedEntropyBits"),
         QStringLiteral("Guaranteed entropy (bits, lower bound)"), false},
        {QStringLiteral("searchSpaceBits"), QStringLiteral("Search space (bits, upper bound)"), false},
        {QStringLiteral("entropyQuality"), QStringLiteral("Entropy estimate quality"), false},
    };
    return md;
}

Result<PasswordGenerator::Prepared, ValidationResult>
PasswordGenerator::prepare(const domain::GeneratorRequest& request,
                           const common::ApplicationLimits& limits) const
{
    using PrepareResult = Result<Prepared, ValidationResult>;

    ValidationResult issues;
    if (request.generatorId != generatorId())
        issues.addError(QStringLiteral("generatorId"), QStringLiteral("wrong_generator"),
                        QStringLiteral("This request is not addressed to the password generator."));
    if (request.count < 1)
        issues.addError(QStringLiteral("count"), QStringLiteral("out_of_range"),
                        QStringLiteral("Count must be at least 1."));

    auto parsed = parsePasswordConfiguration(request.parameters, limits);
    if (!parsed) {
        issues.merge(parsed.error());
        return PrepareResult::failure(std::move(issues));
    }

    auto plan = buildPoolPlan(parsed.value());
    if (!plan) {
        issues.merge(plan.error());
        return PrepareResult::failure(std::move(issues));
    }

    Prepared prepared;
    prepared.config = std::move(parsed).value();
    prepared.plan = std::move(plan).value();

    issues.merge(PasswordConfigurationValidator::validate(prepared.config, prepared.plan, limits));

    prepared.entropy = estimateGeneratorEntropy(prepared.config, prepared.plan);

    // Reject only what is provably impossible: the unconstrained space is an UPPER
    // bound on the number of distinct outputs, so if even it is too small, the batch
    // cannot be unique. Harder cases are caught by the bounded retry budget.
    if (request.unique && request.count > 1 && issues.isValid()) {
        const double neededBits = std::log2(static_cast<double>(request.count));
        if (prepared.entropy.upperBoundBits < neededBits - kBitsTolerance)
            issues.addError(QStringLiteral("count"), QStringLiteral("unique_space_too_small"),
                            QStringLiteral("This configuration cannot produce %1 different "
                                           "passwords. Allow more characters, increase the "
                                           "length, or reduce the count.")
                                .arg(request.count));
    }

    if (!issues.isValid())
        return PrepareResult::failure(std::move(issues));

    prepared.validation = std::move(issues);
    return PrepareResult::success(std::move(prepared));
}

ValidationResult PasswordGenerator::validate(const domain::GeneratorRequest& request) const
{
    // validate() has no runtime context, so it applies the default limits.
    auto prepared = prepare(request, common::ApplicationLimits::defaults());
    return prepared ? prepared.value().validation : prepared.error();
}

Result<GenerationSummary, GenerationError>
PasswordGenerator::generateIntoImpl(const domain::GeneratorRequest& request,
                                    const domain::GenerationContext& context,
                                    domain::IResultSink& sink) const
{
    using SummaryResult = Result<GenerationSummary, GenerationError>;

    auto prepared = prepare(request, context.limits());
    if (!prepared)
        return SummaryResult::failure(GenerationError::invalidConfiguration(std::move(prepared).error()));

    const Prepared& job = prepared.value();
    const double guaranteedBits = job.entropy.lowerBoundBits;
    const double searchSpaceBits = job.entropy.upperBoundBits;
    const QString quality = qualityId(job.entropy.quality);
    const std::size_t maxAttempts = context.limits().maxUniquenessAttempts;

    // Held only for the duration of this call. Needed to enforce batch uniqueness.
    QSet<QString> seen;
    if (request.unique)
        seen.reserve(request.count);

    GenerationSummary summary;
    try {
        for (qsizetype i = 0; i < request.count; ++i) {
            if (context.isCancelled()) {
                summary.status = CompletionStatus::Cancelled;
                return SummaryResult::success(summary);
            }

            QString password;
            bool accepted = false;
            for (std::size_t attempt = 0; attempt < maxAttempts && !accepted; ++attempt) {
                auto generated = generatePasswordCodePoints(job.config, job.plan, context.random());
                if (!generated)
                    return SummaryResult::failure(std::move(generated).error());

                std::u32string codePoints = std::move(generated).value();
                password = QString::fromUcs4(codePoints.data(), static_cast<qsizetype>(codePoints.size()));
                common::secureWipe(codePoints);

                if (!request.unique) {
                    accepted = true;
                } else if (!seen.contains(password)) {
                    seen.insert(password);
                    accepted = true;
                }
            }
            if (!accepted)
                return SummaryResult::failure(GenerationError::uniquenessExhausted());

            domain::GeneratedRecord record;
            record.insert(QStringLiteral("password"), password);
            record.insert(QStringLiteral("guaranteedEntropyBits"), guaranteedBits);
            record.insert(QStringLiteral("searchSpaceBits"), searchSpaceBits);
            record.insert(QStringLiteral("entropyQuality"), quality);

            ++summary.produced;
            if (sink.accept(std::move(record)) == domain::SinkResponse::Stop) {
                summary.status = CompletionStatus::StoppedBySink;
                return SummaryResult::success(summary);
            }
        }
    } catch (const domain::RandomSourceError&) {
        return SummaryResult::failure(GenerationError::randomSourceFailure());
    }

    summary.status = CompletionStatus::Completed;
    return SummaryResult::success(summary);
}

} // namespace account_recommendations::generators::password
