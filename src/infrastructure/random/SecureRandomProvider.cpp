#include "infrastructure/random/SecureRandomProvider.hpp"

#include <array>
#include <stdexcept>
#include <utility>

namespace account_recommendations::infrastructure
{

SecureRandomProvider::SecureRandomProvider()
    : domain::IRandomProvider(domain::SecureAttestation{}) // the only place a token is minted
    , m_source(makeOperatingSystemEntropy())
{
}

SecureRandomProvider::SecureRandomProvider(std::shared_ptr<ISystemEntropy> source)
    : domain::IRandomProvider() // untrusted: no attestation for injected sources
    , m_source(std::move(source))
{
    if (!m_source)
        throw std::invalid_argument("SecureRandomProvider requires an entropy source");
}

common::Result<void, EntropyError> SecureRandomProvider::probe()
{
    std::array<std::byte, 32> block{};
    return m_source->fill(block.data(), block.size());
}

std::uint64_t SecureRandomProvider::nextUInt64()
{
    std::uint64_t word = 0;
    const auto filled = m_source->fill(reinterpret_cast<std::byte*>(&word), sizeof word);
    if (!filled)
        throw domain::RandomSourceError(
            ("operating-system entropy source failed: " + filled.error().description).toStdString());
    return word;
}

} // namespace account_recommendations::infrastructure
