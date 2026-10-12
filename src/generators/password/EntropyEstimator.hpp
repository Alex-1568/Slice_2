#pragma once

#include "generators/password/CharacterPools.hpp"
#include "generators/password/PasswordConfiguration.hpp"

#include <QString>

#include <cstddef>

namespace account_recommendations::generators::password
{

// ---------------------------------------------------------------------------
// What these numbers ARE and ARE NOT
//
// They describe the GENERATOR CONFIGURATION: how unpredictable the output of the
// generator is, assuming the random provider is ideal and an attacker knows the
// configuration. They say NOTHING about one particular password: a generator can
// (rarely) emit "Aaaaaaaa1!" and that password is weak whatever the settings.
// Per-password analysis (patterns, repetition, dictionary words) would be a
// separate PasswordStrengthAssessment taking the password as input; it is not
// implemented and its name is deliberately not used here.
// ---------------------------------------------------------------------------

enum class EntropyQuality
{
    Exact,  ///< lower == upper: the number is the exact entropy of the configuration
    Bounded ///< constraints make the exact value unknown; it lies in [lower, upper]
};

/// Entropy of the generator's output distribution, as a rigorous interval.
///
///  * lowerBoundBits  - the generator's min-entropy is AT LEAST this (hence so are its
///    Shannon entropy and its guessing resistance). Derivation: the pre-shuffle draw
///    sequence is uniform over its valid values and the shuffle is independent of it, so
///    every output has probability <= 2^-lowerBoundBits.
///  * upperBoundBits  - log2 of the number of strings the unconstrained pool allows;
///    entropy can never exceed it.
///
/// Use lowerBoundBits (conservativeBits()) for any display or classification. Never
/// present upperBoundBits as "the strength".
struct GeneratorEntropyEstimate
{
    double lowerBoundBits = 0.0;
    double upperBoundBits = 0.0;
    EntropyQuality quality = EntropyQuality::Exact;

    [[nodiscard]] double conservativeBits() const noexcept { return lowerBoundBits; }
};

/// Coarse label for a CONFIGURATION (not for a password). Always derived from the
/// conservative (lower) bound so it can never overstate.
enum class EntropyBand
{
    Weak,
    Fair,
    Good,
    Strong,
    VeryStrong
};

/// Lower bounds (in bits) of each band above Weak. Configurable; the defaults are a
/// UI convention, NOT a universal security standard.
struct EntropyBandThresholds
{
    double fair = 40.0;
    double good = 60.0;
    double strong = 80.0;
    double veryStrong = 100.0;

    [[nodiscard]] bool isValid() const noexcept
    {
        return fair > 0.0 && fair < good && good < strong && strong < veryStrong;
    }
};

/// log2 of the number of strings of `length` over `poolSize` symbols
/// (with repetition: length*log2(N); without: log2(N!/(N-length)!)).
[[nodiscard]] double searchSpaceBits(std::size_t poolSize, std::size_t length,
                                     bool allowDuplicates) noexcept;

/// Interval for a VALIDATED configuration. An unusable configuration yields {0, 0}.
[[nodiscard]] GeneratorEntropyEstimate estimateGeneratorEntropy(const PasswordConfiguration& config,
                                                                const PasswordPoolPlan& plan) noexcept;

[[nodiscard]] EntropyBand classifyBand(double bits, const EntropyBandThresholds& thresholds = {}) noexcept;

/// Uses the conservative (lower) bound.
[[nodiscard]] EntropyBand classifyBand(const GeneratorEntropyEstimate& estimate,
                                       const EntropyBandThresholds& thresholds = {}) noexcept;

/// Stable lowercase identifiers: "weak", "fair", "good", "strong", "very-strong".
[[nodiscard]] QString bandId(EntropyBand band);
/// "exact" or "bounded".
[[nodiscard]] QString qualityId(EntropyQuality quality);

} // namespace account_recommendations::generators::password
