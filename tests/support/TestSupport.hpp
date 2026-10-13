#pragma once

#include "domain/generator/IGenerator.hpp"
#include "domain/random/IRandomProvider.hpp"
#include "infrastructure/random/ISystemEntropy.hpp"
#include "testing/InjectedEntropyProvider.hpp"
#include "testing/InsecureGenerationContext.hpp"

#include <QJsonObject>

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

// Test-only doubles. None of these may ever be linked into production targets.
namespace account_recommendations::testing
{

/// Replays a fixed list of 64-bit words, then throws RandomSourceError.
class ScriptedRandomProvider final : public domain::IRandomProvider
{
public:
    explicit ScriptedRandomProvider(std::vector<std::uint64_t> words) : m_words(std::move(words)) {}

    std::uint64_t nextUInt64() override
    {
        if (m_next >= m_words.size())
            throw domain::RandomSourceError("script exhausted");
        return m_words[m_next++];
    }
    std::size_t consumed() const noexcept { return m_next; }

private:
    std::vector<std::uint64_t> m_words;
    std::size_t m_next = 0;
};

class ConstantRandomProvider final : public domain::IRandomProvider
{
public:
    explicit ConstantRandomProvider(std::uint64_t word) : m_word(word) {}
    std::uint64_t nextUInt64() override { return m_word; }

private:
    std::uint64_t m_word;
};

/// Always fails: proves that a failing source yields an error and never a fallback-generated
/// value. (It cannot be attested as secure; use testing::insecureContext() with it.)
class FailingRandomProvider final : public domain::IRandomProvider
{
public:
    std::uint64_t nextUInt64() override
    {
        ++m_calls;
        throw domain::RandomSourceError("simulated entropy failure");
    }
    std::size_t calls() const noexcept { return m_calls; }

private:
    std::size_t m_calls = 0;
};

/// Forwards to another provider while counting draws.
class CountingRandomProvider final : public domain::IRandomProvider
{
public:
    explicit CountingRandomProvider(domain::IRandomProvider& inner) : m_inner(inner) {}
    std::uint64_t nextUInt64() override
    {
        ++m_calls;
        return m_inner.nextUInt64();
    }
    std::size_t calls() const noexcept { return m_calls; }

private:
    domain::IRandomProvider& m_inner;
    std::size_t m_calls = 0;
};

/// Entropy-source double for fault injection. Succeeds `failAfter` times (writing a
/// non-constant byte pattern), then fails forever with `errorCode`.
class FaultInjectingEntropy final : public infrastructure::ISystemEntropy
{
public:
    FaultInjectingEntropy(std::size_t failAfter, int errorCode = 5)
        : m_failAfter(failAfter), m_errorCode(errorCode)
    {
    }

    common::Result<void, infrastructure::EntropyError> fill(std::byte* data, std::size_t size) override
    {
        ++m_calls;
        if (m_calls > m_failAfter)
            return common::Result<void, infrastructure::EntropyError>::failure(
                {m_errorCode, QStringLiteral("injected entropy failure")});
        for (std::size_t i = 0; i < size; ++i)
            data[i] = static_cast<std::byte>((m_calls * 31U + i * 7U) & 0xFFU);
        return common::Result<void, infrastructure::EntropyError>::success();
    }
    QString backendName() const override { return QStringLiteral("fault-injecting test double"); }
    std::size_t calls() const noexcept { return m_calls; }

private:
    std::size_t m_failAfter;
    int m_errorCode;
    std::size_t m_calls = 0;
};

class LambdaSink final : public domain::IResultSink
{
public:
    using Handler = std::function<domain::SinkResponse(domain::GeneratedRecord&&)>;
    explicit LambdaSink(Handler handler) : m_handler(std::move(handler)) {}
    domain::SinkResponse accept(domain::GeneratedRecord&& record) override
    {
        return m_handler(std::move(record));
    }

private:
    Handler m_handler;
};

/// Minimal generator used to exercise the registry and the sensitive-data policy.
class StubGenerator final : public domain::IGenerator
{
public:
    static domain::GeneratorMetadata makeMetadata(const QString& id, bool sensitive = false,
                                                  bool supportsUnique = false)
    {
        domain::GeneratorMetadata md;
        md.id = id;
        md.displayName = QStringLiteral("Stub ") + id;
        md.description = QStringLiteral("Test generator");
        md.category = QStringLiteral("Test");
        md.version = QStringLiteral("1.0.0");
        md.supportsUniqueness = supportsUnique;
        md.configurationSchema = domain::ConfigurationSchema(
            {domain::FieldSpec::integer(QStringLiteral("size"), QStringLiteral("Size"), 1, 1, 10)});
        md.outputFields = {{QStringLiteral("value"), QStringLiteral("Value"), sensitive}};
        return md;
    }

    explicit StubGenerator(domain::GeneratorMetadata metadata) : m_metadata(std::move(metadata)) {}
    explicit StubGenerator(const QString& id) : m_metadata(makeMetadata(id)) {}

    domain::GeneratorMetadata metadata() const override { return m_metadata; }
    common::ValidationResult validate(const domain::GeneratorRequest&) const override { return {}; }

    common::Result<domain::GenerationSummary, domain::GenerationError>
    generateIntoImpl(const domain::GeneratorRequest& request, const domain::GenerationContext& context,
                     domain::IResultSink& sink) const override
    {
        (void)context; // Preconditions are enforced by IGenerator's public final boundary.
        using R = common::Result<domain::GenerationSummary, domain::GenerationError>;
        domain::GenerationSummary summary;
        for (qsizetype i = 0; i < request.count; ++i) {
            QJsonObject record;
            record.insert(QStringLiteral("value"), static_cast<double>(i));
            ++summary.produced;
            if (sink.accept(std::move(record)) == domain::SinkResponse::Stop)
                break;
        }
        return R::success(summary);
    }

private:
    domain::GeneratorMetadata m_metadata;
};

inline domain::GeneratorRequest passwordRequest(QJsonObject parameters, qsizetype count = 1,
                                                bool unique = false)
{
    domain::GeneratorRequest request;
    request.generatorId = QStringLiteral("password");
    request.parameters = std::move(parameters);
    request.count = count;
    request.unique = unique;
    return request;
}

} // namespace account_recommendations::testing
