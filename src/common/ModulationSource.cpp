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

#include "ModulationSource.h"
#include "SurgeStorage.h"

class SurgeStorage;

namespace ModulatorName
{

std::string modulatorName(const SurgeStorage *s, int i, bool button, int current_scene,
                          int forScene)
{
    if (!s)
    {
        return "";
    }

    if ((i >= ms_lfo1 && i <= ms_slfo6))
    {
        int idx = i - ms_lfo1;
        bool isS = idx >= 6;
        int fnum = idx % 6;
        auto useScene = forScene >= 0 ? forScene : current_scene;
        auto *lfodata = &(s->getPatch().scene[useScene].lfo[i - ms_lfo1]);

        std::string sceneL = "Scene", shortsceneL = "S-";

        if (forScene >= 0)
        {
            sceneL = fmt::format("Scene {:c}", 'A' + forScene);
            shortsceneL = fmt::format("{:c} S-", 'A' + forScene);
        }

        std::string txt;

        if (lfodata->shape.val.i == lt_envelope)
        {
            if (button)
                txt = fmt::format("{:s}ENV {:d}", (isS ? shortsceneL : ""), fnum + 1);
            else
                txt = fmt::format("{:s} Envelope {:d}", (isS ? sceneL : "Voice"), fnum + 1);
        }
        else if (lfodata->shape.val.i == lt_stepseq)
        {
            if (button)
                txt = fmt::format("{:s}SEQ {:d}", (isS ? shortsceneL : ""), fnum + 1);
            else
                txt = fmt::format("{:s} Step Sequencer {:d}", (isS ? sceneL : "Voice"), fnum + 1);
        }
        else if (lfodata->shape.val.i == lt_mseg)
        {
            if (button)
                txt = fmt::format("{:s}MSEG {:d}", (isS ? shortsceneL : ""), fnum + 1);
            else
                txt = fmt::format("{:s} MSEG {:d}", (isS ? sceneL : "Voice"), fnum + 1);
        }
        else if (lfodata->shape.val.i == lt_formula)
        {
            if (button)
                txt = fmt::format("{:s}FORM {:d}", (isS ? shortsceneL : ""), fnum + 1);
            else
                txt = fmt::format("{:s} Formula {:d}", (isS ? sceneL : "Voice"), fnum + 1);
        }
        else
        {
            if (button)
                txt = fmt::format("{:s}LFO {:d}", (isS ? shortsceneL : ""), fnum + 1);
            else
                txt = fmt::format("{:s} LFO {:d}", (isS ? sceneL : "Voice"), fnum + 1);
        }

        return txt;
    }

    if (i >= ms_ctrl1 && i <= ms_ctrl8)
    {
        if (button)
        {
            std::string ccl = std::string(s->getPatch().CustomControllerLabel[i - ms_ctrl1]);

            if (ccl == "-")
            {
                return std::string(modsource_names[i]);
            }
            else
            {
                return ccl;
            }
        }
        else
        {
            std::string ccl = std::string(s->getPatch().CustomControllerLabel[i - ms_ctrl1]);

            if (ccl == "-")
            {
                return std::string(modsource_names[i]);
            }
            else
            {
                return ccl + " (" + modsource_names[i] + ")";
            }
        }
    }

    if (i == ms_aftertouch && s->mpeEnabled)
    {
        return "MPE Pressure";
    }

    if (i == ms_timbre && s->mpeEnabled)
    {
        return "MPE Timbre";
    }

    if (button)
    {
        return std::string(modsource_names_button[i]);
    }
    else
    {
        return std::string(modsource_names[i]);
    }
}

std::string modulatorIndexExtension(const SurgeStorage *s, int scene, int ms, int index,
                                    bool shortV)
{
    if (!s)
    {
        return "";
    }

    if (ModulatorName::supportsIndexedModulator(scene, (modsources)ms))
    {
        if (ms == ms_random_bipolar)
        {
            if (index == 0)
                return shortV ? "" : " (Uniform)";
            if (index == 1)
                return shortV ? " N" : " (Normal)";
        }
        if (ms == ms_random_unipolar)
        {
            if (index == 0)
                return shortV ? "" : " (Uniform)";
            if (index == 1)
                return shortV ? " HN" : " (Half Normal)";
        }

        /* This is a remnant from unimplemented #4286
        if (ms == ms_lowest_key || ms == ms_latest_key || ms == ms_highest_key)
        {
        return (index == 0 ? " Key" : " Voice");
        }
        */

        if (ms >= ms_lfo1 && ms <= ms_slfo6)
        {
            auto lf = &(s->getPatch().scene[scene].lfo[ms - ms_lfo1]);
            if (lf->shape.val.i != lt_formula)
            {
                if (index == 0)
                    return "";
                if (index == 1)
                {
                    if (shortV)
                        return " Raw";
                    return " (" + Surge::GUI::toOSCase("Raw Waveform") + ")";
                }
                if (index == 2)
                {
                    if (shortV)
                        return " EG";
                    return Surge::GUI::toOSCase(" (EG Only)");
                }
            }
        }

        if (shortV)
            return "." + std::to_string(index + 1);
        else
            return std::string(" Out ") + std::to_string(index + 1);
    }
    return "";
}

std::string modulatorNameWithIndex(const SurgeStorage *s, int scene, int ms, int index,
                                   bool forButton, bool useScene, bool baseNameOnly)
{
    if (!s)
    {
        return "";
    }

    int lfo_id = ms - ms_lfo1;
    bool hasOverride = isLFO((modsources)ms) && index >= 0 &&
                       s->getPatch().LFOBankLabel[scene][lfo_id][index][0] != 0;

    if (baseNameOnly)
    {
        auto base = modulatorName(s, ms, true, scene, useScene ? scene : -1);

        if (ModulatorName::supportsIndexedModulator(scene, (modsources)ms))
        {
            base += ModulatorName::modulatorIndexExtension(s, scene, ms, index, true);
        }

        return base;
    }

    if (!hasOverride)
    {
        auto base = modulatorName(s, ms, forButton, scene, useScene ? scene : -1);

        if (index >= 0 && ModulatorName::supportsIndexedModulator(scene, (modsources)ms))
        {
            base += ModulatorName::modulatorIndexExtension(s, scene, ms, index, forButton);
        }

        return base;
    }
    else
    {
        if (forButton)
        {
            return s->getPatch().LFOBankLabel[scene][ms - ms_lfo1][index];
        }

        // Long name is alias (button name)
        auto base = modulatorName(s, ms, true, scene, useScene ? scene : -1);

        if (ModulatorName::supportsIndexedModulator(scene, (modsources)ms))
        {
            base += ModulatorName::modulatorIndexExtension(s, scene, ms, index, true);
        }

        std::string res = s->getPatch().LFOBankLabel[scene][ms - ms_lfo1][index];
        res = res + " (" + base + ")";
        return res;
    }
}

// This is a copy of a trivial function from SurgeSynthesizer
bool supportsIndexedModulator(int scene, modsources modsource)
{
    if (modsource >= ms_lfo1 && modsource <= ms_slfo6)
    {
        return true;
        // auto lf = &(storage.getPatch().scene[scene].lfo[modsource - ms_lfo1]);
        // return lf->shape.val.i == lt_formula;
    }

    if (modsource == ms_random_bipolar || modsource == ms_random_unipolar)
    {
        return true;
    }

    return false;
}
} // namespace ModulatorName