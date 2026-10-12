#pragma once

#include "common/validation/ValidationResult.hpp"

#include <QLatin1String>
#include <QString>

#include <utility>

namespace account_recommendations::domain
{

/// Error returned by generators. Messages must never contain generated secrets.
struct GenerationError
{
    enum class Code
    {
        InvalidRequest,
        InvalidConfiguration,
        LimitExceeded,
        InsecureRandomSource,
        RandomSourceFailure,
        UniquenessExhausted,
        ConstraintViolation,
        Internal
    };

    Code code = Code::Internal;
    QString message;
    common::ValidationResult validation; ///< populated for InvalidConfiguration

    [[nodiscard]] static GenerationError invalidRequest(QString message)
    {
        return {Code::InvalidRequest, std::move(message), {}};
    }
    [[nodiscard]] static GenerationError invalidConfiguration(common::ValidationResult issues)
    {
        QString text = issues.summary();
        return {Code::InvalidConfiguration, std::move(text), std::move(issues)};
    }
    [[nodiscard]] static GenerationError limitExceeded(QString message)
    {
        return {Code::LimitExceeded, std::move(message), {}};
    }
    [[nodiscard]] static GenerationError insecureRandomSource()
    {
        return {Code::InsecureRandomSource,
                QStringLiteral("This generator requires a cryptographically secure random "
                               "source, but the supplied provider is not one."),
                {}};
    }
    [[nodiscard]] static GenerationError randomSourceFailure()
    {
        return {Code::RandomSourceFailure,
                QStringLiteral("The random source failed. Nothing was generated."),
                {}};
    }
    [[nodiscard]] static GenerationError uniquenessExhausted()
    {
        return {Code::UniquenessExhausted,
                QStringLiteral("Could not generate enough unique values with this "
                               "configuration. Allow more characters or reduce the count."),
                {}};
    }
    [[nodiscard]] static GenerationError constraintViolation(QString message)
    {
        return {Code::ConstraintViolation, std::move(message), {}};
    }
    [[nodiscard]] static GenerationError internal(QString message)
    {
        return {Code::Internal, std::move(message), {}};
    }
};

[[nodiscard]] inline QLatin1String codeName(GenerationError::Code code) noexcept
{
    switch (code) {
    case GenerationError::Code::InvalidRequest: return QLatin1String("invalid_request");
    case GenerationError::Code::InvalidConfiguration: return QLatin1String("invalid_configuration");
    case GenerationError::Code::LimitExceeded: return QLatin1String("limit_exceeded");
    case GenerationError::Code::InsecureRandomSource: return QLatin1String("insecure_random_source");
    case GenerationError::Code::RandomSourceFailure: return QLatin1String("random_source_failure");
    case GenerationError::Code::UniquenessExhausted: return QLatin1String("uniqueness_exhausted");
    case GenerationError::Code::ConstraintViolation: return QLatin1String("constraint_violation");
    case GenerationError::Code::Internal: return QLatin1String("internal");
    }
    return QLatin1String("internal");
}

} // namespace account_recommendations::domain
