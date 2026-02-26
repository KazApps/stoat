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

#include "attacks/attacks.h"
#include "bitboard.h"
#include "core.h"
#include "util/multi_array.h"

namespace stoat {
    constexpr auto kBetweenRays = [] {
        util::MultiArray<Bitboard, Squares::kCount, Squares::kCount> dst{};

        for (i32 aIdx = 0; aIdx < Squares::kCount; ++aIdx) {
            const auto aSq = Square::fromRaw(aIdx);
            const auto aMask = Bitboard::fromSquare(aSq);

            const auto rookAttacks = attacks::sliders::kEmptyBoardRookAttacks[aIdx];
            const auto bishopAttacks = attacks::sliders::kEmptyBoardBishopAttacks[aIdx];

            for (i32 bIdx = 0; bIdx < Squares::kCount; ++bIdx) {
                if (aIdx == bIdx) {
                    continue;
                }

                const auto bSq = Square::fromRaw(bIdx);
                const auto bMask = Bitboard::fromSquare(bSq);

                if (rookAttacks.getSquare(bSq)) {
                    dst[aIdx][bIdx] = attacks::rookAttacks(aSq, bMask) & attacks::rookAttacks(bSq, aMask);
                } else if (bishopAttacks.getSquare(bSq)) {
                    dst[aIdx][bIdx] = attacks::bishopAttacks(aSq, bMask) & attacks::bishopAttacks(bSq, aMask);
                }
            }
        }

        return dst;
    }();

    constexpr auto kIntersectingRays = [] {
        util::MultiArray<Bitboard, Squares::kCount, Squares::kCount> dst{};

        for (i32 aIdx = 0; aIdx < Squares::kCount; ++aIdx) {
            const auto aSq = Square::fromRaw(aIdx);
            const auto aMask = Bitboard::fromSquare(aSq);

            const auto rookAttacks = attacks::sliders::kEmptyBoardRookAttacks[aSq.idx()];
            const auto bishopAttacks = attacks::sliders::kEmptyBoardBishopAttacks[aSq.idx()];

            for (i32 bIdx = 0; bIdx < Squares::kCount; ++bIdx) {
                if (aIdx == bIdx) {
                    continue;
                }

                const auto bSq = Square::fromRaw(bIdx);
                const auto bMask = Bitboard::fromSquare(bSq);

                if (rookAttacks.getSquare(bSq)) {
                    dst[aSq.idx()][bSq.idx()] = (aMask | attacks::rookAttacks(aSq, Bitboards::kEmpty))
                                              & (bMask | attacks::rookAttacks(bSq, Bitboards::kEmpty));
                } else if (bishopAttacks.getSquare(bSq)) {
                    dst[aSq.idx()][bSq.idx()] = (aMask | attacks::bishopAttacks(aSq, Bitboards::kEmpty))
                                              & (bMask | attacks::bishopAttacks(bSq, Bitboards::kEmpty));
                }
            }
        }

        return dst;
    }();

    constexpr auto kPassingRays = [] {
        util::MultiArray<Bitboard, Squares::kCount, Squares::kCount> dst{};

        for (i32 aIdx = 0; aIdx < Squares::kCount; ++aIdx) {
            const auto aSq = Square::fromRaw(aIdx);
            const auto aMask = Bitboard::fromSquare(aSq);

            const auto rookAttacks = attacks::sliders::kEmptyBoardRookAttacks[aSq.idx()];
            const auto bishopAttacks = attacks::sliders::kEmptyBoardBishopAttacks[aSq.idx()];

            for (i32 bIdx = 0; bIdx < Squares::kCount; ++bIdx) {
                if (aIdx == bIdx) {
                    continue;
                }

                const auto bSq = Square::fromRaw(bIdx);
                const auto bMask = Bitboard::fromSquare(bSq);

                if (rookAttacks.getSquare(bSq)) {
                    dst[aSq.idx()][bSq.idx()] =
                        attacks::rookAttacks(aSq, Bitboards::kEmpty) & (bMask | attacks::rookAttacks(bSq, aMask));
                } else if (bishopAttacks.getSquare(bSq)) {
                    dst[aSq.idx()][bSq.idx()] =
                        attacks::bishopAttacks(aSq, Bitboards::kEmpty) & (bMask | attacks::bishopAttacks(bSq, aMask));
                }
            }
        }

        return dst;
    }();

    constexpr auto rayBetween(Square a, Square b) {
        return kBetweenRays[a.idx()][b.idx()];
    }

    constexpr auto rayIntersecting(Square a, Square b) {
        return kIntersectingRays[a.idx()][b.idx()];
    }

    constexpr Bitboard rayPast(Square a, Square b) {
        return kPassingRays[a.idx()][b.idx()];
    }
} // namespace stoat
