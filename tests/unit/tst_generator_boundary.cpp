#include "TestSupport.hpp"

#include <QJsonArray>
#include <QtTest>

using namespace account_recommendations;

namespace
{
enum class RecordShape
{
    Scalar,
    ExtraField,
    ObjectValue,
    ArrayValue,
    MissingField
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

domain::GeneratorRequest request(qsizetype count = 3)
{
    domain::GeneratorRequest r;
    r.generatorId = QStringLiteral("boundary-probe");
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
};

QTEST_APPLESS_MAIN(TstGeneratorBoundary)
#include "tst_generator_boundary.moc"
