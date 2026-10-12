#include "domain/plugin/IGeneratorPlugin.hpp"

#include <exception>

namespace account_recommendations::domain
{

common::Result<void, RegistryError> registerPluginGenerators(IGeneratorPlugin& plugin,
                                                             GeneratorRegistry& registry)
{
    using VoidResult = common::Result<void, RegistryError>;

    if (plugin.apiVersion() != kGeneratorApiVersion)
        return VoidResult::failure(
            {RegistryError::Code::InvalidMetadata,
             QStringLiteral("Plugin \"%1\" targets generator API %2, but this application provides %3.")
                 .arg(plugin.pluginId())
                 .arg(plugin.apiVersion())
                 .arg(kGeneratorApiVersion),
             {}});
    try {
        return plugin.registerGenerators(registry);
    } catch (const std::exception& error) {
        return VoidResult::failure({RegistryError::Code::InvalidMetadata,
                                    QStringLiteral("Plugin \"%1\" threw during registration: %2")
                                        .arg(plugin.pluginId(), QString::fromUtf8(error.what())),
                                    {}});
    } catch (...) {
        return VoidResult::failure({RegistryError::Code::InvalidMetadata,
                                    QStringLiteral("Plugin \"%1\" threw during registration.").arg(plugin.pluginId()),
                                    {}});
    }
}

} // namespace account_recommendations::domain
