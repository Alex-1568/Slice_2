#include "infrastructure/plugins/RuntimePluginManager.hpp"

#include "domain/plugin/IGeneratorPlugin.hpp"

#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QPluginLoader>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QObject>

#include <exception>
#include <utility>

namespace account_recommendations::infrastructure
{
namespace
{
using Error = PluginLoadError;
using InfoResult = common::Result<RuntimePluginInfo, Error>;
using DirectoryResult = common::Result<QList<RuntimePluginInfo>, Error>;

InfoResult fail(Error::Code code, QString message)
{
    return InfoResult::failure({code, std::move(message)});
}

DirectoryResult failDirectory(Error::Code code, QString message)
{
    return DirectoryResult::failure({code, std::move(message)});
}

bool validPluginId(const QString& id)
{
    static const QRegularExpression pattern(QStringLiteral("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$"));
    return id.size() <= 64 && pattern.match(id).hasMatch();
}

bool validVersion(const QString& version)
{
    static const QRegularExpression pattern(QStringLiteral("^\\d+\\.\\d+\\.\\d+$"));
    return pattern.match(version).hasMatch();
}

QString canonicalDirectory(const QString& path)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isDir())
        return {};
    return QDir::cleanPath(info.canonicalFilePath());
}

} // namespace

RuntimePluginManager::RuntimePluginManager(domain::GeneratorRegistry& registry,
                                           RuntimePluginOptions options)
    : m_registry(registry), m_options(std::move(options))
{
    QStringList canonical;
    for (const QString& path : m_options.trustedDirectories) {
        const QString value = canonicalDirectory(path);
        if (!value.isEmpty() && !canonical.contains(value))
            canonical.append(value);
    }
    m_options.trustedDirectories = std::move(canonical);
}

RuntimePluginManager::~RuntimePluginManager() = default;

bool RuntimePluginManager::approveFileForSession(const QString& filePath)
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile())
        return false;
    const QString canonical = QDir::cleanPath(fileInfo.canonicalFilePath());
    if (canonical.isEmpty() || !QLibrary::isLibrary(canonical))
        return false;
    if (!m_approvedFiles.contains(canonical))
        m_approvedFiles.append(canonical);
    m_options.enabled = true;
    return true;
}

void RuntimePluginManager::enableForSession(const QStringList& approvedDirectories)
{
    QStringList canonical;
    for (const QString& path : approvedDirectories) {
        const QString value = canonicalDirectory(path);
        if (!value.isEmpty() && !canonical.contains(value))
            canonical.append(value);
    }
    m_options.trustedDirectories = std::move(canonical);
    // Directory approval is an explicit, broader trust decision. Do not retain prior
    // file-only approvals when the user replaces the approved directory set.
    m_approvedFiles.clear();
    m_options.enabled = !m_options.trustedDirectories.isEmpty();
}

void RuntimePluginManager::disableFurtherLoading() noexcept
{
    m_options.enabled = false;
}

bool RuntimePluginManager::isEnabled() const noexcept
{
    return m_options.enabled;
}

bool RuntimePluginManager::pathIsTrusted(const QString& canonicalFile) const
{
    const QString path = QDir::cleanPath(canonicalFile);
    if (m_approvedFiles.contains(path))
        return true;
    for (const QString& root : m_options.trustedDirectories) {
        const QString relative = QDir(root).relativeFilePath(path);
        if (relative == QStringLiteral("."))
            return false; // a directory cannot be loaded as a file
        if (!relative.startsWith(QStringLiteral("../")) && relative != QStringLiteral("..")
            && !QDir::isAbsolutePath(relative))
            return true;
    }
    return false;
}

InfoResult RuntimePluginManager::loadFile(const QString& filePath)
{
    if (!m_options.enabled)
        return fail(Error::Code::Disabled,
                    QStringLiteral("Runtime plugin loading is disabled. Explicit session consent is required."));

    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile())
        return fail(Error::Code::InvalidPath, QStringLiteral("Plugin path does not name an existing file."));
    const QString canonical = QDir::cleanPath(fileInfo.canonicalFilePath());
    if (canonical.isEmpty() || !QLibrary::isLibrary(canonical))
        return fail(Error::Code::InvalidPath, QStringLiteral("The selected file is not a recognized shared library."));
    if (!pathIsTrusted(canonical))
        return fail(Error::Code::OutsideTrustedDirectory,
                    QStringLiteral("The plugin is outside the directories approved for this session."));

    for (const LoadedPlugin& loaded : m_loaded) {
        if (loaded.info.absolutePath == canonical)
            return fail(Error::Code::DuplicatePluginId, QStringLiteral("This plugin file is already loaded."));
    }
    return loadCanonicalFile(canonical);
}

DirectoryResult RuntimePluginManager::loadDirectory(const QString& directoryPath)
{
    if (!m_options.enabled)
        return failDirectory(Error::Code::Disabled,
                             QStringLiteral("Runtime plugin loading is disabled. Explicit session consent is required."));
    const QString canonical = canonicalDirectory(directoryPath);
    if (canonical.isEmpty())
        return failDirectory(Error::Code::InvalidPath, QStringLiteral("Plugin directory does not exist."));
    // Requiring the directory itself to be approved prevents accidental broad scans.
    bool approved = false;
    for (const QString& root : m_options.trustedDirectories) {
        if (canonical == root) { approved = true; break; }
    }
    if (!approved)
        return failDirectory(Error::Code::OutsideTrustedDirectory,
                             QStringLiteral("Only an explicitly approved plugin directory can be scanned."));

    QList<RuntimePluginInfo> loaded;
    const QFileInfoList entries = QDir(canonical).entryInfoList(QDir::Files | QDir::Readable, QDir::Name);
    for (const QFileInfo& entry : entries) {
        if (!QLibrary::isLibrary(entry.fileName()))
            continue;
        auto result = loadFile(entry.absoluteFilePath());
        if (!result) {
            // A directory may contain unrelated shared libraries. Report the first candidate load
            // failure instead of silently skipping it, because a selected plugin directory is an
            // explicit trust decision and failures should be visible to the user.
            return failDirectory(result.error().code,
                                 QStringLiteral("%1: %2").arg(entry.fileName(), result.error().message));
        }
        loaded.append(std::move(result).value());
    }
    return DirectoryResult::success(std::move(loaded));
}

InfoResult RuntimePluginManager::loadCanonicalFile(const QString& canonicalFile)
{
    auto loader = std::make_unique<QPluginLoader>(canonicalFile);
    // Inspect Qt's embedded metadata before constructing the plugin object, so unrelated
    // shared libraries are rejected before their plugin entry point is invoked.
    const QJsonObject pluginMeta = loader->metaData();
    if (pluginMeta.value(QStringLiteral("IID")).toString()
        != QString::fromLatin1(AR_GENERATOR_PLUGIN_IID)) {
        return fail(Error::Code::MissingInterface,
                    QStringLiteral("The shared library does not declare the expected plugin IID."));
    }
    loader->setLoadHints(QLibrary::PreventUnloadHint);
    QObject* root = loader->instance();
    if (!root) {
        return fail(Error::Code::LoadFailed,
                    QStringLiteral("Qt could not load the plugin: %1").arg(loader->errorString()));
    }

    auto* plugin = qobject_cast<domain::IGeneratorPlugin*>(root);
    if (!plugin)
        return fail(Error::Code::MissingInterface,
                    QStringLiteral("The library does not implement the Account Recommendations generator plugin interface."));

    QString id;
    QString version;
    int apiVersion = 0;
    try {
        id = plugin->pluginId();
        version = plugin->pluginVersion();
        apiVersion = plugin->apiVersion();
    } catch (const std::exception&) {
        return fail(Error::Code::InvalidPluginMetadata,
                    QStringLiteral("Plugin metadata threw an exception."));
    } catch (...) {
        return fail(Error::Code::InvalidPluginMetadata,
                    QStringLiteral("Plugin metadata threw an unknown exception."));
    }

    if (!validPluginId(id) || !validVersion(version))
        return fail(Error::Code::InvalidPluginMetadata,
                    QStringLiteral("Plugin id or version is invalid."));
    if (apiVersion != domain::kGeneratorApiVersion)
        return fail(Error::Code::ApiMismatch,
                    QStringLiteral("Plugin API %1 is incompatible with host API %2.")
                        .arg(apiVersion).arg(domain::kGeneratorApiVersion));
    for (const LoadedPlugin& loaded : m_loaded) {
        if (loaded.info.id == id)
            return fail(Error::Code::DuplicatePluginId,
                        QStringLiteral("Plugin id \"%1\" is already loaded.").arg(id));
    }

    // Stage plugin registration in an isolated registry. Only after the callback succeeds and all
    // generator metadata passes validation do we atomically publish the entire set to the host.
    domain::GeneratorRegistry staging;
    auto registration = domain::registerPluginGenerators(*plugin, staging);
    if (!registration)
        return fail(Error::Code::RegistrationFailed, registration.error().message);

    const QList<domain::GeneratorRegistration> registrations = staging.registrationSnapshot();
    if (registrations.isEmpty())
        return fail(Error::Code::RegistrationFailed,
                    QStringLiteral("The plugin registered no generators."));

    QStringList generatorIds;
    generatorIds.reserve(registrations.size());
    for (const domain::GeneratorRegistration& registration : registrations)
        generatorIds.append(registration.metadata.id);
    RuntimePluginInfo info{id, version, canonicalFile, generatorIds};
    RuntimePluginInfo returnedInfo = info;
    try {
        // Allocate the bookkeeping slot before mutating the host registry. Once the commit succeeds,
        // appending the record is then a non-allocating move in ordinary implementations.
        m_loaded.reserve(m_loaded.size() + 1U);
    } catch (const std::exception&) {
        return fail(Error::Code::LoadFailed,
                    QStringLiteral("Unable to retain plugin lifetime bookkeeping."));
    }
    try {
        auto commit = m_registry.registerGeneratorsAtomically(registrations);
        if (!commit)
            return fail(Error::Code::RegistrationFailed, commit.error().message);
    } catch (const std::exception&) {
        return fail(Error::Code::RegistrationFailed,
                    QStringLiteral("Atomic plugin registration threw an exception."));
    } catch (...) {
        return fail(Error::Code::RegistrationFailed,
                    QStringLiteral("Atomic plugin registration threw an unknown exception."));
    }

    m_loaded.push_back({std::move(info), std::move(loader)});
    return InfoResult::success(std::move(returnedInfo));
}

QList<RuntimePluginInfo> RuntimePluginManager::loadedPlugins() const
{
    QList<RuntimePluginInfo> result;
    result.reserve(static_cast<qsizetype>(m_loaded.size()));
    for (const LoadedPlugin& plugin : m_loaded)
        result.append(plugin.info);
    return result;
}

} // namespace account_recommendations::infrastructure
