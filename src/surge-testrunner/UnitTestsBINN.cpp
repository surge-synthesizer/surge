/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */
#include "catch2/catch_amalgamated.hpp"
#include "binn/binn.h"

TEST_CASE("binn security invariants", "[binn]")
{
    SECTION("Inflated list size does not overflow heap")
    {
        /* Crafted list header claiming 0x7FFFFFFF bytes but carrying only a
           handful of bytes of actual data. Must not crash or corrupt memory.
           With ASan enabled this catches any resulting out-of-bounds write. */
        unsigned char payload[] = {
            0xE0,                         // type: list
            0x7F, 0xFF, 0xFF, 0xFF,       // size: INT_MAX
            0x00, 0x00, 0x00, 0x01,       // count: 1
            0xA0, 0x10,                   // string type, claimed length 16
            'A',  'A',  'A',  'A',  0x00  // only 5 bytes of string data
        };
        binn *obj = binn_open(payload);
        if (obj)
            binn_free(obj);
        REQUIRE(true);
    }

    SECTION("Item size exceeding container does not corrupt heap")
    {
        /* Map container claims 12-byte total size but contains a blob item
           with a claimed length of 0xFFFFFFFF. Must not crash. */
        unsigned char payload[] = {
            0xE1,                               // type: map
            0x00, 0x00, 0x00, 0x0C,             // size: 12
            0x00, 0x00, 0x00, 0x01,             // count: 1
            0x00, 0x01,                         // key id: 1
            0xC0, 0xFF, 0xFF, 0xFF, 0xFF        // blob with huge claimed length
        };
        binn *obj = binn_open(payload);
        if (obj)
        {
            int val = 0;
            binn_map_get_int32(obj, 1, &val);
            binn_free(obj);
        }
        REQUIRE(true);
    }

    SECTION("Valid list round-trips correctly after security hardening")
    {
        binn *list = binn_list();
        binn_list_add_int32(list, 42);
        binn *obj = binn_open(binn_ptr(list));
        REQUIRE(obj != nullptr);
        int result = 0;
        BOOL ret = binn_list_get_int32(obj, 1, &result);
        REQUIRE(ret == TRUE);
        REQUIRE(result == 42);
        binn_free(obj);
        binn_free(list);
    }
}
