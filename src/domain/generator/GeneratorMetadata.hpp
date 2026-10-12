#pragma once

#include "common/validation/ValidationResult.hpp"
#include "domain/generator/ConfigurationSchema.hpp"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace account_recommendations::domain
{

/// Describes one column a generator emits. `sensitive` drives SensitiveDataPolicy:
/// logging, persistence, display, export and clipboard treat the field generically.
struct OutputFieldSpec
{
    QString key;
    QString label;
    bool sensitive = false;
};

struct GeneratorMetadata
{
    QString id;          ///< stable, lowercase-kebab ("password", "email-identifier")
    QString displayName;
    QString description;
    QString category;    ///< sidebar grouping ("Security", "Identity", ...)
    QString version;     ///< "major.minor.patch"
    QStringList tags;

    bool supportsBatch = true;
    bool supportsPresets = true;
    bool supportsUniqueness = false;
    bool requiresSecureRandom = false; ///< refuse non-CSPRNG providers outside test mode

    ConfigurationSchema configurationSchema;
    QList<OutputFieldSpec> outputFields;
};

/// Checks one generated record against the generator's declared output fields and the flat-record
/// contract (see SensitiveDataPolicy.hpp): every key must be declared (`undeclared_field`) and every
/// value must be a scalar (`nested_value`). Intended for generator tests and debug assertions.
[[nodiscard]] common::ValidationResult validateRecord(const GeneratorMetadata& metadata, const QJsonObject& record);

/// Structural validation used by the registry (id/version format, unique keys,
/// non-empty names, schema definition validity).
[[nodiscard]] common::ValidationResult validateMetadata(const GeneratorMetadata& metadata);

} // namespace account_recommendations::domain
