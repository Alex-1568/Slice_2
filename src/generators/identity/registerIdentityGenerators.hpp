#pragma once

#include "common/result/Result.hpp"
#include "domain/generator/GeneratorRegistry.hpp"

namespace account_recommendations::generators::identity
{

/// Registers the built-in Slice 2 identity generators in stable order. This is a composition
/// helper, not a generator-specific GUI switch; callers can still register individual generators.
[[nodiscard]] common::Result<void, domain::RegistryError>
registerIdentityGenerators(domain::GeneratorRegistry& registry);

} // namespace account_recommendations::generators::identity
