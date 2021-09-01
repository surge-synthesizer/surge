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

#pragma once
#include "SurgeStorage.h"
#include "SurgeVoice.h"
#include "Effect.h"
#include "BiquadFilter.h"

struct QuadFilterChainState;

#include <list>
#include <utility>
#include <atomic>
#include <cstdio>

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
    bool audio_processing_active;

    // aligned stuff
    SurgeStorage storage alignas(16);
    lipol_ps FX alignas(16)[n_send_slots], amp alignas(16), amp_mute alignas(16),
        send alignas(16)[n_send_slots][n_scenes];

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
    void playNote(char channel, char key, char velocity, char detune);
    void releaseNote(char channel, char key, char velocity);
    void releaseNotePostHoldCheck(int scene, char channel, char key, char velocity);
    void pitchBend(char channel, int value);
    void polyAftertouch(char channel, int key, int value);
    void channelAftertouch(char channel, int value);
    void channelController(char channel, int cc, int value);
    void programChange(char channel, int value);
    void allNotesOff();
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
     * processThreadunsafeOperations reloads a patch if the audio thread isn't running
     * but if it is running lets the deferred queue handle it. But it has an option
     * which is *extremely dangerous* to force you to load in the current thread immediately.
     * If you use this option and dont' know what you are doing it will explode - basically
     * we only use it in the startup constructor path.
     */
    void processThreadunsafeOperations(bool doItEvenIfAudioIsRunningDANGER = false);
    bool loadFx(bool initp, bool force_reload_all);
    bool loadOscalgos();
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

    void playVoice(int scene, char channel, char key, char velocity, char detune);
    void releaseScene(int s);
    int calculateChannelMask(int channel, int key);
    void softkillVoice(int scene);
    void enforcePolyphonyLimit(int scene, int margin);
    int getNonUltrareleaseVoices(int scene) const;
    int getNonReleasedVoices(int scene) const;

    SurgeVoice *getUnusedVoice(int scene); // not const since it updates voice state
    void freeVoice(SurgeVoice *);
    std::array<std::array<SurgeVoice, MAX_VOICES>, 2> voices_array;
    unsigned int voices_usedby[2][MAX_VOICES]; // 0 indicates no user, 1 is scene A & 2 is scene B
                                               // // TODO: FIX SCENE ASSUMPTION!
    int64_t voiceCounter = 1L;

    std::atomic<unsigned int> processRunning{0};

  public:
    /*
     * So when surge was pre-juce we contemplated writing our own ID remapping between
     * internal indices and DAW IDs so put an indirectin and class in place. JUCE obviates
     * the need for that by using hash of stremaing name as an ID consistently throuhg its
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

    void setMacroParameter01(long macroNum, float val);
    float getMacroParameter01(long macroNum) const;

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

    bool supportsIndexedModulator(int scene, modsources modsource) const;
    int getMaxModulationIndex(int scene, modsources modsource) const;
    std::vector<int> getModulationIndicesBetween(long ptag, modsources modsource,
                                                 int modsourceScene) const;
    ModulationRouting *getModRouting(long ptag, modsources modsource, int modsourceScene,
                                     int index) const;

    /*
     * setModulation etc take a modsource scene. This is only needed for global modulations
     * since for in-scene modulations the paramter implicit in ptag has a scene. But for
     * LFOs modulating FX, we need to knwo which scene they originate from. See #2285
     */
    bool setModulation(long ptag, modsources modsource, int modsourceScene, int index, float value);
    float getModulation(long ptag, modsources modsource, int modsourceScene, int index) const;
    void muteModulation(long ptag, modsources modsource, int modsourceScene, int index, bool mute);
    bool isModulationMuted(long ptag, modsources modsource, int modsourceScene, int index) const;
    float getModDepth(long ptag, modsources modsource, int modsourceScene, int index) const;
    void clearModulation(long ptag, modsources modsource, int modsourceScene, int index,
                         bool clearEvenIfInvalid = false);
    void clear_osc_modulation(
        int scene, int entry); // clear the modulation routings on the algorithm-specific sliders
  public:
    std::atomic<bool> rawLoadEnqueued{false}, rawLoadNeedsUIDawExtraState{false};
    std::mutex rawLoadQueueMutex;
    void *enqueuedLoadData{nullptr}; // if this is set I need to free it
    int enqueuedLoadSize{0};
    void enqueuePatchForLoad(void *data, int size); // safe from any thread
    void processEnqueuedPatchIfNeeded();            // only safe from audio thread

    void loadRaw(const void *data, int size, bool preset = false);
    void loadPatch(int id);
    bool loadPatchByPath(const char *fxpPath, int categoryId, const char *name);
    void incrementPatch(bool nextPrev, bool insideCategory = true);
    void incrementCategory(bool nextPrev);
    void selectRandomPatch();

    void swapMetaControllers(int ct1, int ct2);

    void savePatchToPath(fs::path p);
    void savePatch();
    void updateUsedState();
    void prepareModsourceDoProcess(int scenemask);
    unsigned int saveRaw(void **data);
    // synth -> editor variables
    std::atomic<int>
        polydisplay; // updated in audio thread, read from ui, so have assignments be atomic
    bool refresh_editor, patch_loaded;
    int learn_param, learn_custom;
    int refresh_ctrl_queue[8];
    int refresh_parameter_queue[8];
    bool refresh_overflow = false;
    float refresh_ctrl_queue_value[8];
    bool process_input;
    std::atomic<int> patchid_queue;
    bool has_patchid_file;
    char patchid_file[FILENAME_MAX];

    float vu_peak[8];

    void populateDawExtraState();

    void loadFromDawExtraState();

  public:
    int CC0, CC32, PCH, patchid;
    float masterfade = 0;
    HalfRateFilter halfbandA, halfbandB,
        halfbandIN; // TODO: FIX SCENE ASSUMPTION (for halfbandA/B - use std::array)
    std::list<SurgeVoice *> voices[n_scenes];
    std::unique_ptr<Effect> fx[n_fx_slots];
    std::atomic<bool> halt_engine;
    MidiChannelState channelState[16];
    bool mpeEnabled = false;
    int mpeVoices = 0;
    int mpeGlobalPitchBendRange = 0;

    std::array<uint64_t, 128> midiKeyPressedForScene[n_scenes];
    uint64_t orderedMidiKey = 0;

    int current_category_id = 0;
    bool modsourceused[n_modsources];
    bool midiprogramshavechanged = false;

    bool switch_toggled_queued, release_if_latched[n_scenes], release_anyway[n_scenes];
    void setParameterSmoothed(long index, float value);
    BiquadFilter hpA, hpB; // TODO: FIX SCENE ASSUMPTION (use std::array)

    bool fx_reload[n_fx_slots];   // if true, reload new effect parameters from fxsync
    FxStorage fxsync[n_fx_slots]; // used for synchronisation of parameter init
    bool fx_reload_mod[n_fx_slots];
    std::array<std::vector<std::tuple<int, int, int, int, float>>, n_fx_slots> fxmodsync;
    int32_t fx_suspend_bitmask;

    // hold pedal stuff

    std::list<std::pair<int, int>> holdbuffer[n_scenes];
    void purgeHoldbuffer(int scene);
    quadr_osc sinus;
    int demo_counter = 0;

    QuadFilterChainState *FBQ[n_scenes];

    std::string hostProgram = "Unknown Host";
    std::string juceWrapperType = "Unknown Wrapper Type";
    bool activateExtraOutputs = true;
    void setupActivateExtraOutputs();

    void changeModulatorSmoothing(ControllerModulationSource::SmoothingMode m);

    // these have to be thread-safe, so keep private
  private:
    PluginLayer *_parent = nullptr;

    void switch_toggled();

    // midicontrol-interpolators
    static const int num_controlinterpolators = 128;
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