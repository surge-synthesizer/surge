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
#include "version.h"

#include "strnatcmp.h"

// FIXME probably remove this when we remove the hardcoded hack below
#include "MSEGModulationHelper.h"
// FIXME

#if __cplusplus < 201703L
constexpr float MSEGStorage::minimumDuration;
#endif

float sinctable alignas(16)[(FIRipol_M + 1) * FIRipol_N * 2];
float sinctable1X alignas(16)[(FIRipol_M + 1) * FIRipol_N];
short sinctableI16 alignas(16)[(FIRipol_M + 1) * FIRipolI16_N];
float table_dB alignas(16)[512],
      table_envrate_lpf alignas(16)[512],
      table_envrate_linear alignas(16)[512],
      table_glide_exp alignas(16)[512],
      table_glide_log alignas(16)[512];
float waveshapers alignas(16)[8][1024];
float samplerate = 0, samplerate_inv;
double dsamplerate, dsamplerate_inv;
double dsamplerate_os, dsamplerate_os_inv;

using namespace std;

#if WINDOWS
void dummyExportedWindowsToLookupDLL() {
}
#endif


SurgeStorage::SurgeStorage(std::string suppliedDataPath) : otherscene_clients(0)
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

   for (int s = 0; s < n_scenes; s++)
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
   temposyncratio_inv = 0.0f; // Use this as a sentinel (since it was not initialized prior to 1.6.5 this was the value at least win and mac had). #1444
   
   songpos = 0;

   for (int i = 0; i < n_customcontrollers; i++)
   {
      controllers[i] = 41 + i;
   }
   for (int i = 0; i < n_modsources; i++)
      modsource_vu[i] = 0.f; // remove?

   for (int s = 0; s < n_scenes; s++)
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
       FSRefMakePath(&foundRef, (UInt8*)path, 1024);
       std::string localpath = path;
       localpath += "/Surge/";

       err = FSFindFolder(kLocalDomain, kApplicationSupportFolderType, false, &foundRef);
       FSRefMakePath(&foundRef, (UInt8*)path, 1024);
       std::string rootpath = path;
       rootpath += "/Surge/";

       struct stat linfo;
       auto lxml = localpath + "configuration.xml";
       int lstat = stat(lxml.c_str(), &linfo);

       struct stat rinfo;
       auto rxml = rootpath + "configuration.xml";
       int rstat = stat(rxml.c_str(), &rinfo);

       // if the local one is here, and either is newer than the root one, or the root one is missing
       if( lstat == 0 && ( linfo.st_mtime > rinfo.st_mtime || rstat != 0 ) )
          datapath = localpath; // use the local
       else
          datapath = rootpath; // else use the root. If both are missing we will blow up later.
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
       std::string localDataPath = std::string(homePath) + "/.local/share/surge/";
       if (xdgDataPath)
       {
           datapath = std::string(xdgDataPath) + "/surge/";
       }
       else if (fs::is_directory(string_to_path(localDataPath)))
       {
           datapath = localDataPath;
       }
       else
       {
          datapath = std::string(homePath) + "/.local/share/Surge/";
       }
       
       /*
       ** If local directory doesn't exists - we probably came here through an installer -
       ** check for /usr/share/surge and use /usr/share/Surge as our last guess
       */
       if (!fs::is_directory(string_to_path(datapath)))
       {
          if (fs::is_directory(string_to_path(std::string(Surge::Build::CMAKE_INSTALL_PREFIX) + "/share/surge")))
          {
             datapath = std::string() + Surge::Build::CMAKE_INSTALL_PREFIX + "/share/surge";
          }
          else if (fs::is_directory(string_to_path(std::string(Surge::Build::CMAKE_INSTALL_PREFIX) + "/share/Surge")))
          {
             datapath = std::string() + Surge::Build::CMAKE_INSTALL_PREFIX + "/share/Surge";
          }
          else
          {
             std::string systemDataPath = "/usr/share/surge/";
             if (fs::is_directory(string_to_path(systemDataPath)))
                datapath = systemDataPath;
             else
                datapath = "/usr/share/Surge/";
          }
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

   if (fs::is_directory(string_to_path(documentsSurge)))
   { 
      userDataPath = documentsSurge;
   }
   else if (fs::is_directory(string_to_path(dotSurge)))
   {
      userDataPath = dotSurge;
   }
   else if (fs::is_directory(string_to_path(documents)))
   {
      userDataPath = documentsSurge;
   }
   else
   {
      userDataPath = dotSurge;
   }
   //std::cout << "DataPath is " << datapath << std::endl;
   //std::cout << "UserDataPath is " << userDataPath << std::endl;
  
#elif WINDOWS
#if TARGET_RACK
   datapath = suppliedDataPath;
#else
   fs::path dllPath;

   // First check the portable mode sitting beside me
   {
      WCHAR pathBuf[MAX_PATH];
      HMODULE hm = NULL;

      if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCTSTR>(&dummyExportedWindowsToLookupDLL), &hm) == 0)
      {
         int ret = GetLastError();
         printf( "GetModuleHandle failed, error = %d\n", ret);
         // Return or however you want to handle an error.
         goto bailOnPortable;
      }
      if (GetModuleFileName(hm, pathBuf, std::size(pathBuf)) == 0)
      {
         int ret = GetLastError();
         printf( "GetModuleFileName failed, error = %d\n", ret);
         // Return or however you want to handle an error.
         goto bailOnPortable;
      }

      // The pathBuf variable should now contain the full filepath for this DLL.
      fs::path path(pathBuf);
      path.remove_filename();
      dllPath = path;
      path /= L"SurgeData";
      if (fs::is_directory(path))
      {
         datapath = path_to_string(path);
      }
   }
bailOnPortable:

   if (datapath.empty())
   {
      PWSTR commonAppData;
      if (!SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &commonAppData))
      {
         fs::path path(commonAppData);
         path /= L"Surge";
         if (fs::is_directory(path))
         {
            datapath = path_to_string(path);
         }
      }
   }

   if (datapath.empty())
   {
      PWSTR localAppData;
      if (!SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData))
      {
         fs::path path(localAppData);
         path /= L"Surge";
         datapath = path_to_string(path);
      }
   }

   // Portable - first check for dllPath\\SurgeUserData
   if (!dllPath.empty() && fs::is_directory(dllPath / L"SurgeUserData"))
   {
      userDataPath = path_to_string(dllPath / L"SurgeUserData");
   }
   else 
   {
      PWSTR documentsFolder;
      if (!SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &documentsFolder))
      {
         fs::path path(documentsFolder);
         path /= L"Surge";
         userDataPath = path_to_string(path);
      }
   }
#endif
#endif

   userDefaultFilePath = userDataPath;
   
   std::string userSpecifiedDataPath = Surge::Storage::getUserDefaultValue(this, "userDataPath", "UNSPEC" );
   if( userSpecifiedDataPath != "UNSPEC" )
   {
       userDataPath = userSpecifiedDataPath;
   }

   // append separator if not present
   datapath = Surge::Storage::appendDirectory(datapath, std::string());

   userFXPath = Surge::Storage::appendDirectory(userDataPath, "FXSettings");
   
   userMidiMappingsPath = Surge::Storage::appendDirectory(userDataPath, "MIDIMappings");
   
#if LINUX
   if (!snapshotloader.Parse((const char*)&configurationXmlStart, 0,
                             TIXML_ENCODING_UTF8)) {

      throw Surge::Error("Failed to parse the configuration",
                         "Surge failed to initialize");
   }
#else
   const auto snapshotmenupath{string_to_path(datapath + "configuration.xml")};

   if (!snapshotloader.LoadFile(snapshotmenupath)) // load snapshots (& config-stuff)
   {
      Surge::Error exc("Cannot find 'configuration.xml' in path '" + datapath + "'. Please reinstall surge.",
                       "Surge is not properly installed.");
      Surge::UserInteractions::promptError(exc);
   }
#endif

   load_midi_controllers();

   bool loadWtAndPatch = true;

#if !TARGET_RACK   
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
   load_wt(0, &getPatch().scene[0].osc[0].wt, &getPatch().scene[0].osc[0]);

   // WindowWT is a WaveTable which now has a constructor so don't do this
   // memset(&WindowWT, 0, sizeof(WindowWT));
   if( loadWtAndPatch && ! load_wt_wt(datapath + "windows.wt", &WindowWT) )
   {
      WindowWT.size = 0;
      std::ostringstream oss;
      oss << "Unable to load '" << datapath << "/windows.wt'. This file is required for Surge to work "
          << "properly. This occurs when Surge is incorrectly installed and its resources are not found at "
#if MAC
          << "the global or local user Library/Application Support/Surge directory."
#endif
#if WINDOWS
          << "%ProgramData%\\Surge directory."
#endif
#if LINUX
          << "/usr/share/Surge or ~/.local/share/Surge."
#endif
          << " Please reinstall Surge and try again!";
      Surge::UserInteractions::promptError(oss.str(), "Surge Resources Loading Error");
   }

   // Tunings Library Support
   currentScale = Tunings::evenTemperament12NoteScale();
   currentMapping = Tunings::KeyboardMapping();

   // Load the XML DocStrings if we are loading startup data
   if( loadWtAndPatch )
   {
      auto dsf = string_to_path(datapath + "paramdocumentation.xml");
      TiXmlDocument doc;
      if( ! doc.LoadFile(dsf) || doc.Error() )
      {
         std::cout << "Unable to load  '" << dsf << "'!"
                   << std::endl;
         std::cout << "Unable to parse!\nError is:\n"
                   << doc.ErrorDesc() << " at row " << doc.ErrorRow() << ", column " << doc.ErrorCol()
                   << std::endl;
      }
      else
      {
         TiXmlElement* pdoc = TINYXML_SAFE_TO_ELEMENT(doc.FirstChild("param-doc"));
         if( ! pdoc )
         {
            Surge::UserInteractions::promptError( "Unknown top element in paramdocumentation.xml - not a parameter documentation XML file!", "Error" );
         }
         else
         {
            for( auto pchild = pdoc->FirstChildElement(); pchild; pchild = pchild->NextSiblingElement() )
            {
               if( strcmp( pchild->Value(),"ctrl_group" ) == 0 )
               {
                  int g = 0;
                  if( pchild->QueryIntAttribute("group", &g ) == TIXML_SUCCESS )
                  {
                     std::string help_url = pchild->Attribute( "help_url" );
                     if( help_url.size() > 0 )
                        helpURL_controlgroup[g] = help_url;
                  }
               }
               else if( strcmp( pchild->Value(), "param" ) == 0 )
               {
                  std::string id = pchild->Attribute( "id" );
                  std::string help_url = pchild->Attribute( "help_url" );
                  int t = 0;
                  if( help_url.size() > 0 )
                  {
                     if( pchild->QueryIntAttribute( "type", &t ) == TIXML_SUCCESS )
                     {
                        helpURL_paramidentifier_typespecialized[std::make_pair(id, t)] = help_url;
                     }
                     else
                     {
                        helpURL_paramidentifier[id] = help_url;
                  }
                  }
               }
               else if( strcmp( pchild->Value(), "special" ) == 0 )
               {
                  std::string id = pchild->Attribute( "id" );
                  std::string help_url = pchild->Attribute( "help_url" );
                  if( help_url.size() > 0 )
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
   }


   for( int s = 0; s < n_scenes; ++s )
   {
      for( int i = 0; i < n_lfos; ++i )
      {
         auto ms = &(_patch->msegs[s][i]);
         ms->n_activeSegments = 4;
         ms->segments[0].duration = 0.25;
         ms->segments[0].type = MSEGStorage::segment::LINEAR;
         ms->segments[0].v0 = 0.0;

         ms->segments[1].duration = 0.25;
         ms->segments[1].type = MSEGStorage::segment::LINEAR;
         ms->segments[1].v0 = 1.0;

         ms->segments[2].duration = MSEGStorage::minimumDuration;
         ms->segments[2].type = MSEGStorage::segment::LINEAR;
         ms->segments[2].v0 = 0.7;

         ms->segments[3].duration = 0.5;
         ms->segments[3].type = MSEGStorage::segment::SCURVE;
         ms->segments[3].v0 = -0.7;
         ms->segments[3].nv1 = 0; // Have to set this now due to endpoint mode
         ms->segments[3].cpduration = 0.4;
         ms->segments[3].cpv = -0.3;
         Surge::MSEG::rebuildCache( ms );
      }
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
         << "'. Please reinstall Surge!";
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

   fs::path patchpath = string_to_path(userDir ? userDataPath : datapath);
   if (!subdir.empty())
      patchpath /= string_to_path(subdir);

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
   auto patchpathStr(path_to_string(patchpath));
   auto patchpathSubstrLength = patchpathStr.size() + 1;
   if (patchpathStr.back() == '/' || patchpathStr.back() == '\\')
      patchpathSubstrLength--;
   
   for (auto &p : alldirs )
   {
      PatchCategory c;
      c.name = path_to_string(p).substr(patchpathSubstrLength);
      c.internalid = category;

      c.numberOfPatchesInCatgory = 0;
      for (auto& f : fs::directory_iterator(p))
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

   /*
   ** Now establish parent child relationships between patch categories. Do this by
   ** scanning for names; setting the 'root' to everything without a slash
   ** and finding the parent in the name map for everything with a slash
   */

   std::map<std::string,int> nameToLocalIndex;
   int idx=0;
   for (auto& pc : local_categories)
      nameToLocalIndex[pc.name] = idx++;

   for (auto& pc : local_categories)
   {
       if (pc.name.find(PATH_SEPARATOR) == std::string::npos)
       {
           pc.isRoot = true;
       }
       else
       {
           pc.isRoot = false;
           std::string parent = pc.name.substr(0, pc.name.find_last_of(PATH_SEPARATOR) );
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
         << "'. Please reinstall Surge!";
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
   for (int sc = 0; sc < n_scenes; sc++)
   {
      for (int o = 0; o < n_oscs; o++)
      {
         if (patch.scene[sc].osc[o].wt.queue_id != -1)
         {
            load_wt(patch.scene[sc].osc[o].wt.queue_id, &patch.scene[sc].osc[o].wt, &patch.scene[sc].osc[o]);
            patch.scene[sc].osc[o].wt.refresh_display = true;
         }
         else if (patch.scene[sc].osc[o].wt.queue_filename[0])
         {
            patch.scene[sc].osc[o].queue_type = ot_wavetable;
            patch.scene[sc].osc[o].wt.current_id = -1;
            load_wt(patch.scene[sc].osc[o].wt.queue_filename, &patch.scene[sc].osc[o].wt, &patch.scene[sc].osc[o]);
            patch.scene[sc].osc[o].wt.refresh_display = true;
         }
      }
   }
}

void SurgeStorage::load_wt(int id, Wavetable* wt, OscillatorStorage *osc)
{
   wt->current_id = id;
   wt->queue_id = -1;
   if (id < 0)
      return;
   if (id >= wt_list.size())
      return;
   if (!wt)
      return;

   load_wt(path_to_string(wt_list[id].path), wt, osc);

   if( osc )
   {
      auto n = wt_list.at(id).name;
      strncpy( osc->wavetable_display_name, n.c_str(), 256 );
   }
}

void SurgeStorage::load_wt(string filename, Wavetable* wt, OscillatorStorage *osc)
{
   if( osc )
   {
      char sep = PATH_SEPARATOR;
      auto fn = filename.substr(filename.find_last_of(sep) + 1, filename.npos);
      std::string fnnoext = fn.substr( 0, fn.find_last_of('.' ) );
      
      if( fnnoext.length() > 0 )
      {
         strncpy( osc->wavetable_display_name, fnnoext.c_str(), 256 );
      }
   }
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
       oss << "Unable to load file with extension " << extension << "! Surge only supports .wav and .wt wavetable files!";
       Surge::UserInteractions::promptError(oss.str(), "Error" );
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
   read = fread(data, 1, ds, f);
   // FIXME - error if read != ds

   waveTableDataMutex.lock();
   bool wasBuilt = wt->BuildWT(data, wh, false);
   waveTableDataMutex.unlock();
   free(data);

   if (!wasBuilt)
   {
       std::ostringstream oss;
       oss << "Wavetable could not be built, which means it has too many samples or frames."
           << " You provided " << wh.n_tables << " frames of " << wh.n_samples << "samples, while limit is "
           << max_subtables << " frames and " << max_wtable_size << " samples."
           << " In some cases, Surge detects this situation inconsistently. Surge is now in a potentially "
           << " inconsistent state. It is recommended to restart Surge and not load the problematic wavetable again."
           << " If you would like, please attach the wavetable which caused this message to a new GitHub issue at "
           << " https://github.com/surge-synthesizer/surge/";
       Surge::UserInteractions::promptError( oss.str(),
                                             "Wavetable Loading Error" );
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
      if (getPatch().scene[scene].lfo[entry].shape.val.i == lt_stepseq)
         memcpy(&clipboard_stepsequences[0], &getPatch().stepsequences[scene][entry],
                sizeof(StepSequencerStorage));
      if (getPatch().scene[scene].lfo[entry].shape.val.i == lt_mseg)
         clipboard_msegs[0] = getPatch().msegs[scene][entry];
      break;
   case cp_scene:
   {
      includemod = true;
      includeall = true;
      id = getPatch().scene[scene].octave.id;
      for (int i = 0; i < n_lfos; i++)
      {
         memcpy(&clipboard_stepsequences[i], &getPatch().stepsequences[scene][i],
                sizeof(StepSequencerStorage));
         clipboard_msegs[i] = getPatch().msegs[scene][i];
      }
      for (int i = 0; i < n_oscs; i++)
      {
         clipboard_wt[i].Copy(&getPatch().scene[scene].osc[i].wt);
      }
   }
   break;
   default:
      return;
   }

   modRoutingMutex.lock();
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
   modRoutingMutex.unlock();
}

void SurgeStorage::clipboard_paste(int type, int scene, int entry)
{
   assert(scene < n_scenes);
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
      {
         memcpy(&getPatch().stepsequences[scene][i], &clipboard_stepsequences[i],
                sizeof(StepSequencerStorage));
         getPatch().msegs[scene][i] = clipboard_msegs[i];
      }
      for (int i = 0; i < n_oscs; i++)
      {
         getPatch().scene[scene].osc[i].wt.Copy(&clipboard_wt[i]);
      }
   }
   break;
   default:
      return;
   }

   modRoutingMutex.lock();
   {

      for (int i = start; i < n; i++)
      {
         Parameter p = clipboard_p[i];
         int pid = p.id + id;
         getPatch().param_ptr[pid]->val.i = p.val.i;
         getPatch().param_ptr[pid]->temposync = p.temposync;
         getPatch().param_ptr[pid]->extend_range = p.extend_range;
         getPatch().param_ptr[pid]->deactivated = p.deactivated;
         getPatch().param_ptr[pid]->porta_constrate = p.porta_constrate;
         getPatch().param_ptr[pid]->porta_gliss = p.porta_gliss;
         getPatch().param_ptr[pid]->porta_retrigger = p.porta_retrigger;
         getPatch().param_ptr[pid]->porta_curve = p.porta_curve;
         getPatch().param_ptr[pid]->deform_type = p.deform_type;
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
         if (getPatch().scene[scene].lfo[entry].shape.val.i == lt_stepseq)
            memcpy(&getPatch().stepsequences[scene][entry], &clipboard_stepsequences[0],
                   sizeof(StepSequencerStorage));
         if (getPatch().scene[scene].lfo[entry].shape.val.i == lt_mseg)
            getPatch().msegs[scene][entry] = clipboard_msegs[0];

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

   modRoutingMutex.unlock();
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
   float _512th = 1.f / 512.f;
   for (int i = 0; i < 512; i++)
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

float SurgeStorage::note_to_pitch_ignoring_tuning(float x)
{
   x += 256;
   int e = (int)x;
   float a = x - (float)e;

   if (e > 0x1fe)
      e = 0x1fe;

   return (1 - a) * table_pitch_ignoring_tuning[e & 0x1ff] + a * table_pitch_ignoring_tuning[(e + 1) & 0x1ff];
}

float SurgeStorage::note_to_pitch_inv_ignoring_tuning(float x)
{
   x += 256;
   int e = (int)x;
   float a = x - (float)e;

   if (e > 0x1fe)
      e = 0x1fe;

   return (1 - a) * table_pitch_inv_ignoring_tuning[e & 0x1ff] + a * table_pitch_inv_ignoring_tuning[(e + 1) & 0x1ff];
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

void SurgeStorage::note_to_omega_ignoring_tuning(float x, float& sinu, float& cosi)
{
   x += 256;
   int e = (int)x;
   float a = x - (float)e;

   if (e > 0x1fe)
      e = 0x1fe;
   else if (e < 0)
      e = 0;

   sinu = (1 - a) * table_note_omega_ignoring_tuning[0][e & 0x1ff] + a * table_note_omega_ignoring_tuning[0][(e + 1) & 0x1ff];
   cosi = (1 - a) * table_note_omega_ignoring_tuning[1][e & 0x1ff] + a * table_note_omega_ignoring_tuning[1][(e + 1) & 0x1ff];
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

float envelope_rate_linear_nowrap(float x)
{
   x *= 16.f;
   x += 256.f;
   int e = limit_range( (int)x, 0, 0x1ff - 1 );;
   float a = x - (float)e;

   return (1 - a) * table_envrate_linear[e & 0x1ff] + a * table_envrate_linear[(e + 1) & 0x1ff];
}

// this function is only valid for x = {0, 1}
float glide_exp(float x)
{
   x *= 511.f;
   int e = (int)x;
   float a = x - (float)e;

   return (1 - a) * table_glide_exp[e & 0x1ff] + a * table_glide_exp[(e + 1) & 0x1ff];
}

// this function is only valid for x = {0, 1}
float glide_log(float x)
{
   x *= 511.f;
   int e = (int)x;
   float a = x - (float)e;

   return (1 - a) * table_glide_log[e & 0x1ff] + a * table_glide_log[(e + 1) & 0x1ff];
}

bool SurgeStorage::retuneToScale(const Tunings::Scale& s)
{
   currentScale = s;
   isStandardTuning = false;

   Tunings::Tuning t(currentScale, currentMapping);
   
   for( int i=0; i<512; ++i )
   {
      table_pitch[i] = t.frequencyForMidiNoteScaledByMidi0( i - 256 );
      table_pitch_inv[i] = 1.f / table_pitch[i];
      table_note_omega[0][i] =
          (float)sin(2 * M_PI * min(0.5, 440 * table_pitch[i] * dsamplerate_os_inv));
      table_note_omega[1][i] =
          (float)cos(2 * M_PI * min(0.5, 440 * table_pitch[i] * dsamplerate_os_inv));

   }
   
   return true;
}

bool SurgeStorage::remapToStandardKeyboard()
{
   auto k = Tunings::KeyboardMapping();
   currentMapping = k;
   isStandardMapping = true;
   tuningPitch = 32.0;
   tuningPitchInv = 1.0 / 32.0;
   if( isStandardTuning )
   {
      retuneToStandardTuning();
   }
   else
   {
      retuneToScale(currentScale);
   }
   return true;
}

bool SurgeStorage::remapToKeyboard(const Tunings::KeyboardMapping& k)
{
   currentMapping = k;
   isStandardMapping = false;

   tuningPitch = k.tuningFrequency / Tunings::MIDI_0_FREQ;
   tuningPitchInv = 1.0 / tuningPitch;
   retuneToScale(currentScale);
   return true;
}

#if TARGET_LV2
bool SurgeStorage::skipLoadWtAndPatch = false;
#endif

void SurgeStorage::rescanUserMidiMappings()
{
   userMidiMappingsXMLByName.clear();
   std::error_code ec;
   const auto extension{fs::path{".srgmid"}.native()};
   for (const fs::path& d : fs::directory_iterator{string_to_path(userMidiMappingsPath), ec})
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

void SurgeStorage::loadMidiMappingByName(std::string name)
{
   if( userMidiMappingsXMLByName.find(name) == userMidiMappingsXMLByName.end() )
   {
      // FIXME - why would this ever happen? Probably show an error
      return;
   }

   auto doc = userMidiMappingsXMLByName[name];
   auto sm = TINYXML_SAFE_TO_ELEMENT(doc.FirstChild( "surge-midi" ) );
   // We can do revisio nstuff here later if we need to
   if( ! sm )
   {
      // Invalid XML Document. Show an error?
      Surge::UserInteractions::promptError( "Unable to locate surge-midi element in XML. Not a valid MIDI mapping!", "Surge MIDI" );
      return;
   }

   auto mc = TINYXML_SAFE_TO_ELEMENT(sm->FirstChild("midictrl"));

   if( mc )
   {
      // Clear the current control mapping
      for (int i = 0; i < n_total_params; i++)
      {
         getPatch().param_ptr[i]->midictrl = -1;
      }

      // Apply the new control mapping
      
      auto map = mc->FirstChildElement("map");
      while( map )
      {
         int i, c;
         if( map->QueryIntAttribute("p", &i ) == TIXML_SUCCESS &&
             map->QueryIntAttribute( "cc", &c ) == TIXML_SUCCESS )
         {
            getPatch().param_ptr[i]->midictrl = c;
            if( i >= n_global_params )
            {
               getPatch().param_ptr[i + n_scene_params]->midictrl = c;
            }
         }
         map = map->NextSiblingElement("map");
      }
   }

   auto cc = TINYXML_SAFE_TO_ELEMENT(sm->FirstChild("customctrl"));
   if( cc )
   {
      auto ctrl = cc->FirstChildElement("ctrl" );
      while( ctrl )
      {
         int i, cc;
         if( ctrl->QueryIntAttribute("i", &i ) == TIXML_SUCCESS &&
             ctrl->QueryIntAttribute("cc", &cc ) == TIXML_SUCCESS )
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
   TiXmlElement sm("surge-midi" );
   sm.SetAttribute( "revision", ff_revision );
   sm.SetAttribute( "name", name );

   // Build the XML here
   int n = n_global_params + n_scene_params; // only store midictrl's for scene A (scene A -> scene
                                             // B will be duplicated on load)
   TiXmlElement mc( "midictrl" );
   for (int i = 0; i < n; i++)
   {
      if (getPatch().param_ptr[i]->midictrl >= 0)
      {
         TiXmlElement p( "map" );
         p.SetAttribute( "p", i );
         p.SetAttribute( "cc", getPatch().param_ptr[i]->midictrl );
         mc.InsertEndChild( p );
      }
   }
   sm.InsertEndChild(mc);

   TiXmlElement cc( "customctrl" );
   for (int i=0; i<n_customcontrollers; ++i )
   {
      TiXmlElement p( "ctrl" );
      p.SetAttribute( "i", i );
      p.SetAttribute( "cc", controllers[i] );
      cc.InsertEndChild( p );
   }
   sm.InsertEndChild(cc);

   doc.InsertEndChild( sm );

   fs::create_directories(string_to_path(userMidiMappingsPath));
   std::string fn = Surge::Storage::appendDirectory(userMidiMappingsPath, name + ".srgmid");

   if (!doc.SaveFile(string_to_path(fn)))
   {
      std::ostringstream oss;
      oss << "Unable to save MIDI settings to '" << fn << "'!";
      Surge::UserInteractions::promptError( oss.str(), "Error" );
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

bool isValidUTF8(const std::string &testThis )
{
   int pos = 0;
   const char* data = testThis.c_str();

   // https://helloacm.com/how-to-validate-utf-8-encoding-the-simple-utf-8-validation-algorithm/
   // Valid UTF8 has a specific binary format. If it's a single byte UTF8 character, then it is
   // always of form '0xxxxxxx', where 'x' is any binary digit. If it's a two byte UTF8 character, then it's always of form
   // '110xxxxx10xxxxxx'. Similarly for three and four byte UTF8 characters it starts with '1110xxxx' and '11110xxx' followed by
   // '10xxxxxx' one less times as there are bytes. This tool will locate mistakes in the encoding and tell you where they occured.

   //    https://helloacm.com/how-to-validate-utf-8-encoding-the-simple-utf-8-validation-algorithm/

   auto is10x = [](int a) {
                   int bit1 = (a >> 7) & 1;
                   int bit2 = (a >> 6) & 1;
                   return (bit1 == 1) && (bit2 == 0);
                };
   size_t dsz = testThis.size();
   for (int i = 0; i < dsz; ++ i) {
      // 0xxxxxxx
      int bit1 = (data[i] >> 7) & 1;
      if (bit1 == 0) continue;
      // 110xxxxx 10xxxxxx
      int bit2 = (data[i] >> 6) & 1;
      if (bit2 == 0) return false;
      // 11
      int bit3 = (data[i] >> 5) & 1;
      if (bit3 == 0) {
         // 110xxxxx 10xxxxxx
         if ((++ i) < dsz) {
            if (is10x(data[i])) {
               continue;
            }
            return false;
         } else {
            return false;
         }
      }                        
      int bit4 = (data[i] >> 4) & 1;
      if (bit4 == 0) {
         // 1110xxxx 10xxxxxx 10xxxxxx
         if (i + 2 < dsz) {
            if (is10x(data[i + 1]) && is10x(data[i + 2])) {
               i += 2;
               continue;
            }
            return false;
         } else {
            return false;
         }                
      }            
      int bit5 = (data[i] >> 3) & 1;
      if (bit5 == 1) return false;
      if (i + 3 < dsz) {
         if (is10x(data[i + 1]) && is10x(data[i + 2]) && is10x(data[i + 3])) {
            i += 3;
            continue;
         }
         return false;
      } else {
         return false;
      }                            
   }
   return true;
}

string findReplaceSubstring(string& source, const string& from, const string& to)
{
   string newString;
   newString.reserve(source.length()); // avoids a few memory allocations

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

string makeStringVertical(string& source)
{
   std::ostringstream oss;
   std::string pre = "";

   for (auto c : source)
   {
      oss << pre << c;
      pre = "\n";
   }
   return oss.str();
}

std::string appendDirectory( const std::string &root, const std::string &path1 )
{
   if (root[root.size()-1] == PATH_SEPARATOR)
      return root + path1;
   else
      return root + PATH_SEPARATOR + path1;
}
std::string appendDirectory( const std::string &root, const std::string &path1, const std::string &path2 )
{
   return appendDirectory( appendDirectory( root, path1 ), path2 );
}
std::string appendDirectory( const std::string &root, const std::string &path1, const std::string &path2, const std::string &path3 )
{
   return appendDirectory( appendDirectory( root, path1, path2 ), path3 );
}


} // end ns Surge::Storage
} // end ns Surge

