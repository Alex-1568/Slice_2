// Plain executable (no Qt Test) exercising the concurrency contracts of the project.
// Run it under ThreadSanitizer (preset "tsan"): because it avoids Qt Test's
// uninstrumented watchdog thread, any report here points at project or Qt Core code.
// Exit code 0 == all checks passed.
#include "domain/generator/CancellationToken.hpp"
#include "domain/generator/GeneratorRegistry.hpp"
#include "generators/password/PasswordGenerator.hpp"
#include "infrastructure/random/SecureRandomProvider.hpp"
#include "TestSupport.hpp"

#include <QJsonObject>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

using namespace account_recommendations;

namespace
{
std::atomic<int> g_failures{0};

void check(bool condition, const char* what)
{
    if (!condition) {
        ++g_failures;
        std::fprintf(stderr, "CHECK FAILED: %s\n", what);
    }
}

domain::GeneratorRequest request(QJsonObject params, qsizetype count)
{
    domain::GeneratorRequest r;
    r.generatorId = QStringLiteral("password");
    r.parameters = std::move(params);
    r.count = count;
    return r;
}

void sharedGeneratorAcrossThreads()
{
    const generators::password::PasswordGenerator generator;
    infrastructure::SecureRandomProvider secure;
    std::vector<std::thread> workers;
    std::atomic<int> produced{0};
    for (int t = 0; t < 4; ++t) {
        workers.emplace_back([&] {
            const domain::GenerationContext context(secure);
            const auto result = generator.generate(request({{"length", 16}, {"minimumDigits", 2}}, 1500), context);
            check(result.hasValue(), "shared generator: generation succeeded");
            if (result.hasValue())
                produced += static_cast<int>(result.value().records.size());
        });
    }
    for (auto& w : workers)
        w.join();
    check(produced.load() == 4 * 1500, "shared generator: record count");
}

void registryReadersDuringRegistration()
{
    domain::GeneratorRegistry registry;
    check(registry.registerGenerator(std::make_unique<generators::password::PasswordGenerator>()).hasValue(), "registry: base registered");

    std::atomic<bool> stop{false};
    std::vector<std::thread> readers;
    for (int t = 0; t < 3; ++t) {
        readers.emplace_back([&] {
            while (!stop.load()) {
                const auto found = registry.find(QStringLiteral("password"));
                check(found != nullptr && found->metadata().id == QStringLiteral("password"), "registry: reader sees base generator");
                check(!registry.list().isEmpty(), "registry: list not empty");
            }
        });
    }
    // The writer adds and removes entries (the base generator stays) while readers run.
    for (int i = 0; i < 150; ++i) {
        const QString id = QStringLiteral("churn-%1").arg(i);
        check(registry.registerGenerator(std::make_unique<testing::StubGenerator>(id)).hasValue(), "registry: churn register");
        if (i % 2 == 0)
            check(registry.unregisterGenerator(id), "registry: churn unregister");
    }
    check(registry.size() == 1 + 75, "registry: final size");
    stop = true;
    for (auto& r : readers)
        r.join();
}

void cancellationFromAnotherThread()
{
    const generators::password::PasswordGenerator generator;
    infrastructure::SecureRandomProvider secure;
    domain::CancellationSource source;
    const domain::GenerationContext context(secure, source.token());

    std::thread canceller([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        source.requestCancel();
    });
    const auto start = std::chrono::steady_clock::now();
    // 1 000 000 passwords would take many seconds; cancellation must cut it short.
    const auto result = generator.generate(request({{"length", 64}}, 1'000'000), context);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    canceller.join();

    check(result.hasValue(), "cancel: result is a value, not an error");
    if (result.hasValue()) {
        check(result.value().summary.status == domain::CompletionStatus::Cancelled, "cancel: status Cancelled");
        check(result.value().summary.produced < 1'000'000, "cancel: stopped before completion");
    }
    check(elapsed < std::chrono::seconds(20), "cancel: terminated promptly");
}

void tokenIsSharedAndSourcesAreIndependent()
{
    domain::CancellationSource a;
    domain::CancellationSource b;
    const auto tokenA = a.token();
    const auto copyOfTokenA = tokenA;
    check(!tokenA.isCancelled() && !copyOfTokenA.isCancelled(), "token: initially clear");
    a.requestCancel();
    check(tokenA.isCancelled() && copyOfTokenA.isCancelled(), "token: copies observe cancellation");
    check(!b.token().isCancelled(), "token: independent source unaffected (no global flag)");
    check(!domain::CancellationToken().isCancelled(), "token: default token never cancels");
}
} // namespace

int main()
{
    sharedGeneratorAcrossThreads();
    registryReadersDuringRegistration();
    cancellationFromAnotherThread();
    tokenIsSharedAndSourcesAreIndependent();
    if (g_failures.load() == 0)
        std::puts("concurrency_check: all checks passed");
    return g_failures.load() == 0 ? 0 : 1;
}
