#include "common/validation/ValidationResult.hpp"

#include <QStringList>

#include <algorithm>
#include <utility>

namespace account_recommendations::common
{

void ValidationResult::add(ValidationIssue issue)
{
    m_issues.append(std::move(issue));
}

void ValidationResult::addError(QString field, QString code, QString message)
{
    add({std::move(field), std::move(code), std::move(message), Severity::Error});
}

void ValidationResult::addWarning(QString field, QString code, QString message)
{
    add({std::move(field), std::move(code), std::move(message), Severity::Warning});
}

void ValidationResult::merge(const ValidationResult& other)
{
    m_issues.append(other.m_issues);
}

bool ValidationResult::isValid() const noexcept
{
    return std::none_of(m_issues.cbegin(), m_issues.cend(), [](const ValidationIssue& issue) {
        return issue.severity == Severity::Error;
    });
}

bool ValidationResult::hasWarnings() const noexcept
{
    return std::any_of(m_issues.cbegin(), m_issues.cend(), [](const ValidationIssue& issue) {
        return issue.severity == Severity::Warning;
    });
}

bool ValidationResult::contains(const QString& field, const QString& code) const
{
    return std::any_of(m_issues.cbegin(), m_issues.cend(), [&](const ValidationIssue& issue) {
        return issue.field == field && issue.code == code;
    });
}

bool ValidationResult::containsCode(const QString& code) const
{
    return std::any_of(m_issues.cbegin(), m_issues.cend(),
                       [&](const ValidationIssue& issue) { return issue.code == code; });
}

QString ValidationResult::summary() const
{
    QStringList lines;
    for (const ValidationIssue& issue : m_issues) {
        if (issue.severity == Severity::Error)
            lines.append(issue.message);
    }
    return lines.join(QLatin1Char('\n'));
}

} // namespace account_recommendations::common
