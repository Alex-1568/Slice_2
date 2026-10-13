#include "domain/generator/GeneratorRegistry.hpp"

#include <algorithm>
#include <mutex>
#include <QSet>
#include <exception>
#include <iterator>
#include <utility>

namespace account_recommendations::domain
{

using common::Result;

Result<void, RegistryError> GeneratorRegistry::registerGenerator(std::unique_ptr<IGenerator> generator)
{
    using VoidResult = Result<void, RegistryError>;

    if (!generator)
        return VoidResult::failure(
            {RegistryError::Code::NullGenerator, QStringLiteral("Generator must not be null."), {}});

    GeneratorMetadata metadata = generator->metadata();
    common::ValidationResult validation = validateMetadata(metadata);
    if (!validation.isValid()) {
        QString message = QStringLiteral("Invalid generator metadata: %1").arg(validation.summary());
        return VoidResult::failure(
            {RegistryError::Code::InvalidMetadata, std::move(message), std::move(validation)});
    }

    std::unique_lock lock(m_mutex);
    const bool duplicate = std::any_of(m_entries.cbegin(), m_entries.cend(), [&](const Entry& entry) {
        return entry.metadata.id == metadata.id;
    });
    if (duplicate)
        return VoidResult::failure(
            {RegistryError::Code::DuplicateId,
             QStringLiteral("A generator with id \"%1\" is already registered.").arg(metadata.id),
             {}});

    std::shared_ptr<const IGenerator> shared = std::move(generator);
    m_entries.push_back({std::move(metadata), std::move(shared)});
    return VoidResult::success();
}

Result<void, RegistryError> GeneratorRegistry::registerGeneratorsAtomically(
    const QList<GeneratorRegistration>& registrations)
{
    using VoidResult = Result<void, RegistryError>;
    if (registrations.isEmpty())
        return VoidResult::failure({RegistryError::Code::InvalidMetadata,
                                    QStringLiteral("A plugin must provide at least one generator."), {}});

    std::vector<Entry> pending;
    pending.reserve(static_cast<std::size_t>(registrations.size()));
    QSet<QString> incomingIds;
    for (const GeneratorRegistration& registration : registrations) {
        if (!registration.generator)
            return VoidResult::failure({RegistryError::Code::NullGenerator,
                                        QStringLiteral("Generator batch contains a null entry."), {}});
        common::ValidationResult issues = validateMetadata(registration.metadata);
        if (!issues.isValid())
            return VoidResult::failure({RegistryError::Code::InvalidMetadata,
                                        QStringLiteral("Invalid generator metadata: %1").arg(issues.summary()),
                                        std::move(issues)});
        if (incomingIds.contains(registration.metadata.id))
            return VoidResult::failure({RegistryError::Code::DuplicateId,
                                        QStringLiteral("Generator batch contains duplicate id \"%1\".")
                                            .arg(registration.metadata.id), {}});
        incomingIds.insert(registration.metadata.id);
        pending.push_back({registration.metadata, registration.generator});
    }

    std::unique_lock lock(m_mutex);
    for (const Entry& entry : pending) {
        const bool duplicate = std::any_of(m_entries.cbegin(), m_entries.cend(), [&](const Entry& existing) {
            return existing.metadata.id == entry.metadata.id;
        });
        if (duplicate)
            return VoidResult::failure({RegistryError::Code::DuplicateId,
                                        QStringLiteral("A generator with id \"%1\" is already registered.")
                                            .arg(entry.metadata.id), {}});
    }

    // Reserve before modifying the registry, so allocation failure cannot leave a prefix registered.
    try {
        m_entries.reserve(m_entries.size() + pending.size());
    } catch (const std::exception& error) {
        return VoidResult::failure({RegistryError::Code::InvalidMetadata,
                                    QStringLiteral("Unable to reserve registry storage: %1")
                                        .arg(QString::fromUtf8(error.what())), {}});
    }
    m_entries.insert(m_entries.end(), std::make_move_iterator(pending.begin()),
                     std::make_move_iterator(pending.end()));
    return VoidResult::success();
}

bool GeneratorRegistry::unregisterGenerator(const QString& id)
{
    std::unique_lock lock(m_mutex);
    const auto it = std::find_if(m_entries.begin(), m_entries.end(),
                                 [&](const Entry& entry) { return entry.metadata.id == id; });
    if (it == m_entries.end())
        return false;
    m_entries.erase(it);
    return true;
}

std::shared_ptr<const IGenerator> GeneratorRegistry::find(const QString& id) const
{
    std::shared_lock lock(m_mutex);
    const auto it = std::find_if(m_entries.cbegin(), m_entries.cend(),
                                 [&](const Entry& entry) { return entry.metadata.id == id; });
    return it == m_entries.cend() ? nullptr : it->generator;
}

std::optional<GeneratorMetadata> GeneratorRegistry::metadataFor(const QString& id) const
{
    std::shared_lock lock(m_mutex);
    const auto it = std::find_if(m_entries.cbegin(), m_entries.cend(),
                                 [&](const Entry& entry) { return entry.metadata.id == id; });
    if (it == m_entries.cend())
        return std::nullopt;
    return it->metadata;
}

QList<GeneratorMetadata> GeneratorRegistry::list() const
{
    std::shared_lock lock(m_mutex);
    QList<GeneratorMetadata> result;
    result.reserve(static_cast<qsizetype>(m_entries.size()));
    for (const Entry& entry : m_entries)
        result.append(entry.metadata);
    return result;
}

QList<GeneratorRegistration> GeneratorRegistry::registrationSnapshot() const
{
    std::shared_lock lock(m_mutex);
    QList<GeneratorRegistration> result;
    result.reserve(static_cast<qsizetype>(m_entries.size()));
    for (const Entry& entry : m_entries)
        result.append({entry.metadata, entry.generator});
    return result;
}

qsizetype GeneratorRegistry::size() const
{
    std::shared_lock lock(m_mutex);
    return static_cast<qsizetype>(m_entries.size());
}

} // namespace account_recommendations::domain
