#include "domain/plugin/IGeneratorPlugin.hpp"

#include <exception>
#include <utility>
#include <utility>

namespace account_recommendations::domain
{

common::Result<void, RegistryError> registerPluginGenerators(IGeneratorPlugin& plugin,
                                                             GeneratorRegistry& registry)
{
    using VoidResult = common::Result<void, RegistryError>;

    // Plugin-controlled strings and exception text must not be forwarded into host UI/logs.
    // Only stable error categories are retained across this trust boundary.
    try {
        const int api = plugin.apiVersion();
        if (api != kGeneratorApiVersion)
            return VoidResult::failure(
                {RegistryError::Code::InvalidMetadata,
                 QStringLiteral("Plugin API is incompatible with this application."),
                 {}});

        auto result = plugin.registerGenerators(registry);
        if (!result) {
            // Keep only the stable status code. Both the message and validation details came
            // from plugin-controlled code and must not be surfaced as trusted host diagnostics.
            RegistryError error;
            error.code = result.error().code;
            error.message = QStringLiteral("Plugin generator registration failed.");
            return VoidResult::failure(std::move(error));
        }
        return VoidResult::success();
    } catch (...) {
        return VoidResult::failure({RegistryError::Code::InvalidMetadata,
                                    QStringLiteral("Plugin threw during registration."),
                                    {}});
    }
}

} // namespace account_recommendations::domain
