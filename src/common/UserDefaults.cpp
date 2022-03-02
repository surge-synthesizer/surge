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

/*
** These functions and variables are not exported in the header
*/
struct UserDefaultValue
{
    typedef enum ValueType
    {
        ud_string = 1,
        ud_int = 2,
        ud_pair = 3
    } ValueType;

    std::string keystring;
    DefaultKey key;
    std::string value;
    std::pair<int, int> vpair;
    ValueType type;
};

std::map<DefaultKey, std::string> keysToStrings;
std::map<std::string, DefaultKey> stringsToKeys;

void initMaps()
{
    if (keysToStrings.empty())
    {
        // This is an odd form but it guarantees a compile error if we miss one
        for (int k = HighPrecisionReadouts; k < nKeys; ++k)
        {
            std::string r;
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
                r = "FormulaOverlayLocationTearOut";
                break;
            case ModListValueDisplay:
                r = "modListValueDisplay";
                break;
            case MenuLightness:
                r = "menuLightness";
                break;
            case nKeys:
                break;
            }
            keysToStrings[(DefaultKey)k] = r;
        }

        for (const auto &p : keysToStrings)
            stringsToKeys[p.second] = p.first;
    }
}

std::map<DefaultKey, UserDefaultValue> defaultsFileContents;
bool haveReadDefaultsFile = false;

std::string defaultsFileName(SurgeStorage *storage)
{
    std::string fn = path_to_string(storage->userDefaultFilePath / "SurgeXTUserDefaults.xml");
    return fn;
}

void readDefaultsFile(std::string fn, bool forceRead, SurgeStorage *storage)
{
    if (!haveReadDefaultsFile || forceRead)
    {
        initMaps();
        defaultsFileContents.clear();

        TiXmlDocument defaultsLoader;
        defaultsLoader.LoadFile(string_to_path(fn));
        TiXmlElement *e = TINYXML_SAFE_TO_ELEMENT(defaultsLoader.FirstChild("defaults"));
        if (e)
        {
            const char *version = e->Attribute("version");
            if (strcmp(version, "1") != 0)
            {
                std::ostringstream oss;
                oss << "This version of Surge XT reads only version 1 defaults. Your user defaults "
                       "version is "
                    << version << ". Defaults will be ignored!";
                storage->reportError(oss.str(), "File Version Error");
                return;
            }

            TiXmlElement *def = TINYXML_SAFE_TO_ELEMENT(e->FirstChild("default"));
            while (def)
            {
                UserDefaultValue v;
                int vt;
                def->Attribute("type", &vt);
                v.type = (UserDefaultValue::ValueType)vt;
                v.keystring = def->Attribute("key");

                if (v.type == UserDefaultValue::ud_pair)
                {
                    v.vpair.first = std::atoi(def->Attribute("firstvalue"));
                    v.vpair.second = std::atoi(def->Attribute("secondvalue"));
                }
                else
                {
                    v.value = def->Attribute("value");
                }

                // silently disregard default keys we don't recognize
                if (stringsToKeys.find(v.keystring) != stringsToKeys.end())
                {
                    v.key = stringsToKeys[v.keystring];
                    defaultsFileContents[v.key] = v;
                }

                def = TINYXML_SAFE_TO_ELEMENT(def->NextSibling("default"));
            }
        }
        haveReadDefaultsFile = true;
    }
}

bool streamDefaultsFile(SurgeStorage *storage);

bool storeUserDefaultValue(SurgeStorage *storage, const DefaultKey &key, const std::string &val,
                           UserDefaultValue::ValueType type)
{
    // Re-read the file in case another surge has updated it
    readDefaultsFile(defaultsFileName(storage), true, storage);

    /*
    ** Surge has a habit of creating the user directories it needs.
    ** See SurgeSytnehsizer::savePatch for instance
    ** and so we have to do the same here
    */
    fs::create_directories(storage->userDefaultFilePath);

    UserDefaultValue v;
    v.key = key;
    v.keystring = keysToStrings[key];
    v.value = val;
    v.type = type;

    defaultsFileContents[v.key] = v;
    return streamDefaultsFile(storage);
}

bool storeUserDefaultValue(SurgeStorage *storage, const DefaultKey &key,
                           const std::pair<int, int> &val, UserDefaultValue::ValueType type)
{
    if (type != UserDefaultValue::ud_pair)
    {
        storage->reportError("Software Error: Streamed pair as type non ud_pair", "UserDefaults");
        return false;
    }
    // Re-read the file in case another surge has updated it
    readDefaultsFile(defaultsFileName(storage), true, storage);

    /*
    ** Surge has a habit of creating the user directories it needs.
    ** See SurgeSytnehsizer::savePatch for instance
    ** and so we have to do the same here
    */
    fs::create_directories(storage->userDefaultFilePath);

    UserDefaultValue v;
    v.key = key;
    v.keystring = keysToStrings[key];
    v.vpair = val;
    v.type = type;

    defaultsFileContents[v.key] = v;
    return streamDefaultsFile(storage);
}

bool streamDefaultsFile(SurgeStorage *storage)
{
    /*
    ** For now, the format of our defaults file is so simple that we don't need to mess
    ** around with tinyxml to create it, just to parse it
    */
    std::ofstream dFile(string_to_path(defaultsFileName(storage)));
    if (!dFile.is_open())
    {
        std::ostringstream emsg;
        emsg << "Unable to open defaults file '" << defaultsFileName(storage) << "' for writing.";
        storage->reportError(emsg.str(), "Defaults Not Saved");
        return false;
    }

    dFile << "<?xml version = \"1.0\" encoding = \"UTF-8\" ?>\n"
          << "<!-- User Defaults for Surge XT Synthesizer -->\n"
          << "<defaults version=\"1\">" << std::endl;

    for (auto &el : defaultsFileContents)
    {
        if (el.second.type == UserDefaultValue::ud_pair)
        {
            dFile << "  <default key=\"" << keysToStrings[el.first] << "\" firstvalue=\""
                  << el.second.vpair.first << "\" secondvalue=\"" << el.second.vpair.second
                  << "\" type=\"" << (int)el.second.type << "\"/>\n";
        }
        else
        {
            dFile << "  <default key=\"" << keysToStrings[el.first] << "\" value=\""
                  << el.second.value << "\" type=\"" << (int)el.second.type << "\"/>\n";
        }
    }

    dFile << "</defaults>" << std::endl;
    dFile.close();

    return true;
}

/*
** Functions from the header
*/

std::string getUserDefaultValue(SurgeStorage *storage, const DefaultKey &key,
                                const std::string &valueIfMissing)
{
    if (storage->userPrefOverrides.find(key) != storage->userPrefOverrides.end())
    {
        return storage->userPrefOverrides[key].second;
    }

    readDefaultsFile(defaultsFileName(storage), false, storage);

    if (defaultsFileContents.find(key) != defaultsFileContents.end())
    {
        auto vStruct = defaultsFileContents[key];
        if (vStruct.type != UserDefaultValue::ud_string)
        {
            return valueIfMissing;
        }
        return vStruct.value;
    }

    return valueIfMissing;
}

int getUserDefaultValue(SurgeStorage *storage, const DefaultKey &key, int valueIfMissing)
{
    if (storage->userPrefOverrides.find(key) != storage->userPrefOverrides.end())
    {
        return storage->userPrefOverrides[key].first;
    }

    readDefaultsFile(defaultsFileName(storage), false, storage);

    if (defaultsFileContents.find(key) != defaultsFileContents.end())
    {
        auto vStruct = defaultsFileContents[key];
        if (vStruct.type != UserDefaultValue::ud_int)
        {
            return valueIfMissing;
        }
        return std::stoi(vStruct.value);
    }
    return valueIfMissing;
}

std::pair<int, int> getUserDefaultValue(SurgeStorage *storage, const DefaultKey &key,
                                        const std::pair<int, int> &valueIfMissing)
{
    readDefaultsFile(defaultsFileName(storage), false, storage);

    if (defaultsFileContents.find(key) != defaultsFileContents.end())
    {
        auto vStruct = defaultsFileContents[key];
        if (vStruct.type != UserDefaultValue::ud_pair)
        {
            return valueIfMissing;
        }
        return vStruct.vpair;
    }
    return valueIfMissing;
}

bool updateUserDefaultValue(SurgeStorage *storage, const DefaultKey &key, const std::string &value)
{
    return storeUserDefaultValue(storage, key, value, UserDefaultValue::ud_string);
}

bool updateUserDefaultValue(SurgeStorage *storage, const DefaultKey &key, const int value)
{
    std::ostringstream oss;
    oss << value;
    return storeUserDefaultValue(storage, key, oss.str(), UserDefaultValue::ud_int);
}

bool updateUserDefaultValue(SurgeStorage *storage, const DefaultKey &key,
                            const std::pair<int, int> &value)
{
    return storeUserDefaultValue(storage, key, value, UserDefaultValue::ud_pair);
}

} // namespace Storage
} // namespace Surge
