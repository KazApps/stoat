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

#include <utility>
#include <vector>

#include "format.h"

namespace stoat::datagen::format {
    template <typename ScoredMove>
    class StoatpackBase final : public IDataFormat {
    public:
        StoatpackBase() {
            m_unscoredMoves.reserve(16);
            m_moves.reserve(256);
        }

        ~StoatpackBase() final = default;

        void startStandard() final {
            m_unscoredMoves.clear();
            m_moves.clear();
        }

        void pushUnscored(Move move) final {
            assert(m_moves.empty());
            m_unscoredMoves.push_back(move.raw());
        }

        void push(Move move, Score staticEval, Score searchedScore) final {
            assert(std::abs(searchedScore) <= kScoreInf);

            if constexpr (std::same_as<ScoredMove, std::pair<u16, i16>>) {
                m_moves.emplace_back(move.raw(), static_cast<i16>(searchedScore));
            } else if constexpr (std::same_as<ScoredMove, std::tuple<u16, i16, i16>>) {
                m_moves.emplace_back(move.raw(), staticEval, searchedScore);
            }
        }

        usize writeAllWithOutcome(std::ostream& stream, Outcome outcome) final {
            static constexpr ScoredMove kNullTerminator{};

            static constexpr u8 kStandardType = 0;

            const u8 wdlType = kStandardType | (static_cast<u8>(outcome) << 6);
            stream.write(reinterpret_cast<const char*>(&wdlType), sizeof(wdlType));

            const u16 unscoredCount = m_unscoredMoves.size();
            stream.write(reinterpret_cast<const char*>(&unscoredCount), sizeof(unscoredCount));
            stream.write(reinterpret_cast<const char*>(m_unscoredMoves.data()), m_unscoredMoves.size() * sizeof(u16));

            stream.write(reinterpret_cast<const char*>(m_moves.data()), m_moves.size() * sizeof(ScoredMove));
            stream.write(reinterpret_cast<const char*>(&kNullTerminator), sizeof(kNullTerminator));

            return m_moves.size();
        }

    private:
        static_assert(
            (std::same_as<ScoredMove, std::pair<u16, i16>> && sizeof(ScoredMove) == sizeof(u16) + sizeof(i16))
            || (std::same_as<ScoredMove, std::tuple<u16, i16, i16>>
                && sizeof(ScoredMove) == sizeof(u16) + sizeof(i16) * 2)
        );

        std::vector<u16> m_unscoredMoves{};
        std::vector<ScoredMove> m_moves{};
    };

    using Stoatpack = StoatpackBase<std::pair<u16, i16>>;
    using Stoatpack2 = StoatpackBase<std::tuple<u16, i16, i16>>;
} // namespace stoat::datagen::format
