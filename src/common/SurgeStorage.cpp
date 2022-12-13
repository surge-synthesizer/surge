/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "DSPUtils.h"
#include "SurgeStorage.h"
#include <set>
#include <numeric>
#include <cctype>
#include <map>
#include <queue>
#include <vembertech/vt_dsp_endian.h>
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
#endif
#include "FxPresetAndClipboardManager.h"
#include "ModulatorPresetManager.h"
#include "SurgeMemoryPools.h"

// FIXME probably remove this when we remove the hardcoded hack below
#include "MSEGModulationHelper.h"
// FIXME
#include "FormulaModulationHelper.h"

using namespace std;

std::string SurgeStorage::skipPatchLoadDataPathSentinel = "<SKIP-PATCH-SENTINEL>";

SurgeStorage::SurgeStorage(const SurgeStorage::SurgeStorageConfig &config) : otherscene_clients(0)
{
    auto suppliedDataPath = config.suppliedDataPath;
    bool loadWtAndPatch = true;
    loadWtAndPatch = !skipLoadWtAndPatch && suppliedDataPath != skipPatchLoadDataPathSentinel;

    if (suppliedDataPath == skipPatchLoadDataPathSentinel)
        suppliedDataPath = "";

    if (samplerate == 0)
    {
        setSamplerate(48000);
    }
    else if (samplerate < 12000 || samplerate > 48000 * 32)
    {
        std::ostringstream oss;
        oss << "Warning: SurgeStorage constructed with invalid samplerate :" << samplerate
            << " - resetting to 48000 until Audio System tells us otherwise" << std::endl;
        reportError(oss.str(), "SampleRate Corrputed on Startup");
        setSamplerate(48000);
    }
    else
    {
        // Just in case, make sure we are completely consistent in all tables etc
        setSamplerate(samplerate);
    }

    _patch.reset(new SurgePatch(this));

    float cutoff = 0.455f;
    float cutoff1X = 0.85f;
    float cutoffI16 = 1.0f;
    int j;
    for (j = 0; j < FIRipol_M + 1; j++)
    {
        for (int i = 0; i < FIRipol_N; i++)
        {
            double t = -double(i) + double(FIRipol_N / 2.0) + double(j) / double(FIRipol_M) - 1.0;
            double val = (float)(symmetric_blackman(t, FIRipol_N) * cutoff * sincf(cutoff * t));
            double val1X =
                (float)(symmetric_blackman(t, FIRipol_N) * cutoff1X * sincf(cutoff1X * t));
            sinctable[j * FIRipol_N * 2 + i] = (float)val;
            sinctable1X[j * FIRipol_N + i] = (float)val1X;
        }
    }
    for (j = 0; j < FIRipol_M; j++)
    {
        for (int i = 0; i < FIRipol_N; i++)
        {
            sinctable[j * FIRipol_N * 2 + FIRipol_N + i] =
                (float)((sinctable[(j + 1) * FIRipol_N * 2 + i] -
                         sinctable[j * FIRipol_N * 2 + i]) /
                        65536.0);
        }
    }

    for (j = 0; j < FIRipol_M + 1; j++)
    {
        for (int i = 0; i < FIRipolI16_N; i++)
        {
            double t =
                -double(i) + double(FIRipolI16_N / 2.0) + double(j) / double(FIRipol_M) - 1.0;
            double val =
                (float)(symmetric_blackman(t, FIRipolI16_N) * cutoffI16 * sincf(cutoffI16 * t));

            sinctableI16[j * FIRipolI16_N + i] = (short)((float)val * 16384.f);
        }
    }

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
#elif LINUX
    if (!hasSuppliedDataPath)
    {
        auto userlower = sst::plugininfra::paths::bestLibrarySharedFolderPathFor(sxtlower, true);
        auto userreg = sst::plugininfra::paths::bestLibrarySharedFolderPathFor(sxt, true);
        auto globallower = sst::plugininfra::paths::bestLibrarySharedFolderPathFor(sxtlower, false);
        auto globalreg = sst::plugininfra::paths::bestLibrarySharedFolderPathFor(sxt, true);

        bool founddir{false};
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
    else
    {
        datapath = suppliedDataPath;
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
        userDataPath = sst::plugininfra::paths::bestDocumentsFolderPathFor(sxt);
    }
#endif

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
    userWavetablesPath = userDataPath / "Wavetables";
    userWavetablesExportPath = userWavetablesPath / "Exported";
    userFXPath = userDataPath / "FX Presets";
    userMidiMappingsPath = userDataPath / "MIDI Mappings";
    userModulatorSettingsPath = userDataPath / "Modulator Presets";
    userSkinsPath = userDataPath / "Skins";
    extraThirdPartyWavetablesPath = config.extraThirdPartyWavetablesPath;

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
        if (!load_wt_wt(path_to_string(datapath / "windows.wt"), &WindowWT))
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
    bool mtsMode = Surge::Storage::getUserDefaultValue(this, Surge::Storage::UseODDMTS, false);
    if (mtsMode)
    {
        initialize_oddsound();
    }
    else
    {
        oddsound_mts_client = nullptr;
        oddsound_mts_active = false;
    }
#endif

    initPatchName =
        Surge::Storage::getUserDefaultValue(this, Surge::Storage::InitialPatchName, "Init Saw");
    initPatchCategory = Surge::Storage::getUserDefaultValue(
        this, Surge::Storage::InitialPatchCategory, "Templates");
    initPatchCategoryType = Surge::Storage::getUserDefaultValue(
        this, Surge::Storage::InitialPatchCategoryType, "Factory");

    fxUserPreset = std::make_unique<Surge::Storage::FxUserPreset>();
    fxUserPreset->doPresetRescan(this);

    modulatorPreset = std::make_unique<Surge::Storage::ModulatorPreset>();
    modulatorPreset->forcePresetRescan();

    memoryPools = std::make_unique<Surge::Memory::SurgeMemoryPools>(this);
}

void SurgeStorage::createUserDirectory()
{
    auto p = userDataPath;
    auto needToBuild = false;
    if (!fs::is_directory(p))
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
            reportError(e.what(), "Unable to set up User Directory");
        }
    }
}

void SurgeStorage::initializePatchDb(bool force)
{
    if (patchDBInitialized && !force)
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
        auto qtime = fs::last_write_time(p.path);
        p.lastModTime =
            std::chrono::duration_cast<std::chrono::seconds>(qtime.time_since_epoch()).count();

        auto ps = p.path.u8string();
        auto pf = pathToTrunc(ps);

        if (favSet.find(ps) != favSet.end())
            p.isFavorite = true;
        else if (!pf.empty() && (favTruncSet.find(pf) != favTruncSet.end()))
            p.isFavorite = true;
        else
            p.isFavorite = false;
    }
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

            c.numberOfPatchesInCatgory = 0;
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

                    c.numberOfPatchesInCatgory++;
                }
            }

            c.numberOfPatchesInCategoryAndChildren = c.numberOfPatchesInCatgory;
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
            c.numberOfPatchesInCatgory;
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
    refresh_wtlistAddDir(false, "wavetables_3rdparty");
    if (!extraThirdPartyWavetablesPath.empty())
    {
        refresh_wtlistFrom(false, extraThirdPartyWavetablesPath, "wavetables_3rdparty");
    }
    firstUserWTCategory = wt_category.size();
    refresh_wtlistAddDir(true, "Wavetables");

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
                patch.scene[sc].osc[o].wt.refresh_display = true;
            }
            else if (patch.scene[sc].osc[o].wt.queue_filename[0])
            {
                if (!(patch.scene[sc].osc[o].type.val.i == ot_wavetable ||
                      patch.scene[sc].osc[o].type.val.i == ot_window))
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
            strncpy(osc->wavetable_display_name, "Sin to Saw", TXT_SIZE);

        return;
    }
    if (id < 0)
        return;
    if (id >= wt_list.size())
        return;
    if (!wt)
        return;

    load_wt(path_to_string(wt_list[id].path), wt, osc);

    if (osc)
    {
        auto n = wt_list.at(id).name;
        strncpy(osc->wavetable_display_name, n.c_str(), 256);
    }
}

void SurgeStorage::load_wt(string filename, Wavetable *wt, OscillatorStorage *osc)
{
    strncpy(wt->current_filename, wt->queue_filename, 256);
    wt->queue_filename[0] = 0;
    string extension = filename.substr(filename.find_last_of('.'), filename.npos);
    for (unsigned int i = 0; i < extension.length(); i++)
        extension[i] = tolower(extension[i]);
    bool loaded = false;
    if (extension.compare(".wt") == 0)
        loaded = load_wt_wt(filename, wt);
    else if (extension.compare(".wav") == 0)
        loaded = load_wt_wav_portable(filename, wt);
    else
    {
        std::ostringstream oss;
        oss << "Unable to load file with extension " << extension
            << "! Surge only supports .wav and .wt wavetable files!";
        reportError(oss.str(), "Error");
    }

    if (osc && loaded)
    {
        char sep = PATH_SEPARATOR;
        auto fn = filename.substr(filename.find_last_of(sep) + 1, filename.npos);
        std::string fnnoext = fn.substr(0, fn.find_last_of('.'));

        if (fnnoext.length() > 0)
        {
            strncpy(osc->wavetable_display_name, fnnoext.c_str(), 256);
        }
    }
}

bool SurgeStorage::load_wt_wt(string filename, Wavetable *wt)
{
    std::filebuf f;
    if (!f.open(string_to_path(filename), std::ios::binary | std::ios::in))
        return false;
    wt_header wh;
    memset(&wh, 0, sizeof(wt_header));

    size_t read = f.sgetn(reinterpret_cast<char *>(&wh), sizeof(wh));

    if (!(wh.tag[0] == 'v' && wh.tag[1] == 'a' && wh.tag[2] == 'w' && wh.tag[3] == 't'))
    {
        // SOME sort of error reporting is appropriate
        return false;
    }

    size_t ds;
    if (vt_read_int16LE(wh.flags) & wtf_int16)
        ds = sizeof(short) * vt_read_int16LE(wh.n_tables) * vt_read_int32LE(wh.n_samples);
    else
        ds = sizeof(float) * vt_read_int16LE(wh.n_tables) * vt_read_int32LE(wh.n_samples);

    const std::unique_ptr<char[]> data{new char[ds]};
    read = f.sgetn(data.get(), ds);
    // FIXME - error if read != ds

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
            << "It is recommended to restart Surge and not load "
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
    if (vt_read_int16LE(wh.flags) & wtf_int16)
        ds = sizeof(short) * vt_read_int16LE(wh.n_tables) * vt_read_int32LE(wh.n_samples);
    else
        ds = sizeof(float) * vt_read_int16LE(wh.n_tables) * vt_read_int32LE(wh.n_samples);

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
            << "It is recommended to restart Surge and not load "
               "the problematic wavetable again.\n\n"
            << " If you would like, please attach the wavetable which caused this error to a new "
               "GitHub issue at "
            << stringRepository;
        reportError(oss.str(), "Wavetable Loading Error");
    }
    return wasBuilt;
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
            strncpy(clipboard_wt_names[0],
                    getPatch().scene[scene].osc[entry].wavetable_display_name, 256);
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
            strncpy(clipboard_wt_names[i], getPatch().scene[scene].osc[i].wavetable_display_name,
                    256);
            clipboard_extraconfig[i] = getPatch().scene[scene].osc[i].extraConfig;
        }

        clipboard_primode = getPatch().scene[scene].monoVoicePriorityMode;
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

void SurgeStorage::clipboard_paste(int type, int scene, int entry, modsources ms,
                                   std::function<bool(int, modsources)> isValid)
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
            strncpy(getPatch().scene[scene].osc[i].wavetable_display_name, clipboard_wt_names[i],
                    256);
        }

        getPatch().scene[scene].monoVoicePriorityMode = clipboard_primode;
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
                strncpy(getPatch().scene[scene].osc[entry].wavetable_display_name,
                        clipboard_wt_names[0], 256);
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
                m.depth = clipboard_modulation_scene[i].depth;
                m.destination_id = clipboard_modulation_scene[i].destination_id;
                pushBackOrOverride(getPatch().scene[scene].modulation_scene, m);
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

    int n = n_global_params + n_scene_params; // only store midictrl's for scene A (scene A -> scene
                                              // B will be duplicated on load)
    for (int i = 0; i < n; i++)
    {
        if (getPatch().param_ptr[i]->midictrl >= 0)
        {
            TiXmlElement mc_e("entry");
            mc_e.SetAttribute("p", i);
            mc_e.SetAttribute("ctrl", getPatch().param_ptr[i]->midictrl);
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
    // save_snapshots();
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
                return q;
        }
        return getSnapshotSection(n);
    };

    TiXmlElement *mc = get("midictrl");
    assert(mc);

    TiXmlElement *entry = TINYXML_SAFE_TO_ELEMENT(mc->FirstChild("entry"));
    while (entry)
    {
        int id, ctrl;
        if ((entry->QueryIntAttribute("p", &id) == TIXML_SUCCESS) &&
            (entry->QueryIntAttribute("ctrl", &ctrl) == TIXML_SUCCESS))
        {
            getPatch().param_ptr[id]->midictrl = ctrl;
            if (id >= n_global_params)
                getPatch().param_ptr[id + n_scene_params]->midictrl = ctrl;
        }
        entry = TINYXML_SAFE_TO_ELEMENT(entry->NextSibling("entry"));
    }

    TiXmlElement *cc = get("customctrl");
    assert(cc);

    entry = TINYXML_SAFE_TO_ELEMENT(cc->FirstChild("entry"));
    while (entry)
    {
        int id, ctrl;
        if ((entry->QueryIntAttribute("p", &id) == TIXML_SUCCESS) &&
            (entry->QueryIntAttribute("ctrl", &ctrl) == TIXML_SUCCESS) &&
            (id < n_customcontrollers))
        {
            controllers[id] = ctrl;
        }
        entry = TINYXML_SAFE_TO_ELEMENT(entry->NextSibling("entry"));
    }
}

SurgeStorage::~SurgeStorage()
{
#ifndef SURGE_SKIP_ODDSOUND_MTS
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
    return true;
}

void SurgeStorage::setTuningApplicationMode(const TuningApplicationMode m)
{
    tuningApplicationMode = m;
    patchStoredTuningApplicationMode = m;
    resetToCurrentScaleAndMapping();
    if (oddsound_mts_active)
    {
        tuningApplicationMode = RETUNE_MIDI_ONLY;
    }
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

    // We can do revision stuff here later if we need to
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
            int i, c;
            if (map->QueryIntAttribute("p", &i) == TIXML_SUCCESS &&
                map->QueryIntAttribute("cc", &c) == TIXML_SUCCESS)
            {
                getPatch().param_ptr[i]->midictrl = c;
                if (i >= n_global_params)
                {
                    getPatch().param_ptr[i + n_scene_params]->midictrl = c;
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
            int i, cc;
            if (ctrl->QueryIntAttribute("i", &i) == TIXML_SUCCESS &&
                ctrl->QueryIntAttribute("cc", &cc) == TIXML_SUCCESS)
            {
                controllers[i] = cc;
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

    // only store midictrl's for scene A (scene A -> scene B will be duplicated on load)
    int n = n_global_params + n_scene_params;

    TiXmlElement mc("midictrl");
    for (int i = 0; i < n; i++)
    {
        if (getPatch().param_ptr[i]->midictrl >= 0)
        {
            TiXmlElement p("map");
            p.SetAttribute("p", i);
            p.SetAttribute("cc", getPatch().param_ptr[i]->midictrl);
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
    bool poa = oddsound_mts_active;
    oddsound_mts_active = b;
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

void SurgeStorage::reportError(const std::string &msg, const std::string &title)
{
    std::cout << "Surge Error [" << title << "]\n" << msg << std::endl;
    if (errorListeners.empty())
    {
        std::lock_guard<std::mutex> g(preListenerErrorMutex);
        preListenerErrors.emplace_back(msg, title);
    }

    for (auto l : errorListeners)
        l->onSurgeError(msg, title);
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

} // namespace Storage
} // namespace Surge
