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

#ifndef SURGE_SRC_COMMON_WAVETABLESCRIPTMANAGER_H
#define SURGE_SRC_COMMON_WAVETABLESCRIPTMANAGER_H

#include "filesystem/import.h"
#include <vector>
class SurgeStorage;

namespace Surge
{
namespace Storage
{
struct WavetableScriptManager
{
    /*
     * Given a storage, scene, and OSC, stream it to a file relative to the location
     * in the user directory wavetable script area
     */
    void saveScriptToUser(const fs::path &location, SurgeStorage *s, int scene, int oscid);

    /*
     * Given a completed path, load the script into our storage
     */
    void loadScriptFrom(const fs::path &location, SurgeStorage *s, int scene, int oscid);

    /*
     * What are the scripts we have? In some form of category order
     */
    struct Script
    {
        std::string name;
        fs::path path;
    };

    struct Category
    {
        std::string name;
        std::string path;
        std::string parentPath;
        std::vector<Script> scripts;
    };

    enum class ScriptScanMode
    {
        FactoryOnly,
        UserOnly
    };

    std::vector<Category> getScripts(SurgeStorage *s, ScriptScanMode scanMode);
    void forceScriptRefresh();

    std::vector<Category> scannedUserScripts;
    bool haveScannedUser{false};

    std::vector<Category> scannedFactoryScripts;
    bool haveScannedFactory{false};
};
} // namespace Storage
} // namespace Surge
#endif // SURGE_SRC_COMMON_WAVETABLESCRIPTMANAGER_H
