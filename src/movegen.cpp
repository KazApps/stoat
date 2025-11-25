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

#include "movegen.h"

#include "attacks/attacks.h"
#include "rays.h"

namespace stoat::movegen {
    namespace {
        void serializeNormals(MoveList& dst, i32 offset, Bitboard attacks) {
            while (!attacks.empty()) {
                const auto to = attacks.popLsb();
                const auto from = to.offset(-offset);

                dst.push(Move::makeNormal(from, to));
            }
        }

        void serializeNormals(MoveList& dst, Square from, Bitboard attacks) {
            while (!attacks.empty()) {
                const auto to = attacks.popLsb();
                dst.push(Move::makeNormal(from, to));
            }
        }

        void serializePromotions(MoveList& dst, i32 offset, Bitboard attacks) {
            while (!attacks.empty()) {
                const auto to = attacks.popLsb();
                const auto from = to.offset(-offset);

                dst.push(Move::makePromotion(from, to));
            }
        }

        void serializePromotions(MoveList& dst, Square from, Bitboard attacks) {
            while (!attacks.empty()) {
                const auto to = attacks.popLsb();
                dst.push(Move::makePromotion(from, to));
            }
        }

        void serializeDrops(MoveList& dst, PieceType pt, Bitboard targets) {
            while (!targets.empty()) {
                const auto to = targets.popLsb();
                dst.push(Move::makeDrop(pt, to));
            }
        }

        template <bool kCanPromote>
        struct GeneratePrecalculatedWithColorAndOcc {
            void operator()(
                MoveList& dst,
                const Position& pos,
                Bitboard pieces,
                auto attackGetter,
                Bitboard dstMask,
                Bitboard nonPromoMask = Bitboards::kAll
            ) {
                const auto stm = pos.stm();

                const auto occ = pos.occupancy();

                if constexpr (kCanPromote) {
                    const auto promoArea = Bitboards::promoArea(stm);

                    auto promotable = pieces;
                    while (!promotable.empty()) {
                        const auto piece = promotable.popLsb();
                        const auto attacks = attackGetter(piece, pos.stm(), occ) & dstMask & promoArea;

                        serializePromotions(dst, piece, attacks);
                    }

                    promotable = pieces & promoArea;
                    while (!promotable.empty()) {
                        const auto piece = promotable.popLsb();
                        const auto attacks = attackGetter(piece, pos.stm(), occ) & dstMask & ~promoArea;

                        serializePromotions(dst, piece, attacks);
                    }
                }

                auto movable = pieces;
                while (!movable.empty()) {
                    const auto piece = movable.popLsb();
                    const auto attacks = attackGetter(piece, pos.stm(), occ) & dstMask & nonPromoMask;

                    serializeNormals(dst, piece, attacks);
                }
            }
        };

        template <bool kCanPromote>
        struct GeneratePrecalculatedWithColor {
            void operator()(
                MoveList& dst,
                const Position& pos,
                Bitboard pieces,
                auto attackGetter,
                Bitboard dstMask,
                Bitboard nonPromoMask = Bitboards::kAll
            ) {
                GeneratePrecalculatedWithColorAndOcc<kCanPromote>{}(
                    dst,
                    pos,
                    pieces,
                    [&](Square sq, Color c, Bitboard occ) { return attackGetter(sq, c); },
                    dstMask,
                    nonPromoMask
                );
            }
        };

        template <bool kCanPromote>
        struct GeneratePrecalculatedWithOcc {
            void operator()(
                MoveList& dst,
                const Position& pos,
                Bitboard pieces,
                auto attackGetter,
                Bitboard dstMask,
                Bitboard nonPromoMask = Bitboards::kAll
            ) {
                GeneratePrecalculatedWithColorAndOcc<kCanPromote>{}(
                    dst,
                    pos,
                    pieces,
                    [&](Square sq, Color c, Bitboard occ) { return attackGetter(sq, occ); },
                    dstMask,
                    nonPromoMask
                );
            }
        };

        template <bool kCanPromote>
        struct GeneratePrecalculated {
            void operator()(
                MoveList& dst,
                const Position& pos,
                Bitboard pieces,
                auto attackGetter,
                Bitboard dstMask,
                Bitboard nonPromoMask = Bitboards::kAll
            ) {
                GeneratePrecalculatedWithColorAndOcc<kCanPromote>{}(
                    dst,
                    pos,
                    pieces,
                    [&](Square sq, Color c, Bitboard occ) { return attackGetter(sq); },
                    dstMask,
                    nonPromoMask
                );
            }
        };

        void generatePawns(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto stm = pos.stm();
            const auto pawns = pos.pieceBb(PieceTypes::kPawn, stm);

            const auto shifted = pawns.shiftNorthRelative(stm) & dstMask;

            const auto promos = shifted & Bitboards::promoArea(stm);
            const auto nonPromos = shifted & ~Bitboards::relativeRank(stm, 8);

            const auto offset = offsets::relativeOffset(stm, offsets::kNorth);

            serializePromotions(dst, offset, promos);
            serializeNormals(dst, offset, nonPromos);
        }

        template <typename... PieceTypes>
        void generateNonPawns(
            MoveList& dst,
            const Position& pos,
            auto generator,
            PieceType pieceType,
            auto attackGetter,
            Bitboard dstMask,
            Bitboard nonPromoMask = Bitboards::kAll,
            PieceTypes... types
        ) {
            const auto stm = pos.stm();
            const auto pieces = pos.pieceBb(pieceType, stm) | (pos.pieceBb(types, stm) | ... | Bitboards::kEmpty);
            generator(dst, pos, pieces, attackGetter, dstMask, nonPromoMask);
        }

        void generateDrops(MoveList& dst, const Position& pos, PieceType pt, Bitboard dstMask) {
            const auto stm = pos.stm();
            const auto& hand = pos.hand(stm);

            if (hand.count(pt) > 0) {
                serializeDrops(dst, pt, dstMask);
            }
        }

        template <bool kGenerateDrops>
        void generate(MoveList& dst, const Position& pos, Bitboard dstMask) {
            generateNonPawns(
                dst,
                pos,
                GeneratePrecalculated<false>{},
                PieceTypes::kKing,
                attacks::kingAttacks,
                dstMask
            );

            if (pos.checkers().multiple()) {
                return;
            }

            const auto stm = pos.stm();

            auto pawnMask = ~Bitboards::relativeRank(stm, 8) & ~pos.pieceBb(PieceTypes::kPawn, stm).fillFile();
            auto lanceMask = ~Bitboards::relativeRank(stm, 8);
            auto knightMask = ~(Bitboards::relativeRank(stm, 8) | Bitboards::relativeRank(stm, 7));

            auto dropMask = dstMask & ~pos.occupancy();

            if (!pos.checkers().empty()) {
                const auto checker = pos.checkers().lsb();
                const auto checkRay = rayBetween(pos.kingSq(stm), checker);

                dstMask &= checkRay | checker.bit();
                dropMask &= checkRay;
            }

            generatePawns(dst, pos, dstMask);
            generateNonPawns(
                dst,
                pos,
                GeneratePrecalculatedWithColorAndOcc<true>{},
                PieceTypes::kLance,
                attacks::lanceAttacks,
                dstMask,
                lanceMask
            );
            generateNonPawns(
                dst,
                pos,
                GeneratePrecalculatedWithColor<true>{},
                PieceTypes::kKnight,
                attacks::knightAttacks,
                dstMask,
                knightMask
            );
            generateNonPawns(
                dst,
                pos,
                GeneratePrecalculatedWithColor<true>{},
                PieceTypes::kSilver,
                attacks::silverAttacks,
                dstMask
            );
            generateNonPawns(
                dst,
                pos,
                GeneratePrecalculatedWithColor<false>{},
                PieceTypes::kGold,
                attacks::goldAttacks,
                dstMask,
                Bitboards::kAll,
                PieceTypes::kPromotedPawn,
                PieceTypes::kPromotedLance,
                PieceTypes::kPromotedKnight,
                PieceTypes::kPromotedSilver
            );
            generateNonPawns(
                dst,
                pos,
                GeneratePrecalculatedWithOcc<true>{},
                PieceTypes::kBishop,
                attacks::bishopAttacks,
                dstMask
            );
            generateNonPawns(
                dst,
                pos,
                GeneratePrecalculatedWithOcc<true>{},
                PieceTypes::kRook,
                attacks::rookAttacks,
                dstMask
            );
            generateNonPawns(
                dst,
                pos,
                GeneratePrecalculatedWithOcc<false>{},
                PieceTypes::kPromotedBishop,
                attacks::promotedBishopAttacks,
                dstMask
            );
            generateNonPawns(
                dst,
                pos,
                GeneratePrecalculatedWithOcc<false>{},
                PieceTypes::kPromotedRook,
                attacks::promotedRookAttacks,
                dstMask
            );

            if constexpr (kGenerateDrops) {
                generateDrops(dst, pos, PieceTypes::kPawn, dropMask & pawnMask);
                generateDrops(dst, pos, PieceTypes::kLance, dropMask & lanceMask);
                generateDrops(dst, pos, PieceTypes::kKnight, dropMask & knightMask);
                generateDrops(dst, pos, PieceTypes::kSilver, dropMask);
                generateDrops(dst, pos, PieceTypes::kGold, dropMask);
                generateDrops(dst, pos, PieceTypes::kBishop, dropMask);
                generateDrops(dst, pos, PieceTypes::kRook, dropMask);
            }
        }
    } // namespace

    void generateAll(MoveList& dst, const Position& pos) {
        const auto dstMask = ~pos.colorBb(pos.stm());
        generate<true>(dst, pos, dstMask);
    }

    void generateCaptures(MoveList& dst, const Position& pos) {
        const auto dstMask = pos.colorBb(pos.stm().flip());
        generate<false>(dst, pos, dstMask);
    }

    void generateNonCaptures(MoveList& dst, const Position& pos) {
        const auto dstMask = ~pos.occupancy();
        generate<true>(dst, pos, dstMask);
    }

    void generateRecaptures(MoveList& dst, const Position& pos, Square captureSq) {
        assert(!pos.colorBb(pos.stm()).getSquare(captureSq));
        assert(pos.colorBb(pos.stm().flip()).getSquare(captureSq));

        const auto dstMask = Bitboard::fromSquare(captureSq);
        generate<false>(dst, pos, dstMask);
    }
} // namespace stoat::movegen
