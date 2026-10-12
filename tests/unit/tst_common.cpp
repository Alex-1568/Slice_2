#include "common/result/Result.hpp"
#include "common/security/SecureWipe.hpp"
#include "common/validation/ValidationResult.hpp"

#include <QtTest>

#include <memory>

using namespace account_recommendations::common;

class TstCommon : public QObject
{
    Q_OBJECT

private slots:
    void resultHoldsValue()
    {
        auto result = Result<int, QString>::success(42);
        QVERIFY(result.hasValue());
        QVERIFY(static_cast<bool>(result));
        QCOMPARE(result.value(), 42);
        QCOMPARE(result.valueOr(7), 42);
        QVERIFY_THROWS_EXCEPTION(BadResultAccess, static_cast<void>(result.error()));
    }

    void resultHoldsError()
    {
        auto result = Result<int, QString>::failure(QStringLiteral("boom"));
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error(), QStringLiteral("boom"));
        QCOMPARE(result.valueOr(7), 7);
        QVERIFY_THROWS_EXCEPTION(BadResultAccess, static_cast<void>(result.value()));
    }

    void resultWithIdenticalTypesIsUnambiguous()
    {
        auto ok = Result<int, int>::success(1);
        auto bad = Result<int, int>::failure(1);
        QVERIFY(ok.hasValue());
        QVERIFY(!bad.hasValue());
    }

    void resultSupportsMoveOnlyValues()
    {
        auto result = Result<std::unique_ptr<int>, QString>::success(std::make_unique<int>(5));
        QVERIFY(result.hasValue());
        std::unique_ptr<int> moved = std::move(result).value();
        QCOMPARE(*moved, 5);
    }

    void voidResult()
    {
        auto ok = Result<void, QString>::success();
        QVERIFY(ok.hasValue());
        QVERIFY_THROWS_EXCEPTION(BadResultAccess, static_cast<void>(ok.error()));

        auto bad = Result<void, QString>::failure(QStringLiteral("nope"));
        QVERIFY(!bad.hasValue());
        QCOMPARE(bad.error(), QStringLiteral("nope"));
    }

    void validationWarningsDoNotInvalidate()
    {
        ValidationResult result;
        QVERIFY(result.isValid());
        result.addWarning(QStringLiteral("f"), QStringLiteral("w"), QStringLiteral("careful"));
        QVERIFY(result.isValid());
        QVERIFY(result.hasWarnings());
        result.addError(QStringLiteral("length"), QStringLiteral("out_of_range"),
                        QStringLiteral("Length must be between 4 and 512."));
        QVERIFY(!result.isValid());
        QVERIFY(result.contains(QStringLiteral("length"), QStringLiteral("out_of_range")));
        QVERIFY(!result.contains(QStringLiteral("length"), QStringLiteral("other")));
        QVERIFY(result.containsCode(QStringLiteral("w")));
        QCOMPARE(result.summary(), QStringLiteral("Length must be between 4 and 512."));
    }

    void validationMerge()
    {
        ValidationResult a;
        ValidationResult b;
        b.addError(QStringLiteral("x"), QStringLiteral("c"), QStringLiteral("m"));
        a.merge(b);
        QCOMPARE(a.issues().size(), 1);
        QVERIFY(!a.isValid());
    }

    void secureWipeZeroesBuffer()
    {
        char32_t buffer[4] = {U'a', U'b', U'c', U'd'};
        secureWipe(buffer, 4);
        for (char32_t c : buffer)
            QCOMPARE(static_cast<uint>(c), 0U);

        std::u32string text = U"secret";
        secureWipe(text);
        QVERIFY(text.empty());
    }
};

QTEST_APPLESS_MAIN(TstCommon)
#include "tst_common.moc"
