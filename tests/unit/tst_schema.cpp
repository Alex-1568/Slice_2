#include "domain/generator/ConfigurationSchema.hpp"

#include <QJsonArray>
#include <QtTest>

#include <limits>

using namespace account_recommendations::domain;

namespace
{
ConfigurationSchema sampleSchema()
{
    return ConfigurationSchema(
        {FieldSpec::integer(QStringLiteral("length"), QStringLiteral("Length"), 24, 4, 512),
         FieldSpec::boolean(QStringLiteral("symbols"), QStringLiteral("Symbols"), true),
         FieldSpec::string(QStringLiteral("prefix"), QStringLiteral("Prefix"), QString(), 8),
         FieldSpec::enumeration(QStringLiteral("style"), QStringLiteral("Style"),
                                {QStringLiteral("casual"), QStringLiteral("formal")},
                                QStringLiteral("casual"))});
}
} // namespace

class TstSchema : public QObject
{
    Q_OBJECT

private slots:
    void defaultsAreAppliedOnlyForMissingKeys()
    {
        const auto schema = sampleSchema();
        const QJsonObject defaults = schema.defaults();
        QCOMPARE(defaults.value(QStringLiteral("length")).toInt(), 24);
        QCOMPARE(defaults.value(QStringLiteral("style")).toString(), QStringLiteral("casual"));

        const QJsonObject merged =
            schema.applyDefaults(QJsonObject{{QStringLiteral("length"), 30}});
        QCOMPARE(merged.value(QStringLiteral("length")).toInt(), 30);
        QVERIFY(merged.contains(QStringLiteral("symbols")));
    }

    void validInputPasses()
    {
        QVERIFY(sampleSchema().validate(sampleSchema().defaults()).isValid());
        QVERIFY(sampleSchema().validate({}).isValid());
    }

    void rejectsInvalidInput_data()
    {
        QTest::addColumn<QJsonObject>("params");
        QTest::addColumn<QString>("field");
        QTest::addColumn<QString>("code");
        QTest::newRow("string for int")
            << QJsonObject{{"length", "24"}} << "length" << "invalid_type";
        QTest::newRow("bool for int") << QJsonObject{{"length", true}} << "length" << "invalid_type";
        QTest::newRow("null for int") << QJsonObject{{"length", QJsonValue::Null}} << "length" << "invalid_type";
        QTest::newRow("object for int")
            << QJsonObject{{"length", QJsonObject{{"a", 1}}}} << "length" << "invalid_type";
        QTest::newRow("array for int") << QJsonObject{{"length", QJsonArray{1}}} << "length" << "invalid_type";
        QTest::newRow("fractional") << QJsonObject{{"length", 24.5}} << "length" << "invalid_value";
        QTest::newRow("huge") << QJsonObject{{"length", 1e300}} << "length" << "invalid_value";
        QTest::newRow("below min") << QJsonObject{{"length", 3}} << "length" << "out_of_range";
        QTest::newRow("above max") << QJsonObject{{"length", 513}} << "length" << "out_of_range";
        QTest::newRow("int for bool") << QJsonObject{{"symbols", 1}} << "symbols" << "invalid_type";
        QTest::newRow("int for string") << QJsonObject{{"prefix", 5}} << "prefix" << "invalid_type";
        QTest::newRow("string too long") << QJsonObject{{"prefix", "123456789"}} << "prefix" << "too_long";
        QTest::newRow("bad enum") << QJsonObject{{"style", "wild"}} << "style" << "invalid_choice";
        QTest::newRow("unknown key") << QJsonObject{{"nope", 1}} << "nope" << "unknown_field";
    }
    void rejectsInvalidInput()
    {
        QFETCH(QJsonObject, params);
        QFETCH(QString, field);
        QFETCH(QString, code);
        const auto result = sampleSchema().validate(params);
        QVERIFY(!result.isValid());
        QVERIFY2(result.contains(field, code), qPrintable(result.summary()));
    }

    void rangeMessageIsHumanReadable()
    {
        const auto result = sampleSchema().validate(QJsonObject{{"length", 3}});
        QCOMPARE(result.summary(), QStringLiteral("Length must be between 4 and 512."));
    }

    void requiredFieldMissing()
    {
        auto field = FieldSpec::string(QStringLiteral("name"), QStringLiteral("Name"), QString(), 10);
        field.required = true;
        const ConfigurationSchema schema({field});
        QVERIFY(schema.validate({}).contains(QStringLiteral("name"), QStringLiteral("required")));
    }

    void definitionValidation()
    {
        QVERIFY(sampleSchema().validateDefinition().isValid());

        const ConfigurationSchema duplicate(
            {FieldSpec::boolean(QStringLiteral("a"), QStringLiteral("A"), true),
             FieldSpec::boolean(QStringLiteral("a"), QStringLiteral("A2"), true)});
        QVERIFY(duplicate.validateDefinition().containsCode(QStringLiteral("duplicate_key")));

        const ConfigurationSchema badKey({FieldSpec::boolean(QStringLiteral("a b"), QStringLiteral("A"), true)});
        QVERIFY(badKey.validateDefinition().containsCode(QStringLiteral("invalid_key")));

        const ConfigurationSchema badDefault(
            {FieldSpec::integer(QStringLiteral("n"), QStringLiteral("N"), 99, 1, 10)});
        QVERIFY(badDefault.validateDefinition().containsCode(QStringLiteral("invalid_default")));

        const ConfigurationSchema badRange(
            {FieldSpec::integer(QStringLiteral("n"), QStringLiteral("N"), 5, 10, 1)});
        QVERIFY(badRange.validateDefinition().containsCode(QStringLiteral("invalid_range")));

        const ConfigurationSchema emptyEnum(
            {FieldSpec::enumeration(QStringLiteral("e"), QStringLiteral("E"), {}, QString())});
        QVERIFY(emptyEnum.validateDefinition().containsCode(QStringLiteral("empty_enumeration")));
    }

    void toJsonDescribesFieldsForTheGui()
    {
        const QJsonArray fields = sampleSchema().toJson().value(QStringLiteral("fields")).toArray();
        QCOMPARE(fields.size(), 4);
        const QJsonObject length = fields.at(0).toObject();
        QCOMPARE(length.value(QStringLiteral("key")).toString(), QStringLiteral("length"));
        QCOMPARE(length.value(QStringLiteral("type")).toString(), QStringLiteral("integer"));
        QCOMPARE(length.value(QStringLiteral("minimum")).toInt(), 4);
        QCOMPARE(length.value(QStringLiteral("maximum")).toInt(), 512);
        QCOMPARE(fields.at(3).toObject().value(QStringLiteral("values")).toArray().size(), 2);
    }
};

QTEST_APPLESS_MAIN(TstSchema)
#include "tst_schema.moc"
