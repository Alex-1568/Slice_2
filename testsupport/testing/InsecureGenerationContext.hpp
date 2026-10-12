#pragma once

#include "domain/generator/GenerationContext.hpp"

namespace account_recommendations::testing
{

/// Grants access to GenerationContext's private "permit insecure randomness" switch.
/// GenerationContext befriends this struct by name; the definition lives ONLY here, in
/// testsupport/, whose include path is exported solely by ar_random_testing. A production
/// target that tries to build an insecure context therefore fails at compile time (missing
/// header). Deliberate circumvention (declaring its own struct of this name) remains possible
/// for in-process code and is not a goal of this boundary; accidents are.
struct InsecureContextAccess
{
    [[nodiscard]] static domain::GenerationContext make(domain::IRandomProvider& random,
                                                        domain::CancellationToken cancellation,
                                                        common::ApplicationLimits limits)
    {
        return domain::GenerationContext(random, std::move(cancellation), limits, /*permitInsecureRandom=*/true);
    }
};

/// Test/benchmark-only: a context whose security-sensitive generators accept a
/// non-cryptographic provider such as DeterministicRandomProvider.
[[nodiscard]] inline domain::GenerationContext
insecureContext(domain::IRandomProvider& random, domain::CancellationToken cancellation = {},
                common::ApplicationLimits limits = common::ApplicationLimits::defaults())
{
    return InsecureContextAccess::make(random, std::move(cancellation), limits);
}

} // namespace account_recommendations::testing
