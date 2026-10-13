#include "generators/identity/common/TemplateEngine.hpp"

#include <QtTest>

using namespace account_recommendations;
using account_recommendations::generators::identity::TemplateEngine;

class TstTemplateEngine final : public QObject
{
    Q_OBJECT
private slots:
    void rendersOnlySuppliedTokens()
    {
        QHash<QString, QString> values{{QStringLiteral("first"), QStringLiteral("Rin")},
                                        {QStringLiteral("last"), QStringLiteral("Sato")}};
        const auto rendered = TemplateEngine::render(QStringLiteral("{first}.{last}"), values);
        QVERIFY(rendered.hasValue());
        QCOMPARE(rendered.value(), QStringLiteral("Rin.Sato"));
    }

    void supportsEscapedBraces()
    {
        QHash<QString, QString> values{{QStringLiteral("name"), QStringLiteral("Kai")}};
        const auto rendered = TemplateEngine::render(QStringLiteral("{{user:{name}}}"), values);
        QVERIFY(rendered.hasValue());
        QCOMPARE(rendered.value(), QStringLiteral("{user:Kai}"));
    }

    void rejectsUnknownTokensAndMalformedSyntax()
    {
        QVERIFY(!TemplateEngine::validate(QStringLiteral("{exec}"), {QStringLiteral("first")}).isValid());
        QVERIFY(!TemplateEngine::validate(QStringLiteral("{first"), {QStringLiteral("first")}).isValid());
        QVERIFY(!TemplateEngine::validate(QStringLiteral("first}"), {QStringLiteral("first")}).isValid());
        QVERIFY(!TemplateEngine::validate(QStringLiteral("{first-name}"), {QStringLiteral("first-name")}).isValid());
    }

    void tokenDetectionDistinguishesEscapedPlaceholders()
    {
        QVERIFY(TemplateEngine::containsToken(QStringLiteral("{number}"), QStringLiteral("number")));
        QVERIFY(!TemplateEngine::containsToken(QStringLiteral("{{number}}"), QStringLiteral("number")));
        QVERIFY(!TemplateEngine::containsToken(QStringLiteral("{number"), QStringLiteral("number")));
    }

    void renderingRequiresEveryTokenValue()
    {
        const auto rendered = TemplateEngine::render(QStringLiteral("{first}.{last}"),
                                                      {{QStringLiteral("first"), QStringLiteral("Kai")}});
        QVERIFY(!rendered.hasValue());
        QVERIFY(rendered.error().contains(QStringLiteral("template"), QStringLiteral("missing_template_value")));
    }

    void enforcesRenderedLength()
    {
        common::ApplicationLimits limits;
        limits.maxOutputLength = 5;
        const auto rendered = TemplateEngine::render(QStringLiteral("{first}{last}"),
            {{QStringLiteral("first"), QStringLiteral("hello")},
             {QStringLiteral("last"), QStringLiteral("world")}}, limits);
        QVERIFY(!rendered.hasValue());
        QVERIFY(rendered.error().contains(QStringLiteral("template"), QStringLiteral("output_too_long")));
    }
};

QTEST_GUILESS_MAIN(TstTemplateEngine)
#include "tst_template_engine.moc"
