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
#ifndef SURGE_SRC_COMMON_USERDEFAULTS_H
#define SURGE_SRC_COMMON_USERDEFAULTS_H

#include <string>
#include <vector>
#include <filesystem/import.h>
#include "sst/plugininfra/userdefaults.h"

/*
** Surge has a variety of settings which users can update and save across sessions.
** These two functions give a vector to a persisted key-value pair store which we can
** use in our application logic. Our reference to SurgeStorage allows us to find paths
** in which we can load or persist this information.
*/
class SurgeStorage;

namespace Surge
{
namespace Storage
{

// streamed as strings so feel free to change the order to whatever you want
enum DefaultKey
{
    // these are all the various menu options
    DefaultZoom,

    DefaultSkin,
    DefaultSkinRootType,
    MenuLightness,
    LayoutGridResolution,

    HighPrecisionReadouts,
    ModWindowShowsValues,
    InfoWindowPopupOnIdle,
    ShowGhostedLFOWaveReference,
    ShowCPUUsage,
    MiddleC,

    UserDataPath,

    SliderMoveRateState,
    ShowCursorWhileEditing,
    TouchMouseMode,

    DefaultPatchAuthor,
    DefaultPatchComment,
    InitialPatchName,
    InitialPatchCategory,
    InitialPatchCategoryType,
    AppendOriginalPatchBy,

    OverrideTuningOnPatchLoad,
    OverrideMappingOnPatchLoad,
    OverrideTempoOnPatchLoad,

    RememberTabPositionsPerScene,
    RestoreMSEGSnapFromPatch,
    ActivateExtraOutputs, // TODO: remove in XT2
    PatchJogWraparound,
    RetainPatchSearchboxAfterLoad,
    PromptToLoadOverDirtyPatch,
    TabKeyArmsModulators, // TODO: remove in XT2
    UseKeyboardShortcuts_Plugin,
    UseKeyboardShortcuts_Standalone,
    MenuAndEditKeybindingsFollowKeyboardFocus,
    UseNarratorAnnouncements,
    UseNarratorAnnouncementsForPatchTypeahead,
    ExpandModMenusWithSubMenus,
    FocusModEditorAfterAddModulationFrom,
    ShowVirtualKeyboard_Plugin,
    ShowVirtualKeyboard_Standalone,
    VirtualKeyboardLayout,

    MPEPitchBendRange,
    PitchSmoothingMode,
    UseCh2Ch3ToPlayScenesIndividually,
    MenuBasedMIDILearnChannel,
    MIDISoftTakeover,

    SmoothingMode,
    MonoPedalMode,

    // these are persistent options sprinkled outside of the menu
    UseODDMTS_Deprecated,
    Use3DWavetableView,
    ModListValueDisplay,

    // dialog related stuff
    LastSCLPath,
    LastKBMPath,
    LastWavetablePath,
    LastPatchPath,

    PromptToActivateShortcutsOnAccKeypress,
    PromptToActivateCategoryAndPatchOnKeypress,

    TuningPolarGraphMode,

    // overlay related stuff
    TuningOverlayLocation,
    ModlistOverlayLocation,
    MSEGOverlayLocation,
    FormulaOverlayLocation,
    WTScriptOverlayLocation,
    WSAnalysisOverlayLocation,
    FilterAnalysisOverlayLocation,
    OscilloscopeOverlayLocation,

    TuningOverlayLocationTearOut,
    ModlistOverlayLocationTearOut,
    MSEGOverlayLocationTearOut,
    FormulaOverlayLocationTearOut,
    WTScriptOverlayLocationTearOut,
    WSAnalysisOverlayLocationTearOut,
    FilterAnalysisOverlayLocationTearOut,
    OscilloscopeOverlayLocationTearOut,

    TuningOverlaySizeTearOut,
    ModlistOverlaySizeTearOut,
    MSEGOverlaySizeTearOut,
    FormulaOverlaySizeTearOut,
    WTScriptOverlaySizeTearOut,
    WSAnalysisOverlaySizeTearOut,
    FilterAnalysisOverlaySizeTearOut,
    OscilloscopeOverlaySizeTearOut,

    TuningOverlayTearOutAlwaysOnTop,
    ModlistOverlayTearOutAlwaysOnTop,
    MSEGOverlayTearOutAlwaysOnTop,
    FormulaOverlayTearOutAlwaysOnTop,
    WTScriptOverlayTearOutAlwaysOnTop,
    WSAnalysisOverlayTearOutAlwaysOnTop,
    FilterAnalysisOverlayTearOutAlwaysOnTop,
    OscilloscopeOverlayTearOutAlwaysOnTop,

    TuningOverlayTearOutAlwaysOnTop_Plugin,
    ModlistOverlayTearOutAlwaysOnTop_Plugin,
    MSEGOverlayTearOutAlwaysOnTop_Plugin,
    FormulaOverlayTearOutAlwaysOnTop_Plugin,
    WTScriptOverlayTearOutAlwaysOnTop_Plugin,
    WSAnalysisOverlayTearOutAlwaysOnTop_Plugin,
    FilterAnalysisOverlayTearOutAlwaysOnTop_Plugin,
    OscilloscopeOverlayTearOutAlwaysOnTop_Plugin,

    // Surge XT Effects specific defaults
    FXUnitAssumeFixedBlock,
    FXUnitDefaultZoom,

    IgnoreMIDIProgramChange_Deprecated, // better PC support means skip this

    DontShowAudioErrorsAgain,

    // OSC (Open Sound Control)
    StartOSCIn,
    StartOSCOut,
    OSCPortIn,
    OSCPortOut,
    OSCIPOut,

    nKeys
};

std::string defaultKeyToString(DefaultKey k);
typedef sst::plugininfra::defaults::Provider<DefaultKey, DefaultKey::nKeys> UserDefaultsProvider;

/**
 * getUserDefaultValue
 *
 * Given a key, return the std::string which is the persisted user default value.
 * If no such key is persisted, return the value "valueIfMissing". There is a variation
 * on this for both std::string and int stored values.
 */
std::string getUserDefaultValue(SurgeStorage *storage, const DefaultKey &key,
                                const std::string &valueIfMissing, bool potentiallyRead = true);
int getUserDefaultValue(SurgeStorage *storage, const DefaultKey &key, int valueIfMissing,
                        bool potentiallyRead = true);
std::pair<int, int> getUserDefaultValue(SurgeStorage *storage, const DefaultKey &key,
                                        const std::pair<int, int> &valueIfMissing,
                                        bool potentiallyRead = true);
inline fs::path getUserDefaultPath(SurgeStorage *storage, const DefaultKey &key,
                                   const fs::path &valueIfMissing)
{
    return string_to_path(getUserDefaultValue(storage, key, path_to_string(valueIfMissing)));
}

/**
 * updateUserDefaultValue
 *
 * Given a key and a value, update the user default file
 */
bool updateUserDefaultValue(SurgeStorage *storage, const DefaultKey &key, const std::string &value);
bool updateUserDefaultValue(SurgeStorage *storage, const DefaultKey &key, const int value);
inline bool updateUserDefaultPath(SurgeStorage *storage, const DefaultKey &key,
                                  const fs::path &path)
{
    return updateUserDefaultValue(storage, key, path_to_string(path));
}
bool updateUserDefaultValue(SurgeStorage *stoarge, const DefaultKey &key,
                            const std::pair<int, int> &value);

} // namespace Storage
} // namespace Surge

#endif // SURGE_SRC_COMMON_USERDEFAULTS_H
