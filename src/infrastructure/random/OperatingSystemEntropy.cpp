#include "infrastructure/random/ISystemEntropy.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>

// Exactly one backend per platform. There is deliberately NO fallback chain: if the
// platform call fails, the failure is reported to the caller. This file is the only
// place in src/ allowed to include platform headers (enforced by tst_security).
#if defined(__linux__)
#  include <sys/random.h>
#  include <sys/types.h>
#elif defined(__APPLE__)
#  include <sys/random.h>
#elif defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <bcrypt.h>
#else
#  error "No operating-system CSPRNG backend for this platform. Refusing to build with a weaker source."
#endif

namespace account_recommendations::infrastructure
{

namespace
{
using common::Result;
using VoidResult = Result<void, EntropyError>;

class OperatingSystemEntropy final : public ISystemEntropy
{
public:
    VoidResult fill(std::byte* data, std::size_t size) override
    {
#if defined(__linux__)
        std::size_t done = 0;
        int interrupts = 0;
        while (done < size) {
            const ssize_t got = ::getrandom(data + done, size - done, 0);
            if (got < 0) {
                const int code = errno;
                if (code == EINTR && ++interrupts < 1000)
                    continue;
                return VoidResult::failure(
                    {code, QStringLiteral("getrandom failed: %1").arg(QString::fromLocal8Bit(std::strerror(code)))});
            }
            if (got == 0)
                return VoidResult::failure({0, QStringLiteral("getrandom returned no data")});
            done += static_cast<std::size_t>(got);
        }
        return VoidResult::success();
#elif defined(__APPLE__)
        std::size_t done = 0;
        while (done < size) {
            const std::size_t chunk = std::min<std::size_t>(size - done, 256); // getentropy limit
            if (::getentropy(data + done, chunk) != 0) {
                const int code = errno;
                return VoidResult::failure(
                    {code, QStringLiteral("getentropy failed: %1").arg(QString::fromLocal8Bit(std::strerror(code)))});
            }
            done += chunk;
        }
        return VoidResult::success();
#elif defined(_WIN32)
        std::size_t done = 0;
        while (done < size) {
            const auto chunk = static_cast<ULONG>(std::min<std::size_t>(size - done, std::size_t{1} << 20));
            const NTSTATUS status = ::BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(data + done), chunk,
                                                      BCRYPT_USE_SYSTEM_PREFERRED_RNG);
            if (!BCRYPT_SUCCESS(status))
                return VoidResult::failure(
                    {static_cast<int>(status),
                     QStringLiteral("BCryptGenRandom failed (NTSTATUS 0x%1)")
                         .arg(static_cast<qulonglong>(static_cast<unsigned long>(status)), 8, 16, QLatin1Char('0'))});
            done += chunk;
        }
        return VoidResult::success();
#endif
    }

    QString backendName() const override
    {
#if defined(__linux__)
        return QStringLiteral("getrandom(2)");
#elif defined(__APPLE__)
        return QStringLiteral("getentropy(2)");
#else
        return QStringLiteral("BCryptGenRandom");
#endif
    }
};
} // namespace

std::unique_ptr<ISystemEntropy> makeOperatingSystemEntropy()
{
    return std::make_unique<OperatingSystemEntropy>();
}

} // namespace account_recommendations::infrastructure
