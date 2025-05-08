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

#include "DSPUtils.h"
#include "SurgeStorage.h"
#include <set>
#include <numeric>
#include <cctype>
#include <map>
#include <queue>
#include "UserDefaults.h"
#if HAS_JUCE
#include "SurgeSharedBinary.h"
#endif
#include "DebugHelpers.h"

#include "sst/plugininfra/paths.h"

#include <iostream>
#include <iomanip>
#include <sstream>

#include "UserDefaults.h"
#include "version.h"

#include "sst/plugininfra/strnatcmp.h"
#ifndef SURGE_SKIP_ODDSOUND_MTS
#include "libMTSClient.h"
#include "libMTSMaster.h"
#endif
#include "FxPresetAndClipboardManager.h"
#include "ModulatorPresetManager.h"
#include "SurgeMemoryPools.h"
#include "sst/basic-blocks/tables/SincTableProvider.h"

// FIXME probably remove this when we remove the hardcoded hack below
#include "MSEGModulationHelper.h"
// FIXME
#include "FormulaModulationHelper.h"

#include "sst/basic-blocks/mechanics/endian-ops.h"
namespace mech = sst::basic_blocks::mechanics;

using namespace std;

std::string SurgeStorage::skipPatchLoadDataPathSentinel = "<SKIP-PATCH-SENTINEL>";

SurgeStorage::SurgeStorage(const SurgeStorage::SurgeStorageConfig &config) : otherscene_clients(0)
{
    auto suppliedDataPath = config.suppliedDataPath;
    bool loadWtAndPatch = true;
    loadWtAndPatch = !skipLoadWtAndPatch && suppliedDataPath != skipPatchLoadDataPathSentinel &&
                     config.scanWavetableAndPatches;

    if (suppliedDataPath == skipPatchLoadDataPathSentinel)
        suppliedDataPath = "";

    if (samplerate == 0)
    {
        setSamplerate(48000);
    }
    else if (samplerate < 12000 || samplerate > 48000 * 32)
    {
        std::ostringstream oss;
        oss << "Warning: SurgeStorage constructed with invalid samplerate: " << samplerate
            << " - resetting to 48000 until audio system tells us otherwise" << std::endl;
        reportError(oss.str(), "Sample Rate Corrupted on Startup");
        setSamplerate(48000);
    }
    else
    {
        // Just in case, make sure we are completely consistent in all tables etc
        setSamplerate(samplerate);
    }

    _patch.reset(new SurgePatch(this));

    namespace tabl = sst::basic_blocks::tables;
    sincTableProvider = std::make_unique<tabl::SurgeSincTableProvider>();
    static_assert(tabl::SurgeSincTableProvider::FIRipol_M == FIRipol_M);
    static_assert(tabl::SurgeSincTableProvider::FIRipol_N == FIRipol_N);
    static_assert(tabl::SurgeSincTableProvider::FIRipolI16_N == FIRipolI16_N);
    sinctable = sincTableProvider->sinctable;
    sinctable1X = sincTableProvider->sinctable1X;
    sinctableI16 = sincTableProvider->sinctableI16;

    for (int s = 0; s < n_scenes; s++)
        for (int m = 0; m < n_modsources; ++m)
            getPatch().scene[s].modsource_doprocess[m] = false;

    for (int s = 0; s < n_scenes; s++)
        for (int o = 0; o < n_oscs; o++)
        {
            for (int i = 0; i < max_mipmap_levels; i++)
                for (int j = 0; j < max_subtables; j++)
                {
                    getPatch().scene[s].osc[o].wt.TableF32WeakPointers[i][j] = 0;
                    getPatch().scene[s].osc[o].wt.TableI16WeakPointers[i][j] = 0;
                }
            getPatch().scene[s].osc[o].extraConfig.nData = 0;
            memset(getPatch().scene[s].osc[0].extraConfig.data, 0,
                   sizeof(float) * OscillatorStorage::ExtraConfigurationData::max_config);
        }

    for (int s = 0; s < n_scenes; ++s)
    {
        for (int fu = 0; fu < n_filterunits_per_scene; ++fu)
        {
            for (int t = 0; t < sst::filters::num_filter_types; ++t)
            {
                switch (t)
                {
                case sst::filters::fut_lpmoog:
                case sst::filters::fut_obxd_4pole:
                case sst::filters::fut_diode:
                    subtypeMemory[s][fu][t] = 3;
                    break;
                default:
                    subtypeMemory[s][fu][t] = 0;
                    break;
                }
            }
        }
    }

    init_tables();

    pitch_bend = 0;
    last_key[0] = 60;
    last_key[1] = 60;
    temposyncratio = 1.f;

    // Use this as a sentinel, since it was not initialized prior to 1.6.5
    // this was the value at least Windows and Mac had
    // https://github.com/surge-synthesizer/surge/issues/1444
    temposyncratio_inv = 0.0f;

    songpos = 0;

    for (int i = 0; i < n_customcontrollers; i++)
    {
        controllers[i] = 41 + i;
        controllers_chan[i] = -1;
    }

    for (int i = 0; i < n_modsources; i++)
        modsource_vu[i] = 0.f; // remove?

    for (int s = 0; s < n_scenes; s++)
        for (int c = 0; c < 16; c++)
            for (int cc = 0; cc < 128; cc++)
                poly_aftertouch[s][c][cc] = 0.f;

    memset(&audio_in[0][0], 0, 2 * BLOCK_SIZE_OS * sizeof(float));

    bool hasSuppliedDataPath = !suppliedDataPath.empty();
    std::string buildOverrideDataPath;
    if (getOverrideDataHome(buildOverrideDataPath))
    {
        datapathOverriden = true;
        hasSuppliedDataPath = true;
        suppliedDataPath = buildOverrideDataPath;
    }

    std::string sxt = "Surge XT";
    std::string sxtlower = "surge-xt";

#if MAC
    if (!hasSuppliedDataPath)
    {
        auto shareddp = sst::plugininfra::paths::bestLibrarySharedFolderPathFor(sxt);
        auto userdp = sst::plugininfra::paths::bestLibrarySharedFolderPathFor(sxt, true);

        if (fs::is_directory(userdp))
            datapath = userdp;
        else
            datapath = shareddp;
    }
    else
    {
        datapath = fs::path{suppliedDataPath};
    }

    userDataPath = sst::plugininfra::paths::bestDocumentsFolderPathFor("Surge XT");

    // These are how I test a broken windows install for documents
    // userDataPath = fs::path{"/good/luck/bozo"};
    // userDataPath = fs::path{"/usr/sbin"};
#elif LINUX
    const auto installPath = sst::plugininfra::paths::sharedLibraryBinaryPath().parent_path();

    if (!hasSuppliedDataPath)
    {
        bool founddir{false};

        // First check the portable mode sitting beside me
        auto cp{installPath};

        while (datapath.empty() && cp.has_parent_path() && cp != cp.parent_path())
        {
            auto portable = cp / L"SurgeXTData";

            if (fs::is_directory(portable))
            {
                datapath = std::move(portable);
                founddir = true;
            }

            cp = cp.parent_path();
        }

        if (!founddir)
        {
            auto userlower =
                sst::plugininfra::paths::bestLibrarySharedFolderPathFor(sxtlower, true);
            auto userreg = sst::plugininfra::paths::bestLibrarySharedFolderPathFor(sxt, true);
            auto globallower =
                sst::plugininfra::paths::bestLibrarySharedFolderPathFor(sxtlower, false);
            auto globalreg = sst::plugininfra::paths::bestLibrarySharedFolderPathFor(sxt, true);

            for (const auto &p : {userreg, userlower, globalreg, globallower})
            {
                if (fs::is_directory(p) && !founddir)
                {
                    founddir = true;
                    datapath = p;
                }
            }
            if (!founddir)
                datapath = globallower;
        }
    }
    else
    {
        datapath = suppliedDataPath;
    }

    // First check the portable mode sitting beside me
    auto up{installPath};

    while (userDataPath.empty() && up.has_parent_path() && up != up.parent_path())
    {
        auto portable = up / L"SurgeXTUserData";

        if (fs::is_directory(portable))
        {
            userDataPath = std::move(portable);
        }

        up = up.parent_path();
    }
    userDataPath = sst::plugininfra::paths::bestDocumentsFolderPathFor(sxt);

#elif WINDOWS
    const auto installPath = sst::plugininfra::paths::sharedLibraryBinaryPath().parent_path();

    if (!hasSuppliedDataPath)
    {
        // First check the portable mode sitting beside me
        auto cp{installPath};

        while (datapath.empty() && cp.has_parent_path() && cp != cp.parent_path())
        {
            auto portable = cp / L"SurgeXTData";

            if (fs::is_directory(portable))
            {
                datapath = std::move(portable);
            }

            cp = cp.parent_path();
        }

        if (datapath.empty())
        {
            datapath = sst::plugininfra::paths::bestLibrarySharedFolderPathFor(sxt);
        }

        if (datapath.empty() || !fs::is_directory(datapath))
        {
            datapath = sst::plugininfra::paths::bestLibrarySharedFolderPathFor(sxt, true);
        }
    }
    else
    {
        datapath = suppliedDataPath;
    }

    // Portable - first check for installPath\\SurgeXTUserData
    auto cp{installPath};

    while (userDataPath.empty() && cp.has_parent_path() && cp != cp.parent_path())
    {
        auto portable = cp / L"SurgeXTUserData";

        if (fs::is_directory(portable))
        {
            userDataPath = std::move(portable);
        }

        cp = cp.parent_path();
    }

    if (userDataPath.empty())
    {
        try
        {
            userDataPath = sst::plugininfra::paths::bestDocumentsFolderPathFor(sxt);
        }
        catch (const std::runtime_error &e)
        {
            userDataPath = fs::path{"/documents/not/available"};
            reportError(
                std::string() + "Surge is unable to find the %DOCUMENTS% directory. " +
                    "Your system is misconfigured and several features including saving patches, " +
                    "the search database and preferences will not work. " + e.what(),
                "Unable to determine %DOCUMENTS%");
        }
    }
#endif

    try
    {
        userDataPathValid = false;
        if (fs::is_directory(userDataPath))
        {
            userDataPathValid = true;
            /*
             * This code doesn't work because fs:: doesn't have a writable check and I don't
             * want to create a file just to see in every startup path. We find out later
             * anyway when we set up the directories.
             */
        }
    }
    catch (const fs::filesystem_error &e)
    {
        userDataPathValid = false;
    }

    userDefaultFilePath = userDataPath;

    userDefaultsProvider = std::make_unique<Surge::Storage::UserDefaultsProvider>(
        userDataPath, "SurgeXT", Surge::Storage::defaultKeyToString,
        [this](auto &a, auto &b) { reportError(a, b); });

    std::string userSpecifiedDataPath =
        Surge::Storage::getUserDefaultValue(this, Surge::Storage::UserDataPath, "UNSPEC");
    if (userSpecifiedDataPath != "UNSPEC")
    {
        userDataPath = fs::path{userSpecifiedDataPath};
    }

    // append separator if not present
    userPatchesPath = userDataPath / "Patches";
    userPatchesMidiProgramChangePath = userPatchesPath / midiProgramChangePatchesSubdir;
    userWavetablesPath = userDataPath / "Wavetables";
    userWavetablesExportPath = userWavetablesPath / "Exported";
    userFXPath = userDataPath / "FX Presets";
    userMidiMappingsPath = userDataPath / "MIDI Mappings";
    userModulatorSettingsPath = userDataPath / "Modulator Presets";
    userSkinsPath = userDataPath / "Skins";
    extraThirdPartyWavetablesPath = config.extraThirdPartyWavetablesPath;
    extraUserWavetablesPath = config.extraUsersWavetablesPath;

    if (config.createUserDirectory)
    {
        createUserDirectory();
    }

    // TIXML requires a newline at end.
#if HAS_JUCE
    auto cxmlData = std::string(SurgeSharedBinary::configuration_xml,
                                SurgeSharedBinary::configuration_xmlSize) +
                    "\n";
#else
    std::string cxmlData;

    if (fs::exists(datapath / "configuration.xml"))
    {
        std::ifstream ifs(datapath / "configuration.xml");
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        cxmlData = buffer.str();
    }
    else
    {
        cxmlData = std::string(
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\" ?><midictrl/>");
    }
#endif
    if (!snapshotloader.Parse(cxmlData.c_str()))
    {
#if HAS_JUCE
        std::cout << snapshotloader.ErrorDesc() << std::endl;
        reportError("Cannot parse 'configuration.xml' from memory. Internal Software Error.",
                    "Surge Incorrectly Built");
#endif
    }

    load_midi_controllers();

    patchDB = std::make_unique<Surge::PatchStorage::PatchDB>(this);
    if (loadWtAndPatch)
    {
        refresh_wtlist();
        refresh_patchlist();
    }

#if HAS_JUCE
    if (!load_wt_wt_mem(SurgeSharedBinary::windows_wt, SurgeSharedBinary::windows_wtSize,
                        &WindowWT))
    {
        WindowWT.size = 0;
        std::ostringstream oss;
        oss << "Unable to load 'windows.wt' from memory. "
            << "This is a fatal internal software error which should never occur!";
        reportError(oss.str(), "Resource Loading Error");
    }
#else
    if (fs::exists(datapath / "windows.wt"))
    {
        std::string metadata;
        if (!load_wt_wt(path_to_string(datapath / "windows.wt"), &WindowWT, metadata))
        {
            WindowWT.size = 0;
            std::ostringstream oss;
            oss << "Unable to load 'windows.wt' from file. "
                << "This is a fatal internal software error which should never occur!";
            reportError(oss.str(), "Resource Loading Error");
            _DBGCOUT << oss.str() << std::endl;
        }
    }
#endif

    // Tuning library support
    currentScale = Tunings::evenTemperament12NoteScale();
    currentMapping = Tunings::KeyboardMapping();
    cachedToggleOffScale = currentScale;
    cachedToggleOffMapping = currentMapping;
    twelveToneStandardMapping =
        Tunings::Tuning(Tunings::evenTemperament12NoteScale(), Tunings::KeyboardMapping());
    isStandardTuning = true;
    mapChannelToOctave = false;
    isToggledToCache = false;
    for (int q = 0; q < 3; ++q)
        togglePriorState[q] = false;

    Surge::Formula::setupStorage(this);

    // Load the XML DocStrings if we are loading startup data
    if (loadWtAndPatch)
    {
#if HAS_JUCE
        auto pdData = std::string(SurgeSharedBinary::paramdocumentation_xml,
                                  SurgeSharedBinary::paramdocumentation_xmlSize) +
                      "\n";

        TiXmlDocument doc;
        if (!doc.Parse(pdData.c_str()) || doc.Error())
        {
            std::cout << "Unable to load  'paramdocumentation'!" << std::endl;
            std::cout << "Unable to parse!\nError is:\n"
                      << doc.ErrorDesc() << " at row " << doc.ErrorRow() << ", column "
                      << doc.ErrorCol() << std::endl;
        }
        else
        {
            TiXmlElement *pdoc = TINYXML_SAFE_TO_ELEMENT(doc.FirstChild("param-doc"));
            if (!pdoc)
            {
                reportError(
                    "Unknown top element in paramdocumentation.xml - not a parameter documentation "
                    "XML file!",
                    "Error");
            }
            else
            {
                for (auto pchild = pdoc->FirstChildElement(); pchild;
                     pchild = pchild->NextSiblingElement())
                {
                    if (strcmp(pchild->Value(), "ctrl_group") == 0)
                    {
                        int g = 0;
                        if (pchild->QueryIntAttribute("group", &g) == TIXML_SUCCESS)
                        {
                            std::string help_url = pchild->Attribute("help_url");
                            if (help_url.size() > 0)
                                helpURL_controlgroup[g] = help_url;
                        }
                    }
                    else if (strcmp(pchild->Value(), "param") == 0)
                    {
                        std::string id = pchild->Attribute("id");
                        std::string help_url = pchild->Attribute("help_url");
                        int t = 0;
                        if (help_url.size() > 0)
                        {
                            if (pchild->QueryIntAttribute("type", &t) == TIXML_SUCCESS)
                            {
                                helpURL_paramidentifier_typespecialized[std::make_pair(id, t)] =
                                    help_url;
                            }
                            else
                            {
                                helpURL_paramidentifier[id] = help_url;
                            }
                        }
                    }
                    else if (strcmp(pchild->Value(), "special") == 0)
                    {
                        std::string id = pchild->Attribute("id");
                        std::string help_url = pchild->Attribute("help_url");
                        if (help_url.size() > 0)
                        {
                            helpURL_specials[id] = help_url;
                        }
                    }
                    else
                    {
                        std::cout << "UNKNOWN " << pchild->Value() << std::endl;
                    }
                }
            }
        }
#endif
    }

    for (int s = 0; s < n_scenes; ++s)
    {
        for (int i = 0; i < n_lfos; ++i)
        {
            auto ms = &(_patch->msegs[s][i]);
            if (ms_lfo1 + i >= ms_slfo1 && ms_lfo1 + i <= ms_slfo6)
            {
                Surge::MSEG::createInitSceneMSEG(ms);
            }
            else
            {
                Surge::MSEG::createInitVoiceMSEG(ms);
            }
            Surge::MSEG::rebuildCache(ms);
        }
    }

    monoPedalMode = (MonoPedalMode)Surge::Storage::getUserDefaultValue(
        this, Surge::Storage::MonoPedalMode, MonoPedalMode::HOLD_ALL_NOTES);

    for (int s = 0; s < n_scenes; ++s)
    {
        getPatch().scene[s].drift.set_extend_range(true);
    }

#ifndef SURGE_SKIP_ODDSOUND_MTS
    bool mtsMode =
        true; // Surge::Storage::getUserDefaultValue(this, Surge::Storage::UseODDMTS, false);
    if (mtsMode)
    {
        initialize_oddsound();
    }
    else
    {
        oddsound_mts_client = nullptr;
        oddsound_mts_active_as_client = false;
    }
#endif

    oscPortIn =
        Surge::Storage::getUserDefaultValue(this, Surge::Storage::OSCPortIn, DEFAULT_OSC_PORT_IN);
    oscPortOut =
        Surge::Storage::getUserDefaultValue(this, Surge::Storage::OSCPortOut, DEFAULT_OSC_PORT_OUT);
    oscOutIP =
        Surge::Storage::getUserDefaultValue(this, Surge::Storage::OSCIPOut, DEFAULT_OSC_IPADDR_OUT);

    initPatchName =
        Surge::Storage::getUserDefaultValue(this, Surge::Storage::InitialPatchName, "Init Saw");
    initPatchCategory = Surge::Storage::getUserDefaultValue(
        this, Surge::Storage::InitialPatchCategory, "Templates");
    initPatchCategoryType = Surge::Storage::getUserDefaultValue(
        this, Surge::Storage::InitialPatchCategoryType, "Factory");

    try
    {
        fxUserPreset = std::make_unique<Surge::Storage::FxUserPreset>();
        fxUserPreset->doPresetRescan(this);
    }
    catch (fs::filesystem_error &e)
    {
        reportError(e.what(), "Error Scanning FX Presets");
    }

    try
    {
        modulatorPreset = std::make_unique<Surge::Storage::ModulatorPreset>();
        modulatorPreset->forcePresetRescan();
    }
    catch (fs::filesystem_error &e)
    {
        reportError(e.what(), "Error Scanning Modulator Presets");
    }
    memoryPools = std::make_unique<Surge::Memory::SurgeMemoryPools>(this);
}

void SurgeStorage::createUserDirectory()
{
    auto p = userDataPath;
    auto needToBuild = false;
    if (!fs::is_directory(p) || !fs::is_directory(userPatchesPath))
    {
        needToBuild = true;
    }

    if (needToBuild)
    {
        try
        {
            for (auto &s : {userDataPath, userDefaultFilePath, userPatchesPath, userWavetablesPath,
                            userModulatorSettingsPath, userFXPath, userWavetablesExportPath,
                            userSkinsPath, userMidiMappingsPath})
                fs::create_directories(s);

            userDataPathValid = true;
#if HAS_JUCE
            auto rd = std::string(SurgeSharedBinary::README_UserArea_txt,
                                  SurgeSharedBinary::README_UserArea_txtSize) +
                      "\n";
            auto of = std::ofstream(userDataPath / "README.txt", std::ofstream::out);
            if (of.is_open())
                of << rd << std::endl;
            of.close();
#endif
        }
        catch (const fs::filesystem_error &e)
        {
            reportError(std::string() + "User directory is non-writable. " + e.what(),
                        "Unable to set up User Directory.");
            userDataPathValid = false;
        }
    }

    if (userDataPathValid)
    {
        // Special case - MIDI Program Changes came later
        if (!fs::exists(userPatchesMidiProgramChangePath))
        {
            try
            {
                fs::create_directories(userPatchesMidiProgramChangePath);
            }
            catch (const fs::filesystem_error &e)
            {
                reportError(e.what(), "Unable to set up User Directory");
                userDataPathValid = false;
            }
        }
    }
}

void SurgeStorage::initializePatchDb(bool force)
{
    if (patchDBInitialized && !force)
        return;

    if (!userDataPathValid)
        return;

    patchDBInitialized = true;

    // We do this here, because if there is a schema upgrade we need to do it before we do a patch
    // read, even though our next activity is a read
    patchDB->prepareForWrites();

    auto awid = patchDB->readAllPatchPathsWithIdAndModTime();
    std::vector<Patch> addThese;
    for (const auto p : patch_list)
    {
        if (awid.find(p.path.u8string()) == awid.end())
        {
            addThese.push_back(p);
        }
        else
        {
            if (awid[p.path.u8string()].second < p.lastModTime)
            {
                addThese.push_back(p);
            }
            awid.erase(p.path.u8string());
        }
    }

    auto catToType = [this](int q) {
        auto t = Surge::PatchStorage::PatchDB::CatType::FACTORY;
        if (q >= firstThirdPartyCategory)
            t = Surge::PatchStorage::PatchDB::CatType::THIRD_PARTY;
        if (q >= firstUserCategory)
            t = Surge::PatchStorage::PatchDB::CatType::USER;
        return t;
    };

    std::function<void(const PatchCategory &me, const PatchCategory &parent)> recAdd;
    recAdd = [&recAdd, &catToType, this](const PatchCategory &me, const PatchCategory &parent) {
        if (me.numberOfPatchesInCategoryAndChildren <= 0)
            return;

        auto t = catToType(me.internalid);
        patchDB->addSubCategory(me.name, parent.name, t);
        for (auto k : me.children)
        {
            recAdd(k, me);
        }
    };
    for (auto c : patch_category)
    {
        if (c.isRoot && c.numberOfPatchesInCategoryAndChildren > 0)
        {
            auto t = catToType(c.internalid);
            patchDB->addRootCategory(c.name, t);

            for (auto k : c.children)
            {
                recAdd(k, c);
            }
        }
    }

    for (auto p : addThese)
    {
        auto t = catToType(p.category);
        patchDB->considerFXPForLoad(p.path, p.name, patch_category[p.category].name, t);
    }

    for (auto q : awid)
    {
        patchDB->erasePatchByID(q.second.first);
    }
}

SurgePatch &SurgeStorage::getPatch() const { return *_patch.get(); }

struct PEComparer
{
    bool operator()(const Patch &a, const Patch &b) { return a.name.compare(b.name) < 0; }
};

void SurgeStorage::refresh_patchlist()
{
    patch_category.clear();
    patch_list.clear();

    refreshPatchlistAddDir(false, "patches_factory");
    firstThirdPartyCategory = patch_category.size();

    refreshPatchlistAddDir(false, "patches_3rdparty");
    firstUserCategory = patch_category.size();
    refreshPatchlistAddDir(true, "Patches");

    patchOrdering = std::vector<int>(patch_list.size());
    std::iota(patchOrdering.begin(), patchOrdering.end(), 0);

    auto patchCompare = [this](const int &i1, const int &i2) -> bool {
        return strnatcasecmp(patch_list[i1].name.c_str(), patch_list[i2].name.c_str()) < 0;
    };

    std::sort(patchOrdering.begin(), patchOrdering.end(), patchCompare);

    patchCategoryOrdering = std::vector<int>(patch_category.size());
    std::iota(patchCategoryOrdering.begin(), patchCategoryOrdering.end(), 0);

    for (int i = 0; i < patch_list.size(); i++)
    {
        patch_list[patchOrdering[i]].order = i;
    }

    auto categoryCompare = [this](const int &i1, const int &i2) -> bool {
        return strnatcasecmp(patch_category[i1].name.c_str(), patch_category[i2].name.c_str()) < 0;
    };

    int groups[4] = {0, firstThirdPartyCategory, firstUserCategory, (int)patch_category.size()};

    for (int i = 0; i < 3; i++)
    {
        std::sort(std::next(patchCategoryOrdering.begin(), groups[i]),
                  std::next(patchCategoryOrdering.begin(), groups[i + 1]), categoryCompare);
    }

    for (int i = 0; i < patch_category.size(); i++)
    {
        patch_category[patchCategoryOrdering[i]].order = i;
    }

    auto favorites = patchDB->readUserFavorites();
    auto pathToTrunc = [](const std::string &s) -> std::string {
        auto pf = s.find("patches_factory");
        auto p3 = s.find("patches_3rdparty");

        if (pf != std::string::npos)
        {
            return s.substr(pf);
        }
        if (p3 != std::string::npos)
        {
            return s.substr(p3);
        }
        return "";
    };
    std::unordered_set<std::string> favSet, favTruncSet;
    for (auto f : favorites)
    {
        favSet.insert(f);
        auto pf = pathToTrunc(f);
        if (!pf.empty())
        {
            favTruncSet.insert(pf);
        }
    }
    for (auto &p : patch_list)
    {
        try
        {
            auto qtime = fs::last_write_time(p.path);
            p.lastModTime =
                std::chrono::duration_cast<std::chrono::seconds>(qtime.time_since_epoch()).count();
        }
        catch (const fs::filesystem_error &e)
        {
            std::ostringstream erross;
            erross << "Unable to determine the modification time of '" << p.path.u8string() << ". "
                   << "This usually means the file can't be opened, or is a broken symlink, or "
                      "some such. Underlying error: "
                   << e.what();
            reportError(erross.str(), "Unable to Read File Time");
            p.lastModTime = 0;
        }
        auto ps = p.path.u8string();
        auto pf = pathToTrunc(ps);

        if (favSet.find(ps) != favSet.end())
            p.isFavorite = true;
        else if (!pf.empty() && (favTruncSet.find(pf) != favTruncSet.end()))
            p.isFavorite = true;
        else
            p.isFavorite = false;
    }

    /*
     * Update midi program change here
     */
    auto loadCategoryIntoBank = [this](int catid, int bk) {
        int currProg = 0;

        for (const auto &pd : patchOrdering)
        {
            auto &p = patch_list[pd];

            if (p.category == catid)
            {
                patchIdToMidiBankAndProgram[bk][currProg] = pd;
                currProg++;

                if (currProg >= 128)
                {
                    break;
                }
            }
        }
    };

    // TODO: Initialize the data structure with init patch everywhere
    for (auto &a : patchIdToMidiBankAndProgram)
    {
        for (auto &p : a)
        {
            p = -1;
        }
    }

    int currBank = 0;

    for (const auto &c : patch_category)
    {
        if (c.name == midiProgramChangePatchesSubdir)
        {
            if (c.numberOfPatchesInCategory > 0)
            {
                loadCategoryIntoBank(c.internalid, currBank);
            }

            // makes sure subfolders inside MIDI Programs folder always start from Bank Select MSB 1
            currBank++;

            // luckily, children are sorted
            for (const auto &k : c.children)
            {
                loadCategoryIntoBank(k.internalid, currBank);
                currBank++;

                if (currBank >= 128)
                {
                    break;
                }
            }
        }
    }

    /*
     * Our initial implementation loaded factory banks
     * in order into unused upper banks. We decided we don't want
     * this but if you do, you do something like this:
     * if (currBank < 128)
     * {
     *    for (auto c : patchCategoryOrdering)
     *   {
     *       loadCategoryIntoBank(c, currBank);
     *       currBank++;
     *       if (currBank >= 128)
     *           break;
     *   }
     * }
     */
}

void SurgeStorage::refreshPatchlistAddDir(bool userDir, string subdir)
{
    refreshPatchOrWTListAddDir(
        userDir, userDir ? userDataPath : datapath, subdir,
        [](std::string s) -> bool { return _stricmp(s.c_str(), ".fxp") == 0; }, patch_list,
        patch_category);
}

void SurgeStorage::refreshPatchOrWTListAddDir(bool userDir, const fs::path &initialPatchPath,
                                              string subdir,
                                              std::function<bool(std::string)> filterOp,
                                              std::vector<Patch> &items,
                                              std::vector<PatchCategory> &categories)
{
    int category = categories.size();

    // See issue 4200. In some cases this can throw a filesystem exception so:
    std::vector<PatchCategory> local_categories;

    try
    {
        fs::path patchpath = initialPatchPath;
        if (!subdir.empty())
            patchpath /= subdir;

        if (!fs::is_directory(patchpath))
        {
            return;
        }

        /*
        ** std::filesystem has a recursive_directory_iterator, but between the
        ** hand rolled ipmmlementation on mac, experimental on windows, and
        ** ostensibly standard on linux it isn't consistent enough to warrant
        ** using yet, so build my own recursive directory traversal with a simple
        ** stack
        */
        std::vector<fs::path> alldirs;
        if (userDir)
            alldirs.push_back(patchpath);
        std::deque<fs::path> workStack;
        workStack.push_back(patchpath);
        while (!workStack.empty())
        {
            auto top = workStack.front();
            workStack.pop_front();
            for (auto &d : fs::directory_iterator(top))
            {
                if (fs::is_directory(d))
                {
                    alldirs.push_back(d);
                    workStack.push_back(d);
                }
            }
        }

        /*
        ** We want to remove parent directory /user/foo or c:\\users\\bar\\
        ** with a substr in the main loop, so get the length once
        */
        auto patchpathStr(path_to_string(patchpath));
        auto patchpathSubstrLength = patchpathStr.size() + 1;
        if (patchpathStr.back() == '/' || patchpathStr.back() == '\\')
            patchpathSubstrLength--;

        for (auto &p : alldirs)
        {
            PatchCategory c;
            auto name = std::string("_Unsorted");
            auto pn = path_to_string(p);
            if (pn.size() > patchpathSubstrLength)
                name = pn.substr(patchpathSubstrLength);

            c.name = name;
            c.internalid = category;
            c.isFactory = !userDir;

            c.numberOfPatchesInCategory = 0;
            for (auto &f : fs::directory_iterator(p))
            {
                std::string xtn = path_to_string(f.path().extension());
                if (filterOp(xtn))
                {
                    Patch e;
                    e.category = category;
                    e.path = f.path();
                    e.name = path_to_string(f.path().filename());
                    e.name = e.name.substr(0, e.name.size() - xtn.length());
                    items.push_back(e);

                    c.numberOfPatchesInCategory++;
                }
            }

            c.numberOfPatchesInCategoryAndChildren = c.numberOfPatchesInCategory;
            local_categories.push_back(c);
            category++;
        }
    }
    catch (const fs::filesystem_error &e)
    {
        std::ostringstream oss;
        oss << "Experienced filesystem error when building patches. " << e.what();
        reportError(oss.str(), "Filesystem Error");
    }

    /*
    ** Now establish parent child relationships between patch categories. Do this by
    ** scanning for names; setting the 'root' to everything without a slash
    ** and finding the parent in the name map for everything with a slash
    */

    std::map<std::string, int> nameToLocalIndex;
    int idx = 0;
    for (auto &pc : local_categories)
        nameToLocalIndex[pc.name] = idx++;

    for (auto &pc : local_categories)
    {
        if (pc.name.find(PATH_SEPARATOR) == std::string::npos)
        {
            pc.isRoot = true;
        }
        else
        {
            pc.isRoot = false;
            std::string parent = pc.name.substr(0, pc.name.find_last_of(PATH_SEPARATOR));
            local_categories[nameToLocalIndex[parent]].children.push_back(pc);
        }
    }

    /*
    ** We need to sort the local patch category child to make sure subfolders remain
    ** sorted when displayed using the child data structure in the menu view.
    */

    auto catCompare = [this](const PatchCategory &c1, const PatchCategory &c2) -> bool {
        return strnatcasecmp(c1.name.c_str(), c2.name.c_str()) < 0;
    };
    for (auto &pc : local_categories)
    {
        std::sort(pc.children.begin(), pc.children.end(), catCompare);
    }

    /*
    ** Now we need to prune categories with nothing in their children.
    ** Start by updating the numberOfPatchesInCatgoryAndChildren from the root.
    ** This is complicated because the child list is a copy of values which
    ** I should fix one day. FIXME fix that to avoid this sort of double copy
    ** nonsense. But this keeps it consistent. At a price. Sorry!
    */
    std::function<void(PatchCategory &)> recCorrect = [&recCorrect, &nameToLocalIndex,
                                                       &local_categories](PatchCategory &c) {
        local_categories[nameToLocalIndex[c.name]].numberOfPatchesInCategoryAndChildren =
            c.numberOfPatchesInCategory;
        for (auto &ckid : c.children)
        {
            recCorrect(local_categories[nameToLocalIndex[ckid.name]]);
            ckid.numberOfPatchesInCategoryAndChildren =
                local_categories[nameToLocalIndex[ckid.name]].numberOfPatchesInCategoryAndChildren;

            local_categories[nameToLocalIndex[c.name]].numberOfPatchesInCategoryAndChildren +=
                local_categories[nameToLocalIndex[ckid.name]].numberOfPatchesInCategoryAndChildren;
        }
        c.numberOfPatchesInCategoryAndChildren =
            local_categories[nameToLocalIndex[c.name]].numberOfPatchesInCategoryAndChildren;
    };

    for (auto &c : local_categories)
    {
        if (c.isRoot)
        {
            recCorrect(c);
        }
    }

    /*
    ** Then copy our local patch category onto the member and be done
    */
    for (auto &pc : local_categories)
    {
        categories.push_back(pc);
    }
}

void SurgeStorage::refresh_wtlist()
{
    wt_category.clear();
    wt_list.clear();

    refresh_wtlistAddDir(false, "wavetables");

    firstThirdPartyWTCategory = wt_category.size();
    if (extraThirdPartyWavetablesPath.empty() ||
        !fs::is_directory(extraThirdPartyWavetablesPath / "wavetables_3rdparty"))
    {
        refresh_wtlistAddDir(false, "wavetables_3rdparty");
    }
    else
    {
        refresh_wtlistFrom(false, extraThirdPartyWavetablesPath, "wavetables_3rdparty");
    }
    firstUserWTCategory = wt_category.size();
    refresh_wtlistAddDir(true, "Wavetables");

    if (!extraUserWavetablesPath.empty())
    {
        refresh_wtlistFrom(true, extraUserWavetablesPath, "");
    }

    wtCategoryOrdering = std::vector<int>(wt_category.size());
    std::iota(wtCategoryOrdering.begin(), wtCategoryOrdering.end(), 0);

    // This nonsense deals with the fact that \ < ' ' but ' ' < / and we want "foo bar/h" and
    // "foo/bar" to sort consistently on mac and win. See #1218
    auto categoryCompare = [this](const int &i1, const int &i2) -> bool {
        auto n1 = wt_category[i1].name;
        for (auto i = 0; i < n1.length(); ++i)
            if (n1[i] == '\\')
                n1[i] = '/';

        auto n2 = wt_category[i2].name;
        for (auto i = 0; i < n2.length(); ++i)
            if (n2[i] == '\\')
                n2[i] = '/';

        // OK so now we need to split by path. Sigh.
        // This is because "EMU VSCO/FOO comes after "EMU" but before "ENU/BAR"
        auto split = [](const auto &q) {
            std::vector<std::string> res;
            size_t p = 0, np = 0;
            while (np != std::string::npos)
            {
                np = q.find("/", p);
                if (np == std::string::npos)
                {
                    res.push_back(q.substr(p));
                }
                else
                {
                    res.push_back(q.substr(p, np));
                    p = np + 1;
                }
            }

            return res;
        };

        auto v1 = split(n1);
        auto v2 = split(n2);

        auto sz = std::min(v1.size(), v2.size());
        for (int i = 0; i < sz; ++i)
        {
            if (v1[i] != v2[i])
                return strnatcasecmp(v1[i].c_str(), v2[i].c_str()) < 0;
        }

        return strnatcasecmp(n1.c_str(), n2.c_str()) < 0;
    };

    int groups[4] = {0, firstThirdPartyWTCategory, firstUserWTCategory, (int)wt_category.size()};

    for (int i = 0; i < 3; i++)
    {
        std::sort(std::next(wtCategoryOrdering.begin(), groups[i]),
                  std::next(wtCategoryOrdering.begin(), groups[i + 1]), categoryCompare);
    }

    for (int i = 0; i < wt_category.size(); i++)
        wt_category[wtCategoryOrdering[i]].order = i;

    wtOrdering = std::vector<int>();

    auto wtCompare = [this](const int &i1, const int &i2) -> bool {
        return strnatcasecmp(wt_list[i1].name.c_str(), wt_list[i2].name.c_str()) < 0;
    };

    // Sort wavetables per category in the category order.
    for (auto c : wtCategoryOrdering)
    {
        int start = wtOrdering.size();

        for (int i = 0; i < wt_list.size(); i++)
            if (wt_list[i].category == c)
                wtOrdering.push_back(i);

        int end = wtOrdering.size();

        std::sort(std::next(wtOrdering.begin(), start), std::next(wtOrdering.begin(), end),
                  wtCompare);
    }

    for (int i = 0; i < wt_list.size(); i++)
        wt_list[wtOrdering[i]].order = i;
}

void SurgeStorage::refresh_wtlistAddDir(bool userDir, const std::string &subdir)
{
    refresh_wtlistFrom(userDir, userDir ? userDataPath : datapath, subdir);
}

void SurgeStorage::refresh_wtlistFrom(bool isUser, const fs::path &p, const std::string &subdir)
{
    std::vector<std::string> supportedTableFileTypes;
    supportedTableFileTypes.push_back(".wt");
    supportedTableFileTypes.push_back(".wav");

    refreshPatchOrWTListAddDir(
        isUser, p, subdir,
        [supportedTableFileTypes](std::string in) -> bool {
            for (auto q : supportedTableFileTypes)
            {
                if (_stricmp(q.c_str(), in.c_str()) == 0)
                    return true;
            }
            return false;
        },
        wt_list, wt_category);
}

void SurgeStorage::perform_queued_wtloads()
{
    SurgePatch &patch =
        getPatch(); // Change here is for performance and ease of debugging, simply not calling
                    // getPatch so many times. Code should behave identically.
    for (int sc = 0; sc < n_scenes; sc++)
    {
        for (int o = 0; o < n_oscs; o++)
        {
            if (patch.scene[sc].osc[o].wt.queue_id != -1)
            {
                if (patch.scene[sc].osc[o].wt.everBuilt)
                    patch.isDirty = true;
                load_wt(patch.scene[sc].osc[o].wt.queue_id, &patch.scene[sc].osc[o].wt,
                        &patch.scene[sc].osc[o]);
                patch.scene[sc].osc[o].wt.is_dnd_imported = false;
                patch.scene[sc].osc[o].wt.refresh_display = true;
            }
            else if (patch.scene[sc].osc[o].wt.queue_filename[0])
            {
                if (!(uses_wavetabledata(patch.scene[sc].osc[o].type.val.i)))
                {
                    patch.scene[sc].osc[o].queue_type = ot_wavetable;
                }
                int wtidx = -1, ct = 0;
                for (const auto &wti : wt_list)
                {
                    if (path_to_string(wti.path) == patch.scene[sc].osc[0].wt.queue_filename)
                    {
                        wtidx = ct;
                    }
                    ct++;
                }

                patch.scene[sc].osc[o].wt.current_id = wtidx;
                load_wt(patch.scene[sc].osc[o].wt.queue_filename, &patch.scene[sc].osc[o].wt,
                        &patch.scene[sc].osc[o]);
                patch.scene[sc].osc[o].wt.is_dnd_imported = true;
                patch.scene[sc].osc[o].wt.refresh_display = true;
                if (patch.scene[sc].osc[o].wt.everBuilt)
                    patch.isDirty = true;
            }
        }
    }
}

void SurgeStorage::load_wt(int id, Wavetable *wt, OscillatorStorage *osc)
{
    wt->current_id = id;
    wt->queue_id = -1;

    if (wt_list.empty() && id == 0)
    {
#if HAS_JUCE
        load_wt_wt_mem(SurgeSharedBinary::memoryWavetable_wt,
                       SurgeSharedBinary::memoryWavetable_wtSize, wt);
#endif
        if (osc)
        {
            osc->wavetable_display_name = "Sin to Saw";
        }

        return;
    }

    if (id < 0)
    {
        return;
    }

    if (id >= wt_list.size())
    {
        return;
    }

    load_wt(path_to_string(wt_list[id].path), wt, osc);

    if (osc)
    {
        osc->wavetable_display_name = wt_list.at(id).name;
    }
}

void SurgeStorage::load_wt(string filename, Wavetable *wt, OscillatorStorage *osc)
{
    wt->current_filename = wt->queue_filename;
    wt->queue_filename = "";

    std::string extension = filename.substr(filename.find_last_of('.'), filename.npos);

    for (unsigned int i = 0; i < extension.length(); i++)
    {
        extension[i] = tolower(extension[i]);
    }

    bool loaded = false;
    std::string metadata;

    if (extension.compare(".wt") == 0)
    {
        loaded = load_wt_wt(filename, wt, metadata);
    }
    else if (extension.compare(".wav") == 0)
    {
        loaded = load_wt_wav_portable(filename, wt, metadata);
    }
    else
    {
        std::ostringstream oss;
        oss << "Unable to load file with extension " << extension
            << "! Surge XT only supports .wav and .wt wavetable files!";
        reportError(oss.str(), "Error");
    }

    if (osc && loaded)
    {
        auto fn = filename.substr(filename.find_last_of(PATH_SEPARATOR) + 1, filename.npos);
        std::string fnnoext = fn.substr(0, fn.find_last_of('.'));

        if (fnnoext.length() > 0)
        {
            osc->wavetable_display_name = fnnoext;
        }

        if (metadata.empty())
        {
            osc->wavetable_formula = {};
            osc->wavetable_formula_res_base = 5;
            osc->wavetable_formula_nframes = 10;
        }
        else
        {
            if (!parse_wt_metadata(metadata, osc))
            {
                reportError("Unable to parse metadata", "WaveTable Load");
                std::cerr << metadata << std::endl;
                osc->wavetable_formula = {};
                osc->wavetable_formula_res_base = 5;
                osc->wavetable_formula_nframes = 10;
            }
        }
    }
}

bool SurgeStorage::load_wt_wt(string filename, Wavetable *wt, std::string &metadata)
{
    metadata = {};
    std::filebuf f;

    if (!f.open(string_to_path(filename), std::ios::binary | std::ios::in))
    {
        return false;
    }

    wt_header wh;

    memset(&wh, 0, sizeof(wt_header));

    size_t read = f.sgetn(reinterpret_cast<char *>(&wh), sizeof(wh));

    if (!(wh.tag[0] == 'v' && wh.tag[1] == 'a' && wh.tag[2] == 'w' && wh.tag[3] == 't'))
    {
        // SOME sort of error reporting is appropriate
        return false;
    }

    size_t ds;

    if (mech::endian_read_int16LE(wh.flags) & wtf_int16)
    {
        ds = sizeof(short) * mech::endian_read_int16LE(wh.n_tables) *
             mech::endian_read_int32LE(wh.n_samples);
    }
    else
    {
        ds = sizeof(float) * mech::endian_read_int16LE(wh.n_tables) *
             mech::endian_read_int32LE(wh.n_samples);
    }

    const std::unique_ptr<char[]> data{new char[ds]};
    read = f.sgetn(data.get(), ds);

    if (read != ds)
    {
        /* Somehow the file is corrupt. We have a few options
         * including throw an error here but I think
         * the best thing to do is just zero pad. In the
         * factory set, the 'OneShot/Pulse' wavetable
         * has this problem as a future test case.
         */
        auto dpad = data.get() + read;
        auto drest = ds - read;
        memset(dpad, 0, drest);
    }

    if (mech::endian_read_int16LE(wh.flags) & wtf_has_metadata)
    {
        std::ostringstream xml;
        char buffer[1024]; // Adjust buffer size as needed
        std::streamsize bytesRead;

        while ((bytesRead = f.sgetn(buffer, sizeof(buffer))))
        {
            // Process the data read into 'buffer'
            xml.write(buffer, bytesRead);
        }

        metadata = xml.str();
    }

    waveTableDataMutex.lock();
    bool wasBuilt = wt->BuildWT(data.get(), wh, false);
    waveTableDataMutex.unlock();

    if (!wasBuilt)
    {
        std::ostringstream oss;
        oss << "Wavetable could not be built, which means it has too many frames or samples per "
               "frame.\n"
            << " You have provided " << wh.n_tables << " frames with " << wh.n_samples
            << "samples per frame, while the limit is " << max_subtables << " frames and "
            << max_wtable_size << " samples per frame.\n"
            << "In some cases, Surge XT detects this situation inconsistently, which can lead to a "
               "potentially volatile state\n."
            << "It is recommended to restart Surge XT and not load "
               "the problematic wavetable again.\n\n"
            << " If you would like, please attach the wavetable which caused this error to a new "
               "GitHub issue at "
            << stringRepository;
        reportError(oss.str(), "Wavetable Loading Error");
    }
    return wasBuilt;
}

bool SurgeStorage::load_wt_wt_mem(const char *data, size_t dataSize, Wavetable *wt)
{
    wt_header wh;
    if (dataSize < sizeof(wt_header))
        return false;

    memcpy(&wh, data, sizeof(wt_header));

    if (!(wh.tag[0] == 'v' && wh.tag[1] == 'a' && wh.tag[2] == 'w' && wh.tag[3] == 't'))
    {
        // SOME sort of error reporting is appropriate
        return false;
    }

    size_t ds;
    if (mech::endian_read_int16LE(wh.flags) & wtf_int16)
        ds = sizeof(short) * mech::endian_read_int16LE(wh.n_tables) *
             mech::endian_read_int32LE(wh.n_samples);
    else
        ds = sizeof(float) * mech::endian_read_int16LE(wh.n_tables) *
             mech::endian_read_int32LE(wh.n_samples);

    if (dataSize < ds + sizeof(wt_header))
    {
        std::cout << "Data size " << dataSize << " < " << ds << " + " << sizeof(wt_header)
                  << std::endl;
        return false;
    }

    const char *wtData = data + sizeof(wt_header);
    waveTableDataMutex.lock();
    bool wasBuilt = wt->BuildWT((void *)wtData, wh, false);
    waveTableDataMutex.unlock();

    if (!wasBuilt)
    {
        std::ostringstream oss;
        oss << "Wavetable could not be built, which means it has too many frames or samples per "
               "frame.\n"
            << " You have provided " << wh.n_tables << " frames with " << wh.n_samples
            << "samples per frame, while the limit is " << max_subtables << " frames and "
            << max_wtable_size << " samples per frame.\n"
            << "In some cases, Surge XT detects this situation inconsistently, which can lead to a "
               "potentially volatile state\n."
            << "It is recommended to restart Surge XT and not load "
               "the problematic wavetable again.\n\n"
            << " If you would like, please attach the wavetable which caused this error to a new "
               "GitHub issue at "
            << stringRepository;
        reportError(oss.str(), "Wavetable Loading Error");
    }
    return wasBuilt;
}

bool SurgeStorage::export_wt_wt_portable(const fs::path &fname, Wavetable *wt,
                                         const std::string &metadata)
{
    std::string errorMessage{"Unkown error"};
    std::filebuf wfp;

    if (!wfp.open(fname, std::ios::binary | std::ios::out))
    {
        errorMessage = "Unable to open file " + fname.u8string() + "!";
        errorMessage += std::strerror(errno);

        reportError(errorMessage, "Wavetable Export");

        return false;
    }

    wt_header wth;
    wth.tag[0] = 'v';
    wth.tag[1] = 'a';
    wth.tag[2] = 'w';
    wth.tag[3] = 't';

    wth.n_samples = wt->size;
    wth.n_tables = wt->n_tables;
    wth.flags = wt->flags;
    if (!metadata.empty())
        wth.flags |= wtf_has_metadata;

    wfp.sputn((const char *)&wth, 12);

    bool is16 = (wt->flags & wtf_int16) || (wt->flags & wtf_int16_is_16);

    if (is16)
    {
        for (int i = 0; i < wt->n_tables; ++i)
        {
            wfp.sputn((const char *)&wt->TableI16WeakPointers[0][i][FIRoffsetI16],
                      wt->size * sizeof(short));
        }
    }
    else
    {
        for (int i = 0; i < wt->n_tables; ++i)
        {
            wfp.sputn((const char *)&wt->TableF32WeakPointers[0][i][0], wt->size * sizeof(float));
        }
    }

    if (!metadata.empty())
    {
        wfp.sputn(metadata.c_str(), metadata.length() + 1); // include null term
    }

    wfp.close();

    return true;
}

std::string SurgeStorage::make_wt_metadata(OscillatorStorage *oscdata)
{
    bool hasMeta{false};
    TiXmlDocument doc("wtmeta");
    TiXmlElement root("wtmeta");
    TiXmlElement surge("surge");
    if (!oscdata->wavetable_formula.empty())
    {
        TiXmlElement script("script");

        auto wtfo = oscdata->wavetable_formula;
        auto wtfol = wtfo.length();

        script.SetAttribute(
            "lua", Surge::Storage::base64_encode((unsigned const char *)wtfo.c_str(), wtfol));
        script.SetAttribute("nframes", oscdata->wavetable_formula_nframes);
        script.SetAttribute("res_base", oscdata->wavetable_formula_res_base);
        surge.InsertEndChild(script);
        hasMeta = true;
    }

    root.InsertEndChild(surge);
    doc.InsertEndChild(root);
    if (hasMeta)
    {
        std::string res;
        res << doc;
        return res;
    }
    return {};
}
bool SurgeStorage::parse_wt_metadata(const std::string &m, OscillatorStorage *oscdata)
{
    TiXmlDocument doc("wtmeta");
    doc.Parse(m.c_str());

    auto root = TINYXML_SAFE_TO_ELEMENT(doc.FirstChildElement("wtmeta"));
    if (!root)
    {
        std::cout << "NO ROOT" << std::endl;
        return false;
    }

    auto surge = TINYXML_SAFE_TO_ELEMENT(root->FirstChildElement("surge"));
    if (!surge)
    {
        std::cout << "NO SURGE" << std::endl;
        return false;
    }

    auto script = TINYXML_SAFE_TO_ELEMENT(surge->FirstChildElement("script"));
    if (!script)
    {
        std::cout << "NO SCRIPT" << std::endl;
        return false;
    }

    int n, s;
    if (script->QueryIntAttribute("nframes", &n) != TIXML_SUCCESS)
    {
        std::cout << "NO NFRAMES" << std::endl;
        return false;
    }
    if (script->QueryIntAttribute("res_base", &s) != TIXML_SUCCESS)
    {
        std::cout << "NO RES_BASE" << std::endl;
        return false;
    }
    auto bscript = script->Attribute("lua");
    if (!bscript)
    {
        std::cout << "NO LUA" << std::endl;
        return false;
    }

    oscdata->wavetable_formula = Surge::Storage::base64_decode(bscript);
    oscdata->wavetable_formula_nframes = n;
    oscdata->wavetable_formula_res_base = s;

    return true;
}

bool SurgeStorage::getOverrideDataHome(std::string &v)
{
    bool r = false;
    if (auto *c = getenv("PIPELINE_OVERRIDE_DATA_HOME"))
    {
        r = true;
        v = c;
    }

    if (auto *c = getenv("SURGE_DATA_HOME"))
    {
        r = true;
        v = c;
    }

    return r;
}

int SurgeStorage::get_clipboard_type() const { return clipboard_type; }

int SurgeStorage::getAdjacentWaveTable(int id, bool direction) const
{
    int n = wt_list.size();
    if (!n)
        return -1;

    // See comment in SurgeSynthesizerIO::jogPatch and #319
    if (id < 0 || id > n - 1)
    {
        return wtOrdering[0];
    }
    else
    {
        int order = wt_list[id].order;

        if (direction)
            order = (order >= (n - 1)) ? 0 : order + 1; // see comment in jogPatch for that >= vs ==
        else
            order = (order <= 0) ? n - 1 : order - 1;

        return wtOrdering[order];
    }
}

std::string SurgeStorage::getCurrentWavetableName(OscillatorStorage *oscdata)
{
    waveTableDataMutex.lock();

    std::string wttxt;
    int wtid = oscdata->wt.current_id;

    if (!oscdata->wavetable_display_name.empty())
    {
        wttxt = oscdata->wavetable_display_name;
    }
    else if ((wtid >= 0) && (wtid < wt_list.size()))
    {
        wttxt = wt_list.at(wtid).name;
    }
    else if (oscdata->wt.flags & wtf_is_sample)
    {
        wttxt = "(Patch Sample)";
    }
    else
    {
        wttxt = "(Patch Wavetable)";
    }

    waveTableDataMutex.unlock();

    return wttxt;
}

void SurgeStorage::clipboard_copy(int type, int scene, int entry, modsources ms)
{
    bool includemod = false, includeall = false;
    int cgroup = -1;
    int cgroup_e = -1;
    int id = -1;

    if (type & cp_oscmod)
    {
        type = cp_osc;
        includemod = true;
    }

    clipboard_type = type;

    if (type & cp_osc)
    {
        cgroup = cg_OSC;
        cgroup_e = entry;
        id = getPatch().scene[scene].osc[entry].type.id; // first parameter id

        if (uses_wavetabledata(getPatch().scene[scene].osc[entry].type.val.i))
        {
            clipboard_wt[0].Copy(&getPatch().scene[scene].osc[entry].wt);
            clipboard_wt_names[0] = getPatch().scene[scene].osc[entry].wavetable_display_name;
        }

        clipboard_extraconfig[0] = getPatch().scene[scene].osc[entry].extraConfig;
    }

    if (type & cp_lfo)
    {
        cgroup = cg_LFO;
        cgroup_e = entry + ms_lfo1;
        id = getPatch().scene[scene].lfo[entry].shape.id;

        if (getPatch().scene[scene].lfo[entry].shape.val.i == lt_stepseq)
        {
            clipboard_stepsequences[0] = getPatch().stepsequences[scene][entry];
        }

        if (getPatch().scene[scene].lfo[entry].shape.val.i == lt_mseg)
        {
            clipboard_msegs[0] = getPatch().msegs[scene][entry];
        }

        if (getPatch().scene[scene].lfo[entry].shape.val.i == lt_formula)
        {
            clipboard_formulae[0] = getPatch().formulamods[scene][entry];
        }

        for (int idx = 0; idx < max_lfo_indices; ++idx)
        {
            strncpy(clipboard_modulator_names[0][idx], getPatch().LFOBankLabel[scene][entry][idx],
                    CUSTOM_CONTROLLER_LABEL_SIZE);
        }
    }

    if (type & cp_scene)
    {
        includemod = true;
        includeall = true;
        id = getPatch().scene[scene].octave.id;

        for (int i = 0; i < n_lfos; i++)
        {
            clipboard_stepsequences[i] = getPatch().stepsequences[scene][i];
            clipboard_msegs[i] = getPatch().msegs[scene][i];
            clipboard_formulae[i] = getPatch().formulamods[scene][i];

            for (int idx = 0; idx < max_lfo_indices; ++idx)
            {
                strncpy(clipboard_modulator_names[i][idx], getPatch().LFOBankLabel[scene][i][idx],
                        CUSTOM_CONTROLLER_LABEL_SIZE);
            }
        }

        for (int i = 0; i < n_oscs; i++)
        {
            clipboard_wt[i].Copy(&getPatch().scene[scene].osc[i].wt);
            clipboard_wt_names[i] = getPatch().scene[scene].osc[i].wavetable_display_name;
            clipboard_extraconfig[i] = getPatch().scene[scene].osc[i].extraConfig;
        }

        auto fxOffset = 0;
        if (scene == 0)
        {
            fxOffset = 0;
        }
        else if (scene == 1)
        {
            fxOffset = 4;
        }
        else
        {
            throw std::logic_error(
                "Scene not 0 or 1; have you gone multi-scene without updating clipboard?");
        }

        for (int i = 0; i < n_fx_per_chain; ++i)
        {
            auto sl = fxslot_order[i + fxOffset];
            if (!clipboard_scenefx[i])
                clipboard_scenefx[i] = std::make_unique<Surge::FxClipboard::Clipboard>();
            Surge::FxClipboard::copyFx(this, &getPatch().fx[sl], *(clipboard_scenefx[i]));
        }

        clipboard_primode = getPatch().scene[scene].monoVoicePriorityMode;
        clipboard_envmode = getPatch().scene[scene].monoVoiceEnvelopeMode;
    }

    modRoutingMutex.lock();

    {
        clipboard_p.clear();
        clipboard_modulation_scene.clear();
        clipboard_modulation_voice.clear();
        clipboard_modulation_global.clear();

        std::set<int> used_entries;

        int n = getPatch().param_ptr.size();

        for (int i = 0; i < n; i++)
        {
            Parameter p = *getPatch().param_ptr[i];

            if (((p.ctrlgroup == cgroup) || (cgroup < 0)) &&
                ((p.ctrlgroup_entry == cgroup_e) || (cgroup_e < 0)) && (p.scene == (scene + 1)))
            {
                p.id = p.id - id;
                used_entries.insert(p.id);
                clipboard_p.push_back(p);
            }
        }

        if (includemod)
        {
            int idoffset = 0;

            if (!includeall)
            {
                idoffset = -id + n_global_params;
            }

            n = getPatch().scene[scene].modulation_voice.size();
            for (int i = 0; i < n; i++)
            {
                ModulationRouting m;
                m.source_id = getPatch().scene[scene].modulation_voice[i].source_id;
                m.source_index = getPatch().scene[scene].modulation_voice[i].source_index;
                m.depth = getPatch().scene[scene].modulation_voice[i].depth;
                m.destination_id =
                    getPatch().scene[scene].modulation_voice[i].destination_id + idoffset;

                if (includeall || (used_entries.find(m.destination_id) != used_entries.end()))
                {
                    clipboard_modulation_voice.push_back(m);
                }
            }

            n = getPatch().scene[scene].modulation_scene.size();
            for (int i = 0; i < n; i++)
            {
                ModulationRouting m;
                m.source_id = getPatch().scene[scene].modulation_scene[i].source_id;
                m.source_index = getPatch().scene[scene].modulation_scene[i].source_index;
                m.depth = getPatch().scene[scene].modulation_scene[i].depth;
                m.destination_id =
                    getPatch().scene[scene].modulation_scene[i].destination_id + idoffset;

                if (includeall || (used_entries.find(m.destination_id) != used_entries.end()))
                {
                    clipboard_modulation_scene.push_back(m);
                }
            }

            if (type & cp_scene)
            {
                n = getPatch().modulation_global.size();
                for (int i = 0; i < n; i++)
                {
                    if (getPatch().modulation_global[i].source_scene != scene)
                    {
                        continue;
                    }

                    ModulationRouting m;
                    m.source_id = getPatch().modulation_global[i].source_id;
                    m.source_index = getPatch().modulation_global[i].source_index;
                    m.source_scene = getPatch().modulation_global[i].source_scene;
                    m.depth = getPatch().modulation_global[i].depth;

                    auto did = getPatch().modulation_global[i].destination_id;
                    auto dpar = getPatch().param_ptr[did];
                    if (dpar->ctrlgroup == cg_FX)
                    {
                        assert(scene < 2);
                        for (int q = 0; q < n_fx_per_chain; ++q)
                        {
                            auto srcSl = fxslot_order[q + scene * 4];
                            auto tgtSl = fxslot_order[q + (1 - scene) * 4];
                            if (dpar->ctrlgroup_entry == srcSl)
                            {
                                int64_t d0 =
                                    getPatch().fx[tgtSl].p[0].id - getPatch().fx[srcSl].p[0].id;
                                m.destination_id =
                                    getPatch().modulation_global[i].destination_id + d0;
                            }
                        }
                    }
                    else
                    {
                        m.destination_id = getPatch().modulation_global[i].destination_id;
                    }
                    clipboard_modulation_global.push_back(m);
                }
            }
        }

        if (type & cp_modulator_target)
        {
            if (entry >= 0)
            {
                ms = (modsources)(entry + ms_lfo1);
            }

            n = getPatch().scene[scene].modulation_voice.size();
            for (int i = 0; i < n; i++)
            {
                if (getPatch().scene[scene].modulation_voice[i].source_id != ms)
                {
                    continue;
                }

                ModulationRouting m;
                m.source_id = getPatch().scene[scene].modulation_voice[i].source_id;
                m.source_index = getPatch().scene[scene].modulation_voice[i].source_index;
                m.depth = getPatch().scene[scene].modulation_voice[i].depth;
                m.destination_id = getPatch().scene[scene].modulation_voice[i].destination_id;
                clipboard_modulation_voice.push_back(m);
            }

            n = getPatch().scene[scene].modulation_scene.size();
            for (int i = 0; i < n; i++)
            {
                if (getPatch().scene[scene].modulation_scene[i].source_id != ms)
                {
                    continue;
                }

                ModulationRouting m;
                m.source_id = getPatch().scene[scene].modulation_scene[i].source_id;
                m.source_index = getPatch().scene[scene].modulation_scene[i].source_index;
                m.depth = getPatch().scene[scene].modulation_scene[i].depth;
                m.destination_id = getPatch().scene[scene].modulation_scene[i].destination_id;
                clipboard_modulation_scene.push_back(m);
            }

            n = getPatch().modulation_global.size();
            for (int i = 0; i < n; i++)
            {
                if (getPatch().modulation_global[i].source_id != ms ||
                    getPatch().modulation_global[i].source_scene != scene)
                {
                    continue;
                }

                ModulationRouting m;
                m.source_id = getPatch().modulation_global[i].source_id;
                m.source_index = getPatch().modulation_global[i].source_index;
                m.source_scene = getPatch().modulation_global[i].source_scene;
                m.depth = getPatch().modulation_global[i].depth;
                m.destination_id = getPatch().modulation_global[i].destination_id;
                clipboard_modulation_global.push_back(m);
            }
        }
    }

    modRoutingMutex.unlock();
}

void SurgeStorage::clipboard_paste(
    int type, int scene, int entry, modsources ms, std::function<bool(int, modsources)> isValid,
    std::function<void(std::unique_ptr<Surge::FxClipboard::Clipboard> &, int)> updateFxInSlot)

{
    assert(scene < n_scenes);

    if (!(type & clipboard_type))
    {
        return;
    }

    int cgroup = -1;
    int cgroup_e = -1;
    int id = -1;
    int n = clipboard_p.size();
    int start = 0;

    // fixme make this one or the other
    // if (!n)
    //    return;

    if (type & cp_osc)
    {
        cgroup = cg_OSC;
        cgroup_e = entry;
        id = getPatch().scene[scene].osc[entry].type.id; // first parameter id
        getPatch().scene[scene].osc[entry].type.val.i = clipboard_p[0].val.i;
        start = 1;
        getPatch().update_controls(false, &getPatch().scene[scene].osc[entry]);

        getPatch().scene[scene].osc[entry].extraConfig = clipboard_extraconfig[0];
    }

    if (type & cp_lfo)
    {
        cgroup = cg_LFO;
        cgroup_e = entry + ms_lfo1;
        id = getPatch().scene[scene].lfo[entry].shape.id;
    }

    if (type & cp_scene)
    {
        id = getPatch().scene[scene].octave.id;

        for (int i = 0; i < n_lfos; i++)
        {
            getPatch().stepsequences[scene][i] = clipboard_stepsequences[i];
            getPatch().msegs[scene][i] = clipboard_msegs[i];
            getPatch().formulamods[scene][i] = clipboard_formulae[i];

            for (int idx = 0; idx < max_lfo_indices; ++idx)
            {
                strncpy(getPatch().LFOBankLabel[scene][i][idx], clipboard_modulator_names[i][idx],
                        CUSTOM_CONTROLLER_LABEL_SIZE);
            }
        }

        for (int i = 0; i < n_oscs; i++)
        {
            getPatch().scene[scene].osc[i].extraConfig = clipboard_extraconfig[i];
            getPatch().scene[scene].osc[i].wt.Copy(&clipboard_wt[i]);
            getPatch().scene[scene].osc[i].wavetable_display_name = clipboard_wt_names[i];
        }

        auto fxOffset = 0;
        if (scene == 0)
        {
            fxOffset = 0;
        }
        else if (scene == 1)
        {
            fxOffset = 4;
        }
        else
        {
            throw std::logic_error(
                "Scene not 0 or 1; have you gone multi-scene without updating clipboard?");
        }
        for (int i = 0; i < n_fx_per_chain; ++i)
        {
            auto sl = fxslot_order[i + fxOffset];
            if (clipboard_scenefx[i] &&
                Surge::FxClipboard::isPasteAvailable(*(clipboard_scenefx[i])))
            {
                updateFxInSlot(clipboard_scenefx[i], sl);
            }
        }

        getPatch().scene[scene].monoVoicePriorityMode = clipboard_primode;
        getPatch().scene[scene].monoVoiceEnvelopeMode = clipboard_envmode;
    }

    if (type == cp_modulator_target)
    {
        // We use an == here rather than an & because in this case we want *only* the mod targets
        // so disable the parameter copies
        n = 0;
    }

    modRoutingMutex.lock();

    auto pushBackOrOverride = [this](std::vector<ModulationRouting> &modvec,
                                     const ModulationRouting &m) {
        bool wasDup = false;
        for (auto &mt : modvec)
        {
            if (mt.destination_id == m.destination_id && mt.source_id == m.source_id &&
                mt.source_scene == m.source_scene && mt.source_index == m.source_index)
            {
                mt.depth = m.depth;
                wasDup = true;
            }
        }
        if (!wasDup)
            modvec.push_back(m);
    };

    {
        for (int i = start; i < n; i++)
        {
            Parameter p = clipboard_p[i];
            int pid = p.id + id;
            getPatch().param_ptr[pid]->val.i = p.val.i;
            getPatch().param_ptr[pid]->temposync = p.temposync;
            getPatch().param_ptr[pid]->set_extend_range(p.extend_range);
            getPatch().param_ptr[pid]->deactivated = p.deactivated;
            getPatch().param_ptr[pid]->absolute = p.absolute;
            getPatch().param_ptr[pid]->porta_constrate = p.porta_constrate;
            getPatch().param_ptr[pid]->porta_gliss = p.porta_gliss;
            getPatch().param_ptr[pid]->porta_retrigger = p.porta_retrigger;
            getPatch().param_ptr[pid]->porta_curve = p.porta_curve;
            getPatch().param_ptr[pid]->deform_type = p.deform_type;

            /*
             * This is a really nasty special case that the bool relative switch
             * modifies the cross-param *type* of the filter cutoff when it is setvalue01ed
             * This is a gross workaround for the bug in #6475 which I will ponder after pushing.
             */
            if (pid == getPatch().scene[scene].f2_cutoff_is_offset.id)
            {
                auto down = p.val.b;
                if (down)
                {
                    getPatch().scene[scene].filterunit[1].cutoff.set_type(ct_freq_mod);
                    getPatch().scene[scene].filterunit[1].cutoff.set_name("Offset");
                }
                else
                {
                    getPatch().scene[scene].filterunit[1].cutoff.set_type(
                        ct_freq_audible_with_tunability);
                    getPatch().scene[scene].filterunit[1].cutoff.set_name("Cutoff");
                }
            }
        }

        if (type & cp_osc)
        {
            if (uses_wavetabledata(getPatch().scene[scene].osc[entry].type.val.i))
            {
                getPatch().scene[scene].osc[entry].wt.Copy(&clipboard_wt[0]);
                getPatch().scene[scene].osc[entry].wavetable_display_name = clipboard_wt_names[0];
            }

            // copy modroutings
            n = clipboard_modulation_voice.size();
            for (int i = 0; i < n; i++)
            {
                ModulationRouting m;
                m.source_id = clipboard_modulation_voice[i].source_id;
                m.source_index = clipboard_modulation_voice[i].source_index;
                m.depth = clipboard_modulation_voice[i].depth;
                m.source_scene = scene;
                m.destination_id =
                    clipboard_modulation_voice[i].destination_id + id - n_global_params;
                pushBackOrOverride(getPatch().scene[scene].modulation_voice, m);
            }

            n = clipboard_modulation_scene.size();
            for (int i = 0; i < n; i++)
            {
                ModulationRouting m;
                m.source_id = clipboard_modulation_scene[i].source_id;
                m.source_index = clipboard_modulation_scene[i].source_index;
                m.depth = clipboard_modulation_scene[i].depth;
                m.source_scene = scene;
                m.destination_id =
                    clipboard_modulation_scene[i].destination_id + id - n_global_params;
                pushBackOrOverride(getPatch().scene[scene].modulation_scene, m);
            }
        }

        if (type & cp_lfo)
        {
            if (getPatch().scene[scene].lfo[entry].shape.val.i == lt_stepseq)
            {
                getPatch().stepsequences[scene][entry] = clipboard_stepsequences[0];
            }

            if (getPatch().scene[scene].lfo[entry].shape.val.i == lt_mseg)
            {
                getPatch().msegs[scene][entry] = clipboard_msegs[0];
            }

            if (getPatch().scene[scene].lfo[entry].shape.val.i == lt_formula)
            {
                getPatch().formulamods[scene][entry] = clipboard_formulae[0];
            }

            for (int idx = 0; idx < max_lfo_indices; ++idx)
            {
                strncpy(getPatch().LFOBankLabel[scene][entry][idx],
                        clipboard_modulator_names[0][idx], CUSTOM_CONTROLLER_LABEL_SIZE);
            }
        }

        if (type & cp_modulator_target)
        {
            if (entry >= 0)
            {
                ms = (modsources)(entry + ms_lfo1);
            }

            // copy modroutings
            n = clipboard_modulation_voice.size();
            for (int i = 0; i < n; i++)
            {
                ModulationRouting m;
                m.source_id = ms;
                m.source_index = clipboard_modulation_voice[i].source_index;
                m.depth = clipboard_modulation_voice[i].depth;
                m.source_scene = scene;
                m.destination_id = clipboard_modulation_voice[i].destination_id;

                if (isValid(m.destination_id + getPatch().scene_start[scene],
                            (modsources)m.source_id))
                {
                    if (isScenelevel((modsources)m.source_id))
                    {
                        pushBackOrOverride(getPatch().scene[scene].modulation_scene, m);
                    }
                    else
                    {
                        pushBackOrOverride(getPatch().scene[scene].modulation_voice, m);
                    }
                }
            }

            n = clipboard_modulation_scene.size();
            for (int i = 0; i < n; i++)
            {
                ModulationRouting m;
                m.source_id = ms;
                m.source_index = clipboard_modulation_scene[i].source_index;
                m.depth = clipboard_modulation_scene[i].depth;
                m.source_scene = scene;
                m.destination_id = clipboard_modulation_scene[i].destination_id;

                if (isValid(m.destination_id + getPatch().scene_start[scene],
                            (modsources)m.source_id))
                {
                    if (isScenelevel((modsources)m.source_id))
                    {
                        pushBackOrOverride(getPatch().scene[scene].modulation_scene, m);
                    }
                    else
                    {
                        pushBackOrOverride(getPatch().scene[scene].modulation_voice, m);
                    }
                }
            }

            n = clipboard_modulation_global.size();
            for (int i = 0; i < n; i++)
            {
                ModulationRouting m;
                m.source_id = ms;
                m.source_index = clipboard_modulation_global[i].source_index;
                m.source_scene = scene; /* clipboard_modulation_global[i].source_scene; */
                m.depth = clipboard_modulation_global[i].depth;
                m.destination_id = clipboard_modulation_global[i].destination_id;

                if (isValid(m.destination_id, (modsources)m.source_id))
                {
                    pushBackOrOverride(getPatch().modulation_global, m);
                }
            }
        }

        if (type & cp_scene)
        {
            getPatch().scene[scene].modulation_voice.clear();
            getPatch().scene[scene].modulation_scene.clear();
            getPatch().update_controls(false);

            n = clipboard_modulation_voice.size();
            for (int i = 0; i < n; i++)
            {
                ModulationRouting m;
                m.source_id = clipboard_modulation_voice[i].source_id;
                m.source_index = clipboard_modulation_voice[i].source_index;
                m.source_scene = scene; /* clipboard_modulation_global[i].source_scene; */
                m.depth = clipboard_modulation_voice[i].depth;
                m.destination_id = clipboard_modulation_voice[i].destination_id;
                pushBackOrOverride(getPatch().scene[scene].modulation_voice, m);
            }

            n = clipboard_modulation_scene.size();
            for (int i = 0; i < n; i++)
            {
                ModulationRouting m;
                m.source_id = clipboard_modulation_scene[i].source_id;
                m.source_index = clipboard_modulation_scene[i].source_index;
                m.source_scene = scene; /* clipboard_modulation_global[i].source_scene; */
                m.depth = clipboard_modulation_scene[i].depth;
                m.destination_id = clipboard_modulation_scene[i].destination_id;
                pushBackOrOverride(getPatch().scene[scene].modulation_scene, m);
            }

            n = clipboard_modulation_global.size();
            for (int i = 0; i < n; i++)
            {
                ModulationRouting m;
                m.source_id = clipboard_modulation_global[i].source_id;
                m.source_index = clipboard_modulation_global[i].source_index;
                m.source_scene = scene; /* clipboard_modulation_global[i].source_scene; */
                m.depth = clipboard_modulation_global[i].depth;
                m.destination_id = clipboard_modulation_global[i].destination_id;

                pushBackOrOverride(getPatch().modulation_global, m);
            }
        }
    }

    modRoutingMutex.unlock();
}

TiXmlElement *SurgeStorage::getSnapshotSection(const char *name)
{
    TiXmlElement *e = TINYXML_SAFE_TO_ELEMENT(snapshotloader.FirstChild(name));
    if (e)
        return e;

    // ok, create a new one then
    TiXmlElement ne(name);
    snapshotloader.InsertEndChild(ne);
    return TINYXML_SAFE_TO_ELEMENT(snapshotloader.FirstChild(name));
}

void SurgeStorage::save_snapshots() { snapshotloader.SaveFile(); }

void SurgeStorage::write_midi_controllers_to_user_default()
{
    TiXmlDocument doc;

    TiXmlElement root("midiconfig");
    TiXmlElement mc("midictrl");

    int n = n_global_params + (n_scene_params * n_scenes);

    for (int i = 0; i < n; i++)
    {
        if (getPatch().param_ptr[i]->midictrl >= 0)
        {
            TiXmlElement mc_e("entry");
            mc_e.SetAttribute("p", i);
            mc_e.SetAttribute("ctrl", getPatch().param_ptr[i]->midictrl);
            mc_e.SetAttribute("chan", getPatch().param_ptr[i]->midichan);
            mc.InsertEndChild(mc_e);
        }
    }

    root.InsertEndChild(mc);

    TiXmlElement cc("customctrl");

    for (int i = 0; i < n_customcontrollers; i++)
    {
        TiXmlElement cc_e("entry");
        cc_e.SetAttribute("p", i);
        cc_e.SetAttribute("ctrl", controllers[i]);
        cc_e.SetAttribute("chan", controllers_chan[i]);
        cc.InsertEndChild(cc_e);
    }

    root.InsertEndChild(cc);
    doc.InsertEndChild(root);

    try
    {
        fs::create_directories(userDataPath);
        auto f = userDataPath / "SurgeMIDIDefaults.xml";
        doc.SaveFile(f);
    }
    catch (const fs::filesystem_error &e)
    {
        // Oh well.
    }
}

void SurgeStorage::setSamplerate(float sr)
{
    // If I am changing my sample rate I will change my internal tables, so this
    // needs to be tuning aware and reapply tuning if needed
    auto s = currentScale;
    bool wasST = isStandardTuning;

    samplerate = sr;
    dsamplerate = sr;
    samplerate_inv = 1.0 / sr;
    dsamplerate_inv = 1.0 / sr;
    dsamplerate_os = dsamplerate * OSC_OVERSAMPLING;
    dsamplerate_os_inv = 1.0 / dsamplerate_os;
    init_tables();

    if (!wasST)
    {
        retuneToScale(s);
    }
}

void SurgeStorage::load_midi_controllers()
{
    auto mcp = userDataPath / "SurgeMIDIDefaults.xml";
    TiXmlDocument mcd;
    TiXmlElement *midiRoot = nullptr;

    if (mcd.LoadFile(mcp))
    {
        midiRoot = mcd.FirstChildElement("midiconfig");
    }

    auto get = [this, midiRoot](const char *n) {
        if (midiRoot)
        {
            auto q = TINYXML_SAFE_TO_ELEMENT(midiRoot->FirstChild(n));

            if (q)
            {
                return q;
            }
        }

        return getSnapshotSection(n);
    };

    TiXmlElement *mc = get("midictrl");
    assert(mc);

    TiXmlElement *entry = TINYXML_SAFE_TO_ELEMENT(mc->FirstChild("entry"));

    while (entry)
    {
        int id, ctrl, chan;

        if (entry->QueryIntAttribute("p", &id) == TIXML_SUCCESS)
        {
            if (entry->QueryIntAttribute("ctrl", &ctrl) == TIXML_SUCCESS)
            {
                getPatch().param_ptr[id]->midictrl = ctrl;
            }

            if (entry->QueryIntAttribute("chan", &chan) == TIXML_SUCCESS)
            {
                getPatch().param_ptr[id]->midichan = chan;
            }
            else
            {
                getPatch().param_ptr[id]->midichan = -1;

                // care for old MIDI config files - duplicate scene A assignment to scene B
                if (id >= n_global_params && id < (n_global_params + n_scene_params))
                {
                    getPatch().param_ptr[id + n_scene_params]->midictrl = ctrl;
                }
            }
        }

        entry = TINYXML_SAFE_TO_ELEMENT(entry->NextSibling("entry"));
    }

    TiXmlElement *cc = get("customctrl");
    assert(cc);

    entry = TINYXML_SAFE_TO_ELEMENT(cc->FirstChild("entry"));

    while (entry)
    {
        int id, ctrl, chan;

        if (entry->QueryIntAttribute("p", &id) == TIXML_SUCCESS)
        {
            if (entry->QueryIntAttribute("ctrl", &ctrl) == TIXML_SUCCESS &&
                id < n_customcontrollers)
            {
                controllers[id] = ctrl;
            }

            if (entry->QueryIntAttribute("chan", &chan) == TIXML_SUCCESS &&
                id < n_customcontrollers)
            {
                controllers_chan[id] = chan;
            }
            else
            {
                controllers_chan[id] = -1;
            }
        }

        entry = TINYXML_SAFE_TO_ELEMENT(entry->NextSibling("entry"));
    }
}

SurgeStorage::~SurgeStorage()
{
#ifndef SURGE_SKIP_ODDSOUND_MTS
    if (oddsound_mts_active_as_main)
        disconnect_as_oddsound_main();

    deinitialize_oddsound();
#endif
}

double shafted_tanh(double x) { return (exp(x) - exp(-x * 1.2)) / (exp(x) + exp(-x)); }

void SurgeStorage::init_tables()
{
    isStandardTuning = true;
    float db60 = powf(10.f, 0.05f * -60.f);
    float _512th = 1.f / 512.f;

    for (int i = 0; i < tuning_table_size; i++)
    {
        table_dB[i] = powf(10.f, 0.05f * ((float)i - 384.f));
        table_pitch[i] = powf(2.f, ((float)i - 256.f) * (1.f / 12.f));
        table_pitch_ignoring_tuning[i] = table_pitch[i];
        table_pitch_inv[i] = 1.f / table_pitch[i];
        table_pitch_inv_ignoring_tuning[i] = table_pitch_inv[i];
        table_note_omega[0][i] =
            (float)sin(2 * M_PI * min(0.5, 440 * table_pitch[i] * dsamplerate_os_inv));
        table_note_omega[1][i] =
            (float)cos(2 * M_PI * min(0.5, 440 * table_pitch[i] * dsamplerate_os_inv));
        table_note_omega_ignoring_tuning[0][i] = table_note_omega[0][i];
        table_note_omega_ignoring_tuning[1][i] = table_note_omega[1][i];
        double k = dsamplerate_os * pow(2.0, (((double)i - 256.0) / 16.0)) / (double)BLOCK_SIZE_OS;
        table_envrate_linear[i] = (float)(1.f / k);
        table_envrate_lpf[i] = (float)(1.f - exp(log(db60) / k));
        table_glide_log[i] = log2(1.0 + (i * _512th * 10.f)) / log2(1.f + 10.f);
        table_glide_exp[511 - i] = 1.0 - table_glide_log[i];
    }

    for (int i = 0; i < 1001; ++i)
    {
        double twelths = i * 1.0 / 12.0 / 1000.0;
        table_two_to_the[i] = pow(2.0, twelths);
        table_two_to_the_minus[i] = pow(2.0, -twelths);
    }

    // include some margin for error (and to avoid denormals in IIR filter clamping)
    nyquist_pitch =
        (float)12.f * log((0.75 * M_PI) / (dsamplerate_os_inv * 2 * M_PI * 440.0)) / log(2.0);

    // This factor used to be hardcoded to 0.997, which thanks to some reverse engineering
    // by @selenologist was deduced to be the factor of a one pole lowpass with a ~21 Hz cutoff.
    // (Please refer to the VU falloff calc at the end of process() method in SurgeSynthesizer.cpp!)
    // But this made the meters fall off at different rates depending on sample rate we're
    // running at, so we unrolled this into an equation to make the VU meters have identical
    // ballistics at any sample rate!
    // We have also decided to make the meters less sluggish, so we increased the cutoff to 60 Hz
    vu_falloff = exp(-2 * M_PI * (60.f * samplerate_inv));

    cpu_falloff = exp(-2 * M_PI * (60.f * samplerate_inv));
}

float SurgeStorage::note_to_pitch(float x)
{

    if (tuningTableIs12TET())
    {
        return note_to_pitch_ignoring_tuning(x);
    }
    else
    {
        x = limit_range(x + 256, 0.f, tuning_table_size - (float)1.e-4);
        int e = (int)x;
        float a = x - (float)e;

        return (1 - a) * table_pitch[e] + a * table_pitch[(e + 1) & 0x1ff];
    }
}

float SurgeStorage::note_to_pitch_inv(float x)
{
    if (tuningTableIs12TET())
    {
        return note_to_pitch_inv_ignoring_tuning(x);
    }
    else
    {
        x = limit_range(x + 256, 0.f, tuning_table_size - (float)1.e-4);
        int e = (int)x;
        float a = x - (float)e;

        return (1 - a) * table_pitch_inv[e] + a * table_pitch_inv[(e + 1) & 0x1ff];
    }
}

float SurgeStorage::note_to_pitch_ignoring_tuning(float x)
{
    x = limit_range(x + 256, 1.e-4f, tuning_table_size - (float)1.e-4);
    // x += 256;
    int e = (int)x;
    float a = x - (float)e;

    float pow2pos = a * 1000.0;
    int pow2idx = (int)pow2pos;
    float pow2frac = pow2pos - pow2idx;
    float pow2v =
        (1 - pow2frac) * table_two_to_the[pow2idx] + pow2frac * table_two_to_the[pow2idx + 1];
    return table_pitch_ignoring_tuning[e] * pow2v;
}

float SurgeStorage::note_to_pitch_inv_ignoring_tuning(float x)
{
    x = limit_range(x + 256, 0.f, tuning_table_size - (float)1.e-4);
    int e = (int)x;
    float a = x - (float)e;

    float pow2pos = a * 1000.0;
    int pow2idx = (int)pow2pos;
    float pow2frac = pow2pos - pow2idx;
    float pow2v = (1 - pow2frac) * table_two_to_the_minus[pow2idx] +
                  pow2frac * table_two_to_the_minus[pow2idx + 1];
    return table_pitch_inv_ignoring_tuning[e] * pow2v;
}

void SurgeStorage::note_to_omega(float x, float &sinu, float &cosi)
{
    x = limit_range(x + 256, 0.f, tuning_table_size - (float)1.e-4);
    // x += 256;
    int e = (int)x;
    float a = x - (float)e;

    sinu = (1 - a) * table_note_omega[0][e] + a * table_note_omega[0][(e + 1) & 0x1ff];
    cosi = (1 - a) * table_note_omega[1][e] + a * table_note_omega[1][(e + 1) & 0x1ff];
}

void SurgeStorage::note_to_omega_ignoring_tuning(float x, float &sinu, float &cosi,
                                                 float /*sampleRate*/)
{
    x = limit_range(x + 256, 0.f, tuning_table_size - (float)1.e-4);
    // x += 256;
    int e = (int)x;
    float a = x - (float)e;

    sinu = (1 - a) * table_note_omega_ignoring_tuning[0][e] +
           a * table_note_omega_ignoring_tuning[0][(e + 1) & 0x1ff];
    cosi = (1 - a) * table_note_omega_ignoring_tuning[1][e] +
           a * table_note_omega_ignoring_tuning[1][(e + 1) & 0x1ff];
}

float SurgeStorage::db_to_linear(float x)
{
    x += 384;
    int e = (int)x;
    float a = x - (float)e;

    return (1 - a) * table_dB[e & 0x1ff] + a * table_dB[(e + 1) & 0x1ff];
}

float SurgeStorage::lookup_waveshape(sst::waveshapers::WaveshaperType entry, float x)
{
    x *= 32.f;
    x += 512.f;
    int e = (int)x;
    float a = x - (float)e;

    if (e > 0x3fd)
        return 1;
    if (e < 1)
        return -1;

    const auto &waveshapers = sst::waveshapers::globalWaveshaperTables.waveshapers[(int)entry];
    return (1 - a) * waveshapers[e & 0x3ff] + a * waveshapers[(e + 1) & 0x3ff];
}

float SurgeStorage::lookup_waveshape_warp(sst::waveshapers::WaveshaperType entry, float x)
{
    x *= 256.f;
    x += 512.f;

    int e = (int)x;
    float a = x - (float)e;

    const auto &waveshapers = sst::waveshapers::globalWaveshaperTables.waveshapers[(int)entry];
    return (1 - a) * waveshapers[e & 0x3ff] + a * waveshapers[(e + 1) & 0x3ff];
}

float SurgeStorage::envelope_rate_lpf(float x)
{
    x *= 16.f;
    x += 256.f;
    int e = (int)x;
    float a = x - (float)e;

    return (1 - a) * table_envrate_lpf[e & 0x1ff] + a * table_envrate_lpf[(e + 1) & 0x1ff];
}

float SurgeStorage::envelope_rate_linear(float x)
{
    x *= 16.f;
    x += 256.f;
    int e = (int)x;
    float a = x - (float)e;

    return (1 - a) * table_envrate_linear[e & 0x1ff] + a * table_envrate_linear[(e + 1) & 0x1ff];
}

float SurgeStorage::envelope_rate_linear_nowrap(float x)
{
    x *= 16.f;
    x += 256.f;
    int e = limit_range((int)x, 0, 0x1ff - 1);
    ;
    float a = x - (float)e;

    return (1 - a) * table_envrate_linear[e & 0x1ff] + a * table_envrate_linear[(e + 1) & 0x1ff];
}

// this function is only valid for x = {0, 1}
float SurgeStorage::glide_exp(float x)
{
    x *= 511.f;
    int e = (int)x;
    float a = x - (float)e;

    return (1 - a) * table_glide_exp[e & 0x1ff] + a * table_glide_exp[(e + 1) & 0x1ff];
}

// this function is only valid for x = {0, 1}
float SurgeStorage::glide_log(float x)
{
    x *= 511.f;
    int e = (int)x;
    float a = x - (float)e;

    return (1 - a) * table_glide_log[e & 0x1ff] + a * table_glide_log[(e + 1) & 0x1ff];
}

bool SurgeStorage::resetToCurrentScaleAndMapping()
{
    try
    {
        currentTuning =
            Tunings::Tuning(currentScale, currentMapping).withSkippedNotesInterpolated();
    }
    catch (Tunings::TuningError &e)
    {
        retuneTo12TETScaleC261Mapping();
        reportError(e.what(), "SCL/KBM Error - Resetting to standard");
    }

    auto t = currentTuning;

    if (tuningApplicationMode == RETUNE_MIDI_ONLY)
    {
        tuningPitch = 32.0;
        tuningPitchInv = 1.0 / 32.0;
        t = twelveToneStandardMapping;
    }
    else
    {
        tuningPitch = currentMapping.tuningFrequency / Tunings::MIDI_0_FREQ;
        tuningPitchInv = 1.0 / tuningPitch;
    }

    for (int i = 0; i < 512; ++i)
    {
        table_pitch[i] = t.frequencyForMidiNoteScaledByMidi0(i - 256);
        table_pitch_inv[i] = 1.f / table_pitch[i];
        table_note_omega[0][i] =
            (float)sin(2 * M_PI * min(0.5, 440 * table_pitch[i] * dsamplerate_os_inv));
        table_note_omega[1][i] =
            (float)cos(2 * M_PI * min(0.5, 440 * table_pitch[i] * dsamplerate_os_inv));
    }

#ifndef SURGE_SKIP_ODDSOUND_MTS
    if (oddsound_mts_active_as_main && !uiThreadChecksTunings)
    {
        for (int i = 0; i < 128; ++i)
        {
            MTS_SetNoteTuning(currentTuning.frequencyForMidiNote(i), i);
        }
        MTS_SetScaleName(currentTuning.scale.description.c_str());
    }
    tuningUpdates++;
#endif
    return true;
}

void SurgeStorage::loadTuningFromSCL(const fs::path &p)
{
    try
    {
        retuneToScale(Tunings::readSCLFile(p.u8string()));
    }
    catch (Tunings::TuningError &e)
    {
        retuneTo12TETScaleC261Mapping();
        reportError(e.what(), "SCL Error");
    }
    if (onTuningChanged)
        onTuningChanged();
}

void SurgeStorage::loadMappingFromKBM(const fs::path &p)
{
    try
    {
        remapToKeyboard(Tunings::readKBMFile(p.u8string()));
    }
    catch (Tunings::TuningError &e)
    {
        remapToConcertCKeyboard();
        reportError(e.what(), "KBM Error");
    }
    if (onTuningChanged)
        onTuningChanged();
}

void SurgeStorage::setTuningApplicationMode(const TuningApplicationMode m)
{
    tuningApplicationMode = m;
    patchStoredTuningApplicationMode = m;
    resetToCurrentScaleAndMapping();
    if (oddsound_mts_active_as_client)
    {
        tuningApplicationMode = RETUNE_MIDI_ONLY;
    }
}

SurgeStorage::TuningApplicationMode SurgeStorage::getTuningApplicationMode() const
{
    return tuningApplicationMode;
}

bool SurgeStorage::skipLoadWtAndPatch = false;

void SurgeStorage::rescanUserMidiMappings()
{
    userMidiMappingsXMLByName.clear();
    std::error_code ec;
    const auto extension{fs::path{".srgmid"}.native()};
    try
    {
        for (const fs::path &d : fs::directory_iterator{userMidiMappingsPath, ec})
        {
            if (d.extension().native() == extension)
            {
                TiXmlDocument doc;
                if (!doc.LoadFile(d))
                    continue;
                const auto r{TINYXML_SAFE_TO_ELEMENT(doc.FirstChild("surge-midi"))};
                if (!r)
                    continue;
                const auto a{r->Attribute("name")};
                if (!a)
                    continue;
                userMidiMappingsXMLByName.emplace(a, std::move(doc));
            }
        }
    }
    catch (const fs::filesystem_error &e)
    {
        std::ostringstream oss;
        oss << "Experienced filesystem error when loading MIDI settings. " << e.what();
        reportError(oss.str(), "Filesystem Error");
    }
}

void SurgeStorage::loadMidiMappingByName(std::string name)
{
    if (userMidiMappingsXMLByName.find(name) == userMidiMappingsXMLByName.end())
    {
        // FIXME - why would this ever happen? Probably show an error
        return;
    }

    auto doc = userMidiMappingsXMLByName[name];
    auto sm = TINYXML_SAFE_TO_ELEMENT(doc.FirstChild("surge-midi"));

    // we can do revision stuff here later if we need to
    if (!sm)
    {
        // Invalid XML Document. Show an error?
        reportError("Unable to locate surge-midi element in XML. Not a valid MIDI mapping!",
                    "Surge MIDI");
        return;
    }

    auto mc = TINYXML_SAFE_TO_ELEMENT(sm->FirstChild("midictrl"));

    if (mc)
    {
        // Clear the current control mapping
        for (int i = 0; i < n_total_params; i++)
        {
            getPatch().param_ptr[i]->midictrl = -1;
        }

        // Apply the new control mapping
        auto map = mc->FirstChildElement("map");

        while (map)
        {
            int i, c, ch;

            if (map->QueryIntAttribute("p", &i) == TIXML_SUCCESS)
            {
                if (map->QueryIntAttribute("cc", &c) == TIXML_SUCCESS)
                {
                    getPatch().param_ptr[i]->midictrl = c;
                }

                if (map->QueryIntAttribute("chan", &ch) == TIXML_SUCCESS)
                {
                    getPatch().param_ptr[i]->midichan = ch;
                }
                else
                {
                    // care for old MIDI config files - duplicate scene A assignment to scene B
                    if (i >= n_global_params && i < (n_global_params + n_scene_params))
                    {
                        getPatch().param_ptr[i + n_scene_params]->midictrl = c;
                    }
                }
            }

            map = map->NextSiblingElement("map");
        }
    }

    auto cc = TINYXML_SAFE_TO_ELEMENT(sm->FirstChild("customctrl"));

    if (cc)
    {
        auto ctrl = cc->FirstChildElement("ctrl");

        while (ctrl)
        {
            int i, cc, ch;

            if (ctrl->QueryIntAttribute("i", &i) == TIXML_SUCCESS)
            {
                if (ctrl->QueryIntAttribute("cc", &cc) == TIXML_SUCCESS)
                {
                    controllers[i] = cc;
                }

                if (ctrl->QueryIntAttribute("chan", &ch) == TIXML_SUCCESS)
                {
                    controllers_chan[i] = ch;
                }
                else
                {
                    controllers_chan[i] = -1;
                }
            }

            ctrl = ctrl->NextSiblingElement("ctrl");
        }
    }
}

void SurgeStorage::storeMidiMappingToName(std::string name)
{
    TiXmlDocument doc;
    TiXmlElement sm("surge-midi");
    sm.SetAttribute("revision", ff_revision);
    sm.SetAttribute("name", name);

    // Build the XML here

    int n = n_global_params + (n_scene_params * n_scenes);

    TiXmlElement mc("midictrl");

    for (int i = 0; i < n; i++)
    {
        if (getPatch().param_ptr[i]->midictrl >= 0)
        {
            TiXmlElement p("map");
            p.SetAttribute("p", i);
            p.SetAttribute("cc", getPatch().param_ptr[i]->midictrl);
            p.SetAttribute("chan", getPatch().param_ptr[i]->midichan);
            mc.InsertEndChild(p);
        }
    }

    sm.InsertEndChild(mc);

    TiXmlElement cc("customctrl");

    for (int i = 0; i < n_customcontrollers; ++i)
    {
        TiXmlElement p("ctrl");
        p.SetAttribute("i", i);
        p.SetAttribute("cc", controllers[i]);
        p.SetAttribute("chan", controllers_chan[i]);
        cc.InsertEndChild(p);
    }

    sm.InsertEndChild(cc);

    doc.InsertEndChild(sm);

    fs::create_directories(userMidiMappingsPath);
    auto fn = userMidiMappingsPath / (name + ".srgmid");

    if (!doc.SaveFile(path_to_string(fn)))
    {
        std::ostringstream oss;
        oss << "Unable to save MIDI settings to '" << fn << "'!";
        reportError(oss.str(), "Error");
    }
}

#ifndef SURGE_SKIP_ODDSOUND_MTS
void SurgeStorage::initialize_oddsound()
{
    if (oddsound_mts_client)
    {
        deinitialize_oddsound();
    }
    oddsound_mts_client = MTS_RegisterClient();
    if (oddsound_mts_client)
    {
        setOddsoundMTSActiveTo(MTS_HasMaster(oddsound_mts_client));
    }
}

void SurgeStorage::deinitialize_oddsound()
{
    if (oddsound_mts_client)
    {
        MTS_DeregisterClient(oddsound_mts_client);
    }
    oddsound_mts_client = nullptr;
    setOddsoundMTSActiveTo(false);
}

void SurgeStorage::setOddsoundMTSActiveTo(bool b)
{
    bool poa = oddsound_mts_active_as_client;
    oddsound_mts_active_as_client = b;
    if (b && b != poa)
    {
        // Oddsound right now is MIDI_ONLY so force that to avoid lingering problems
        tuningApplicationMode = RETUNE_MIDI_ONLY;
    }
    if (!b && b != poa)
    {
        tuningApplicationMode = patchStoredTuningApplicationMode;
    }
}

void SurgeStorage::connect_as_oddsound_main()
{
    if (oddsound_mts_client)
    {
        deinitialize_oddsound();
    }
    if (oddsound_mts_active_as_main)
    // should never happen. This is different from can-register;
    // this is "I am registered as the main"
    {
        MTS_DeregisterMaster();
        oddsound_mts_active_as_main = false;
    }

    // so now if there is a main it isn't me so check if I can register
    if (MTS_CanRegisterMaster())
    {
        oddsound_mts_active_as_main = true;
        MTS_RegisterMaster();
    }
    else
    {
        reportError("Another software program is registered as an MTS-ESP source. "
                    "As such, this session cannot become a source and that other program "
                    "will provide tuning information to this setting. If you want to "
                    "reset the MTS-ESP system, use the 'Reinitialize MTS-ESP' option in Surge XT. "
                    "Alternatively, quit the other program and attempt re-enabling Act as MTS-ESP "
                    "source option.",
                    "MTS-ESP Source Initialization Error");
    }
    lastSentTuningUpdate = -1;

    if (!uiThreadChecksTunings && oddsound_mts_active_as_main)
    {
        for (int i = 0; i < 128; ++i)
        {
            MTS_SetNoteTuning(currentTuning.frequencyForMidiNote(i), i);
        }
        MTS_SetScaleName(currentTuning.scale.description.c_str());
    }
}
void SurgeStorage::disconnect_as_oddsound_main()
{
    oddsound_mts_active_as_main = false;
    MTS_DeregisterMaster();
    initialize_oddsound();
}
void SurgeStorage::send_tuning_update()
{
    if (lastSentTuningUpdate == tuningUpdates)
        return;

    lastSentTuningUpdate = tuningUpdates;
    if (!oddsound_mts_active_as_main)
        return;

    for (int i = 0; i < 128; ++i)
    {
        MTS_SetNoteTuning(currentTuning.frequencyForMidiNote(i), i);
    }
    MTS_SetScaleName(currentTuning.scale.description.c_str());
}
#endif

void SurgeStorage::toggleTuningToCache()
{
    if (isToggledToCache)
    {
        currentScale = cachedToggleOffScale;
        currentMapping = cachedToggleOffMapping;

        isStandardTuning = togglePriorState[0];
        isStandardScale = togglePriorState[1];
        isStandardMapping = togglePriorState[2];

        resetToCurrentScaleAndMapping();
        isToggledToCache = false;
    }
    else
    {
        cachedToggleOffScale = currentScale;
        cachedToggleOffMapping = currentMapping;
        togglePriorState[0] = isStandardTuning;
        togglePriorState[1] = isStandardScale;
        togglePriorState[2] = isStandardMapping;

        retuneTo12TETScaleC261Mapping();
        isToggledToCache = true;
    }
}

bool SurgeStorage::isStandardTuningAndHasNoToggle()
{
    auto res = isStandardTuning;
    if (isToggledToCache)
    {
        res = res && togglePriorState[0];
    }

    return res;
}

void SurgeStorage::resetTuningToggle() { isToggledToCache = false; }

void SurgeStorage::reportError(const std::string &msg, const std::string &title,
                               const ErrorType errorType, bool reportToStdout)
{
    if (reportToStdout)
    {
        std::cout << "Surge Error [" << title << "]\n" << msg << std::endl;
    }
    if (errorListeners.empty())
    {
        std::lock_guard<std::mutex> g(preListenerErrorMutex);
        preListenerErrors.emplace_back(msg, title, errorType);
    }

    for (auto l : errorListeners)
    {
        l->onSurgeError(msg, title, errorType);
    }
}

float SurgeStorage::remapKeyInMidiOnlyMode(float res)
{
    if (!isStandardTuning && tuningApplicationMode == RETUNE_MIDI_ONLY)
    {
        auto idx = (int)floor(res);
        float frac = res - idx; // frac is 0 means use idx; frac is 1 means use idx + 1
        float b0 = currentTuning.logScaledFrequencyForMidiNote(idx) * 12;
        float b1 = currentTuning.logScaledFrequencyForMidiNote(idx + 1) * 12;
        res = (1.f - frac) * b0 + frac * b1;
    }
    return res;
}

Surge::Storage::ScenesOutputData::ScenesOutputData()
{
    for (int i = 0; i < n_scenes; i++)
    {
        for (int j = 0; j < N_OUTPUTS; j++)
        {
            std::shared_ptr<float> block(new float[BLOCK_SIZE], [](float *p) { delete[] p; });
            memset(block.get(), 0, BLOCK_SIZE * sizeof(float));
            sceneData[i][j] = block;
        }
    }
}
const std::shared_ptr<float> &Surge::Storage::ScenesOutputData::getSceneData(int scene,
                                                                             int channel) const
{
    assert(scene < n_scenes && scene >= 0);
    assert(channel < N_OUTPUTS && channel >= 0);
    return sceneData[scene][channel];
}
bool Surge::Storage::ScenesOutputData::thereAreClients(int scene) const
{
    return std::any_of(std::begin(sceneData[scene]), std::end(sceneData[scene]),
                       [](const auto &channel) { return channel.use_count() > 1; });
}
void Surge::Storage::ScenesOutputData::provideSceneData(int scene, int channel, float *data)
{
    if (scene < n_scenes && scene >= 0 && channel < N_OUTPUTS && channel >= 0 &&
        sceneData[scene][channel].use_count() > 1) // we don't provide data if there are no clients
    {
        memcpy(sceneData[scene][channel].get(), data, BLOCK_SIZE * sizeof(float));
    }
}

namespace Surge
{
namespace Storage
{
bool isValidName(const std::string &patchName)
{
    bool valid = false;

    // No need to validate size separately as an empty string won't have visible characters.
    for (const char &c : patchName)
        if (std::isalnum(c) || std::ispunct(c))
            valid = true;
        else if (c != ' ')
            return false;

    return valid;
}

bool isValidUTF8(const std::string &testThis)
{
    int pos = 0;
    const char *data = testThis.c_str();

    // https://helloacm.com/how-to-validate-utf-8-encoding-the-simple-utf-8-validation-algorithm/
    //
    // Valid UTF8 has a specific binary format. If it's a single byte UTF8 character, then it is
    // always of form '0xxxxxxx', where 'x' is any binary digit. If it's a two byte UTF8 character,
    // then it's always of form '110xxxxx10xxxxxx'. Similarly for three and four byte UTF8
    // characters it starts with '1110xxxx' and '11110xxx' followed by '10xxxxxx' one less times as
    // there are bytes. This tool will locate mistakes in the encoding and tell you where they
    // occurred.

    auto is10x = [](int a) {
        int bit1 = (a >> 7) & 1;
        int bit2 = (a >> 6) & 1;
        return (bit1 == 1) && (bit2 == 0);
    };
    size_t dsz = testThis.size();
    for (int i = 0; i < dsz; ++i)
    {
        // 0xxxxxxx
        int bit1 = (data[i] >> 7) & 1;
        if (bit1 == 0)
            continue;
        // 110xxxxx 10xxxxxx
        int bit2 = (data[i] >> 6) & 1;
        if (bit2 == 0)
            return false;
        // 11
        int bit3 = (data[i] >> 5) & 1;
        if (bit3 == 0)
        {
            // 110xxxxx 10xxxxxx
            if ((++i) < dsz)
            {
                if (is10x(data[i]))
                {
                    continue;
                }
                return false;
            }
            else
            {
                return false;
            }
        }
        int bit4 = (data[i] >> 4) & 1;
        if (bit4 == 0)
        {
            // 1110xxxx 10xxxxxx 10xxxxxx
            if (i + 2 < dsz)
            {
                if (is10x(data[i + 1]) && is10x(data[i + 2]))
                {
                    i += 2;
                    continue;
                }
                return false;
            }
            else
            {
                return false;
            }
        }
        int bit5 = (data[i] >> 3) & 1;
        if (bit5 == 1)
            return false;
        if (i + 3 < dsz)
        {
            if (is10x(data[i + 1]) && is10x(data[i + 2]) && is10x(data[i + 3]))
            {
                i += 3;
                continue;
            }
            return false;
        }
        else
        {
            return false;
        }
    }
    return true;
}

string findReplaceSubstring(string &source, const string &from, const string &to)
{
    string newString;
    // avoids a few memory allocations
    newString.reserve(source.length());

    string::size_type lastPos = 0;
    string::size_type findPos;

    while (string::npos != (findPos = source.find(from, lastPos)))
    {
        newString.append(source, lastPos, findPos - lastPos);
        newString += to;
        lastPos = findPos + from.length();
    }

    // care for the rest after last occurrence
    newString += source.substr(lastPos);

    source.swap(newString);

    return newString;
}

// BASE 64 SUPPORT, THANKS TO:
// https://renenyffenegger.ch/notes/development/Base64/Encoding-and-decoding-base-64-with-cpp
static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                        "abcdefghijklmnopqrstuvwxyz"
                                        "0123456789+/";

bool is_base64(unsigned char c) { return (isalnum(c) || (c == '+') || (c == '/')); }

std::string base64_encode(unsigned char const *bytes_to_encode, unsigned int in_len)
{
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (in_len--)
    {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3)
        {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];

        while ((i++ < 3))
            ret += '=';
    }

    return ret;
}

std::string base64_decode(std::string const &encoded_string)
{
    int in_len = encoded_string.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;

    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_]))
    {
        char_array_4[i++] = encoded_string[in_];
        in_++;
        if (i == 4)
        {
            for (i = 0; i < 4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
                ret += char_array_3[i];
            i = 0;
        }
    }

    if (i)
    {
        for (j = 0; j < i; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);

        for (j = 0; (j < i - 1); j++)
            ret += char_array_3[j];
    }

    return ret;
}

int getValueDisplayPrecision(SurgeStorage *storage)
{
    return getValueDisplayIsHighPrecision(storage) ? 6 : 2;
}

bool getValueDisplayIsHighPrecision(SurgeStorage *storage)
{
    bool isHigh{false};
    if (storage)
    {
        isHigh = getUserDefaultValue(storage, HighPrecisionReadouts, false);
    }
    return isHigh;
    ;
}

} // namespace Storage
} // namespace Surge
