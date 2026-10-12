#pragma once

#include <atomic>
#include <memory>

namespace account_recommendations::domain
{

/// Cooperative cancellation, project-owned.
///
/// Why not std::stop_token? libc++ (the standard library of Apple/LLVM toolchains)
/// only ships it behind -fexperimental-library (verified with libc++ 18), so relying
/// on it would break macOS/Clang builds. This type is 30 lines and portable.
///
/// Each CancellationSource owns its own flag (there is no global flag). Tokens are
/// cheap shared views of it; every method is thread-safe and noexcept.
class CancellationToken
{
public:
    /// A default token is never cancelled.
    CancellationToken() noexcept = default;

    [[nodiscard]] bool isCancelled() const noexcept
    {
        return m_state && m_state->load(std::memory_order_acquire);
    }

private:
    friend class CancellationSource;
    explicit CancellationToken(std::shared_ptr<const std::atomic<bool>> state) noexcept
        : m_state(std::move(state))
    {
    }

    std::shared_ptr<const std::atomic<bool>> m_state;
};

class CancellationSource
{
public:
    CancellationSource() : m_state(std::make_shared<std::atomic<bool>>(false)) {}

    [[nodiscard]] CancellationToken token() const noexcept { return CancellationToken(m_state); }
    void requestCancel() noexcept { m_state->store(true, std::memory_order_release); }
    [[nodiscard]] bool isCancelled() const noexcept { return m_state->load(std::memory_order_acquire); }

private:
    std::shared_ptr<std::atomic<bool>> m_state;
};

} // namespace account_recommendations::domain
