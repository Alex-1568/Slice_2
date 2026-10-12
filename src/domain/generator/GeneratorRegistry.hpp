#pragma once

#include "common/result/Result.hpp"
#include "common/validation/ValidationResult.hpp"
#include "domain/generator/GeneratorMetadata.hpp"
#include "domain/generator/IGenerator.hpp"

#include <QList>
#include <QString>

#include <memory>
#include <optional>
#include <shared_mutex>
#include <vector>

namespace account_recommendations::domain
{

struct RegistryError
{
    enum class Code
    {
        NullGenerator,
        InvalidMetadata,
        DuplicateId
    };

    Code code = Code::InvalidMetadata;
    QString message;
    common::ValidationResult validation;
};

/// Thread-safe registry of generators.
///
/// `find` returns shared ownership so a worker that is mid-generation keeps its
/// generator alive even if another thread unregisters it. Metadata is captured and
/// validated once at registration time; list() returns that snapshot.
class GeneratorRegistry
{
public:
    GeneratorRegistry() = default;
    GeneratorRegistry(const GeneratorRegistry&) = delete;
    GeneratorRegistry& operator=(const GeneratorRegistry&) = delete;

    [[nodiscard]] common::Result<void, RegistryError>
    registerGenerator(std::unique_ptr<IGenerator> generator);

    bool unregisterGenerator(const QString& id);

    [[nodiscard]] std::shared_ptr<const IGenerator> find(const QString& id) const;
    [[nodiscard]] std::optional<GeneratorMetadata> metadataFor(const QString& id) const;

    /// Registration order (stable); the GUI groups by category itself.
    [[nodiscard]] QList<GeneratorMetadata> list() const;
    [[nodiscard]] qsizetype size() const;

private:
    struct Entry
    {
        GeneratorMetadata metadata;
        std::shared_ptr<const IGenerator> generator;
    };

    mutable std::shared_mutex m_mutex;
    std::vector<Entry> m_entries;
};

} // namespace account_recommendations::domain
