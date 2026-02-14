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

        template <bool kCanPromote, bool kForcePromote = false>
        void generatePrecalculatedWithColorAndOcc(
            MoveList& dst,
            const Position& pos,
            Bitboard pieces,
            auto attackGetter,
            Bitboard dstMask,
            Bitboard nonPromoMask = Bitboards::kAll
        ) {
            static_assert(kCanPromote || !kForcePromote);

            const auto stm = pos.stm();
            const auto occ = pos.occupancy();
            const auto promoArea = Bitboards::promoArea(stm);

            if constexpr (kCanPromote) {
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

            auto movable = kForcePromote ? pieces & ~promoArea : pieces;
            while (!movable.empty()) {
                const auto piece = movable.popLsb();
                const auto attacks = attackGetter(piece, pos.stm(), occ) & dstMask & nonPromoMask;

                serializeNormals(dst, piece, kForcePromote ? attacks & ~promoArea : attacks);
            }
        }

        template <bool kCanPromote, bool kForcePromote = false>
        void generatePrecalculatedWithColor(
            MoveList& dst,
            const Position& pos,
            Bitboard pieces,
            auto attackGetter,
            Bitboard dstMask,
            Bitboard nonPromoMask = Bitboards::kAll
        ) {
            generatePrecalculatedWithColorAndOcc<kCanPromote, kForcePromote>(
                dst,
                pos,
                pieces,
                [&](Square sq, Color c, [[maybe_unused]] Bitboard occ) { return attackGetter(sq, c); },
                dstMask,
                nonPromoMask
            );
        }

        template <bool kCanPromote, bool kForcePromote = false>
        void generatePrecalculatedWithOcc(
            MoveList& dst,
            const Position& pos,
            Bitboard pieces,
            auto attackGetter,
            Bitboard dstMask,
            Bitboard nonPromoMask = Bitboards::kAll
        ) {
            generatePrecalculatedWithColorAndOcc<kCanPromote, kForcePromote>(
                dst,
                pos,
                pieces,
                [&](Square sq, [[maybe_unused]] Color c, Bitboard occ) { return attackGetter(sq, occ); },
                dstMask,
                nonPromoMask
            );
        }

        template <bool kCanPromote, bool kForcePromote = false>
        void generatePrecalculated(
            MoveList& dst,
            const Position& pos,
            Bitboard pieces,
            auto attackGetter,
            Bitboard dstMask,
            Bitboard nonPromoMask = Bitboards::kAll
        ) {
            generatePrecalculatedWithColorAndOcc<kCanPromote, kForcePromote>(
                dst,
                pos,
                pieces,
                [&](Square sq, [[maybe_unused]] Color c, [[maybe_unused]] Bitboard occ) { return attackGetter(sq); },
                dstMask,
                nonPromoMask
            );
        }

        template <bool kGenerateUnlikelyMove>
        void generatePawns(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto stm = pos.stm();
            const auto pawns = pos.pieceBb(PieceTypes::kPawn, stm);

            const auto shifted = pawns.shiftNorthRelative(stm) & dstMask;

            const auto promos = shifted & Bitboards::promoArea(stm);
            const auto nonPromos =
                shifted & (kGenerateUnlikelyMove ? ~Bitboards::relativeRank(stm, 8) : ~Bitboards::promoArea(stm));

            const auto offset = offsets::relativeOffset(stm, offsets::kNorth);

            serializePromotions(dst, offset, promos);
            serializeNormals(dst, offset, nonPromos);
        }

        template <bool kGenerateUnlikelyMove>
        void generateLances(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto lances = pos.pieceBb(PieceTypes::kLance, pos.stm());
            generatePrecalculatedWithColorAndOcc<true>(
                dst,
                pos,
                lances,
                attacks::lanceAttacks,
                dstMask,
                ~(Bitboards::relativeRank(pos.stm(), 8)
                  | (kGenerateUnlikelyMove ? Bitboards::kEmpty : Bitboards::relativeRank(pos.stm(), 7)))
            );
        }

        void generateKnights(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto knights = pos.pieceBb(PieceTypes::kKnight, pos.stm());
            generatePrecalculatedWithColor<true>(
                dst,
                pos,
                knights,
                attacks::knightAttacks,
                dstMask,
                ~(Bitboards::relativeRank(pos.stm(), 8) | Bitboards::relativeRank(pos.stm(), 7))
            );
        }

        void generateSilvers(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto silvers = pos.pieceBb(PieceTypes::kSilver, pos.stm());
            generatePrecalculatedWithColor<true>(dst, pos, silvers, attacks::silverAttacks, dstMask);
        }

        void generateGolds(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto golds = pos.pieceBb(PieceTypes::kGold, pos.stm())
                             | pos.pieceBb(PieceTypes::kPromotedPawn, pos.stm())
                             | pos.pieceBb(PieceTypes::kPromotedLance, pos.stm())
                             | pos.pieceBb(PieceTypes::kPromotedKnight, pos.stm())
                             | pos.pieceBb(PieceTypes::kPromotedSilver, pos.stm());
            generatePrecalculatedWithColor<false>(dst, pos, golds, attacks::goldAttacks, dstMask);
        }

        template <bool kGenerateUnlikelyMove>
        void generateBishops(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto bishops = pos.pieceBb(PieceTypes::kBishop, pos.stm());
            generatePrecalculatedWithOcc<true, !kGenerateUnlikelyMove>(
                dst,
                pos,
                bishops,
                attacks::bishopAttacks,
                dstMask
            );
        }

        template <bool kGenerateUnlikelyMove>
        void generateRooks(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto rooks = pos.pieceBb(PieceTypes::kRook, pos.stm());
            generatePrecalculatedWithOcc<true, !kGenerateUnlikelyMove>(dst, pos, rooks, attacks::rookAttacks, dstMask);
        }

        void generatePromotedBishops(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto horses = pos.pieceBb(PieceTypes::kPromotedBishop, pos.stm());
            generatePrecalculatedWithOcc<false>(dst, pos, horses, attacks::promotedBishopAttacks, dstMask);
        }

        void generatePromotedRooks(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto dragons = pos.pieceBb(PieceTypes::kPromotedRook, pos.stm());
            generatePrecalculatedWithOcc<false>(dst, pos, dragons, attacks::promotedRookAttacks, dstMask);
        }

        void generateKings(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto kings = pos.pieceBb(PieceTypes::kKing, pos.stm());
            generatePrecalculated<false>(dst, pos, kings, attacks::kingAttacks, dstMask);
        }

        void generateDrops(MoveList& dst, const Position& pos, Bitboard dstMask) {
            if (dstMask.empty()) {
                return;
            }

            const auto stm = pos.stm();
            const auto& hand = pos.hand(stm);

            if (hand.empty()) {
                return;
            }

            const auto generate = [&](PieceType pt, Bitboard restriction = Bitboards::kAll) {
                if (hand.count(pt) > 0) {
                    const auto targets = dstMask & restriction;
                    serializeDrops(dst, pt, targets);
                }
            };

            generate(
                PieceTypes::kPawn,
                ~Bitboards::relativeRank(stm, 8) & ~pos.pieceBb(PieceTypes::kPawn, stm).fillFile()
            );
            generate(PieceTypes::kLance, ~Bitboards::relativeRank(stm, 8));
            generate(PieceTypes::kKnight, ~(Bitboards::relativeRank(stm, 8) | Bitboards::relativeRank(stm, 7)));
            generate(PieceTypes::kSilver);
            generate(PieceTypes::kGold);
            generate(PieceTypes::kBishop);
            generate(PieceTypes::kRook);
        }

        template <bool kGenerateDrops, bool kGenerateUnlikelyMoves>
        void generate(MoveList& dst, const Position& pos, Bitboard dstMask) {
            generateKings(dst, pos, dstMask);

            if (pos.checkers().multiple()) {
                return;
            }

            auto dropMask = dstMask & ~pos.occupancy();

            if (!pos.checkers().empty()) {
                const auto checker = pos.checkers().lsb();
                const auto checkRay = rayBetween(pos.kingSq(pos.stm()), checker);

                dstMask &= checkRay | checker.bit();
                dropMask &= checkRay;
            }

            generatePawns<kGenerateUnlikelyMoves>(dst, pos, dstMask);
            generateLances<kGenerateUnlikelyMoves>(dst, pos, dstMask);
            generateKnights(dst, pos, dstMask);
            generateSilvers(dst, pos, dstMask);
            generateGolds(dst, pos, dstMask);
            generateBishops<kGenerateUnlikelyMoves>(dst, pos, dstMask);
            generateRooks<kGenerateUnlikelyMoves>(dst, pos, dstMask);
            generatePromotedBishops(dst, pos, dstMask);
            generatePromotedRooks(dst, pos, dstMask);

            if constexpr (kGenerateDrops) {
                generateDrops(dst, pos, dropMask);
            }
        }
    } // namespace

    template <bool kGenerateUnlikelyMoves>
    void generateAll(MoveList& dst, const Position& pos) {
        const auto dstMask = ~pos.colorBb(pos.stm());
        generate<true, kGenerateUnlikelyMoves>(dst, pos, dstMask);
    }

    template <bool kGenerateUnlikelyMoves>
    void generateCaptures(MoveList& dst, const Position& pos) {
        const auto dstMask = pos.colorBb(pos.stm().flip());
        generate<false, kGenerateUnlikelyMoves>(dst, pos, dstMask);
    }

    template <bool kGenerateUnlikelyMoves>
    void generateNonCaptures(MoveList& dst, const Position& pos) {
        const auto dstMask = ~pos.occupancy();
        generate<true, kGenerateUnlikelyMoves>(dst, pos, dstMask);
    }

    template <bool kGenerateUnlikelyMoves>
    void generateRecaptures(MoveList& dst, const Position& pos, Square captureSq) {
        assert(!pos.colorBb(pos.stm()).getSquare(captureSq));
        assert(pos.colorBb(pos.stm().flip()).getSquare(captureSq));

        const auto dstMask = Bitboard::fromSquare(captureSq);
        generate<false, kGenerateUnlikelyMoves>(dst, pos, dstMask);
    }

    template void generateAll<true>(MoveList&, const Position&);
    template void generateAll<false>(MoveList&, const Position&);
    template void generateCaptures<true>(MoveList&, const Position&);
    template void generateCaptures<false>(MoveList&, const Position&);
    template void generateNonCaptures<true>(MoveList&, const Position&);
    template void generateNonCaptures<false>(MoveList&, const Position&);
    template void generateRecaptures<true>(MoveList&, const Position&, Square);
    template void generateRecaptures<false>(MoveList&, const Position&, Square);
} // namespace stoat::movegen
