#pragma once
#include "SurgeSynthesizer.h"
#include "AllLv2.h"
#include "util/FpuState.h"
#include <atomic>
#include <memory>

class SurgeLv2Ui;

class SurgeLv2Wrapper
{
public:
   static LV2_Descriptor createDescriptor();

   explicit SurgeLv2Wrapper(double sampleRate);
   ~SurgeLv2Wrapper();

   void setEditor(SurgeLv2Ui* editor)
   {
      _editor = editor;
   }

   enum
   {
      NumInputs = 2,
      NumOutputs = 2,
   };

   enum
   {
      EventBufferSize = 8192,
   };

   enum
   {
      /* [0,n] parameters */
      pEvents = n_total_params,
      pAudioInput1,
      pAudioOutput1 = pAudioInput1 + NumInputs,
      NumPorts = pAudioOutput1 + NumOutputs,
   };

   SurgeSynthesizer* synthesizer() const noexcept
   {
      return _synthesizer.get();
   }

   // PluginLayer
   void updateDisplay();
   void setParameterAutomated(int externalparam, float value);
   void patchChanged();

private:
   static LV2_Handle instantiate(const LV2_Descriptor* descriptor,
                                 double sample_rate,
                                 const char* bundle_path,
                                 const LV2_Feature* const* features);
   static void connectPort(LV2_Handle instance, uint32_t port, void* data_location);
   static void activate(LV2_Handle instance);
   static void run(LV2_Handle instance, uint32_t sample_count);
   void handleEvent(const LV2_Atom_Event* event);
   static void deactivate(LV2_Handle instance);
   static void cleanup(LV2_Handle instance);
   static const void* extensionData(const char* uri);

   ///
   static LV2_State_Interface createStateInterface();
   static LV2_State_Status saveState(LV2_Handle instance, LV2_State_Store_Function store, LV2_State_Handle handle, uint32_t, const LV2_Feature* const*);
   static LV2_State_Status restoreState(LV2_Handle instance, LV2_State_Retrieve_Function retrieve, LV2_State_Handle handle, uint32_t, const LV2_Feature* const*);

private:
   std::unique_ptr<SurgeSynthesizer> _synthesizer;
   std::unique_ptr<void*[]> _dataLocation;
   std::unique_ptr<float[]> _oldControlValues;
   double _sampleRate = 44100.0;

   uint32_t _blockPos = 0;
   FpuState _fpuState;

   double _timePositionSpeed = 0.0;
   double _timePositionTempoBpm = 0.0;
   double _timePositionBeat = 0.0;

   LV2_URID _uridMidiEvent;
   LV2_URID _uridAtomBlank;
   LV2_URID _uridAtomObject;
   LV2_URID _uridAtomDouble;
   LV2_URID _uridAtomFloat;
   LV2_URID _uridAtomInt;
   LV2_URID _uridAtomLong;
   LV2_URID _uridAtomChunk;
   LV2_URID _uridTimePosition;
   LV2_URID _uridTime_beatsPerMinute;
   LV2_URID _uridTime_speed;
   LV2_URID _uridTime_beat;

   LV2_URID _uridSurgePatch;

   SurgeLv2Ui* _editor = nullptr;

public:
   // tells the editor it must take parameters from synth and update itself
   std::atomic<bool> _editorMustReloadPatch{false};
};
