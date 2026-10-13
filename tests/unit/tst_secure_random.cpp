// Contract, trust-boundary and fault-injection tests for the fail-closed operating-system
// randomness path.
#include "TestSupport.hpp"
#include "generators/password/PasswordGenerator.hpp"
#include "infrastructure/random/SecureRandomProvider.hpp"
#include "testing/DeterministicRandomProvider.hpp"

#include <QProcess>
#include <QtTest>

#include <memory>
#include <type_traits>

using namespace account_recommendations;
using domain::GenerationContext;
using domain::GenerationError;
using generators::password::PasswordGenerator;
using infrastructure::SecureRandomProvider;
using testing::FaultInjectingEntropy;
using testing::passwordRequest;

// ---- compile-time trust boundary (these fail the BUILD, not just a test, if weakened) ----
// Ordinary code cannot mint or copy the attestation token ...
static_assert(!std::is_default_constructible_v<domain::SecureAttestation>);
static_assert(!std::is_copy_constructible_v<domain::SecureAttestation>);
static_assert(!std::is_move_constructible_v<domain::SecureAttestation>);
// ... and the production provider cannot be built around a custom entropy source.
static_assert(!std::is_constructible_v<SecureRandomProvider, std::shared_ptr<infrastructure::ISystemEntropy>>);
static_assert(std::is_default_constructible_v<SecureRandomProvider>);

namespace
{
/// A provider that "pretends" to be secure by declaring its own function of the same name.
class LyingProvider final : public domain::IRandomProvider
{
public:
    std::uint64_t nextUInt64() override { return 0x123456789ABCDEFULL; }
    bool isCryptographicallySecure() const noexcept { return true; } // hides, does not override
};
class CopyProbeProvider final : public domain::IRandomProvider
{
public:
    std::uint64_t nextUInt64() override { return 0xA5A5A5A5A5A5A5A5ULL; }
};
} // namespace

class TstSecureRandom : public QObject
{
    Q_OBJECT

private slots:
    // ---- the real operating-system backend ----------------------------------------
    void operatingSystemBackendDeliversDistinctBlocks()
    {
        const auto source = infrastructure::makeOperatingSystemEntropy();
        QVERIFY(!source->backendName().isEmpty());
        std::array<std::byte, 32> a{};
        std::array<std::byte, 32> b{};
        QVERIFY(source->fill(a.data(), a.size()).hasValue());
        QVERIFY(source->fill(b.data(), b.size()).hasValue());
        QVERIFY(a != b);                       // 2^-256 chance of a false failure
        const std::array<std::byte, 32> zeros{};
        QVERIFY(a != zeros);
        QVERIFY(source->fill(a.data(), 0).hasValue()); // zero-length request is a no-op, not an error
    }

    void defaultProviderIsAttestedAndPassesItsStartupProbe()
    {
        SecureRandomProvider provider;
        QVERIFY(provider.isCryptographicallySecure());
        QVERIFY(provider.probe().hasValue());
        const GenerationContext context(provider);
        QVERIFY(PasswordGenerator().generate(passwordRequest({}, 2), context).hasValue());
    }

    // ---- trust boundary: nobody can talk the framework into trusting a provider ----------
    void aProviderWithAnInjectedSourceIsNeverAttestedRegardlessOfWhatTheSourceDoes()
    {
        auto source = std::make_shared<FaultInjectingEntropy>(1000);
        auto provider = testing::providerWithInjectedSource(source);
        QVERIFY(!provider.isCryptographicallySecure());

        const GenerationContext production(provider);
        const auto result = PasswordGenerator().generate(passwordRequest({}, 3), production);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, GenerationError::Code::InsecureRandomSource);
        QCOMPARE(source->calls(), std::size_t{0}); // rejected BEFORE drawing anything
    }

    void untrustedDerivedProvidersCannotCopyOrMoveTheAttestationBase()
    {
        // With a deleted base copy/move operation, the implicitly-generated operations
        // on a concrete derived provider are also unavailable. A regression that restores
        // a defaulted base copy/assignment makes these runtime assertions fail.
        QVERIFY(!std::is_copy_constructible_v<CopyProbeProvider>);
        QVERIFY(!std::is_copy_assignable_v<CopyProbeProvider>);
        QVERIFY(!std::is_move_constructible_v<CopyProbeProvider>);
        QVERIFY(!std::is_move_assignable_v<CopyProbeProvider>);

        CopyProbeProvider custom;
        QVERIFY(!custom.isCryptographicallySecure());
        const GenerationContext context(custom);
        const auto result = PasswordGenerator().generate(passwordRequest({}, 1), context);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, GenerationError::Code::InsecureRandomSource);
    }

    void aCustomProviderCannotClaimSecurityBySayingSo()
    {
        LyingProvider lying;
        domain::IRandomProvider& asGenerators_see_it = lying;
        QVERIFY(!asGenerators_see_it.isCryptographicallySecure()); // the base, non-virtual value
        const GenerationContext production(lying);
        const auto result = PasswordGenerator().generate(passwordRequest({}, 1), production);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code, GenerationError::Code::InsecureRandomSource);
    }

    void testDoublesAreNeverAttested()
    {
        testing::ScriptedRandomProvider scripted({});
        testing::ConstantRandomProvider constant(1);
        testing::FailingRandomProvider failing;
        testing::DeterministicRandomProvider deterministic(1);
        for (domain::IRandomProvider* provider : std::initializer_list<domain::IRandomProvider*>{&scripted, &constant, &failing, &deterministic}) {
            QVERIFY(!provider->isCryptographicallySecure());
            const GenerationContext production(*provider);
            QCOMPARE(PasswordGenerator().generate(passwordRequest({}, 1), production).error().code,
                     GenerationError::Code::InsecureRandomSource);
        }
    }

    void nullInjectedSourceIsAContractViolation()
    {
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 static_cast<void>(testing::providerWithInjectedSource(nullptr)));
    }

    // ---- fault injection: the source fails (through the test-support seam) ---------------
    void sourceFailureSurfacesAsRandomSourceErrorNeverAValue()
    {
        auto provider = testing::providerWithInjectedSource(std::make_shared<FaultInjectingEntropy>(0, 13));
        QVERIFY_THROWS_EXCEPTION(domain::RandomSourceError, static_cast<void>(provider.nextUInt64()));
        QVERIFY_THROWS_EXCEPTION(domain::RandomSourceError, static_cast<void>(provider.uniformIndex(10)));
        QVERIFY(!provider.probe().hasValue());
        QCOMPARE(provider.probe().error().osCode, 13);
    }

    void generatorStopsAtTheFirstFailureAndNeverRetriesOrFallsBack()
    {
        for (std::size_t failAfter : {std::size_t{0}, std::size_t{1}, std::size_t{37}}) {
            auto source = std::make_shared<FaultInjectingEntropy>(failAfter);
            auto provider = testing::providerWithInjectedSource(source);
            const auto context = testing::insecureContext(provider); // explicit test-mode context
            const auto result = PasswordGenerator().generate(passwordRequest({{"length", 16}}, 500), context);
            QVERIFY(!result.hasValue());
            QCOMPARE(result.error().code, GenerationError::Code::RandomSourceFailure);
            // exactly `failAfter` successful draws plus the single failing call: no retry, no second source
            QCOMPARE(source->calls(), failAfter + 1);
        }
    }

    void failureMidBatchReturnsNoPartialPasswords()
    {
        auto source = std::make_shared<FaultInjectingEntropy>(200);
        auto provider = testing::providerWithInjectedSource(source);
        const auto context = testing::insecureContext(provider);
        std::size_t delivered = 0;
        testing::LambdaSink sink([&](domain::GeneratedRecord&&) {
            ++delivered;
            return domain::SinkResponse::Continue;
        });
        const auto summary = PasswordGenerator().generateInto(passwordRequest({{"length", 8}}, 1000), context, sink);
        QVERIFY(!summary.hasValue());
        QCOMPARE(summary.error().code, GenerationError::Code::RandomSourceFailure);
        QVERIFY(delivered > 0); // streaming consumers saw records produced before the fault ...
        auto again = testing::providerWithInjectedSource(std::make_shared<FaultInjectingEntropy>(200));
        const auto result = PasswordGenerator().generate(passwordRequest({{"length", 8}}, 1000), testing::insecureContext(again));
        QVERIFY(!result.hasValue()); // ... and the convenience API returns none
    }

    // ---- fault injection against the REAL syscall (separate process, Linux x86-64) ------
    void realBackendReportsAFailedGetrandomSyscallInsteadOfFallingBack()
    {
#if defined(Q_OS_LINUX)
        QProcess probe;
        probe.start(QStringLiteral(AR_SECCOMP_PROBE), {});
        QVERIFY(probe.waitForFinished(20000));
        if (probe.exitCode() == 77)
            QSKIP("seccomp filter could not be installed in this environment");
        const QString output = QString::fromUtf8(probe.readAllStandardOutput());
        QVERIFY2(probe.exitCode() == 10, qPrintable(QStringLiteral("exit %1: %2").arg(probe.exitCode()).arg(output)));
        QVERIFY(output.contains(QStringLiteral("getrandom")));
#else
        QSKIP("seccomp fault injection is Linux-only");
#endif
    }

    // The PRODUCTION path end to end: attested provider + ordinary context + real generator,
    // with the real getrandom syscall failing. Must yield RandomSourceFailure and no records.
    void productionPathUnderARealSyscallFailureReturnsAnErrorAndNoPasswords()
    {
#if defined(Q_OS_LINUX)
        QProcess probe;
        probe.start(QStringLiteral(AR_SECCOMP_PROBE), {QStringLiteral("--generate")});
        QVERIFY(probe.waitForFinished(30000));
        if (probe.exitCode() == 77)
            QSKIP("seccomp filter could not be installed in this environment");
        const QString output = QString::fromUtf8(probe.readAllStandardOutput());
        QVERIFY2(probe.exitCode() == 13, qPrintable(QStringLiteral("exit %1: %2").arg(probe.exitCode()).arg(output)));
#else
        QSKIP("seccomp fault injection is Linux-only");
#endif
    }

    // Observation, NOT a gate: documents (for the audit) how Qt's own system generator behaves
    // under the same fault. It is not used anywhere in src/.
    void observationHowQtSystemGeneratorBehavesUnderTheSameFault()
    {
#if defined(Q_OS_LINUX)
        QProcess probe;
        probe.start(QStringLiteral(AR_SECCOMP_PROBE), {QStringLiteral("--qt-system")});
        QVERIFY(probe.waitForFinished(20000));
        if (probe.exitCode() == 77)
            QSKIP("seccomp filter could not be installed in this environment");
        qInfo().noquote() << "Qt system generator under denied getrandom: exit" << probe.exitCode()
                          << QString::fromUtf8(probe.readAllStandardOutput()).trimmed();
#else
        QSKIP("Linux-only observation");
#endif
    }
};

QTEST_APPLESS_MAIN(TstSecureRandom)
#include "tst_secure_random.moc"
