#pragma once

#include "common/result/Result.hpp"
#include "domain/generator/GeneratorRegistry.hpp"

#include <QString>

namespace account_recommendations::domain
{

/// Version of the generator contract (IGenerator + GeneratorMetadata + GenerationContext
/// + IResultSink + GeneratorRequest/Result). Increment on ANY change that would require
/// third-party generators to be rebuilt. A plugin built against another version is
/// rejected by registerPluginGenerators() before any of its code is asked to register.
inline constexpr int kGeneratorApiVersion = 1;

/// Boundary a future runtime loader will target. Slice 1.1 contains NO loader:
/// built-in generators are registered directly, and this interface is exercised by
/// in-process test doubles only.
///
/// Trust: a real plugin is native code running with the full privileges of the process.
class IGeneratorPlugin
{
public:
    virtual ~IGeneratorPlugin() = default;

    [[nodiscard]] virtual QString pluginId() const = 0;
    [[nodiscard]] virtual QString pluginVersion() const = 0;
    [[nodiscard]] virtual int apiVersion() const noexcept = 0;

    /// Registers this plugin's generators. Report failures through the Result.
    [[nodiscard]] virtual common::Result<void, RegistryError> registerGenerators(GeneratorRegistry& registry) = 0;

protected:
    IGeneratorPlugin() = default;
    IGeneratorPlugin(const IGeneratorPlugin&) = default;
    IGeneratorPlugin& operator=(const IGeneratorPlugin&) = default;
};

/// Failure-isolating entry point: checks the API version, then runs the plugin's
/// registration, converting an escaping exception into an error so a faulty plugin
/// cannot take the host down through this path. Generators registered before a
/// failure stay registered (the registry has no transaction support).
[[nodiscard]] common::Result<void, RegistryError> registerPluginGenerators(IGeneratorPlugin& plugin,
                                                                           GeneratorRegistry& registry);

} // namespace account_recommendations::domain
