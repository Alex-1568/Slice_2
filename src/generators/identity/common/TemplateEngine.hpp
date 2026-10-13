#pragma once

#include "common/limits/ApplicationLimits.hpp"
#include "common/result/Result.hpp"
#include "common/validation/ValidationResult.hpp"

#include <QHash>
#include <QString>
#include <QStringList>

namespace account_recommendations::generators::identity
{

/// Deliberately small, non-evaluating template language:
/// - `{token}` inserts a value supplied by the caller;
/// - `{{` and `}}` escape literal braces;
/// - no conditionals, loops, file access, or code evaluation exist.
class TemplateEngine final
{
public:
    [[nodiscard]] static common::ValidationResult validate(
        const QString& pattern, const QStringList& allowedTokens,
        qsizetype maxPatternLength = 1024);

    /// True only for a syntactically valid, unescaped `{token}` occurrence.
    /// Escaped `{{token}}` text is not considered a token.
    [[nodiscard]] static bool containsToken(
        const QString& pattern, const QString& token, qsizetype maxPatternLength = 1024);

    [[nodiscard]] static common::Result<QString, common::ValidationResult> render(
        const QString& pattern, const QHash<QString, QString>& values,
        const common::ApplicationLimits& limits = common::ApplicationLimits::defaults());
};

} // namespace account_recommendations::generators::identity
