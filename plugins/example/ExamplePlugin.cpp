#include "ExamplePlugin.hpp"

#include "domain/generator/ConfigurationSchema.hpp"
#include "domain/generator/IGenerator.hpp"

#include <QJsonObject>

#include <memory>
#include <utility>

namespace account_recommendations::plugins::example
{
namespace
{
class ExampleTagGenerator final : public domain::IGenerator
{
public:
    [[nodiscard]] domain::GeneratorMetadata metadata() const override
    {
        domain::GeneratorMetadata md;
        md.id = QStringLiteral("example-tag");
        md.displayName = QStringLiteral("Example Tag (Plugin)");
        md.description = QStringLiteral("Demonstration plugin generator. It emits deterministic synthetic tags and performs no I/O.");
        md.category = QStringLiteral("Plugin examples");
        md.version = QStringLiteral("1.0.0");
        md.tags = {QStringLiteral("example"), QStringLiteral("plugin"), QStringLiteral("synthetic")};
        md.supportsBatch = true;
        md.supportsPresets = false;
        md.supportsUniqueness = false;
        md.requiresSecureRandom = false;
        md.configurationSchema = domain::ConfigurationSchema({
            domain::FieldSpec::string(QStringLiteral("prefix"), QStringLiteral("Prefix"),
                                      QStringLiteral("demo-"), 32, QStringLiteral("Format")),
            domain::FieldSpec::integer(QStringLiteral("startNumber"), QStringLiteral("Starting number"),
                                       1, 0, 1000000, QStringLiteral("Format"))});
        md.outputFields = {{QStringLiteral("exampleTag"), QStringLiteral("Example tag"), false}};
        return md;
    }

    [[nodiscard]] common::ValidationResult validate(const domain::GeneratorRequest& request) const override
    {
        const auto md = metadata();
        auto parameters = md.configurationSchema.applyDefaults(request.parameters);
        auto issues = md.configurationSchema.validate(parameters);
        if (request.generatorId != md.id)
            issues.addError(QStringLiteral("generatorId"), QStringLiteral("wrong_generator"),
                            QStringLiteral("Request is not addressed to example-tag."));
        if (request.count < 1)
            issues.addError(QStringLiteral("count"), QStringLiteral("out_of_range"),
                            QStringLiteral("Count must be at least one."));
        return issues;
    }

protected:
    [[nodiscard]] common::Result<domain::GenerationSummary, domain::GenerationError>
    generateIntoImpl(const domain::GeneratorRequest& request,
                     const domain::GenerationContext& context,
                     domain::IResultSink& sink) const override
    {
        using R = common::Result<domain::GenerationSummary, domain::GenerationError>;
        const auto md = metadata();
        const auto parameters = md.configurationSchema.applyDefaults(request.parameters);
        const auto issues = md.configurationSchema.validate(parameters, context.limits());
        if (!issues.isValid())
            return R::failure(domain::GenerationError::invalidConfiguration(issues));
        const QString prefix = parameters.value(QStringLiteral("prefix")).toString();
        const qint64 start = parameters.value(QStringLiteral("startNumber")).toInteger();
        domain::GenerationSummary summary;
        for (qsizetype index = 0; index < request.count; ++index) {
            if (context.isCancelled()) {
                summary.status = domain::CompletionStatus::Cancelled;
                return R::success(summary);
            }
            const QString value = prefix + QString::number(start + index);
            if (static_cast<std::size_t>(value.size()) > context.limits().maxOutputLength)
                return R::failure(domain::GenerationError::limitExceeded(
                    QStringLiteral("Example tag exceeds the application output length limit.")));
            domain::GeneratedRecord record;
            record.insert(QStringLiteral("exampleTag"), value);
            if (sink.accept(std::move(record)) == domain::SinkResponse::Stop) {
                summary.status = domain::CompletionStatus::StoppedBySink;
                return R::success(summary);
            }
            ++summary.produced;
        }
        summary.status = domain::CompletionStatus::Completed;
        return R::success(summary);
    }
};

class ExampleCounterGenerator final : public domain::IGenerator
{
public:
    [[nodiscard]] domain::GeneratorMetadata metadata() const override
    {
        domain::GeneratorMetadata md;
        md.id = QStringLiteral("example-counter");
        md.displayName = QStringLiteral("Example Counter (Plugin)");
        md.description = QStringLiteral("A second example output used to demonstrate atomic plugin registration.");
        md.category = QStringLiteral("Plugin examples");
        md.version = QStringLiteral("1.0.0");
        md.tags = {QStringLiteral("example"), QStringLiteral("plugin")};
        md.supportsBatch = true;
        md.supportsPresets = false;
        md.supportsUniqueness = false;
        md.configurationSchema = domain::ConfigurationSchema({
            domain::FieldSpec::integer(QStringLiteral("start"), QStringLiteral("Start"), 0, 0, 1000000)});
        md.outputFields = {{QStringLiteral("counter"), QStringLiteral("Counter"), false}};
        return md;
    }

    [[nodiscard]] common::ValidationResult validate(const domain::GeneratorRequest& request) const override
    {
        const auto md = metadata();
        auto values = md.configurationSchema.applyDefaults(request.parameters);
        auto issues = md.configurationSchema.validate(values);
        if (request.generatorId != md.id)
            issues.addError(QStringLiteral("generatorId"), QStringLiteral("wrong_generator"),
                            QStringLiteral("Request is not addressed to example-counter."));
        if (request.count < 1)
            issues.addError(QStringLiteral("count"), QStringLiteral("out_of_range"),
                            QStringLiteral("Count must be at least one."));
        return issues;
    }

protected:
    [[nodiscard]] common::Result<domain::GenerationSummary, domain::GenerationError>
    generateIntoImpl(const domain::GeneratorRequest& request, const domain::GenerationContext& context,
                     domain::IResultSink& sink) const override
    {
        using R = common::Result<domain::GenerationSummary, domain::GenerationError>;
        const auto md = metadata();
        const auto values = md.configurationSchema.applyDefaults(request.parameters);
        const auto issues = md.configurationSchema.validate(values, context.limits());
        if (!issues.isValid())
            return R::failure(domain::GenerationError::invalidConfiguration(issues));
        const qint64 start = values.value(QStringLiteral("start")).toInteger();
        domain::GenerationSummary summary;
        for (qsizetype index = 0; index < request.count; ++index) {
            if (context.isCancelled()) {
                summary.status = domain::CompletionStatus::Cancelled;
                return R::success(summary);
            }
            domain::GeneratedRecord record;
            record.insert(QStringLiteral("counter"), start + index);
            if (sink.accept(std::move(record)) == domain::SinkResponse::Stop) {
                summary.status = domain::CompletionStatus::StoppedBySink;
                return R::success(summary);
            }
            ++summary.produced;
        }
        summary.status = domain::CompletionStatus::Completed;
        return R::success(summary);
    }
};
} // namespace

QString ExamplePlugin::pluginId() const { return QStringLiteral("example-prefix-plugin"); }
QString ExamplePlugin::pluginVersion() const { return QStringLiteral("1.0.0"); }
int ExamplePlugin::apiVersion() const noexcept { return domain::kGeneratorApiVersion; }

common::Result<void, domain::RegistryError>
ExamplePlugin::registerGenerators(domain::GeneratorRegistry& registry)
{
    auto first = registry.registerGenerator(std::make_unique<ExampleTagGenerator>());
    if (!first)
        return first;
    return registry.registerGenerator(std::make_unique<ExampleCounterGenerator>());
}

} // namespace account_recommendations::plugins::example
