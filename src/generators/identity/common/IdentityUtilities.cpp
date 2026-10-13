#include "generators/identity/common/IdentityUtilities.hpp"

#include <QSet>

#include <array>
#include <cstdint>

namespace account_recommendations::generators::identity
{

QString choose(const QStringList& values, domain::IRandomProvider& random)
{
    if (values.isEmpty())
        return {};
    return values.at(static_cast<qsizetype>(random.uniformIndex(static_cast<std::size_t>(values.size()))));
}

QString randomDigits(domain::IRandomProvider& random, qsizetype length)
{
    QString result;
    result.reserve(length);
    for (qsizetype i = 0; i < length; ++i) {
        const auto digit = random.uniformIndex(10U);
        result.append(QChar(static_cast<char16_t>(u'0' + static_cast<char16_t>(digit))));
    }
    return result;
}

QString randomFromAlphabet(domain::IRandomProvider& random, const QString& alphabet, qsizetype length)
{
    if (alphabet.isEmpty() || length < 0)
        return {};
    QString result;
    result.reserve(length);
    for (qsizetype i = 0; i < length; ++i) {
        const auto index = random.uniformIndex(static_cast<std::size_t>(alphabet.size()));
        result.append(alphabet.at(static_cast<qsizetype>(index)));
    }
    return result;
}

QString randomUuidV4(domain::IRandomProvider& random)
{
    std::array<std::uint8_t, 16> bytes{};
    for (std::size_t wordIndex = 0; wordIndex < 2U; ++wordIndex) {
        const std::uint64_t word = random.nextUInt64();
        for (std::size_t byteIndex = 0; byteIndex < 8U; ++byteIndex) {
            const auto shift = static_cast<unsigned int>((7U - byteIndex) * 8U);
            bytes[wordIndex * 8U + byteIndex] = static_cast<std::uint8_t>((word >> shift) & 0xFFU);
        }
    }
    bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0FU) | 0x40U); // RFC 4122 version 4
    bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3FU) | 0x80U); // RFC variant

    static constexpr char kHex[] = "0123456789abcdef";
    QString result;
    result.reserve(36);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4U || i == 6U || i == 8U || i == 10U)
            result.append(QLatin1Char('-'));
        const std::uint8_t value = bytes[i];
        result.append(QLatin1Char(kHex[(value >> 4U) & 0x0FU]));
        result.append(QLatin1Char(kHex[value & 0x0FU]));
    }
    return result;
}

QStringList parseCustomWords(const QString& input, std::size_t maximumEntries)
{
    QStringList result;
    if (input.isEmpty() || maximumEntries == 0U)
        return result;

    const QStringList lines = input.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (const QString& rawLine : lines) {
        // Semicolon is the secondary delimiter so names containing commas remain intact.
        const QStringList entries = rawLine.split(QLatin1Char(';'), Qt::KeepEmptyParts);
        for (const QString& rawEntry : entries) {
            const QString entry = rawEntry.trimmed();
            if (entry.isEmpty())
                continue;
            if (static_cast<std::size_t>(result.size()) >= maximumEntries)
                return result;
            result.append(entry);
        }
    }
    return result;
}

QHash<QString, QString> nameTemplateValues(const NameDataset& dataset, const QString& first,
                                           const QString& last, const QString& nickname,
                                           const QString& number, const QString& separator)
{
    QHash<QString, QString> values;
    values.insert(QStringLiteral("first"), first);
    values.insert(QStringLiteral("last"), last);
    values.insert(QStringLiteral("nickname"), nickname);
    values.insert(QStringLiteral("number"), number);
    values.insert(QStringLiteral("separator"), separator);
    values.insert(QStringLiteral("locale"), dataset.locale);
    values.insert(QStringLiteral("language"), dataset.language);
    values.insert(QStringLiteral("countryCode"), dataset.countryCode);
    return values;
}

QString sanitizeUsername(QString value, const QString& allowedCharacters, const QString& blockedCharacters)
{
    const QString allowed = allowedCharacters.toCaseFolded();
    const QString blocked = blockedCharacters.toCaseFolded();
    value = value.toCaseFolded();
    QString cleaned;
    cleaned.reserve(value.size());
    for (const QChar character : value) {
        if (blocked.contains(character) || !allowed.contains(character))
            continue;
        cleaned.append(character);
    }
    while (!cleaned.isEmpty()) {
        const QChar first = cleaned.front();
        if (first == QLatin1Char('.') || first == QLatin1Char('_') || first == QLatin1Char('-'))
            cleaned.remove(0, 1);
        else
            break;
    }
    while (!cleaned.isEmpty()) {
        const QChar last = cleaned.back();
        if (last == QLatin1Char('.') || last == QLatin1Char('_') || last == QLatin1Char('-'))
            cleaned.chop(1);
        else
            break;
    }
    return cleaned;
}

QString sanitizeEmailLocalPart(QString value)
{
    value = value.toLower();
    QString cleaned;
    cleaned.reserve(value.size());
    bool previousDot = false;
    for (const QChar character : value) {
        const bool alphaNumeric = (character >= QLatin1Char('a') && character <= QLatin1Char('z'))
                               || (character >= QLatin1Char('0') && character <= QLatin1Char('9'));
        const bool allowedPunctuation = character == QLatin1Char('.') || character == QLatin1Char('_')
                                    || character == QLatin1Char('+') || character == QLatin1Char('-');
        if (!alphaNumeric && !allowedPunctuation)
            continue;
        if (character == QLatin1Char('.') && (cleaned.isEmpty() || previousDot))
            continue;
        cleaned.append(character);
        previousDot = character == QLatin1Char('.');
    }
    while (!cleaned.isEmpty() && cleaned.front() == QLatin1Char('.'))
        cleaned.remove(0, 1);
    while (!cleaned.isEmpty() && cleaned.back() == QLatin1Char('.'))
        cleaned.chop(1);
    if (cleaned.size() > 64)
        cleaned.truncate(64);
    return cleaned;
}

const QStringList& safeEmailDomains()
{
    static const QStringList domains{
        QStringLiteral("example.com"), QStringLiteral("example.net"),
        QStringLiteral("example.org"), QStringLiteral("example.test"),
        QStringLiteral("example.invalid"), QStringLiteral("demo.test"),
        QStringLiteral("sample.invalid")};
    return domains;
}

bool isSafeEmailDomain(const QString& domain)
{
    return safeEmailDomains().contains(domain.toLower());
}

bool isPrintableNonWhitespaceAscii(const QString& value)
{
    if (value.isEmpty())
        return false;
    for (const QChar character : value) {
        const ushort code = character.unicode();
        if (code < 0x21U || code > 0x7EU)
            return false;
    }
    return true;
}

bool hasDuplicateCharacters(const QString& value)
{
    QSet<QChar> seen;
    for (const QChar character : value) {
        if (seen.contains(character))
            return true;
        seen.insert(character);
    }
    return false;
}

} // namespace account_recommendations::generators::identity
