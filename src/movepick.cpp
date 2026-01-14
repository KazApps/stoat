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

#include "movepick.h"

#include "see.h"

namespace stoat {
    Move MoveGenerator::next() {
        switch (m_stage) {
            case MovegenStage::kTtMove: {
                ++m_stage;

                if (m_ttMove && m_pos.isPseudolegal(m_ttMove)) {
                    return m_ttMove;
                }

                [[fallthrough]];
            }

            case MovegenStage::kGenerateNoisy: {
                movegen::generateNoisy(m_moves, m_pos);
                m_end = m_moves.size();

                scoreNoisy();

                ++m_stage;
                [[fallthrough]];
            }

            case MovegenStage::kGoodNoisy: {
                while (m_idx < m_end) {
                    const auto idx = findNext();
                    const auto move = m_moves[idx];

                    if (move == m_ttMove) {
                        continue;
                    }

                    if (see::see(m_pos, move, -m_scores[idx] / 4)) {
                        return move;
                    }

                    m_moves[m_badNoisyEnd++] = m_moves[idx];
                }

                ++m_stage;
                [[fallthrough]];
            }

            case MovegenStage::kGenerateQuiet: {
                if (!m_skipQuiet) {
                    movegen::generateQuiet(m_moves, m_pos);
                    m_end = m_moves.size();
                }

                scoreQuiet();

                ++m_stage;
                [[fallthrough]];
            }

            case MovegenStage::kQuiet: {
                if (!m_skipQuiet) {
                    if (const auto move = selectNext([this](Move move) { return move != m_ttMove; })) {
                        return move;
                    }
                }

                m_idx = 0;
                m_end = m_badNoisyEnd;

                ++m_stage;
                [[fallthrough]];
            }

            case MovegenStage::kBadNoisy: {
                if (const auto move = selectNext<false>([this](auto move) { return move != m_ttMove; })) {
                    return move;
                }

                m_stage = MovegenStage::kEnd;
                return kNullMove;
            }

            case MovegenStage::kQsearchGenerateCaptures: {
                movegen::generateCaptures(m_moves, m_pos);
                m_end = m_moves.size();

                scoreNoisy();

                ++m_stage;
                [[fallthrough]];
            }

            case MovegenStage::kQsearchCaptures: {
                if (const auto move = selectNext([](Move) { return true; })) {
                    return move;
                }

                m_stage = MovegenStage::kEnd;
                return kNullMove;
            }

            case MovegenStage::kQsearchEvasionsGenerateCaptures: {
                movegen::generateCaptures(m_moves, m_pos);
                m_end = m_moves.size();

                scoreNoisy();

                ++m_stage;
                [[fallthrough]];
            }

            case MovegenStage::kQsearchEvasionsCaptures: {
                if (const auto move = selectNext([](Move) { return true; })) {
                    return move;
                }

                ++m_stage;
                [[fallthrough]];
            }

            case MovegenStage::kQsearchEvasionsGenerateQuiet: {
                if (!m_skipQuiet) {
                    movegen::generateQuiet(m_moves, m_pos);
                    m_end = m_moves.size();
                }

                scoreQuiet();

                ++m_stage;
                [[fallthrough]];
            }

            case MovegenStage::kQsearchEvasionsQuiet: {
                if (!m_skipQuiet) {
                    if (const auto move = selectNext([this](Move move) { return move != m_ttMove; })) {
                        return move;
                    }
                }

                m_stage = MovegenStage::kEnd;
                return kNullMove;
            }

            default:
                return kNullMove;
        }
    }

    MoveGenerator MoveGenerator::main(
        const Position& pos,
        Move ttMove,
        const HistoryTables& history,
        std::span<ContinuationSubtable* const> continuations,
        i32 ply
    ) {
        assert(continuations.size() == kMaxDepth + 1);
        return MoveGenerator{MovegenStage::kTtMove, pos, ttMove, history, continuations, ply};
    }

    MoveGenerator MoveGenerator::qsearch(
        const Position& pos,
        const HistoryTables& history,
        std::span<ContinuationSubtable* const> continuations,
        i32 ply
    ) {
        assert(continuations.size() == kMaxDepth + 1);
        const auto initialStage =
            pos.isInCheck() ? MovegenStage::kQsearchEvasionsGenerateCaptures : MovegenStage::kQsearchGenerateCaptures;
        return MoveGenerator{initialStage, pos, kNullMove, history, continuations, ply};
    }

    MoveGenerator::MoveGenerator(
        MovegenStage initialStage,
        const Position& pos,
        Move ttMove,
        const HistoryTables& history,
        std::span<ContinuationSubtable* const> continuations,
        i32 ply
    ) :
            m_stage{initialStage},
            m_pos{pos},
            m_ttMove{ttMove},
            m_history{history},
            m_continuations{continuations},
            m_ply{ply} {}

    i32 MoveGenerator::scoreNoisy(Move move) {
        const auto captured = m_pos.pieceOn(move.to());
        const auto pieceValue = captured != Pieces::kNone ? see::pieceValue(captured.type()) : 0;

        return pieceValue + m_history.noisyScore(m_pos, move) / 4;
    }

    void MoveGenerator::scoreNoisy() {
        for (usize idx = m_idx; idx < m_end; ++idx) {
            m_scores[idx] = scoreNoisy(m_moves[idx]);
        }
    }

    i32 MoveGenerator::scoreQuiet(Move move) {
        return m_history.quietScore(m_continuations, m_ply, m_pos, move);
    }

    void MoveGenerator::scoreQuiet() {
        for (usize idx = m_idx; idx < m_end; ++idx) {
            m_scores[idx] = scoreQuiet(m_moves[idx]);
        }
    }

    usize MoveGenerator::findNext() {
        auto bestIdx = m_idx;
        auto bestScore = m_scores[m_idx];

        for (usize idx = m_idx + 1; idx < m_end; ++idx) {
            if (m_scores[idx] > bestScore) {
                bestIdx = idx;
                bestScore = m_scores[idx];
            }
        }

        if (bestIdx != m_idx) {
            std::swap(m_moves[m_idx], m_moves[bestIdx]);
            std::swap(m_scores[m_idx], m_scores[bestIdx]);
        }

        return m_idx++;
    }
} // namespace stoat
