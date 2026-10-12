#include "domain/generator/ConfigurationSchema.hpp"

#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

#include <cmath>
#include <utility>

namespace account_recommendations::domain
{

using common::ValidationResult;

FieldSpec FieldSpec::integer(QString key, QString label, qint64 defaultValue, qint64 minimum,
                             qint64 maximum, QString group, QString description)
{
    FieldSpec spec;
    spec.key = std::move(key);
    spec.label = std::move(label);
    spec.description = std::move(description);
    spec.group = std::move(group);
    spec.type = FieldType::Integer;
    spec.defaultValue = QJsonValue(defaultValue);
    spec.minimum = minimum;
    spec.maximum = maximum;
    return spec;
}

FieldSpec FieldSpec::boolean(QString key, QString label, bool defaultValue, QString group,
                             QString description)
{
    FieldSpec spec;
    spec.key = std::move(key);
    spec.label = std::move(label);
    spec.description = std::move(description);
    spec.group = std::move(group);
    spec.type = FieldType::Boolean;
    spec.defaultValue = QJsonValue(defaultValue);
    return spec;
}

FieldSpec FieldSpec::string(QString key, QString label, QString defaultValue, qsizetype maxLength,
                            QString group, QString description)
{
    FieldSpec spec;
    spec.key = std::move(key);
    spec.label = std::move(label);
    spec.description = std::move(description);
    spec.group = std::move(group);
    spec.type = FieldType::String;
    spec.defaultValue = QJsonValue(std::move(defaultValue));
    spec.maxLength = maxLength;
    return spec;
}

FieldSpec FieldSpec::enumeration(QString key, QString label, QStringList values,
                                 QString defaultValue, QString group, QString description)
{
    FieldSpec spec;
    spec.key = std::move(key);
    spec.label = std::move(label);
    spec.description = std::move(description);
    spec.group = std::move(group);
    spec.type = FieldType::Enumeration;
    spec.allowedValues = std::move(values);
    spec.defaultValue = QJsonValue(std::move(defaultValue));
    return spec;
}

namespace
{

// Largest integer a JSON double represents exactly.
constexpr double kMaxExactInteger = 9007199254740992.0;

void validateField(const FieldSpec& spec, const QJsonValue& value,
                   const common::ApplicationLimits& limits, ValidationResult& out)
{
    switch (spec.type) {
    case FieldType::Integer: {
        if (!value.isDouble()) {
            out.addError(spec.key, QStringLiteral("invalid_type"),
                         QStringLiteral("%1 must be a whole number.").arg(spec.label));
            return;
        }
        const double raw = value.toDouble();
        if (!std::isfinite(raw) || raw != std::floor(raw) || std::fabs(raw) > kMaxExactInteger) {
            out.addError(spec.key, QStringLiteral("invalid_value"),
                         QStringLiteral("%1 must be a whole number.").arg(spec.label));
            return;
        }
        const auto number = static_cast<qint64>(raw);
        const bool belowMin = spec.minimum && number < *spec.minimum;
        const bool aboveMax = spec.maximum && number > *spec.maximum;
        if (belowMin || aboveMax) {
            QString message;
            if (spec.minimum && spec.maximum)
                message = QStringLiteral("%1 must be between %2 and %3.")
                              .arg(spec.label)
                              .arg(*spec.minimum)
                              .arg(*spec.maximum);
            else if (spec.minimum)
                message = QStringLiteral("%1 must be at least %2.").arg(spec.label).arg(*spec.minimum);
            else
                message = QStringLiteral("%1 must be at most %2.").arg(spec.label).arg(*spec.maximum);
            out.addError(spec.key, QStringLiteral("out_of_range"), message);
        }
        return;
    }
    case FieldType::Boolean:
        if (!value.isBool())
            out.addError(spec.key, QStringLiteral("invalid_type"),
                         QStringLiteral("%1 must be true or false.").arg(spec.label));
        return;
    case FieldType::String:
        if (!value.isString()) {
            out.addError(spec.key, QStringLiteral("invalid_type"),
                         QStringLiteral("%1 must be text.").arg(spec.label));
            return;
        }
    {
        const qsizetype ceiling =
            qMin(spec.maxLength, static_cast<qsizetype>(limits.maxStringLength));
        if (value.toString().size() > ceiling)
            out.addError(spec.key, QStringLiteral("too_long"),
                         QStringLiteral("%1 must be at most %2 characters.")
                             .arg(spec.label)
                             .arg(ceiling));
    }
        return;
    case FieldType::Enumeration:
        if (!value.isString() || !spec.allowedValues.contains(value.toString()))
            out.addError(spec.key, QStringLiteral("invalid_choice"),
                         QStringLiteral("%1 must be one of: %2.")
                             .arg(spec.label, spec.allowedValues.join(QStringLiteral(", "))));
        return;
    }
}

QString typeName(FieldType type)
{
    switch (type) {
    case FieldType::Integer: return QStringLiteral("integer");
    case FieldType::Boolean: return QStringLiteral("boolean");
    case FieldType::String: return QStringLiteral("string");
    case FieldType::Enumeration: return QStringLiteral("enum");
    }
    return QStringLiteral("string");
}

} // namespace

ConfigurationSchema::ConfigurationSchema(QList<FieldSpec> fields) : m_fields(std::move(fields)) {}

const FieldSpec* ConfigurationSchema::find(const QString& key) const noexcept
{
    for (const FieldSpec& field : m_fields) {
        if (field.key == key)
            return &field;
    }
    return nullptr;
}

QJsonObject ConfigurationSchema::defaults() const
{
    return applyDefaults({});
}

QJsonObject ConfigurationSchema::applyDefaults(QJsonObject parameters) const
{
    for (const FieldSpec& field : m_fields) {
        if (!parameters.contains(field.key) && !field.defaultValue.isUndefined())
            parameters.insert(field.key, field.defaultValue);
    }
    return parameters;
}

ValidationResult ConfigurationSchema::validate(const QJsonObject& parameters,
                                               const common::ApplicationLimits& limits) const
{
    ValidationResult result;

    if (static_cast<std::size_t>(parameters.size()) > limits.maxParameterCount) {
        result.addError(QString(), QStringLiteral("too_many_parameters"),
                        QStringLiteral("Too many settings were supplied (at most %1 are allowed).")
                            .arg(limits.maxParameterCount));
        return result; // no per-key work on hostile input
    }

    for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
        if (find(it.key()) == nullptr)
            result.addError(it.key(), QStringLiteral("unknown_field"),
                            QStringLiteral("Unknown setting \"%1\".").arg(it.key().left(64)));
    }

    for (const FieldSpec& field : m_fields) {
        const auto it = parameters.constFind(field.key);
        if (it == parameters.constEnd()) {
            if (field.required)
                result.addError(field.key, QStringLiteral("required"),
                                QStringLiteral("%1 is required.").arg(field.label));
            continue;
        }
        validateField(field, it.value(), limits, result);
    }
    return result;
}

ValidationResult ConfigurationSchema::validateDefinition() const
{
    static const QRegularExpression keyPattern(QStringLiteral("^[A-Za-z][A-Za-z0-9]*$"));

    ValidationResult result;
    QSet<QString> seen;
    for (const FieldSpec& field : m_fields) {
        if (!keyPattern.match(field.key).hasMatch()) {
            result.addError(field.key, QStringLiteral("invalid_key"),
                            QStringLiteral("Field key \"%1\" is not valid.").arg(field.key));
            continue;
        }
        if (seen.contains(field.key))
            result.addError(field.key, QStringLiteral("duplicate_key"),
                            QStringLiteral("Field key \"%1\" is declared twice.").arg(field.key));
        seen.insert(field.key);

        if (field.label.isEmpty())
            result.addError(field.key, QStringLiteral("missing_label"),
                            QStringLiteral("Field \"%1\" has no label.").arg(field.key));
        if (field.type == FieldType::Integer && field.minimum && field.maximum
            && *field.minimum > *field.maximum)
            result.addError(field.key, QStringLiteral("invalid_range"),
                            QStringLiteral("Field \"%1\" has minimum above maximum.").arg(field.key));
        if (field.type == FieldType::Enumeration && field.allowedValues.isEmpty())
            result.addError(field.key, QStringLiteral("empty_enumeration"),
                            QStringLiteral("Field \"%1\" has no allowed values.").arg(field.key));

        if (!field.defaultValue.isUndefined()) {
            ValidationResult defaultCheck;
            validateField(field, field.defaultValue, common::ApplicationLimits::defaults(), defaultCheck);
            if (!defaultCheck.isValid())
                result.addError(field.key, QStringLiteral("invalid_default"),
                                QStringLiteral("Default for \"%1\" violates its own constraints.")
                                    .arg(field.key));
        }
    }
    return result;
}

QJsonObject ConfigurationSchema::toJson() const
{
    QJsonArray fields;
    for (const FieldSpec& field : m_fields) {
        QJsonObject entry;
        entry.insert(QStringLiteral("key"), field.key);
        entry.insert(QStringLiteral("label"), field.label);
        entry.insert(QStringLiteral("description"), field.description);
        entry.insert(QStringLiteral("group"), field.group);
        entry.insert(QStringLiteral("type"), typeName(field.type));
        entry.insert(QStringLiteral("required"), field.required);
        if (!field.defaultValue.isUndefined())
            entry.insert(QStringLiteral("default"), field.defaultValue);
        if (field.minimum)
            entry.insert(QStringLiteral("minimum"), static_cast<double>(*field.minimum));
        if (field.maximum)
            entry.insert(QStringLiteral("maximum"), static_cast<double>(*field.maximum));
        if (field.type == FieldType::String)
            entry.insert(QStringLiteral("maxLength"), static_cast<double>(field.maxLength));
        if (field.type == FieldType::Enumeration)
            entry.insert(QStringLiteral("values"), QJsonArray::fromStringList(field.allowedValues));
        fields.append(entry);
    }
    QJsonObject root;
    root.insert(QStringLiteral("fields"), fields);
    return root;
}

} // namespace account_recommendations::domain
