#include "generators/identity/data/NameDataProvider.hpp"

#include <utility>

namespace account_recommendations::generators::identity
{

QStringList builtInNameLocales()
{
    return {QStringLiteral("en-US"), QStringLiteral("en-GB"), QStringLiteral("id-ID"),
            QStringLiteral("neutral")};
}

QStringList BuiltinNameDataProvider::supportedLocales() const
{
    return builtInNameLocales();
}

std::optional<NameDataset> BuiltinNameDataProvider::dataset(const QString& locale) const
{
    if (locale == QLatin1String("en-US")) {
        return NameDataset{
            QStringLiteral("en-US"), QStringLiteral("en"), QStringLiteral("US"),
            {QStringLiteral("Alex"), QStringLiteral("Jordan"), QStringLiteral("Taylor"),
             QStringLiteral("Morgan"), QStringLiteral("Casey"), QStringLiteral("Riley"),
             QStringLiteral("Avery"), QStringLiteral("Jamie"), QStringLiteral("Cameron"),
             QStringLiteral("Quinn"), QStringLiteral("Robin"), QStringLiteral("Drew"),
             QStringLiteral("Harper"), QStringLiteral("Logan"), QStringLiteral("Parker"),
             QStringLiteral("Reese"), QStringLiteral("Skyler"), QStringLiteral("Rowan"),
             QStringLiteral("Emerson"), QStringLiteral("Finley"), QStringLiteral("Sage"),
             QStringLiteral("Ellis"), QStringLiteral("Arin"), QStringLiteral("Kai")},
            {QStringLiteral("Miller"), QStringLiteral("Wilson"), QStringLiteral("Taylor"),
             QStringLiteral("Anderson"), QStringLiteral("Thomas"), QStringLiteral("Jackson"),
             QStringLiteral("White"), QStringLiteral("Harris"), QStringLiteral("Martin"),
             QStringLiteral("Thompson"), QStringLiteral("Garcia"), QStringLiteral("Clark"),
             QStringLiteral("Lewis"), QStringLiteral("Walker"), QStringLiteral("Hall"),
             QStringLiteral("Young"), QStringLiteral("King"), QStringLiteral("Wright"),
             QStringLiteral("Green"), QStringLiteral("Baker"), QStringLiteral("Reed"),
             QStringLiteral("Brooks")},
            {QStringLiteral("Alex"), QStringLiteral("Sam"), QStringLiteral("Riley"),
             QStringLiteral("Jay"), QStringLiteral("Ash"), QStringLiteral("Kit"),
             QStringLiteral("Remy"), QStringLiteral("Ari"), QStringLiteral("Rin"),
             QStringLiteral("Mika"), QStringLiteral("Lee"), QStringLiteral("Max"),
             QStringLiteral("Robin"), QStringLiteral("Ren"), QStringLiteral("Kai"),
             QStringLiteral("Sunny"), QStringLiteral("Blue"), QStringLiteral("River")}};
    }
    if (locale == QLatin1String("en-GB")) {
        return NameDataset{
            QStringLiteral("en-GB"), QStringLiteral("en"), QStringLiteral("GB"),
            {QStringLiteral("Alex"), QStringLiteral("Charlie"), QStringLiteral("Freddie"),
             QStringLiteral("Jamie"), QStringLiteral("Morgan"), QStringLiteral("Rory"),
             QStringLiteral("Rowan"), QStringLiteral("Ellis"), QStringLiteral("Avery"),
             QStringLiteral("Finley"), QStringLiteral("Harper"), QStringLiteral("Robin"),
             QStringLiteral("Eden"), QStringLiteral("Sasha"), QStringLiteral("Taylor"),
             QStringLiteral("Reese"), QStringLiteral("Dylan"), QStringLiteral("Jules"),
             QStringLiteral("Arin"), QStringLiteral("Kai")},
            {QStringLiteral("Smith"), QStringLiteral("Jones"), QStringLiteral("Williams"),
             QStringLiteral("Brown"), QStringLiteral("Wilson"), QStringLiteral("Davies"),
             QStringLiteral("Evans"), QStringLiteral("Thomas"), QStringLiteral("Roberts"),
             QStringLiteral("Walker"), QStringLiteral("Wright"), QStringLiteral("Thompson"),
             QStringLiteral("White"), QStringLiteral("Hughes"), QStringLiteral("Edwards"),
             QStringLiteral("Green"), QStringLiteral("Lewis"), QStringLiteral("Clarke"),
             QStringLiteral("Harris"), QStringLiteral("Wood")},
            {QStringLiteral("Al"), QStringLiteral("Charlie"), QStringLiteral("Freddie"),
             QStringLiteral("Jamie"), QStringLiteral("Rory"), QStringLiteral("Ellis"),
             QStringLiteral("Kit"), QStringLiteral("Ash"), QStringLiteral("Jules"),
             QStringLiteral("Remy"), QStringLiteral("Ari"), QStringLiteral("Ren"),
             QStringLiteral("Robin"), QStringLiteral("Sam"), QStringLiteral("Kai")}};
    }
    if (locale == QLatin1String("id-ID")) {
        return NameDataset{
            QStringLiteral("id-ID"), QStringLiteral("id"), QStringLiteral("ID"),
            {QStringLiteral("Ayu"), QStringLiteral("Bima"), QStringLiteral("Citra"),
             QStringLiteral("Dewi"), QStringLiteral("Dimas"), QStringLiteral("Intan"),
             QStringLiteral("Jaya"), QStringLiteral("Kartika"), QStringLiteral("Laras"),
             QStringLiteral("Mira"), QStringLiteral("Nadia"), QStringLiteral("Putri"),
             QStringLiteral("Raka"), QStringLiteral("Rani"), QStringLiteral("Rizky"),
             QStringLiteral("Sari"), QStringLiteral("Tara"), QStringLiteral("Wira"),
             QStringLiteral("Yuda"), QStringLiteral("Zahra"), QStringLiteral("Arin"),
             QStringLiteral("Kai")},
            {QStringLiteral("Pratama"), QStringLiteral("Wijaya"), QStringLiteral("Santoso"),
             QStringLiteral("Saputra"), QStringLiteral("Permata"), QStringLiteral("Kusuma"),
             QStringLiteral("Hidayat"), QStringLiteral("Putra"), QStringLiteral("Putri"),
             QStringLiteral("Wibowo"), QStringLiteral("Setiawan"), QStringLiteral("Nugraha"),
             QStringLiteral("Utami"), QStringLiteral("Kurniawan"), QStringLiteral("Maulana"),
             QStringLiteral("Ananda"), QStringLiteral("Ramadhan"), QStringLiteral("Pangestu")},
            {QStringLiteral("Ayu"), QStringLiteral("Bim"), QStringLiteral("Cici"),
             QStringLiteral("Dew"), QStringLiteral("Dima"), QStringLiteral("Intan"),
             QStringLiteral("Jaya"), QStringLiteral("Lala"), QStringLiteral("Mimi"),
             QStringLiteral("Nana"), QStringLiteral("Raka"), QStringLiteral("Rara"),
             QStringLiteral("Rin"), QStringLiteral("Tara"), QStringLiteral("Wira"),
             QStringLiteral("Yuyu"), QStringLiteral("Zara")}};
    }
    if (locale == QLatin1String("neutral")) {
        NameDataset result;
        const auto en = dataset(QStringLiteral("en-US"));
        const auto id = dataset(QStringLiteral("id-ID"));
        if (!en || !id)
            return std::nullopt;
        result.locale = QStringLiteral("neutral");
        result.language = QStringLiteral("und");
        result.countryCode = QStringLiteral("ZZ");
        result.firstNames = en->firstNames;
        result.firstNames.append(id->firstNames);
        result.lastNames = en->lastNames;
        result.lastNames.append(id->lastNames);
        result.nicknames = en->nicknames;
        result.nicknames.append(id->nicknames);
        return result;
    }
    return std::nullopt;
}


bool isUsableNameDataset(const NameDataset& dataset)
{
    if (dataset.locale.trimmed().isEmpty() || dataset.language.trimmed().isEmpty()
        || dataset.countryCode.trimmed().isEmpty() || dataset.firstNames.isEmpty()
        || dataset.lastNames.isEmpty() || dataset.nicknames.isEmpty())
        return false;
    const auto validList = [](const QStringList& values) {
        for (const QString& value : values) {
            if (value.trimmed().isEmpty() || value.size() > 128)
                return false;
            for (qsizetype index = 0; index < value.size(); ++index) {
                const QChar character = value.at(index);
                // Valid UTF-16 surrogate pairs are legitimate non-BMP characters. Reject
                // only unpaired surrogates, rather than every supplementary code point.
                if (character.isHighSurrogate()) {
                    if (index + 1 >= value.size() || !value.at(index + 1).isLowSurrogate())
                        return false;
                    ++index;
                    continue;
                }
                if (character.isLowSurrogate())
                    return false;
                const QChar::Category category = character.category();
                if (character.isNull() || category == QChar::Other_Control
                    || category == QChar::Other_Format)
                    return false;
            }
        }
        return true;
    };
    return validList(dataset.firstNames) && validList(dataset.lastNames)
        && validList(dataset.nicknames);
}

std::shared_ptr<const INameDataProvider> defaultNameDataProvider()
{
    static const auto provider = std::make_shared<const BuiltinNameDataProvider>();
    return provider;
}

} // namespace account_recommendations::generators::identity
