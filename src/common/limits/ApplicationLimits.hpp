#pragma once

#include <cstddef>

namespace account_recommendations::common
{

/// Centralised resource limits. Nothing else in the code base should hard-code
/// batch sizes, string lengths or retry budgets; they flow in through
/// GenerationContext (and, later, configuration) so they can be tuned in one place.
/// Generator-specific bounds (e.g. the minimum password length) stay with their
/// generator; limits here can only TIGHTEN those, never loosen them.
struct ApplicationLimits
{
    std::size_t maxBatchSize = 1'000'000;       ///< records per generation request
    std::size_t maxStringLength = 4096;         ///< any single user-supplied string value
    std::size_t maxParameterCount = 256;        ///< keys in one parameters object
    std::size_t maxOutputLength = 512;          ///< length of one generated string value
    std::size_t maxUniquenessAttempts = 1'000;  ///< consecutive collisions before giving up
    // Reserved for later slices (data providers / export). Not consulted by Slice 1 code.
    std::size_t maxCustomListSize = 100'000;    ///< entries in user-supplied lists
    std::size_t maxExportRecords = 5'000'000;   ///< records per export

    [[nodiscard]] static ApplicationLimits defaults() noexcept { return ApplicationLimits{}; }
};

} // namespace account_recommendations::common
