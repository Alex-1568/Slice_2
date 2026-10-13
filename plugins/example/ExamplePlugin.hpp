#pragma once

#include "domain/plugin/IGeneratorPlugin.hpp"

#include <QObject>

namespace account_recommendations::plugins::example
{

class ExamplePlugin final : public QObject, public domain::IGeneratorPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID AR_GENERATOR_PLUGIN_IID)
    Q_INTERFACES(account_recommendations::domain::IGeneratorPlugin)

public:
    [[nodiscard]] QString pluginId() const override;
    [[nodiscard]] QString pluginVersion() const override;
    [[nodiscard]] int apiVersion() const noexcept override;
    [[nodiscard]] common::Result<void, domain::RegistryError>
    registerGenerators(domain::GeneratorRegistry& registry) override;
};

} // namespace account_recommendations::plugins::example
