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

#include <array>

#include "core.h"
#include "util/rng.h"

namespace stoat::keys {
    namespace sizes {
        constexpr usize kPieceSquares = Pieces::kCount * Squares::kCount;
        constexpr usize kStm = 1;
        constexpr usize kPawnsInHand = (maxPiecesInHand(PieceTypes::kPawn) + 1) * Colors::kCount;
        constexpr usize kLancesInHand = (maxPiecesInHand(PieceTypes::kLance) + 1) * Colors::kCount;
        constexpr usize kKnightsInHand = (maxPiecesInHand(PieceTypes::kKnight) + 1) * Colors::kCount;
        constexpr usize kSilversInHand = (maxPiecesInHand(PieceTypes::kSilver) + 1) * Colors::kCount;
        constexpr usize kGoldsInHand = (maxPiecesInHand(PieceTypes::kGold) + 1) * Colors::kCount;
        constexpr usize kBishopsInHand = (maxPiecesInHand(PieceTypes::kBishop) + 1) * Colors::kCount;
        constexpr usize kRooksInHand = (maxPiecesInHand(PieceTypes::kRook) + 1) * Colors::kCount;
        constexpr usize kKingZone = 9;

        constexpr auto kTotal = kPieceSquares + kStm + kPawnsInHand + kLancesInHand + kKnightsInHand + kSilversInHand
                              + kGoldsInHand + kBishopsInHand + kRooksInHand + kKingZone;
    } // namespace sizes

    namespace offsets {
        constexpr usize kPieceSquares = 0;
        constexpr auto kStm = kPieceSquares + sizes::kPieceSquares;
        constexpr auto kPawnsInHand = kStm + sizes::kStm;
        constexpr auto kLancesInHand = kPawnsInHand + sizes::kPawnsInHand;
        constexpr auto kKnightsInHand = kLancesInHand + sizes::kLancesInHand;
        constexpr auto kSilversInHand = kKnightsInHand + sizes::kKnightsInHand;
        constexpr auto kGoldsInHand = kSilversInHand + sizes::kSilversInHand;
        constexpr auto kBishopsInHand = kGoldsInHand + sizes::kGoldsInHand;
        constexpr auto kRooksInHand = kBishopsInHand + sizes::kBishopsInHand;
        constexpr auto kKingZone = kRooksInHand + sizes::kRooksInHand;
    } // namespace offsets

    constexpr auto kKeys = [] {
        constexpr auto Seed = UINT64_C(0x590d3524d1d6301c);

        std::array<u64, sizes::kTotal> keys{};

        util::rng::Jsf64Rng rng{Seed};

        for (auto& key : keys) {
            key = rng.nextU64();
        }

        return keys;
    }();

    [[nodiscard]] constexpr u64 pieceSquare(Piece piece, Square sq) {
        assert(piece);
        assert(sq);

        return kKeys[offsets::kPieceSquares + sq.idx() * Pieces::kCount + piece.idx()];
    }

    [[nodiscard]] constexpr u64 stm() {
        return kKeys[offsets::kStm];
    }

    [[nodiscard]] constexpr u64 pieceInHand(Color c, PieceType pt, u32 count) {
        constexpr auto kOffsets = [] {
            std::array<i32, PieceTypes::kCount> offsets{};
            offsets.fill(-1);

            offsets[PieceTypes::kPawn.idx()] = offsets::kPawnsInHand;
            offsets[PieceTypes::kLance.idx()] = offsets::kLancesInHand;
            offsets[PieceTypes::kKnight.idx()] = offsets::kKnightsInHand;
            offsets[PieceTypes::kSilver.idx()] = offsets::kSilversInHand;
            offsets[PieceTypes::kGold.idx()] = offsets::kGoldsInHand;
            offsets[PieceTypes::kBishop.idx()] = offsets::kBishopsInHand;
            offsets[PieceTypes::kRook.idx()] = offsets::kRooksInHand;

            return offsets;
        }();

        assert(c);
        assert(pt);
        assert(count <= maxPiecesInHand(pt));

        const auto offset = kOffsets[pt.idx()];
        assert(offset != -1);

        return kKeys[offset + count * Colors::kCount + c.idx()];
    }

    [[nodiscard]] constexpr u64 kingZone(Square sq) {
        assert(sq);

        return kKeys[offsets::kKingZone + sq.file() / 3 * 3 + sq.rank() / 3];
    }
} // namespace stoat::keys
