#pragma once

#include <string>
#include <vector>
#include <filesystem/import.h>

/*
** Surge has a variety of settings which users can update and save across
** sessions. These two functions give a vector to a persisted key-value
** pair store which we can use in our application logic. Our reference
** to SurgeStorage allows us to find paths in which we can load or
** persist this information.
*/

class SurgeStorage;

namespace Surge
{
namespace Storage
{

enum DefaultKey // streamed as strings so feel free to change the order to whatever you want
{
    HighPrecisionReadouts,
    SmoothingMode,
    PitchSmoothingMode,
    MiddleC,
    MPEPitchBendRange,
    RestoreMSEGSnapFromPatch,
    UserDataPath,
    UseODDMTS,
    ActivateExtraOutputs,
    MonoPedalMode,
    ShowCursorWhileEditing,
    TouchMouseMode,
    ShowGhostedLFOWaveReference,
    DefaultSkin,
    DefaultSkinRootType,
    DefaultZoom,
    SliderMoveRateState,
    RememberTabPositionsPerScene,
    PatchJogWraparound,

    DefaultPatchAuthor,
    DefaultPatchComment,
    AppendOriginalPatchBy,

    ModWindowShowsValues,
    LayoutGridResolution,

    ShowVirtualKeyboard_Plugin,
    ShowVirtualKeyboard_Standalone,

    InitialPatchName,
    InitialPatchCategory,
    InitialPatchCategoryType,

    LastSCLPath,
    LastKBMPath,
    LastWavetablePath,
    LastPatchPath,

    TabKeyArmsModulators,

    InfoWindowPopupOnIdle,

    UseKeyboardShortcuts_Plugin,
    UseKeyboardShortcuts_Standalone,

    TuningOverlayLocation,
    ModlistOverlayLocation,
    MSEGOverlayLocation,
    FormulaOverlayLocation,
    WSAnalysisOverlayLocation,

    TuningOverlayLocationTearOut,
    ModlistOverlayLocationTearOut,
    MSEGOverlayLocationTearOut,
    FormulaOverlayLocationTearOut,

    ModListValueDisplay,

    nKeys
};
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
