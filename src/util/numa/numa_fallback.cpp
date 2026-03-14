/*
 * Stoat, a USI shogi engine
 * Copyright (C) 2026 Ciekce
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

#ifndef ST_USE_LIBNUMA
    #include "numa.h"

namespace stoat::numa {
    bool init() {
        return true;
    }

    void bindThread(u32 numaId) {
        ST_UNUSED(numaId);
    }

    i32 nodeCount() {
        return 1;
    }
} // namespace stoat::numa
#endif
