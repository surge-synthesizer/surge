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

#include "SurgeSynthesizer.h"
#include "DSPUtils.h"
#include <time.h>

#include "filesystem/import.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include "SurgeMemoryPools.h"

#include "sst/basic-blocks/mechanics/endian-ops.h"
#include "PatchFileHeaderStructs.h"

namespace mech = sst::basic_blocks::mechanics;

using namespace std;

void SurgeSynthesizer::jogPatch(bool increment, bool insideCategory)
{
    // Don't increment if we still have an outstanding load
    if (patchid_queue >= 0)
        return;
    int p = storage.patch_list.size();

    if (!p)
    {
        return;
    }

    /*
    ** Ideally we would never call this with an out
    ** of range patchid, but the init case where we
    ** have a non-loaded in memory patch proves that
    ** false, as may some other cases. So add this
    ** defensive approach. See #319
    */
    if (patchid < 0 || patchid > p - 1)
    {
        // Find patch 0 category 0 and select it
        int ccid = storage.patchCategoryOrdering[0];

        int target = p + 1;
        for (auto &patch : storage.patch_list)
        {
            if (patch.category == ccid && patch.order < target)
                target = patch.order;
        }

        patchid_queue = storage.patchOrdering[target];
        current_category_id = ccid;
    }
    else
    {
        int order = storage.patch_list[patchid].order;
        int category = storage.patch_list[patchid].category;
        int currentPatchId;
        vector<int> patchesInCategory;

        // get all patches belonging to current category
        for (auto i : storage.patchOrdering)
        {
            if (storage.patch_list[i].category == category)
            {
                patchesInCategory.push_back(i);
                if (storage.patch_list[i].order == order)
                {
                    currentPatchId = i;
                }
            }
        }

        int np = increment == true ? 1 : -1;
        int numPatches = patchesInCategory.size();

        do
        {
            if (increment)
            {
                if (!insideCategory && order >= p - 1)
                {
                    jogCategory(increment);
                    category = current_category_id;
                }
                order = (order >= p - 1) ? 0 : order + 1;
            }
            else
            {
                if (!insideCategory && order <= 0)
                {
                    jogCategory(increment);
                    category = current_category_id;
                }
                order = (order <= 0) ? p - 1 : order - 1;
            }
        } while (storage.patch_list[storage.patchOrdering[order]].category != category);

        patchid_queue = storage.patchOrdering[order];
    }
    processAudioThreadOpsWhenAudioEngineUnavailable();
    return;
}

void SurgeSynthesizer::jogCategory(bool increment)
{
    int c = storage.patch_category.size();

    if (!c)
        return;

    // See comment above and #319
    if (current_category_id < 0 || current_category_id > c - 1)
    {
        current_category_id = storage.patchCategoryOrdering[0];
    }
    else
    {
        int order = storage.patch_category[current_category_id].order;
        int orderOrig = order;
        do
        {
            if (increment)
                order = (order >= (c - 1)) ? 0 : order + 1;
            else
                order = (order <= 0) ? c - 1 : order - 1;

            current_category_id = storage.patchCategoryOrdering[order];
        } while (storage.patch_category[current_category_id].numberOfPatchesInCategory == 0 &&
                 order != orderOrig);
        // That order != orderOrig isn't needed unless we have an entire empty category tree, in
        // which case it stops an inf loop
    }

    // Find the first patch within the category.
    for (auto p : storage.patchOrdering)
    {
        if (storage.patch_list[p].category == current_category_id)
        {
            patchid_queue = p;
            processAudioThreadOpsWhenAudioEngineUnavailable();
            return;
        }
    }
}

void SurgeSynthesizer::jogPatchOrCategory(bool increment, bool isCategory, bool insideCategory)
{
    if (isCategory)
    {
        jogCategory(increment);
    }
    else
    {
        jogPatch(increment, insideCategory);
    }
}

void SurgeSynthesizer::selectRandomPatch()
{
    if (patchid_queue >= 0)
        return;
    int p = storage.patch_list.size();

    if (!p)
    {
        return;
    }

    auto r = storage.rand_u32() % p;
    patchid_queue = r;
}

void SurgeSynthesizer::loadPatch(int id)
{
    if (id < 0)
        id = 0;
    if (id >= storage.patch_list.size())
        id = id % storage.patch_list.size();

    patchid = id;

    Patch e = storage.patch_list[id];
    loadPatchByPath(path_to_string(e.path).c_str(), e.category, e.name.c_str());
    storage.getPatch().isDirty = false;
}

bool SurgeSynthesizer::loadPatchByPath(const char *fxpPath, int categoryId, const char *patchName,
                                       bool forceIsPreset)
{
    storage.getPatch().dawExtraState.editor.clearAllFormulaStates();
    storage.getPatch().dawExtraState.editor.clearAllWTEStates();
    storage.getPatch().dawExtraState.editor.clearAllModulationSourceButtonStates();

    using namespace sst::io;

    std::filebuf f;
    if (!f.open(string_to_path(fxpPath), std::ios::binary | std::ios::in))
    {
        storage.reportError(std::string() + "Unable to open file " + std::string(fxpPath),
                            "Unable to open file");
        return false;
    }
    fxChunkSetCustom fxp;
    auto read = f.sgetn(reinterpret_cast<char *>(&fxp), sizeof(fxp));
    // FIXME - error if read != chunk size
    if ((mech::endian_read_int32BE(fxp.chunkMagic) != 'CcnK') ||
        (mech::endian_read_int32BE(fxp.fxMagic) != 'FPCh') ||
        (mech::endian_read_int32BE(fxp.fxID) != 'cjs3'))
    {
        f.close();
        auto cm = mech::endian_read_int32BE(fxp.chunkMagic);
        auto fm = mech::endian_read_int32BE(fxp.fxMagic);
        auto id = mech::endian_read_int32BE(fxp.fxID);

        std::ostringstream oss;
        oss << "Unable to load " << patchName << ".fxp!";
        // if( cm != 'CcnK' )
        //{
        //   oss << "ChunkMagic is not 'CcnK'. ";
        //}
        // if( fm != 'FPCh' )
        //{
        //   oss << "FxMagic is not 'FPCh'. ";
        //}
        // if( id != 'cjs3' )
        //{
        //   union {
        //      char c[4];
        //      int id;
        //   } q;
        //   q.id = id;
        //   oss << "Synth ID is '" << q.c[0] << q.c[1] << q.c[2] << q.c[3] << "'; Surge expected
        //   'cjs3'. ";
        //}
        oss << "This error usually occurs when you attempt to load an .fxp that belongs to another "
               "plugin into Surge XT.";
        storage.reportError(oss.str(), "Unknown FXP File");
        return false;
    }

    int cs = mech::endian_read_int32BE(fxp.chunkSize);
    std::unique_ptr<char[]> data{new char[cs]};

    if (f.sgetn(data.get(), cs) != cs)
    {
        perror("Error while loading patch!");
    }

    f.close();

    storage.getPatch().comment = "";
    storage.getPatch().author = "";

    if (categoryId >= 0)
    {
        storage.getPatch().category = storage.patch_category[categoryId].name;
    }
    else
    {
        storage.getPatch().category = "Drag & Drop";
    }

    current_category_id = categoryId;
    storage.getPatch().name = patchName;

    loadRaw(data.get(), cs, forceIsPreset);
    data.reset();

    // OK so at this point we may have loaded a patch with a tuning override
    if (storage.getPatch().patchTuning.tuningStoredInPatch)
    {
        // This code changed radically at the end of the XT 1.0 cycle. Previously we would
        // check and prompt. Now the semantic is:
        //
        // 1. If the patch has a tuning, always parse it onto the patchStreamedTuning member
        //    (which we reset below if the patch doesn't have it)
        // 2. based on the tuning setting, override the tuning if there's a patch tuning
        bool ot = Surge::Storage::getUserDefaultValue(
            &storage, Surge::Storage::OverrideTuningOnPatchLoad, false);
        bool om = Surge::Storage::getUserDefaultValue(
            &storage, Surge::Storage::OverrideMappingOnPatchLoad, false);

        if (ot || om)
        {
            try
            {
                if (ot)
                {
                    if (storage.getPatch().patchTuning.scaleContents.size() > 1)
                    {
                        storage.retuneToScale(
                            Tunings::parseSCLData(storage.getPatch().patchTuning.scaleContents));
                    }
                    else
                    {
                        storage.retuneTo12TETScale();
                    }
                }

                if (om)
                {
                    if (storage.getPatch().patchTuning.mappingContents.size() > 1)
                    {
                        auto kb =
                            Tunings::parseKBMData(storage.getPatch().patchTuning.mappingContents);
                        if (storage.getPatch().patchTuning.mappingName.size() > 1)
                            kb.name = storage.getPatch().patchTuning.mappingName;
                        else
                            kb.name = storage.guessAtKBMName(kb);
                        storage.remapToKeyboard(kb);
                    }
                    else
                    {
                        storage.remapToConcertCKeyboard();
                    }
                }
            }
            catch (Tunings::TuningError &e)
            {
                storage.reportError(e.what(), "Error applying tuning!");
                storage.retuneTo12TETScaleC261Mapping();
            }
        }

        try
        {
            auto s = Tunings::evenTemperament12NoteScale();
            auto m = Tunings::KeyboardMapping();
            if (storage.getPatch().patchTuning.scaleContents.size() > 1)
            {
                s = Tunings::parseSCLData(storage.getPatch().patchTuning.scaleContents);
            }
            if (storage.getPatch().patchTuning.mappingContents.size() > 1)
            {
                m = Tunings::parseKBMData(storage.getPatch().patchTuning.mappingContents);
            }
            storage.patchStoredTuning = Tunings::Tuning(s, m);
            storage.hasPatchStoredTuning = true;
        }
        catch (Tunings::TuningError &e)
        {
            storage.hasPatchStoredTuning = false;
            storage.patchStoredTuning = Tunings::Tuning();
        }
    }
    else
    {
        storage.hasPatchStoredTuning = false;
        storage.patchStoredTuning = Tunings::Tuning();
    }

    masterfade = 1.f;

    // Notify the host display that the patch name has changed
    storage.getPatch().isDirty = false;
    updateDisplay();

    return true;
}

void SurgeSynthesizer::enqueuePatchForLoad(const void *data, int size)
{
    {
        // If we are forcing values on, we don't want to do any enqueued loads
        // or want to wait for them to complete
        std::lock_guard<std::mutex> mg(patchLoadSpawnMutex);
        has_patchid_file = false;
        patchid_queue = -1;
    }
    {
        std::lock_guard<std::mutex> g(rawLoadQueueMutex);

        enqueuedLoadData.reset(new char[size]);
        memcpy(enqueuedLoadData.get(), data, size);
        enqueuedLoadSize = size;
        rawLoadEnqueued = true;
        rawLoadNeedsUIDawExtraState = false;
    }
}

void SurgeSynthesizer::processEnqueuedPatchIfNeeded()
{
    bool expected = true;
    if (rawLoadEnqueued.compare_exchange_weak(expected, true) && expected)
    {
        {
            // If we are forcing values on, we don't want to do any enqueued loads
            // or want to wait for them to complete
            std::lock_guard<std::mutex> mg(patchLoadSpawnMutex);
            has_patchid_file = false;
            patchid_queue = -1;
        }
        std::lock_guard<std::mutex> g(rawLoadQueueMutex);
        rawLoadEnqueued = false;
        loadRaw(enqueuedLoadData.get(), enqueuedLoadSize);
        loadFromDawExtraState();

        rawLoadNeedsUIDawExtraState = true;
        refresh_editor = true;
    }
}

void SurgeSynthesizer::loadRaw(const void *data, int size, bool preset)
{
    halt_engine = true;
    stopSound();
    for (int s = 0; s < n_scenes; s++)
        for (int i = 0; i < n_customcontrollers; i++)
            storage.getPatch().scene[s].modsources[ms_ctrl1 + i]->reset();

    storage.getPatch().init_default_values();
    storage.getPatch().load_patch(data, size, preset);
    storage.getPatch().update_controls(false, nullptr, true);
    for (int i = 0; i < n_fx_slots; i++)
    {
        fxsync[i] = storage.getPatch().fx[i];
        fx_reload[i] = true;
    }

    loadFx(false, true);

    for (int sc = 0; sc < n_scenes; sc++)
    {
        setParameter01(storage.getPatch().scene[sc].f2_cutoff_is_offset.id,
                       storage.getPatch().scene[sc].f2_cutoff_is_offset.get_value_f01());
    }

    storage.memoryPools->resetAllPools(&storage);

    for (int sc = 0; sc < n_scenes; ++sc)
    {
        for (int m = 0; m < n_lfos; ++m)
        {
            Surge::Formula::removeFunctionsAssociatedWith(&storage,
                                                          &(storage.getPatch().formulamods[sc][m]));
        }
    }

    storage.getPatch().isDirty = false;

    halt_engine = false;
    patch_loaded = true;
    refresh_editor = true;

    // New patch was just loaded so I look up and set the current category and patch.
    // This is used to draw checkmarks in the menu. If for some reason we don't find one,
    // nothing will break
    std::vector<int> inferredPatchIds;
    int cnt = storage.patch_list.size();
    string name = storage.getPatch().name;
    string cat = storage.getPatch().category;

    for (int p = 0; p < cnt; ++p)
    {
        if (storage.patch_list[p].name == name &&
            storage.patch_category[storage.patch_list[p].category].name == cat)
        {
            current_category_id = storage.patch_list[p].category;
            inferredPatchIds.push_back(p);
        }
    }

    if (!inferredPatchIds.empty())
    {
        bool foundOne{false};
        int nextPatchId = patchid;
        for (auto inferredPatchId : inferredPatchIds)
        {
            // If the patchid is out of range or if it is the default overrule
            if (patchid < 0 && patchid >= storage.patch_list.size())
            {
                nextPatchId = inferredPatchId;
                foundOne = true;
            }
            else if (storage.patch_list[patchid].name == storage.initPatchName &&
                     storage.patch_category[storage.patch_list[patchid].category].name ==
                         storage.initPatchCategory)
            {
                nextPatchId = inferredPatchId;
                foundOne = true;
            }
            else if (patchid == inferredPatchId)
            {
                foundOne = true;
            }
        }
        if (foundOne)
        {
            patchid = nextPatchId;
        }
        else
        {
            // I don't see how this could ever happen. Punt.
            patchid = inferredPatchIds.back();
        }
    }
}

#if MAC || LINUX
#include <sys/types.h>
#include <sys/stat.h>
#endif

void SurgeSynthesizer::savePatch(bool factoryInPlace, bool skipOverwrite)
{
    if (storage.getPatch().category.empty())
    {
        storage.getPatch().category = "Default";
    }

    fs::path savepath = storage.userPatchesPath;

    if (factoryInPlace && patchid >= 0 && patchid < storage.patch_list.size())
    {
        auto fpath = storage.patch_list[patchid].path;
        savePatchToPath(fpath);
        return;
    }

    try
    {
        std::string tempCat = storage.getPatch().category;

#if WINDOWS
        if (tempCat[0] == '\\' || tempCat[0] == '/')
        {
            tempCat.erase(0, 1);
        }
#endif

        fs::path catPath = (string_to_path(tempCat));

        if (!catPath.is_relative())
        {
            storage.reportError(
                "Please use relative paths when saving patches. Referring to drive names directly "
                "and using absolute paths is not allowed!",
                "Error");
            return;
        }

        auto comppath = savepath;
        savepath /= catPath;

        comppath = comppath.lexically_normal();
        savepath = savepath.lexically_normal();

        // make sure your category isnt "../../../etc/config"

        auto [_, compIt] =
            std::mismatch(savepath.begin(), savepath.end(), comppath.begin(), comppath.end());
        if (compIt != comppath.end())
        {
            storage.reportError(
                "Your save path is not a directory below the user patches directory. "
                "This usually means you are doing something like trying to use too many ../"
                " in your category name.",
                "Save Path not below user path");
            return;
        }

        create_directories(savepath);
    }
    catch (...)
    {
        storage.reportError(
            "Exception occurred while creating category folder! Most likely, invalid characters "
            "were used to name the category. Please remove suspicious characters and try again!",
            "Error");
        return;
    }

    fs::path filename = savepath;
    filename /= string_to_path(storage.getPatch().name + ".fxp");

    try
    {
        if (fs::exists(filename) && !skipOverwrite)
        {
            storage.okCancelProvider(std::string("The patch '" + storage.getPatch().name +
                                                 "' already exists in '" +
                                                 storage.getPatch().category +
                                                 "'. Are you sure you want to overwrite it?"),
                                     std::string("Overwrite Patch"), SurgeStorage::OK,
                                     [filename, this](SurgeStorage::OkCancel okc) {
                                         if (okc == SurgeStorage::OK)
                                         {
                                             savePatchToPath(filename);
                                         }
                                     });
        }
        else
        {
            savePatchToPath(filename);
        }
    }
    catch (...)
    {
        storage.reportError("Exception occurred while attempting to write the patch! Most likely, "
                            "invalid characters or a reserved name was used to name the patch. "
                            "Please try again with a different name!",
                            "Error");
        return;
    }

    storage.getPatch().isDirty = false;
}

void SurgeSynthesizer::savePatchToPath(fs::path filename, bool refreshPatchList)
{
    using namespace sst::io;

    std::ofstream f(filename, std::ios::out | std::ios::binary);

    if (!f)
    {
        storage.reportError(
            "Unable to save the patch to the specified path! Maybe it contains invalid characters?",
            "Error");
        return;
    }

    fxChunkSetCustom fxp;
    fxp.chunkMagic = mech::endian_write_int32BE('CcnK');
    fxp.fxMagic = mech::endian_write_int32BE('FPCh');
    fxp.fxID = mech::endian_write_int32BE('cjs3');
    fxp.numPrograms = mech::endian_write_int32BE(1);
    fxp.version = mech::endian_write_int32BE(1);
    fxp.fxVersion = mech::endian_write_int32BE(1);
    strncpy(fxp.prgName, storage.getPatch().name.c_str(), 28);

    void *data;
    unsigned int datasize = storage.getPatch().save_patch(&data);

    fxp.chunkSize = mech::endian_write_int32BE(datasize);
    fxp.byteSize = 0;

    f.write((char *)&fxp, sizeof(fxChunkSetCustom));
    f.write((char *)data, datasize);
    f.close();

    if (refreshPatchList)
    {
        // refresh list
        storage.refresh_patchlist();
        storage.initializePatchDb(true);

        int idx = 0;
        for (auto p : storage.patch_list)
        {
            if (p.path == filename)
            {
                patchid = idx;
                current_category_id = p.category;
            }
            idx++;
        }
        refresh_editor = true;
        midiprogramshavechanged = true;

        storage.getPatch().isDirty = false;
    }
}

unsigned int SurgeSynthesizer::saveRaw(void **data) { return storage.getPatch().save_patch(data); }
