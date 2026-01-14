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
    namespace {
        inline void updateConthist(
            std::span<ContinuationSubtable*> continuations,
            i32 ply,
            const Position& pos,
            Move move,
            HistoryScore bonus,
            i32 offset
        ) {
            if (offset > ply) {
                return;
            }

            if (auto* ptr = continuations[ply - offset]) {
                (*ptr)[{pos, move}].update(bonus);
            }
        }

        inline HistoryScore conthistScore(
            std::span<ContinuationSubtable* const> continuations,
            i32 ply,
            const Position& pos,
            Move move,
            i32 offset
        ) {
            if (offset > ply) {
                return 0;
            }

            if (const auto* ptr = continuations[ply - offset]) {
                return (*ptr)[{pos, move}];
            }

            return 0;
        }
    } // namespace

    void HistoryTables::clear() {
        std::memset(m_quietNonDrop.data(), 0, sizeof(m_quietNonDrop));
        std::memset(m_drop.data(), 0, sizeof(m_drop));
        std::memset(m_continuation.data(), 0, sizeof(m_continuation));
        std::memset(m_capture.data(), 0, sizeof(m_capture));
        std::memset(m_capture.data(), 0, sizeof(m_promotion));
    }

    i32 HistoryTables::mainQuietScore(Move move) const {
        if (move.isDrop()) {
            return m_drop[move.dropPiece().idx()][move.to().idx()];
        } else {
            return m_quietNonDrop[move.from().idx()][move.to().idx()];
        }
    }

    i32 HistoryTables::quietScore(
        std::span<ContinuationSubtable* const> continuations,
        i32 ply,
        const Position& pos,
        Move move
    ) const {
        i32 score{};

        if (move.isDrop()) {
            score += m_drop[move.dropPiece().idx()][move.to().idx()];
        } else {
            score += m_quietNonDrop[move.from().idx()][move.to().idx()];
        }

        score += conthistScore(continuations, ply, pos, move, 1);
        score += conthistScore(continuations, ply, pos, move, 2);
        score += conthistScore(continuations, ply, pos, move, 3);

        return score;
    }

    void HistoryTables::updateQuietScore(
        std::span<ContinuationSubtable*> continuations,
        i32 ply,
        const Position& pos,
        Move move,
        HistoryScore bonus
    ) {
        if (move.isDrop()) {
            m_drop[move.dropPiece().idx()][move.to().idx()].update(bonus);
        } else {
            m_quietNonDrop[move.from().idx()][move.to().idx()].update(bonus);
        }

        updateQuietConthistScore(continuations, ply, pos, move, bonus);
    }

    void HistoryTables::updateQuietConthistScore(
        std::span<ContinuationSubtable*> continuations,
        i32 ply,
        const Position& pos,
        Move move,
        HistoryScore bonus
    ) {
        updateConthist(continuations, ply, pos, move, bonus, 1);
        updateConthist(continuations, ply, pos, move, bonus, 2);
        updateConthist(continuations, ply, pos, move, bonus, 3);
    }

    i32 HistoryTables::noisyScore(const Position& pos, Move move) const {
        const auto captured = pos.pieceOn(move.to());

        if (captured != Pieces::kNone) {
            return m_capture[move.isPromo()][move.from().idx()][move.to().idx()][captured.type().idx()];
        } else {
            return m_promotion[pos.pieceOn(move.from()).type().idx()][move.from().idx()][move.to().idx()];
        }
    }

    void HistoryTables::updateNoisyScore(const Position& pos, Move move, HistoryScore bonus) {
        const auto captured = pos.pieceOn(move.to());

        if (captured != Pieces::kNone) {
            m_capture[move.isPromo()][move.from().idx()][move.to().idx()][captured.type().idx()].update(bonus);
        } else {
            m_promotion[pos.pieceOn(move.from()).type().idx()][move.from().idx()][move.to().idx()].update(bonus);
        }
    }
} // namespace stoat
