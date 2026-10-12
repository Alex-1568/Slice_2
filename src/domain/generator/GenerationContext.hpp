#pragma once

#include "common/limits/ApplicationLimits.hpp"
#include "domain/generator/CancellationToken.hpp"
#include "domain/random/IRandomProvider.hpp"

#include <utility>

namespace account_recommendations::testing
{
struct InsecureContextAccess; // defined only in testsupport/testing/InsecureGenerationContext.hpp
}

namespace account_recommendations::domain
{

/// Concrete bundle of non-owning services for one generation run. Dependency
/// injection happens through the interfaces it holds (IRandomProvider today;
/// data providers etc. later), not by making the context itself an interface.
///
/// Insecure (non-CSPRNG) providers are accepted by security-sensitive generators only through
/// a context built by the helper in testsupport/testing/InsecureGenerationContext.hpp, which is
/// not visible to production targets. This header offers NO way to enable it.
///
/// Lifetime: the referenced provider must outlive the context.
/// Threading: a context is cheap to copy; use one per worker. A shared provider
/// must be thread-safe (SecureRandomProvider is, DeterministicRandomProvider is not).
class GenerationContext
{
public:
    /// Production context. Generators that require secure randomness will reject a
    /// provider whose isCryptographicallySecure() is false.
    explicit GenerationContext(IRandomProvider& random, CancellationToken cancellation = {},
                               common::ApplicationLimits limits = common::ApplicationLimits::defaults())
        : GenerationContext(random, std::move(cancellation), limits, false)
    {
    }

    [[nodiscard]] IRandomProvider& random() const noexcept { return *m_random; }
    [[nodiscard]] bool isCancelled() const noexcept { return m_cancellation.isCancelled(); }
    [[nodiscard]] const common::ApplicationLimits& limits() const noexcept { return m_limits; }
    [[nodiscard]] bool insecureRandomPermitted() const noexcept { return m_permitInsecureRandom; }

private:
    friend struct testing::InsecureContextAccess;

    GenerationContext(IRandomProvider& random, CancellationToken cancellation,
                      common::ApplicationLimits limits, bool permitInsecureRandom)
        : m_random(&random)
        , m_cancellation(std::move(cancellation))
        , m_limits(limits)
        , m_permitInsecureRandom(permitInsecureRandom)
    {
    }

    IRandomProvider* m_random;
    CancellationToken m_cancellation;
    common::ApplicationLimits m_limits;
    bool m_permitInsecureRandom;
};

} // namespace account_recommendations::domain
