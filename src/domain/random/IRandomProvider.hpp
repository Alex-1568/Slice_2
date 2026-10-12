#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace account_recommendations::infrastructure
{
class SecureRandomProvider;
}

namespace account_recommendations::domain
{

/// Capability token. Only infrastructure::SecureRandomProvider can create one (private
/// constructor, friend declaration, copy/move deleted so it cannot be stored or passed on).
/// A provider that is built from one is "attested" as the operating-system CSPRNG provider.
///
/// Trust boundary, stated plainly: this prevents ORDINARY code (a custom or test provider, a
/// misconfigured composition root) from claiming cryptographic security, because it cannot
/// obtain the token. It does not stop code that deliberately subverts the type system
/// (editing this header, ODR tricks, memory corruption) and it is no evidence about the
/// operating system itself. The trusted computing base is SecureRandomProvider + the
/// OS backend + this token.
class SecureAttestation
{
public:
    SecureAttestation(const SecureAttestation&) = delete;
    SecureAttestation& operator=(const SecureAttestation&) = delete;

private:
    SecureAttestation() noexcept = default;
    friend class infrastructure::SecureRandomProvider;
};

/// Thrown by a provider when it cannot deliver randomness (e.g. the OS source
/// failed, or rejection sampling was exhausted by a broken source). Generators
/// translate it into GenerationError::randomSourceFailure(); they must never fall
/// back to a weaker source.
class RandomSourceError final : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

/// The ONLY randomness boundary generators see.
///
/// Design: implementations supply raw 64-bit words (nextUInt64). Range reduction
/// (uniformIndex) is deliberately NON-virtual and implemented once here using
/// rejection sampling, so no provider can accidentally introduce modulo bias.
class IRandomProvider
{
public:
    virtual ~IRandomProvider() = default;

    /// Next uniformly distributed 64-bit word. May throw RandomSourceError.
    [[nodiscard]] virtual std::uint64_t nextUInt64() = 0;

    /// True ONLY for a provider constructed with a SecureAttestation, i.e. the production
    /// SecureRandomProvider. NOT virtual: an implementation cannot answer "yes" for itself
    /// (a derived class that declares its own function of this name merely hides it; calls
    /// through IRandomProvider, as generators make them, still see the attested value).
    /// Generators flagged `requiresSecureRandom` refuse providers for which this is false,
    /// unless the context was built with the test-support helper.
    [[nodiscard]] bool isCryptographicallySecure() const noexcept { return m_attestedSecure; }

    [[nodiscard]] std::uint32_t nextUInt32();

    /// Uniform integer in [0, upperExclusive) via rejection sampling (no modulo bias).
    /// Throws std::invalid_argument if upperExclusive == 0, RandomSourceError if the
    /// source is so broken that sampling cannot terminate (bounded retry).
    [[nodiscard]] std::size_t uniformIndex(std::size_t upperExclusive);

protected:
    IRandomProvider() = default;                                         ///< untrusted (the default)
    explicit IRandomProvider(SecureAttestation&&) noexcept : m_attestedSecure(true) {}
    IRandomProvider(const IRandomProvider&) = default;
    IRandomProvider& operator=(const IRandomProvider&) = default;

private:
    bool m_attestedSecure = false;
};

} // namespace account_recommendations::domain
