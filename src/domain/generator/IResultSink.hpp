#pragma once

#include "domain/generator/GeneratorResult.hpp"

#include <algorithm>
#include <utility>

namespace account_recommendations::domain
{

enum class SinkResponse
{
    Continue,
    Stop
};

/// Receives records as they are produced. Streaming consumers (table model
/// batches, streaming exporters) implement this instead of waiting for a full list.
class IResultSink
{
public:
    virtual ~IResultSink() = default;

    /// Called once per record, on the generating thread. Returning Stop means the current
    /// record was submitted to the sink (and counts toward GenerationSummary::produced), but
    /// requests that the generator emit no more records. Implementations decide whether to
    /// retain that final record. The framework boundary rejects generators that emit again
    /// after Stop or whose summary count does not match submitted records.
    [[nodiscard]] virtual SinkResponse accept(GeneratedRecord&& record) = 0;

protected:
    IResultSink() = default;
    IResultSink(const IResultSink&) = default;
    IResultSink& operator=(const IResultSink&) = default;
};

/// Simple in-memory sink backing the single-result convenience API.
class CollectingSink final : public IResultSink
{
public:
    void reserve(qsizetype count) { m_records.reserve(std::max<qsizetype>(count, 0)); }

    [[nodiscard]] SinkResponse accept(GeneratedRecord&& record) override
    {
        m_records.append(std::move(record));
        return SinkResponse::Continue;
    }

    [[nodiscard]] QList<GeneratedRecord> take() { return std::exchange(m_records, {}); }
    [[nodiscard]] qsizetype size() const noexcept { return m_records.size(); }

private:
    QList<GeneratedRecord> m_records;
};

} // namespace account_recommendations::domain
