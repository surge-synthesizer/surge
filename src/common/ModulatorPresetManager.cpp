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


#include "ModulatorPresetManager.h"
#include <iostream>
#include "DebugHelpers.h"
#include "SurgeStorage.h"
#include "tinyxml/tinyxml.h"
#include "DebugHelpers.h"
#include "strnatcmp.h"

namespace Surge
{
namespace ModulatorPreset
{

const static std::string PresetDir = "Modulator Presets";
const static std::string PresetXtn = ".modpreset";

void savePresetToUser( const fs::path & location, SurgeStorage *s, int scene, int lfoid )
{
   auto lfo = &(s->getPatch().scene[scene].lfo[lfoid]);
   int lfotype = lfo->shape.val.i;

   auto containingPath = fs::path{ string_to_path( s->userDataPath ) / fs::path{ PresetDir }};

   if( lfotype == lt_mseg )
      containingPath = containingPath / fs::path{ "MSEG" };
   else if( lfotype == lt_stepseq )
      containingPath = containingPath / fs::path{ "Step Seq" };
   else
      containingPath = containingPath / fs::path{ "LFO" };

   fs::create_directories(containingPath);
   auto fullLocation = fs::path( { containingPath / location }).replace_extension( fs::path{ PresetXtn } );

   TiXmlDeclaration decl("1.0", "UTF-8", "yes");

   TiXmlDocument doc;
   doc.InsertEndChild(decl);

   TiXmlElement lfox( "lfo" );
   lfox.SetAttribute("shape", lfotype );

   TiXmlElement params( "params" );
   for( auto curr = &(lfo->rate); curr <= &(lfo->release); ++curr )
   {
      // shape is odd
      if( curr == & ( lfo->shape ) )
      {
         continue;
      }

      // OK the internal name has "lfo7_" at the top or what not. We need this
      // loadable into any LFO so...
      std::string in( curr->get_internal_name());
      auto p = in.find("_");
      in = in.substr(p+1);
      TiXmlElement pn( in );


      if( curr->valtype == vt_float )
         pn.SetDoubleAttribute("v", curr->val.f );
      else
         pn.SetAttribute( "i", curr->val.i );
      pn.SetAttribute( "temposync", curr->temposync );
      pn.SetAttribute( "deform_type", curr->deform_type );
      pn.SetAttribute( "extend_range", curr->extend_range );
      pn.SetAttribute( "deactivated", curr->deactivated );

      params.InsertEndChild(pn);
   }
   lfox.InsertEndChild(params);

   if( lfotype == lt_mseg )
   {
      TiXmlElement ms( "mseg" );
      s->getPatch().msegToXMLElement(&(s->getPatch().msegs[scene][lfoid]), ms );
      lfox.InsertEndChild( ms );
   }
   if( lfotype == lt_stepseq )
   {
      TiXmlElement ss( "sequence" );
      s->getPatch().stepSeqToXmlElement(&(s->getPatch().stepsequences[scene][lfoid]), ss, true );
      lfox.InsertEndChild( ss );
   }

   doc.InsertEndChild(lfox);

   if( ! doc.SaveFile(fullLocation) )
   {
      // uhh ... do something I guess?
      std::cout << "Could not save" << std::endl;
   }

   forcePresetRescan();
}

/*
 * Given a completed path, load the preset into our storage
 */
void loadPresetFrom( const fs::path &location, SurgeStorage *s, int scene, int lfoid )
{
   auto lfo = &(s->getPatch().scene[scene].lfo[lfoid]);
   // ToDo: Inverse of above
   TiXmlDocument doc;
   doc.LoadFile(location);
   auto lfox = TINYXML_SAFE_TO_ELEMENT(doc.FirstChildElement("lfo"));
   if (!lfox)
   {
      std::cout << "Unable to find LFO node in document" << std::endl;
      return;
   }

   int lfotype = 0;
   if( lfox->QueryIntAttribute("shape", &lfotype) != TIXML_SUCCESS )
   {
      std::cout << "Bad shape" << std::endl;
      return;
   }
   lfo->shape.val.i = lfotype;

   auto params = TINYXML_SAFE_TO_ELEMENT(lfox->FirstChildElement("params"));
   if (!params)
   {
      std::cout << "NO PARAMS" << std::endl;
      return;
   }
   for( auto curr = &(lfo->rate); curr <= &(lfo->release); ++curr )
   {
      // shape is odd
      if (curr == &(lfo->shape))
      {
         continue;
      }

      // OK the internal name has "lfo7_" at the top or what not. We need this
      // loadable into any LFO so...
      std::string in(curr->get_internal_name());
      auto p = in.find("_");
      in = in.substr(p + 1);
      auto valNode = params->FirstChildElement(in.c_str());
      if (valNode)
      {
         double v;
         int q;
         if( curr->valtype == vt_float )
         {
            if (valNode->QueryDoubleAttribute("v", &v) == TIXML_SUCCESS)
            {
               curr->val.f = v;
            }
         }
         else
         {
            if( valNode->QueryIntAttribute("i", &q) == TIXML_SUCCESS )
            {
               curr->val.i = q;
            }
         }



         if (valNode->QueryIntAttribute("temposync", &q) == TIXML_SUCCESS)
            curr->temposync = q;
         else
            curr->temposync = false;
         if (valNode->QueryIntAttribute("deform_type", &q) == TIXML_SUCCESS)
            curr->deform_type = q;
         else
            curr->deform_type = 0;
         if (valNode->QueryIntAttribute("extend_range", &q ) == TIXML_SUCCESS)
            curr->extend_range = q;
         else
            curr->extend_range = false;
         if( valNode->QueryIntAttribute( "deactivated", &q ) == TIXML_SUCCESS && curr->can_deactivate() )
            curr->deactivated = q;
      }
   }

   if (lfotype == lt_mseg)
   {
      auto msn = lfox->FirstChildElement("mseg");
      if( msn )
         s->getPatch().msegFromXMLElement(&(s->getPatch().msegs[scene][lfoid]), msn, true);
   }

   if( lfotype == lt_stepseq )
   {
      auto msn = lfox->FirstChildElement("sequence");
      if( msn )
         s->getPatch().stepSeqFromXmlElement(&(s->getPatch().stepsequences[scene][lfoid]), msn);
   }
}

static std::vector<Category> scanedPresets;
static bool haveScanedPresets = false;

std::vector<Category> getPresets( SurgeStorage *s )
{
   if (haveScanedPresets)
      return scanedPresets;

   // Do a dual directory traversal of factory and user data with the fs::directory_iterator stuff looking for .lfopreset
   auto factoryPath = fs::path( { string_to_path(s->datapath) / fs::path{ "modulator_presets" } } );
   auto userPath = fs::path( { string_to_path(s->userDataPath) / fs::path{ PresetDir } } );

   std::map<std::string, Category> resMap; // handy it is sorted!

   for( int i=0; i<2; ++i )
   {
      auto p = (i ? userPath : factoryPath);
      bool isU = i;
      try
      {
         std::string currentCategoryName = "";
         Category currentCategory;
         for (auto& d : fs::recursive_directory_iterator(p))
         {
            auto dp = fs::path(d);
            auto base = dp.stem();
            auto fn = dp.filename();
            auto ext = dp.extension();
            if (path_to_string(ext) != ".modpreset")
            {
               continue;
            }
            auto rd = path_to_string(dp.replace_filename(fs::path()));
            rd = rd.substr(path_to_string(p).length() + 1);
            rd = rd.substr(0, rd.length() - 1);

            auto catName = rd;
            auto ppos = rd.rfind(fs::path::preferred_separator);
            auto pd = std::string();
            if (ppos != std::string::npos)
            {
               pd = rd.substr(0, ppos);
               catName = rd.substr(ppos + 1);
            }
            if (resMap.find(rd) == resMap.end())
            {
               resMap[rd] = Category();
               resMap[rd].name = catName;
               resMap[rd].parentPath = pd;
               resMap[rd].path = rd;

               /*
                * We only create categories if we find a preset. So that means parent directories
                * with just subdirs need categories made. This recurses up as far as we need to go
                */
               while (pd != "" && resMap.find(pd) == resMap.end())
               {
                  auto cd = pd;
                  catName = cd;
                  ppos = cd.rfind(fs::path::preferred_separator);

                  if (ppos != std::string::npos)
                  {
                     pd = cd.substr(0, ppos);
                     catName = cd.substr(ppos + 1);
                  }
                  else
                  {
                     pd = "";
                  }

                  resMap[cd] = Category();
                  resMap[cd].name = catName;
                  resMap[cd].parentPath = pd;
                  resMap[cd].path = cd;
               }
            }

            Preset prs;
            prs.name = path_to_string(base);
            prs.path = fs::path(d);
            resMap[rd].presets.push_back(prs);
         }
      }
      catch (const fs::filesystem_error& e)
      {
         // That's OK!
      }
   }

   std::vector<Category> res;
   for (auto& m : resMap)
   {
      std::sort(m.second.presets.begin(), m.second.presets.end(),
                [](const Preset& a, const Preset& b) {
                   return strnatcasecmp(a.name.c_str(), b.name.c_str()) < 0;
                });

      res.push_back(m.second);
   }
   scanedPresets = res;
   haveScanedPresets = true;
   return res;
}

void forcePresetRescan()
{
   haveScanedPresets = false;
   scanedPresets.clear();
}
}
}
