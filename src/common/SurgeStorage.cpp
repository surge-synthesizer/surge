//-------------------------------------------------------------------------------------------------------
//	Copyright 2005-2006 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "DspUtilities.h"
#include "SurgeError.h"
#include "SurgeStorage.h"
#include "UserInteractions.h"
#include <set>
#include <numeric>
#include <cctype>
#include <map>
#include <queue>
#include <vt_dsp/vt_dsp_endian.h>
#if MAC
#include <cstdlib>
#include <sys/stat.h>
//#include <MoreFilesX.h>
//#include <MacErrorHandling.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreServices/CoreServices.h>
#elif LINUX
#include <stdlib.h>
#include "ConfigurationXml.h"
#else
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

#include <iostream>
#include <iomanip>
#include <sstream>

#include "UserDefaults.h"

#include "strnatcmp.h"


float sinctable alignas(16)[(FIRipol_M + 1) * FIRipol_N * 2];
float sinctable1X alignas(16)[(FIRipol_M + 1) * FIRipol_N];
short sinctableI16 alignas(16)[(FIRipol_M + 1) * FIRipolI16_N];
float table_dB alignas(16)[512],
      table_envrate_lpf alignas(16)[512],
      table_envrate_linear alignas(16)[512];
float waveshapers alignas(16)[8][1024];
float samplerate = 0, samplerate_inv;
double dsamplerate, dsamplerate_inv;
double dsamplerate_os, dsamplerate_os_inv;

using namespace std;

SurgeStorage::SurgeStorage(std::string suppliedDataPath)
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

   bool hasSuppliedDataPath = false;
   if(suppliedDataPath.size() != 0)
   {
       hasSuppliedDataPath = true;
   }
   
#if MAC || LINUX
   const char* homePath = getenv("HOME");
   if (!homePath)
      throw Surge::Error("The environment variable HOME does not exist",
                         "Surge failed to initialize");
#endif

#if MAC
   char path[1024];
   if (!hasSuppliedDataPath)
   {
       FSRef foundRef;
       OSErr err = FSFindFolder(kUserDomain, kApplicationSupportFolderType, false, &foundRef);
       // or kUserDomain
       FSRefMakePath(&foundRef, (UInt8*)path, 1024);
       datapath = path;
       datapath += "/SurgePlusPlus/";

       auto cxmlpath = datapath + "configuration.xml";
       // check if the directory exist in the user domain (if it doesn't, fall back to the local domain)
       // See #863 where I chaned this to dir exists and contains config
       CFStringRef testpathCF = CFStringCreateWithCString(0, cxmlpath.c_str(), kCFStringEncodingUTF8);
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
   }
   else
   {
       datapath = suppliedDataPath;
   }
   
   // ~/Documents/Surge in full name
   sprintf(path, "%s/Documents/Surge", homePath);
   userDataPath = path;
#elif LINUX
   if(!hasSuppliedDataPath)
   {
       const char* xdgDataPath = getenv("XDG_DATA_HOME");
       if (xdgDataPath)
           datapath = std::string(xdgDataPath) + "/SurgePlusPlus/";
       else
           datapath = std::string(homePath) + "/.local/share/SurgePlusPlus/";
       
       /*
       ** If local directory doesn't exists - we probably came here through an installer -
       ** use /usr/share/Surge as our last guess
       */
       if (! fs::is_directory(datapath))
       {
           datapath = "/usr/share/SurgePlusPlus/";
       }
   }
   else
   {
       datapath = suppliedDataPath;
   }
   
   /* 
   ** See the discussion in github issue #930. Basically
   ** if ~/Documents/Surge exists use that
   ** else if ~/.Surge exists use that
   ** else if ~/.Documents exists, use ~/Documents/Surge
   ** else use ~/.Surge
   ** Compensating for whether your distro makes you a ~/Documents or not
   */

   std::string documentsSurge = std::string(homePath) + "/Documents/Surge";
   std::string dotSurge = std::string(homePath) + "/.Surge";
   std::string documents = std::string(homePath) + "/Documents/";

   if( fs::is_directory(documentsSurge) )
   { 
      userDataPath = documentsSurge;
   }
   else if( fs::is_directory(dotSurge) )
   {
      userDataPath = dotSurge;
   }
   else if( fs::is_directory(documents) )
   {
      userDataPath = documentsSurge;
   }
   else
   {
      userDataPath = dotSurge;
   }
  
#elif WINDOWS
#if TARGET_RACK
   datapath = suppliedDataPath;
#else
   PWSTR localAppData;
   if (!SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData))
   {
      CHAR path[4096];
      wsprintf(path, "%S\\SurgePlusPlus\\", localAppData);
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
#endif

   userDefaultFilePath = userDataPath;
   
   std::string userSpecifiedDataPath = Surge::Storage::getUserDefaultValue(this, "userDataPath", "UNSPEC" );
   if( userSpecifiedDataPath != "UNSPEC" )
   {
       userDataPath = userSpecifiedDataPath;
   }
   
#if LINUX
   if (!snapshotloader.Parse((const char*)&configurationXmlStart, 0,
                             TIXML_ENCODING_UTF8)) {

      throw Surge::Error("Failed to parse the configuration",
                         "Surge failed to initialize");
   }
#else
   string snapshotmenupath = datapath + "configuration.xml";

   if (!snapshotloader.LoadFile(snapshotmenupath.c_str())) // load snapshots (& config-stuff)
   {
      Surge::Error exc("Cannot find 'configuration.xml' in path '" + datapath + "'. Please reinstall surge.",
                       "Surge is not properly installed.");
      Surge::UserInteractions::promptError(exc);
   }
#endif

   TiXmlElement* e = TINYXML_SAFE_TO_ELEMENT(snapshotloader.FirstChild("autometa"));
   if (e)
   {
      defaultname = e->Attribute("name");
      defaultsig = e->Attribute("comment");
   }

   load_midi_controllers();

#if !TARGET_RACK   
   bool loadWtAndPatch = true;
#if TARGET_LV2
   // skip loading during export, it pops up an irrelevant error dialog
   loadWtAndPatch = !skipLoadWtAndPatch;
#endif
   if (loadWtAndPatch)
   {
      refresh_wtlist();
      refresh_patchlist();
   }
#endif
   
   getPatch().scene[0].osc[0].wt.dt = 1.0f / 512.f;
   load_wt(0, &getPatch().scene[0].osc[0].wt);

   // WindowWT is a WaveTable which now has a constructor so don't do this
   // memset(&WindowWT, 0, sizeof(WindowWT));
   if( ! load_wt_wt(datapath + "windows.wt", &WindowWT) )
   {
      std::ostringstream oss;
      oss << "Unable to load '" << datapath << "/windows.wt'. This file is required for surge to operate "
          << "properly. This occurs when Surge is mis-installed and shared resources are not in the "
          << "os-specific shared directory, which on your OS is a directory called 'Surge' in "
#if MAC
          << "the global or user local `Library/Application Support` directory."
#endif
#if WINDOWS
          << "your %LocalAppData% directory."
#endif
#if LINUX
          << "/usr/share or ~/.local/share."
#endif
          << " Please install shared assets correctly and restart.";
      Surge::UserInteractions::promptError(oss.str(), "Unable to load windows.wt");
   }
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

   /*
   ** Do a quick sanity check here - if there are no patches in factory we are mis-installed
   */
   int totalFactory = 0;
   for(auto &cat : patch_category)
       totalFactory += cat.numberOfPatchesInCatgory;
   if(totalFactory == 0)
   {
      std::ostringstream ss;
      ss << "Surge was unable to load factory patches from '" << datapath
         << "'. Surge found 0 factory patches. Please reinstall using the Surge installer.";
      Surge::UserInteractions::promptError(ss.str(),
                                           "Surge Installation Error");

   }

   refreshPatchlistAddDir(false, "patches_3rdparty");
   firstUserCategory = patch_category.size();
   refreshPatchlistAddDir(true, "");

   patchOrdering = std::vector<int>(patch_list.size());
   std::iota(patchOrdering.begin(), patchOrdering.end(), 0);

   auto patchCompare =
      [this](const int &i1, const int &i2) -> bool
      {
         return strnatcasecmp(patch_list[i1].name.c_str(),
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
         return strnatcasecmp(patch_category[i1].name.c_str(),
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
   refreshPatchOrWTListAddDir(
       userDir, subdir, [](std::string s) -> bool { return _stricmp(s.c_str(), ".fxp") == 0; },
       patch_list, patch_category);
}

void SurgeStorage::refreshPatchOrWTListAddDir(bool userDir,
                                              string subdir,
                                              std::function<bool(std::string)> filterOp,
                                              std::vector<Patch>& items,
                                              std::vector<PatchCategory>& categories)
{
   int category = categories.size();

   fs::path patchpath = (userDir ? userDataPath : datapath);
   if (!subdir.empty())
      patchpath.append(subdir);

   if (!fs::is_directory(patchpath))
   {
      return;
   }

   /*
   ** std::filesystem has a recursive_directory_iterator, but between the 
   ** hand rolled ipmmlementation on mac, expermiental on windows, and 
   ** ostensibly standard on linux it isn't consistent enough to warrant
   ** using yet, so build my own recursive directory traversal with a simple
   ** stack
   */
   std::vector<fs::path> alldirs;
   std::deque<fs::path> workStack;
   workStack.push_back( patchpath );
   while (!workStack.empty())
   {
       auto top = workStack.front();
       workStack.pop_front();
       for (auto &d : fs::directory_iterator( top ))
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
   std::vector<PatchCategory> local_categories;
   int patchpathSubstrLength= patchpath.generic_string().size() + 1;
   if (patchpath.generic_string().back() == '/' || patchpath.generic_string().back() == '\\')
       patchpathSubstrLength --;
   
   for (auto &p : alldirs )
   {
      PatchCategory c;
#if WINDOWS && ! TARGET_RACK
      /*
      ** Windows filesystem names are properly wstrings which, if we want them to 
      ** display properly in vstgui, need to be converted to UTF8 using the 
      ** windows widechar API. Linux and Mac do not require this.
      */
      std::wstring str = p.wstring().substr(patchpathSubstrLength);
      c.name = Surge::Storage::wstringToUTF8(str);
#else
      c.name = p.generic_string().substr(patchpathSubstrLength);
#endif
      c.internalid = category;

      c.numberOfPatchesInCatgory = 0;
      for (auto& f : fs::directory_iterator(p))
      {
         std::string xtn = f.path().extension().generic_string();
         if (filterOp(xtn))
         {
            Patch e;
            e.category = category;
            e.path = f.path();
#if WINDOWS && ! TARGET_RACK
              std::wstring str = f.path().filename().wstring();
              str = str.substr(0, str.size() - xtn.length());
              e.name = Surge::Storage::wstringToUTF8(str);
#else
              e.name = f.path().filename().generic_string();
              e.name = e.name.substr(0, e.name.size() - xtn.length());
#endif
              items.push_back(e);

              c.numberOfPatchesInCatgory ++;
         }
      }

      c.numberOfPatchesInCategoryAndChildren = c.numberOfPatchesInCatgory;
      local_categories.push_back(c);
      category++;
   }

   /*
   ** Now establish parent child relationships between patch categories. Do this by
   ** scanning for names; setting the 'root' to everything without a slash
   ** and finding the parent in the name map for everything with a slash
   */

   std::map<std::string,int> nameToLocalIndex;
   int idx=0;
   for (auto& pc : local_categories)
      nameToLocalIndex[pc.name] = idx++;

   std::string pathSep = "/";
#if WINDOWS
   pathSep = "\\";
#endif

   for (auto& pc : local_categories)
   {
       if (pc.name.find(pathSep) == std::string::npos)
       {
           pc.isRoot = true;
       }
       else
       {
           pc.isRoot = false;
           std::string parent = pc.name.substr(0, pc.name.find_last_of(pathSep) );
           local_categories[nameToLocalIndex[parent]].children.push_back(pc);
       }
   }

   /*
   ** We need to sort the local patch category child to make sure subfolders remain
   ** sorted when displayed using the child data structure in the menu view.
   */
   
   auto catCompare =
       [this](const PatchCategory &c1, const PatchCategory &c2) -> bool
       {
           return strnatcasecmp(c1.name.c_str(),c2.name.c_str()) < 0;
       };
   for (auto& pc : local_categories)
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
   std::function<void(PatchCategory&)> recCorrect = [&recCorrect, &nameToLocalIndex,
                                                     &local_categories](PatchCategory& c) {
      local_categories[nameToLocalIndex[c.name]].numberOfPatchesInCategoryAndChildren = c.numberOfPatchesInCatgory;
      for (auto& ckid : c.children)
      {
         recCorrect(local_categories[nameToLocalIndex[ckid.name]]);
         ckid.numberOfPatchesInCategoryAndChildren = local_categories[nameToLocalIndex[ckid.name]].numberOfPatchesInCategoryAndChildren;

         local_categories[nameToLocalIndex[c.name]].numberOfPatchesInCategoryAndChildren +=
             local_categories[nameToLocalIndex[ckid.name]].numberOfPatchesInCategoryAndChildren;
      }
      c.numberOfPatchesInCategoryAndChildren = local_categories[nameToLocalIndex[c.name]].numberOfPatchesInCategoryAndChildren;
   };

   for (auto& c : local_categories)
   {
      if (c.isRoot)
      {
         recCorrect(c);
      }
   }

   /*
   ** Then copy our local patch category onto the member and be done
   */
   for (auto& pc : local_categories)
   {
      categories.push_back(pc);
   }
}

void SurgeStorage::refresh_wtlist()
{
   wt_category.clear();
   wt_list.clear();

   refresh_wtlistAddDir(false, "wavetables");

   if (wt_category.size() == 0 || wt_list.size() == 0)
   {
      std::ostringstream ss;
      ss << "Surge was unable to load wavetables from '" << datapath
         << "'. The directory contains no wavetables. Please reinstall using the Surge installer.";
      Surge::UserInteractions::promptError(ss.str(),
                                           "Surge Installation Error" );
   }

   firstThirdPartyWTCategory = wt_category.size();
   refresh_wtlistAddDir(false, "wavetables_3rdparty");
   firstUserWTCategory = wt_category.size();
   refresh_wtlistAddDir(true, "");

   wtCategoryOrdering = std::vector<int>(wt_category.size());
   std::iota(wtCategoryOrdering.begin(), wtCategoryOrdering.end(), 0);

   // This nonsense deals with the fact that \ < ' ' but ' ' < / and we want "foo bar/h" and "foo/bar" to sort consistently on mac and win.
   // See #1218
   auto categoryCompare = [this](const int& i1, const int& i2) -> bool {
      auto n1 = wt_category[i1].name;
      for( auto i=0; i<n1.length(); ++i)
         if( n1[i] == '\\' )
            n1[i] = '/';
      
      auto n2 = wt_category[i2].name;
      for( auto i=0; i<n2.length(); ++i)
         if( n2[i] == '\\' )
            n2[i] = '/';

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

   auto wtCompare = [this](const int& i1, const int& i2) -> bool {
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

void SurgeStorage::refresh_wtlistAddDir(bool userDir, std::string subdir)
{
   std::vector<std::string> supportedTableFileTypes;
   supportedTableFileTypes.push_back(".wt");
   supportedTableFileTypes.push_back(".wav");

   refreshPatchOrWTListAddDir(
       userDir, subdir,
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
   SurgePatch& patch = getPatch();  //Change here is for performance and ease of debugging, simply not calling getPatch so many times. Code should behave identically.
   for (int sc = 0; sc < 2; sc++)
   {
      for (int o = 0; o < n_oscs; o++)
      {
         if (patch.scene[sc].osc[o].wt.queue_id != -1)
         {
            load_wt(patch.scene[sc].osc[o].wt.queue_id, &patch.scene[sc].osc[o].wt);
            patch.scene[sc].osc[o].wt.refresh_display = true;
         }
         else if (patch.scene[sc].osc[o].wt.queue_filename[0])
         {
            patch.scene[sc].osc[o].queue_type = ot_wavetable;
            patch.scene[sc].osc[o].wt.current_id = -1;
            load_wt(patch.scene[sc].osc[o].wt.queue_filename, &patch.scene[sc].osc[o].wt);
            patch.scene[sc].osc[o].wt.refresh_display = true;
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
   else if (extension.compare(".wav") == 0)
      load_wt_wav_portable(filename, wt);
   else
   {
       std::ostringstream oss;
       oss << "Unable to load file with extension '" << extension << "'. Surge only supports .wav and .wt files";
       Surge::UserInteractions::promptError(oss.str(), "load_wt error" );
   }
}

bool SurgeStorage::load_wt_wt(string filename, Wavetable* wt)
{
   FILE* f = fopen(filename.c_str(), "rb");
   if (!f)
      return false;
   wt_header wh;
   memset(&wh, 0, sizeof(wt_header));

   size_t read = fread(&wh, sizeof(wt_header), 1, f);
   // I'm not sure why this ever worked but it is checking the 4 bytes against vawt so...
   // if (wh.tag != vt_read_int32BE('vawt'))
   if (!(wh.tag[0] == 'v' && wh.tag[1] == 'a' && wh.tag[2] == 'w' && wh.tag[3] == 't'))
   {
      // SOME sort of error reporting is appropriate
      fclose(f);
      return false;
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
   bool wasBuilt = wt->BuildWT(data, wh, false);
   CS_WaveTableData.leave();
   free(data);

   if (!wasBuilt)
   {
       std::ostringstream oss;
       oss << "Your wavetable was unable to build. This often means that it has too many samples or tables."
           << " You provided " << wh.n_tables << " tables of size " << wh.n_samples << " vs max limits of "
           << max_subtables << " tables and " << max_wtable_size << " samples."
           << " In some cases, Surge detects this situation inconsistently leading to this message. Surge is now"
           << " in a potentially inconsistent state. We recommend you restart Surge and do not load the wavetable again."
           << " If you would like, please attach the wavetable which caused this message to a new github issue at "
           << " https://github.com/surge-synthesizer/surge/";
       Surge::UserInteractions::promptError( oss.str(),
                                             "Software Error on WT Load" );
       fclose(f);
       return false;
   }
   fclose(f);
   return true;
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

   // See comment in SurgeSynthesizerIO::incrementPatch and #319
   if( id < 0 || id > n-1 )
   {
       return wtOrdering[0];
   }
   else
   {
       int order = wt_list[id].order;
       
       if (nextPrev)
           order = (order >= (n - 1)) ? 0 : order + 1; // see comment in incrementPatch for that >= vs ==
       else
           order = (order <= 0) ? n - 1 : order - 1;
       
       return wtOrdering[order];
   }
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
   TiXmlElement* e = TINYXML_SAFE_TO_ELEMENT(snapshotloader.FirstChild(name));
   if (e)
      return e;

   // ok, create a new one then
   TiXmlElement ne(name);
   snapshotloader.InsertEndChild(ne);
   return TINYXML_SAFE_TO_ELEMENT(snapshotloader.FirstChild(name));
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

   TiXmlElement* entry = TINYXML_SAFE_TO_ELEMENT(mc->FirstChild("entry"));
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

   TiXmlElement* cc = getSnapshotSection("customctrl");
   assert(cc);

   entry = TINYXML_SAFE_TO_ELEMENT(cc->FirstChild("entry"));
   while (entry)
   {
      int id, ctrl;
      if ((entry->QueryIntAttribute("p", &id) == TIXML_SUCCESS) &&
          (entry->QueryIntAttribute("ctrl", &ctrl) == TIXML_SUCCESS) && (id < n_customcontrollers))
      {
         controllers[id] = ctrl;
      }
      entry = TINYXML_SAFE_TO_ELEMENT(entry->NextSibling("entry"));
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
   isStandardTuning = true;
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

float SurgeStorage::note_to_pitch(float x)
{
   x += 256;
   int e = (int)x;
   float a = x - (float)e;

   if (e > 0x1fe)
      e = 0x1fe;

   return (1 - a) * table_pitch[e & 0x1ff] + a * table_pitch[(e + 1) & 0x1ff];
}

float SurgeStorage::note_to_pitch_inv(float x)
{
   x += 256;
   int e = (int)x;
   float a = x - (float)e;

   if (e > 0x1fe)
      e = 0x1fe;

   return (1 - a) * table_pitch_inv[e & 0x1ff] + a * table_pitch_inv[(e + 1) & 0x1ff];
}

void SurgeStorage::note_to_omega(float x, float& sinu, float& cosi)
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

bool SurgeStorage::retuneToScale(const Surge::Storage::Scale& s)
{
   if (!s.isValid())
      return false;

   currentScale = s;
   isStandardTuning = false;
   
   float pitches[512];
   int pos0 = 256 + scaleConstantNote();
   float pitchMod = log(scaleConstantPitch())/log(2) - 1;
   pitches[pos0] = 1.0;
   for (int i=0; i<512; ++i)
   {
       int distanceFromScale0 = i - pos0;
       
       if( distanceFromScale0 == 0 )
       {
       }
       else 
       {
           int rounds = (distanceFromScale0-1) / s.count;
           int thisRound = (distanceFromScale0-1) % s.count;

           if( thisRound < 0 )
           {
               thisRound += s.count;
               rounds -= 1;
           }
           float mul = pow( s.tones[s.count-1].floatValue, rounds);
           pitches[i] = s.tones[thisRound].floatValue + rounds * (s.tones[s.count - 1].floatValue - 1.0);
           float otp = table_pitch[i];
           table_pitch[i] = pow( 2.0, pitches[i] + pitchMod );

#if DEBUG_SCALES
           if( i > 296 && i < 340 )
               std::cout << "PITCH: i=" << i << " n=" << i - 256 << " r=" << rounds << " t=" << thisRound
                         << " p=" << pitches[i]
                         << " t=" << s.tones[thisRound].floatValue
                         << " tp=" << table_pitch[i]
                         << " otp=" << otp
                         << " diff=" << table_pitch[i] - otp
                         
                   //<< " l2p=" << log(otp)/log(2.0)
                   //<< " l2p-p=" << log(otp)/log(2.0) - pitches[i] - rounds - 3
                         << std::endl;
#endif           
       }
   }
   
   for( int i=0; i<512; ++i )
   {
      table_pitch_inv[i] = 1.f / table_pitch[i];
      table_note_omega[0][i] =
          (float)sin(2 * M_PI * min(0.5, 440 * table_pitch[i] * dsamplerate_os_inv));
      table_note_omega[1][i] =
          (float)cos(2 * M_PI * min(0.5, 440 * table_pitch[i] * dsamplerate_os_inv));

   }

   return true;
}

#if TARGET_LV2
bool SurgeStorage::skipLoadWtAndPatch = false;
#endif

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

#if WINDOWS
std::string wstringToUTF8(const std::wstring &str)
{
    std::string ret;
    int len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), str.length(), NULL, 0, NULL, NULL);
    if (len > 0)
    {
        ret.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, str.c_str(), str.length(), &ret[0], len, NULL, NULL);
    }
    return ret;
}
#endif    
    
} // end ns Surge::Storage
} // end ns Surge

