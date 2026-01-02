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

#include <bit>

#include "../arch.h"

namespace stoat::util {
    [[nodiscard]] constexpr i32 ctz(u128 v) {
        const auto [high, low] = fromU128(v);

        if (low == 0) {
            return 64 + std::countr_zero(high);
        } else {
            return std::countr_zero(low);
        }
    }

    [[nodiscard]] constexpr i32 clz(u128 v) {
        const auto [high, low] = fromU128(v);

        if (high == 0) {
            return 64 + std::countl_zero(low);
        } else {
            return std::countl_zero(high);
        }
    }

    [[nodiscard]] constexpr i32 popcount(u128 v) {
        const auto [high, low] = fromU128(v);
        return std::popcount(high) + std::popcount(low);
    }
} // namespace stoat::util
