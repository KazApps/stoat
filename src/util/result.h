/*
 * Stoat, a USI shogi engine
 * Copyright (C) 2025 Ciekce
 *
 * Stoat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Stoat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Stoat. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "../types.h"

#include <stdexcept>
#include <variant>

namespace stoat::util {
    template <typename T>
    class OkHelper {
    public:
        [[nodiscard]] T&& value() {
            return std::move(m_value);
        }

    private:
        template <typename... Args>
        explicit OkHelper(Args&&... args) :
                m_value{std::forward<Args>(args)...} {}

        T m_value;

        template <typename O, typename... Args>
        friend OkHelper<O> ok(Args&&... args);

        template <typename O>
        friend OkHelper<O> ok(O v);
    };

    template <typename T, typename... Args>
    [[nodiscard]] OkHelper<T> ok(Args&&... args) {
        return OkHelper<T>(std::forward<Args>(args)...);
    }

    template <typename T>
    [[nodiscard]] OkHelper<T> ok(T v) {
        return OkHelper<T>(std::move(v));
    }

    template <typename E>
    class ErrHelper {
    public:
        [[nodiscard]] E&& value() {
            return std::move(m_value);
        }

    private:
        template <typename... Args>
        explicit ErrHelper(Args&&... args) :
                m_value{std::forward<Args>(args)...} {}

        E m_value;

        template <typename O, typename... Args>
        friend ErrHelper<O> err(Args&&... args);

        template <typename O>
        friend ErrHelper<O> err(O v);
    };

    template <typename E, typename... Args>
    [[nodiscard]] ErrHelper<E> err(Args&&... args) {
        return ErrHelper<E>(std::forward<Args>(args)...);
    }

    template <typename E>
    [[nodiscard]] ErrHelper<E> err(E v) {
        return ErrHelper<E>(std::move(v));
    }

    struct BadResultException : public std::logic_error {
        explicit BadResultException(const char* message) :
                std::logic_error{message} {}
    };

    template <typename T, typename E>
    class Result {
    public:
        Result(OkHelper<T> v) : // NOLINT(google-explicit-constructor)
                m_value{std::move(v.value())} {}

        Result(ErrHelper<E> v) : // NOLINT(google-explicit-constructor)
                m_value{ErrContainer{std::move(v.value())}} {}

        template <typename... Args>
        [[nodiscard]] static constexpr Result<T, E> ofOk(Args&&... args) {
            return Result<T, E>{std::forward<Args>(args)...};
        }

        template <typename... Args>
        [[nodiscard]] static constexpr Result<T, E> ofErr(Args&&... args) {
            return Result<T, E>{ErrTag, std::forward<Args>(args)...};
        }

        [[nodiscard]] constexpr bool ok() const {
            return std::holds_alternative<T>(m_value);
        }

        [[nodiscard]] constexpr bool err() const {
            return std::holds_alternative<ErrContainer>(m_value);
        }

        [[nodiscard]] constexpr bool empty() const {
            return std::holds_alternative<EmptyType>(m_value);
        }

        [[nodiscard]] explicit constexpr operator bool() const {
            return ok();
        }

        [[nodiscard]] T take() {
            if (!ok()) {
                throw BadResultException{"Attempted to take from a non-ok Result"};
            }

            auto v = std::move(std::get<T>(m_value));
            m_value = Empty;
            return v;
        }

        [[nodiscard]] E takeErr() {
            if (!err()) {
                throw BadResultException{"Attempted to takeErr from a non-error Result"};
            }

            auto v = std::move(std::get<ErrContainer>(m_value).value);
            m_value = Empty;
            return v;
        }

        template <typename FT, typename FE>
        void consume(FT okFunc, FE errFunc) {
            if (empty()) {
                throw BadResultException{"Attempted to consume an empty Result"};
            }

            if (ok()) {
                okFunc(take());
            } else {
                errFunc(takeErr());
            }
        }

    private:
        struct ErrTagType {};
        static constexpr ErrTagType ErrTag{};

        struct ErrContainer {
            E value;

            template <typename... Args>
            explicit constexpr ErrContainer(Args&&... args) :
                    value{std::forward<Args>(args)...} {}
        };

        struct EmptyType {};
        static constexpr EmptyType Empty{};

        template <typename... Args>
        explicit constexpr Result(Args&&... args) :
                m_value{std::in_place_type<T>, std::forward<Args>(args)...} {};

        template <typename... Args>
        explicit constexpr Result(ErrTagType, Args&&... args) :
                m_value{std::in_place_type<ErrContainer>, std::forward<Args>(args)...} {};

        std::variant<T, ErrContainer, EmptyType> m_value;
    };
} // namespace stoat::util
