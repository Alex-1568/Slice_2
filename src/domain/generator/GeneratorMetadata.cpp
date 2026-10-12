#include "domain/generator/GeneratorMetadata.hpp"

#include <QRegularExpression>
#include <QSet>

namespace account_recommendations::domain
{

common::ValidationResult validateMetadata(const GeneratorMetadata& metadata)
{
    static const QRegularExpression idPattern(
        QStringLiteral("^[a-z][a-z0-9]*(-[a-z0-9]+)*$"));
    static const QRegularExpression versionPattern(QStringLiteral("^\\d+\\.\\d+\\.\\d+$"));
    constexpr qsizetype kMaxIdLength = 64;

    common::ValidationResult result;

    if (metadata.id.isEmpty() || metadata.id.size() > kMaxIdLength
        || !idPattern.match(metadata.id).hasMatch())
        result.addError(QStringLiteral("id"), QStringLiteral("invalid_id"),
                        QStringLiteral("Generator id must be lowercase kebab-case (max 64 chars)."));
    if (metadata.displayName.trimmed().isEmpty())
        result.addError(QStringLiteral("displayName"), QStringLiteral("missing_display_name"),
                        QStringLiteral("Generator display name must not be empty."));
    if (metadata.category.trimmed().isEmpty())
        result.addError(QStringLiteral("category"), QStringLiteral("missing_category"),
                        QStringLiteral("Generator category must not be empty."));
    if (!versionPattern.match(metadata.version).hasMatch())
        result.addError(QStringLiteral("version"), QStringLiteral("invalid_version"),
                        QStringLiteral("Generator version must look like 1.0.0."));

    result.merge(metadata.configurationSchema.validateDefinition());

    QSet<QString> outputKeys;
    for (const OutputFieldSpec& field : metadata.outputFields) {
        if (field.key.isEmpty() || field.label.isEmpty())
            result.addError(QStringLiteral("outputFields"), QStringLiteral("invalid_output_field"),
                            QStringLiteral("Output fields need a key and a label."));
        if (outputKeys.contains(field.key))
            result.addError(QStringLiteral("outputFields"), QStringLiteral("duplicate_output_field"),
                            QStringLiteral("Output field \"%1\" is declared twice.").arg(field.key));
        outputKeys.insert(field.key);
    }
    if (metadata.outputFields.isEmpty())
        result.addError(QStringLiteral("outputFields"), QStringLiteral("no_output_fields"),
                        QStringLiteral("A generator must declare at least one output field."));

    return result;
}

common::ValidationResult validateRecord(const GeneratorMetadata& metadata, const QJsonObject& record)
{
    QSet<QString> declared;
    for (const OutputFieldSpec& field : metadata.outputFields)
        declared.insert(field.key);

    common::ValidationResult result;
    for (auto it = record.constBegin(); it != record.constEnd(); ++it) {
        if (!declared.contains(it.key()))
            result.addError(it.key(), QStringLiteral("undeclared_field"),
                            QStringLiteral("Record field \"%1\" is not declared in the generator's outputFields.").arg(it.key().left(64)));
        if (it.value().isObject() || it.value().isArray())
            result.addError(it.key(), QStringLiteral("nested_value"),
                            QStringLiteral("Record field \"%1\" is not a scalar; records must be flat.").arg(it.key().left(64)));
    }
    return result;
}

} // namespace account_recommendations::domain
