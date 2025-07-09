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

#include "uci_like.h"

#include <algorithm>
#include <iterator>

#include "../eval/eval.h"
#include "../eval/nnue.h"
#include "../limit.h"
#include "../perft.h"
#include "../ttable.h"
#include "../util/parse.h"
#include "common.h"

namespace stoat::protocol {
    UciLikeHandler::UciLikeHandler(EngineState& state) :
            m_state{state} {
#define REGISTER_HANDLER(Command) \
    registerCommandHandler(#Command, [this](auto args, auto startTime) { handle_##Command(args, startTime); })

        REGISTER_HANDLER(position);
        REGISTER_HANDLER(go);
        REGISTER_HANDLER(stop);
        REGISTER_HANDLER(setoption);

        REGISTER_HANDLER(d);
        REGISTER_HANDLER(splitperft);
        REGISTER_HANDLER(raweval);

#undef REGISTER_HANDLER
    }

    void UciLikeHandler::printInitialInfo() const {
        fmt::println("id name {} {}", kName, kVersion);
        fmt::println("id author {}", kAuthor);

        fmt::print("option name ");
        printOptionName("Hash");
        fmt::println(
            " type spin default {} min {} max {}",
            tt::kDefaultTtSizeMib,
            tt::kTtSizeRange.min(),
            tt::kTtSizeRange.max()
        );

        fmt::print("option name ");
        printOptionName("Threads");
        fmt::println(
            " type spin default {} min {} max {}",
            kDefaultThreadCount,
            kThreadCountRange.min(),
            kThreadCountRange.max()
        );

        fmt::print("option name ");
        printOptionName("MultiPV");
        fmt::println(" type spin default {} min {} max {}", kDefaultMultiPv, kMultiPvRange.min(), kMultiPvRange.max());

        fmt::print("option name ");
        printOptionName("MoveOverhead");
        fmt::println(
            " type spin default {} min {} max {}",
            kDefaultMoveOverhead,
            kMoveOverheadRange.min(),
            kMoveOverheadRange.max()
        );

        fmt::print("option name ");
        printOptionName("CuteChessWorkaround");
        fmt::println(" type check default false");

        finishInitialInfo();
    }

    CommandResult UciLikeHandler::handleCommand(
        std::string_view command,
        std::span<std::string_view> args,
        util::Instant startTime
    ) {
        if (command == "quit") {
            return CommandResult::kQuit;
        }

        if (auto itr = m_cmdHandlers.find(command); itr != m_cmdHandlers.end()) {
            itr->second(args, startTime);
            return CommandResult::kContinue;
        }

        return CommandResult::kUnknown;
    }

    void UciLikeHandler::printSearchInfo(const SearchInfo& info) const {
        fmt::print("info");

        if (info.multiPv > 1) {
            fmt::print(" multipv {}", info.pvIdx + 1);
        }

        fmt::print(" depth {}", info.depth);

        if (info.seldepth) {
            fmt::print(" seldepth {}", *info.seldepth);
        }

        if (info.timeSec) {
            const auto ms = static_cast<usize>(*info.timeSec * 1000.0);
            fmt::print(" time {}", ms);
        }

        fmt::print(" nodes {}", info.nodes);

        if (info.timeSec) {
            const auto nps = static_cast<usize>(static_cast<f64>(info.nodes) / *info.timeSec);
            fmt::print(" nps {}", nps);
        }

        fmt::print(" score ");

        if (std::holds_alternative<MateDisplayScore>(info.score)) {
            const auto plies = std::get<MateDisplayScore>(info.score).plies;
            fmt::print("mate ");
            printMateScore(plies);
        } else {
            const auto score = std::get<CpDisplayScore>(info.score).score;
            fmt::print("cp {}", score);
        }

        if (info.scoreBound == ScoreBound::kUpperBound) {
            fmt::print(" upperbound");
        } else if (info.scoreBound == ScoreBound::kLowerBound) {
            fmt::print(" lowerbound");
        }

        if (info.hashfull) {
            fmt::print(" hashfull {}", *info.hashfull);
        }

        fmt::print(" pv");

        for (usize i = 0; i < info.pv.length; ++i) {
            fmt::print(" ");
            printMove(info.pv.moves[i]);
        }

        fmt::println("");
    }

    void UciLikeHandler::printInfoString(std::string_view str) const {
        fmt::println("info string {}", str);
    }

    void UciLikeHandler::printBestMove(Move move) const {
        fmt::print("bestmove ");
        printMove(move);
        fmt::println("");
    }

    void UciLikeHandler::registerCommandHandler(std::string_view command, CommandHandlerType handler) {
        if (m_cmdHandlers.contains(command)) {
            fmt::println(stderr, "tried to overwrite command handler for '{}'", command);
            return;
        }

        m_cmdHandlers[std::string{command}] = std::move(handler);
    }

    void UciLikeHandler::handleNewGame() {
        if (m_state.searcher->isSearching()) {
            fmt::println(stderr, "Still searching");
            return;
        }

        m_state.searcher->newGame();
    }

    void UciLikeHandler::handle_position(std::span<std::string_view> args, [[maybe_unused]] util::Instant startTime) {
        if (m_state.searcher->isSearching()) {
            fmt::println(stderr, "Still searching");
            return;
        }

        if (args.empty()) {
            return;
        }

        usize next = 0;

        if (args[0] == "startpos") {
            m_state.pos = Position::startpos();
            m_state.posHistory.clear();

            next = 1;
        } else {
            const auto count = std::distance(args.begin(), std::ranges::find(args, "moves"));

            if (count == 0) {
                fmt::println(stderr, "Missing position");
                return;
            }

            if (auto parsed = parsePosition(args.subspan(0, count))) {
                m_state.pos = parsed.take();
                m_state.posHistory.clear();
            } else {
                if (const auto err = parsed.takeErr()) {
                    fmt::println("{}", *err);
                }
                return;
            }

            next = count;
        }

        assert(next <= args.size());

        if (next >= args.size() || args[next] != "moves") {
            return;
        }

        for (usize i = next + 1; i < args.size(); ++i) {
            if (auto parsedMove = parseMove(args[i])) {
                m_state.posHistory.push_back(m_state.pos);
                m_state.pos = m_state.pos.applyMove(parsedMove.take());
            } else {
                fmt::println(stderr, "Invalid move '{}'", args[i]);
                break;
            }
        }
    }

    void UciLikeHandler::handle_go(std::span<std::string_view> args, util::Instant startTime) {
        if (m_state.searcher->isSearching()) {
            fmt::println(stderr, "Still searching");
            return;
        }

        auto limiter = std::make_unique<limit::CompoundLimiter>();

        bool infinite = false;

        auto maxDepth = kMaxDepth;

        std::optional<f64> btime{};
        std::optional<f64> wtime{};

        std::optional<f64> binc{};
        std::optional<f64> winc{};

        std::optional<f64> byoyomi{};

        for (i32 i = 0; i < args.size(); ++i) {
            if (args[i] == "infinite") {
                infinite = true;
            } else if (args[i] == "depth") {
                if (++i == args.size()) {
                    fmt::println(stderr, "Missing depth");
                    return;
                }

                if (!util::tryParse(maxDepth, args[i])) {
                    fmt::println(stderr, "Invalid depth '{}'", args[i]);
                    return;
                }
            } else if (args[i] == "nodes") {
                if (++i == args.size()) {
                    fmt::println(stderr, "Missing node limit");
                    return;
                }

                usize maxNodes{};

                if (!util::tryParse(maxNodes, args[i])) {
                    fmt::println(stderr, "Invalid node limit '{}'", args[i]);
                    return;
                }

                limiter->addLimiter<limit::NodeLimiter>(maxNodes);
            } else if (args[i] == "movetime") {
                if (++i == args.size()) {
                    fmt::println(stderr, "Missing move time limit");
                    return;
                }

                i64 maxTimeMs{};

                if (!util::tryParse(maxTimeMs, args[i])) {
                    fmt::println(stderr, "Invalid move time limit '{}'", args[i]);
                    return;
                }

                maxTimeMs = std::max<i64>(maxTimeMs, 1);

                const auto maxTimeSec = static_cast<f64>(maxTimeMs) / 1000.0;
                limiter->addLimiter<limit::MoveTimeLimiter>(startTime, maxTimeSec);
            } else if (args[i] == btimeToken()) {
                if (++i == args.size()) {
                    fmt::println(stderr, "Missing {} limit", btimeToken());
                    return;
                }

                i64 btimeMs{};

                if (!util::tryParse(btimeMs, args[i])) {
                    fmt::println(stderr, "Invalid {} limit '{}'", btimeToken(), args[i]);
                    return;
                }

                btimeMs = std::max<i64>(btimeMs, 1);
                btime = static_cast<f64>(btimeMs) / 1000.0;
            } else if (args[i] == wtimeToken()) {
                if (++i == args.size()) {
                    fmt::println(stderr, "Missing {} limit", wtimeToken());
                    return;
                }

                i64 wtimeMs{};

                if (!util::tryParse(wtimeMs, args[i])) {
                    fmt::println(stderr, "Invalid {} limit '{}'", wtimeToken(), args[i]);
                    return;
                }

                wtimeMs = std::max<i64>(wtimeMs, 1);
                wtime = static_cast<f64>(wtimeMs) / 1000.0;
            } else if (args[i] == bincToken()) {
                if (++i == args.size()) {
                    fmt::println(stderr, "Missing {} limit", bincToken());
                    return;
                }

                i64 bincMs{};

                if (!util::tryParse(bincMs, args[i])) {
                    fmt::println(stderr, "Invalid {} limit '{}'", bincToken(), args[i]);
                    return;
                }

                bincMs = std::max<i64>(bincMs, 0);
                binc = static_cast<f64>(bincMs) / 1000.0;
            } else if (args[i] == wincToken()) {
                if (++i == args.size()) {
                    fmt::println(stderr, "Missing {} limit", wincToken());
                    return;
                }

                i64 wincMs{};

                if (!util::tryParse(wincMs, args[i])) {
                    fmt::println(stderr, "Invalid {} limit '{}'", wincToken(), args[i]);
                    return;
                }

                wincMs = std::max<i64>(wincMs, 0);
                winc = static_cast<f64>(wincMs) / 1000.0;
            } else if (args[i] == "byoyomi") {
                if (++i == args.size()) {
                    fmt::println(stderr, "Missing byoyomi");
                    return;
                }

                i64 byoyomiMs{};

                if (!util::tryParse(byoyomiMs, args[i])) {
                    fmt::println(stderr, "Invalid byoyomi '{}'", args[i]);
                    return;
                }

                byoyomiMs = std::max<i64>(byoyomiMs, 0);
                byoyomi = static_cast<f64>(byoyomiMs) / 1000.0;
            } else if (args[i] == "mate") {
                printInfoString("go mate not supported");
                printGoMateResponse();
                return;
            }
        }

        const auto time = m_state.pos.stm() == Colors::kBlack ? btime : wtime;
        const auto inc = m_state.pos.stm() == Colors::kBlack ? binc : winc;

        if (time) {
            const limit::TimeLimits limits{
                .remaining = *time,
                .increment = inc.value_or(0.0),
                .byoyomi = byoyomi.value_or(0.0),
            };

            limiter->addLimiter<limit::TimeManager>(startTime, limits, m_state.moveOverhead);
        } else if (inc) {
            printInfoString("Warning: increment given but no time, ignoring");
        }

        m_state.searcher
            ->startSearch(m_state.pos, m_state.posHistory, startTime, infinite, maxDepth, std::move(limiter));
    }

    void UciLikeHandler::handle_stop(std::span<std::string_view> args, [[maybe_unused]] util::Instant startTime) {
        if (m_state.searcher->isSearching()) {
            m_state.searcher->stop();
        } else {
            fmt::println(stderr, "Not searching");
        }
    }

    void UciLikeHandler::handle_setoption(std::span<std::string_view> args, [[maybe_unused]] util::Instant startTime) {
        if (m_state.searcher->isSearching()) {
            fmt::println(stderr, "Still searching");
            return;
        }

        if (args.size() < 2 || args[0] != "name") {
            return;
        }

        //TODO handle options more generically

        const auto valueIdx = std::distance(args.begin(), std::ranges::find(args, "value"));

        if (valueIdx == 1) {
            fmt::println(stderr, "Missing option name");
            return;
        }

        if (valueIdx >= args.size() - 1) {
            fmt::println(stderr, "Missing value");
            return;
        }

        if (valueIdx > 2) {
            std::string str{};
            auto itr = std::back_inserter(str);

            bool first = true;
            for (usize i = 2; i < valueIdx; ++i) {
                if (!first) {
                    fmt::format_to(itr, " {}", args[i]);
                } else {
                    fmt::format_to(itr, "{}", args[i]);
                    first = false;
                }
            }

            printInfoString(fmt::format("Warning: spaces in option names not supported, skipping \"{}\"", str));
        }

        std::string name{};
        name.reserve(args[1].length());
        std::ranges::transform(args[1], std::back_inserter(name), [](char c) {
            return static_cast<char>(std::tolower(c));
        });

        name = transformOptionName(name);

        std::string value{};
        auto itr = std::back_inserter(value);

        bool first = true;
        for (usize i = valueIdx + 1; i < args.size(); ++i) {
            if (!first) {
                fmt::format_to(itr, " {}", args[i]);
            } else {
                fmt::format_to(itr, "{}", args[i]);
                first = false;
            }
        }

        assert(!value.empty());

        if (name == "hash") {
            if (const auto newHash = util::tryParse<usize>(value)) {
                const auto size = tt::kTtSizeRange.clamp(*newHash);
                m_state.searcher->setTtSize(size);
            } else {
                fmt::println(stderr, "Invalid hash size '{}'", value);
            }
        } else if (name == "threads") {
            if (const auto newThreadCount = util::tryParse<u32>(value)) {
                const auto threadCount = kThreadCountRange.clamp(*newThreadCount);
                m_state.searcher->setThreadCount(threadCount);
            } else {
                fmt::println(stderr, "Invalid thread count '{}'", value);
            }
        } else if (name == "multipv") {
            if (const auto newMultiPv = util::tryParse<u32>(value)) {
                const auto multiPv = kMultiPvRange.clamp(*newMultiPv);
                m_state.searcher->setMultiPv(multiPv);
            } else {
                fmt::println(stderr, "Invalid multiPV count '{}'", value);
            }
        } else if (name == "moveoverhead") {
            if (const auto newMoveOverhead = util::tryParse<u32>(value)) {
                const auto moveOverhead = kMoveOverheadRange.clamp(*newMoveOverhead);
                m_state.moveOverhead = moveOverhead;
            } else {
                fmt::println(stderr, "Invalid move overhead '{}'", value);
            }
        } else if (name == "cutechessworkaround") {
            if (const auto newCcWorkaround = util::tryParseBool(value)) {
                m_state.searcher->setCuteChessWorkaround(*newCcWorkaround);
            } else {
                fmt::println(stderr, "Invalid check value '{}'", value);
            }
        } else {
            fmt::println(stderr, "Unknown option '{}'", value);
        }
    }

    void UciLikeHandler::handle_d(
        [[maybe_unused]] std::span<std::string_view> args,
        [[maybe_unused]] util::Instant startTime
    ) {
        fmt::println("");
        printBoard(m_state.pos);

        fmt::println("");
        fmt::println("");
        printFenLine(m_state.pos);

        fmt::println("Key: {:016x}", m_state.pos.key());

        fmt::print("Checkers:");
        auto checkers = m_state.pos.checkers();
        while (!checkers.empty()) {
            fmt::print(" {}", checkers.popLsb());
        }
        fmt::println("");

        fmt::print("Pinned:");
        auto pinned = m_state.pos.pinned();
        while (!pinned.empty()) {
            fmt::print(" {}", pinned.popLsb());
        }
        fmt::println("");

        const auto staticEval = eval::staticEvalOnce(m_state.pos);
        fmt::println("Static eval: {:+}.{:02}", staticEval / 100, std::abs(staticEval) % 100);
    }

    void UciLikeHandler::handle_splitperft(std::span<std::string_view> args, [[maybe_unused]] util::Instant startTime) {
        if (args.empty()) {
            return;
        }

        if (const auto depth = util::tryParse<i32>(args[0])) {
            splitPerft(m_state.pos, *depth);
        }
    }

    void UciLikeHandler::handle_raweval(
        [[maybe_unused]] std::span<std::string_view> args,
        [[maybe_unused]] util::Instant startTime
    ) {
        fmt::println("{}", eval::nnue::evaluateOnce(m_state.pos));
    }
} // namespace stoat::protocol
