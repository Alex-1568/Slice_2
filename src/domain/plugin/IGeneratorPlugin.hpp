#pragma once

#include "common/result/Result.hpp"
#include "domain/generator/GeneratorRegistry.hpp"

#include <QString>
#include <QtPlugin>

namespace account_recommendations::domain
{

/// Version of the generator contract (IGenerator + GeneratorMetadata + GenerationContext
/// + IResultSink + GeneratorRequest/Result). Increment on ANY change that would require
/// third-party generators to be rebuilt. A plugin built against another version is
/// rejected by registerPluginGenerators() before any of its code is asked to register.
inline constexpr int kGeneratorApiVersion = 3;

/// Qt runtime-plugin boundary. Plugins must be built against the same compatible Qt/C++ ABI
/// and the shared ar_core library. API compatibility is checked before registration.
///
/// Trust: a native plugin executes with the full privileges of this process. Loading is disabled
/// by default and must be explicitly enabled after user consent.
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
/// cannot take the host down through this path. This helper does not roll back a registry.
/// RuntimePluginManager calls it against a staging registry and commits staged generators atomically.
[[nodiscard]] common::Result<void, RegistryError> registerPluginGenerators(IGeneratorPlugin& plugin,
                                                                           GeneratorRegistry& registry);

} // namespace account_recommendations::domain

#define AR_GENERATOR_PLUGIN_IID "org.account-recommendations.IGeneratorPlugin/3.0"
Q_DECLARE_INTERFACE(account_recommendations::domain::IGeneratorPlugin, AR_GENERATOR_PLUGIN_IID)
