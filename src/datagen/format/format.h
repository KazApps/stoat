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

#include "../../types.h"

#include <iostream>

#include "../../core.h"
#include "../../move.h"
#include "../../position.h"

namespace stoat::datagen::format {
    enum class Outcome : u8 {
        kBlackLoss = 0,
        kDraw,
        kBlackWin,
    };

    // new: P, L, N, S, B, R, G, K, +P, +L, +N, +S, +B, +R
    // old: P, +P, L, N, +L, +N, S, +S, G, B, R, +B, +R, K
    constexpr std::array kPieceTypeMap{0, 2, 3, 6, 9, 10, 8, 13, 1, 4, 5, 7, 11, 12};

    class IDataFormat {
    public:
        virtual ~IDataFormat() = default;

        virtual void startStandard() = 0;
        //TODO shogi960, arbitrary position

        virtual void pushUnscored(Move move) = 0;
        virtual void push(Move move, Score score) = 0;

        virtual usize writeAllWithOutcome(std::ostream& stream, Outcome outcome) = 0;
    };
} // namespace stoat::datagen::format
