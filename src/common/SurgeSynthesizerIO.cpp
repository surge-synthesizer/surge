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

#include "SurgeSynthesizer.h"
#include "DspUtilities.h"
#include <time.h>
#include <vt_dsp/vt_dsp_endian.h>

#include "filesystem/import.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include "UserInteractions.h"

#if TARGET_AUDIOUNIT
#include "aulayer.h"
#endif

#if AU
#include "aulayer.h"
#endif

using namespace std;

// seems to be missing from VST2.3, so it's copied from the VST list instead
//--------------------------------------------------------------------
// For Preset (Program) (.fxp) with chunk (magic = 'FPCh')
//--------------------------------------------------------------------
struct fxChunkSetCustom
{
    int chunkMagic; // 'CcnK'
    int byteSize;   // of this chunk, excl. magic + byteSize

    int fxMagic; // 'FPCh'
    int version;
    int fxID; // fx unique id
    int fxVersion;

    int numPrograms;
    char prgName[28];

    int chunkSize;
    // char chunk[8]; // variable
};

void SurgeSynthesizer::incrementPatch(bool nextPrev, bool insideCategory)
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

        int np = nextPrev == true ? 1 : -1;
        int numPatches = patchesInCategory.size();

        do
        {
            if (nextPrev)
            {
                if (!insideCategory && order >= p - 1)
                {
                    incrementCategory(nextPrev);
                    category = current_category_id;
                }
                order = (order >= p - 1) ? 0 : order + 1;
            }
            else
            {
                if (!insideCategory && order <= 0)
                {
                    incrementCategory(nextPrev);
                    category = current_category_id;
                }
                order = (order <= 0) ? p - 1 : order - 1;
            }
        } while (storage.patch_list[storage.patchOrdering[order]].category != category);

        patchid_queue = storage.patchOrdering[order];
    }
    processThreadunsafeOperations();
    return;
}

void SurgeSynthesizer::incrementCategory(bool nextPrev)
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
            if (nextPrev)
                order = (order >= (c - 1)) ? 0 : order + 1;
            else
                order = (order <= 0) ? c - 1 : order - 1;

            current_category_id = storage.patchCategoryOrdering[order];
        } while (storage.patch_category[current_category_id].numberOfPatchesInCatgory == 0 &&
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
            processThreadunsafeOperations();
            return;
        }
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
}

bool SurgeSynthesizer::loadPatchByPath(const char *fxpPath, int categoryId, const char *patchName)
{
    std::filebuf f;
    if (!f.open(string_to_path(fxpPath), std::ios::binary | std::ios::in))
        return false;
    fxChunkSetCustom fxp;
    auto read = f.sgetn(reinterpret_cast<char *>(&fxp), sizeof(fxp));
    // FIXME - error if read != chunk size
    if ((vt_read_int32BE(fxp.chunkMagic) != 'CcnK') || (vt_read_int32BE(fxp.fxMagic) != 'FPCh') ||
        (vt_read_int32BE(fxp.fxID) != 'cjs3'))
    {
        f.close();
        auto cm = vt_read_int32BE(fxp.chunkMagic);
        auto fm = vt_read_int32BE(fxp.fxMagic);
        auto id = vt_read_int32BE(fxp.fxID);

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
               "plugin into Surge.";
        Surge::UserInteractions::promptError(oss.str(), "Unknown FXP File");
        return false;
    }

    int cs = vt_read_int32BE(fxp.chunkSize);
    std::unique_ptr<char[]> data{new char[cs]};
    if (f.sgetn(data.get(), cs) != cs)
        perror("Error while loading patch!");
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

    loadRaw(data.get(), cs, true);
    data.reset();

    /*
    ** OK so at this point we may have loaded a patch with a tuning override
    */
    if (storage.getPatch().patchTuning.tuningStoredInPatch)
    {
        if (storage.isStandardTuning)
        {
            try
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
                if (storage.getPatch().patchTuning.mappingContents.size() > 1)
                {
                    auto kb = Tunings::parseKBMData(storage.getPatch().patchTuning.mappingContents);
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
            catch (Tunings::TuningError &e)
            {
                Surge::UserInteractions::promptError(e.what(), "Error restoring tuning!");
                storage.retuneTo12TETScaleC261Mapping();
            }
        }
        else
        {
            auto okc = Surge::UserInteractions::promptOKCancel(
                std::string("Loaded patch contains a custom tuning, but there is ") +
                    "already a user-selected tuning in place. Do you want to replace the currently "
                    "loaded tuning " +
                    "with the tuning stored in the patch? (The rest of the patch will load "
                    "normally.)",
                "Replace Tuning");
            if (okc == Surge::UserInteractions::MessageResult::OK)
            {
                try
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
                catch (Tunings::TuningError &e)
                {
                    Surge::UserInteractions::promptError(e.what(), "Error Restoring Tuning");
                    storage.retuneTo12TETScaleC261Mapping();
                }
            }
        }
    }

    masterfade = 1.f;
    /*
    ** Notify the host display that the patch name has changed
    */
    updateDisplay();
    return true;
}

void SurgeSynthesizer::enqueuePatchForLoad(void *data, int size)
{
    {
        std::lock_guard<std::mutex> g(rawLoadQueueMutex);

        if (enqueuedLoadData) // this means we missed one because we only free under the lock
            free(enqueuedLoadData);

        enqueuedLoadData = data;
        enqueuedLoadSize = size;
        rawLoadEnqueued = true;
        rawLoadNeedsUIDawExtraState = false;
    }
}

void SurgeSynthesizer::processEnqueuedPatchIfNeeded()
{
    bool expected = true;
    void *freeThis = nullptr;
    if (rawLoadEnqueued.compare_exchange_weak(expected, true) && expected)
    {
        std::lock_guard<std::mutex> g(rawLoadQueueMutex);
        rawLoadEnqueued = false;
        loadRaw(enqueuedLoadData, enqueuedLoadSize);
        loadFromDawExtraState();

        freeThis = enqueuedLoadData;
        enqueuedLoadData = nullptr;
        rawLoadNeedsUIDawExtraState = true;
    }
    if (freeThis)
        free(freeThis); // do this outside the lock
}

void SurgeSynthesizer::loadRaw(const void *data, int size, bool preset)
{
    halt_engine = true;
    allNotesOff();
    for (int s = 0; s < n_scenes; s++)
        for (int i = 0; i < n_customcontrollers; i++)
            storage.getPatch().scene[s].modsources[ms_ctrl1 + i]->reset();

    storage.getPatch().init_default_values();
    storage.getPatch().load_patch(data, size, preset);
    storage.getPatch().update_controls(false, nullptr, true);
    for (int i = 0; i < n_fx_slots; i++)
    {
        memcpy((void *)&fxsync[i], (void *)&storage.getPatch().fx[i], sizeof(FxStorage));
        fx_reload[i] = true;
    }

    loadFx(false, true);

    for (int sc = 0; sc < n_scenes; sc++)
    {
        setParameter01(storage.getPatch().scene[sc].f2_cutoff_is_offset.id,
                       storage.getPatch().scene[sc].f2_cutoff_is_offset.get_value_f01());
    }

    halt_engine = false;
    patch_loaded = true;
    refresh_editor = true;

    if (patchid < 0)
    {
        /*
        ** new patch just loaded so I look up and set the current category and patch.
        ** This is used to draw checkmarks in the menu. If for some reason we don't
        ** find one, nothing will break
        */
        int cnt = storage.patch_list.size();
        string name = storage.getPatch().name;
        string cat = storage.getPatch().category;
        for (int p = 0; p < cnt; ++p)
        {
            if (storage.patch_list[p].name == name &&
                storage.patch_category[storage.patch_list[p].category].name == cat)
            {
                current_category_id = storage.patch_list[p].category;
                patchid = p;
                break;
            }
        }
    }
}

#if MAC || LINUX
#include <sys/types.h>
#include <sys/stat.h>
#endif

string SurgeSynthesizer::getUserPatchDirectory() { return storage.userDataPath; }
string SurgeSynthesizer::getLegacyUserPatchDirectory()
{
    return storage.datapath + "patches_user" + PATH_SEPARATOR;
}

void SurgeSynthesizer::savePatch()
{
    if (storage.getPatch().category.empty())
        storage.getPatch().category = "Default";

    fs::path savepath = string_to_path(getUserPatchDirectory());

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
            Surge::UserInteractions::promptError(
                "Please use relative paths when saving patches. Referring to drive names directly "
                "and using absolute paths is not allowed!",
                "Error");
            return;
        }

        savepath /= catPath;
        create_directories(savepath);
    }
    catch (...)
    {
        Surge::UserInteractions::promptError(
            "Exception occured while creating category folder! Most likely, invalid characters "
            "were used to name the category. Please remove suspicious characters and try again!",
            "Error");
        return;
    }

    fs::path filename = savepath;
    filename /= string_to_path(storage.getPatch().name + ".fxp");

    bool checkExists = true;
#if LINUX
    // Overwrite prompt hangs UI in Bitwig 3.3
    checkExists = (hostProgram.find("bitwig") != std::string::npos);
#endif
    if (checkExists && fs::exists(filename))
    {
        if (Surge::UserInteractions::promptOKCancel(
                std::string("The patch '" + storage.getPatch().name + "' already exists in '" +
                            storage.getPatch().category +
                            "'. Are you sure you want to overwrite it?"),
                std::string("Overwrite patch")) == Surge::UserInteractions::CANCEL)
            return;
    }
    savePatchToPath(filename);
}

void SurgeSynthesizer::savePatchToPath(fs::path filename)
{
    std::ofstream f(filename, std::ios::out | std::ios::binary);

    if (!f)
    {
        Surge::UserInteractions::promptError(
            "Unable to save the patch to the specified path! Maybe it contains invalid characters?",
            "Error");
        return;
    }

    fxChunkSetCustom fxp;
    fxp.chunkMagic = vt_write_int32BE('CcnK');
    fxp.fxMagic = vt_write_int32BE('FPCh');
    fxp.fxID = vt_write_int32BE('cjs3');
    fxp.numPrograms = vt_write_int32BE(1);
    fxp.version = vt_write_int32BE(1);
    fxp.fxVersion = vt_write_int32BE(1);
    strncpy(fxp.prgName, storage.getPatch().name.c_str(), 28);

    void *data;
    unsigned int datasize = storage.getPatch().save_patch(&data);

    fxp.chunkSize = vt_write_int32BE(datasize);
    fxp.byteSize = 0;

    f.write((char *)&fxp, sizeof(fxChunkSetCustom));
    f.write((char *)data, datasize);
    f.close();

    // refresh list
    storage.refresh_patchlist();
    refresh_editor = true;
    midiprogramshavechanged = true;
}

unsigned int SurgeSynthesizer::saveRaw(void **data) { return storage.getPatch().save_patch(data); }
