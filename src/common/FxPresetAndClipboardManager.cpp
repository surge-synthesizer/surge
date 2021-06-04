/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "FxPresetAndClipboardManager.h"
#include "StringOps.h"
#include "Effect.h"

namespace Surge
{
namespace FxUserPreset
{

static std::unordered_map<int, std::vector<Preset>> scannedPresets;
static bool haveScannedPresets = false;

std::unordered_map<int, std::vector<Preset>> getPresetsByType() { return scannedPresets; }

void forcePresetRescan(SurgeStorage *storage)
{
    scannedPresets.clear();
    haveScannedPresets = true;

    auto ud = storage->userFXPath;

    std::vector<fs::path> sfxfiles;

    std::deque<fs::path> workStack;
    workStack.push_back(fs::path(ud));

    try
    {
        while (!workStack.empty())
        {
            auto top = workStack.front();
            workStack.pop_front();
            if (fs::is_directory(top))
            {
                for (auto &d : fs::directory_iterator(top))
                {
                    if (fs::is_directory(d))
                    {
                        workStack.push_back(d);
                    }
                    else if (path_to_string(d.path().extension()) == ".srgfx")
                    {
                        sfxfiles.push_back(d.path());
                    }
                }
            }
        }
    }
    catch (const fs::filesystem_error &e)
    {
        std::ostringstream oss;
        oss << "Experienced file system error when scanning user FX. " << e.what();

        if (storage)
            storage->reportError(oss.str(), "FileSystem Error");
    }

    for (const auto &f : sfxfiles)
    {
        {
            Preset preset;
            preset.file = path_to_string(f);
            TiXmlDocument d;
            int t;

            if (!d.LoadFile(f))
                goto badPreset;

            auto r = TINYXML_SAFE_TO_ELEMENT(d.FirstChild("single-fx"));

            if (!r)
                goto badPreset;

            auto s = TINYXML_SAFE_TO_ELEMENT(r->FirstChild("snapshot"));

            if (!s)
                goto badPreset;

            if (!s->Attribute("name"))
                goto badPreset;

            preset.name = s->Attribute("name");

            if (s->QueryIntAttribute("type", &t) != TIXML_SUCCESS)
                goto badPreset;

            preset.type = t;

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

                if (s->QueryDoubleAttribute((p + std::to_string(i) + "_extend_range").c_str(),
                                            &fl) == TIXML_SUCCESS &&
                    fl != 0)
                {
                    preset.er[i] = true;
                }

                if (s->QueryDoubleAttribute((p + std::to_string(i) + "_deactivated").c_str(),
                                            &fl) == TIXML_SUCCESS &&
                    fl != 0)
                {
                    preset.da[i] = true;
                }
            }

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
                return _stricmp(a.name.c_str(), b.name.c_str()) < 0;
            }
            else
            {
                return a.type < b.type;
            }
        });
    }
}

std::vector<Preset> getPresetsForSingleType(int id)
{
    if (scannedPresets.find(id) == scannedPresets.end())
    {
        return std::vector<Preset>();
    }
    return scannedPresets[id];
}

bool hasPresetsForSingleType(int id) { return scannedPresets.find(id) != scannedPresets.end(); }

void saveFxIn(SurgeStorage *storage, FxStorage *fx, const std::string &s)
{
    char fxName[TXT_SIZE];
    fxName[0] = 0;
    strxcpy(fxName, s.c_str(), TXT_SIZE);

    if (strlen(fxName) == 0)
    {
        return;
    }

    if (!Surge::Storage::isValidName(fxName))
    {
        return;
    }

    int ti = fx->type.val.i;

    std::ostringstream oss;
    oss << storage->userFXPath << PATH_SEPARATOR << fx_type_names[ti] << PATH_SEPARATOR;

    auto pn = oss.str();
    fs::create_directories(string_to_path(pn));

    auto fn = pn + fxName + ".srgfx";
    std::ofstream pfile(fn, std::ios::out);
    if (!pfile.is_open())
    {
        storage->reportError(std::string("Unable to open FX preset file '") + fn + "' for writing!",
                             "Error");
        return;
    }

    // this used to say streaming_versio before (was a typo)
    // make sure both variants are checked when checking sv in the future on patch load
    pfile << "<single-fx streaming_version=\"" << ff_revision << "\">\n";

    // take care of 5 special XML characters
    std::string fxNameSub(fxName);
    Surge::Storage::findReplaceSubstring(fxNameSub, std::string("&"), std::string("&amp;"));
    Surge::Storage::findReplaceSubstring(fxNameSub, std::string("<"), std::string("&lt;"));
    Surge::Storage::findReplaceSubstring(fxNameSub, std::string(">"), std::string("&gt;"));
    Surge::Storage::findReplaceSubstring(fxNameSub, std::string("\""), std::string("&quot;"));
    Surge::Storage::findReplaceSubstring(fxNameSub, std::string("'"), std::string("&apos;"));

    pfile << "  <snapshot name=\"" << fxNameSub.c_str() << "\" \n";

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
        }
    }

    pfile << "  />\n";
    pfile << "</single-fx>\n";
    pfile.close();

    Surge::FxUserPreset::forcePresetRescan(storage);
}

void loadPresetOnto(const Preset &p, SurgeStorage *storage, FxStorage *fxbuffer)
{
    fxbuffer->type.val.i = p.type;

    Effect *t_fx = spawn_effect(fxbuffer->type.val.i, storage, fxbuffer, 0);

    if (t_fx)
    {
        t_fx->init_ctrltypes();
        t_fx->init_default_values();
        delete t_fx;
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
        fxbuffer->p[i].extend_range = (int)p.er[i];
        fxbuffer->p[i].deactivated = (int)p.da[i];
    }
}
} // namespace FxUserPreset

namespace FxClipboard
{
Clipboard::Clipboard() {}

void copyFx(SurgeStorage *storage, FxStorage *fx, Clipboard &cb)
{
    cb.fxCopyPaste.clear();
    cb.fxCopyPaste.resize(n_fx_params * 4 + 1); // type then (val; ts; extend; deact)
    cb.fxCopyPaste[0] = fx->type.val.i;
    for (int i = 0; i < n_fx_params; ++i)
    {
        int vp = i * 4 + 1;
        int tp = i * 4 + 2;
        int xp = i * 4 + 3;
        int dp = i * 4 + 4;

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
    }
}

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
        int vp = i * 4 + 1;
        int tp = i * 4 + 2;
        int xp = i * 4 + 3;
        int dp = i * 4 + 4;

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
        fxbuffer->p[i].extend_range = (int)cb.fxCopyPaste[xp];
        fxbuffer->p[i].deactivated = (int)cb.fxCopyPaste[dp];
    }

    cb.fxCopyPaste.clear();
}
} // namespace FxClipboard
} // namespace Surge