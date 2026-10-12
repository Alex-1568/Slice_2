#pragma once

#include <QJsonObject>
#include <QString>

namespace account_recommendations::domain
{

struct GeneratorRequest
{
    QString generatorId;
    QJsonObject parameters; ///< generator-specific, validated against its schema
    qsizetype count = 1;
    bool unique = false;    ///< no duplicate records within this batch
};

} // namespace account_recommendations::domain
