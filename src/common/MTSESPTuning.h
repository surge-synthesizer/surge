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

#ifndef SURGE_SRC_COMMON_MTSESPTUNING_H
#define SURGE_SRC_COMMON_MTSESPTUNING_H

#include "Tunings.h"

#include <array>
#include <bitset>
#include <string>

class MTSClient;

namespace Surge
{
namespace Storage
{

/*
 * Everything an MTS-ESP source tells its clients about the current tuning: a frequency and
 * a note filter flag per MIDI note, plus the optional description of the scale's shape.
 * A map size, start key or ref key of -1 means the source did not supply it.
 */
struct MTSESPTuningInfo
{
    std::array<double, 128> frequencies{};
    std::bitset<128> filtered{0};
    std::string scaleName;
    double periodRatio{2.0};
    int mapSize{-1}, mapStartKey{-1}, refKey{-1};

    bool operator==(const MTSESPTuningInfo &other) const;
    bool operator!=(const MTSESPTuningInfo &other) const { return !(*this == other); }
};

/*
 * Build a tuning which reproduces what an MTS-ESP source is sending, so the tuning editor
 * can show it with no special casing. MTS-ESP only carries frequencies, so the scale is
 * inferred from them and then checked against the whole table, falling back to a scale
 * spanning the entire keyboard when no smaller pattern matches. The fallback reproduces
 * any table exactly, so the result is always faithful, if not always compact.
 */
Tunings::Tuning tuningFromMTSESPInfo(const MTSESPTuningInfo &info);

// What Surge broadcasts when it acts as an MTS-ESP source for this tuning
MTSESPTuningInfo mtsESPInfoForTuning(const Tunings::Tuning &tuning);

#ifndef SURGE_SKIP_ODDSOUND_MTS
// What a client currently sees on MIDI channel 1
MTSESPTuningInfo mtsESPInfoFromClient(MTSClient *client);
#endif

} // namespace Storage
} // namespace Surge

#endif // SURGE_SRC_COMMON_MTSESPTUNING_H
