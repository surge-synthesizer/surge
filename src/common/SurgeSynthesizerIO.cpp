//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "SurgeSynthesizer.h"
#include "DspUtilities.h"
#include <time.h>
#include <vt_dsp/vt_dsp_endian.h>
#if __linux__
#include <experimental/filesystem>
#elif MAC
#include <filesystem.h>
#else
#include <filesystem>
#endif
#include <fstream>
#include <iterator>

namespace fs = std::experimental::filesystem;

#if AU
#include "aulayer.h"
#endif

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

void SurgeSynthesizer::incrementPatch(int category, int patch)
{
   if (category)
   {
      int n = storage.patch_category.size();
      if (!n)
         return;
      current_category_id += category;
      if (current_category_id >= n)
         current_category_id = 0;
      else if (current_category_id < 0)
         current_category_id = n - 1;
      n = storage.patch_list.size();
      for (int i = 0; i < n; i++)
      {
         if (storage.patch_list[i].category ==
             current_category_id) // find the first patch within the new category
         {
            // load_patch(i);
            patchid_queue = i;
            processThreadunsafeOperations();
            return;
         }
      }
   }
   else
   {
      // load_patch(patchid + patch);
      patchid_queue = patchid + patch;
      if (patchid_queue < 0)
         patchid_queue = storage.patch_list.size() - 1;
      processThreadunsafeOperations();
      return;
   }
}

void SurgeSynthesizer::loadPatch(int id)
{
   if (id < 0)
      id = storage.patch_list.size() - 1;
   else if (id >= storage.patch_list.size())
      id = 0;
   patchid = id;
   if (id >= storage.patch_list.size())
      return;
   patchlist_entry e = storage.patch_list[id];

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
}

#if MAC || __linux__
#include <sys/types.h>
#include <sys/stat.h>
#endif

string SurgeSynthesizer::getUserPatchDirectory()
{
   return storage.userDataPath;
}
string SurgeSynthesizer::getLegacyUserPatchDirectory()
{
#if MAC || __linux__
   return storage.datapath + "patches_user/";
#else
   return storage.datapath + "patches_user\\";
#endif
}

bool askIfShouldOverwrite()
{
#if MAC
   CFOptionFlags responseFlags;
   CFUserNotificationDisplayAlert(0, kCFUserNotificationPlainAlertLevel, 0, 0, 0,
                                  CFSTR("Overwrite?"),
                                  CFSTR("The file already exists, do you wish to overwrite it?"),
                                  CFSTR("Yes"), CFSTR("No"), 0, &responseFlags);
   if ((responseFlags & 0x3) != kCFUserNotificationDefaultResponse)
      return false;
#elif __linux__
   printf("Implement me\n");
#else
   if (MessageBox(::GetActiveWindow(), "The file already exists, do you wish to overwrite it?",
                  "Overwrite?", MB_YESNO | MB_ICONQUESTION) != IDYES)
      return false;
#endif

   return true;
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
      if (!askIfShouldOverwrite())
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
