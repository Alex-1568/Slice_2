#pragma once

#include <cstddef>
#include <string>

namespace account_recommendations::common
{

/// Best-effort zeroing of a working buffer. Volatile stores cannot be elided by
/// the optimiser. This only clears *this* buffer: copies made elsewhere (QString,
/// QJsonObject, clipboard, swap, crash dumps) are NOT covered. Treat it as
/// hygiene for internal scratch buffers, not as a security guarantee.
inline void secureWipe(char32_t* data, std::size_t count) noexcept
{
    volatile char32_t* p = data;
    for (std::size_t i = 0; i < count; ++i)
        p[i] = U'\0';
}

inline void secureWipe(std::u32string& text) noexcept
{
    secureWipe(text.data(), text.size());
    text.clear();
}

} // namespace account_recommendations::common
