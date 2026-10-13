#pragma once

#include "common/result/Result.hpp"
#include "domain/generator/GeneratorRegistry.hpp"

#include <QStringList>
#include <QPluginLoader>

#include <memory>
#include <vector>


namespace account_recommendations::infrastructure
{

struct RuntimePluginInfo
{
    QString id;
    QString version;
    QString absolutePath;
    QStringList generatorIds;
};

struct PluginLoadError
{
    enum class Code
    {
        Disabled,
        InvalidPath,
        OutsideTrustedDirectory,
        LoadFailed,
        MissingInterface,
        InvalidPluginMetadata,
        ApiMismatch,
        RegistrationFailed,
        DuplicatePluginId
    };

    Code code = Code::LoadFailed;
    QString message;
};

struct RuntimePluginOptions
{
    /// False by default. The GUI must obtain explicit user consent before enabling this.
    bool enabled = false;
    /// Canonical directories approved by the user for this process session.
    QStringList trustedDirectories;
};

/// Explicitly opt-in Qt runtime plugin loader.
///
/// Security model: plugins are native code with the same process privileges as the host. This
/// manager does not sandbox plugins and does not claim that a directory is cryptographically
/// trusted. The caller must obtain explicit user consent and pass approved directories. Plugins
/// are loaded with PreventUnloadHint and are never unloaded by this manager: registered generators
/// may still be referenced by workers after registry removal, and their vtables live in the plugin
/// module. Restart the application to remove a loaded native plugin from memory.
class RuntimePluginManager final
{
public:
    explicit RuntimePluginManager(domain::GeneratorRegistry& registry,
                                  RuntimePluginOptions options = {});
    ~RuntimePluginManager();

    RuntimePluginManager(const RuntimePluginManager&) = delete;
    RuntimePluginManager& operator=(const RuntimePluginManager&) = delete;

    void enableForSession(const QStringList& approvedDirectories);
    void disableFurtherLoading() noexcept;
    [[nodiscard]] bool isEnabled() const noexcept;

    [[nodiscard]] common::Result<RuntimePluginInfo, PluginLoadError> loadFile(const QString& filePath);
    [[nodiscard]] common::Result<QList<RuntimePluginInfo>, PluginLoadError>
    loadDirectory(const QString& directoryPath);

    [[nodiscard]] QList<RuntimePluginInfo> loadedPlugins() const;

private:
    struct LoadedPlugin
    {
        RuntimePluginInfo info;
        std::unique_ptr<QPluginLoader> loader;
    };

    [[nodiscard]] bool pathIsTrusted(const QString& canonicalFile) const;
    [[nodiscard]] common::Result<RuntimePluginInfo, PluginLoadError>
    loadCanonicalFile(const QString& canonicalFile);

    domain::GeneratorRegistry& m_registry;
    RuntimePluginOptions m_options;
    std::vector<LoadedPlugin> m_loaded;
};

} // namespace account_recommendations::infrastructure
