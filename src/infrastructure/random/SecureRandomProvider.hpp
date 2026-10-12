#pragma once

#include "domain/random/IRandomProvider.hpp"
#include "infrastructure/random/ISystemEntropy.hpp"

#include <memory>

namespace account_recommendations::testing
{
struct EntropyInjectionAccess; // defined only in testsupport/testing/InjectedEntropyProvider.hpp
}

namespace account_recommendations::infrastructure
{

/// Production randomness: bytes from the operating system CSPRNG, FAIL-CLOSED.
///
/// Guarantees (audit: docs/RANDOMNESS_AUDIT.md):
///  * Output is either OS CSPRNG data or an exception. If the OS call fails,
///    nextUInt64() throws domain::RandomSourceError; nothing is substituted.
///  * ONLY the public default constructor produces an attested-secure provider
///    (isCryptographicallySecure() == true): it builds the built-in OS source itself and
///    is the only code that can mint a domain::SecureAttestation.
///  * The constructor that accepts a custom entropy source is PRIVATE (friend:
///    testing::EntropyInjectionAccess, declared in testsupport/, whose include path
///    production targets do not have). A provider built that way is never attested, whatever
///    its source claims, so security-sensitive generators refuse it in ordinary contexts.
///  * Thread-safe when the source is (the built-in OS source is stateless).
///  * No buffering: one OS call per 64-bit word; nothing secret is cached here.
///
/// Why not the Qt system generator: its implemented behaviour includes silent fallback and
/// ignored OS failure. Qt random classes must not appear anywhere under src/ (scan test).
class SecureRandomProvider final : public domain::IRandomProvider
{
public:
    /// Uses the operating-system CSPRNG backend of this platform. Attested secure.
    SecureRandomProvider();

    /// Startup health check: draws a small block and reports failure instead of throwing.
    /// Composition roots should call this once and refuse to run if it fails.
    [[nodiscard]] common::Result<void, EntropyError> probe();

    [[nodiscard]] std::uint64_t nextUInt64() override;

private:
    friend struct testing::EntropyInjectionAccess;

    /// Test seam. Untrusted by construction (see class comment). Throws std::invalid_argument for null.
    explicit SecureRandomProvider(std::shared_ptr<ISystemEntropy> source);

    std::shared_ptr<ISystemEntropy> m_source;
};

} // namespace account_recommendations::infrastructure
