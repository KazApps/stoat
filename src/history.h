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
#include <utility>

#include "core.h"
#include "move.h"
#include "position.h"
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
        return static_cast<HistoryScore>(std::clamp(static_cast<i32>(depth * 823 - 300), 0, 2500));
    }

    class HistoryTables {
    public:
        void clear();

        [[nodiscard]] i32 mainNonCaptureScore(const Position& pos, Move move) const;

        [[nodiscard]] i32 nonCaptureScore(const Position& pos, std::span<const u64> keyHistory, Move move) const;

        void updateNonCaptureScore(const Position& pos, std::span<const u64> keyHistory, Move move, HistoryScore bonus);

        void updateNonCaptureConthistScore(const Position& pos, std::span<const u64> keyHistory, HistoryScore bonus);

        [[nodiscard]] i32 captureScore(Move move, PieceType captured) const;
        void updateCaptureScore(Move move, PieceType captured, HistoryScore bonus);

    private:
        static constexpr usize kContEntries = 66536;

        // [stm][promo][from][to]
        util::MultiArray<HistoryEntry, Colors::kCount, 2, Squares::kCount, Squares::kCount> m_nonCaptureNonDrop{};
        // [dropped piece][drop square]
        util::MultiArray<HistoryEntry, Pieces::kCount, Squares::kCount> m_drop{};

        // [promo][from][to][captured]
        util::MultiArray<HistoryEntry, 2, Squares::kCount, Squares::kCount, PieceTypes::kCount> m_capture{};

        std::array<HistoryEntry, kContEntries> m_cont{};
    };
} // namespace stoat
