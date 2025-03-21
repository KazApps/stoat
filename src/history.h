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

#include "types.h"

#include <algorithm>

#include "core.h"
#include "move.h"
#include "util/multi_array.h"

namespace stoat {
    using HistoryScore = i16;

    struct HistoryEntry {
        i16 value{};

        HistoryEntry() = default;
        HistoryEntry(HistoryScore v) :
                value{v} {}

        [[nodiscard]] inline operator HistoryScore() const {
            return value;
        }

        [[nodiscard]] inline HistoryEntry& operator=(HistoryScore v) {
            value = v;
            return *this;
        }

        inline void update(HistoryScore bonus) {
            value += bonus - value * std::abs(bonus) / 16384;
        }
    };

    [[nodiscard]] constexpr HistoryScore historyBonus(i32 depth) {
        return static_cast<HistoryScore>(std::clamp(depth * 300 - 300, 0, 2500));
    }

    class HistoryTables {
    public:
        void clear();

        [[nodiscard]] HistoryScore CaptureScore(Move move) const;
        void updateCaptureScore(Move move, HistoryScore bonus);
        [[nodiscard]] HistoryScore nonCaptureScore(Move move) const;
        void updateNonCaptureScore(Move move, HistoryScore bonus);

    private:
        // [promo][from][to]
        util::MultiArray<HistoryEntry, 2, Squares::kCount, Squares::kCount> m_Capture{};
        // [promo][from][to]
        util::MultiArray<HistoryEntry, 2, Squares::kCount, Squares::kCount> m_nonCaptureNonDrop{};
        // [dropped piece type][drop square]
        util::MultiArray<HistoryEntry, PieceTypes::kCount, Squares::kCount> m_drop{};
    };
} // namespace stoat
