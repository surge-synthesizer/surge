//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "SurgeSynthesizer.h"
#include "DspUtilities.h"
#include <time.h>
#include <vt_dsp/vt_dsp_endian.h>
#if LINUX
#include <experimental/filesystem>
#elif MAC
#include <filesystem.h>
#else
#include <filesystem>
#endif
#include <fstream>
#include <iterator>
#include "UserInteractions.h"

namespace fs = std::experimental::filesystem;

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

void SurgeSynthesizer::incrementPatch(bool nextPrev)
{
   int n = storage.patch_list.size();
   if (!n)
      return;

   /*
   ** Ideally we would never call this with an out
   ** of range patchid, but the init case where we
   ** have a non-loaded in memory patch proves that
   ** false, as may some other cases. So add this
   ** defensive approach. See #319
   */
   if( patchid < 0 || patchid > n-1 )
   {
       // Find patch 0 category 0 and select it
       int ccid = storage.patchCategoryOrdering[0];

       int target = n+1;
       for(auto &patch : storage.patch_list)
       {
           if(patch.category == ccid && patch.order < target)
               target = patch.order;
       }
       
       patchid_queue = storage.patchOrdering[target];
       current_category_id = ccid;
   }
   else
   {
       int order = storage.patch_list[patchid].order;
       int category = storage.patch_list[patchid].category;
       
       if (nextPrev) {
           do {
               order = (order >= (n - 1)) ? 0 : order + 1;
           } while (storage.patch_list[storage.patchOrdering[order]].category !=
                    category);
       } else {
           do {
               order = (order <= 0) ? n - 1 : order - 1;
           } while (storage.patch_list[storage.patchOrdering[order]].category !=
                    category);
       }
       patchid_queue = storage.patchOrdering[order];
   }
   processThreadunsafeOperations();
   return;
}

void SurgeSynthesizer::incrementCategory(bool nextPrev)
{
   int n = storage.patch_category.size();
   if (!n)
      return;

   // See comment above and #319
   if(current_category_id < 0 || current_category_id > n-1 )
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
               order = (order >= (n - 1)) ? 0 : order + 1;
           else
               order = (order <= 0) ? n - 1 : order - 1;

           current_category_id = storage.patchCategoryOrdering[order];
       }
       while (storage.patch_category[current_category_id].numberOfPatchesInCatgory == 0 && order != orderOrig);
       // That order != orderOrig isn't needed unless we have an entire empty category tree, in which case it stops an inf loop
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

void SurgeSynthesizer::loadPatch(int id)
{
   patchid = id;
   Patch e = storage.patch_list[id];

   FILE* f = fopen(e.path.generic_string().c_str(), "rb");
   if (!f)
      return;
   fxChunkSetCustom fxp;
   fread(&fxp, sizeof(fxChunkSetCustom), 1, f);
   if ((vt_read_int32BE(fxp.chunkMagic) != 'CcnK') || (vt_read_int32BE(fxp.fxMagic) != 'FPCh') ||
       (vt_read_int32BE(fxp.fxID) != 'cjs3'))
   {
      fclose(f);
      return;
   }

   int cs = vt_read_int32BE(fxp.chunkSize);
   void* data = malloc(cs);
   assert(data);
   size_t actual_cs = fread(data, 1, cs, f);
   int error = ferror(f);
   if (error)
      perror("error while loading patch");
   fclose(f);

   storage.getPatch().comment = "";
   storage.getPatch().author = "";
   storage.getPatch().category = storage.patch_category[e.category].name;
   current_category_id = e.category;
   storage.getPatch().name = e.name;

   loadRaw(data, cs, true);
   free(data);

   masterfade = 1.f;
#if AU
   /*	AUPreset preset;
           preset.presetNumber = patchid;
           preset.presetName =
      CFStringCreateWithCString(NULL,storage.patch_list[patchid].name.c_str(),
      kCFStringEncodingUTF8);
           ((aulayer*)parent)->SetAFactoryPresetAsCurrent(preset);*/
#endif
   /*
   ** Notify the host display that the patch name has changed
   */
   updateDisplay();
}

void SurgeSynthesizer::loadRaw(const void* data, int size, bool preset)
{
   halt_engine = true;
   allNotesOff();
   for (int s = 0; s < 2; s++)
      for (int i = 0; i < n_customcontrollers; i++)
         storage.getPatch().scene[s].modsources[ms_ctrl1 + i]->reset();

   storage.getPatch().init_default_values();
   storage.getPatch().load_patch(data, size, preset);
   storage.getPatch().update_controls(false);
   for (int i = 0; i < 8; i++)
   {
      memcpy(&fxsync[i], &storage.getPatch().fx[i], sizeof(FxStorage));
      fx_reload[i] = true;
   }

   loadFx(false, true);

   setParameter01(storage.getPatch().scene[0].f2_cutoff_is_offset.id,
                  storage.getPatch().scene[0].f2_cutoff_is_offset.get_value_f01());
   setParameter01(storage.getPatch().scene[1].f2_cutoff_is_offset.id,
                  storage.getPatch().scene[1].f2_cutoff_is_offset.get_value_f01());

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

string SurgeSynthesizer::getUserPatchDirectory()
{
   return storage.userDataPath;
}
string SurgeSynthesizer::getLegacyUserPatchDirectory()
{
#if MAC || LINUX
   return storage.datapath + "patches_user/";
#else
   return storage.datapath + "patches_user\\";
#endif
}


void SurgeSynthesizer::savePatch()
{
   if (storage.getPatch().category.empty())
      storage.getPatch().category = "default";

   fs::path savepath = getUserPatchDirectory();
   savepath.append(storage.getPatch().category);

   create_directories(savepath);

   string legalname = storage.getPatch().name;
   for (int i = 0; i < legalname.length(); i++)
   {
      switch (legalname[i])
      {
      case '<':
         legalname[i] = '[';
         break;
      case '>':
         legalname[i] = ']';
         break;
      case '*':
      case '?':
      case '"':
      case '\\':
      case '|':
      case '/':
      case ':':
         legalname[i] = ' ';
         break;
      }
   }

   fs::path filename = savepath;
   filename.append(legalname + ".fxp");

   if (fs::exists(filename))
   {
       if( Surge::UserInteractions::promptOKCancel(std::string( "The patch '" + storage.getPatch().name + "' already exists in '" + storage.getPatch().category
                                                                + "'. Are you sure you want to overwrite it?" ),
                                                   std::string( "Overwrite patch" )) ==
           Surge::UserInteractions::CANCEL )
           return;
   }

   std::ofstream f(filename, std::ios::out | std::ios::binary);

   if (!f)
      return;

   fxChunkSetCustom fxp;
   fxp.chunkMagic = vt_write_int32BE('CcnK');
   fxp.fxMagic = vt_write_int32BE('FPCh');
   fxp.fxID = vt_write_int32BE('cjs3');
   fxp.numPrograms = vt_write_int32BE(1);
   fxp.version = vt_write_int32BE(1);
   fxp.fxVersion = vt_write_int32BE(1);
   strncpy(fxp.prgName, storage.getPatch().name.c_str(), 28);

   void* data;
   unsigned int datasize = storage.getPatch().save_patch(&data);

   fxp.chunkSize = vt_write_int32BE(datasize);
   fxp.byteSize = 0;

   f.write((char*)&fxp, sizeof(fxChunkSetCustom));
   f.write((char*)data, datasize);
   f.close();

   // refresh list
   storage.refresh_patchlist();
   refresh_editor = true;
   midiprogramshavechanged = true;
}

unsigned int SurgeSynthesizer::saveRaw(void** data)
{
   return storage.getPatch().save_patch(data);
}
