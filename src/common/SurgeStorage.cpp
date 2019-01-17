//-------------------------------------------------------------------------------------------------------
//	Copyright 2005-2006 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "DspUtilities.h"
#include "SurgeError.h"
#include "SurgeStorage.h"
#include "UserInteractions.h"
#include <set>
#include <numeric>
#include <vt_dsp/vt_dsp_endian.h>
#if MAC
#include <cstdlib>
#include <sys/stat.h>
//#include <MoreFilesX.h>
//#include <MacErrorHandling.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreServices/CoreServices.h>
#elif __linux__
#include <stdlib.h>
#else
#include <windows.h>
#include <Shellapi.h>
#include <Shlobj.h>
#endif

float sinctable alignas(16)[(FIRipol_M + 1) * FIRipol_N * 2];
float sinctable1X alignas(16)[(FIRipol_M + 1) * FIRipol_N];
short sinctableI16 alignas(16)[(FIRipol_M + 1) * FIRipolI16_N];
float table_dB alignas(16)[512],
      table_pitch alignas(16)[512],
      table_pitch_inv alignas(16)[512],
      table_envrate_lpf alignas(16)[512],
      table_envrate_linear alignas(16)[512];
float table_note_omega alignas(16)[2][512];
float waveshapers alignas(16)[8][1024];
float samplerate, samplerate_inv;
double dsamplerate, dsamplerate_inv;
double dsamplerate_os, dsamplerate_os_inv;

#if MAC
#include <CoreFoundation/CoreFoundation.h>
string getSelfLocation()
{
   char path[PATH_MAX];
   // TODO: use a build-provided symbol
   CFStringRef selfName = CFSTR("com.vemberaudio.plugins.surge");
   CFBundleRef mainBundle = CFBundleGetBundleWithIdentifier(selfName);
   CFURLRef resourcesURL = CFBundleCopyBundleURL(mainBundle);
   CFStringRef str = CFURLCopyFileSystemPath(resourcesURL, kCFURLPOSIXPathStyle);
   CFRelease(resourcesURL);
   CFStringGetCString(str, path, FILENAME_MAX, kCFStringEncodingASCII);
   CFRelease(str);
   string out(path);
   return out;
}
#endif

SurgeStorage::SurgeStorage()
{
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
         double val1X = (float)(symmetric_blackman(t, FIRipol_N) * cutoff1X * sincf(cutoff1X * t));
         sinctable[j * FIRipol_N * 2 + i] = (float)val;
         sinctable1X[j * FIRipol_N + i] = (float)val1X;
      }
   }
   for (j = 0; j < FIRipol_M; j++)
   {
      for (int i = 0; i < FIRipol_N; i++)
      {
         sinctable[j * FIRipol_N * 2 + FIRipol_N + i] =
             (float)((sinctable[(j + 1) * FIRipol_N * 2 + i] - sinctable[j * FIRipol_N * 2 + i]) /
                     65536.0);
      }
   }

   for (j = 0; j < FIRipol_M + 1; j++)
   {
      for (int i = 0; i < FIRipolI16_N; i++)
      {
         double t = -double(i) + double(FIRipolI16_N / 2.0) + double(j) / double(FIRipol_M) - 1.0;
         double val =
             (float)(symmetric_blackman(t, FIRipolI16_N) * cutoffI16 * sincf(cutoffI16 * t));

         sinctableI16[j * FIRipolI16_N + i] = (short)((float)val * 16384.f);
      }
   }
   /*for(j=0; j<FIRipolI16_N; j++){
           for(int i=0; i<FIRipol_N; i++){
                   sinctable[j*FIRipolI16_N*2 + FIRipolI16_N + i] = sinctable[(j+1)*FIRipolI16_N*2 +
   i] - sinctable[j*FIRipolI16_N*2 + i]));
           }
   }*/

   for (int s = 0; s < 2; s++)
      for (int o = 0; o < n_oscs; o++)
         for (int i = 0; i < max_mipmap_levels; i++)
            for (int j = 0; j < max_subtables; j++)
            {
               getPatch().scene[s].osc[o].wt.TableF32WeakPointers[i][j] = 0;
               getPatch().scene[s].osc[o].wt.TableI16WeakPointers[i][j] = 0;
            }
   init_tables();
   pitch_bend = 0;
   last_key[0] = 60;
   last_key[1] = 60;
   temposyncratio = 1.f;
   songpos = 0;

   for (int i = 0; i < n_customcontrollers; i++)
   {
      controllers[i] = 41 + i;
   }
   for (int i = 0; i < n_modsources; i++)
      modsource_vu[i] = 0.f; // remove?

   for (int s = 0; s < 2; s++)
      for (int cc = 0; cc < 128; cc++)
         poly_aftertouch[s][cc] = 0.f;

   memset(&audio_in[0][0], 0, 2 * BLOCK_SIZE_OS * sizeof(float));

#if MAC
   char path[1024];
   FSRef foundRef;
   OSErr err = FSFindFolder(kUserDomain, kApplicationSupportFolderType, false, &foundRef);
   // or kUserDomain
   FSRefMakePath(&foundRef, (UInt8*)path, 1024);
   datapath = path;
   datapath += "/Surge/";

   // check if the directory exist in the user domain (if it doesn't, fall back to the local domain)
   CFStringRef testpathCF = CFStringCreateWithCString(0, datapath.c_str(), kCFStringEncodingUTF8);
   CFURLRef testCat = CFURLCreateWithFileSystemPath(0, testpathCF, kCFURLPOSIXPathStyle, true);
   CFRelease(testpathCF);
   FSRef myfsRef;
   Boolean works = CFURLGetFSRef(testCat, &myfsRef);
   CFRelease(testCat); // don't need it anymore?!?
   if (!works)
   {
      OSErr err = FSFindFolder(kLocalDomain, kApplicationSupportFolderType, false, &foundRef);
      FSRefMakePath(&foundRef, (UInt8*)path, 1024);
      datapath = path;
      datapath += "/Surge/";
   }

   // ~/Documents/Surge in full name
   sprintf( path, "%s/Documents/Surge", getenv( "HOME" ) );
   userDataPath = path;
   
#elif __linux__

   /*
    * Even though Linux distinguishes between configuration and data folders,
    * Surge's configuration.xml does not contain any user preferences that are
    * modified through the interface, so it seems simplest to continue treating
    * it as part of the data.
    */

   string home = string(getenv("HOME"));
   char* data_home_cstr = getenv("XDG_DATA_HOME");
   string data_home = (data_home_cstr) ? string(data_home_cstr)
                                       : home + "/.local/share";

   datapath = data_home + "/Surge/";

   userDataPath = home + "/Documents/Surge";

#else

   PWSTR localAppData;
   if (!SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData))
   {
      CHAR path[4096];
      wsprintf(path, "%S\\Surge\\", localAppData);
      datapath = path;
   }

   PWSTR documentsFolder;
   if (!SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &documentsFolder))
   {
      CHAR path[4096];
      wsprintf(path, "%S\\Surge\\", documentsFolder);
      userDataPath = path;
   }

#endif
   string snapshotmenupath = datapath + "configuration.xml";

   if (!snapshotloader.LoadFile(snapshotmenupath.c_str())) // load snapshots (& config-stuff)
   {
      Surge::Error exc("Cannot find 'configuration.xml' in path '" + datapath + "'. Please reinstall surge.",
                       "Surge is not properly installed.");
#if __linux__
      throw exc;
#else
      Surge::UserInteractions::promptError(exc);
#endif
   }

   TiXmlElement* e = snapshotloader.FirstChild("autometa")->ToElement();
   if (e)
   {
      defaultname = e->Attribute("name");
      defaultsig = e->Attribute("comment");
   }

   load_midi_controllers();
   refresh_wtlist();
   refresh_patchlist();
   getPatch().scene[0].osc[0].wt.dt = 1.0f / 512.f;
   load_wt(0, &getPatch().scene[0].osc[0].wt);

   memset(&WindowWT, 0, sizeof(WindowWT));
   load_wt_wt(datapath + "windows.wt", &WindowWT);
}

SurgePatch& SurgeStorage::getPatch()
{
   return *_patch.get();
}

struct PEComparer
{
   bool operator()(const Patch& a, const Patch& b)
   {
      return a.name.compare(b.name) < 0;
   }
};

void SurgeStorage::refresh_patchlist()
{
   patch_category.clear();
   patch_list.clear();

   refreshPatchlistAddDir(false, "patches_factory");
   firstThirdPartyCategory = patch_category.size();
   refreshPatchlistAddDir(false, "patches_3rdparty");
   firstUserCategory = patch_category.size();
   refreshPatchlistAddDir(true, "");

   patchOrdering = std::vector<int>(patch_list.size());
   std::iota(patchOrdering.begin(), patchOrdering.end(), 0);

   auto patchCompare =
      [this](const int &i1, const int &i2) -> bool
      {
         return _stricmp(patch_list[i1].name.c_str(),
                         patch_list[i2].name.c_str()) < 0;
      };

   std::sort(patchOrdering.begin(), patchOrdering.end(), patchCompare);

   patchCategoryOrdering = std::vector<int>(patch_category.size());
   std::iota(patchCategoryOrdering.begin(), patchCategoryOrdering.end(), 0);

   for (int i = 0; i < patch_list.size(); i++)
      patch_list[patchOrdering[i]].order = i;

   auto categoryCompare =
      [this](const int &i1, const int &i2) -> bool
      {
         return _stricmp(patch_category[i1].name.c_str(),
                         patch_category[i2].name.c_str()) < 0;
      };

   int groups[4] = {0, firstThirdPartyCategory, firstUserCategory,
                    (int)patch_category.size()};

   for (int i = 0; i < 3; i++)
      std::sort(std::next(patchCategoryOrdering.begin(), groups[i]),
                std::next(patchCategoryOrdering.begin(), groups[i + 1]),
                categoryCompare);

   for (int i = 0; i < patch_category.size(); i++)
      patch_category[patchCategoryOrdering[i]].order = i;
}

void SurgeStorage::refreshPatchlistAddDir(bool userDir, string subdir)
{
   int category = patch_category.size();

   fs::path patchpath = (userDir ? userDataPath : datapath);
   if (!subdir.empty())
      patchpath.append(subdir);

   if (!fs::is_directory(patchpath))
   {
      return;
   }

   for (auto& p : fs::directory_iterator(patchpath))
   {
      if (fs::is_directory(p))
      {
         PatchCategory c;
         c.name = p.path().filename().generic_string();
         patch_category.push_back(c);

         for (auto& f : fs::directory_iterator(p))
         {
            if (_stricmp(f.path().extension().generic_string().c_str(), ".fxp") == 0)
            {
               Patch e;
               e.category = category;
               e.path = f.path();
               e.name = f.path().filename().generic_string();
               e.name = e.name.substr(0, e.name.size() - 4);
               patch_list.push_back(e);
            }
         }

         category++;
      }
   }
}

void SurgeStorage::refresh_wtlist()
{
   wt_category.clear();
   wt_list.clear();

   int category = 0;

   fs::path patchpath = datapath;
   patchpath.append("wavetables");

   if (!fs::is_directory(patchpath))
   {
      return;
   }

   for (auto& p : fs::directory_iterator(patchpath))
   {
      if (fs::is_directory(p))
      {
         PatchCategory c;
         c.name = p.path().filename().generic_string();
         wt_category.push_back(c);

         for (auto& f : fs::directory_iterator(p))
         {
            if (_stricmp(f.path().extension().generic_string().c_str(), ".wt") == 0)
            {
               Patch e;
               e.category = category;
               e.path = f.path();
               e.name = f.path().filename().generic_string();
               e.name = e.name.substr(0, e.name.size() - 3);
               wt_list.push_back(e);
            }
            else if (_stricmp(f.path().extension().generic_string().c_str(), ".wav") == 0)
            {
               Patch e;
               e.category = category;
               e.path = f.path();
               e.name = f.path().filename().generic_string();
               e.name = e.name.substr(0, e.name.size() - 4);
               wt_list.push_back(e);
            }
         }

         category++;
      }
   }

   if (wt_category.size() < 1 && wt_list.size() < 1)
   {
       Surge::UserInteractions::promptError(std::string("Surge was unable to load wavetables from disk. ") +
                                            "Please reinstall using the surge installer.",
                                            "Surge Installation Error" );
   }

   wtCategoryOrdering = std::vector<int>(wt_category.size());
   std::iota(wtCategoryOrdering.begin(), wtCategoryOrdering.end(), 0);

   auto categoryCompare =
      [this](const int &i1, const int &i2) -> bool
      {
         return _stricmp(wt_category[i1].name.c_str(),
                         wt_category[i2].name.c_str()) < 0;
      };

   std::sort(wtCategoryOrdering.begin(), wtCategoryOrdering.end(),
             categoryCompare);

   for (int i = 0; i < wt_category.size(); i++)
      wt_category[wtCategoryOrdering[i]].order = i;

   wtOrdering = std::vector<int>();

   auto wtCompare =
      [this](const int &i1, const int &i2) -> bool
      {
         return _stricmp(wt_list[i1].name.c_str(),
                         wt_list[i2].name.c_str()) < 0;
      };

   // Sort wavetables per category in the category order.
   for (auto c : wtCategoryOrdering) {
      int start = wtOrdering.size();

      for (int i = 0; i < wt_list.size(); i++)
         if (wt_list[i].category == c)
            wtOrdering.push_back(i);

      int end = wtOrdering.size();

      std::sort(std::next(wtOrdering.begin(), start),
                std::next(wtOrdering.begin(), end), wtCompare);
   }

   for (int i = 0; i < wt_list.size(); i++)
      wt_list[wtOrdering[i]].order = i;
}

void SurgeStorage::perform_queued_wtloads()
{
   for (int sc = 0; sc < 2; sc++)
   {
      for (int o = 0; o < n_oscs; o++)
      {
         if (getPatch().scene[sc].osc[o].wt.queue_id != -1)
         {
            load_wt(getPatch().scene[sc].osc[o].wt.queue_id, &getPatch().scene[sc].osc[o].wt);
            getPatch().scene[sc].osc[o].wt.refresh_display = true;
         }
         else if (getPatch().scene[sc].osc[o].wt.queue_filename[0])
         {
            getPatch().scene[sc].osc[o].queue_type = ot_wavetable;
            getPatch().scene[sc].osc[o].wt.current_id = -1;
            load_wt(getPatch().scene[sc].osc[o].wt.queue_filename, &getPatch().scene[sc].osc[o].wt);
            getPatch().scene[sc].osc[o].wt.refresh_display = true;
         }
      }
   }
}

void SurgeStorage::load_wt(int id, Wavetable* wt)
{
   wt->current_id = id;
   wt->queue_id = -1;
   if (id < 0)
      return;
   if (id >= wt_list.size())
      return;
   if (!wt)
      return;
   load_wt(wt_list[id].path.generic_string(), wt);
}

void SurgeStorage::load_wt(string filename, Wavetable* wt)
{
   wt->queue_filename[0] = 0;
   string extension = filename.substr(filename.find_last_of('.'), filename.npos);
   for (unsigned int i = 0; i < extension.length(); i++)
      extension[i] = tolower(extension[i]);
   if (extension.compare(".wt") == 0)
      load_wt_wt(filename, wt);
#if !MAC
   else if (extension.compare(".wav") == 0)
      load_wt_wav(filename, wt);
#endif
}

void SurgeStorage::load_wt_wt(string filename, Wavetable* wt)
{
   FILE* f = fopen(filename.c_str(), "rb");
   if (!f)
      return;
   wt_header wh;
   memset(&wh, 0, sizeof(wt_header));

   size_t read = fread(&wh, sizeof(wt_header), 1, f);
   // I'm not sure why this ever worked but it is checking the 4 bytes against vawt so...
   // if (wh.tag != vt_read_int32BE('vawt'))
   if (!(wh.tag[0] == 'v' && wh.tag[1] == 'a' && wh.tag[2] == 'w' && wh.tag[3] == 't'))
   {
      // SOME sort of error reporting is appropriate
      fclose(f);
      return;
   }

   void* data;
   size_t ds;
   if (vt_read_int16LE(wh.flags) & wtf_int16)
      ds = sizeof(short) * vt_read_int16LE(wh.n_tables) * vt_read_int32LE(wh.n_samples);
   else
      ds = sizeof(float) * vt_read_int16LE(wh.n_tables) * vt_read_int32LE(wh.n_samples);

   data = malloc(ds);
   fread(data, 1, ds, f);
   CS_WaveTableData.enter();
   wt->BuildWT(data, wh, false);
   CS_WaveTableData.leave();
   free(data);

   fclose(f);
}
int SurgeStorage::get_clipboard_type()
{
   return clipboard_type;
}

int SurgeStorage::getAdjacentWaveTable(int id, bool nextPrev)
{
   int n = wt_list.size();
   if (!n)
      return -1;

   int order = wt_list[id].order;

   if (nextPrev)
      order = (order == (n - 1)) ? 0 : order + 1;
   else
      order = (order == 0) ? n - 1 : order - 1;

   return wtOrdering[order];
}

void SurgeStorage::clipboard_copy(int type, int scene, int entry)
{
   bool includemod = false, includeall = false;
   if (type == cp_oscmod)
   {
      type = cp_osc;
      includemod = true;
   }
   int cgroup = -1;
   int cgroup_e = -1;
   int id = -1;

   clipboard_type = type;
   switch (type)
   {
   case cp_osc:
      cgroup = 2;
      cgroup_e = entry;
      id = getPatch().scene[scene].osc[entry].type.id; // first parameter id
      if (uses_wavetabledata(getPatch().scene[scene].osc[entry].type.val.i))
      {
         clipboard_wt[0].Copy(&getPatch().scene[scene].osc[entry].wt);
      }
      break;
   case cp_lfo:
      cgroup = 6;
      cgroup_e = entry + ms_lfo1;
      id = getPatch().scene[scene].lfo[entry].shape.id;
      if (getPatch().scene[scene].lfo[entry].shape.val.i == ls_stepseq)
         memcpy(&clipboard_stepsequences[0], &getPatch().stepsequences[scene][entry],
                sizeof(StepSequencerStorage));
      break;
   case cp_scene:
   {
      includemod = true;
      includeall = true;
      id = getPatch().scene[scene].octave.id;
      for (int i = 0; i < n_lfos; i++)
         memcpy(&clipboard_stepsequences[i], &getPatch().stepsequences[scene][i],
                sizeof(StepSequencerStorage));
      for (int i = 0; i < n_oscs; i++)
      {
         clipboard_wt[i].Copy(&getPatch().scene[scene].osc[i].wt);
      }
   }
   break;
   default:
      return;
   }

   // CS ENTER
   CS_ModRouting.enter();
   {

      clipboard_p.clear();
      clipboard_modulation_scene.clear();
      clipboard_modulation_voice.clear();

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
            idoffset = -id + n_global_params;
         n = getPatch().scene[scene].modulation_voice.size();
         for (int i = 0; i < n; i++)
         {
            ModulationRouting m;
            m.source_id = getPatch().scene[scene].modulation_voice[i].source_id;
            m.depth = getPatch().scene[scene].modulation_voice[i].depth;
            m.destination_id =
                getPatch().scene[scene].modulation_voice[i].destination_id + idoffset;
            if (includeall || (used_entries.find(m.destination_id) != used_entries.end()))
               clipboard_modulation_voice.push_back(m);
         }
         n = getPatch().scene[scene].modulation_scene.size();
         for (int i = 0; i < n; i++)
         {
            ModulationRouting m;
            m.source_id = getPatch().scene[scene].modulation_scene[i].source_id;
            m.depth = getPatch().scene[scene].modulation_scene[i].depth;
            m.destination_id =
                getPatch().scene[scene].modulation_scene[i].destination_id + idoffset;
            if (includeall || (used_entries.find(m.destination_id) != used_entries.end()))
               clipboard_modulation_scene.push_back(m);
         }
      }
   }
   // CS LEAVE
   CS_ModRouting.leave();
}

void SurgeStorage::clipboard_paste(int type, int scene, int entry)
{
   assert(scene < 2);
   if (type != clipboard_type)
      return;

   int cgroup = -1;
   int cgroup_e = -1;
   int id = -1;
   int n = clipboard_p.size();
   int start = 0;

   if (!n)
      return;

   switch (type)
   {
   case cp_osc:
      cgroup = 2;
      cgroup_e = entry;
      id = getPatch().scene[scene].osc[entry].type.id; // first parameter id
      getPatch().scene[scene].osc[entry].type.val.i = clipboard_p[0].val.i;
      start = 1;
      getPatch().update_controls(false, &getPatch().scene[scene].osc[entry]);
      break;
   case cp_lfo:
      cgroup = 6;
      cgroup_e = entry + ms_lfo1;
      id = getPatch().scene[scene].lfo[entry].shape.id;
      break;
   case cp_scene:
   {
      id = getPatch().scene[scene].octave.id;
      for (int i = 0; i < n_lfos; i++)
         memcpy(&getPatch().stepsequences[scene][i], &clipboard_stepsequences[i],
                sizeof(StepSequencerStorage));
      for (int i = 0; i < n_oscs; i++)
      {
         getPatch().scene[scene].osc[i].wt.Copy(&clipboard_wt[i]);
      }
   }
   break;
   default:
      return;
   }

   // CS ENTER
   CS_ModRouting.enter();
   {

      for (int i = start; i < n; i++)
      {
         Parameter p = clipboard_p[i];
         int pid = p.id + id;
         getPatch().param_ptr[pid]->val.i = p.val.i;
         getPatch().param_ptr[pid]->temposync = p.temposync;
         getPatch().param_ptr[pid]->extend_range = p.extend_range;
      }

      switch (type)
      {
      case cp_osc:
      {
         if (uses_wavetabledata(getPatch().scene[scene].osc[entry].type.val.i))
         {
            getPatch().scene[scene].osc[entry].wt.Copy(&clipboard_wt[0]);
         }

         // copy modroutings
         n = clipboard_modulation_voice.size();
         for (int i = 0; i < n; i++)
         {
            ModulationRouting m;
            m.source_id = clipboard_modulation_voice[i].source_id;
            m.depth = clipboard_modulation_voice[i].depth;
            m.destination_id = clipboard_modulation_voice[i].destination_id + id - n_global_params;
            getPatch().scene[scene].modulation_voice.push_back(m);
         }
         n = clipboard_modulation_scene.size();
         for (int i = 0; i < n; i++)
         {
            ModulationRouting m;
            m.source_id = clipboard_modulation_scene[i].source_id;
            m.depth = clipboard_modulation_scene[i].depth;
            m.destination_id = clipboard_modulation_scene[i].destination_id + id - n_global_params;
            getPatch().scene[scene].modulation_scene.push_back(m);
         }
      }
      break;
      case cp_lfo:
         if (getPatch().scene[scene].lfo[entry].shape.val.i == ls_stepseq)
            memcpy(&getPatch().stepsequences[scene][entry], &clipboard_stepsequences[0],
                   sizeof(StepSequencerStorage));
         break;
      case cp_scene:
      {
         getPatch().scene[scene].modulation_voice.clear();
         getPatch().scene[scene].modulation_scene.clear();
         getPatch().update_controls(false);

         n = clipboard_modulation_voice.size();
         for (int i = 0; i < n; i++)
         {
            ModulationRouting m;
            m.source_id = clipboard_modulation_voice[i].source_id;
            m.depth = clipboard_modulation_voice[i].depth;
            m.destination_id = clipboard_modulation_voice[i].destination_id;
            getPatch().scene[scene].modulation_voice.push_back(m);
         }
         n = clipboard_modulation_scene.size();
         for (int i = 0; i < n; i++)
         {
            ModulationRouting m;
            m.source_id = clipboard_modulation_scene[i].source_id;
            m.depth = clipboard_modulation_scene[i].depth;
            m.destination_id = clipboard_modulation_scene[i].destination_id;
            getPatch().scene[scene].modulation_scene.push_back(m);
         }
      }
      }
   }
   // CS LEAVE
   CS_ModRouting.leave();
}

TiXmlElement* SurgeStorage::getSnapshotSection(const char* name)
{
   TiXmlElement* e = snapshotloader.FirstChild(name)->ToElement();
   if (e)
      return e;

   // ok, create a new one then
   TiXmlElement ne(name);
   snapshotloader.InsertEndChild(ne);
   return snapshotloader.FirstChild(name)->ToElement();
}

void SurgeStorage::save_snapshots()
{
   snapshotloader.SaveFile();
}

void SurgeStorage::save_midi_controllers()
{
   TiXmlElement* mc = getSnapshotSection("midictrl");
   assert(mc);
   mc->Clear();

   int n = n_global_params + n_scene_params; // only store midictrl's for scene A (scene A -> scene
                                             // B will be duplicated on load)
   for (int i = 0; i < n; i++)
   {
      if (getPatch().param_ptr[i]->midictrl >= 0)
      {
         TiXmlElement mc_e("entry");
         mc_e.SetAttribute("p", i);
         mc_e.SetAttribute("ctrl", getPatch().param_ptr[i]->midictrl);
         mc->InsertEndChild(mc_e);
      }
   }

   TiXmlElement* cc = getSnapshotSection("customctrl");
   assert(cc);
   cc->Clear();

   for (int i = 0; i < n_customcontrollers; i++)
   {
      TiXmlElement cc_e("entry");
      cc_e.SetAttribute("p", i);
      cc_e.SetAttribute("ctrl", controllers[i]);
      cc->InsertEndChild(cc_e);
   }

   save_snapshots();
}

void SurgeStorage::load_midi_controllers()
{
   TiXmlElement* mc = getSnapshotSection("midictrl");
   assert(mc);

   TiXmlElement* entry = mc->FirstChild("entry")->ToElement();
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
      entry = entry->NextSibling("entry")->ToElement();
   }

   TiXmlElement* cc = getSnapshotSection("customctrl");
   assert(cc);

   entry = cc->FirstChild("entry")->ToElement();
   while (entry)
   {
      int id, ctrl;
      if ((entry->QueryIntAttribute("p", &id) == TIXML_SUCCESS) &&
          (entry->QueryIntAttribute("ctrl", &ctrl) == TIXML_SUCCESS) && (id < n_customcontrollers))
      {
         controllers[id] = ctrl;
      }
      entry = entry->NextSibling("entry")->ToElement();
   }
}

SurgeStorage::~SurgeStorage()
{}

double shafted_tanh(double x)
{
   return (exp(x) - exp(-x * 1.2)) / (exp(x) + exp(-x));
}

void SurgeStorage::init_tables()
{
   float db60 = powf(10.f, 0.05f * -60.f);
   for (int i = 0; i < 512; i++)
   {
      table_dB[i] = powf(10.f, 0.05f * ((float)i - 384.f));
      table_pitch[i] = powf(2.f, ((float)i - 256.f) * (1.f / 12.f));
      table_pitch_inv[i] = 1.f / table_pitch[i];
      table_note_omega[0][i] =
          (float)sin(2 * M_PI * min(0.5, 440 * table_pitch[i] * dsamplerate_os_inv));
      table_note_omega[1][i] =
          (float)cos(2 * M_PI * min(0.5, 440 * table_pitch[i] * dsamplerate_os_inv));
      double k = dsamplerate_os * pow(2.0, (((double)i - 256.0) / 16.0)) / (double)BLOCK_SIZE_OS;
      table_envrate_lpf[i] = (float)(1.f - exp(log(db60) / k));
      table_envrate_linear[i] = (float)1.f / k;
   }

   double mult = 1.0 / 32.0;
   for (int i = 0; i < 1024; i++)
   {
      double x = ((double)i - 512.0) * mult;
      waveshapers[0][i] = (float)tanh(x);                            // wst_tanh,
      waveshapers[1][i] = (float)pow(tanh(pow(::abs(x), 5.0)), 0.2); // wst_hard
      if (x < 0)
         waveshapers[1][i] = -waveshapers[1][i];
      waveshapers[2][i] = (float)shafted_tanh(x + 0.5) - shafted_tanh(0.5);       // wst_assym
      waveshapers[3][i] = (float)sin((double)((double)i - 512.0) * M_PI / 512.0); // wst_sinus
      waveshapers[4][i] = (float)tanh((double)((double)i - 512.0) * mult);        // wst_digi
   }
   /*for(int i=0; i<512; i++)
   {
           double x = 2.0*M_PI*((double)i)/512.0;
           table_sin[i] = sin(x);
           table_sin_offset[i] = sin(x+(2.0*M_PI/512.0))-sin(x);
   }*/
   // from 1.2.2
   // nyquist_pitch = (float)12.f*log((0.49999*M_PI) / (dsamplerate_os_inv *
   // 2*M_PI*440.0))/log(2.0);	// include some margin for error (and to avoid denormals in IIR
   // filter clamping)
   // 1.3
   nyquist_pitch =
       (float)12.f * log((0.75 * M_PI) / (dsamplerate_os_inv * 2 * M_PI * 440.0)) /
       log(2.0); // include some margin for error (and to avoid denormals in IIR filter clamping)
   vu_falloff = 0.997f; // TODO should be samplerate-dependent (this is per 32-sample block at 44.1)
}

float note_to_pitch(float x)
{
   x += 256;
   int e = (int)x;
   float a = x - (float)e;

   if (e > 0x1fe)
      e = 0x1fe;

   return (1 - a) * table_pitch[e & 0x1ff] + a * table_pitch[(e + 1) & 0x1ff];
}

float note_to_pitch_inv(float x)
{
   x += 256;
   int e = (int)x;
   float a = x - (float)e;

   if (e > 0x1fe)
      e = 0x1fe;

   return (1 - a) * table_pitch_inv[e & 0x1ff] + a * table_pitch_inv[(e + 1) & 0x1ff];
}

void note_to_omega(float x, float& sinu, float& cosi)
{
   x += 256;
   int e = (int)x;
   float a = x - (float)e;

   if (e > 0x1fe)
      e = 0x1fe;
   else if (e < 0)
      e = 0;

   sinu = (1 - a) * table_note_omega[0][e & 0x1ff] + a * table_note_omega[0][(e + 1) & 0x1ff];
   cosi = (1 - a) * table_note_omega[1][e & 0x1ff] + a * table_note_omega[1][(e + 1) & 0x1ff];
}

float db_to_linear(float x)
{
   x += 384;
   int e = (int)x;
   float a = x - (float)e;

   return (1 - a) * table_dB[e & 0x1ff] + a * table_dB[(e + 1) & 0x1ff];
}

float lookup_waveshape(int entry, float x)
{
   x *= 32.f;
   x += 512.f;
   int e = (int)x;
   float a = x - (float)e;

   if (e > 0x3fd)
      return 1;
   if (e < 1)
      return -1;

   return (1 - a) * waveshapers[entry][e & 0x3ff] + a * waveshapers[entry][(e + 1) & 0x3ff];
}

float lookup_waveshape_warp(int entry, float x)
{
   x *= 256.f;
   x += 512.f;

   int e = (int)x;
   float a = x - (float)e;

   return (1 - a) * waveshapers[entry][e & 0x3ff] + a * waveshapers[entry][(e + 1) & 0x3ff];
}

float envelope_rate_lpf(float x)
{
   x *= 16.f;
   x += 256.f;
   int e = (int)x;
   float a = x - (float)e;

   return (1 - a) * table_envrate_lpf[e & 0x1ff] + a * table_envrate_lpf[(e + 1) & 0x1ff];
}

float envelope_rate_linear(float x)
{
   x *= 16.f;
   x += 256.f;
   int e = (int)x;
   float a = x - (float)e;

   return (1 - a) * table_envrate_linear[e & 0x1ff] + a * table_envrate_linear[(e + 1) & 0x1ff];
}
