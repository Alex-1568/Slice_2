#include "testing/DeterministicRandomProvider.hpp"

#include <bit>

namespace account_recommendations::testing
{

namespace
{
std::uint64_t splitMix64(std::uint64_t& state) noexcept
{
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = state;
    z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31U);
}
} // namespace

DeterministicRandomProvider::DeterministicRandomProvider(std::uint64_t seed) noexcept
{
    // SplitMix64 expands the seed into a well-mixed, never-all-zero state.
    for (std::uint64_t& word : m_state)
        word = splitMix64(seed);
}

std::uint64_t DeterministicRandomProvider::nextUInt64()
{
    const std::uint64_t result = std::rotl(m_state[1] * 5U, 7) * 9U;
    const std::uint64_t t = m_state[1] << 17U;

    m_state[2] ^= m_state[0];
    m_state[3] ^= m_state[1];
    m_state[1] ^= m_state[2];
    m_state[0] ^= m_state[3];
    m_state[2] ^= t;
    m_state[3] = std::rotl(m_state[3], 45);

    return result;
}

} // namespace account_recommendations::testing
