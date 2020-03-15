#include "SurgeLv2Wrapper.h"
#include "SurgeLv2Util.h"
#include "SurgeLv2Helpers.h"
#include "SurgeLv2Ui.h" // for editor's setParameterAutomated
#include <cstring>

///
SurgeLv2Wrapper::SurgeLv2Wrapper(double sampleRate)
    : _synthesizer(new SurgeSynthesizer(this)), _dataLocation(new void*[NumPorts]()),
      _sampleRate(sampleRate)
{
   // needed?
   _synthesizer->time_data.ppqPos = 0;

   for( int i=0; i<NumPorts; ++i )
      _dataLocation[i] = nullptr;
}

SurgeLv2Wrapper::~SurgeLv2Wrapper()
{}

void SurgeLv2Wrapper::updateDisplay()
{
   // should I do anything particular here?
}

void SurgeLv2Wrapper::setParameterAutomated(int externalparam, float value)
{
   // logic is different depending if it's the DSP or UI calling
   // use a thread-local variable in order to track who is calling

   switch (callerType) {
   case kCallerNone:
      fprintf(stderr, "SurgeLv2: unable to automate parameter from unknown caller (%d).\n", externalparam);
      break;
   case kCallerDsp:
      writeOutputParameterToPort(externalparam, value);
      break;
   case kCallerUi:
      if (_editor)
         _editor->setParameterAutomated(externalparam, value);
      else
         fprintf(stderr, "SurgeLv2: attempt to automate parameter without editor (%d).\n", externalparam);
      break;
   }
}

void SurgeLv2Wrapper::patchChanged()
{
   _editorMustReloadPatch.store(true);
}

void SurgeLv2Wrapper::writeOutputParameterToPort(int externalparam, float value)
{
   auto* forge = &_eventsForge;
   auto* eventSequence = (const LV2_Atom_Sequence*)_dataLocation[pEventsOut];

   if (!eventSequence) {
      fprintf(stderr, "SurgeLv2: attempt to write parameter as the port is not connected.\n");
      return;
   }

   lv2_atom_forge_set_buffer(forge, (uint8_t*)eventSequence, eventSequence->atom.size);

   LV2_Atom_Forge_Frame objFrame;
   if (!lv2_atom_forge_frame_time(forge, 0) ||
       !lv2_atom_forge_object(forge, &objFrame, 0, _uridPatchSet) ||
       // property
       !lv2_atom_forge_key(forge, _uridPatch_property) ||
       !lv2_atom_forge_urid(forge, _uridSurgeParameter[externalparam]) ||
       // value
       !lv2_atom_forge_key(forge, _uridPatch_value) ||
       !lv2_atom_forge_float(forge, value))
   {
      fprintf(stderr, "SurgeLv2: cannot write patch:Set object, insufficient capacity (%d).\n", externalparam);
      return;
   }

   lv2_atom_forge_pop(forge, &objFrame);
}

LV2_Handle SurgeLv2Wrapper::instantiate(const LV2_Descriptor* descriptor,
                                        double sample_rate,
                                        const char* bundle_path,
                                        const LV2_Feature* const* features)
{
   std::unique_ptr<SurgeLv2Wrapper> self(new SurgeLv2Wrapper(sample_rate));

   SurgeSynthesizer* synth = self->_synthesizer.get();
   synth->setSamplerate(sample_rate);

   auto* featureUridMap = (LV2_URID_Map*)SurgeLv2::requireFeature(LV2_URID__map, features);

   lv2_atom_forge_init(&self->_eventsForge, featureUridMap);

   self->_uridMidiEvent = featureUridMap->map(featureUridMap->handle, LV2_MIDI__MidiEvent);
   self->_uridAtomBlank = featureUridMap->map(featureUridMap->handle, LV2_ATOM__Blank);
   self->_uridAtomObject = featureUridMap->map(featureUridMap->handle, LV2_ATOM__Object);
   self->_uridAtomDouble = featureUridMap->map(featureUridMap->handle, LV2_ATOM__Double);
   self->_uridAtomFloat = featureUridMap->map(featureUridMap->handle, LV2_ATOM__Float);
   self->_uridAtomInt = featureUridMap->map(featureUridMap->handle, LV2_ATOM__Int);
   self->_uridAtomLong = featureUridMap->map(featureUridMap->handle, LV2_ATOM__Long);
   self->_uridAtomChunk = featureUridMap->map(featureUridMap->handle, LV2_ATOM__Chunk);
   self->_uridAtomURID = featureUridMap->map(featureUridMap->handle, LV2_ATOM__URID);
   self->_uridTimePosition = featureUridMap->map(featureUridMap->handle, LV2_TIME__Position);
   self->_uridTime_beatsPerMinute =
       featureUridMap->map(featureUridMap->handle, LV2_TIME__beatsPerMinute);
   self->_uridTime_speed = featureUridMap->map(featureUridMap->handle, LV2_TIME__speed);
   self->_uridTime_beat = featureUridMap->map(featureUridMap->handle, LV2_TIME__beat);
   self->_uridPatchSet = featureUridMap->map(featureUridMap->handle, LV2_PATCH__Set);
   self->_uridPatch_property = featureUridMap->map(featureUridMap->handle, LV2_PATCH__property);
   self->_uridPatch_value = featureUridMap->map(featureUridMap->handle, LV2_PATCH__value);

   self->_uridSurgePatch = featureUridMap->map(featureUridMap->handle, SURGE_PATCH_URI);

   for (unsigned pNth = 0; pNth < n_total_params; ++pNth)
   {
      LV2_URID urid = featureUridMap->map(featureUridMap->handle, self->getParameterUri(pNth).c_str());
      self->_uridSurgeParameter[pNth] = urid;
      self->_surgeParameterUrid[urid] = pNth;
   }

   return (LV2_Handle)self.release();
}

void SurgeLv2Wrapper::connectPort(LV2_Handle instance, uint32_t port, void* data_location)
{
   SurgeLv2Wrapper* self = (SurgeLv2Wrapper*)instance;
   self->_dataLocation[port] = data_location;
#if DEBUG_STARTUP_SETS
   if( port == 118 )
      std::cout << "connectPort " << port << " " << *(float *)(data_location) << std::endl;
#endif
}

void SurgeLv2Wrapper::activate(LV2_Handle instance)
{
   SurgeLv2Wrapper* self = (SurgeLv2Wrapper*)instance;
   SurgeSynthesizer* s = self->_synthesizer.get();

   self->_blockPos = 0;
   s->audio_processing_active = true;
}

void SurgeLv2Wrapper::run(LV2_Handle instance, uint32_t sample_count)
{
   ScopedCallerType callerType(kCallerDsp);
   SurgeLv2Wrapper* self = (SurgeLv2Wrapper*)instance;
   SurgeSynthesizer* s = self->_synthesizer.get();
   double sampleRate = self->_sampleRate;

   self->_fpuState.set();

   bool isPlaying = SurgeLv2::isNotZero(self->_timePositionSpeed);
   bool hasValidTempo = SurgeLv2::isGreaterThanZero(self->_timePositionTempoBpm);
   bool hasValidPosition = self->_timePositionBeat >= 0.0;

   if (hasValidTempo)
   {
      if (isPlaying)
         s->time_data.tempo = self->_timePositionTempoBpm * std::fabs(self->_timePositionSpeed);
   }
   else
   {
      s->time_data.tempo = 120.0;
   }

   if (isPlaying && hasValidPosition)
      s->time_data.ppqPos = self->_timePositionBeat; // TODO is it correct?

   auto* eventSequence = (const LV2_Atom_Sequence*)self->_dataLocation[pEventsIn];
   const LV2_Atom_Event* eventIter = lv2_atom_sequence_begin(&eventSequence->body);
   const LV2_Atom_Event* event = SurgeLv2::popNextEvent(eventSequence, &eventIter);

   const float* inputs[NumInputs];
   float* outputs[NumOutputs];
   bool input_connected = true;

   for (uint32_t i = 0; i < NumInputs; ++i)
   {
      inputs[i] = (const float*)self->_dataLocation[pAudioInput1 + i];
      if (!inputs[i])
         input_connected = false;
   }
   for (uint32_t i = 0; i < NumOutputs; ++i)
      outputs[i] = (float*)self->_dataLocation[pAudioOutput1 + i];

   s->process_input = (/*!plug_is_synth ||*/ input_connected);

   for (uint32_t i = 0; i < sample_count; ++i)
   {
      if (self->_blockPos == 0)
      {
         // move clock
         s->time_data.ppqPos += (double)BLOCK_SIZE * s->time_data.tempo / (60. * sampleRate);

         // process events for the current block
         while (event && i >= event->time.frames)
         {
            self->handleEvent(event);
            event = SurgeLv2::popNextEvent(eventSequence, &eventIter);
         }

         // run sampler engine for the current block
         s->process();
      }

      if (s->process_input)
      {
         int inp;
         for (inp = 0; inp < NumInputs; inp++)
         {
            s->input[inp][self->_blockPos] = inputs[inp][i];
         }
      }

      int outp;
      for (outp = 0; outp < NumOutputs; outp++)
      {
         outputs[outp][i] = (float)s->output[outp][self->_blockPos]; // replacing
      }

      self->_blockPos++;
      if (self->_blockPos >= BLOCK_SIZE)
         self->_blockPos = 0;
   }

   while (event)
   {
      self->handleEvent(event);
      event = SurgeLv2::popNextEvent(eventSequence, &eventIter);
   }

   self->_fpuState.restore();
}

void SurgeLv2Wrapper::handleEvent(const LV2_Atom_Event* event)
{
   SurgeSynthesizer* s = _synthesizer.get();

   if (event->body.type == _uridMidiEvent)
   {
      const char* midiData = (char*)event + sizeof(*event);
      int status = midiData[0] & 0xf0;
      int channel = midiData[0] & 0x0f;
      if (status == 0x90 || status == 0x80) // we only look at notes
      {
         int newnote = midiData[1] & 0x7f;
         int velo = midiData[2] & 0x7f;
         if ((status == 0x80) || (velo == 0))
         {
            _synthesizer->releaseNote((char)channel, (char)newnote, (char)velo);
         }
         else
         {
            _synthesizer->playNote((char)channel, (char)newnote, (char)velo, 0 /*event->detune*/);
         }
      }
      else if (status == 0xe0) // pitch bend
      {
         long value = (midiData[1] & 0x7f) + ((midiData[2] & 0x7f) << 7);
         _synthesizer->pitchBend((char)channel, value - 8192);
      }
      else if (status == 0xB0) // controller
      {
         if (midiData[1] == 0x7b || midiData[1] == 0x7e)
            _synthesizer->allNotesOff(); // all notes off
         else
            _synthesizer->channelController((char)channel, midiData[1] & 0x7f, midiData[2] & 0x7f);
      }
      else if (status == 0xC0) // program change
      {
         _synthesizer->programChange((char)channel, midiData[1] & 0x7f);
      }
      else if (status == 0xD0) // channel aftertouch
      {
         _synthesizer->channelAftertouch((char)channel, midiData[1] & 0x7f);
      }
      else if (status == 0xA0) // poly aftertouch
      {
         _synthesizer->polyAftertouch((char)channel, midiData[1] & 0x7f, midiData[2] & 0x7f);
      }
      else if ((status == 0xfc) || (status == 0xff)) //  MIDI STOP or reset
      {
         _synthesizer->allNotesOff();
      }
   }
   else if ((event->body.type == _uridAtomBlank || event->body.type == _uridAtomObject) &&
            (((const LV2_Atom_Object*)&event->body)->body.otype) == _uridPatchSet)
   {
      const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&event->body;
      const LV2_Atom* property = nullptr;
      const LV2_Atom* value = nullptr;

      lv2_atom_object_get(obj, _uridPatch_property, &property, _uridPatch_value, &value, 0);

      if (!property || !value || property->type != _uridAtomURID)
      {
         // malformed patch message
         return;
      }

      if (value->type == _uridAtomFloat)
      {
         const LV2_URID urid = ((const LV2_Atom_URID*)property)->body;
         const float value = ((const LV2_Atom_Float*)property)->body;

         auto it = _surgeParameterUrid.find(urid);
         if (it != _surgeParameterUrid.end())
         {
            unsigned pNth = it->second;
            unsigned index = s->remapExternalApiToInternalId(pNth);
            s->setParameter01(index, value);
         }
      }
   }
   else if ((event->body.type == _uridAtomBlank || event->body.type == _uridAtomObject) &&
            (((const LV2_Atom_Object*)&event->body)->body.otype) == _uridTimePosition)
   {
      const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&event->body;

      LV2_Atom* beatsPerMinute = nullptr;
      LV2_Atom* speed = nullptr;
      LV2_Atom* beat = nullptr;

      lv2_atom_object_get(obj, _uridTime_beatsPerMinute, &beatsPerMinute, _uridTime_speed, &speed,
                          _uridTime_beat, &beat, 0);

      auto atomGetNumber = [this](const LV2_Atom* atom, double* number) -> bool {
         if (atom->type == _uridAtomDouble)
            *number = ((const LV2_Atom_Double*)atom)->body;
         else if (atom->type == _uridAtomFloat)
            *number = ((const LV2_Atom_Float*)atom)->body;
         else if (atom->type == _uridAtomInt)
            *number = ((const LV2_Atom_Int*)atom)->body;
         else if (atom->type == _uridAtomLong)
            *number = ((const LV2_Atom_Long*)atom)->body;
         else
            return false;
         return true;
      };

      if (speed)
         atomGetNumber(speed, &_timePositionSpeed);
      if (beatsPerMinute)
         atomGetNumber(beatsPerMinute, &_timePositionTempoBpm);
      if (beat)
         atomGetNumber(beat, &_timePositionBeat);
   }
}

void SurgeLv2Wrapper::deactivate(LV2_Handle instance)
{
   SurgeLv2Wrapper* self = (SurgeLv2Wrapper*)instance;

   self->_synthesizer->allNotesOff();
   self->_synthesizer->audio_processing_active = false;
}

void SurgeLv2Wrapper::cleanup(LV2_Handle instance)
{
   SurgeLv2Wrapper* self = (SurgeLv2Wrapper*)instance;
   delete self;
}

const void* SurgeLv2Wrapper::extensionData(const char* uri)
{
   if (!strcmp(uri, LV2_STATE__interface))
   {
      static const LV2_State_Interface state = createStateInterface();
      return &state;
   }

   return nullptr;
}

LV2_State_Interface SurgeLv2Wrapper::createStateInterface()
{
   LV2_State_Interface intf;
   intf.save = &saveState;
   intf.restore = &restoreState;
   return intf;
}

LV2_State_Status SurgeLv2Wrapper::saveState(LV2_Handle instance, LV2_State_Store_Function store, LV2_State_Handle handle, uint32_t flags, const LV2_Feature* const* features)
{
   SurgeLv2Wrapper* self = (SurgeLv2Wrapper*)instance;
   SurgeSynthesizer* s = self->_synthesizer.get();

   void *data = nullptr;
   s->populateDawExtraState();
   // TODO also save editor stuff?
   size_t size = s->saveRaw(&data);

   uint32_t storageFlags = LV2_STATE_IS_POD|LV2_STATE_IS_PORTABLE;
   return store(handle, self->_uridSurgePatch, data, size, self->_uridAtomChunk, storageFlags);
}

LV2_State_Status SurgeLv2Wrapper::restoreState(LV2_Handle instance, LV2_State_Retrieve_Function retrieve, LV2_State_Handle handle, uint32_t flags, const LV2_Feature* const* features)
{
#if DEBUG_STARTUP_SETS
   std::cout << "restoreState" << std::endl;
#endif

   SurgeLv2Wrapper* self = (SurgeLv2Wrapper*)instance;
   SurgeSynthesizer* s = self->_synthesizer.get();

   size_t size = 0;
   uint32_t type = 0;
   uint32_t storageFlags = LV2_STATE_IS_POD|LV2_STATE_IS_PORTABLE;
   const void* data = retrieve(handle, self->_uridSurgePatch, &size, &type, &storageFlags);

   if (type != self->_uridAtomChunk)
   {
      return LV2_STATE_ERR_BAD_TYPE;
   }

   s->loadRaw(data, size, false);
   s->loadFromDawExtraState();

   // TODO also load editor stuff?

   return LV2_STATE_SUCCESS;
}

std::string SurgeLv2Wrapper::getParameterSymbol(unsigned pNth) const
{
   unsigned index = _synthesizer->remapExternalApiToInternalId(pNth);
   parametermeta pMeta;
   _synthesizer->getParameterMeta(index, pMeta);

   std::string pSymbol;
   if( index >= metaparam_offset )
   {
      pSymbol = "meta_cc" + std::to_string( index - metaparam_offset + 1 );
   }
   else
   {
      auto *par = _synthesizer->storage.getPatch().param_ptr[index];
      pSymbol = par->get_storage_name();
   }

   return pSymbol;
}

std::string SurgeLv2Wrapper::getParameterUri(unsigned pNth) const
{
   return SURGE_PLUGIN_URI "#parameter_" + getParameterSymbol(pNth);
}

LV2_Descriptor SurgeLv2Wrapper::createDescriptor()
{
   LV2_Descriptor desc = {};
   desc.URI = SURGE_PLUGIN_URI;
   desc.instantiate = &instantiate;
   desc.connect_port = &connectPort;
   desc.activate = &activate;
   desc.run = &run;
   desc.deactivate = &deactivate;
   desc.cleanup = &cleanup;
   desc.extension_data = &extensionData;
   return desc;
}

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
   if (index == 0)
   {
      static LV2_Descriptor desc = SurgeLv2Wrapper::createDescriptor();
      return &desc;
   }
   return nullptr;
}
