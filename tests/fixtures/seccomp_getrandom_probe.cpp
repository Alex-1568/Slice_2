// Test helper (Linux x86-64): installs a seccomp filter that makes the getrandom(2) syscall
// fail with EPERM, then exercises the REAL operating-system entropy backend.
//
// Exit codes
//   10  backend reported the failure with errno EPERM          (required behaviour)
//   11  backend reported SUCCESS although getrandom was denied (a silent fallback exists: BAD)
//   12  backend reported a different failure
//   13  (--generate) production path (attested provider + ordinary context + real generator)
//       returned RandomSourceFailure and no records                       (required behaviour)
//   14  (--generate) anything else (e.g. passwords were produced)         (BAD)
//   20  (--qt-system) Qt's system generator returned normally under the same filter
//   77  filter could not be installed / unsupported platform   (test is skipped)
#include "domain/generator/GenerationContext.hpp"
#include "generators/password/PasswordGenerator.hpp"
#include "infrastructure/random/ISystemEntropy.hpp"
#include "infrastructure/random/SecureRandomProvider.hpp"

#include <QRandomGenerator>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>

#if defined(__linux__) && defined(__x86_64__)
#  include <linux/audit.h>
#  include <linux/filter.h>
#  include <linux/seccomp.h>
#  include <stddef.h>
#  include <sys/prctl.h>
#  include <sys/syscall.h>
#  include <unistd.h>

static bool denyGetrandom()
{
    sock_filter filter[] = {
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, arch)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 1, 0),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_getrandom, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    };
    sock_fprog program{static_cast<unsigned short>(sizeof filter / sizeof filter[0]), filter};
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0)
        return false;
    return prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program) == 0;
}
#else
static bool denyGetrandom() { return false; }
#endif

int main(int argc, char** argv)
{
    if (!denyGetrandom()) {
        std::puts("seccomp filter unavailable");
        return 77;
    }
    if (argc > 1 && std::strcmp(argv[1], "--qt-system") == 0) {
        const auto value = QRandomGenerator::system()->generate64();
        std::printf("qt-system-generator-returned-normally value=%llu\n", static_cast<unsigned long long>(value));
        return 20;
    }
    if (argc > 1 && std::strcmp(argv[1], "--generate") == 0) {
        using namespace account_recommendations;
        infrastructure::SecureRandomProvider provider; // the PRODUCTION provider
        const domain::GenerationContext context(provider); // an ORDINARY context
        domain::GeneratorRequest request;
        request.generatorId = QStringLiteral("password");
        request.count = 25;
        const auto result = generators::password::PasswordGenerator().generate(request, context);
        if (!result && result.error().code == domain::GenerationError::Code::RandomSourceFailure) {
            std::puts("production path reported RandomSourceFailure and returned no records");
            return 13;
        }
        std::puts(result ? "passwords were produced although getrandom was denied" : "unexpected error code");
        return 14;
    }
    using namespace account_recommendations::infrastructure;
    const auto source = makeOperatingSystemEntropy();
    std::array<std::byte, 16> block{};
    const auto result = source->fill(block.data(), block.size());
    if (result) {
        std::puts("backend reported success although getrandom was denied");
        return 11;
    }
    std::printf("backend=%s osCode=%d\n", source->backendName().toStdString().c_str(), result.error().osCode);
    return result.error().osCode == EPERM ? 10 : 12;
}
