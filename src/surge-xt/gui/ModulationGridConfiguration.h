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
#ifndef SURGE_SRC_SURGE_XT_GUI_MODULATIONGRIDCONFIGURATION_H
#define SURGE_SRC_SURGE_XT_GUI_MODULATIONGRIDCONFIGURATION_H

#include "ModulationSource.h"

#include <unordered_map>

namespace Surge
{
namespace GUI
{
struct ModulationGrid : juce::DeletedAtShutdown
{
    ModulationGrid()
    {
        add(ms_original, 0, 0);

        add(ms_velocity, 0, 3);
        add(ms_keytrack, 6, 7);
        add(ms_polyaftertouch, 2, 3);
        add(ms_aftertouch, 3, 3);
        add(ms_pitchbend, 4, 3);
        add(ms_modwheel, 5, 3);

        for (int i = ms_ctrl1; i <= ms_ctrl8; ++i)
        {
            add((modsources)i, (int)i - ms_ctrl1, 0);
        }

        add(ms_ampeg, 7, 5);
        add(ms_filtereg, 6, 5);

        for (int i = ms_lfo1; i <= ms_lfo6; ++i)
        {
            add((modsources)i, (int)i - ms_lfo1, 5);
        }
        for (int i = ms_slfo1; i <= ms_slfo6; ++i)
        {
            add((modsources)i, (int)i - ms_slfo1, 7);
        }

        add(ms_timbre, 9, 3);
        add(ms_releasevelocity, 1, 3);
        add(ms_random_bipolar, 8, 5, {ms_random_unipolar});
        add(ms_alternate_bipolar, 9, 5, {ms_alternate_unipolar});
        add(ms_breath, 6, 3);
        add(ms_sustain, 8, 3);
        add(ms_expression, 7, 3);
        add(ms_lowest_key, 7, 7);
        add(ms_highest_key, 8, 7);
        add(ms_latest_key, 9, 7);
    }

    // This is only called at dll shutdown basically
    ~ModulationGrid() { grid = nullptr; }

    void add(modsources ms, int x, int y) { data[ms] = Entry{x, y, ms}; }
    void add(modsources ms, int x, int y, const std::vector<modsources> &alternates)
    {
        data[ms] = Entry{x, y, ms, alternates};
        for (auto a : alternates)
        {
            data[a] = Entry{a, ms};
        }
    }
    struct Entry
    {
        Entry() {}
        Entry(int x, int y, modsources ms) : x(x), y(y), ms(ms) {}
        Entry(int x, int y, modsources ms, const std::vector<modsources> &alternates)
            : x(x), y(y), ms(ms), alternates(alternates)
        {
        }
        Entry(modsources sub, modsources primary)
            : ms(sub), associatedPrimary(primary), isPrimary(false)
        {
        }

        modsources ms{ms_original};
        modsources associatedPrimary{ms_original};
        int x{-1}, y{-1};
        bool isPrimary{true};
        std::vector<modsources> alternates;
    };

    std::unordered_map<modsources, Entry> data;

    const Entry &get(modsources ms) const
    {
        jassert(data.find(ms) != data.end());
        return data.at(ms);
    }

    static ModulationGrid *grid;
    static inline const ModulationGrid *getModulationGrid()
    {
        // The juce::DeletedAtShutdown makes sure this gets cleared up
        if (!grid)
            grid = new ModulationGrid();
        return grid;
    }
};
} // namespace GUI
} // namespace Surge

#endif // SURGE_XT_MODULATIONGRIDCONFIGURATION_H
