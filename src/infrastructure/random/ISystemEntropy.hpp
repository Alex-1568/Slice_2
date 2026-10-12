#pragma once

#include "common/result/Result.hpp"

#include <QString>

#include <cstddef>
#include <memory>

namespace account_recommendations::infrastructure
{

struct EntropyError
{
    int osCode = 0;      ///< errno / NTSTATUS reported by the operating system (0 if none)
    QString description; ///< human-readable; never contains random bytes
};

/// Source of raw random BYTES. This is the seam between SecureRandomProvider and the
/// operating system, and the point where faults are injected in tests.
///
/// Contract for the built-in operating-system implementation (makeOperatingSystemEntropy):
///  * fill() either writes exactly `size` bytes obtained from the OS CSPRNG, or returns an
///    error. It never substitutes another generator and never returns partial data.
///  * It is thread-safe and stateless.
/// A source does NOT get to say whether it is trustworthy: whether a provider is attested as
/// secure depends solely on how the provider was constructed (see SecureRandomProvider).
class ISystemEntropy
{
public:
    virtual ~ISystemEntropy() = default;

    [[nodiscard]] virtual common::Result<void, EntropyError> fill(std::byte* data, std::size_t size) = 0;
    [[nodiscard]] virtual QString backendName() const = 0;

protected:
    ISystemEntropy() = default;
    ISystemEntropy(const ISystemEntropy&) = default;
    ISystemEntropy& operator=(const ISystemEntropy&) = default;
};

/// The operating system CSPRNG for this platform (see docs/RANDOMNESS_AUDIT.md):
///   Linux   getrandom(2), flags 0   (blocks until the kernel pool is initialised)
///   macOS   getentropy(2)           (<= 256 bytes per call)
///   Windows BCryptGenRandom with BCRYPT_USE_SYSTEM_PREFERRED_RNG
/// Compilation FAILS on other platforms instead of picking a weaker source.
[[nodiscard]] std::unique_ptr<ISystemEntropy> makeOperatingSystemEntropy();

} // namespace account_recommendations::infrastructure
