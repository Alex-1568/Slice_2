#pragma once

#include <QJsonValue>
#include <QString>

namespace account_recommendations::gui::export_format
{

/// Converts the flat-record scalar values used by GUI export to readable text.
[[nodiscard]] QString scalarText(const QJsonValue& value);

/// Escapes a string-valued cell before CSV/TSV export so spreadsheet applications are less
/// likely to evaluate user-controlled values as formulas. The protection is intentionally
/// applied only to string values (and all header strings); numeric JSON values remain numeric.
[[nodiscard]] QString protectSpreadsheetFormula(QString value);

[[nodiscard]] QString escapeCsvText(QString value);
[[nodiscard]] QString csvCell(const QJsonValue& value);
[[nodiscard]] QString tsvText(QString value);
[[nodiscard]] QString tsvCell(const QJsonValue& value);

} // namespace account_recommendations::gui::export_format
