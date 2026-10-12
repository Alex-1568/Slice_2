#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace account_recommendations::common
{

/// Thrown only when a Result is accessed in the wrong state (a programming error).
class BadResultAccess final : public std::logic_error
{
public:
    using std::logic_error::logic_error;
};

/// Lightweight, project-owned value-or-error type (C++20 has no std::expected).
/// Expected failures (validation, limits, RNG failure) travel through Result;
/// exceptions are reserved for programming errors and exceptional sources.
template <typename T, typename E>
class [[nodiscard]] Result
{
    static_assert(!std::is_reference_v<T> && !std::is_reference_v<E>,
                  "Result does not support reference types");
    static_assert(!std::is_void_v<T>, "Use the Result<void, E> specialisation");

public:
    using ValueType = T;
    using ErrorType = E;

    [[nodiscard]] static Result success(T value)
    {
        return Result(std::in_place_index<0>, std::move(value));
    }

    [[nodiscard]] static Result failure(E error)
    {
        return Result(std::in_place_index<1>, std::move(error));
    }

    [[nodiscard]] bool hasValue() const noexcept { return m_storage.index() == 0; }
    explicit operator bool() const noexcept { return hasValue(); }

    [[nodiscard]] T& value() &
    {
        requireValue();
        return *std::get_if<0>(&m_storage);
    }
    [[nodiscard]] const T& value() const&
    {
        requireValue();
        return *std::get_if<0>(&m_storage);
    }
    [[nodiscard]] T&& value() &&
    {
        requireValue();
        return std::move(*std::get_if<0>(&m_storage));
    }

    [[nodiscard]] E& error() &
    {
        requireError();
        return *std::get_if<1>(&m_storage);
    }
    [[nodiscard]] const E& error() const&
    {
        requireError();
        return *std::get_if<1>(&m_storage);
    }
    [[nodiscard]] E&& error() &&
    {
        requireError();
        return std::move(*std::get_if<1>(&m_storage));
    }

    template <typename U>
    [[nodiscard]] T valueOr(U&& fallback) const&
    {
        return hasValue() ? *std::get_if<0>(&m_storage)
                          : static_cast<T>(std::forward<U>(fallback));
    }

private:
    template <std::size_t Index, typename Arg>
    Result(std::in_place_index_t<Index> tag, Arg&& arg)
        : m_storage(tag, std::forward<Arg>(arg))
    {
    }

    void requireValue() const
    {
        if (!hasValue())
            throw BadResultAccess("Result holds an error, not a value");
    }
    void requireError() const
    {
        if (hasValue())
            throw BadResultAccess("Result holds a value, not an error");
    }

    std::variant<T, E> m_storage;
};

template <typename E>
class [[nodiscard]] Result<void, E>
{
    static_assert(!std::is_reference_v<E>, "Result does not support reference types");

public:
    using ValueType = void;
    using ErrorType = E;

    [[nodiscard]] static Result success() { return Result(std::optional<E>{}); }
    [[nodiscard]] static Result failure(E error)
    {
        return Result(std::optional<E>(std::move(error)));
    }

    [[nodiscard]] bool hasValue() const noexcept { return !m_error.has_value(); }
    explicit operator bool() const noexcept { return hasValue(); }

    [[nodiscard]] E& error() &
    {
        requireError();
        return *m_error;
    }
    [[nodiscard]] const E& error() const&
    {
        requireError();
        return *m_error;
    }
    [[nodiscard]] E&& error() &&
    {
        requireError();
        return std::move(*m_error);
    }

private:
    explicit Result(std::optional<E> error) : m_error(std::move(error)) {}

    void requireError() const
    {
        if (hasValue())
            throw BadResultAccess("Result holds success, not an error");
    }

    std::optional<E> m_error;
};

} // namespace account_recommendations::common
