#include "SurgeLv2Wrapper.h"
#include "version.h"
#include <fstream>
#include <iomanip>

#if defined _WIN32
#define LV2_DLL_SUFFIX ".dll"
#define LV2_UI_TYPE "WindowsUI"
#elif defined __APPLE__
#define LV2_DLL_SUFFIX ".dylib"
#define LV2_UI_TYPE "CocoaUI"
#else
#define LV2_DLL_SUFFIX ".so"
#define LV2_UI_TYPE "X11UI"
#endif

static void writePrefix(std::ofstream& os);

LV2_SYMBOL_EXPORT
void lv2_generate_ttl(const char* baseName)
{
   const LV2_Descriptor* desc = lv2_descriptor(0);
   const LV2UI_Descriptor* uidesc = lv2ui_descriptor(0);

   SurgeStorage::skipLoadWtAndPatch = true;

   SurgeLv2Wrapper defaultInstance(44100.0);
   SurgeSynthesizer* defaultSynth = defaultInstance.synthesizer();

   {
      std::ofstream osMan("manifest.ttl");
      writePrefix(osMan);

      osMan << "<" << desc->URI << ">\n"
               "    a lv2:Plugin, lv2:InstrumentPlugin ;\n"
               "    lv2:binary <" << baseName << LV2_DLL_SUFFIX "> ;\n"
               "    rdfs:seeAlso <" << baseName << "_dsp.ttl> .\n"
               "\n"
               "<" << uidesc->URI << ">\n"
               "    a ui:" LV2_UI_TYPE " ;\n"
               "    ui:binary <" << baseName << LV2_DLL_SUFFIX "> ;\n"
               "    rdfs:seeAlso <" << baseName << "_ui.ttl> .\n";
   }

   {
      std::ofstream osDsp(baseName + std::string("_dsp.ttl"));
      writePrefix(osDsp);

      osDsp << "<" << desc->URI << ">\n"
         "    doap:name \"" << stringProductName << "\" ;\n"
         "    doap:license <https://www.gnu.org/licenses/gpl-3.0.en.html> ;\n"
         "    doap:maintainer [\n"
         "        foaf:name \"" << stringCompanyName << "\" ;\n"
         "        foaf:homepage <" << stringWebsite << "> ;\n"
         "    ] ;\n"
         "    ui:ui <" << uidesc->URI << "> ;\n"
         "    lv2:optionalFeature lv2:hardRTCapable ;\n"
         "    lv2:requiredFeature urid:map ;\n"
         "    lv2:extensionData state:interface ;\n";

      unsigned portIndex = 0;
      osDsp << "    lv2:port";
      // parameters
      for (unsigned pNth = 0; pNth < n_total_params; ++pNth)
      {
         if (portIndex > 0)
            osDsp << " ,";

         unsigned index = defaultSynth->remapExternalApiToInternalId(pNth);
         parametermeta pMeta;
         defaultSynth->getParameterMeta(index, pMeta);

         char pName[256];
         defaultSynth->getParameterName(index, pName);

         std::string pSymbol;
         if( index >= metaparam_offset )
         {
            pSymbol = "meta_cc" + std::to_string( index - metaparam_offset + 1 );
         }
         else
         {
            auto *par = defaultSynth->storage.getPatch().param_ptr[index];
            pSymbol = par->get_storage_name();
         }
         
         osDsp << " [\n"
                  "        a lv2:InputPort, lv2:ControlPort ;\n"
                  "        lv2:index " << portIndex << " ;\n"
                  "        lv2:symbol \"" << pSymbol << "\" ;\n"
                  "        lv2:name \"" << pName << "\" ;\n"
                  "        lv2:default " << std::fixed << std::setw(10) << std::setprecision(8) << defaultSynth->getParameter01(index) << " ;\n"
                  "        lv2:minimum " << std::fixed << std::setw(10) <<  std::setprecision(8) << pMeta.fmin << " ;\n"
                  "        lv2:maximum " << std::fixed << std::setw(10) << std::setprecision(8) << pMeta.fmax << " ;\n"
                  "    ]";
         ++portIndex;
      }
      // event input
      {
         if (portIndex > 0)
            osDsp << " ,";
         osDsp << " [\n"
                  "        a lv2:InputPort, atom:AtomPort ;\n"
                  "        lv2:index " << portIndex << " ;\n"
                  "        lv2:symbol \"events_in\" ;\n"
                  "        lv2:name \"Event input\" ;\n"
                  "        rsz:minimumSize " << SurgeLv2Wrapper::EventBufferSize << " ;\n"
                  "        atom:bufferType atom:Sequence ;\n"
                  "        atom:supports midi:MidiEvent,\n"
                  "                      time:Position ;\n"
                  "    ]";
         ++portIndex;
      }
      // audio input
      for (unsigned i = 0; i < SurgeLv2Wrapper::NumInputs; ++i, ++portIndex)
      {
         if (portIndex > 0)
            osDsp << " ,";
         osDsp << " [\n"
                  "        a lv2:InputPort, lv2:AudioPort ;\n"
                  "        lv2:index " << portIndex << " ;\n"
                  "        lv2:symbol \"audio_in_" << (i + 1) << "\" ;\n"
                  "        lv2:name \"Audio input " << (i + 1) << "\" ;\n"
                  "        lv2:portProperty lv2:connectionOptional ;\n"
                  "    ]";
      }
      // audio output
      for (unsigned i = 0; i < SurgeLv2Wrapper::NumOutputs; ++i, ++portIndex)
      {
         if (portIndex > 0)
            osDsp << " ,";
         osDsp << " [\n"
                  "        a lv2:OutputPort, lv2:AudioPort ;\n"
                  "        lv2:index " << portIndex << " ;\n"
                  "        lv2:symbol \"audio_out_" << (i + 1) << "\" ;\n"
                  "        lv2:name \"Audio output " << (i + 1) << "\" ;\n"
                  "    ]";
      }
      osDsp << " ;\n";

      // TODO LV2: implement an adequate version number scheme. For now, make it the last two (so 1.6.2 gets 6 2)
      osDsp << "    lv2:minorVersion " << Surge::Build::SubVersionInt << " ;\n"
         "    lv2:microVersion " << Surge::Build::ReleaseNumberStr << " .\n";
   }

   {
      std::ofstream osUi(baseName + std::string("_ui.ttl"));
      writePrefix(osUi);

      osUi << "<" << uidesc->URI
           << ">\n"
              "    lv2:optionalFeature ui:parent,\n"
              "                        ui:resize,\n"
              "                        ui:noUserResize ;\n"
              "    lv2:requiredFeature ui:idleInterface ,\n"
              "                        <" LV2_INSTANCE_ACCESS_URI "> ;\n"
              "    opts:supportedOption ui:scaleFactor ;\n"
              "    lv2:extensionData ui:idleInterface .\n";
   }
}

static void writePrefix(std::ofstream& os)
{
   os << "@prefix lv2:  <" LV2_CORE_PREFIX "> .\n"
         "@prefix opts: <" LV2_OPTIONS_PREFIX "> .\n"
         "@prefix ui:   <" LV2_UI_PREFIX "> .\n"
         "@prefix atom: <" LV2_ATOM_PREFIX "> .\n"
         "@prefix urid: <" LV2_URID_PREFIX "> .\n"
         "@prefix unit: <" LV2_UNITS_PREFIX "> .\n"
         "@prefix rsz:  <" LV2_RESIZE_PORT_PREFIX "> .\n"
         "@prefix midi: <" LV2_MIDI_PREFIX "> .\n"
         "@prefix time: <" LV2_TIME_PREFIX "> .\n"
         "@prefix state: <" LV2_STATE_PREFIX "> .\n"
         "@prefix rdf:  <http://www.w3.org/1999/02/22-rdf-syntax-ns#> .\n"
         "@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .\n"
         "@prefix doap: <http://usefulinc.com/ns/doap#> .\n"
         "@prefix foaf: <http://xmlns.com/foaf/0.1/> .\n"
         "\n";
}
