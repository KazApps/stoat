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

#include <cmath>

namespace stoat {
    void CorrectionHistory::clear() {
        std::memset(&m_castle, 0, sizeof(m_castle));
        std::memset(&m_cavalry, 0, sizeof(m_cavalry));
        std::memset(&m_hand, 0, sizeof(m_hand));
        std::memset(&m_kpr, 0, sizeof(m_kpr));
        std::memset(&m_cont, 0, sizeof(m_cont));
    }

    void CorrectionHistory::update(
        const Position& pos,
        std::span<const u64> keyHistory,
        i32 depth,
        Score searchScore,
        Score staticEval,
        i32 complexity
    ) {
        const double factor = 1.0 + std::log2(complexity + 1) / 10.0;

        const auto bonus =
            std::clamp(static_cast<i32>((searchScore - staticEval) * depth / 8 * factor), -kMaxBonus, kMaxBonus);
        const auto black_bonus = pos.stm() == Colors::kBlack ? bonus : -bonus;

        m_castle[pos.castleKey() % kEntries].update(black_bonus);
        m_cavalry[pos.cavalryKey() % kEntries].update(black_bonus);
        m_hand[pos.kingHandKey() % kEntries].update(black_bonus);
        m_kpr[pos.kprKey() % kEntries].update(black_bonus);

        const auto updateCont = [&](const u64 offset) {
            if (keyHistory.size() >= offset) {
                m_cont[(pos.key() ^ keyHistory[keyHistory.size() - offset]) % kEntries].update(bonus);
            }
        };

        updateCont(1);
        updateCont(2);
    }

    i32 CorrectionHistory::correction(const Position& pos, std::span<const u64> keyHistory) const {
        i32 correction{};

        correction += 128 * m_castle[pos.castleKey() % kEntries];
        correction += 128 * m_cavalry[pos.cavalryKey() % kEntries];
        correction += 128 * m_hand[pos.kingHandKey() % kEntries];
        correction += 128 * m_kpr[pos.kprKey() % kEntries];

        correction = pos.stm() == Colors::kBlack ? correction : -correction;

        const auto applyCont = [&](const u64 offset) {
            if (keyHistory.size() >= offset) {
                correction += 128 * m_cont[(pos.key() ^ keyHistory[keyHistory.size() - offset]) % kEntries];
            }
        };

        applyCont(1);
        applyCont(2);

        return correction / 2048;
    }
} // namespace stoat
