#include "gui/ExportFormatting.hpp"

#include "domain/security/SensitiveDataPolicy.hpp"

#include <utility>

namespace account_recommendations::gui::export_format
{
namespace
{
bool isLeadingIgnorable(QChar character) noexcept
{
    return character.isSpace() || character == QChar(0xFEFF) || character == QChar(0x200B)
           || character == QChar(0x200E) || character == QChar(0x200F);
}
} // namespace

QString scalarText(const QJsonValue& value)
{
    if (value.isString())
        return value.toString();
    if (value.isBool())
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (value.isNull())
        return QStringLiteral("null");
    if (value.isDouble())
        return QString::number(value.toDouble(), 'g', 15);
    return domain::SensitiveDataPolicy::redactionText();
}

QString protectSpreadsheetFormula(QString value)
{
    // Spreadsheet programs commonly skip leading whitespace before deciding whether a cell
    // starts with a formula. Tab/CR/LF are treated as hazardous controls even at column 0.
    // Prefixing an apostrophe is a compatibility-oriented mitigation, not a universal guarantee
    // across every spreadsheet/import tool; documentation warns users about external handling.
    bool dangerous = false;
    for (const QChar character : value) {
        if (character == QLatin1Char('\t') || character == QLatin1Char('\r')
            || character == QLatin1Char('\n')) {
            dangerous = true;
            break;
        }
        if (isLeadingIgnorable(character))
            continue;
        dangerous = character == QLatin1Char('=') || character == QLatin1Char('+')
                    || character == QLatin1Char('-') || character == QLatin1Char('@');
        break;
    }
    if (dangerous && !value.startsWith(QLatin1Char('\'')))
        value.prepend(QLatin1Char('\''));
    return value;
}

QString escapeCsvText(QString value)
{
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    if (value.contains(QLatin1Char(',')) || value.contains(QLatin1Char('"'))
        || value.contains(QLatin1Char('\n')) || value.contains(QLatin1Char('\r')))
        value = QLatin1Char('"') + value + QLatin1Char('"');
    return value;
}

QString csvCell(const QJsonValue& value)
{
    QString text = scalarText(value);
    if (value.isString())
        text = protectSpreadsheetFormula(std::move(text));
    return escapeCsvText(std::move(text));
}

QString tsvText(QString value)
{
    value = protectSpreadsheetFormula(std::move(value));
    value.replace(QLatin1Char('\t'), QLatin1Char(' '));
    value.replace(QLatin1Char('\r'), QLatin1Char(' '));
    value.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return value;
}

QString tsvCell(const QJsonValue& value)
{
    QString text = scalarText(value);
    if (value.isString())
        return tsvText(std::move(text));
    text.replace(QLatin1Char('\t'), QLatin1Char(' '));
    text.replace(QLatin1Char('\r'), QLatin1Char(' '));
    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return text;
}

} // namespace account_recommendations::gui::export_format
