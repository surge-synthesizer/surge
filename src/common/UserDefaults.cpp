#include "UserDefaults.h"
#include "SurgeStorage.h"

#include <string>
#include <iostream>
#include <iomanip>
#include <map>
#include <sstream>
#include <fstream>

#include "filesystem/import.h"

namespace Surge
{
namespace Storage
{

// This is an odd form but it guarantees a compile error if we miss one
std::string defaultKeyToString(DefaultKey k)
{
    std::string r = "";
    switch ((DefaultKey)k)
    {
    case HighPrecisionReadouts:
        r = "highPrecisionReadouts";
        break;
    case SmoothingMode:
        r = "smoothingMode";
        break;
    case PitchSmoothingMode:
        r = "pitchSmoothingMode";
        break;
    case MiddleC:
        r = "middleC";
        break;
    case MPEPitchBendRange:
        r = "mpePitchBendRange";
        break;
    case UseCh2Ch3ToPlayScenesIndividually:
        r = "useCh2Ch3ToPlayScenesIndividually";
        break;
    case RestoreMSEGSnapFromPatch:
        r = "restoreMSEGSnapFromPatch";
        break;
    case UserDataPath:
        r = "userDataPath";
        break;
    case UseODDMTS:
        r = "useODDMTS";
        break;
    case ActivateExtraOutputs:
        // not used anymore, see GitHub issue #5657
        r = "activateExtraOutputs";
        break;
    case MonoPedalMode:
        r = "monoPedalMode";
        break;
    case ShowCursorWhileEditing:
        r = "showCursorWhileEditing";
        break;
    case TouchMouseMode:
        r = "touchMouseMode";
        break;
    case ShowGhostedLFOWaveReference:
        r = "showGhostedLFOWaveReference";
        break;
    case ShowCPUUsage:
        r = "showCPUUsage";
        break;
    case Use3DWavetableView:
        r = "use3DWavetableView";
        break;
    case DefaultSkin:
        r = "defaultSkin";
        break;
    case DefaultSkinRootType:
        r = "defaultSkinRootType";
        break;
    case DefaultZoom:
        r = "defaultZoom";
        break;
    case SliderMoveRateState:
        r = "sliderMoveRateState";
        break;
    case RememberTabPositionsPerScene:
        r = "rememberTabPositionsPerScene";
        break;
    case PatchJogWraparound:
        r = "patchJogWraparound";
        break;
    case RetainPatchSearchboxAfterLoad:
        r = "retainPatchSearchboxAfterLoad";
        break;
    case OverrideTuningOnPatchLoad:
        r = "overrideTuningOnPatchLoad";
        break;
    case OverrideMappingOnPatchLoad:
        r = "overrideMappingOnPatchLoad";
        break;
    case DefaultPatchAuthor:
        r = "defaultPatchAuthor";
        break;
    case DefaultPatchComment:
        r = "defaultPatchComment";
        break;
    case AppendOriginalPatchBy:
        r = "appendOriginalPatchBy";
        break;
    case ModWindowShowsValues:
        r = "modWindowShowsValues";
        break;
    case LayoutGridResolution:
        r = "layoutGridResolution";
        break;
    case ShowVirtualKeyboard_Plugin:
        r = "showVirtualKeyboardPlugin";
        break;
    case ShowVirtualKeyboard_Standalone:
        r = "showVirtualKeyboardStandalone";
        break;
    case InitialPatchName:
        r = "initialPatchName";
        break;
    case InitialPatchCategory:
        r = "initialPatchCategory";
        break;
    case InitialPatchCategoryType:
        r = "initialPatchCategoryType";
        break;
    case LastSCLPath:
        r = "lastSCLPath";
        break;
    case LastKBMPath:
        r = "lastKBMPath";
        break;
    case LastPatchPath:
        r = "lastPatchPath";
        break;
    case LastWavetablePath:
        r = "lastWavetablePath";
        break;
    // TODO: remove in XT2
    case TabKeyArmsModulators:
        r = "tabKeyArmsModulators";
        break;
    case UseKeyboardShortcuts_Plugin:
        r = "useKeyboardShortcutsPlugin";
        break;
    case UseKeyboardShortcuts_Standalone:
        r = "useKeyboardShortcutsStandalone";
        break;
    case PromptToActivateShortcutsOnAccKeypress:
        r = "promptToActivateShortcutsOnAccKeypress";
        break;
    case PromptToActivateCategoryAndPatchOnKeypress:
        r = "promptToActivateCategoryAndPatchOnKeypress";
        break;
    case PromptToLoadOverDirtyPatch:
        r = "promptToLoadOverDirtyPatch";
        break;
    case InfoWindowPopupOnIdle:
        r = "infoWindowPopupOnIdle";
        break;
    case TuningOverlayLocation:
        r = "tuningOverlayLocation";
        break;
    case ModlistOverlayLocation:
        r = "modlistOverlayLocation";
        break;
    case MSEGOverlayLocation:
        r = "msegOverlayLocation";
        break;
    case FormulaOverlayLocation:
        r = "formulaOverlayLocation";
        break;
    case WSAnalysisOverlayLocation:
        r = "wsAnalysisOverlayLocation";
        break;
    case FilterAnalysisOverlayLocation:
        r = "filterAnalysisOverlayLocation";
        break;
    case OscilloscopeOverlayLocation:
        r = "oscilloscopeOverlayLocation";
        break;

    case TuningOverlayLocationTearOut:
        r = "tuningOverlayLocationTearOut";
        break;
    case ModlistOverlayLocationTearOut:
        r = "modlistOverlayLocationTearOut";
        break;
    case MSEGOverlayLocationTearOut:
        r = "msegOverlayLocationTearOut";
        break;
    case FormulaOverlayLocationTearOut:
        r = "formulaOverlayLocationTearOut";
        break;
    case WSAnalysisOverlayLocationTearOut:
        r = "wsAnalysisOverlayLocationTearOut";
        break;
    case FilterAnalysisOverlayLocationTearOut:
        r = "filterAnalysisOverlayLocationTearOut";
        break;
    case OscilloscopeOverlayLocationTearOut:
        r = "oscilloscopeOverlayLocationTearOut";
        break;

    case TuningOverlaySizeTearOut:
        r = "tuningOverlaySizeTearOut";
        break;
    case ModlistOverlaySizeTearOut:
        r = "modlistOverlaySizeTearOut";
        break;
    case MSEGOverlaySizeTearOut:
        r = "msegOverlaySizeTearOut";
        break;
    case FormulaOverlaySizeTearOut:
        r = "formulaOverlaySizeTearOut";
        break;
    case WSAnalysisOverlaySizeTearOut:
        r = "wsAnalysisOverlaySizeTearOut";
        break;
    case FilterAnalysisOverlaySizeTearOut:
        r = "filterAnalysisOverlaySizeTearOut";
        break;
    case OscilloscopeOverlaySizeTearOut:
        r = "oscilloscopeOverlaySizeTearOut";
        break;

    case TuningOverlayTearOutAlwaysOnTop:
        r = "tuningOverlayTearOutAlwaysOnTop";
        break;
    case ModlistOverlayTearOutAlwaysOnTop:
        r = "modlistOverlayTearOutAlwaysOnTop";
        break;
    case MSEGOverlayTearOutAlwaysOnTop:
        r = "msegOverlayTearOutAlwaysOnTop";
        break;
    case FormulaOverlayTearOutAlwaysOnTop:
        r = "formulaOverlayTearOutAlwaysOnTop";
        break;
    case WSAnalysisOverlayTearOutAlwaysOnTop:
        r = "wsAnalysisOverlayTearOutAlwaysOnTop";
        break;
    case FilterAnalysisOverlayTearOutAlwaysOnTop:
        r = "filterAnalysisOverlayTearOutAlwaysOnTop";
        break;
    case OscilloscopeOverlayTearOutAlwaysOnTop:
        r = "oscilloscopeOverlayTearOutAlwaysOnTop";
        break;

    case ModListValueDisplay:
        r = "modListValueDisplay";
        break;
    case MenuLightness:
        r = "menuLightness";
        break;

    case UseNarratorAnnouncements:
        r = "useNarratorAnnouncements";
        break;
    case UseNarratorAnnouncementsForPatchTypeahead:
        r = "useNarratorAnnouncementsForPatchTypeahead";
        break;

    case FXUnitAssumeFixedBlock:
        r = "fxAssumeFixedBlock";
        break;
    case FXUnitDefaultZoom:
        r = "fxUnitDefaultZoom";
        break;

    case MenuAndEditKeybindingsFollowKeyboardFocus:
        r = "menuAndEditKeybindingsFollowKeyboardFocus";
        break;

    case ExpandModMenusWithSubMenus:
        r = "expandModMenusWithSubmenus";
        break;

    case nKeys:
        break;
    }
    return r;
}

/*
** Functions from the header
*/

std::string getUserDefaultValue(SurgeStorage *storage, const DefaultKey &key,
                                const std::string &valueIfMissing)
{
    return storage->userDefaultsProvider->getUserDefaultValue(key, valueIfMissing);
}

int getUserDefaultValue(SurgeStorage *storage, const DefaultKey &key, int valueIfMissing)
{
    return storage->userDefaultsProvider->getUserDefaultValue(key, valueIfMissing);
}

std::pair<int, int> getUserDefaultValue(SurgeStorage *storage, const DefaultKey &key,
                                        const std::pair<int, int> &valueIfMissing)
{
    return storage->userDefaultsProvider->getUserDefaultValue(key, valueIfMissing);
}

bool updateUserDefaultValue(SurgeStorage *storage, const DefaultKey &key, const std::string &value)
{
    return storage->userDefaultsProvider->updateUserDefaultValue(key, value);
}

bool updateUserDefaultValue(SurgeStorage *storage, const DefaultKey &key, const int value)
{
    return storage->userDefaultsProvider->updateUserDefaultValue(key, value);
}

bool updateUserDefaultValue(SurgeStorage *storage, const DefaultKey &key,
                            const std::pair<int, int> &value)
{
    return storage->userDefaultsProvider->updateUserDefaultValue(key, value);
}

} // namespace Storage
} // namespace Surge
