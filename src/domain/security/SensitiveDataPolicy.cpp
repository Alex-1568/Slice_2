#include "domain/security/SensitiveDataPolicy.hpp"

#include <utility>

namespace account_recommendations::domain
{

SensitiveDataPolicy::SensitiveDataPolicy(const QList<OutputFieldSpec>& fields,
                                         SensitiveDataSettings settings)
    : m_settings(settings)
{
    for (const OutputFieldSpec& field : fields) {
        m_declaredKeys.insert(field.key);
        if (field.sensitive)
            m_sensitiveKeys.insert(field.key);
    }
}

SensitiveDataPolicy SensitiveDataPolicy::forGenerator(const GeneratorMetadata& metadata,
                                                      SensitiveDataSettings settings)
{
    return SensitiveDataPolicy(metadata.outputFields, settings);
}

bool SensitiveDataPolicy::isSensitive(const QString& key) const
{
    return m_sensitiveKeys.contains(key) || !m_declaredKeys.contains(key);
}

bool SensitiveDataPolicy::isSensitiveValue(const QString& key, const QJsonValue& value) const
{
    return isSensitive(key) || value.isObject() || value.isArray();
}

FieldHandling SensitiveDataPolicy::handling(const QString& key, const QJsonValue& value, OutputChannel channel,
                                            const ReleaseAuthorization& authorization) const
{
    if (!isSensitiveValue(key, value))
        return FieldHandling::Allow;

    switch (channel) {
    case OutputChannel::Log:
        return FieldHandling::Redact; // never configurable, never unlockable
    case OutputChannel::Persistence:
    case OutputChannel::Workspace:
        return m_settings.persistSensitiveValues ? FieldHandling::Allow : FieldHandling::Omit;
    case OutputChannel::Display:
        return m_settings.revealOnDisplay ? FieldHandling::Allow : FieldHandling::Mask;
    case OutputChannel::Export:
    case OutputChannel::Clipboard:
        return authorization.permits(channel) ? FieldHandling::Allow : FieldHandling::Redact;
    }
    return FieldHandling::Redact;
}

bool SensitiveDataPolicy::requiresConfirmation(const QJsonObject& record, OutputChannel channel) const
{
    if (channel != OutputChannel::Export && channel != OutputChannel::Clipboard)
        return false;
    for (auto it = record.constBegin(); it != record.constEnd(); ++it) {
        if (isSensitiveValue(it.key(), it.value()))
            return true;
    }
    return false;
}

bool SensitiveDataPolicy::requiresConfirmation(const QList<QJsonObject>& records, OutputChannel channel) const
{
    for (const QJsonObject& record : records) {
        if (requiresConfirmation(record, channel))
            return true;
    }
    return false;
}

QJsonObject SensitiveDataPolicy::sanitize(const QJsonObject& record, OutputChannel channel,
                                          const ReleaseAuthorization& authorization) const
{
    QJsonObject result;
    for (auto it = record.constBegin(); it != record.constEnd(); ++it) {
        switch (handling(it.key(), it.value(), channel, authorization)) {
        case FieldHandling::Allow:
            result.insert(it.key(), it.value());
            break;
        case FieldHandling::Mask:
            result.insert(it.key(), maskText());
            break;
        case FieldHandling::Redact:
            result.insert(it.key(), redactionText());
            break;
        case FieldHandling::Omit:
            break;
        }
    }
    return result;
}

QList<QJsonObject> SensitiveDataPolicy::sanitize(const QList<QJsonObject>& records, OutputChannel channel,
                                                 const ReleaseAuthorization& authorization) const
{
    QList<QJsonObject> result;
    result.reserve(records.size());
    for (const QJsonObject& record : records)
        result.append(sanitize(record, channel, authorization));
    return result;
}

QString SensitiveDataPolicy::maskText()
{
    return QStringLiteral("********"); // fixed width: does not leak the real length
}

QString SensitiveDataPolicy::redactionText()
{
    return QStringLiteral("[REDACTED]");
}

} // namespace account_recommendations::domain
