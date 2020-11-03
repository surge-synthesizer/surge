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

namespace Surge
{
namespace ModulatorPreset
{

const static std::string PresetDir = "Modulator Presets";
const static std::string PresetXtn = ".modpreset";

void savePresetToUser( const fs::path & location, SurgeStorage *s, int scene, int lfoid )
{
   auto lfo = &(s->getPatch().scene[scene].lfo[lfoid]);
   int shapev = lfo->shape.val.i;

   auto containingPath = fs::path{ string_to_path( s->userDataPath ) / fs::path{ PresetDir }};

   if( shapev == ls_mseg )
      containingPath = containingPath / fs::path{ "MSEGs" };
   else if( shapev == ls_stepseq )
      containingPath = containingPath / fs::path{ "StepSequences" };
   else
      containingPath = containingPath / fs::path{ "Lfos" };

   fs::create_directories(containingPath);
   auto fullLocation = fs::path( { containingPath / location }).replace_extension( fs::path{ PresetXtn } );

   TiXmlDeclaration decl("1.0", "UTF-8", "yes");

   TiXmlDocument doc;
   doc.InsertEndChild(decl);

   TiXmlElement lfox( "lfo" );
   lfox.SetAttribute("shape", shapev );

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

      params.InsertEndChild(pn);
   }
   lfox.InsertEndChild(params);

   if( shapev == ls_mseg )
   {
      TiXmlElement ms( "mseg" );
      s->getPatch().msegToXMLElement(&(s->getPatch().msegs[scene][lfoid]), ms );
      lfox.InsertEndChild( ms );
   }
   if( shapev == ls_stepseq )
   {
      TiXmlElement ss( "sequence" );
      s->getPatch().stepSeqToXmlElement(&(s->getPatch().stepsequences[scene][lfoid]), ss, true );
      lfox.InsertEndChild( ss );
   }

   doc.InsertEndChild(lfox);

   if( ! doc.SaveFile(fullLocation) )
   {
      // uhh ... do somethign I guess?
      std::cout << "Could not save" << std::endl;
   }
}

/*
 * Given a compelted path, load the preset into our storage
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

   int shapev = 0;
   if( lfox->QueryIntAttribute("shape", &shapev) != TIXML_SUCCESS )
   {
      std::cout << "Bad shape" << std::endl;
      return;
   }
   lfo->shape.val.i = shapev;

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
         if (valNode->QueryIntAttribute("deform_type", &q) == TIXML_SUCCESS)
            curr->deform_type = q;
      }
   }

   if (shapev == ls_mseg)
   {
      auto msn = lfox->FirstChildElement("mseg");
      if( msn )
         s->getPatch().msegFromXMLElement(&(s->getPatch().msegs[scene][lfoid]), msn);
   }

   if( shapev == ls_stepseq )
   {
      auto msn = lfox->FirstChildElement("sequence");
      if( msn )
         s->getPatch().stepSeqFromXmlElement(&(s->getPatch().stepsequences[scene][lfoid]), msn);
   }
}


std::vector<Category> getPresets( SurgeStorage *s )
{
   // Do a dual directory traversal of factory and user data with the fs::directory_iterator stuff looking for .lfopreset
   auto factoryPath = fs::path( { string_to_path(s->datapath) / fs::path{ PresetDir } } );
   auto userPath = fs::path( { string_to_path(s->userDataPath) / fs::path{ PresetDir } } );

   std::vector<Category> res;

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
            // blah
            auto base = fs::path(d).stem();
            auto fn = fs::path(d).filename();
            if (path_to_string(base) == path_to_string(fn))
            {
               if (currentCategory.presets.size() > 0)
               {
                  std::sort(currentCategory.presets.begin(), currentCategory.presets.end(),
                            [](const Preset& a, const Preset& b) { return a.name < b.name; });
                  res.push_back(currentCategory);
               }
               currentCategoryName = path_to_string(base);
               currentCategory = Category();
               currentCategory.name = currentCategoryName;
               currentCategory.isUserCategory = isU;
               continue;
            }

            Preset p;
            p.name = path_to_string(base);
            p.path = fs::path(d);
            currentCategory.presets.push_back(p);
         }
         if (currentCategory.presets.size() > 0)
         {
            std::sort(currentCategory.presets.begin(), currentCategory.presets.end(),
                      [](const Preset& a, const Preset& b) { return a.name < b.name; } );
            res.push_back(currentCategory);
         }
      }
      catch (const fs::filesystem_error& e)
      {
         // That's OK!
      }
   }
   std::sort( res.begin(), res.end(), [](const Category &a, const Category &b ) {
               if (a.isUserCategory == b.isUserCategory)
                  return a.name < b.name;
               if (a.isUserCategory)
                  return true;
               return false;
               });
    return res;
}
}
}
