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

#include "history.h"

#include <cstring>

namespace stoat {
    void HistoryTables::clear() {
        std::memset(m_nonCaptureNonDrop.data(), 0, sizeof(m_nonCaptureNonDrop));
        std::memset(m_drop.data(), 0, sizeof(m_drop));
        std::memset(m_capture.data(), 0, sizeof(m_capture));
        std::memset(m_cont.data(), 0, sizeof(m_cont));
    }

    i32 HistoryTables::mainNonCaptureScore(const Position& pos, Move move) const {
        if (move.isDrop()) {
            return m_drop[move.dropPiece().withColor(pos.stm()).idx()][move.to().idx()];
        } else {
            return m_nonCaptureNonDrop[pos.stm().idx()][move.isPromo()][move.from().idx()][move.to().idx()];
        }
    }

    i32 HistoryTables::nonCaptureScore(const Position& pos, std::span<const u64> keyHistory, Move move) const {
        i32 score{};

        if (move.isDrop()) {
            score += m_drop[move.dropPiece().withColor(pos.stm()).idx()][move.to().idx()];
        } else {
            score += m_nonCaptureNonDrop[pos.stm().idx()][move.isPromo()][move.from().idx()][move.to().idx()];
        }

        const auto conthistScore = [&](const u64 offset) {
            if (keyHistory.size() >= offset) {
                return m_cont[(pos.key() ^ keyHistory[keyHistory.size() - offset]) % kContEntries].value;
            } else {
                return static_cast<HistoryScore>(0);
            }
        };

        score += conthistScore(1);
        score += conthistScore(2);
        score += conthistScore(3);

        return score;
    }

    void HistoryTables::updateNonCaptureScore(
        const Position& pos,
        std::span<const u64> keyHistory,
        Move move,
        HistoryScore bonus
    ) {
        if (move.isDrop()) {
            m_drop[move.dropPiece().withColor(pos.stm()).idx()][move.to().idx()].update(bonus);
        } else {
            m_nonCaptureNonDrop[pos.stm().idx()][move.isPromo()][move.from().idx()][move.to().idx()].update(bonus);
        }

        updateNonCaptureConthistScore(pos, keyHistory, bonus);
    }

    void HistoryTables::updateNonCaptureConthistScore(
        const Position& pos,
        std::span<const u64> keyHistory,
        HistoryScore bonus
    ) {
        const auto updateConthist = [&](const u64 offset) {
            if (keyHistory.size() >= offset) {
                m_cont[(pos.key() ^ keyHistory[keyHistory.size() - offset]) % kContEntries].update(bonus);
            }
        };

        updateConthist(1);
        updateConthist(2);
        updateConthist(3);
    }

    i32 HistoryTables::captureScore(Move move, PieceType captured) const {
        return m_capture[move.isPromo()][move.from().idx()][move.to().idx()][captured.idx()];
    }

    void HistoryTables::updateCaptureScore(Move move, PieceType captured, HistoryScore bonus) {
        m_capture[move.isPromo()][move.from().idx()][move.to().idx()][captured.idx()].update(bonus);
    }
} // namespace stoat
