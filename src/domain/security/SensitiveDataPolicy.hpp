#pragma once

#include "domain/generator/GeneratorMetadata.hpp"

#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>

namespace account_recommendations::domain
{

/// Where a record's values are about to go. See docs/SENSITIVE_DATA.md for which
/// channels are enforced today and which are contract-only until their consumer exists.
enum class OutputChannel
{
    Display,     ///< result table / any on-screen rendering
    Log,         ///< diagnostics, crash reports, telemetry (always redacted)
    Persistence, ///< presets, settings, any file the app writes by itself
    Workspace,   ///< saved workspace/project state containing generated results
    Export,      ///< user-initiated file export (needs explicit authorization)
    Clipboard    ///< copy to system clipboard (needs explicit authorization)
};

enum class FieldHandling
{
    Allow,  ///< value may pass through unchanged
    Mask,   ///< replace with a fixed-width mask (UI)
    Redact, ///< replace with a redaction marker (logs, unauthorised export/clipboard)
    Omit    ///< remove the key entirely (persistence)
};

/// Explicit, user-controlled opt-ins. Everything defaults to the safest behaviour.
struct SensitiveDataSettings
{
    /// Applies to Persistence AND Workspace. false == "Do not persist generated passwords".
    bool persistSensitiveValues = false;
    bool revealOnDisplay = false;        ///< show/hide toggle in the results view
};

/// Proof, passed explicitly at the call site, that the USER confirmed releasing sensitive
/// values to ONE channel (Export or Clipboard). Default-constructed == no authorization.
///
/// Honest limits: this is a type-level speed bump, not an unforgeable capability: any code
/// in the process can call userConfirmed(). What it does guarantee is that a sensitive value
/// can never reach Export/Clipboard through sanitize()/handling() by accident: the caller
/// must write the words `ReleaseAuthorization::userConfirmed(...)`, bound to that channel.
/// A scan test restricts those words to this module, src/gui/ and tests.
/// It cannot unlock Log, and it does not apply to other channels.
class ReleaseAuthorization
{
public:
    ReleaseAuthorization() noexcept = default;

    [[nodiscard]] static ReleaseAuthorization none() noexcept { return {}; }

    /// Call ONLY after the confirmation dialog for `channel` was accepted by the user.
    [[nodiscard]] static ReleaseAuthorization userConfirmed(OutputChannel channel) noexcept
    {
        ReleaseAuthorization authorization;
        authorization.m_confirmed = true;
        authorization.m_channel = channel;
        return authorization;
    }

    [[nodiscard]] bool permits(OutputChannel channel) const noexcept
    {
        return m_confirmed && m_channel == channel
               && (channel == OutputChannel::Export || channel == OutputChannel::Clipboard);
    }

private:
    bool m_confirmed = false;
    OutputChannel m_channel = OutputChannel::Log;
};

/// RECORD SHAPE CONTRACT (decided before Slice 2)
///  * Generated records are FLAT: every output field holds a SCALAR JSON value (string, number,
///    boolean or null). Nested objects/arrays are not part of the contract.
///    domain::validateRecord() checks this, and every generator's tests must call it.
///  * Sensitivity is decided per TOP-LEVEL key and applies to the WHOLE value. The policy never
///    recurses: names of nested keys are irrelevant, there are no per-nested-key declarations,
///    and future generators must not assume otherwise. Generators needing partial sensitivity
///    must flatten into separate declared fields.
///  * Defence in depth (fail closed): a value that IS an object or array is treated as sensitive
///    as a whole (all descendants), even under a field declared non-sensitive, because nothing
///    can be said about what it contains.
///
/// SECURITY POLICY ABSTRACTION. Every component that moves generated values out of the
/// generator (logger, preset/workspace store, exporter, clipboard, results view) MUST
/// route records through sanitize()/handling(). The policy decides; it performs no I/O,
/// shows no dialogs and wipes nothing, and it makes no promise about copies of values that
/// already exist elsewhere in memory.
///
/// Fail-closed rules enforced here:
///  * A key not declared in the generator's outputFields is treated as sensitive.
///  * Export/Clipboard never receive a sensitive value without a matching
///    ReleaseAuthorization: without it the value is replaced by the redaction marker.
///  * requiresConfirmation() inspects the ACTUAL record(s), so an undeclared extra field
///    triggers confirmation even if the metadata declares nothing sensitive.
class SensitiveDataPolicy
{
public:
    explicit SensitiveDataPolicy(const QList<OutputFieldSpec>& fields,
                                 SensitiveDataSettings settings = {});

    [[nodiscard]] static SensitiveDataPolicy forGenerator(const GeneratorMetadata& metadata,
                                                          SensitiveDataSettings settings = {});

    /// Key-level: declared sensitive, or not declared at all.
    [[nodiscard]] bool isSensitive(const QString& key) const;
    /// Value-level (what sanitize() uses): key-level OR the value is an object/array.
    [[nodiscard]] bool isSensitiveValue(const QString& key, const QJsonValue& value) const;
    [[nodiscard]] bool hasSensitiveFields() const noexcept { return !m_sensitiveKeys.isEmpty(); }

    /// Handling of ONE top-level entry; applies to the whole value including any descendants.
    [[nodiscard]] FieldHandling handling(const QString& key, const QJsonValue& value, OutputChannel channel,
                                         const ReleaseAuthorization& authorization = {}) const;

    /// True if releasing `record` (or any of `records`) to `channel` needs the user's
    /// confirmation: channel is Export/Clipboard AND some key present is sensitive
    /// (declared sensitive, or undeclared).
    [[nodiscard]] bool requiresConfirmation(const QJsonObject& record, OutputChannel channel) const;
    [[nodiscard]] bool requiresConfirmation(const QList<QJsonObject>& records, OutputChannel channel) const;

    [[nodiscard]] QJsonObject sanitize(const QJsonObject& record, OutputChannel channel,
                                       const ReleaseAuthorization& authorization = {}) const;
    [[nodiscard]] QList<QJsonObject> sanitize(const QList<QJsonObject>& records, OutputChannel channel,
                                              const ReleaseAuthorization& authorization = {}) const;

    [[nodiscard]] static QString maskText();
    [[nodiscard]] static QString redactionText();

private:
    QSet<QString> m_declaredKeys;
    QSet<QString> m_sensitiveKeys;
    SensitiveDataSettings m_settings;
};

} // namespace account_recommendations::domain
