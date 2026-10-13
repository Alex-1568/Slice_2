#include "generators/identity/common/TemplateEngine.hpp"

#include <QSet>
#include <algorithm>
#include <utility>

namespace account_recommendations::generators::identity
{
namespace
{
struct ParsedTemplate
{
    QStringList tokens;
    common::ValidationResult issues;
};

ParsedTemplate parsePattern(const QString& pattern, qsizetype maxPatternLength)
{
    ParsedTemplate parsed;
    if (maxPatternLength < 0 || pattern.size() > maxPatternLength) {
        parsed.issues.addError(QStringLiteral("template"), QStringLiteral("template_too_long"),
                               QStringLiteral("Template exceeds the maximum permitted length."));
        return parsed;
    }

    qsizetype index = 0;
    while (index < pattern.size()) {
        const QChar current = pattern.at(index);
        if (current == QLatin1Char('{')) {
            if (index + 1 < pattern.size() && pattern.at(index + 1) == QLatin1Char('{')) {
                index += 2; // escaped literal brace
                continue;
            }
            const qsizetype close = pattern.indexOf(QLatin1Char('}'), index + 1);
            if (close < 0) {
                parsed.issues.addError(QStringLiteral("template"), QStringLiteral("unclosed_token"),
                                       QStringLiteral("Template contains an opening brace without a closing brace."));
                return parsed;
            }
            const QString token = pattern.mid(index + 1, close - index - 1);
            bool validToken = !token.isEmpty();
            for (const QChar tokenChar : token) {
                const bool validChar = (tokenChar >= QLatin1Char('a') && tokenChar <= QLatin1Char('z'))
                    || (tokenChar >= QLatin1Char('A') && tokenChar <= QLatin1Char('Z'))
                    || (tokenChar >= QLatin1Char('0') && tokenChar <= QLatin1Char('9'))
                    || tokenChar == QLatin1Char('_');
                validToken = validToken && validChar;
            }
            if (!validToken) {
                parsed.issues.addError(QStringLiteral("template"), QStringLiteral("invalid_token"),
                                       QStringLiteral("Template token syntax is invalid."));
                return parsed;
            }
            parsed.tokens.append(token);
            index = close + 1;
            continue;
        }
        if (current == QLatin1Char('}')) {
            if (index + 1 < pattern.size() && pattern.at(index + 1) == QLatin1Char('}')) {
                index += 2; // escaped literal brace
                continue;
            }
            parsed.issues.addError(QStringLiteral("template"), QStringLiteral("unmatched_closing_brace"),
                                   QStringLiteral("Template contains an unmatched closing brace."));
            return parsed;
        }
        ++index;
    }
    return parsed;
}
} // namespace

bool TemplateEngine::containsToken(const QString& pattern, const QString& token, qsizetype maxPatternLength)
{
    if (token.isEmpty())
        return false;
    const ParsedTemplate parsed = parsePattern(pattern, maxPatternLength);
    return parsed.issues.isValid() && parsed.tokens.contains(token);
}

common::ValidationResult TemplateEngine::validate(const QString& pattern,
                                                  const QStringList& allowedTokens,
                                                  qsizetype maxPatternLength)
{
    ParsedTemplate parsed = parsePattern(pattern, maxPatternLength);
    if (!parsed.issues.isValid())
        return parsed.issues;

    QSet<QString> allowed;
    for (const QString& token : allowedTokens)
        allowed.insert(token);
    common::ValidationResult result;
    for (const QString& token : parsed.tokens) {
        if (!allowed.contains(token)) {
            result.addError(QStringLiteral("template"), QStringLiteral("unknown_template_token"),
                            QStringLiteral("Template contains an unsupported token: {%1}.")
                                .arg(token.left(32)));
        }
    }
    return result;
}

common::Result<QString, common::ValidationResult> TemplateEngine::render(
    const QString& pattern, const QHash<QString, QString>& values,
    const common::ApplicationLimits& limits)
{
    using R = common::Result<QString, common::ValidationResult>;
    const qsizetype templateLimit = static_cast<qsizetype>(std::min<std::size_t>(1024U, limits.maxStringLength));
    ParsedTemplate parsed = parsePattern(pattern, templateLimit);
    if (!parsed.issues.isValid())
        return R::failure(std::move(parsed.issues));

    for (const QString& token : parsed.tokens) {
        if (!values.contains(token)) {
            common::ValidationResult issues;
            issues.addError(QStringLiteral("template"), QStringLiteral("missing_template_value"),
                            QStringLiteral("No value was supplied for template token: {%1}.")
                                .arg(token.left(32)));
            return R::failure(std::move(issues));
        }
    }

    QString result;
    result.reserve(std::min<qsizetype>(pattern.size(), static_cast<qsizetype>(limits.maxOutputLength)));
    qsizetype index = 0;
    while (index < pattern.size()) {
        const QChar current = pattern.at(index);
        if (current == QLatin1Char('{') && index + 1 < pattern.size()
            && pattern.at(index + 1) == QLatin1Char('{')) {
            result.append(QLatin1Char('{'));
            index += 2;
        } else if (current == QLatin1Char('}') && index + 1 < pattern.size()
                   && pattern.at(index + 1) == QLatin1Char('}')) {
            result.append(QLatin1Char('}'));
            index += 2;
        } else if (current == QLatin1Char('{')) {
            const qsizetype close = pattern.indexOf(QLatin1Char('}'), index + 1);
            const QString token = pattern.mid(index + 1, close - index - 1);
            const QString value = values.value(token);
            if (value.size() > static_cast<qsizetype>(limits.maxOutputLength)
                || result.size() > static_cast<qsizetype>(limits.maxOutputLength) - value.size()) {
                common::ValidationResult issues;
                issues.addError(QStringLiteral("template"), QStringLiteral("output_too_long"),
                                QStringLiteral("Rendered template exceeds the configured output limit."));
                return R::failure(std::move(issues));
            }
            result.append(value);
            index = close + 1;
        } else {
            if (result.size() >= static_cast<qsizetype>(limits.maxOutputLength)) {
                common::ValidationResult issues;
                issues.addError(QStringLiteral("template"), QStringLiteral("output_too_long"),
                                QStringLiteral("Rendered template exceeds the configured output limit."));
                return R::failure(std::move(issues));
            }
            result.append(current);
            ++index;
        }
        if (result.size() > static_cast<qsizetype>(limits.maxOutputLength)) {
            common::ValidationResult issues;
            issues.addError(QStringLiteral("template"), QStringLiteral("output_too_long"),
                            QStringLiteral("Rendered template exceeds the configured output limit."));
            return R::failure(std::move(issues));
        }
    }
    return R::success(std::move(result));
}

} // namespace account_recommendations::generators::identity
