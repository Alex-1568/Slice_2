#pragma once

#include <QJsonObject>
#include <QList>

namespace account_recommendations::domain
{

/// One generated row, keyed by the generator's declared output field keys.
using GeneratedRecord = QJsonObject;

enum class CompletionStatus
{
    Completed,     ///< every requested record was produced
    Cancelled,     ///< cooperative cancellation was observed
    StoppedBySink  ///< the sink asked the generator to stop (e.g. a full buffer)
};

struct GenerationSummary
{
    qsizetype produced = 0;
    CompletionStatus status = CompletionStatus::Completed;
};

struct GeneratorResult
{
    QList<GeneratedRecord> records;
    GenerationSummary summary;
};

} // namespace account_recommendations::domain
