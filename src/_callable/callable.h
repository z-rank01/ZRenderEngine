#ifndef CALLABLE_H
#define CALLABLE_H

#include <variant>
#include <string>

namespace Callable
{
    /// @brief Represents the result of a callable operation.
    /// @tparam T The type of the successful result (std::string for the error message).
    template<typename T>
    using Result = std::variant<T, std::string>;

    /// @brief Represents a callable object.
    /// @tparam F The type of the callable function.
    template<typename F>
    struct Callable
    {
        F func;

        /// @brief Applies the function to the input Result.
        /// @tparam T parameter type
        /// @param input The input Result to process.
        /// @return A Result containing either the function's output or an error message.
        template<typename T>
        auto operator()(Result<T>&& input) const {
            try {
                auto param = std::get_if<T>(&input);
                if (!param) {
                    return Result<decltype(func(std::get<T>(input)))>{std::string("Invalid type")};
                }
                return Result<decltype(func(*param))>{func(*param)};
            }
            catch (const std::exception& e) {
                return Result<decltype(func(std::get<T>(input)))>{e.what()};
            }
        }
    };

    /// @brief Pipes the input Result through the given Callable.
    /// @tparam T The type of the input Result.
    /// @tparam F The type of the Callable function.
    /// @param input The input Result to process.
    /// @param callable The Callable to apply.
    /// @return A Result containing either the Callable's output or an error message.
    /// @note - The input result must be of the same type as the Callable's input type.
    /// @note - The Callable's output type must be convertible to the input type of the next Callable.
    template<typename T, typename F>
    auto operator|(Result<T>&& input, const Callable<F>& callable) {
        return callable(std::move(input));
    }

    /// @brief Combines two Callables into a single Callable.
    /// @tparam F1 The type of the first Callable function.
    /// @tparam F2 The type of the second Callable function.
    /// @param left The first Callable to apply.
    /// @param right The second Callable to apply.
    /// @return A new Callable that applies the first Callable followed by the second.
    /// @note - The output type of the first Callable must match the input type of the second Callable.
    template<typename F1, typename F2>
    auto operator|(const Callable<F1>& left, const Callable<F2>& right) {
        return Callable{ [left, right](auto&& input) {
            return right(left(std::forward<decltype(input)>(input)));
        } };
    }
}

#endif // CALLABLE_H