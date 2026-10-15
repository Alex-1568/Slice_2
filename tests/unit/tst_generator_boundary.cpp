#include "TestSupport.hpp"

#include <QJsonArray>
#include <QtTest>

#include <algorithm>
#include <stdexcept>
#include <utility>

using namespace account_recommendations;

namespace
{
enum class RecordShape
{
    Scalar,
    ExtraField,
    ObjectValue,
    ArrayValue,
    MissingField,
    LongString
};

class BoundaryProbeGenerator final : public domain::IGenerator
{
public:
    explicit BoundaryProbeGenerator(RecordShape shape = RecordShape::Scalar,
                                    bool invalidateAfterOne = false,
                                    domain::CancellationSource* cancelAfterFirst = nullptr)
        : m_shape(shape), m_invalidateAfterOne(invalidateAfterOne), m_cancelAfterFirst(cancelAfterFirst)
    {
    }

    domain::GeneratorMetadata metadata() const override
    {
        domain::GeneratorMetadata md;
        md.id = QStringLiteral("boundary-probe");
        md.displayName = QStringLiteral("Boundary Probe");
        md.description = QStringLiteral("Test generator for the public record-validation boundary.");
        md.category = QStringLiteral("Test");
        md.version = QStringLiteral("1.0.0");
        md.configurationSchema = domain::ConfigurationSchema({
            domain::FieldSpec::integer(QStringLiteral("size"), QStringLiteral("Size"), 1, 1, 10)});
        md.outputFields = {{QStringLiteral("value"), QStringLiteral("Value"), false}};
        return md;
    }

    common::ValidationResult validate(const domain::GeneratorRequest&) const override { return {}; }

protected:
    common::Result<domain::GenerationSummary, domain::GenerationError>
    generateIntoImpl(const domain::GeneratorRequest& request, const domain::GenerationContext& context,
                     domain::IResultSink& sink) const override
    {
        using R = common::Result<domain::GenerationSummary, domain::GenerationError>;
        domain::GenerationSummary summary;
        for (qsizetype i = 0; i < request.count; ++i) {
            if (m_cancelAfterFirst && i == 1)
                m_cancelAfterFirst->requestCancel();
            if (context.isCancelled()) {
                summary.status = domain::CompletionStatus::Cancelled;
                return R::success(summary);
            }

            const bool makeInvalid = !m_invalidateAfterOne || i >= 1;
            QJsonObject record;
            if (!(makeInvalid && m_shape == RecordShape::MissingField))
                record.insert(QStringLiteral("value"), static_cast<double>(i));
            if (makeInvalid) {
                switch (m_shape) {
                case RecordShape::Scalar:
                case RecordShape::MissingField:
                    break;
                case RecordShape::ExtraField:
                    record.insert(QStringLiteral("undeclared"), QStringLiteral("must-not-escape"));
                    break;
                case RecordShape::ObjectValue:
                    record.insert(QStringLiteral("value"), QJsonObject{{QStringLiteral("nested"), 1}});
                    break;
                case RecordShape::ArrayValue:
                    record.insert(QStringLiteral("value"), QJsonArray{1, 2, 3});
                    break;
                case RecordShape::LongString:
                    record.insert(QStringLiteral("value"), QStringLiteral("more-than-three"));
                    break;
                }
            }

            ++summary.produced;
            if (sink.accept(std::move(record)) == domain::SinkResponse::Stop) {
                summary.status = domain::CompletionStatus::StoppedBySink;
                return R::success(summary);
            }
        }
        summary.status = domain::CompletionStatus::Completed;
        return R::success(summary);
    }

private:
    RecordShape m_shape;
    bool m_invalidateAfterOne;
    domain::CancellationSource* m_cancelAfterFirst;
};

enum class BoundaryViolation
{
    Overproduce,
    Underproduce,
    IgnoreSinkStop,
    ThrowAfterFirst
};

class BoundaryViolationGenerator final : public domain::IGenerator
{
public:
    explicit BoundaryViolationGenerator(BoundaryViolation violation) : m_violation(violation) {}

    domain::GeneratorMetadata metadata() const override
    {
        domain::GeneratorMetadata md;
        md.id = QStringLiteral("boundary-violation");
        md.displayName = QStringLiteral("Boundary Violation");
        md.description = QStringLiteral("Adversarial implementation for boundary tests.");
        md.category = QStringLiteral("Test");
        md.version = QStringLiteral("1.0.0");
        md.configurationSchema = domain::ConfigurationSchema{};
        md.outputFields = {{QStringLiteral("value"), QStringLiteral("Value"), false}};
        return md;
    }

    common::ValidationResult validate(const domain::GeneratorRequest&) const override { return {}; }

protected:
    common::Result<domain::GenerationSummary, domain::GenerationError>
    generateIntoImpl(const domain::GeneratorRequest& request, const domain::GenerationContext& context,
                     domain::IResultSink& sink) const override
    {
        using R = common::Result<domain::GenerationSummary, domain::GenerationError>;
        domain::GenerationSummary summary;
        qsizetype target = request.count;
        if (m_violation == BoundaryViolation::Overproduce)
            ++target;
        else if (m_violation == BoundaryViolation::Underproduce)
            target = std::max<qsizetype>(0, request.count - 1);

        for (qsizetype i = 0; i < target; ++i) {
            if (context.isCancelled()) {
                summary.status = domain::CompletionStatus::Cancelled;
                return R::success(summary);
            }
            domain::GeneratedRecord record;
            record.insert(QStringLiteral("value"), static_cast<double>(i));
            ++summary.produced;
            const auto response = sink.accept(std::move(record));
            if (response == domain::SinkResponse::Stop
                && m_violation != BoundaryViolation::IgnoreSinkStop) {
                summary.status = domain::CompletionStatus::StoppedBySink;
                return R::success(summary);
            }
            if (m_violation == BoundaryViolation::ThrowAfterFirst && i == 0)
                throw std::runtime_error("synthetic plugin exception containing no useful diagnostics");
        }
        summary.status = domain::CompletionStatus::Completed;
        return R::success(summary);
    }

private:
    BoundaryViolation m_violation;
};

class RejectingValidationGenerator final : public domain::IGenerator
{
public:
    explicit RejectingValidationGenerator(bool* implementationCalled)
        : m_implementationCalled(implementationCalled)
    {
    }

    domain::GeneratorMetadata metadata() const override
    {
        domain::GeneratorMetadata md;
        md.id = QStringLiteral("rejecting-validation");
        md.displayName = QStringLiteral("Rejecting Validation");
        md.description = QStringLiteral("Test generator whose validator must run at the public boundary.");
        md.category = QStringLiteral("Test");
        md.version = QStringLiteral("1.0.0");
        md.configurationSchema = domain::ConfigurationSchema{};
        md.outputFields = {{QStringLiteral("value"), QStringLiteral("Value"), false}};
        return md;
    }

    common::ValidationResult validate(const domain::GeneratorRequest&) const override
    {
        common::ValidationResult result;
        result.addError(QStringLiteral("value"), QStringLiteral("test_rejected"),
                        QStringLiteral("Rejected by boundary test."));
        return result;
    }

protected:
    common::Result<domain::GenerationSummary, domain::GenerationError>
    generateIntoImpl(const domain::GeneratorRequest& request, const domain::GenerationContext&,
                     domain::IResultSink& sink) const override
    {
        using R = common::Result<domain::GenerationSummary, domain::GenerationError>;
        *m_implementationCalled = true;
        domain::GenerationSummary summary;
        for (qsizetype i = 0; i < request.count; ++i) {
            domain::GeneratedRecord record{{QStringLiteral("value"), static_cast<double>(i)}};
            ++summary.produced;
            if (sink.accept(std::move(record)) == domain::SinkResponse::Stop) {
                summary.status = domain::CompletionStatus::StoppedBySink;
                return R::success(summary);
            }
        }
        summary.status = domain::CompletionStatus::Completed;
        return R::success(summary);
    }

private:
    bool* m_implementationCalled;
};

class StopAfterFirstSink final : public domain::IResultSink
{
public:
    [[nodiscard]] domain::SinkResponse accept(domain::GeneratedRecord&& record) override
    {
        records.append(std::move(record));
        return domain::SinkResponse::Stop;
    }

    QList<domain::GeneratedRecord> records;
};

domain::GeneratorRequest request(qsizetype count = 3)
{
    domain::GeneratorRequest r;
    r.generatorId = QStringLiteral("boundary-probe");
    r.count = count;
    return r;
}

domain::GeneratorRequest violatingRequest(qsizetype count = 3)
{
    domain::GeneratorRequest r;
    r.generatorId = QStringLiteral("boundary-violation");
    r.count = count;
    return r;
}
} // namespace

class TstGeneratorBoundary : public QObject
{
    Q_OBJECT

private slots:
    void validBatchPassesThroughTheValidatedBoundary()
    {
        testing::DeterministicRandomProvider random(11);
        const domain::GenerationContext context(random);
        const auto result = BoundaryProbeGenerator().generate(request(4), context);
        QVERIFY(result.hasValue());
        QCOMPARE(result.value().summary.status, domain::CompletionStatus::Completed);
        QCOMPARE(result.value().records.size(), qsizetype{4});
        for (const auto& record : result.value().records) {
            QCOMPARE(record.size(), 1);
            QVERIFY(record.value(QStringLiteral("value")).isDouble());
        }
    }

    void batchRejectsUndeclaredFieldsWithoutReturningPartialRecords()
    {
        testing::DeterministicRandomProvider random(12);
        const domain::GenerationContext context(random);
        const auto result = BoundaryProbeGenerator(RecordShape::ExtraField, true).generate(request(3), context);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, domain::GenerationError::Code::InvalidOutputRecord);
        QVERIFY(result.error().validation.contains(QStringLiteral("undeclared"), QStringLiteral("undeclared_field")));
    }

    void batchRejectsNestedObjectsAndArrays()
    {
        testing::DeterministicRandomProvider random(13);
        const domain::GenerationContext context(random);
        for (RecordShape shape : {RecordShape::ObjectValue, RecordShape::ArrayValue}) {
            const auto result = BoundaryProbeGenerator(shape).generate(request(1), context);
            QVERIFY(!result.hasValue());
            QCOMPARE(result.error().code, domain::GenerationError::Code::InvalidOutputRecord);
            QVERIFY(result.error().validation.contains(QStringLiteral("value"), QStringLiteral("nested_value")));
        }
    }

    void publicBoundaryRejectsNestedParametersEvenWhenValidatorWouldAcceptThem()
    {
        testing::DeterministicRandomProvider random(141);
        const domain::GenerationContext context(random);
        auto generator = BoundaryViolationGenerator(BoundaryViolation::Underproduce);
        domain::GeneratorRequest request = violatingRequest(1);
        request.parameters.insert(QStringLiteral("nested"), QJsonObject{{QStringLiteral("x"), 1}});
        domain::CollectingSink sink;

        const auto result = generator.generateInto(request, context, sink);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, domain::GenerationError::Code::InvalidConfiguration);
        QVERIFY(result.error().validation.contains(QStringLiteral("nested"), QStringLiteral("nested_parameter_value")));
        QCOMPARE(sink.size(), qsizetype{0});
    }

    void publicBoundaryEnforcesParameterCountAndStringLimits()
    {
        testing::DeterministicRandomProvider random(142);
        auto limits = common::ApplicationLimits::defaults();
        limits.maxParameterCount = 1;
        limits.maxStringLength = 4;
        const domain::GenerationContext context(random, domain::CancellationToken{}, limits);
        auto generator = BoundaryViolationGenerator(BoundaryViolation::Underproduce);

        domain::GeneratorRequest tooMany = violatingRequest(1);
        tooMany.parameters.insert(QStringLiteral("a"), 1);
        tooMany.parameters.insert(QStringLiteral("b"), 2);
        domain::CollectingSink countSink;
        const auto countResult = generator.generateInto(tooMany, context, countSink);
        QVERIFY(!countResult.hasValue());
        QCOMPARE(countResult.error().code, domain::GenerationError::Code::LimitExceeded);
        QCOMPARE(countSink.size(), qsizetype{0});

        domain::GeneratorRequest tooLong = violatingRequest(1);
        tooLong.parameters.insert(QStringLiteral("text"), QStringLiteral("longer-than-limit"));
        domain::CollectingSink stringSink;
        const auto stringResult = generator.generateInto(tooLong, context, stringSink);
        QVERIFY(!stringResult.hasValue());
        QCOMPARE(stringResult.error().code, domain::GenerationError::Code::InvalidConfiguration);
        QVERIFY(stringResult.error().validation.contains(QStringLiteral("text"), QStringLiteral("too_long")));
        QCOMPARE(stringSink.size(), qsizetype{0});
    }

    void publicBoundaryEnforcesOutputStringLength()
    {
        testing::DeterministicRandomProvider random(143);
        auto limits = common::ApplicationLimits::defaults();
        limits.maxOutputLength = 3;
        const domain::GenerationContext context(random, domain::CancellationToken{}, limits);
        domain::CollectingSink sink;
        const auto result = BoundaryProbeGenerator(RecordShape::LongString).generateInto(request(1), context, sink);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, domain::GenerationError::Code::LimitExceeded);
        QCOMPARE(sink.size(), qsizetype{0});
    }

    void batchRejectsMissingDeclaredFields()
    {
        testing::DeterministicRandomProvider random(14);
        const domain::GenerationContext context(random);
        const auto result = BoundaryProbeGenerator(RecordShape::MissingField).generate(request(1), context);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, domain::GenerationError::Code::InvalidOutputRecord);
        QVERIFY(result.error().validation.contains(QStringLiteral("value"), QStringLiteral("missing_field")));
    }

    void streamingWithholdsAnInvalidRecordButDocumentsEarlierDeliveredRows()
    {
        testing::DeterministicRandomProvider random(15);
        const domain::GenerationContext context(random);
        domain::CollectingSink sink;
        const auto result = BoundaryProbeGenerator(RecordShape::ExtraField, true).generateInto(request(4), context, sink);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, domain::GenerationError::Code::InvalidOutputRecord);
        QCOMPARE(sink.size(), qsizetype{1}); // first valid row was already streamed
        const auto prior = sink.take();
        QCOMPARE(prior.size(), qsizetype{1});
        QVERIFY(!prior.first().contains(QStringLiteral("undeclared")));
    }

    void streamingWithholdsObjectAndArrayRecords()
    {
        testing::DeterministicRandomProvider random(16);
        const domain::GenerationContext context(random);
        for (RecordShape shape : {RecordShape::ObjectValue, RecordShape::ArrayValue}) {
            domain::CollectingSink sink;
            const auto result = BoundaryProbeGenerator(shape).generateInto(request(1), context, sink);
            QVERIFY(!result.hasValue());
            QCOMPARE(result.error().code, domain::GenerationError::Code::InvalidOutputRecord);
            QCOMPARE(sink.size(), qsizetype{0});
        }
    }

    void cancelledBatchReturnsAnErrorInsteadOfPartialRecords()
    {
        testing::DeterministicRandomProvider random(17);
        domain::CancellationSource cancellation;
        const domain::GenerationContext context(random, cancellation.token());
        const auto result = BoundaryProbeGenerator(RecordShape::Scalar, false, &cancellation).generate(request(10), context);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, domain::GenerationError::Code::Cancelled);
    }

    void publicBoundaryRunsRequestValidationBeforeGeneratorImplementation()
    {
        testing::DeterministicRandomProvider random(181);
        const domain::GenerationContext context(random);
        bool implementationCalled = false;
        domain::CollectingSink sink;
        auto generator = RejectingValidationGenerator(&implementationCalled);
        domain::GeneratorRequest request;
        request.generatorId = QStringLiteral("rejecting-validation");
        request.count = 1;

        const auto result = generator.generateInto(request, context, sink);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, domain::GenerationError::Code::InvalidConfiguration);
        QVERIFY(result.error().validation.contains(QStringLiteral("value"), QStringLiteral("test_rejected")));
        QVERIFY(!implementationCalled);
        QCOMPARE(sink.size(), qsizetype{0});
    }

    void publicStreamingEntryPointAlsoEnforcesTheContract()
    {
        testing::DeterministicRandomProvider random(18);
        const domain::GenerationContext context(random);
        domain::CollectingSink sink;
        const auto result = BoundaryProbeGenerator(RecordShape::ExtraField).generateInto(request(1), context, sink);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, domain::GenerationError::Code::InvalidOutputRecord);
        QCOMPARE(sink.size(), qsizetype{0});
    }

    void productionBoundaryCapsOverproduction()
    {
        testing::DeterministicRandomProvider random(19);
        const domain::GenerationContext context(random);
        const auto result = BoundaryViolationGenerator(BoundaryViolation::Overproduce).generate(violatingRequest(3), context);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, domain::GenerationError::Code::LimitExceeded);

        domain::CollectingSink streamingSink;
        const auto streamed = BoundaryViolationGenerator(BoundaryViolation::Overproduce)
                                 .generateInto(violatingRequest(3), context, streamingSink);
        QVERIFY(!streamed.hasValue());
        QCOMPARE(streamed.error().code, domain::GenerationError::Code::LimitExceeded);
        QCOMPARE(streamingSink.size(), qsizetype{3}); // earlier streaming rows cannot be rolled back
    }

    void productionBoundaryRejectsUnderproductionReportedAsCompleted()
    {
        testing::DeterministicRandomProvider random(20);
        const domain::GenerationContext context(random);
        const auto result = BoundaryViolationGenerator(BoundaryViolation::Underproduce).generate(violatingRequest(3), context);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, domain::GenerationError::Code::Internal);
    }

    void productionBoundaryStopsAProviderThatIgnoresSinkStop()
    {
        testing::DeterministicRandomProvider random(21);
        const domain::GenerationContext context(random);
        StopAfterFirstSink sink;
        const auto result = BoundaryViolationGenerator(BoundaryViolation::IgnoreSinkStop)
                                .generateInto(violatingRequest(5), context, sink);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, domain::GenerationError::Code::Internal);
        QCOMPARE(sink.records.size(), qsizetype{1});
    }

    void compliantSinkStopIsReportedConsistently()
    {
        testing::DeterministicRandomProvider random(22);
        const domain::GenerationContext context(random);
        StopAfterFirstSink sink;
        const auto result = BoundaryProbeGenerator().generateInto(request(5), context, sink);
        QVERIFY(result.hasValue());
        QCOMPARE(result.value().status, domain::CompletionStatus::StoppedBySink);
        QCOMPARE(result.value().produced, qsizetype{1});
        QCOMPARE(sink.records.size(), qsizetype{1});
    }

    void pluginExceptionIsContainedAndBatchPartialRowsAreNotReturned()
    {
        testing::DeterministicRandomProvider random(23);
        const domain::GenerationContext context(random);
        const auto result = BoundaryViolationGenerator(BoundaryViolation::ThrowAfterFirst).generate(violatingRequest(3), context);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, domain::GenerationError::Code::Internal);
        QVERIFY(!result.error().message.contains(QStringLiteral("synthetic plugin exception")));
        QVERIFY(!result.error().message.contains(QStringLiteral("generated"), Qt::CaseInsensitive));
    }
};

QTEST_APPLESS_MAIN(TstGeneratorBoundary)
#include "tst_generator_boundary.moc"
