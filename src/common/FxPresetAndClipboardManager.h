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

#ifndef SURGE_SRC_COMMON_FXPRESETANDCLIPBOARDMANAGER_H
#define SURGE_SRC_COMMON_FXPRESETANDCLIPBOARDMANAGER_H

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
