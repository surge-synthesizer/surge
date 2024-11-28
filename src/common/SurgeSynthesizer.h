/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#ifndef SURGE_SRC_COMMON_SURGESYNTHESIZER_H
#define SURGE_SRC_COMMON_SURGESYNTHESIZER_H
#include "SurgeStorage.h"
#include "SurgeVoice.h"
#include "Effect.h"
#include "BiquadFilter.h"
#include <set>
#include <sst/filters/HalfRateFilter.h>

struct QuadFilterChainState;

#include <list>
#include <utility>
#include <atomic>
#include <cstdio>
#include <bitset>
#include <vector>

struct timedata
{
    double ppqPos, tempo;
    int timeSigNumerator = 4, timeSigDenominator = 4;
};

struct parametermeta
{
    float fmin, fmax, fdefault;
    unsigned int flags, clump;
    bool hide, expert, meta;
};

class alignas(16) SurgeSynthesizer
{
  public:
    float output alignas(16)[N_OUTPUTS][BLOCK_SIZE];
    float sceneout alignas(
        16)[n_scenes][N_OUTPUTS][BLOCK_SIZE_OS]; // this is blocksize_os but has been downsampled by
                                                 // the end of process into block_size

    float input alignas(16)[N_INPUTS][BLOCK_SIZE];
    timedata time_data;

    // aligned stuff
    SurgeStorage storage alignas(16);

    // Each of these operations is post downsample so need to be blocksize not blocksize os.
    lipol_ps_blocksz FX alignas(16)[n_send_slots], amp alignas(16), amp_mute alignas(16),
        send alignas(16)[n_send_slots][n_scenes];

    std::atomic<bool> audio_processing_active;
    std::atomic<bool> patchChanged{false};

    // methods
  public:
    struct ID;
    struct PluginLayer
    {
        virtual void surgeParameterUpdated(const ID &, float) = 0;
        virtual void surgeMacroUpdated(long macroNum, float) = 0;
    };
    SurgeSynthesizer(PluginLayer *parent, const std::string &suppliedDataPath = "");
    virtual ~SurgeSynthesizer();

    // Also see setNoteExpression() which allows you to control all note parameters polyphonically
    // with the user-provided host_noteid parameter. forceScene means to ignore any channel
    // related shenanigans and force you onto scene 0, 1, 2 etc... which is useful mostly
    // for latch mode working in MPE and ignore 23 mode
    void playNote(char channel, char key, char velocity, char detune, int32_t host_noteid = -1,
                  int32_t forceScene = -1);
    void playNoteByFrequency(float freq, char velocity, int32_t id);
    void releaseNote(char channel, char key, char velocity, int32_t host_noteid = -1);
    void chokeNote(int16_t channel, int16_t key, char velocity, int32_t host_noteid = -1);
    // Release all notes matching just this host noteid. Mostly used for OpenSoundCtrl right now
    void releaseNoteByHostNoteID(int32_t host_noteid, char velocity);

    void releaseNotePostHoldCheck(int scene, char channel, char key, char velocity,
                                  int32_t host_noteid = -1);
    void resetPitchBend(int8_t channel);
    void pitchBend(char channel, int value);
    void polyAftertouch(char channel, int key, int value);
    void channelAftertouch(char channel, int value);
    void channelController(char channel, int cc, int value);
    void programChange(char channel, int value);
    void allNotesOff();
    void allSoundOff();
    void setSamplerate(float sr);
    void updateHighLowKeys(int scene);
    int getNumInputs() { return N_INPUTS; }
    int getNumOutputs() { return N_OUTPUTS; }
    int getBlockSize() { return BLOCK_SIZE; }
    int getMpeMainChannel(int voiceChannel, int key);
    void process();

    PluginLayer *getParent();

    // protected:

    void onRPN(int channel, int lsbRPN, int msbRPN, int lsbValue, int msbValue);
    void onNRPN(int channel, int lsbNRPN, int msbNRPN, int lsbValue, int msbValue);

    void resetStateFromTimeData();
    void processControl();
    /*
     * processAudioThreadOpsWhenAudioEngineUnavailable reloads a patch if the audio thread
     * isn't running but if it is running lets the deferred queue handle it. But it has an option
     * which is *extremely dangerous* to force you to load in the current thread immediately.
     * If you use this option and don't know what you are doing it will explode - basically
     * we only use it in the startup constructor path.
     */
    void
    processAudioThreadOpsWhenAudioEngineUnavailable(bool doItEvenIfAudioIsRunningDANGER = false);
    bool loadFx(bool initp, bool force_reload_all);
    void enqueueFXOff(int whichFX);
    bool loadOscalgos();
    std::atomic<bool> resendOscParam[n_scenes][n_oscs]{};
    std::atomic<bool> resendFXParam[n_fx_slots]{};

    bool load_fx_needed;

    /*
     * FX Lifecycle events happen on the audio thread but is read in the openOrRecreateEditor
     * so if you swpan or init the fx[s] object lock this mutex
     */
    std::mutex fxSpawnMutex;
    std::mutex patchLoadSpawnMutex;
    enum FXReorderMode
    {
        NONE,
        SWAP,
        COPY,
        MOVE
    };
    void reorderFx(
        int source, int target,
        FXReorderMode m); // This is safe to call from the UI thread since it just edits the sync

    void playVoice(int scene, char channel, char key, char velocity, char detune,
                   int32_t host_noteid, int16_t okey = -1, int16_t ochan = -1);
    void releaseScene(int s);
    int calculateChannelMask(int channel, int key);
    void softkillVoice(int scene);
    void enforcePolyphonyLimit(int scene, int margin);
    int getNonUltrareleaseVoices(int scene) const;
    int getNonReleasedVoices(int scene) const;

    SurgeVoice *getUnusedVoice(int scene); // not const since it updates voice state
    void freeVoice(SurgeVoice *);
    void reclaimVoiceFor(SurgeVoice *v, char key, char channel, char velocity, int scene,
                         int host_note_id, int host_originating_channel, int host_originating_key,
                         bool envFromZero = false);
    void notifyEndedNote(int32_t nid, int16_t key, int16_t chan, bool thisBlock = true);
    std::array<std::array<SurgeVoice, MAX_VOICES>, 2> voices_array;
    // TODO: FIX SCENE ASSUMPTION!
    unsigned int voices_usedby[2][MAX_VOICES]; // 0 indicates no user, 1 is scene A, 2 is scene B

    int64_t voiceCounter = 1L;

    std::atomic<unsigned int> processRunning{0};

    bool doNotifyEndedNote{true};
    int32_t hostNoteEndedDuringBlockCount{0};
    int32_t endedHostNoteIds[MAX_VOICES << 3];
    int16_t endedHostNoteOriginalKey[MAX_VOICES << 3];
    int16_t endedHostNoteOriginalChannel[MAX_VOICES << 3];

    int32_t hostNoteEndedToPushToNextBlock{0};
    int32_t nextBlockEndedHostNoteIds[MAX_VOICES << 3];
    int16_t nextBlockEndedHostNoteOriginalKey[MAX_VOICES << 3];
    int16_t nextBlockEndedHostNoteOriginalChannel[MAX_VOICES << 3];

  public:
    /*
     * So when surge was pre-juce we contemplated writing our own ID remapping between
     * internal indices and DAW IDs so put an indirectin and class in place. JUCE obviates
     * the need for that by using hash of stremaing name as an ID consistently through its
     * param mechanism. I could, in theory, have gone right back to int as my accessor class
     * but there's something compelilng about keeping that indirection I plumbed in just in case
     * i need it in the future. So the ID class is now just a simple wrapper on an int which is
     * entirely inline.
     */
    struct ID
    {
        int getSynthSideId() const { return synthid; }

        std::string toString() const
        {
            std::ostringstream oss;
            oss << "ID[" << synthid << "]";
            return oss.str();
        }

        bool operator==(const ID &other) const { return synthid == other.synthid; }
        bool operator!=(const ID &other) const { return !(*this == other); }

        friend std::ostream &operator<<(std::ostream &os, const ID &id)
        {
            os << id.toString();
            return os;
        }

      private:
        int synthid = -1;
        friend SurgeSynthesizer;
    };

    bool fromSynthSideId(int i, ID &q) const
    {
        if (i < 0 || i >= n_total_params)
            return false;

        q.synthid = i;
        return true;
    }

    ID idForParameter(const Parameter *p) const
    {
        // We know this will always work
        ID i;
        fromSynthSideId(p->id, i);
        return i;
    }

    void getParameterDisplay(const ID &index, char *text) const
    {
        getParameterDisplay(index.getSynthSideId(), text);
    }
    void getParameterDisplay(const ID &index, char *text, float x) const
    {
        getParameterDisplay(index.getSynthSideId(), text);
    }
    void getParameterDisplayAlt(const ID &index, char *text) const
    {
        getParameterDisplayAlt(index.getSynthSideId(), text);
    }
    void getParameterName(const ID &index, char *text) const
    {
        getParameterName(index.getSynthSideId(), text);
    }
    void getParameterNameExtendedByFXGroup(const ID &index, char *text) const
    {
        getParameterNameExtendedByFXGroup(index.getSynthSideId(), text);
    }
    void getParameterAccessibleName(const ID &index, char *text) const
    {
        getParameterAccessibleName(index.getSynthSideId(), text);
    }
    void getParameterMeta(const ID &index, parametermeta &pm) const
    {
        getParameterMeta(index.getSynthSideId(), pm);
    }
    float getParameter01(const ID &index) const { return getParameter01(index.getSynthSideId()); }
    bool setParameter01(const ID &index, float value, bool external = false,
                        bool force_integer = false)
    {
        return setParameter01(index.getSynthSideId(), value, external, force_integer);
    }

    void applyParameterMonophonicModulation(Parameter *, float depth);
    void applyParameterPolyphonicModulation(Parameter *, int32_t note_id, int16_t key,
                                            int16_t channel, float depth);

    void setMacroParameter01(long macroNum, float val);
    float getMacroParameter01(long macroNum) const;
    float getMacroParameterTarget01(long macroNum) const;
    void applyMacroMonophonicModulation(long macroNum, float val);

    // This is a method that, along with playNote(), allows for polyphonic control of the
    // synthesizer.
    //
    // A "note expression" controls some aspect of a synthesizer voice. It can control the volume,
    // pan, pitch (-120 to 120 key tuning), timbre (MPE timbre parameter 0..1) and pressure (channel
    // AT or poly AT, depending on MPE mode, from 0..1).
    //
    // By specifying an arbitrary note_id, you can control any of the above parameters. This is
    // entirely polyphonic, based on the given note_id. Alternatively, you can provide the other
    // parameters (key and channel) to modify the ongoing note expression. Setting any of these
    // parameters to -1 will skip them in the search (so, for example, you can search only by
    // channel+key, or only by note_id).
    //
    // For example, to play a note at an arbitrary hz frequency "f" for a given strike velocity
    // "velocity", you could do the following:
    //   float k = 12.f*log2(f/440.f) + 69.f;  // Convert into standard 12TET
    //   int mk = int(std::floor(k));          // The "key" value in 12TET.
    //   float off = k - mk;                   // And the leftover hz outside of the standard key.
    //   int note_id = <pick something arbitrary>;
    //   playNote(0, mk, velocity, 0, note_id);
    //   setNoteExpression(SurgeVoice::PITCH, note_id, mk, -1, off);
    // Then, later, call releaseNote() as appropriate.
    void setNoteExpression(SurgeVoice::NoteExpressionType net, int32_t note_id, int16_t key,
                           int16_t channel, float value);

    bool getParameterIsBoolean(const ID &index) const;

    bool stringToNormalizedValue(const ID &index, std::string s, float &outval) const;

    float normalizedToValue(const ID &index, float val) const
    {
        return normalizedToValue(index.getSynthSideId(), val);
    }

    float valueToNormalized(const ID &index, float val) const
    {
        return valueToNormalized(index.getSynthSideId(), val);
    }

    void sendParameterAutomation(const ID &index, float val)
    {
        sendParameterAutomation(index.getSynthSideId(), val);
    }

  private:
    bool setParameter01(long index, float value, bool external = false, bool force_integer = false);
    void sendParameterAutomation(long index, float value);
    float getParameter01(long index) const;
    float getParameter(long index) const;
    float normalizedToValue(long parameterIndex, float value) const;
    float valueToNormalized(long parameterIndex, float value) const;
    void getParameterDisplay(long index, char *text) const;
    void getParameterDisplay(long index, char *text, float x) const;
    void getParameterDisplayAlt(long index, char *text) const;
    void getParameterName(long index, char *text) const;
    void getParameterNameExtendedByFXGroup(long index, char *text) const;
    void getParameterAccessibleName(long index, char *text) const;
    void getParameterMeta(long index, parametermeta &pm) const;

  public:
    void updateDisplay();
    bool isValidModulation(long ptag, modsources modsource) const;
    bool isActiveModulation(long ptag, modsources modsource, int modsourceScene, int index) const;
    bool isAnyActiveModulation(long ptag, modsources modsource,
                               int modsourceScene) const; // independent of index
    bool isBipolarModulation(modsources modsources) const;
    bool isModsourceUsed(modsources modsource); // FIXME - this should be const
    bool isModDestUsed(long moddest) const;
    bool isModulatorDistinctPerScene(modsources modsource) const; // Modwheel no; SLFO2 yes. etc...

    bool supportsIndexedModulator(int scene, modsources modsource) const;
    int getMaxModulationIndex(int scene, modsources modsource) const;
    std::vector<int> getModulationIndicesBetween(long ptag, modsources modsource,
                                                 int modsourceScene) const;
    ModulationRouting *getModRouting(long ptag, modsources modsource, int modsourceScene,
                                     int index) const;

    /*
     * setModDepth01 etc take a modsource scene. This is only needed for global modulations
     * since for in-scene modulations the parameter implicit in ptag has a scene. But for
     * LFOs modulating FX, we need to know which scene they originate from. See #2285
     */
    bool setModDepth01(long ptag, modsources modsource, int modsourceScene, int index, float value);
    float getModDepth01(long ptag, modsources modsource, int modsourceScene, int index) const;
    float getModDepth(long ptag, modsources modsource, int modsourceScene, int index) const;
    void muteModulation(long ptag, modsources modsource, int modsourceScene, int index, bool mute);
    bool isModulationMuted(long ptag, modsources modsource, int modsourceScene, int index) const;
    void clearModulation(long ptag, modsources modsource, int modsourceScene, int index,
                         bool clearEvenIfInvalid);
    // clear the modulation routings on the algorithm-specific sliders
    void clear_osc_modulation(int scene, int entry);

    /*
     * The modulation API (setModDepth01 etc...) is called from all sorts of places
     * but mostly from the UI. This adds a listener which gets notified but this listener
     * should just post a message over to the UI thread and be quick in case there
     * are cases where the audio thread calls set/clear/mute modulation.
     *
     * Be super careful with threading here basically. It doesn't lock so make sure you
     * are not in a situation where you add or remove a listener while a modulation is
     * being changed.
     */
    struct ModulationAPIListener
    {
        virtual ~ModulationAPIListener() = default;
        virtual void modSet(long ptag, modsources modsource, int modsourceScene, int index,
                            float value, bool isNewModulation) = 0;
        virtual void modMuted(long ptag, modsources modsource, int modsourceScene, int index,
                              bool mute) = 0;
        virtual void modCleared(long ptag, modsources modsource, int modsourceScene, int index) = 0;

        /*
         * These two methods are called optionally if an event is initiated by a user drag
         * action or so on. They will only be called on the UI thread. Not every set etc...
         * is contained in a begin/end and mute and cleared almost definitely will never
         * be wrapped.
         */
        virtual void modBeginEdit(long ptag, modsources modsource, int modsourceScene, int index,
                                  float depth01)
        {
        }
        virtual void modEndEdit(long ptag, modsources modsource, int modsourceScene, int index,
                                float depth01)
        {
        }
    };
    std::set<ModulationAPIListener *> modListeners;
    void addModulationAPIListener(ModulationAPIListener *l) { modListeners.insert(l); }
    void removeModulationAPIListener(ModulationAPIListener *l) { modListeners.erase(l); }

  public:
    std::atomic<bool> rawLoadEnqueued{false}, rawLoadNeedsUIDawExtraState{false};
    std::mutex rawLoadQueueMutex;
    std::unique_ptr<char[]> enqueuedLoadData{nullptr}; // if this is set I need to free it
    int enqueuedLoadSize{0};
    void enqueuePatchForLoad(const void *data, int size); // safe from any thread
    void processEnqueuedPatchIfNeeded();                  // only safe from audio thread

    void loadRaw(const void *data, int size, bool preset = false);
    void loadPatch(int id);
    bool loadPatchByPath(const char *fxpPath, int categoryId, const char *name,
                         bool forceIsPreset = true);
    void selectRandomPatch();
    std::unique_ptr<std::thread> patchLoadThread;

    // if increment is true, we go to next patch, else go to previous patch
    void jogCategory(bool increment);
    void jogPatch(bool increment, bool insideCategory = true);
    void jogPatchOrCategory(bool increment, bool isCategory, bool insideCategory = true);

    void swapMetaControllers(int ct1, int ct2);

    void savePatchToPath(fs::path p, bool refreshPatchList = true);
    void savePatch(bool factoryInPlace = false, bool skipOverwrite = false);
    void updateUsedState();
    void prepareModsourceDoProcess(int scenemask);
    unsigned int saveRaw(void **data);

    //==============================================================================
    // --- 'patch loaded' listener(s) ----
    // Listeners are notified whenever a patch changes, with the path of the new patch.
    // Listeners should do any significant work on their own thread; for example, the OSC
    // patchLoadedListener calls OSCSender::send(), which runs on a juce::MessageManager thread.
    //
    // Be sure to delete any added listeners in the destructor of the class that added them.
    std::unordered_map<std::string, std::function<void(const fs::path &)>> patchLoadedListeners;
    void addPatchLoadedListener(std::string key, std::function<void(const fs::path &)> const &l)
    {
        patchLoadedListeners.insert({key, l});
    }
    void deletePatchLoadedListener(std::string key) { patchLoadedListeners.erase(key); }

    //==============================================================================
    // Parameter changes coming from within the synth (e.g. from MIDI-learned input)
    // are communicated to listeners here
    std::unordered_map<std::string, std::function<void(const std::string oscname, const float fval,
                                                       std::string valstr)>>
        audioThreadParamListeners;

    void addAudioParamListener(std::string key,
                               std::function<void(const std::string oscname, const float fval,
                                                  std::string valstr)> const &l)
    {
        audioThreadParamListeners.insert({key, l});
    }
    void deleteAudioParamListener(std::string key) { audioThreadParamListeners.erase(key); }

    //==============================================================================
    // synth -> editor variables
    bool refresh_editor, refresh_vkb, patch_loaded;
    int learn_param_from_cc, learn_macro_from_cc, learn_param_from_note;
    int refresh_ctrl_queue[8];
    int refresh_parameter_queue[8];
    bool refresh_overflow = false;
    float refresh_ctrl_queue_value[8];
    bool process_input;
    std::atomic<bool> has_patchid_file;
    char patchid_file[FILENAME_MAX];
    std::atomic<int> patchid_queue;

    // updated in audio thread, read from UI, so have assignments be atomic
    std::atomic<int> hasUpdatedMidiCC;
    std::atomic<int> modwheelCC, pitchbendMIDIVal, sustainpedalCC;
    std::atomic<bool> midiSoftTakeover;

    float vu_peak[8];
    std::atomic<float> cpu_level{0.f};

    void populateDawExtraState();

    void loadFromDawExtraState();

  public:
    int CC0, CC32, PCH, patchid;
    float masterfade = 0;
    bool approachingAllSoundOff{false};
    // TODO: FIX SCENE ASSUMPTION (for halfbandA/B - use std::array)
    sst::filters::HalfRate::HalfRateFilter halfbandA, halfbandB, halfbandIN;
    std::list<SurgeVoice *> voices[n_scenes];
    std::unique_ptr<Effect> fx[n_fx_slots];
    std::atomic<bool> halt_engine;
    MidiChannelState channelState[16];
    bool &mpeEnabled;
    int mpeVoices = 0;
    int mpeGlobalPitchBendRange = 0;

    std::bitset<128> disallowedLearnCCs{0};
    std::array<uint64_t, 128> midiKeyPressedForScene[n_scenes];
    uint64_t orderedMidiKey = 0;
    std::atomic<uint64_t> midiNoteEvents{0};

    int current_category_id = 0;
    bool modsourceused[n_modsources];
    bool midiprogramshavechanged = false;

    bool switch_toggled_queued, release_if_latched[n_scenes], release_anyway[n_scenes];
    void setParameterSmoothed(long index, float value);

    static constexpr int n_hpBQ = 4;

    // TODO: FIX SCENE ASSUMPTION (use std::array)
    std::array<BiquadFilter, n_hpBQ> hpA, hpB;

    bool fx_reload[n_fx_slots]; // if true, reload new effect parameters from fxsync
    FxStorage fxsync[n_fx_slots]{
        FxStorage(fxslot_ains1),   FxStorage(fxslot_ains2),   FxStorage(fxslot_bins1),
        FxStorage(fxslot_bins2),   FxStorage(fxslot_send1),   FxStorage(fxslot_send2),
        FxStorage(fxslot_global1), FxStorage(fxslot_global2), FxStorage(fxslot_ains3),
        FxStorage(fxslot_ains4),   FxStorage(fxslot_bins3),   FxStorage(fxslot_bins4),
        FxStorage(fxslot_send3),   FxStorage(fxslot_send4),   FxStorage(fxslot_global3),
        FxStorage(fxslot_global4)}; // used for synchronisation of parameter init
    bool fx_reload_mod[n_fx_slots];

    struct FXModSyncItem
    {
        int source_id;
        int source_scene;
        int source_index;
        int whichForReal;
        float depth;
        bool muted{false};
    };
    std::array<std::vector<FXModSyncItem>, n_fx_slots> fxmodsync;
    int32_t fx_suspend_bitmask;

    // hold pedal stuff
    struct HoldBufferItem
    {
        int channel;
        int key;
        int originalChannel;
        int originalKey;
        int32_t host_noteid;
    };
    std::list<HoldBufferItem> holdbuffer[n_scenes];
    void purgeHoldbuffer(int scene);
    void purgeDuplicateHeldVoicesInPolyMode(int scehe, int channel, int key);
    void stopSound();

    QuadFilterChainState *FBQ[n_scenes];

    std::string hostProgram = "Unknown Host";
    std::string juceWrapperType = "Unknown Wrapper Type";
    bool activateExtraOutputs = true;

    void changeModulatorSmoothing(Modulator::SmoothingMode m);

    void queueForRefresh(int param_index);

    // these have to be thread-safe, so keep them private
  private:
    PluginLayer *_parent = nullptr;

    void switch_toggled();

    // MIDI control interpolators
    static constexpr int num_controlinterpolators = 128;
    ControllerModulationSource mControlInterpolator[num_controlinterpolators];
    bool mControlInterpolatorUsed[num_controlinterpolators];

    int GetFreeControlInterpolatorIndex();
    int GetControlInterpolatorIndex(int Idx);
    void ReleaseControlInterpolator(int Idx);
    ControllerModulationSource *ControlInterpolator(int Idx);
    ControllerModulationSource *AddControlInterpolator(int Idx, bool &AlreadyExisted);
};

namespace std
{

template <> struct hash<SurgeSynthesizer::ID>
{
    std::size_t operator()(const SurgeSynthesizer::ID &k) const
    {
        return std::hash<int>()(k.getSynthSideId());
    }
};

} // namespace std
#endif // SURGE_SRC_COMMON_SURGESYNTHESIZER_H
