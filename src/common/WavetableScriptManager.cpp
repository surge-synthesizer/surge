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

#include "WavetableScriptManager.h"
#include <iostream>
#include "DebugHelpers.h"
#include "SurgeStorage.h"
#include "tinyxml/tinyxml.h"
#include "sst/plugininfra/strnatcmp.h"

namespace Surge
{
namespace Storage
{

const static std::string ScriptDir = "Wavetable Scripts";
const static std::string ScriptExt = ".wtscript";

void WavetableScriptManager::saveScriptToUser(const fs::path &location, SurgeStorage *s, int scene,
                                              int oscid)
{
    try
    {
        auto osc = &(s->getPatch().scene[scene].osc[oscid]);
        auto containingPath = s->userDataPath / fs::path{ScriptDir};

        // validate location before using
        if (!location.is_relative())
        {
            s->reportError(
                "Please use relative paths when saving scripts. Referring to drive names directly "
                "and using absolute paths is not allowed!",
                "Relative Path Required");
            return;
        }

        auto comppath = containingPath;
        auto fullLocation =
            (containingPath / location).lexically_normal().replace_extension(ScriptExt);

        // make sure your category isnt "../../../etc/config"
        auto [_, compIt] = std::mismatch(fullLocation.begin(), fullLocation.end(), comppath.begin(),
                                         comppath.end());
        if (compIt != comppath.end())
        {
            s->reportError("Your save path is not a directory inside the user scripts directory. "
                           "This usually means you are trying to use ../ in your script name.",
                           "Invalid Save Path");
            return;
        }

        fs::create_directories(fullLocation.parent_path());

        TiXmlDeclaration decl("1.0", "UTF-8", "yes");

        TiXmlDocument doc;
        doc.InsertEndChild(decl);

        TiXmlElement wts("wts");

        TiXmlElement params("params");
        params.SetAttribute("frames", osc->wavetable_formula_nframes);
        params.SetAttribute("samples", osc->wavetable_formula_res_base);
        wts.InsertEndChild(params);

        TiXmlElement sc("wavetable_script");
        s->getPatch().wtsToXMLElement(&(s->getPatch().scene[scene].osc[oscid]), sc);
        wts.InsertEndChild(sc);

        doc.InsertEndChild(wts);

        if (!doc.SaveFile(fullLocation))
        {
            // uhh ... do something I guess?
            std::cout << "Could not save" << std::endl;
        }

        forceScriptRefresh();
    }
    catch (const fs::filesystem_error &e)
    {
        std::ostringstream oss;
        oss << "Exception occurred while attempting to write the wavetable script! "
               "Most likely, invalid characters or a reserved name was used to name "
               "the script. Please try again with a different name!\n"
            << "Details " << e.what();
        s->reportError(oss.str(), "Script Write Error");
    }
}

/*
 * Given a completed path, load the script into our storage
 */
void WavetableScriptManager::loadScriptFrom(const fs::path &location, SurgeStorage *s, int scene,
                                            int oscid)
{
    auto osc = &(s->getPatch().scene[scene].osc[oscid]);

    TiXmlDocument doc;
    doc.LoadFile(location);
    auto wts = TINYXML_SAFE_TO_ELEMENT(doc.FirstChildElement("wts"));
    if (!wts)
    {
        std::cout << "Unable to find wts node in document" << std::endl;
        return;
    }

    auto params = TINYXML_SAFE_TO_ELEMENT(wts->FirstChildElement("params"));
    if (!params)
    {
        std::cout << "NO PARAMS" << std::endl;
        return;
    }

    int nframes = 0;
    if (params->QueryIntAttribute("frames", &nframes) == TIXML_SUCCESS)
        osc->wavetable_formula_nframes = nframes;

    int res_base = 0;
    if (params->QueryIntAttribute("samples", &res_base) == TIXML_SUCCESS)
        osc->wavetable_formula_res_base = res_base;

    auto sc = wts->FirstChildElement("wavetable_script");
    if (sc)
        s->getPatch().wtsFromXMLElement(&(s->getPatch().scene[scene].osc[oscid]), sc);
}

/*
 * Note: Clients rely on this being sorted by category path if you change it
 */
std::vector<WavetableScriptManager::Category>
WavetableScriptManager::getScripts(SurgeStorage *s, ScriptScanMode scanMode)
{
    if (scanMode == ScriptScanMode::UserOnly && haveScannedUser)
        return scannedUserScripts;
    if (scanMode == ScriptScanMode::FactoryOnly && haveScannedFactory)
        return scannedFactoryScripts;

    std::vector<fs::path> scanTargets;
    if (scanMode == ScriptScanMode::UserOnly)
        scanTargets.push_back(s->userDataPath / fs::path{ScriptDir});
    if (scanMode == ScriptScanMode::FactoryOnly)
        scanTargets.push_back(s->datapath / fs::path{"wavetable_scripts"});

    std::map<std::string, Category> resMap; // handy it is sorted!

    for (const auto &p : scanTargets)
    {
        try
        {
            for (auto &d : fs::recursive_directory_iterator(p))
            {
                auto dp = fs::path(d);
                auto base = dp.stem();
                auto ext = dp.extension();
                if (path_to_string(ext) != ScriptExt)
                {
                    continue;
                }
                auto rd = path_to_string(dp.replace_filename(fs::path()));
                rd = rd.substr(path_to_string(p).length() + 1);
                rd = rd.substr(0, rd.length() - 1);

                auto catName = rd;
                auto ppos = rd.rfind(fs::path::preferred_separator);
                auto pd = std::string();
                if (ppos != std::string::npos)
                {
                    pd = rd.substr(0, ppos);
                    catName = rd.substr(ppos + 1);
                }
                if (resMap.find(rd) == resMap.end())
                {
                    resMap[rd] = Category();
                    resMap[rd].name = catName;
                    resMap[rd].parentPath = pd;
                    resMap[rd].path = rd;

                    /*
                     * We only create categories if we find a script. So that means parent
                     * directories with just subdirs need categories made. This recurses up as far
                     * as we need to go
                     */
                    while (pd != "" && resMap.find(pd) == resMap.end())
                    {
                        auto cd = pd;
                        catName = cd;
                        ppos = cd.rfind(fs::path::preferred_separator);

                        if (ppos != std::string::npos)
                        {
                            pd = cd.substr(0, ppos);
                            catName = cd.substr(ppos + 1);
                        }
                        else
                        {
                            pd = "";
                        }

                        resMap[cd] = Category();
                        resMap[cd].name = catName;
                        resMap[cd].parentPath = pd;
                        resMap[cd].path = cd;
                    }
                }

                Script prs;
                prs.name = path_to_string(base);
                prs.path = fs::path(d);
                resMap[rd].scripts.push_back(prs);
            }
        }
        catch (const fs::filesystem_error &e)
        {
            // That's OK!
        }
    }

    std::vector<Category> res;
    for (auto &m : resMap)
    {
        std::sort(m.second.scripts.begin(), m.second.scripts.end(),
                  [](const Script &a, const Script &b) {
                      return strnatcasecmp(a.name.c_str(), b.name.c_str()) < 0;
                  });

        res.push_back(m.second);
    }

    if (scanMode == ScriptScanMode::UserOnly)
    {
        scannedUserScripts = res;
        haveScannedUser = true;
    }
    if (scanMode == ScriptScanMode::FactoryOnly)
    {
        scannedFactoryScripts = res;
        haveScannedFactory = true;
    }

    return res;
}

void WavetableScriptManager::forceScriptRefresh()
{
    haveScannedUser = false;
    scannedUserScripts.clear();

    haveScannedFactory = false;
    scannedFactoryScripts.clear();
}
} // namespace Storage
} // namespace Surge
