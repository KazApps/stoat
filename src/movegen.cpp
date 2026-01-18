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
        void generatePrecalculatedWithColorAndOcc(
            MoveList& dst,
            const Position& pos,
            Bitboard pieces,
            auto attackGetter,
            Bitboard dstMask,
            Bitboard nonPromoMask,
            Bitboard promoMask = Bitboards::kAll
        ) {
            const auto stm = pos.stm();

            const auto occ = pos.occupancy();

            if constexpr (kCanPromote) {
                const auto promoArea = Bitboards::promoArea(stm);

                auto promotable = pieces;
                while (!promotable.empty()) {
                    const auto piece = promotable.popLsb();
                    const auto attacks = attackGetter(piece, pos.stm(), occ) & dstMask & promoArea & promoMask;

                    serializePromotions(dst, piece, attacks);
                }

                promotable = pieces & promoArea;
                while (!promotable.empty()) {
                    const auto piece = promotable.popLsb();
                    const auto attacks = attackGetter(piece, pos.stm(), occ) & dstMask & ~promoArea & promoMask;

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

        template <bool kCanPromote>
        void generatePrecalculatedWithColor(
            MoveList& dst,
            const Position& pos,
            Bitboard pieces,
            auto attackGetter,
            Bitboard dstMask,
            Bitboard nonPromoMask,
            Bitboard promoMask = Bitboards::kAll
        ) {
            generatePrecalculatedWithColorAndOcc<kCanPromote>(
                dst,
                pos,
                pieces,
                [&](Square sq, Color c, [[maybe_unused]] Bitboard occ) { return attackGetter(sq, c); },
                dstMask,
                nonPromoMask,
                promoMask
            );
        }

        template <bool kCanPromote>
        void generatePrecalculatedWithOcc(
            MoveList& dst,
            const Position& pos,
            Bitboard pieces,
            auto attackGetter,
            Bitboard dstMask,
            Bitboard nonPromoMask,
            Bitboard promoMask = Bitboards::kAll
        ) {
            generatePrecalculatedWithColorAndOcc<kCanPromote>(
                dst,
                pos,
                pieces,
                [&](Square sq, [[maybe_unused]] Color c, Bitboard occ) { return attackGetter(sq, occ); },
                dstMask,
                nonPromoMask,
                promoMask
            );
        }

        template <bool kCanPromote>
        void generatePrecalculated(
            MoveList& dst,
            const Position& pos,
            Bitboard pieces,
            auto attackGetter,
            Bitboard dstMask,
            Bitboard nonPromoMask,
            Bitboard promoMask = Bitboards::kAll
        ) {
            generatePrecalculatedWithColorAndOcc<kCanPromote>(
                dst,
                pos,
                pieces,
                [&](Square sq, [[maybe_unused]] Color c, [[maybe_unused]] Bitboard occ) { return attackGetter(sq); },
                dstMask,
                nonPromoMask,
                promoMask
            );
        }

        template <bool kChecksOnly>
        void generatePawns(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto stm = pos.stm();
            const auto nstm = stm.flip();
            const auto pawns = pos.pieceBb(PieceTypes::kPawn, stm);

            const auto checkMask = kChecksOnly ? attacks::pawnAttacks(pos.kingSq(nstm), nstm) : Bitboards::kAll;

            const auto shifted = pawns.shiftNorthRelative(stm) & dstMask & checkMask;

            const auto promos = shifted & Bitboards::promoArea(stm);
            const auto nonPromos = shifted & ~Bitboards::relativeRank(stm, 8);

            const auto offset = offsets::relativeOffset(stm, offsets::kNorth);

            serializePromotions(dst, offset, promos);
            serializeNormals(dst, offset, nonPromos);
        }

        template <bool kChecksOnly>
        void generateLances(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto nstm = pos.stm().flip();
            const auto ksq = pos.kingSq(nstm);
            const auto nonPromoMask = kChecksOnly ? attacks::pawnAttacks(ksq, nstm) : Bitboards::kAll;
            const auto promoMask = kChecksOnly ? attacks::goldAttacks(ksq, nstm) : Bitboards::kAll;

            const auto lances = pos.pieceBb(PieceTypes::kLance, pos.stm());
            generatePrecalculatedWithColorAndOcc<true>(
                dst,
                pos,
                lances,
                attacks::lanceAttacks,
                dstMask,
                ~Bitboards::relativeRank(pos.stm(), 8) & nonPromoMask,
                promoMask
            );
        }

        template <bool kChecksOnly>
        void generateKnights(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto nstm = pos.stm().flip();
            const auto ksq = pos.kingSq(nstm);
            const auto nonPromoMask = kChecksOnly ? attacks::knightAttacks(ksq, nstm) : Bitboards::kAll;
            const auto promoMask = kChecksOnly ? attacks::goldAttacks(ksq, nstm) : Bitboards::kAll;

            const auto knights = pos.pieceBb(PieceTypes::kKnight, pos.stm());
            generatePrecalculatedWithColor<true>(
                dst,
                pos,
                knights,
                attacks::knightAttacks,
                dstMask,
                ~(Bitboards::relativeRank(pos.stm(), 8) | Bitboards::relativeRank(pos.stm(), 7)) & nonPromoMask,
                promoMask
            );
        }

        template <bool kChecksOnly>
        void generateSilvers(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto nstm = pos.stm().flip();
            const auto ksq = pos.kingSq(nstm);
            const auto nonPromoMask = kChecksOnly ? attacks::silverAttacks(ksq, nstm) : Bitboards::kAll;
            const auto promoMask = kChecksOnly ? attacks::goldAttacks(ksq, nstm) : Bitboards::kAll;

            const auto silvers = pos.pieceBb(PieceTypes::kSilver, pos.stm());
            generatePrecalculatedWithColor<
                true>(dst, pos, silvers, attacks::silverAttacks, dstMask, nonPromoMask, promoMask);
        }

        template <bool kChecksOnly>
        void generateGolds(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto nstm = pos.stm().flip();
            const auto ksq = pos.kingSq(nstm);
            const auto mask = kChecksOnly ? attacks::goldAttacks(ksq, nstm) : Bitboards::kAll;

            const auto golds = pos.pieceBb(PieceTypes::kGold, pos.stm())
                             | pos.pieceBb(PieceTypes::kPromotedPawn, pos.stm())
                             | pos.pieceBb(PieceTypes::kPromotedLance, pos.stm())
                             | pos.pieceBb(PieceTypes::kPromotedKnight, pos.stm())
                             | pos.pieceBb(PieceTypes::kPromotedSilver, pos.stm());
            generatePrecalculatedWithColor<false>(dst, pos, golds, attacks::goldAttacks, dstMask, mask);
        }

        template <bool kChecksOnly>
        void generateBishops(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto ksq = pos.kingSq(pos.stm().flip());
            const auto nonPromoMask = kChecksOnly ? attacks::bishopAttacks(ksq, Bitboards::kAll) : Bitboards::kAll;
            const auto promoMask = kChecksOnly ? attacks::promotedBishopAttacks(ksq, Bitboards::kAll) : Bitboards::kAll;

            const auto bishops = pos.pieceBb(PieceTypes::kBishop, pos.stm());
            generatePrecalculatedWithOcc<
                true>(dst, pos, bishops, attacks::bishopAttacks, dstMask, nonPromoMask, promoMask);
        }

        template <bool kChecksOnly>
        void generateRooks(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto ksq = pos.kingSq(pos.stm().flip());
            const auto nonPromoMask = kChecksOnly ? attacks::rookAttacks(ksq, Bitboards::kAll) : Bitboards::kAll;
            const auto promoMask = kChecksOnly ? attacks::promotedRookAttacks(ksq, Bitboards::kAll) : Bitboards::kAll;

            const auto rooks = pos.pieceBb(PieceTypes::kRook, pos.stm());
            generatePrecalculatedWithOcc<true>(dst, pos, rooks, attacks::rookAttacks, dstMask, nonPromoMask, promoMask);
        }

        template <bool kChecksOnly>
        void generatePromotedBishops(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto ksq = pos.kingSq(pos.stm().flip());
            const auto mask = kChecksOnly ? attacks::promotedBishopAttacks(ksq, Bitboards::kAll) : Bitboards::kAll;

            const auto horses = pos.pieceBb(PieceTypes::kPromotedBishop, pos.stm());
            generatePrecalculatedWithOcc<false>(dst, pos, horses, attacks::promotedBishopAttacks, dstMask, mask);
        }

        template <bool kChecksOnly>
        void generatePromotedRooks(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto ksq = pos.kingSq(pos.stm().flip());
            const auto mask = kChecksOnly ? attacks::promotedRookAttacks(ksq, Bitboards::kAll) : Bitboards::kAll;

            const auto dragons = pos.pieceBb(PieceTypes::kPromotedRook, pos.stm());
            generatePrecalculatedWithOcc<false>(dst, pos, dragons, attacks::promotedRookAttacks, dstMask, mask);
        }

        void generateKings(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto kings = pos.pieceBb(PieceTypes::kKing, pos.stm());
            generatePrecalculated<false>(dst, pos, kings, attacks::kingAttacks, dstMask, Bitboards::kAll);
        }

        template <bool kChecksOnly>
        void generateDrops(MoveList& dst, const Position& pos, Bitboard dstMask) {
            if (dstMask.empty()) {
                return;
            }

            const auto stm = pos.stm();
            const auto nstm = stm.flip();
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

            const auto ksq = pos.kingSq(nstm);
            const auto pawnCheckMask = kChecksOnly ? attacks::pawnAttacks(ksq, nstm) : Bitboards::kAll;
            const auto lanceCheckMask =
                kChecksOnly ? attacks::lanceAttacks(ksq, nstm, Bitboards::kAll) : Bitboards::kAll;
            const auto knightCheckMask = kChecksOnly ? attacks::knightAttacks(ksq, nstm) : Bitboards::kAll;
            const auto silverCheckMask = kChecksOnly ? attacks::silverAttacks(ksq, nstm) : Bitboards::kAll;
            const auto goldCheckMask = kChecksOnly ? attacks::goldAttacks(ksq, nstm) : Bitboards::kAll;
            const auto bishopCheckMask = kChecksOnly ? attacks::bishopAttacks(ksq, Bitboards::kAll) : Bitboards::kAll;
            const auto rookCheckMask = kChecksOnly ? attacks::rookAttacks(ksq, Bitboards::kAll) : Bitboards::kAll;

            generate(
                PieceTypes::kPawn,
                ~Bitboards::relativeRank(stm, 8) & ~pos.pieceBb(PieceTypes::kPawn, stm).fillFile() & pawnCheckMask
            );
            generate(PieceTypes::kLance, ~Bitboards::relativeRank(stm, 8) & lanceCheckMask);
            generate(
                PieceTypes::kKnight,
                ~(Bitboards::relativeRank(stm, 8) | Bitboards::relativeRank(stm, 7)) & knightCheckMask
            );
            generate(PieceTypes::kSilver, silverCheckMask);
            generate(PieceTypes::kGold, goldCheckMask);
            generate(PieceTypes::kBishop, bishopCheckMask);
            generate(PieceTypes::kRook, rookCheckMask);
        }

        template <bool kGenerateDrops, bool kChecksOnly = false>
        void generate(MoveList& dst, const Position& pos, Bitboard dstMask) {
            if constexpr (!kChecksOnly) {
                generateKings(dst, pos, dstMask);
            }

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

            generatePawns<kChecksOnly>(dst, pos, dstMask);
            generateLances<kChecksOnly>(dst, pos, dstMask);
            generateKnights<kChecksOnly>(dst, pos, dstMask);
            generateSilvers<kChecksOnly>(dst, pos, dstMask);
            generateGolds<kChecksOnly>(dst, pos, dstMask);
            generateBishops<kChecksOnly>(dst, pos, dstMask);
            generateRooks<kChecksOnly>(dst, pos, dstMask);
            generatePromotedBishops<kChecksOnly>(dst, pos, dstMask);
            generatePromotedRooks<kChecksOnly>(dst, pos, dstMask);

            if constexpr (kGenerateDrops) {
                generateDrops<kChecksOnly>(dst, pos, dropMask);
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

    void generateNonCaptureChecks(MoveList& dst, const Position& pos) {
        const auto dstMask = ~pos.occupancy();
        generate<true, true>(dst, pos, dstMask);
    }
} // namespace stoat::movegen
