// Developer tool: proves the Registry -> IGenerator -> IRandomProvider chain works
// without any GUI. This file is also the reference "composition root": the only
// place where concrete classes are wired together.
//
//   ar_cli_demo [--count N] [--length L] [--reveal]
#include "domain/generator/GenerationContext.hpp"
#include "domain/generator/GeneratorRegistry.hpp"
#include "domain/security/SensitiveDataPolicy.hpp"
#include "generators/password/PasswordGenerator.hpp"
#include "generators/identity/registerIdentityGenerators.hpp"
#include "infrastructure/random/SecureRandomProvider.hpp"

#include <QJsonObject>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

using namespace account_recommendations;

int main(int argc, char** argv)
{
    qsizetype count = 3;
    int length = 24;
    bool reveal = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--count") == 0 && i + 1 < argc)
            count = std::atoll(argv[++i]);
        else if (std::strcmp(argv[i], "--length") == 0 && i + 1 < argc)
            length = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--reveal") == 0)
            reveal = true;
        else {
            std::fprintf(stderr, "usage: ar_cli_demo [--count N] [--length L] [--reveal]\n");
            return 2;
        }
    }

    // ---- composition root ----
    domain::GeneratorRegistry registry;
    infrastructure::SecureRandomProvider secureRandom;
    // Fail closed: refuse to run at all if the operating-system CSPRNG cannot be used.
    if (const auto health = secureRandom.probe(); !health) {
        std::fprintf(stderr, "the operating-system random source is unavailable: %s\n",
                     health.error().description.toStdString().c_str());
        return 1;
    }
    if (auto registered = registry.registerGenerator(std::make_unique<generators::password::PasswordGenerator>());
        !registered) {
        std::fprintf(stderr, "registration failed: %s\n", registered.error().message.toStdString().c_str());
        return 1;
    }

    if (auto registered = generators::identity::registerIdentityGenerators(registry); !registered) {
        std::fprintf(stderr, "identity generator registration failed: %s\n",
                     registered.error().message.toStdString().c_str());
        return 1;
    }

    for (const auto& metadata : registry.list())
        std::printf("generator: %s  [%s]  v%s\n", metadata.id.toStdString().c_str(),
                    metadata.category.toStdString().c_str(), metadata.version.toStdString().c_str());

    // ---- use case: generate through the registry only ----
    const auto generator = registry.find(QStringLiteral("password"));
    domain::GeneratorRequest request;
    request.generatorId = QStringLiteral("password");
    request.parameters = QJsonObject{{"length", length}, {"minimumDigits", 2}, {"minimumSymbols", 2}};
    request.count = count;

    const domain::GenerationContext context(secureRandom);
    const auto result = generator->generate(request, context);
    if (!result) {
        std::fprintf(stderr, "generation failed: %s\n", result.error().message.toStdString().c_str());
        return 1;
    }

    // ---- presentation goes through the sensitive-data policy ----
    domain::SensitiveDataSettings settings;
    settings.revealOnDisplay = reveal;
    const auto policy = domain::SensitiveDataPolicy::forGenerator(generator->metadata(), settings);
    for (const auto& record : result.value().records) {
        const QJsonObject shown = policy.sanitize(record, domain::OutputChannel::Display);
        // Numbers describe the configuration, not this particular password.
        std::printf("%s   config guarantees >= %.1f bits (%s)\n",
                    shown.value(QStringLiteral("password")).toString().toStdString().c_str(),
                    shown.value(QStringLiteral("guaranteedEntropyBits")).toDouble(),
                    shown.value(QStringLiteral("entropyQuality")).toString().toStdString().c_str());
    }
    if (!reveal)
        std::printf("(passwords masked; use --reveal to show them)\n");
    return 0;
}
