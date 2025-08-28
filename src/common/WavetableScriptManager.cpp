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
#include "WavetableScriptEvaluator.h"
#include "tinyxml/tinyxml.h"
#include "sst/plugininfra/strnatcmp.h"

namespace Surge
{
namespace Storage
{

const static fs::path ScriptDir = "Wavetables/Scripted";
const static fs::path ScriptExt = ".wtscript";

/*
 * Given a storage, scene, and OSC, stream to a .wtscript file relative to the location
 * in the user directory wavetable script area
 */
void WavetableScriptManager::saveScriptToUser(const fs::path &location, SurgeStorage *s, int scene,
                                              int oscid)
{
    try
    {
        auto osc = &(s->getPatch().scene[scene].osc[oscid]);

        auto containingPath = s->userDataPath / ScriptDir;

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
        TiXmlElement wtscript("wtscript");
        TiXmlElement params("params");
        TiXmlElement script("wavetable_script");

        if (!osc->wavetable_formula.empty())
        {
            params.SetAttribute("frames", osc->wavetable_formula_nframes);
            params.SetAttribute("samples", osc->wavetable_formula_res_base);

            auto wtfo = osc->wavetable_formula;
            auto wtfol = wtfo.length();

            script.SetAttribute(
                "lua", Surge::Storage::base64_encode((unsigned const char *)wtfo.c_str(), wtfol));
        }

        wtscript.InsertEndChild(params);
        wtscript.InsertEndChild(script);
        doc.InsertEndChild(wtscript);
        if (!doc.SaveFile(fullLocation))
        {
            s->reportError("Failed to save XML file.", "XML Save Error");
        }

        s->refresh_wtlist();
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
 * Given a completed path, load the script and generate the wavetable
 */
bool WavetableScriptManager::loadScriptFrom(const fs::path &filename, SurgeStorage *s,
                                            OscillatorStorage *osc)
{
    TiXmlDocument doc;
    if (!doc.LoadFile(filename))
    {
        s->reportError("Failed to load XML file", "XML Load Error");
        return false;
    }

    auto wtscript = TINYXML_SAFE_TO_ELEMENT(doc.FirstChildElement("wtscript"));
    if (!wtscript)
    {
        s->reportError("No root <wtscript> element found", "XML Load Error");
        return false;
    }

    auto params = TINYXML_SAFE_TO_ELEMENT(wtscript->FirstChildElement("params"));
    if (!params)
    {
        s->reportError("No <params> element found", "XML Load Error");
        return false;
    }

    int nframes = 0;
    if (params->QueryIntAttribute("frames", &nframes) != TIXML_SUCCESS)
    {
        s->reportError("Missing or invalid frames attribute", "XML Load Error");
        return false;
    }

    int res_base = 0;
    if (params->QueryIntAttribute("samples", &res_base) != TIXML_SUCCESS)
    {
        s->reportError("Missing or invalid samples attribute", "XML Load Error");
        return false;
    }

    auto wavetable_script =
        TINYXML_SAFE_TO_ELEMENT(wtscript->FirstChildElement("wavetable_script"));
    if (!wavetable_script)
    {
        s->reportError("No <wavetable_script> element found", "XML Load Error");
        return false;
    }

    auto bscript = wavetable_script->Attribute("lua");
    if (!bscript)
    {
        s->reportError("No lua attribute found in <wavetable_script>", "XML Load Error");
        return false;
    }

    osc->wavetable_formula_nframes = nframes;
    osc->wavetable_formula_res_base = res_base;
    auto script = Surge::Storage::base64_decode(bscript);
    osc->wavetable_formula = script;

    auto respt = 32;

    for (int i = 1; i < res_base; ++i)
        respt *= 2;

    if (!evaluator)
        evaluator = std::make_unique<Surge::WavetableScript::LuaWTEvaluator>();

    evaluator->setStorage(s);
    evaluator->setScript(script);
    evaluator->setResolution(respt);
    evaluator->setFrameCount(nframes);

    wt_header wh;
    float *wd = nullptr;
    evaluator->populateWavetable(wh, &wd);
    s->waveTableDataMutex.lock();
    bool wasBuilt = osc->wt.BuildWT(wd, wh, wh.flags & wtf_is_sample);
    osc->wavetable_display_name = evaluator->getSuggestedWavetableName();
    s->waveTableDataMutex.unlock();

    delete[] wd;

    return wasBuilt;
}
} // namespace Storage
} // namespace Surge
