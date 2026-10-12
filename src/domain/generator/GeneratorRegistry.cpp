#include "domain/generator/GeneratorRegistry.hpp"

#include <algorithm>
#include <mutex>
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

qsizetype GeneratorRegistry::size() const
{
    std::shared_lock lock(m_mutex);
    return static_cast<qsizetype>(m_entries.size());
}

} // namespace account_recommendations::domain
