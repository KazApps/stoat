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

#include "correction.h"

namespace stoat {
    void EvalCorrectionHistoryTable::clear() {
        std::memset(&m_castleTable, 0, sizeof(m_castleTable));
        std::memset(&m_cavalryTable, 0, sizeof(m_cavalryTable));
    }

    void EvalCorrectionHistoryTable::update(const Position& pos, i32 depth, Score searchScore, Score staticEval) {
        const auto bonus = std::clamp((searchScore - staticEval) * depth / 8, -kMaxBonus, kMaxBonus);
        m_castleTable[pos.stm().idx()][pos.castleKey() % kEntries].update(bonus);
        m_cavalryTable[pos.stm().idx()][pos.cavalryKey() % kEntries].update(bonus);
    }

    i32 EvalCorrectionHistoryTable::correction(const Position& pos) const {
        i32 correction{};

        correction += m_castleTable[pos.stm().idx()][pos.castleKey() % kEntries];
        correction += m_cavalryTable[pos.stm().idx()][pos.cavalryKey() % kEntries];

        return correction / 16;
    }

    void LmrCorrectionHistoryTable::clear() {
        std::memset(&m_lmrTable, 0, sizeof(m_lmrTable));
    }

    void LmrCorrectionHistoryTable::update(i32 depth, u32 moveNumber, i32 bestMoveReduction, i32 reduction) {
        moveNumber = std::min<u32>(moveNumber, kTableSize - 1);

        const auto bonus =
            std::clamp<i32>(bestMoveReduction + static_cast<i32>(moveNumber) - 1 - reduction, -kMaxBonus, kMaxBonus);
        m_lmrTable[depth][moveNumber].update(bonus);
    }

    i32 LmrCorrectionHistoryTable::correction(i32 depth, u32 moveNumber) const {
        moveNumber = std::min<u32>(moveNumber, kTableSize - 1);

        i32 correction{};

        correction += m_lmrTable[depth][moveNumber];

        return correction / 16;
    }
} // namespace stoat
