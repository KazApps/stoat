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

#include "ctrlc.h"

#include <cassert>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #ifndef NOMINMAX // mingw
        #define NOMINMAX
    #endif
    #include <Windows.h>
#else
    #include <signal.h>
#endif

namespace stoat::util::signal {
    namespace {
        CtrlCHandler s_handler{};
    }

    void setCtrlCHandler(CtrlCHandler handler) {
        assert(!s_handler);
        assert(handler);

        s_handler = std::move(handler);

#ifdef _WIN32
        const auto result = SetConsoleCtrlHandler(
            [](DWORD dwCtrlType) -> BOOL {
                if (dwCtrlType == CTRL_BREAK_EVENT) {
                    return FALSE;
                }

                s_handler();
                return TRUE;
            },
            TRUE
        );

        if (!result) {
            fmt::println(stderr, "failed to set ctrl+c handler");
        }
#else
        struct sigaction action{};

        action.sa_flags = SA_RESTART;
        action.sa_handler = []([[maybe_unused]] int signal) { s_handler(); };

        if (sigaction(SIGINT, &action, nullptr)) {
            fmt::println(stderr, "failed to set SIGINT handler");
        }

        if (sigaction(SIGTERM, &action, nullptr)) {
            fmt::println(stderr, "failed to set SIGTERM handler");
        }

        if (sigaction(SIGHUP, &action, nullptr)) {
            fmt::println(stderr, "failed to set SIGHUP handler");
        }
#endif
    }
} // namespace stoat::util::signal
