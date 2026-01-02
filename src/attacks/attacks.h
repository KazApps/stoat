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

#include "../types.h"

#include <array>

#include "../bitboard.h"
#include "../core.h"
#include "../util/multi_array.h"

namespace stoat::attacks {
    namespace sliders {
        namespace internal {
            [[nodiscard]] constexpr Bitboard edges(i32 dir) {
                switch (dir) {
                    case offsets::kNorth:
                        return Bitboards::kRankA;
                    case offsets::kSouth:
                        return Bitboards::kRankI;
                    case offsets::kWest:
                        return Bitboards::kFile9;
                    case offsets::kEast:
                        return Bitboards::kFile1;
                    case offsets::kNorthWest:
                        return Bitboards::kRankA | Bitboards::kFile9;
                    case offsets::kNorthEast:
                        return Bitboards::kRankA | Bitboards::kFile1;
                    case offsets::kSouthWest:
                        return Bitboards::kRankI | Bitboards::kFile9;
                    case offsets::kSouthEast:
                        return Bitboards::kRankI | Bitboards::kFile1;
                    default:
                        assert(false);
                        return Bitboards::kEmpty;
                }
            }

            [[nodiscard]] constexpr Bitboard generateSlidingAttacks(Square src, i32 dir, Bitboard occ) {
                assert(src);

                auto blockers = edges(dir);
                auto bit = Bitboard::fromSquare(src);

                if (!(blockers & bit).empty()) {
                    return Bitboards::kEmpty;
                }

                blockers |= occ;

                const bool right = dir < 0;
                const auto shift = dir < 0 ? -dir : dir;

                Bitboard dst{};

                do {
                    if (right) {
                        bit >>= shift;
                    } else {
                        bit <<= shift;
                    }

                    dst |= bit;
                } while ((bit & blockers).empty());

                return dst;
            }

            template <i32... kDirs>
            consteval std::array<Bitboard, Squares::kCount> generateEmptyBoardAttacks() {
                std::array<Bitboard, Squares::kCount> dst{};

                for (i32 sqIdx = 0; sqIdx < Squares::kCount; ++sqIdx) {
                    const auto sq = Square::fromRaw(sqIdx);

                    for (const auto dir : {kDirs...}) {
                        const auto attacks = generateSlidingAttacks(sq, dir, Bitboards::kEmpty);
                        dst[sq.idx()] |= attacks;
                    }
                }

                return dst;
            }
        } // namespace internal

        struct SlidingMask {
            std::array<Bitboard, 2> backwards{};
            std::array<Bitboard, 2> forwards{};
            Bitboard all{};
        };

        template <i32... kDirs>
        constexpr std::array<SlidingMask, Squares::kCount> generateSlidingMasks() {
            assert(src);
            std::array<SlidingMask, Squares::kCount> dst{};

            for (i32 sqIdx = 0; sqIdx < Squares::kCount; ++sqIdx) {
                const auto sq = Square::fromRaw(sqIdx);

                SlidingMask mask{};
                u32 b_idx = 0;
                u32 f_idx = 0;

                for (const auto dir : {kDirs...}) {
                    const auto attacks = internal::generateSlidingAttacks(sq, dir, Bitboards::kEmpty);

                    if (dir < 0) {
                        mask.backwards[b_idx++] = attacks;
                    } else if (dir > 0) {
                        mask.forwards[f_idx++] = attacks;
                    }

                    mask.all |= attacks;
                }

                dst[sq.idx()] = mask;
            }

            return dst;
        }

        constexpr util::MultiArray<Bitboard, Colors::kCount, Squares::kCount> kEmptyBoardLanceAttacks = {
            internal::generateEmptyBoardAttacks<offsets::kNorth>(), // black
            internal::generateEmptyBoardAttacks<offsets::kSouth>(), // white
        };

        constexpr auto kEmptyBoardBishopAttacks = internal::generateEmptyBoardAttacks<
            offsets::kNorthWest,
            offsets::kNorthEast,
            offsets::kSouthWest,
            offsets::kSouthEast>();
        constexpr auto kEmptyBoardRookAttacks =
            internal::generateEmptyBoardAttacks<offsets::kNorth, offsets::kSouth, offsets::kWest, offsets::kEast>();

        constexpr auto kBishopMasks =
            generateSlidingMasks<offsets::kNorthWest, offsets::kNorthEast, offsets::kSouthWest, offsets::kSouthEast>();
        constexpr auto kRookMasks =
            generateSlidingMasks<offsets::kNorth, offsets::kSouth, offsets::kWest, offsets::kEast>();

        constexpr Bitboard sliding_backward(Bitboard occupied, Bitboard mask) {
            const auto lz = util::clz((occupied & mask | Squares::k9I.bit()).raw());
            return Bitboard{mask.raw() & ~((u128{1} << (127 - lz)) - 1)};
        }

        constexpr Bitboard sliding_forward(Bitboard occupied, Bitboard mask) {
            const auto tz = util::ctz((occupied & mask | Squares::k1A.bit()).raw());
            return Bitboard{mask.raw() & ((u128{1} << (tz + 1)) - 1)};
        }
    } // namespace sliders

    namespace tables {
        consteval std::array<Bitboard, Squares::kCount> generateAttacks(auto func) {
            std::array<Bitboard, Squares::kCount> attacks{};

            for (i32 idx = 0; idx < Squares::kCount; ++idx) {
                func(attacks[idx], Square::fromRaw(idx));
                attacks[idx] &= Bitboards::kAll;
            }

            return attacks;
        }

        consteval std::array<std::array<Bitboard, Squares::kCount>, 2> generateSidedAttacks(auto func) {
            util::MultiArray<Bitboard, 2, Squares::kCount> attacks{};

            attacks[Colors::kBlack.idx()] =
                generateAttacks([&func](Bitboard& attacks, Square sq) { return func(Colors::kBlack, attacks, sq); });
            attacks[Colors::kWhite.idx()] =
                generateAttacks([&func](Bitboard& attacks, Square sq) { return func(Colors::kWhite, attacks, sq); });

            return attacks;
        }

        constexpr auto kPawnAttacks = generateSidedAttacks([](Color c, Bitboard& attacks, Square sq) {
            const auto bit = Bitboard::fromSquare(sq);
            attacks |= bit.shiftNorthRelative(c);
        });

        constexpr auto kKnightAttacks = generateSidedAttacks([](Color c, Bitboard& attacks, Square sq) {
            const auto bit = Bitboard::fromSquare(sq);

            attacks |= bit.shiftNorthRelative(c).shiftNorthWestRelative(c);
            attacks |= bit.shiftNorthRelative(c).shiftNorthEastRelative(c);
        });

        constexpr auto kSilverAttacks = generateSidedAttacks([](Color c, Bitboard& attacks, Square sq) {
            const auto bit = Bitboard::fromSquare(sq);

            attacks |= bit.shiftNorthWest();
            attacks |= bit.shiftNorthEast();
            attacks |= bit.shiftSouthWest();
            attacks |= bit.shiftSouthEast();

            attacks |= bit.shiftNorthRelative(c);
        });

        constexpr auto kGoldAttacks = generateSidedAttacks([](Color c, Bitboard& attacks, Square sq) {
            const auto bit = Bitboard::fromSquare(sq);

            attacks |= bit.shiftNorth();
            attacks |= bit.shiftSouth();
            attacks |= bit.shiftWest();
            attacks |= bit.shiftEast();

            attacks |= bit.shiftNorthWestRelative(c);
            attacks |= bit.shiftNorthEastRelative(c);
        });

        constexpr auto kKingAttacks = generateAttacks([](Bitboard& attacks, Square sq) {
            const auto bit = Bitboard::fromSquare(sq);

            attacks |= bit.shiftNorth();
            attacks |= bit.shiftSouth();
            attacks |= bit.shiftWest();
            attacks |= bit.shiftEast();

            attacks |= bit.shiftNorthWest();
            attacks |= bit.shiftNorthEast();
            attacks |= bit.shiftSouthWest();
            attacks |= bit.shiftSouthEast();
        });
    } // namespace tables

    [[nodiscard]] constexpr Bitboard pawnAttacks(Square sq, Color c) {
        assert(sq);
        assert(c);
        return tables::kPawnAttacks[c.idx()][sq.idx()];
    }

    [[nodiscard]] constexpr Bitboard lanceAttacks(Square sq, Color c, Bitboard occ) {
        assert(sq);
        assert(c);

        const auto emptyLanceAttacks = sliders::kEmptyBoardLanceAttacks[c.idx()][sq.idx()];

        if (c == Colors::kBlack) {
            return sliders::sliding_forward(occ, emptyLanceAttacks);
        } else {
            return sliders::sliding_backward(occ, emptyLanceAttacks);
        }

        __builtin_unreachable();
    }

    [[nodiscard]] constexpr Bitboard knightAttacks(Square sq, Color c) {
        assert(sq);
        assert(c);
        return tables::kKnightAttacks[c.idx()][sq.idx()];
    }

    [[nodiscard]] constexpr Bitboard silverAttacks(Square sq, Color c) {
        assert(sq);
        assert(c);
        return tables::kSilverAttacks[c.idx()][sq.idx()];
    }

    [[nodiscard]] constexpr Bitboard goldAttacks(Square sq, Color c) {
        assert(sq);
        assert(c);
        return tables::kGoldAttacks[c.idx()][sq.idx()];
    }

    [[nodiscard]] constexpr Bitboard bishopAttacks(Square sq, Bitboard occ) {
        assert(sq);

        const auto mask = sliders::kBishopMasks[sq.idx()];
        return sliders::sliding_backward(occ, mask.backwards[0]) | sliders::sliding_backward(occ, mask.backwards[1])
             | sliders::sliding_forward(occ, mask.forwards[0]) | sliders::sliding_forward(occ, mask.forwards[1]);
    }

    [[nodiscard]] constexpr Bitboard rookAttacks(Square sq, Bitboard occ) {
        assert(sq);

        const auto mask = sliders::kRookMasks[sq.idx()];
        return sliders::sliding_backward(occ, mask.backwards[0]) | sliders::sliding_backward(occ, mask.backwards[1])
             | sliders::sliding_forward(occ, mask.forwards[0]) | sliders::sliding_forward(occ, mask.forwards[1]);
    }

    [[nodiscard]] constexpr Bitboard kingAttacks(Square sq) {
        assert(sq);
        return tables::kKingAttacks[sq.idx()];
    }

    [[nodiscard]] constexpr Bitboard promotedBishopAttacks(Square sq, Bitboard occ) {
        assert(sq);
        return bishopAttacks(sq, occ) | kingAttacks(sq);
    }

    [[nodiscard]] constexpr Bitboard promotedRookAttacks(Square sq, Bitboard occ) {
        assert(sq);
        return rookAttacks(sq, occ) | kingAttacks(sq);
    }

    [[nodiscard]] constexpr Bitboard pieceAttacks(PieceType pt, Square sq, Color c, Bitboard occ) {
        assert(pt);
        assert(sq);
        assert(c);

        switch (pt.raw()) {
            case PieceTypes::kPawn.raw():
                return pawnAttacks(sq, c);
            case PieceTypes::kPromotedPawn.raw():
                return goldAttacks(sq, c);
            case PieceTypes::kLance.raw():
                return lanceAttacks(sq, c, occ);
            case PieceTypes::kKnight.raw():
                return knightAttacks(sq, c);
            case PieceTypes::kPromotedLance.raw():
                return goldAttacks(sq, c);
            case PieceTypes::kPromotedKnight.raw():
                return goldAttacks(sq, c);
            case PieceTypes::kSilver.raw():
                return silverAttacks(sq, c);
            case PieceTypes::kPromotedSilver.raw():
                return goldAttacks(sq, c);
            case PieceTypes::kGold.raw():
                return goldAttacks(sq, c);
            case PieceTypes::kBishop.raw():
                return bishopAttacks(sq, occ);
            case PieceTypes::kRook.raw():
                return rookAttacks(sq, occ);
            case PieceTypes::kPromotedBishop.raw():
                return promotedBishopAttacks(sq, occ);
            case PieceTypes::kPromotedRook.raw():
                return promotedRookAttacks(sq, occ);
            case PieceTypes::kKing.raw():
                return kingAttacks(sq);
        }

        __builtin_unreachable();
    }
} // namespace stoat::attacks
