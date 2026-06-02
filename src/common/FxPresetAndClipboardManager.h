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

        // Extra attributes that go in the userdata.
        std::string filename; // special for convolver
        std::string userdata; // whole thing

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

struct FxChainUserPreset
{
    struct Preset
    {
        std::string file;
        std::string name;
        int streamingVersion{ff_revision};
        fs::path subPath{};

        struct SlotData
        {
            int type{0};
            float p[n_fx_params]{};
            bool ts[n_fx_params]{};
            bool er[n_fx_params]{};
            bool da[n_fx_params]{};
            int dt[n_fx_params]{};

            std::string presetName;

            // Extra attributes that go in the userdata.
            std::string filename; // special for convolver
            std::string userdata; // whole thing
        };
        SlotData slots[n_fx_per_chain];

        Preset()
        {
            for (auto &s : slots)
            {
                s.type = 0;
                for (int i = 0; i < n_fx_params; ++i)
                {
                    s.p[i] = 0.f;
                    s.ts[i] = false;
                    s.er[i] = false;
                    s.da[i] = false;
                    s.dt[i] = -1;
                }
            }
        }
    };

    std::vector<Preset> scannedPresets;
    bool haveScannedPresets{false};

    void doPresetRescan(SurgeStorage *storage, bool forceRescan = false);
    std::vector<Preset> getPresets() const { return scannedPresets; }

    bool readFromXMLSnapshot(Preset &p, TiXmlElement *snapshotEl);
    void saveChainPresetIn(SurgeStorage *s, const Preset::SlotData slots[n_fx_per_chain],
                           const std::string &fn);
    void loadPresetOnto(const Preset &p, SurgeStorage *s, FxStorage *fxbuffer[n_fx_per_chain]);
};

} // namespace Storage

namespace FxClipboard
{
struct Clipboard
{
    Clipboard();
    std::vector<float> fxCopyPaste;
    std::unordered_map<std::string, std::shared_ptr<std::vector<std::uint8_t>>> user_data;
};

void copyFx(SurgeStorage *, FxStorage *from, Clipboard &cb);
bool isPasteAvailable(const Clipboard &cb);
void pasteFx(SurgeStorage *, FxStorage *onto, Clipboard &cb);

struct ChainClipboard
{
    struct SlotCopy
    {
        std::vector<float> params;
        std::unordered_map<std::string, std::shared_ptr<std::vector<std::uint8_t>>> user_data;
        std::string presetName;
    };
    SlotCopy slots[n_fx_per_chain];
    bool hasData{false};
};

void copyFxChain(SurgeStorage *, FxStorage *from[n_fx_per_chain],
                 const std::string presetNames[n_fx_per_chain], ChainClipboard &cb);
bool isChainPasteAvailable(const ChainClipboard &cb);
void pasteFxChain(SurgeStorage *, FxStorage *onto[n_fx_per_chain], ChainClipboard &cb);

} // namespace FxClipboard
} // namespace Surge
#endif // SURGE_XT_FXPRESETANDCLIPBOARDMANAGER_H
