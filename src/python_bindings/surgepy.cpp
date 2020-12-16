#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <thread>

#include "SurgeSynthesizer.h"
#include "HeadlessPluginLayerProxy.h"
#include "version.h"

namespace py = pybind11;

static std::unique_ptr<HeadlessPluginLayerProxy> spysetup_parent = nullptr;
static std::mutex spysetup_mutex;

/*
 * The way we've decided to expose to python is through some wrapper objects
 * which give us the control gorup / control group entry / param heirarchy.
 * So here's some small helper objects
 */

struct SurgePyNamedParam {
   std::string name;
   SurgeSynthesizer::ID id;
   std::string getName() const { return name; }
   SurgeSynthesizer::ID getID() const { return id; }
   std::string toString() const { return std::string( "<SurgeNamedParam '" ) + name +  "'>"; }

};

struct SurgePyControlGroupEntry
{
   std::vector<SurgePyNamedParam> params;
   int entry;
   int scene;
   std::string groupName;

   int getEntry() const { return entry; }
   int getScene() const { return scene; }
   std::vector<SurgePyNamedParam> getParams() const { return params; }

   std::string toString() const {
      auto res = std::string( "<SurgeControlGroupEntry entry=" ) + std::to_string(entry);
      switch(scene)
      {
      case 0:
         break;
      case 1:
         res += "/sceneA";
         break;
      case 2:
         res += "/sceneB";
         break;
      }
      res += " in " + groupName + ">";
      return res;
   }

};

#define SCG(c) static SurgePyControlGroup ocg_ ## c ( c, #c )
struct SurgePyControlGroup
{
   std::vector<SurgePyControlGroupEntry> entries;
   ControlGroup id = endCG;
   std::string name;

   SurgePyControlGroup() = default;
   SurgePyControlGroup(ControlGroup id, std::string name) : id(id), name(name) {};

   int getControlGroupId() const { return (int)id; }
   std::string getControlGroupName() const { return name; }
   std::vector<SurgePyControlGroupEntry> getEntries() const { return entries; }

   std::string toString() const { return std::string( "<SurgeControlGroup cg=" ) + std::to_string(id) + ", " + name + ">"; }
};

struct SurgePyModSource
{
   modsources ms = ms_original;
   std::string name;

   SurgePyModSource() = default;
   explicit SurgePyModSource(modsources ms ) : ms(ms)
   {
      name = modsource_names[(int)ms];
   }

   int getModSource() const { return ms; }
   std::string getName() const { return name; }
   std::string toString() const {
      return std::string( "<SurgeModSource " ) + name + ">";
   }
};

static std::unordered_map<ControlGroup, SurgePyControlGroup> spysetup_cgMap;
static std::unordered_map<modsources, SurgePyModSource> spysetup_msMap;

class SurgeSynthesizerWithPythonExtensions : public SurgeSynthesizer
{
public:
   SurgeSynthesizerWithPythonExtensions( PluginLayer *sparent ) : SurgeSynthesizer( sparent ) {
      std::lock_guard<std::mutex> lg( spysetup_mutex );
      if( spysetup_cgMap.empty() )
      {
         // First time setup
#define CGM(c) spysetup_cgMap[c] = SurgePyControlGroup( c, # c );

         CGM(cg_GLOBAL);
         CGM(cg_OSC);
         CGM(cg_MIX);
         CGM(cg_LFO);
         CGM(cg_FX);
         CGM(cg_FILTER);
         CGM(cg_ENV);

         for( auto &me : spysetup_cgMap )
         {
            auto cg = me.first;
            auto pc = me.second;

            // Populate with a lazy multi pass
            std::set<std::pair<int,int>> entrySet;
            for( auto pa : storage.getPatch().param_ptr )
            {
               if( pa && pa->ctrlgroup == cg )
               {
                  entrySet.insert( std::make_pair( pa->ctrlgroup_entry, pa->scene ) );
               }
            }

            for( auto ent : entrySet )
            {
               auto entO = SurgePyControlGroupEntry();
               entO.groupName = me.second.name;
               entO.entry = ent.first;
               entO.scene = ent.second;
               for (auto pa : storage.getPatch().param_ptr)
               {
                  if (pa && pa->ctrlgroup == cg && pa->ctrlgroup_entry == ent.first &&
                      pa->scene == ent.second)
                  {
                     auto paO = SurgePyNamedParam();
                     paO.name = pa->get_full_name();
                     paO.id = idForParameter(pa);
                     entO.params.push_back(paO);
                  }
               }
               me.second.entries.push_back(entO);
            }
         }

#define CMS(c) spysetup_msMap[c] = SurgePyModSource( c );

         CMS(ms_velocity);
         CMS(ms_releasevelocity);
         CMS(ms_keytrack);
         CMS(ms_lowest_key);
         CMS(ms_highest_key);
         CMS(ms_latest_key);
         CMS(ms_polyaftertouch);
         CMS(ms_aftertouch);
         CMS(ms_modwheel);
         CMS(ms_breath);
         CMS(ms_expression);
         CMS(ms_sustain);
         CMS(ms_pitchbend);
         CMS(ms_timbre);
         CMS(ms_alternate_bipolar);
         CMS(ms_alternate_unipolar);
         CMS(ms_random_bipolar);
         CMS(ms_random_unipolar);
         CMS(ms_filtereg);
         CMS(ms_ampeg);
         CMS(ms_lfo1);
         CMS(ms_lfo2);
         CMS(ms_lfo3);
         CMS(ms_lfo4);
         CMS(ms_lfo5);
         CMS(ms_lfo6);
         CMS(ms_slfo1);
         CMS(ms_slfo2);
         CMS(ms_slfo3);
         CMS(ms_slfo4);
         CMS(ms_slfo5);
         CMS(ms_slfo6);
         CMS(ms_ctrl1);
         CMS(ms_ctrl2);
         CMS(ms_ctrl3);
         CMS(ms_ctrl4);
         CMS(ms_ctrl5);
         CMS(ms_ctrl6);
         CMS(ms_ctrl7);
         CMS(ms_ctrl8);
      }
   }

   SurgePyControlGroup getControlGroup( int cg ) const {
      if( spysetup_cgMap.find((ControlGroup)cg) == spysetup_cgMap.end() )
      {
         throw std::out_of_range("getControlGroup called with invalid control group value");
      }
      return spysetup_cgMap[(ControlGroup)cg];
   }

   SurgePyModSource getModSource( int ms ) const
   {
      if( spysetup_msMap.find((modsources)ms) == spysetup_msMap.end() )
      {
         throw std::out_of_range("getModSource called with invalid mod source group value");
      }
      return spysetup_msMap[(modsources)ms];
   }


   std::string getParameterName_py( const SurgeSynthesizer::ID &id )
   {
      char txt[256];
      getParameterName( id, txt );
      return std::string( txt );
   }

   SurgeSynthesizer::ID createSynthSideId( int id )
   {
      SurgeSynthesizer::ID idr;
      fromSynthSideId( id, idr );
      return idr;
   }

   void playNoteWithInts( int ch, int note, int vel, int detune )
   {
      playNote( ch, note, vel, detune );
   }

   float getv( Parameter *p, const pdata &v )
   {
      if( p->valtype == vt_float )
         return v.f;
      if( p->valtype == vt_int )
         return v.i;
      if( p->valtype == vt_bool )
         return v.b;
      return 0;
   }
   float getParamMin( const SurgePyNamedParam &id )
   {
      auto p = storage.getPatch().param_ptr[id.getID().getSynthSideId()];
      if( p ) return getv( p, p->val_min );
      return 0;
   }
   float getParamMax( const SurgePyNamedParam &id )
   {
      auto p = storage.getPatch().param_ptr[id.getID().getSynthSideId()];
      if( p ) return getv( p, p->val_max );
      return 0;
   }
   float getParamDef( const SurgePyNamedParam &id )
   {
      auto p = storage.getPatch().param_ptr[id.getID().getSynthSideId()];
      if( p ) return getv( p, p->val_default );
      return 0;
   }
   float getParamVal( const SurgePyNamedParam &id )
   {
      auto p = storage.getPatch().param_ptr[id.getID().getSynthSideId()];
      if( p ) return getv( p, p->val );
      return 0;
   }
   std::string getParamValType( const SurgePyNamedParam &id )
   {
      auto p = storage.getPatch().param_ptr[id.getID().getSynthSideId()];
      if( ! p ) return "<error>";
      switch( p->valtype )
      {
      case vt_float:
         return "float";
      case vt_int:
         return "int";
      case vt_bool:
         return "bool";
      }
      return "<error>";
   }

   void setParamVal( const SurgePyNamedParam &id, float f )
   {
      auto p = storage.getPatch().param_ptr[id.getID().getSynthSideId()];
      if( ! p ) return;
      switch( p->valtype )
      {
      case vt_float:
         p->val.f = f;
         break;
      case vt_int:
         p->val.i = round(f);
         break;
      case vt_bool:
         p->val.b = f > 0.5;
         break;
      }
   }


   void releaseNoteWithInts( int ch, int note, int vel)
   {
      releaseNote( ch, note, vel);
   }

   void loadPatchPy( const std::string &s )
   {
      loadPatchByPath(s.c_str(), -1, "Python" );
   }

   void savePatchPy( const std::string &s )
   {
      savePatchToPath(string_to_path(s));
   }

   std::string factoryDataPath() const { return storage.datapath; }
   std::string userDataPath() const { return storage.userDataPath; }

   py::array_t<float> getOutput( )
   {
      return py::array_t<float> ({ 2, BLOCK_SIZE },
                                { 2* sizeof(float), sizeof(float)},
                                 (const float*)( & output[0][0] ) );
   }

   void setModulationPy( const SurgePyNamedParam& to, SurgePyModSource from, float amt )
   {
      setModulation(to.getID().getSynthSideId(), (modsources)from.getModSource(), amt );
   }
   float getModulationPy( const SurgePyNamedParam& to, SurgePyModSource from )
   {
      return getModulation(to.getID().getSynthSideId(), (modsources)from.getModSource() );
   }
};

SurgeSynthesizer *createSurge( float sr )
{
   if (spysetup_parent==nullptr)
      spysetup_parent = std::make_unique<HeadlessPluginLayerProxy>();
   auto surge = new SurgeSynthesizerWithPythonExtensions( spysetup_parent.get() );
   surge->setSamplerate(sr);
   surge->time_data.tempo = 120;
   surge->time_data.ppqPos = 0;
   return surge;
}


#define C(x) m_const.attr( #x ) = py::int_((int)x);

PYBIND11_MODULE(surgepy, m) {
   m.doc() = "Python bindings for Surge Synthesizer";
   m.def( "createSurge", &createSurge, "Create a surge instance" );
   m.def( "getVersion", []() { return Surge::Build::FullVersionStr; });
   py::class_<SurgeSynthesizer::ID>(m, "SurgeSynthesizer_ID" )
       .def( py::init<>())
       .def( "getDawSideIndex", &SurgeSynthesizer::ID::getDawSideIndex )
       .def( "getDawSideId", &SurgeSynthesizer::ID::getDawSideId )
       .def( "getSynthSideId", &SurgeSynthesizer::ID::getSynthSideId )
       .def( "__repr__", &SurgeSynthesizer::ID::toString );

   py::class_<SurgeSynthesizerWithPythonExtensions>(m, "SurgeSynthesizer" )
       .def( "getControlGroup", &SurgeSynthesizerWithPythonExtensions::getControlGroup )

       .def( "getNumInputs", &SurgeSynthesizer::getNumInputs )
       .def( "getNumOutputs", &SurgeSynthesizer::getNumOutputs )
       .def( "getBlockSize", &SurgeSynthesizer::getBlockSize )
       .def( "getFactoryDataPath", &SurgeSynthesizerWithPythonExtensions::factoryDataPath)
       .def( "getUserDataPath", &SurgeSynthesizerWithPythonExtensions::userDataPath)

       .def( "fromSynthSideId", &SurgeSynthesizer::fromSynthSideId )
       .def( "createSynthSideId", &SurgeSynthesizerWithPythonExtensions::createSynthSideId )

       .def( "getParameterName", &SurgeSynthesizerWithPythonExtensions::getParameterName_py )

       .def( "playNote", &SurgeSynthesizerWithPythonExtensions::playNoteWithInts )
       .def( "releaseNote", &SurgeSynthesizerWithPythonExtensions::releaseNoteWithInts )

       .def( "getParamMin", &SurgeSynthesizerWithPythonExtensions::getParamMin )
       .def( "getParamMax", &SurgeSynthesizerWithPythonExtensions::getParamMax )
       .def( "getParamDef", &SurgeSynthesizerWithPythonExtensions::getParamDef )
       .def( "getParamVal", &SurgeSynthesizerWithPythonExtensions::getParamVal )
       .def( "getParamValType", &SurgeSynthesizerWithPythonExtensions::getParamValType )

       .def( "setParamVal", &SurgeSynthesizerWithPythonExtensions::setParamVal )

       .def( "loadPatch", &SurgeSynthesizerWithPythonExtensions::loadPatchPy )
       .def( "savePatch", &SurgeSynthesizerWithPythonExtensions::savePatchPy )

       .def( "getModSource", &SurgeSynthesizerWithPythonExtensions::getModSource )
       .def( "setModulation", &SurgeSynthesizerWithPythonExtensions::setModulationPy )
       .def( "getModulation", &SurgeSynthesizerWithPythonExtensions::getModulationPy )

       .def( "process", &SurgeSynthesizer::process )
       .def( "getOutput", &SurgeSynthesizerWithPythonExtensions::getOutput )
       ;

   py::class_<SurgePyControlGroup>(m, "SurgeControlGroup" )
       .def( "getId", &SurgePyControlGroup::getControlGroupId )
       .def( "getName", &SurgePyControlGroup::getControlGroupName )
       .def( "getEntries", &SurgePyControlGroup::getEntries )
       .def( "__repr__", &SurgePyControlGroup::toString );

   py::class_<SurgePyControlGroupEntry>(m, "SurgeControlGroupEntry" )
       .def( "getEntry", &SurgePyControlGroupEntry::getEntry )
       .def( "getScene", &SurgePyControlGroupEntry::getScene )
       .def( "getParams", &SurgePyControlGroupEntry::getParams )
       .def( "__repr__", &SurgePyControlGroupEntry::toString );

   py::class_<SurgePyNamedParam>( m, "SurgeNamedParamId" )
       .def( "getName", &SurgePyNamedParam::getName )
       .def( "getId", &SurgePyNamedParam::getID )
       .def( "__repr__", &SurgePyNamedParam::toString );

   py::class_<SurgePyModSource>(m, "SurgeModSource" )
       .def( "getModSource", &SurgePyModSource::getModSource )
       .def( "getName", &SurgePyModSource::getName )
       .def( "__repr__", &SurgePyModSource::toString );

   py::module m_const = m.def_submodule("constants", "Constants which are used to navigate Surge");

   C(cg_GLOBAL);
   C(cg_OSC);
   C(cg_MIX);
   C(cg_FILTER);
   C(cg_ENV);
   C(cg_LFO);
   C(cg_FX);

   C(ms_velocity);
   C(ms_releasevelocity);
   C(ms_keytrack);
   C(ms_lowest_key);
   C(ms_highest_key);
   C(ms_latest_key);
   C(ms_polyaftertouch);
   C(ms_aftertouch);
   C(ms_modwheel);
   C(ms_breath);
   C(ms_expression);
   C(ms_sustain);
   C(ms_pitchbend);
   C(ms_timbre);
   C(ms_alternate_bipolar);
   C(ms_alternate_unipolar);
   C(ms_random_bipolar);
   C(ms_random_unipolar);
   C(ms_filtereg);
   C(ms_ampeg);
   C(ms_lfo1);
   C(ms_lfo2);
   C(ms_lfo3);
   C(ms_lfo4);
   C(ms_lfo5);
   C(ms_lfo6);
   C(ms_slfo1);
   C(ms_slfo2);
   C(ms_slfo3);
   C(ms_slfo4);
   C(ms_slfo5);
   C(ms_slfo6);
   C(ms_ctrl1);
   C(ms_ctrl2);
   C(ms_ctrl3);
   C(ms_ctrl4);
   C(ms_ctrl5);
   C(ms_ctrl6);
   C(ms_ctrl7);
   C(ms_ctrl8);
}