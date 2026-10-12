#include "domain/random/IRandomProvider.hpp"

#include <limits>

namespace account_recommendations::domain
{

namespace
{
// For any bound <= 2^63 the per-draw rejection probability is < 1/2, so 256
// consecutive rejections has probability < 2^-256 with a healthy source. Hitting
// the cap means the source is broken; fail instead of spinning forever.
constexpr int kMaxRejectionAttempts = 256;
}

std::uint32_t IRandomProvider::nextUInt32()
{
    return static_cast<std::uint32_t>(nextUInt64() >> 32U);
}

std::size_t IRandomProvider::uniformIndex(std::size_t upperExclusive)
{
    if (upperExclusive == 0)
        throw std::invalid_argument("uniformIndex: upper bound must be greater than zero");
    if (upperExclusive == 1)
        return 0;

    const auto bound = static_cast<std::uint64_t>(upperExclusive);
    // threshold = 2^64 mod bound. Words below it are rejected, so the accepted
    // range [threshold, 2^64) has a size that is an exact multiple of `bound`.
    const std::uint64_t threshold =
        (std::numeric_limits<std::uint64_t>::max() - bound + 1U) % bound;

    for (int attempt = 0; attempt < kMaxRejectionAttempts; ++attempt) {
        const std::uint64_t word = nextUInt64();
        if (word >= threshold)
            return static_cast<std::size_t>(word % bound);
    }
    throw RandomSourceError("random source failed to produce usable output");
}

} // namespace account_recommendations::domain
