#pragma once

#include "common/limits/ApplicationLimits.hpp"
#include "common/validation/ValidationResult.hpp"

#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace account_recommendations::domain
{

enum class FieldType
{
    Integer,
    Boolean,
    String,
    Enumeration
};

/// One configuration field. This is an application-specific schema (NOT JSON
/// Schema): it carries exactly what a dynamic GUI form and generic validation need.
struct FieldSpec
{
    QString key;         ///< JSON key, [A-Za-z][A-Za-z0-9]*
    QString label;       ///< UI label
    QString description; ///< tooltip / help text
    QString group;       ///< UI grouping (e.g. "Character sets")
    FieldType type = FieldType::String;
    QJsonValue defaultValue{QJsonValue::Undefined};
    std::optional<qint64> minimum;   ///< Integer only
    std::optional<qint64> maximum;   ///< Integer only
    qsizetype maxLength = 256;       ///< String only (UTF-16 code units)
    QStringList allowedValues;       ///< Enumeration only
    bool required = false;

    [[nodiscard]] static FieldSpec integer(QString key, QString label, qint64 defaultValue,
                                           qint64 minimum, qint64 maximum, QString group = {},
                                           QString description = {});
    [[nodiscard]] static FieldSpec boolean(QString key, QString label, bool defaultValue,
                                           QString group = {}, QString description = {});
    [[nodiscard]] static FieldSpec string(QString key, QString label, QString defaultValue,
                                          qsizetype maxLength, QString group = {},
                                          QString description = {});
    [[nodiscard]] static FieldSpec enumeration(QString key, QString label, QStringList values,
                                               QString defaultValue, QString group = {},
                                               QString description = {});
};

class ConfigurationSchema
{
public:
    ConfigurationSchema() = default;
    explicit ConfigurationSchema(QList<FieldSpec> fields);

    [[nodiscard]] const QList<FieldSpec>& fields() const noexcept { return m_fields; }
    [[nodiscard]] const FieldSpec* find(const QString& key) const noexcept;

    /// Object holding every field's default value.
    [[nodiscard]] QJsonObject defaults() const;
    /// `parameters` plus defaults for keys that are absent.
    [[nodiscard]] QJsonObject applyDefaults(QJsonObject parameters) const;

    /// Checks types, ranges, enum membership, string length, required and unknown keys.
    /// Resource ceilings come from `limits`: more than maxParameterCount keys is
    /// rejected before any per-key work, and string values are capped at
    /// min(field.maxLength, limits.maxStringLength). Limits can only tighten a field.
    /// Does not check cross-field rules; generators add those on top.
    [[nodiscard]] common::ValidationResult
    validate(const QJsonObject& parameters,
             const common::ApplicationLimits& limits = common::ApplicationLimits::defaults()) const;

    /// Checks the schema itself (unique keys, defaults conform, min <= max).
    [[nodiscard]] common::ValidationResult validateDefinition() const;

    /// Serialised form for the dynamic GUI and for presets.
    [[nodiscard]] QJsonObject toJson() const;

private:
    QList<FieldSpec> m_fields;
};

} // namespace account_recommendations::domain
