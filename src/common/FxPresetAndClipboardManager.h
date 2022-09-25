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

#ifndef SURGE_XT_FXPRESETANDCLIPBOARDMANAGER_H
#define SURGE_XT_FXPRESETANDCLIPBOARDMANAGER_H

#include "SurgeStorage.h"

#include <vector>
#include <unordered_map>
#include <string>

namespace Surge
{
namespace Storage
{
struct FxUserPreset
{
    struct Preset
    {
        std::string file;
        std::string name;
        int streamingVersion{ff_revision};
        fs::path subPath{};
        bool isFactory{false};
        int type{-1};
        float p[n_fx_params];
        bool ts[n_fx_params], er[n_fx_params], da[n_fx_params];
        int dt[n_fx_params];

        Preset()
        {
            type = 0;
            isFactory = false;

            for (int i = 0; i < n_fx_params; ++i)
            {
                p[i] = 0.0;
                ts[i] = false;
                er[i] = false;
                da[i] = false;
                dt[i] = -1;
            }
        }
    };

    std::unordered_map<int, std::vector<Preset>> scannedPresets;
    bool haveScannedPresets{false};

    void doPresetRescan(SurgeStorage *storage, bool forceRescan = false);
    std::unordered_map<int, std::vector<Preset>> getPresetsByType();
    std::vector<Preset> getPresetsForSingleType(int type_id);
    bool hasPresetsForSingleType(int type_id);
    bool readFromXMLSnapshot(Preset &p, TiXmlElement *);

    void saveFxIn(SurgeStorage *s, FxStorage *fxdata, const std::string &fn);

    void loadPresetOnto(const Preset &p, SurgeStorage *s, FxStorage *fxbuffer);
};
} // namespace Storage

namespace FxClipboard
{
struct Clipboard
{
    Clipboard();
    std::vector<float> fxCopyPaste;
};
void copyFx(SurgeStorage *, FxStorage *from, Clipboard &cb);
bool isPasteAvailable(const Clipboard &cb);
void pasteFx(SurgeStorage *, FxStorage *onto, Clipboard &cb);
} // namespace FxClipboard
} // namespace Surge
#endif // SURGE_XT_FXPRESETANDCLIPBOARDMANAGER_H
