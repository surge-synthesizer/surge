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

#include "ImportFilesystem.h"

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
   if (id < 0)
      id = 0;
   if (id >= storage.patch_list.size())
      id = id % storage.patch_list.size();

   patchid = id;

   Patch e = storage.patch_list[id];
   loadPatchByPath( e.path.generic_string().c_str(), e.category, e.name.c_str() );
}

bool SurgeSynthesizer::loadPatchByPath( const char* fxpPath, int categoryId, const char* patchName )
{
   FILE* f = fopen(fxpPath, "rb");
   if (!f)
      return false;
   fxChunkSetCustom fxp;
   auto read = fread(&fxp, sizeof(fxChunkSetCustom), 1, f);
   // FIXME - error if read != chunk size
   if ((vt_read_int32BE(fxp.chunkMagic) != 'CcnK') || (vt_read_int32BE(fxp.fxMagic) != 'FPCh') ||
       (vt_read_int32BE(fxp.fxID) != 'cjs3'))
   {
      fclose(f);
      auto cm = vt_read_int32BE(fxp.chunkMagic);
      auto fm = vt_read_int32BE(fxp.fxMagic);
      auto id = vt_read_int32BE(fxp.fxID);

      std::ostringstream oss;
      oss << "Unable to load patch " << patchName << ". ";
      if( cm != 'CcnK' )
      {
         oss << "ChunkMagic is not 'CcnK'. ";
      }
      if( fm != 'FPCh' )
      {
         oss << "FxMagic is not 'FPCh'. ";
      }
      if( id != 'cjs3' )
      {
         union {
            char c[4];
            int id;
         } q;
         q.id = id;
         oss << "Synth id is '" << q.c[0] << q.c[1] << q.c[2] << q.c[3] << "'; Surge expected 'cjs3'. ";
      }
      oss << "This error usually occurs when you attempt to load an .fxp file for an instrument other than Surge into Surge.";
      Surge::UserInteractions::promptError( oss.str(), "Loading Non-Surge FXP" );
      return false;
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
   if( categoryId >= 0 )
   {
      storage.getPatch().category = storage.patch_category[categoryId].name;
   }
   else
   {
      storage.getPatch().category = "direct-load";
   }
   current_category_id = categoryId;
   storage.getPatch().name = patchName;

   loadRaw(data, cs, true);
   free(data);

   /*
   ** OK so at this point we may have loaded a patch with a tuning override
   */
   if( storage.getPatch().patchTuning.tuningStoredInPatch )
   {
       if( storage.isStandardTuning )
       {
          try {
             storage.retuneToScale(Tunings::parseSCLData(storage.getPatch().patchTuning.tuningContents ));
             if( storage.getPatch().patchTuning.mappingContents.size() > 1 )
             {
                storage.remapToKeyboard(Tunings::parseKBMData(storage.getPatch().patchTuning.mappingContents ) );
             }
          }
          catch( Tunings::TuningError &e )
          {
             Surge::UserInteractions::promptError( e.what(), "Error restoring tunings" );
             storage.retuneToStandardTuning();
          }
       }
       else
       {
           auto okc = Surge::UserInteractions::promptOKCancel(std::string("The patch you loaded contains a recommended tuning, but you ") +
                                                              "already have a tuning in place. Do you want to override your current tuning " +
                                                              "with the patch sugeested tuning?",
                                                              "Replace Tuning? (The rest of the patch will still load).");
           if( okc == Surge::UserInteractions::MessageResult::OK )
           {
              try {
                 storage.retuneToScale(Tunings::parseSCLData(storage.getPatch().patchTuning.tuningContents));
                 if( storage.getPatch().patchTuning.mappingContents.size() > 1 )
                 {
                    storage.remapToKeyboard(Tunings::parseKBMData(storage.getPatch().patchTuning.mappingContents ) );
                 }
              }
              catch( Tunings::TuningError &e )
              {
                 Surge::UserInteractions::promptError( e.what(), "Error restoring tunings" );
                 storage.retuneToStandardTuning();
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

void SurgeSynthesizer::loadRaw(const void* data, int size, bool preset)
{
   halt_engine = true;
   allNotesOff();
   for (int s = 0; s < 2; s++)
      for (int i = 0; i < n_customcontrollers; i++)
         storage.getPatch().scene[s].modsources[ms_ctrl1 + i]->reset();

   storage.getPatch().init_default_values();
   storage.getPatch().load_patch(data, size, preset);
   storage.getPatch().update_controls(false, nullptr, true);
   for (int i = 0; i < 8; i++)
   {
      memcpy((void*)&fxsync[i], (void*)&storage.getPatch().fx[i], sizeof(FxStorage));
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

#if WINDOWS && TARGET_RACK
   std::ofstream f(filename.c_str(), std::ios::out | std::ios::binary);
#else
   std::ofstream f(filename, std::ios::out | std::ios::binary);
#endif

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
