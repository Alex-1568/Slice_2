#pragma once

#include <QList>
#include <QString>

namespace account_recommendations::common
{

enum class Severity
{
    Info,
    Warning,
    Error
};

struct ValidationIssue
{
    QString field;   ///< configuration key the issue refers to (may be empty)
    QString code;    ///< stable machine-readable code, e.g. "out_of_range"
    QString message; ///< human-readable explanation suitable for the GUI
    Severity severity = Severity::Error;
};

/// Accumulates structured validation issues. Valid means "no Error-severity issues";
/// warnings and infos do not invalidate a configuration.
class ValidationResult
{
public:
    void add(ValidationIssue issue);
    void addError(QString field, QString code, QString message);
    void addWarning(QString field, QString code, QString message);
    void merge(const ValidationResult& other);

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] bool hasWarnings() const noexcept;
    [[nodiscard]] bool contains(const QString& field, const QString& code) const;
    [[nodiscard]] bool containsCode(const QString& code) const;
    [[nodiscard]] const QList<ValidationIssue>& issues() const noexcept { return m_issues; }

    /// All error messages joined with newlines.
    [[nodiscard]] QString summary() const;

private:
    QList<ValidationIssue> m_issues;
};

} // namespace account_recommendations::common
