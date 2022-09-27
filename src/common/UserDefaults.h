#pragma once

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
    ShowVirtualKeyboard_Plugin,
    ShowVirtualKeyboard_Standalone,

    MPEPitchBendRange,
    PitchSmoothingMode,
    UseCh2Ch3ToPlayScenesIndividually,

    SmoothingMode,
    MonoPedalMode,

    // these are persistent options sprinkled outside of the menu
    UseODDMTS,
    Use3DWavetableView,
    ModListValueDisplay,

    // dialog related stuff
    LastSCLPath,
    LastKBMPath,
    LastWavetablePath,
    LastPatchPath,

    PromptToActivateShortcutsOnAccKeypress,
    PromptToActivateCategoryAndPatchOnKeypress,

    // overlay related stuff
    TuningOverlayLocation,
    ModlistOverlayLocation,
    MSEGOverlayLocation,
    FormulaOverlayLocation,
    WSAnalysisOverlayLocation,
    FilterAnalysisOverlayLocation,
    OscilloscopeOverlayLocation,

    TuningOverlayLocationTearOut,
    ModlistOverlayLocationTearOut,
    MSEGOverlayLocationTearOut,
    FormulaOverlayLocationTearOut,
    WSAnalysisOverlayLocationTearOut,
    FilterAnalysisOverlayLocationTearOut,
    OscilloscopeOverlayLocationTearOut,

    TuningOverlaySizeTearOut,
    ModlistOverlaySizeTearOut,
    MSEGOverlaySizeTearOut,
    FormulaOverlaySizeTearOut,
    WSAnalysisOverlaySizeTearOut,
    FilterAnalysisOverlaySizeTearOut,
    OscilloscopeOverlaySizeTearOut,

    TuningOverlayTearOutAlwaysOnTop,
    ModlistOverlayTearOutAlwaysOnTop,
    MSEGOverlayTearOutAlwaysOnTop,
    FormulaOverlayTearOutAlwaysOnTop,
    WSAnalysisOverlayTearOutAlwaysOnTop,
    FilterAnalysisOverlayTearOutAlwaysOnTop,
    OscilloscopeOverlayTearOutAlwaysOnTop,

    // Surge XT Effects specific defaults
    FXUnitAssumeFixedBlock,
    FXUnitDefaultZoom,

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
                                const std::string &valueIfMissing);
int getUserDefaultValue(SurgeStorage *storage, const DefaultKey &key, int valueIfMissing);
std::pair<int, int> getUserDefaultValue(SurgeStorage *storage, const DefaultKey &key,
                                        const std::pair<int, int> &valueIfMissing);
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
