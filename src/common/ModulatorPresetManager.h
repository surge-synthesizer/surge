/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#pragma once

#include "filesystem/import.h"
#include <vector>
class SurgeStorage;

namespace Surge
{
namespace ModulatorPreset
{
/*
 * Given a storage, scene, and LFO, stream stream it to a file relative to the location
 * in the user directory LFO presets area
 */
void savePresetToUser(const fs::path &location, SurgeStorage *s, int scene, int lfo);

/*
 * Given a compelted path, load the preset into our storage
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
} // namespace ModulatorPreset
} // namespace Surge