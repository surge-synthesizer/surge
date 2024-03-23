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

#include "FxPresetAndClipboardManager.h"
#include "StringOps.h"
#include "Effect.h"
#include "DebugHelpers.h"

namespace Surge
{
namespace Storage
{

std::unordered_map<int, std::vector<FxUserPreset::Preset>> FxUserPreset::getPresetsByType()
{
    return scannedPresets;
}

void FxUserPreset::doPresetRescan(SurgeStorage *storage, bool forceRescan)
{
    if (haveScannedPresets && !forceRescan)
        return;

    // auto tb = Surge::Debug::TimeBlock(__func__);
    scannedPresets.clear();
    haveScannedPresets = true;

    auto ud = storage->userFXPath;
    auto fd = storage->datapath / "fx_presets";

    std::vector<std::pair<fs::path, bool>> sfxfiles;

    std::deque<std::pair<fs::path, bool>> workStack;
    workStack.emplace_back(fs::path(ud), false);
    workStack.emplace_back(fd, true);

    try
    {
        while (!workStack.empty())
        {
            auto top = workStack.front();
            workStack.pop_front();
            if (fs::is_directory(top.first))
            {
                for (auto &d : fs::directory_iterator(top.first))
                {
                    if (fs::is_directory(d))
                    {
                        workStack.emplace_back(d, top.second);
                    }
                    else if (path_to_string(d.path().extension()) == ".srgfx")
                    {
                        sfxfiles.emplace_back(d.path(), top.second);
                    }
                }
            }
        }

        for (const auto &f : sfxfiles)
        {
            {
                Preset preset;
                preset.file = path_to_string(f.first);

                TiXmlDocument d;
                int t;

                if (!d.LoadFile(f.first))
                    goto badPreset;

                auto r = TINYXML_SAFE_TO_ELEMENT(d.FirstChild("single-fx"));

                if (!r)
                    goto badPreset;

                preset.streamingVersion = ff_revision;
                int sv;
                if (r->QueryIntAttribute("streaming_version", &sv) == TIXML_SUCCESS)
                {
                    preset.streamingVersion = sv;
                }

                auto s = TINYXML_SAFE_TO_ELEMENT(r->FirstChild("snapshot"));

                if (!s)
                    goto badPreset;

                if (s->QueryIntAttribute("type", &t) != TIXML_SUCCESS)
                    goto badPreset;

                preset.type = t;
                preset.isFactory = f.second;

                fs::path rpath;

                if (f.second)
                    rpath = f.first.lexically_relative(fd).parent_path();
                else
                    rpath = f.first.lexically_relative(storage->userFXPath).parent_path();

                auto startCatPath = rpath.begin();
                if (*(startCatPath) == fx_type_shortnames[t])
                {
                    startCatPath++;
                }

                while (startCatPath != rpath.end())
                {
                    preset.subPath /= *startCatPath;
                    startCatPath++;
                }

                if (!readFromXMLSnapshot(preset, s))
                    goto badPreset;

                if (scannedPresets.find(preset.type) == scannedPresets.end())
                {
                    scannedPresets[preset.type] = std::vector<Preset>();
                }

                scannedPresets[preset.type].push_back(preset);
            }

        badPreset:;
        }

        for (auto &a : scannedPresets)
        {
            std::sort(a.second.begin(), a.second.end(), [](const Preset &a, const Preset &b) {
                if (a.type == b.type)
                {
                    if (a.isFactory != b.isFactory)
                    {
                        return a.isFactory;
                    }

                    if (a.subPath != b.subPath)
                    {
                        return a.subPath < b.subPath;
                    }

                    return _stricmp(a.name.c_str(), b.name.c_str()) < 0;
                }
                else
                {
                    return a.type < b.type;
                }
            });
        }
    }
    catch (const fs::filesystem_error &e)
    {
        std::ostringstream oss;
        oss << "Experienced file system error when scanning user FX. " << e.what();

        if (storage)
            storage->reportError(oss.str(), "FileSystem Error");
    }
}

bool FxUserPreset::readFromXMLSnapshot(Preset &preset, TiXmlElement *s)
{
    if (!s->Attribute("name"))
        return false;

    preset.name = s->Attribute("name");

    int t;
    if (s->QueryIntAttribute("type", &t) == TIXML_SUCCESS)
    {
        // The individual entry overrides the type. Thats OK!
        preset.type = t;
    }

    for (int i = 0; i < n_fx_params; ++i)
    {
        double fl;
        std::string p = "p";

        if (s->QueryDoubleAttribute((p + std::to_string(i)).c_str(), &fl) == TIXML_SUCCESS)
        {
            preset.p[i] = fl;
        }

        if (s->QueryDoubleAttribute((p + std::to_string(i) + "_temposync").c_str(), &fl) ==
                TIXML_SUCCESS &&
            fl != 0)
        {
            preset.ts[i] = true;
        }

        if (s->QueryDoubleAttribute((p + std::to_string(i) + "_extend_range").c_str(), &fl) ==
                TIXML_SUCCESS &&
            fl != 0)
        {
            preset.er[i] = true;
        }

        if (s->QueryDoubleAttribute((p + std::to_string(i) + "_deactivated").c_str(), &fl) ==
                TIXML_SUCCESS &&
            fl != 0)
        {
            preset.da[i] = true;
        }

        if (s->QueryDoubleAttribute((p + std::to_string(i) + "_deform_type").c_str(), &fl) ==
            TIXML_SUCCESS)
        {
            preset.dt[i] = (int)fl;
        }
    }
    return true;
}

std::vector<FxUserPreset::Preset> FxUserPreset::getPresetsForSingleType(int id)
{
    if (scannedPresets.find(id) == scannedPresets.end())
    {
        return std::vector<Preset>();
    }
    return scannedPresets[id];
}

bool FxUserPreset::hasPresetsForSingleType(int id)
{
    return scannedPresets.find(id) != scannedPresets.end();
}

void FxUserPreset::saveFxIn(SurgeStorage *storage, FxStorage *fx, const std::string &s)
{
    try
    {
        char fxName[TXT_SIZE];
        fxName[0] = 0;
        strxcpy(fxName, s.c_str(), TXT_SIZE);

        if (strlen(fxName) == 0)
        {
            return;
        }

        /*
         * OK so lets see if there's a path separator in the string
         */
        auto sp = string_to_path(s);
        auto spp = sp.parent_path();
        auto fnp = sp.filename();

        if (!Surge::Storage::isValidName(path_to_string(fnp)))
        {
            return;
        }

        int ti = fx->type.val.i;

        auto storagePath = storage->userFXPath / fs::path(fx_type_shortnames[ti]);

        if (!spp.empty())
            storagePath /= spp;

        auto outputPath = storagePath / string_to_path(path_to_string(fnp) + ".srgfx");

        fs::create_directories(storagePath);

        std::ofstream pfile(outputPath, std::ios::out);
        if (!pfile.is_open())
        {
            storage->reportError(std::string("Unable to open FX preset file '") +
                                     path_to_string(outputPath) + "' for writing!",
                                 "Error");
            return;
        }

        // this used to say streaming_versio before (was a typo)
        // make sure both variants are checked when checking sv in the future on patch load
        pfile << "<single-fx streaming_version=\"" << ff_revision << "\">\n";

        // take care of 5 special XML characters
        std::string fxNameSub(path_to_string(fnp));
        Surge::Storage::findReplaceSubstring(fxNameSub, std::string("&"), std::string("&amp;"));
        Surge::Storage::findReplaceSubstring(fxNameSub, std::string("<"), std::string("&lt;"));
        Surge::Storage::findReplaceSubstring(fxNameSub, std::string(">"), std::string("&gt;"));
        Surge::Storage::findReplaceSubstring(fxNameSub, std::string("\""), std::string("&quot;"));
        Surge::Storage::findReplaceSubstring(fxNameSub, std::string("'"), std::string("&apos;"));

        pfile << "  <snapshot name=\"" << fxNameSub.c_str() << "\"\n";

        pfile << "     type=\"" << fx->type.val.i << "\"\n";
        for (int i = 0; i < n_fx_params; ++i)
        {
            if (fx->p[i].ctrltype != ct_none)
            {
                switch (fx->p[i].valtype)
                {
                case vt_float:
                    pfile << "     p" << i << "=\"" << fx->p[i].val.f << "\"\n";
                    break;
                case vt_int:
                    pfile << "     p" << i << "=\"" << fx->p[i].val.i << "\"\n";
                    break;
                }

                if (fx->p[i].can_temposync() && fx->p[i].temposync)
                {
                    pfile << "     p" << i << "_temposync=\"1\"\n";
                }
                if (fx->p[i].can_extend_range() && fx->p[i].extend_range)
                {
                    pfile << "     p" << i << "_extend_range=\"1\"\n";
                }
                if (fx->p[i].can_deactivate() && fx->p[i].deactivated)
                {
                    pfile << "     p" << i << "_deactivated=\"1\"\n";
                }

                if (fx->p[i].has_deformoptions())
                {
                    pfile << "     p" << i << "_deform_type=\"" << fx->p[i].deform_type << "\"\n";
                }
            }
        }

        pfile << "  />\n";
        pfile << "</single-fx>\n";
        pfile.close();

        doPresetRescan(storage, true);
    }
    catch (const fs::filesystem_error &e)
    {
        storage->reportError(e.what(), "Unable to save FX Preset");
    }
}

void FxUserPreset::loadPresetOnto(const Preset &p, SurgeStorage *storage, FxStorage *fxbuffer)
{
    fxbuffer->type.val.i = p.type;

    Effect *t_fx = spawn_effect(fxbuffer->type.val.i, storage, fxbuffer, 0);

    if (t_fx)
    {
        t_fx->init_ctrltypes();
        t_fx->init_default_values();
    }

    for (int i = 0; i < n_fx_params; i++)
    {
        switch (fxbuffer->p[i].valtype)
        {
        case vt_float:
        {
            fxbuffer->p[i].val.f = p.p[i];
        }
        break;
        case vt_int:
            fxbuffer->p[i].val.i = (int)p.p[i];
            break;
        default:
            break;
        }

        fxbuffer->p[i].temposync = (int)p.ts[i];
        fxbuffer->p[i].set_extend_range((int)p.er[i]);
        fxbuffer->p[i].deactivated = (int)p.da[i];

        // only set deform type if it could be read from the xml
        if (p.dt[i] >= 0)
        {
            fxbuffer->p[i].deform_type = p.dt[i];
        }
    }

    if (t_fx)
    {
        // we need to handle streaming mismatches after we've set all parameters from the preset,
        // because the for loop above goes through all 12 parameters irrespective of the effect
        // having them or not, which in some cases can clobber param extensions like deactivated,
        // extend, deform etc.
        if (p.streamingVersion != ff_revision)
        {
            t_fx->handleStreamingMismatches(p.streamingVersion, ff_revision);
        }

        delete t_fx;
    }
}
} // namespace Storage

namespace FxClipboard
{
Clipboard::Clipboard() {}

void copyFx(SurgeStorage *storage, FxStorage *fx, Clipboard &cb)
{
    cb.fxCopyPaste.clear();
    cb.fxCopyPaste.resize(n_fx_params * 5 + 1); // type then (val; deform; ts; extend; deact)
    cb.fxCopyPaste[0] = fx->type.val.i;
    for (int i = 0; i < n_fx_params; ++i)
    {
        int vp = i * 5 + 1;
        int tp = i * 5 + 2;
        int xp = i * 5 + 3;
        int dp = i * 5 + 4;
        int dt = i * 5 + 5;

        switch (fx->p[i].valtype)
        {
        case vt_float:
            cb.fxCopyPaste[vp] = fx->p[i].val.f;
            break;
        case vt_int:
            cb.fxCopyPaste[vp] = fx->p[i].val.i;
            break;
        }

        cb.fxCopyPaste[tp] = fx->p[i].temposync;
        cb.fxCopyPaste[xp] = fx->p[i].extend_range;
        cb.fxCopyPaste[dp] = fx->p[i].deactivated;

        if (fx->p[i].has_deformoptions())
            cb.fxCopyPaste[dt] = fx->p[i].deform_type;
    }
}

bool isPasteAvailable(const Clipboard &cb) { return !cb.fxCopyPaste.empty(); }

void pasteFx(SurgeStorage *storage, FxStorage *fxbuffer, Clipboard &cb)
{
    if (cb.fxCopyPaste.empty())
        return;

    fxbuffer->type.val.i = (int)cb.fxCopyPaste[0];

    Effect *t_fx = spawn_effect(fxbuffer->type.val.i, storage, fxbuffer, 0);
    if (t_fx)
    {
        t_fx->init_ctrltypes();
        t_fx->init_default_values();
        delete t_fx;
    }

    for (int i = 0; i < n_fx_params; i++)
    {
        int vp = i * 5 + 1;
        int tp = i * 5 + 2;
        int xp = i * 5 + 3;
        int dp = i * 5 + 4;
        int dt = i * 5 + 5;

        switch (fxbuffer->p[i].valtype)
        {
        case vt_float:
        {
            fxbuffer->p[i].val.f = cb.fxCopyPaste[vp];
            if (fxbuffer->p[i].val.f < fxbuffer->p[i].val_min.f)
            {
                fxbuffer->p[i].val.f = fxbuffer->p[i].val_min.f;
            }
            if (fxbuffer->p[i].val.f > fxbuffer->p[i].val_max.f)
            {
                fxbuffer->p[i].val.f = fxbuffer->p[i].val_max.f;
            }
        }
        break;
        case vt_int:
            fxbuffer->p[i].val.i = (int)cb.fxCopyPaste[vp];
            break;
        default:
            break;
        }
        fxbuffer->p[i].temposync = (int)cb.fxCopyPaste[tp];
        fxbuffer->p[i].set_extend_range((int)cb.fxCopyPaste[xp]);
        fxbuffer->p[i].deactivated = (int)cb.fxCopyPaste[dp];

        if (fxbuffer->p[i].has_deformoptions())
            fxbuffer->p[i].deform_type = (int)cb.fxCopyPaste[dt];
    }

    cb.fxCopyPaste.clear();
}
} // namespace FxClipboard
} // namespace Surge