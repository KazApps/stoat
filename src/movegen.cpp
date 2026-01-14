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

        template <bool kGeneratePromotions, bool kGenerateNonPromotions>
        void generatePrecalculatedWithColorAndOcc(
            MoveList& dst,
            const Position& pos,
            Bitboard pieces,
            auto attackGetter,
            Bitboard dstMask,
            Bitboard nonPromoMask = Bitboards::kAll
        ) {
            const auto stm = pos.stm();

            const auto occ = pos.occupancy();

            if constexpr (kGeneratePromotions) {
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

            if constexpr (kGenerateNonPromotions) {
                auto movable = pieces;
                while (!movable.empty()) {
                    const auto piece = movable.popLsb();
                    const auto attacks = attackGetter(piece, pos.stm(), occ) & dstMask & nonPromoMask;

                    serializeNormals(dst, piece, attacks);
                }
            }
        }

        template <bool kGeneratePromotions, bool kGenerateNonPromotions>
        void generatePrecalculatedWithColor(
            MoveList& dst,
            const Position& pos,
            Bitboard pieces,
            auto attackGetter,
            Bitboard dstMask,
            Bitboard nonPromoMask = Bitboards::kAll
        ) {
            generatePrecalculatedWithColorAndOcc<kGeneratePromotions, kGenerateNonPromotions>(
                dst,
                pos,
                pieces,
                [&](Square sq, Color c, [[maybe_unused]] Bitboard occ) { return attackGetter(sq, c); },
                dstMask,
                nonPromoMask
            );
        }

        template <bool kGeneratePromotions, bool kGenerateNonPromotions>
        void generatePrecalculatedWithOcc(
            MoveList& dst,
            const Position& pos,
            Bitboard pieces,
            auto attackGetter,
            Bitboard dstMask,
            Bitboard nonPromoMask = Bitboards::kAll
        ) {
            generatePrecalculatedWithColorAndOcc<kGeneratePromotions, kGenerateNonPromotions>(
                dst,
                pos,
                pieces,
                [&](Square sq, [[maybe_unused]] Color c, Bitboard occ) { return attackGetter(sq, occ); },
                dstMask,
                nonPromoMask
            );
        }

        template <bool kGeneratePromotions, bool kGenerateNonPromotions>
        void generatePrecalculated(
            MoveList& dst,
            const Position& pos,
            Bitboard pieces,
            auto attackGetter,
            Bitboard dstMask,
            Bitboard nonPromoMask = Bitboards::kAll
        ) {
            generatePrecalculatedWithColorAndOcc<kGeneratePromotions, kGenerateNonPromotions>(
                dst,
                pos,
                pieces,
                [&](Square sq, [[maybe_unused]] Color c, [[maybe_unused]] Bitboard occ) { return attackGetter(sq); },
                dstMask,
                nonPromoMask
            );
        }

        template <bool kGeneratePromotions, bool kGenerateNonPromotions>
        void generatePawns(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto stm = pos.stm();
            const auto pawns = pos.pieceBb(PieceTypes::kPawn, stm);

            const auto shifted = pawns.shiftNorthRelative(stm) & dstMask;

            const auto promos = shifted & Bitboards::promoArea(stm);
            const auto nonPromos = shifted & ~Bitboards::relativeRank(stm, 8);

            const auto offset = offsets::relativeOffset(stm, offsets::kNorth);

            if constexpr (kGeneratePromotions) {
                serializePromotions(dst, offset, promos);
            }

            if constexpr (kGenerateNonPromotions) {
                serializeNormals(dst, offset, nonPromos);
            }
        }

        template <bool kGeneratePromotions, bool kGenerateNonPromotions>
        void generateLances(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto lances = pos.pieceBb(PieceTypes::kLance, pos.stm());
            generatePrecalculatedWithColorAndOcc<kGeneratePromotions, kGenerateNonPromotions>(
                dst,
                pos,
                lances,
                attacks::lanceAttacks,
                dstMask,
                ~Bitboards::relativeRank(pos.stm(), 8)
            );
        }

        template <bool kGeneratePromotions, bool kGenerateNonPromotions>
        void generateKnights(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto knights = pos.pieceBb(PieceTypes::kKnight, pos.stm());
            generatePrecalculatedWithColor<kGeneratePromotions, kGenerateNonPromotions>(
                dst,
                pos,
                knights,
                attacks::knightAttacks,
                dstMask,
                ~(Bitboards::relativeRank(pos.stm(), 8) | Bitboards::relativeRank(pos.stm(), 7))
            );
        }

        template <bool kGeneratePromotions, bool kGenerateNonPromotions>
        void generateSilvers(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto silvers = pos.pieceBb(PieceTypes::kSilver, pos.stm());
            generatePrecalculatedWithColor<kGeneratePromotions, kGenerateNonPromotions>(
                dst,
                pos,
                silvers,
                attacks::silverAttacks,
                dstMask
            );
        }

        template <bool kGenerateNonPromotions>
        void generateGolds(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto golds = pos.pieceBb(PieceTypes::kGold, pos.stm())
                             | pos.pieceBb(PieceTypes::kPromotedPawn, pos.stm())
                             | pos.pieceBb(PieceTypes::kPromotedLance, pos.stm())
                             | pos.pieceBb(PieceTypes::kPromotedKnight, pos.stm())
                             | pos.pieceBb(PieceTypes::kPromotedSilver, pos.stm());
            generatePrecalculatedWithColor<false, kGenerateNonPromotions>(
                dst,
                pos,
                golds,
                attacks::goldAttacks,
                dstMask
            );
        }

        template <bool kGeneratePromotions, bool kGenerateNonPromotions>
        void generateBishops(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto bishops = pos.pieceBb(PieceTypes::kBishop, pos.stm());
            generatePrecalculatedWithOcc<kGeneratePromotions, kGenerateNonPromotions>(
                dst,
                pos,
                bishops,
                attacks::bishopAttacks,
                dstMask
            );
        }

        template <bool kGeneratePromotions, bool kGenerateNonPromotions>
        void generateRooks(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto rooks = pos.pieceBb(PieceTypes::kRook, pos.stm());
            generatePrecalculatedWithOcc<kGeneratePromotions, kGenerateNonPromotions>(
                dst,
                pos,
                rooks,
                attacks::rookAttacks,
                dstMask
            );
        }

        template <bool kGenerateNonPromotions>
        void generatePromotedBishops(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto horses = pos.pieceBb(PieceTypes::kPromotedBishop, pos.stm());
            generatePrecalculatedWithOcc<false, kGenerateNonPromotions>(dst, pos, horses, attacks::promotedBishopAttacks, dstMask);
        }

        template <bool kGenerateNonPromotions>
        void generatePromotedRooks(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto dragons = pos.pieceBb(PieceTypes::kPromotedRook, pos.stm());
            generatePrecalculatedWithOcc<false, kGenerateNonPromotions>(
                dst,
                pos,
                dragons,
                attacks::promotedRookAttacks,
                dstMask
            );
        }

        template <bool kGenerateNonPromotions>
        void generateKings(MoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto kings = pos.pieceBb(PieceTypes::kKing, pos.stm());
            generatePrecalculated<false, kGenerateNonPromotions>(dst, pos, kings, attacks::kingAttacks, dstMask);
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

        template <bool kGeneratePromotions, bool kGenerateNonPromotions, bool kGenerateDrops>
        void generate(MoveList& dst, const Position& pos, Bitboard dstMask) {
            generateKings<kGenerateNonPromotions>(dst, pos, dstMask);

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

            generatePawns<kGeneratePromotions, kGenerateNonPromotions>(dst, pos, dstMask);
            generateLances<kGeneratePromotions, kGenerateNonPromotions>(dst, pos, dstMask);
            generateKnights<kGeneratePromotions, kGenerateNonPromotions>(dst, pos, dstMask);
            generateSilvers<kGeneratePromotions, kGenerateNonPromotions>(dst, pos, dstMask);
            generateGolds<kGenerateNonPromotions>(dst, pos, dstMask);
            generateBishops<kGeneratePromotions, kGenerateNonPromotions>(dst, pos, dstMask);
            generateRooks<kGeneratePromotions, kGenerateNonPromotions>(dst, pos, dstMask);
            generatePromotedBishops<kGenerateNonPromotions>(dst, pos, dstMask);
            generatePromotedRooks<kGenerateNonPromotions>(dst, pos, dstMask);

            if constexpr (kGenerateDrops) {
                generateDrops(dst, pos, dropMask);
            }
        }
    } // namespace

    void generateAll(MoveList& dst, const Position& pos) {
        const auto dstMask = ~pos.colorBb(pos.stm());
        generate<true, true, true>(dst, pos, dstMask);
    }

    void generateCaptures(MoveList& dst, const Position& pos) {
        const auto dstMask = pos.colorBb(pos.stm().flip());
        generate<true, true, false>(dst, pos, dstMask);
    }

    void generateNonCaptures(MoveList& dst, const Position& pos) {
        const auto dstMask = ~pos.occupancy();
        generate<true, true, true>(dst, pos, dstMask);
    }

    void generateRecaptures(MoveList& dst, const Position& pos, Square captureSq) {
        assert(!pos.colorBb(pos.stm()).getSquare(captureSq));
        assert(pos.colorBb(pos.stm().flip()).getSquare(captureSq));

        const auto dstMask = Bitboard::fromSquare(captureSq);
        generate<true, true, false>(dst, pos, dstMask);
    }

    void generateNoisy(MoveList& dst, const Position& pos) {
        const auto dstMask = ~pos.occupancy();
        generate<true, false, false>(dst, pos, dstMask);

        generateCaptures(dst, pos);
    }

    void generateQuiet(MoveList& dst, const Position& pos) {
        const auto dstMask = ~pos.occupancy();
        generate<false, true, true>(dst, pos, dstMask);
    }
} // namespace stoat::movegen
