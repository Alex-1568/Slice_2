#pragma once

#include "infrastructure/random/SecureRandomProvider.hpp"

namespace account_recommendations::testing
{

/// Friend of SecureRandomProvider: the ONLY way to build a provider around a custom entropy
/// source (fault injection). The resulting provider is never attested as secure; use
/// testing::insecureContext() to exercise generators with it. Defined in testsupport/, whose
/// include path is exported solely by ar_random_testing.
struct EntropyInjectionAccess
{
    [[nodiscard]] static infrastructure::SecureRandomProvider make(std::shared_ptr<infrastructure::ISystemEntropy> source)
    {
        return infrastructure::SecureRandomProvider(std::move(source));
    }
};

[[nodiscard]] inline infrastructure::SecureRandomProvider
providerWithInjectedSource(std::shared_ptr<infrastructure::ISystemEntropy> source)
{
    return EntropyInjectionAccess::make(std::move(source));
}

} // namespace account_recommendations::testing
