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

#include "correction.h"

#include "attacks/attacks.h"

#include <cmath>

namespace stoat {
    namespace {
        u64 splitMix64(u64 x) {
            x += 0x9e3779b97f4a7c15;
            x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9;
            x = (x ^ (x >> 27)) * 0x94d049bb133111eb;

            return x ^ (x >> 31);
        }

        u64 getHashKey(Bitboard bb) {
            const auto [high, low] = fromU128(bb.raw());
            return splitMix64(high) ^ splitMix64(low);
        }

        std::pair<u64, u64> getAttackKey(const Position& pos) {
            u64 blackAttackKey{}, whiteAttackKey{};

            auto blackBishops = pos.pieceBb(Pieces::kBlackBishop) | pos.pieceBb(Pieces::kBlackPromotedBishop);
            auto blackRooks = pos.pieceBb(Pieces::kBlackRook) | pos.pieceBb(Pieces::kBlackPromotedRook);
            auto whiteBishops = pos.pieceBb(Pieces::kWhiteBishop) | pos.pieceBb(Pieces::kWhitePromotedBishop);
            auto whiteRooks = pos.pieceBb(Pieces::kWhiteRook) | pos.pieceBb(Pieces::kWhitePromotedRook);

            const auto occ = pos.occupancy();

            while (!blackBishops.empty()) {
                const auto sq = blackBishops.popLsb();
                blackAttackKey ^= getHashKey(attacks::bishopAttacks(sq, occ));
            }

            while (!blackRooks.empty()) {
                const auto sq = blackRooks.popLsb();
                blackAttackKey ^= getHashKey(attacks::rookAttacks(sq, occ));
            }

            while (!whiteBishops.empty()) {
                const auto sq = whiteBishops.popLsb();
                whiteAttackKey ^= getHashKey(attacks::bishopAttacks(sq, occ));
            }

            while (!whiteRooks.empty()) {
                const auto sq = whiteRooks.popLsb();
                whiteAttackKey ^= getHashKey(attacks::rookAttacks(sq, occ));
            }

            return {blackAttackKey, whiteAttackKey};
        }
    } // namespace

    void CorrectionHistory::clear() {
        std::memset(&m_tables, 0, sizeof(m_tables));
        std::memset(&m_cont, 0, sizeof(m_cont));
    }

    void CorrectionHistory::update(
        const Position& pos,
        std::span<const u64> keyHistory,
        i32 depth,
        Score searchScore,
        Score staticEval,
        i32 complexity
    ) {
        auto& tables = m_tables[pos.stm().idx()];

        const double factor = 1.0 + std::log2(complexity + 1) / 10.0;

        const auto bonus =
            std::clamp(static_cast<i32>((searchScore - staticEval) * depth / 8 * factor), -kMaxBonus, kMaxBonus);

        tables.castle[pos.castleKey() % kEntries].update(bonus);
        tables.cavalry[pos.cavalryKey() % kEntries].update(bonus);
        tables.hand[pos.kingHandKey() % kEntries].update(bonus);
        tables.kpr[pos.kprKey() % kEntries].update(bonus);

        const auto [blackAttack, whiteAttack] = getAttackKey(pos);

        tables.blackAttack[blackAttack % kEntries].update(bonus);
        tables.whiteAttack[whiteAttack % kEntries].update(bonus);

        const auto updateCont = [&](const u64 offset) {
            if (keyHistory.size() >= offset) {
                m_cont[(pos.key() ^ keyHistory[keyHistory.size() - offset]) % kEntries].update(bonus);
            }
        };

        updateCont(1);
        updateCont(2);
    }

    i32 CorrectionHistory::correction(const Position& pos, std::span<const u64> keyHistory) const {
        const auto& tables = m_tables[pos.stm().idx()];

        i32 correction{};

        correction += 128 * tables.castle[pos.castleKey() % kEntries];
        correction += 128 * tables.cavalry[pos.cavalryKey() % kEntries];
        correction += 128 * tables.hand[pos.kingHandKey() % kEntries];
        correction += 128 * tables.kpr[pos.kprKey() % kEntries];

        const auto [blackAttack, whiteAttack] = getAttackKey(pos);

        correction += 128 * tables.blackAttack[blackAttack % kEntries];
        correction += 128 * tables.whiteAttack[whiteAttack % kEntries];

        const auto applyCont = [&](const u64 offset) {
            if (keyHistory.size() >= offset) {
                correction += 128 * m_cont[(pos.key() ^ keyHistory[keyHistory.size() - offset]) % kEntries];
            }
        };

        applyCont(1);
        applyCont(2);

        return correction / 2048;
    }
} // namespace stoat
