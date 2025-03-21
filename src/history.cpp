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
        std::memset(m_Capture.data(), 0, sizeof(m_Capture));
        std::memset(m_nonCaptureNonDrop.data(), 0, sizeof(m_nonCaptureNonDrop));
        std::memset(m_drop.data(), 0, sizeof(m_drop));
    }

    HistoryScore HistoryTables::nonCaptureScore(Move move) const {
        if (move.isDrop()) {
            return m_drop[move.dropPiece().idx()][move.to().idx()];
        } else {
            return m_nonCaptureNonDrop[move.isPromo()][move.from().idx()][move.to().idx()];
        }
    }

    void HistoryTables::updateNonCaptureScore(Move move, HistoryScore bonus) {
        if (move.isDrop()) {
            m_drop[move.dropPiece().idx()][move.to().idx()].update(bonus);
        } else {
            m_nonCaptureNonDrop[move.isPromo()][move.from().idx()][move.to().idx()].update(bonus);
        }
    }

    HistoryScore HistoryTables::CaptureScore(Move move) const {
        return m_Capture[move.isPromo()][move.from().idx()][move.to().idx()];
    }

    void HistoryTables::updateCaptureScore(Move move, HistoryScore bonus) {
        m_Capture[move.isPromo()][move.from().idx()][move.to().idx()].update(bonus);
    }
} // namespace stoat
