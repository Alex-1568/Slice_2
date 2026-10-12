#pragma once

#include "domain/random/IRandomProvider.hpp"

#include <array>
#include <cstdint>

namespace account_recommendations::testing
{

/// Seeded, reproducible generator (xoshiro256**) for tests, fuzzing and debugging.
///
/// Lives under testsupport/, whose include directory is exported ONLY by the ar_random_testing
/// target. Production targets do not get that include path, so they cannot even include this header.
///
/// NOT CRYPTOGRAPHICALLY SECURE (it cannot be attested: only SecureRandomProvider can mint the
/// token) and NOT thread-safe. isCryptographicallySecure()
/// returns false, so security-sensitive generators reject it unless the
/// GenerationContext was created through GenerationContext::forTesting().
class DeterministicRandomProvider final : public domain::IRandomProvider
{
public:
    explicit DeterministicRandomProvider(std::uint64_t seed) noexcept;

    [[nodiscard]] std::uint64_t nextUInt64() override;

private:
    std::array<std::uint64_t, 4> m_state{};
};

} // namespace account_recommendations::testing
