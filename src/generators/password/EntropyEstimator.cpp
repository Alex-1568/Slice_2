#include "generators/password/EntropyEstimator.hpp"

#include <algorithm>
#include <cmath>

namespace account_recommendations::generators::password
{

namespace
{
constexpr double kExactTolerance = 1e-9;

double log2Factorial(std::size_t n) noexcept
{
    return std::lgamma(static_cast<double>(n) + 1.0) / std::log(2.0);
}

double log2Of(std::size_t n) noexcept
{
    return n <= 1 ? 0.0 : std::log2(static_cast<double>(n));
}
} // namespace

double searchSpaceBits(std::size_t poolSize, std::size_t length, bool allowDuplicates) noexcept
{
    if (poolSize <= 1 || length == 0)
        return 0.0;
    if (allowDuplicates)
        return static_cast<double>(length) * log2Of(poolSize);
    if (length > poolSize)
        return 0.0;
    return log2Factorial(poolSize) - log2Factorial(poolSize - length);
}

GeneratorEntropyEstimate estimateGeneratorEntropy(const PasswordConfiguration& config,
                                                  const PasswordPoolPlan& plan) noexcept
{
    const std::size_t length = config.length;
    const std::size_t allowed = plan.allowed.size();
    const std::size_t minimumTotal = config.totalMinimum();

    const bool usable = allowed > 0 && length > 0 && minimumTotal <= length
                        && (config.allowDuplicates || length <= allowed);
    if (!usable)
        return {};

    const double upper = searchSpaceBits(allowed, length, config.allowDuplicates);

    // Entropy of the pre-shuffle draw sequence: required draws per category, then
    // (length - minimumTotal) fill draws from the combined pool.
    double lower = 0.0;
    for (std::size_t c = 0; c < kStandardCategoryCount; ++c) {
        const std::size_t minimum = config.minimumFor(static_cast<CharacterCategory>(c));
        const std::size_t pool = plan.pool(static_cast<CharacterCategory>(c)).size();
        if (minimum == 0)
            continue;
        if (pool == 0 || (!config.allowDuplicates && minimum > pool))
            return {};
        lower += config.allowDuplicates
                     ? static_cast<double>(minimum) * log2Of(pool)
                     : log2Factorial(pool) - log2Factorial(pool - minimum);
    }
    if (config.allowDuplicates) {
        lower += static_cast<double>(length - minimumTotal) * log2Of(allowed);
    } else {
        // Each required pick removes exactly one character from the combined pool
        // (pools are disjoint), leaving allowed - minimumTotal for the fill draws.
        lower += log2Factorial(allowed - minimumTotal) - log2Factorial(allowed - length);
    }

    GeneratorEntropyEstimate estimate;
    estimate.upperBoundBits = upper;
    estimate.lowerBoundBits = std::min(lower, upper); // guard against rounding noise
    if (upper - estimate.lowerBoundBits < kExactTolerance) {
        estimate.lowerBoundBits = upper;
        estimate.quality = EntropyQuality::Exact;
    } else {
        estimate.quality = EntropyQuality::Bounded;
    }
    return estimate;
}

EntropyBand classifyBand(double bits, const EntropyBandThresholds& thresholds) noexcept
{
    if (bits >= thresholds.veryStrong)
        return EntropyBand::VeryStrong;
    if (bits >= thresholds.strong)
        return EntropyBand::Strong;
    if (bits >= thresholds.good)
        return EntropyBand::Good;
    if (bits >= thresholds.fair)
        return EntropyBand::Fair;
    return EntropyBand::Weak;
}

EntropyBand classifyBand(const GeneratorEntropyEstimate& estimate,
                         const EntropyBandThresholds& thresholds) noexcept
{
    return classifyBand(estimate.conservativeBits(), thresholds);
}

QString bandId(EntropyBand band)
{
    switch (band) {
    case EntropyBand::Weak: return QStringLiteral("weak");
    case EntropyBand::Fair: return QStringLiteral("fair");
    case EntropyBand::Good: return QStringLiteral("good");
    case EntropyBand::Strong: return QStringLiteral("strong");
    case EntropyBand::VeryStrong: return QStringLiteral("very-strong");
    }
    return QStringLiteral("weak");
}

QString qualityId(EntropyQuality quality)
{
    return quality == EntropyQuality::Exact ? QStringLiteral("exact") : QStringLiteral("bounded");
}

} // namespace account_recommendations::generators::password
