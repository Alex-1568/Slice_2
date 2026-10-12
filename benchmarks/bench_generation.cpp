// Core-only micro-benchmarks (no GUI). Build with the dev-release preset:
//   cmake --preset dev-release && cmake --build --preset dev-release
//   ./build/dev-release/benchmarks/ar_bench_generation
//
// Numbers depend on hardware and compiler; they are measurements, not targets.
#include "domain/generator/GenerationContext.hpp"
#include "generators/password/PasswordGenerator.hpp"
#include "testing/DeterministicRandomProvider.hpp" // benchmark-only (test mode)
#include "testing/InsecureGenerationContext.hpp"
#include "infrastructure/random/SecureRandomProvider.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace account_recommendations;
using Clock = std::chrono::steady_clock;

namespace
{

/// Discards records but touches them so the work cannot be optimised away.
class DiscardingSink final : public domain::IResultSink
{
public:
    domain::SinkResponse accept(domain::GeneratedRecord&& record) override
    {
        m_checksum += static_cast<std::size_t>(record.value(QStringLiteral("password")).toString().size());
        return domain::SinkResponse::Continue;
    }
    std::size_t checksum() const noexcept { return m_checksum; }

private:
    std::size_t m_checksum = 0;
};

struct Scenario
{
    const char* name;
    QJsonObject parameters;
    bool unique;
};

void benchPasswords(const char* providerName, domain::IRandomProvider& provider, bool testMode)
{
    const std::vector<Scenario> scenarios = {
        {"default (24, all sets)", {}, false},
        {"min 2 per category (24)", {{"minimumUppercase", 2}, {"minimumLowercase", 2}, {"minimumDigits", 2}, {"minimumSymbols", 2}}, false},
        {"no duplicates (24)", {{"allowDuplicates", false}}, false},
        {"long (128)", {{"length", 128}}, false},
        {"unique, digits only (8)", {{"uppercase", false}, {"lowercase", false}, {"symbols", false}, {"length", 8}}, true},
    };
    const generators::password::PasswordGenerator generator;
    const domain::GenerationContext context =
        testMode ? testing::insecureContext(provider) : domain::GenerationContext(provider);

    std::printf("\n[%s]\n%-28s %8s %12s %14s\n", providerName, "scenario", "count", "time (ms)", "passwords/s");
    for (const Scenario& scenario : scenarios) {
        for (qsizetype count : {100, 1000, 10000, 100000}) {
            domain::GeneratorRequest request;
            request.generatorId = QStringLiteral("password");
            request.parameters = scenario.parameters;
            request.count = count;
            request.unique = scenario.unique;

            DiscardingSink sink;
            const auto start = Clock::now();
            const auto summary = generator.generateInto(request, context, sink);
            const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            if (!summary) {
                std::printf("%-28s %8lld  FAILED: %s\n", scenario.name, static_cast<long long>(count),
                            summary.error().message.toStdString().c_str());
                continue;
            }
            std::printf("%-28s %8lld %12.2f %14.0f\n", scenario.name, static_cast<long long>(count), ms,
                        static_cast<double>(count) / (ms / 1000.0));
            if (sink.checksum() == 0)
                std::printf("unexpected empty output\n");
        }
    }
}

void benchUniformIndex(const char* name, domain::IRandomProvider& provider)
{
    constexpr std::size_t kCalls = 20'000'000;
    std::size_t sink = 0;
    const auto start = Clock::now();
    for (std::size_t i = 0; i < kCalls; ++i)
        sink += provider.uniformIndex(87);
    const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    std::printf("%-24s %10.1f M calls/s   (checksum %zu)\n", name,
                static_cast<double>(kCalls) / (ms * 1000.0), sink);
}

} // namespace

int main()
{
    infrastructure::SecureRandomProvider secure;
    testing::DeterministicRandomProvider deterministic(42);

    std::printf("uniformIndex(87) throughput\n");
    benchUniformIndex("SecureRandomProvider", secure);
    benchUniformIndex("DeterministicRandomProvider", deterministic);

    benchPasswords("SecureRandomProvider (production path)", secure, false);
    benchPasswords("DeterministicRandomProvider (test mode)", deterministic, true);
    return 0;
}
