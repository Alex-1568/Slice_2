#include "TestSupport.hpp"
#include "domain/security/SensitiveDataPolicy.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

using namespace account_recommendations;
using domain::FieldHandling;
using domain::OutputChannel;
using domain::SensitiveDataPolicy;
using domain::SensitiveDataSettings;

namespace
{
QList<domain::OutputFieldSpec> fields()
{
    return {{QStringLiteral("secret"), QStringLiteral("Secret"), true},
            {QStringLiteral("label"), QStringLiteral("Label"), false}};
}
QJsonObject record()
{
    return {{QStringLiteral("secret"), QStringLiteral("hunter2-xyz")},
            {QStringLiteral("label"), QStringLiteral("visible")}};
}
} // namespace

class TstSensitive : public QObject
{
    Q_OBJECT

private slots:
    void logsAlwaysRedactSensitiveValues()
    {
        // Even with every opt-in enabled, logs never receive sensitive values.
        const SensitiveDataPolicy policy(fields(), SensitiveDataSettings{true, true});
        const QJsonObject out = policy.sanitize(record(), OutputChannel::Log);
        QCOMPARE(out.value(QStringLiteral("secret")).toString(), SensitiveDataPolicy::redactionText());
        QCOMPARE(out.value(QStringLiteral("label")).toString(), QStringLiteral("visible"));
    }

    void persistenceOmitsSensitiveByDefault()
    {
        const SensitiveDataPolicy policy(fields());
        const QJsonObject out = policy.sanitize(record(), OutputChannel::Persistence);
        QVERIFY(!out.contains(QStringLiteral("secret")));
        QVERIFY(out.contains(QStringLiteral("label")));
    }

    void persistenceRequiresExplicitOptIn()
    {
        const SensitiveDataPolicy policy(fields(), SensitiveDataSettings{true, false});
        QVERIFY(policy.sanitize(record(), OutputChannel::Persistence).contains(QStringLiteral("secret")));
    }

    void workspaceFollowsThePersistenceOptIn()
    {
        const SensitiveDataPolicy strict(fields());
        QVERIFY(!strict.sanitize(record(), OutputChannel::Workspace).contains(QStringLiteral("secret")));
        QVERIFY(strict.sanitize(record(), OutputChannel::Workspace).contains(QStringLiteral("label")));
        const SensitiveDataPolicy optedIn(fields(), SensitiveDataSettings{true, false});
        QVERIFY(optedIn.sanitize(record(), OutputChannel::Workspace).contains(QStringLiteral("secret")));
    }

    void displayMasksUntilRevealed()
    {
        const SensitiveDataPolicy masked(fields());
        QCOMPARE(masked.sanitize(record(), OutputChannel::Display).value(QStringLiteral("secret")).toString(),
                 SensitiveDataPolicy::maskText());
        const SensitiveDataPolicy revealed(fields(), SensitiveDataSettings{false, true});
        QCOMPARE(revealed.sanitize(record(), OutputChannel::Display).value(QStringLiteral("secret")).toString(),
                 QStringLiteral("hunter2-xyz"));
    }

    void maskDoesNotLeakLength()
    {
        const SensitiveDataPolicy policy(fields());
        const QJsonObject shortRecord{{QStringLiteral("secret"), QStringLiteral("a")}};
        const QJsonObject longRecord{{QStringLiteral("secret"), QString(200, QLatin1Char('z'))}};
        QCOMPARE(policy.sanitize(shortRecord, OutputChannel::Display),
                 policy.sanitize(longRecord, OutputChannel::Display));
    }

    // ---- Export / Clipboard confirmation contract (regression tests) ----------------

    // 1. Metadata declares NOTHING sensitive, yet the record carries an extra, undeclared field.
    void undeclaredExtraFieldTriggersConfirmationEvenWhenMetadataDeclaresNothingSensitive()
    {
        const SensitiveDataPolicy policy(
            QList<domain::OutputFieldSpec>{{QStringLiteral("name"), QStringLiteral("Name"), false}});
        QVERIFY(!policy.hasSensitiveFields()); // the old metadata-level shortcut would have said "no confirmation"
        const QJsonObject honest{{QStringLiteral("name"), QStringLiteral("Alex")}};
        const QJsonObject leaky{{QStringLiteral("name"), QStringLiteral("Alex")},
                                {QStringLiteral("apiKey"), QStringLiteral("sk-live-123")}};
        for (auto channel : {OutputChannel::Export, OutputChannel::Clipboard}) {
            QVERIFY(!policy.requiresConfirmation(honest, channel));
            QVERIFY(policy.requiresConfirmation(leaky, channel));
            QVERIFY(policy.requiresConfirmation(QList<QJsonObject>{honest, leaky}, channel));
            QVERIFY(!policy.requiresConfirmation(QList<QJsonObject>{honest, honest}, channel));
        }
    }

    // 2. + 3. Without authorization nothing sensitive or undeclared leaves through Export/Clipboard.
    void exportAndClipboardWithoutAuthorizationNeverCarryPlaintext()
    {
        const SensitiveDataPolicy declaredPolicy(fields());
        const SensitiveDataPolicy undeclaredPolicy(
            QList<domain::OutputFieldSpec>{{QStringLiteral("name"), QStringLiteral("Name"), false}});
        const QJsonObject extra{{QStringLiteral("name"), QStringLiteral("Alex")}, {QStringLiteral("apiKey"), QStringLiteral("sk-live-123")}};

        for (auto channel : {OutputChannel::Export, OutputChannel::Clipboard}) {
            const QJsonObject declared = declaredPolicy.sanitize(record(), channel);
            QCOMPARE(declared.value(QStringLiteral("secret")).toString(), SensitiveDataPolicy::redactionText());
            QCOMPARE(declared.value(QStringLiteral("label")).toString(), QStringLiteral("visible"));

            const QJsonObject undeclared = undeclaredPolicy.sanitize(extra, channel);
            QCOMPARE(undeclared.value(QStringLiteral("apiKey")).toString(), SensitiveDataPolicy::redactionText());
            QCOMPARE(undeclared.value(QStringLiteral("name")).toString(), QStringLiteral("Alex"));

            QCOMPARE(declaredPolicy.handling(QStringLiteral("secret"), QJsonValue(QStringLiteral("x")), channel), FieldHandling::Redact);
            QCOMPARE(declaredPolicy.handling(QStringLiteral("secret"), QJsonValue(QStringLiteral("x")), channel, domain::ReleaseAuthorization::none()), FieldHandling::Redact);
        }
    }

    // 4. After explicit authorization for THAT channel, values pass; nothing else is unlocked.
    void explicitAuthorizationReleasesOnlyForItsOwnChannel()
    {
        const SensitiveDataPolicy policy(fields());
        const auto exportAuth = domain::ReleaseAuthorization::userConfirmed(OutputChannel::Export);
        const auto clipboardAuth = domain::ReleaseAuthorization::userConfirmed(OutputChannel::Clipboard);

        QCOMPARE(policy.sanitize(record(), OutputChannel::Export, exportAuth).value(QStringLiteral("secret")).toString(),
                 QStringLiteral("hunter2-xyz"));
        QCOMPARE(policy.sanitize(record(), OutputChannel::Clipboard, clipboardAuth).value(QStringLiteral("secret")).toString(),
                 QStringLiteral("hunter2-xyz"));

        // An export authorization does not release to the clipboard, and vice versa.
        QCOMPARE(policy.sanitize(record(), OutputChannel::Clipboard, exportAuth).value(QStringLiteral("secret")).toString(),
                 SensitiveDataPolicy::redactionText());
        QCOMPARE(policy.sanitize(record(), OutputChannel::Export, clipboardAuth).value(QStringLiteral("secret")).toString(),
                 SensitiveDataPolicy::redactionText());
    }

    void authorizationCannotUnlockLogsDisplayOrPersistence()
    {
        const SensitiveDataPolicy policy(fields());
        for (auto granted : {OutputChannel::Export, OutputChannel::Clipboard, OutputChannel::Log,
                             OutputChannel::Display, OutputChannel::Persistence, OutputChannel::Workspace}) {
            const auto authorization = domain::ReleaseAuthorization::userConfirmed(granted);
            QCOMPARE(policy.sanitize(record(), OutputChannel::Log, authorization).value(QStringLiteral("secret")).toString(),
                     SensitiveDataPolicy::redactionText());
            QCOMPARE(policy.sanitize(record(), OutputChannel::Display, authorization).value(QStringLiteral("secret")).toString(),
                     SensitiveDataPolicy::maskText());
            QVERIFY(!policy.sanitize(record(), OutputChannel::Persistence, authorization).contains(QStringLiteral("secret")));
            QVERIFY(!policy.sanitize(record(), OutputChannel::Workspace, authorization).contains(QStringLiteral("secret")));
        }
        QVERIFY(!domain::ReleaseAuthorization::userConfirmed(OutputChannel::Log).permits(OutputChannel::Log));
        QVERIFY(!domain::ReleaseAuthorization().permits(OutputChannel::Export));
    }

    void recordsWithOnlyDeclaredNonSensitiveFieldsPassWithoutConfirmation()
    {
        const SensitiveDataPolicy policy(fields());
        const QJsonObject harmless{{QStringLiteral("label"), QStringLiteral("visible")}};
        QVERIFY(!policy.requiresConfirmation(harmless, OutputChannel::Export));
        QCOMPARE(policy.sanitize(harmless, OutputChannel::Export), harmless);
        QVERIFY(policy.requiresConfirmation(record(), OutputChannel::Clipboard));
    }

    // ---- nested JSON: flat-record contract, whole-value handling, NO recursion ----------------
    void nestedValuesUnderADeclaredNonSensitiveFieldAreTreatedAsSensitiveAsAWhole()
    {
        const SensitiveDataPolicy policy(
            QList<domain::OutputFieldSpec>{{QStringLiteral("meta"), QStringLiteral("Meta"), false},
                                           {QStringLiteral("name"), QStringLiteral("Name"), false}});
        const QJsonObject record{{QStringLiteral("name"), QStringLiteral("Alex")},
                                 {QStringLiteral("meta"), QJsonObject{{QStringLiteral("token"), QStringLiteral("tok-1")},
                                                                       {QStringLiteral("deep"), QJsonObject{{QStringLiteral("x"), 1}}}}}};
        // Log: the whole subtree is redacted, not just keys that look sensitive.
        const QJsonObject logged = policy.sanitize(record, OutputChannel::Log);
        QCOMPARE(logged.value(QStringLiteral("meta")).toString(), SensitiveDataPolicy::redactionText());
        QCOMPARE(logged.value(QStringLiteral("name")).toString(), QStringLiteral("Alex"));
        QVERIFY(!QJsonDocument(logged).toJson().contains("tok-1"));
        QVERIFY(!QJsonDocument(logged).toJson().contains("deep"));
        // Display masks, persistence/workspace omit, export/clipboard redact without authorization.
        QCOMPARE(policy.sanitize(record, OutputChannel::Display).value(QStringLiteral("meta")).toString(), SensitiveDataPolicy::maskText());
        QVERIFY(!policy.sanitize(record, OutputChannel::Persistence).contains(QStringLiteral("meta")));
        QVERIFY(!policy.sanitize(record, OutputChannel::Workspace).contains(QStringLiteral("meta")));
        for (auto channel : {OutputChannel::Export, OutputChannel::Clipboard}) {
            QVERIFY(policy.requiresConfirmation(record, channel));
            QCOMPARE(policy.sanitize(record, channel).value(QStringLiteral("meta")).toString(), SensitiveDataPolicy::redactionText());
            // only an explicit authorization releases the whole subtree, unchanged
            const auto released = policy.sanitize(record, channel, domain::ReleaseAuthorization::userConfirmed(channel));
            QCOMPARE(released.value(QStringLiteral("meta")).toObject().value(QStringLiteral("token")).toString(), QStringLiteral("tok-1"));
        }
    }

    void arraysAreTreatedLikeObjects()
    {
        const SensitiveDataPolicy policy(QList<domain::OutputFieldSpec>{{QStringLiteral("tags"), QStringLiteral("Tags"), false}});
        const QJsonObject record{{QStringLiteral("tags"), QJsonArray{QStringLiteral("a"), QStringLiteral("b")}}};
        QCOMPARE(policy.sanitize(record, OutputChannel::Log).value(QStringLiteral("tags")).toString(), SensitiveDataPolicy::redactionText());
        QVERIFY(policy.requiresConfirmation(record, OutputChannel::Export));
    }

    void sensitivityNeverLooksInsideValues()
    {
        // The policy decides per top-level key. A declared SENSITIVE field that holds an object is
        // handled as a whole too; and nested keys that happen to share a name with other declared
        // fields do not change anything (there is no recursion to rely on).
        const SensitiveDataPolicy policy(fields());
        const QJsonObject record{{QStringLiteral("secret"), QJsonObject{{QStringLiteral("label"), QStringLiteral("visible")}}},
                                 {QStringLiteral("label"), QStringLiteral("ok")}};
        const QJsonObject logged = policy.sanitize(record, OutputChannel::Log);
        QCOMPARE(logged.value(QStringLiteral("secret")).toString(), SensitiveDataPolicy::redactionText());
        QCOMPARE(logged.value(QStringLiteral("label")).toString(), QStringLiteral("ok"));
    }

    void scalarsOfEveryJsonTypeUnderNonSensitiveFieldsPassUntouched()
    {
        const SensitiveDataPolicy policy(QList<domain::OutputFieldSpec>{{QStringLiteral("s"), QStringLiteral("S"), false},
                                                                         {QStringLiteral("n"), QStringLiteral("N"), false},
                                                                         {QStringLiteral("b"), QStringLiteral("B"), false},
                                                                         {QStringLiteral("z"), QStringLiteral("Z"), false}});
        const QJsonObject record{{QStringLiteral("s"), QStringLiteral("text")}, {QStringLiteral("n"), 1.5},
                                 {QStringLiteral("b"), true}, {QStringLiteral("z"), QJsonValue::Null}};
        QCOMPARE(policy.sanitize(record, OutputChannel::Export), record);
        QVERIFY(!policy.requiresConfirmation(record, OutputChannel::Export));
    }

    void validateRecordEnforcesTheFlatDeclaredContract()
    {
        auto md = testing::StubGenerator::makeMetadata(QStringLiteral("flat"));
        QVERIFY(domain::validateRecord(md, QJsonObject{{QStringLiteral("value"), 1}}).isValid());
        const auto extra = domain::validateRecord(md, QJsonObject{{QStringLiteral("value"), 1}, {QStringLiteral("oops"), 2}});
        QVERIFY(extra.contains(QStringLiteral("oops"), QStringLiteral("undeclared_field")));
        const auto nested = domain::validateRecord(md, QJsonObject{{QStringLiteral("value"), QJsonObject{{QStringLiteral("a"), 1}}}});
        QVERIFY(nested.contains(QStringLiteral("value"), QStringLiteral("nested_value")));
        const auto array = domain::validateRecord(md, QJsonObject{{QStringLiteral("value"), QJsonArray{1}}});
        QVERIFY(array.contains(QStringLiteral("value"), QStringLiteral("nested_value")));
    }

    void undeclaredFieldsFailClosed()
    {
        const SensitiveDataPolicy policy(fields());
        QVERIFY(policy.isSensitive(QStringLiteral("forgotten")));
        const QJsonObject out = policy.sanitize({{QStringLiteral("forgotten"), QStringLiteral("x")}},
                                                OutputChannel::Log);
        QCOMPARE(out.value(QStringLiteral("forgotten")).toString(), SensitiveDataPolicy::redactionText());
    }

    // Handling depends on metadata only, not on a generator id such as "password".
    void behavesIdenticallyForAnyGenerator()
    {
        const auto md = testing::StubGenerator::makeMetadata(QStringLiteral("api-token"), /*sensitive=*/true);
        const auto policy = SensitiveDataPolicy::forGenerator(md);
        const QJsonObject out = policy.sanitize({{QStringLiteral("value"), QStringLiteral("tok_123")}},
                                                OutputChannel::Persistence);
        QVERIFY(out.isEmpty());
    }

    void listOverloadSanitizesEveryRecord()
    {
        const SensitiveDataPolicy policy(fields());
        const auto out = policy.sanitize(QList<QJsonObject>{record(), record()}, OutputChannel::Log);
        QCOMPARE(out.size(), 2);
        for (const auto& r : out)
            QCOMPARE(r.value(QStringLiteral("secret")).toString(), SensitiveDataPolicy::redactionText());
    }
};

QTEST_APPLESS_MAIN(TstSensitive)
#include "tst_sensitive.moc"
