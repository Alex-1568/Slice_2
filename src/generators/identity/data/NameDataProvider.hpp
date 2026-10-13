#pragma once

#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

namespace account_recommendations::generators::identity
{

/// A local, synthetic name dataset. These are common/example names for generated test data,
/// not records loaded from a real-person directory or an online service.
struct NameDataset
{
    QString locale;
    QString language;
    QString countryCode;
    QStringList firstNames;
    QStringList lastNames;
    QStringList nicknames;
};

/// Read-only name-data boundary. Implementations must be thread-safe after construction and
/// must not perform network access. Returning a value avoids exposing mutable global data.
class INameDataProvider
{
public:
    virtual ~INameDataProvider() = default;
    [[nodiscard]] virtual QStringList supportedLocales() const = 0;
    [[nodiscard]] virtual std::optional<NameDataset> dataset(const QString& locale) const = 0;
};

/// Built-in, small local presets. The interface allows larger local datasets to be supplied
/// later without changing generators. No remote lookup or real-person API is used.
/// Canonical locale keys exposed by the built-in provider and generator schemas.
[[nodiscard]] QStringList builtInNameLocales();

class BuiltinNameDataProvider final : public INameDataProvider
{
public:
    [[nodiscard]] QStringList supportedLocales() const override;
    [[nodiscard]] std::optional<NameDataset> dataset(const QString& locale) const override;
};

[[nodiscard]] bool isUsableNameDataset(const NameDataset& dataset);
[[nodiscard]] std::shared_ptr<const INameDataProvider> defaultNameDataProvider();

} // namespace account_recommendations::generators::identity
