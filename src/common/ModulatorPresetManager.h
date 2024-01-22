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

#ifndef SURGE_SRC_COMMON_MODULATORPRESETMANAGER_H
#define SURGE_SRC_COMMON_MODULATORPRESETMANAGER_H

#include "filesystem/import.h"
#include <vector>
class SurgeStorage;

namespace Surge
{
namespace Storage
{
struct ModulatorPreset
{
    /*
     * Given a storage, scene, and LFO, stream stream it to a file relative to the location
     * in the user directory LFO presets area
     */
    void savePresetToUser(const fs::path &location, SurgeStorage *s, int scene, int lfo);

    /*
     * Given a completed path, load the preset into our storage
     */
    void loadPresetFrom(const fs::path &location, SurgeStorage *s, int scene, int lfo);

    /*
     * What are the presets we have? In some form of category order
     */
    struct Preset
    {
        std::string name;
        fs::path path;
    };

    struct Category
    {
        std::string name;
        std::string path;
        std::string parentPath;
        std::vector<Preset> presets;
    };

    std::vector<Category> getPresets(SurgeStorage *s);
    void forcePresetRescan();

    std::vector<Category> scanedPresets;
    bool haveScanedPresets{false};
};
} // namespace Storage
} // namespace Surge
#endif // SURGE_SRC_COMMON_MODULATORPRESETMANAGER_H
