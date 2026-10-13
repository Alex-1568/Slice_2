#pragma once

#include "domain/random/IRandomProvider.hpp"
#include "generators/identity/data/NameDataProvider.hpp"

#include <QHash>
#include <QString>
#include <QStringList>

#include <optional>

namespace account_recommendations::generators::identity
{

[[nodiscard]] QString choose(const QStringList& values, domain::IRandomProvider& random);
[[nodiscard]] QString randomDigits(domain::IRandomProvider& random, qsizetype length);
[[nodiscard]] QString randomFromAlphabet(domain::IRandomProvider& random, const QString& alphabet,
                                         qsizetype length);
[[nodiscard]] QString randomUuidV4(domain::IRandomProvider& random);
[[nodiscard]] QStringList parseCustomWords(const QString& input, std::size_t maximumEntries);
[[nodiscard]] QHash<QString, QString> nameTemplateValues(const NameDataset& dataset,
                                                         const QString& first,
                                                         const QString& last,
                                                         const QString& nickname,
                                                         const QString& number,
                                                         const QString& separator);
[[nodiscard]] QString sanitizeUsername(QString value, const QString& allowedCharacters,
                                       const QString& blockedCharacters);
[[nodiscard]] QString sanitizeEmailLocalPart(QString value);
[[nodiscard]] const QStringList& safeEmailDomains();
[[nodiscard]] bool isSafeEmailDomain(const QString& domain);
[[nodiscard]] bool isPrintableNonWhitespaceAscii(const QString& value);
[[nodiscard]] bool hasDuplicateCharacters(const QString& value);

} // namespace account_recommendations::generators::identity
