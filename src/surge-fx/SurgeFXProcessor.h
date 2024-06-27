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

#ifndef SURGE_SRC_SURGE_FX_SURGEFXPROCESSOR_H
#define SURGE_SRC_SURGE_FX_SURGEFXPROCESSOR_H

#include "SurgeStorage.h"
#include "Effect.h"
#include "FXOpenSoundControl.h"

#include "sst/filters/HalfRateFilter.h"

#include "juce_audio_processors/juce_audio_processors.h"

#if MAC
#include <execinfo.h>
#endif

//==============================================================================
/**
 */
class SurgefxAudioProcessor : public juce::AudioProcessor,
                              public juce::AudioProcessorParameter::Listener,
                              public juce::AsyncUpdater
{
  public:
    //==============================================================================
    SurgefxAudioProcessor();
    ~SurgefxAudioProcessor();

    float input_buffer alignas(16)[2][BLOCK_SIZE];
    float sidechain_buffer alignas(16)[2][BLOCK_SIZE];
    float output_buffer alignas(16)[2][BLOCK_SIZE];
    int input_position{0};
    int output_position{-1};

    bool nonLatentBlockMode{true};

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    void processBlockOSC();

    //==============================================================================
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    // Open Sound Control
    enum oscToAudio_type
    {
        FX_PARAM
    };

    // Message from OSC input to the audio thread
    struct oscToAudio
    {
        oscToAudio_type type;
        int p_index{0};
        float fval{0.0};

        oscToAudio() {}
        explicit oscToAudio(oscToAudio_type omtype, float f, int pidx)
            : type(omtype), fval(f), p_index(pidx)
        {
        }
    };
    sst::cpputils::SimpleRingBuffer<oscToAudio, 4096> oscRingBuf;

    SurgeFX::FxOSC::FXOpenSoundControl oscHandler;
    std::atomic<bool> oscCheckStartup{false};
    void tryLazyOscStartupFromStreamedState();

    bool initOSCIn(int port);
    bool changeOSCInPort(int newport);
    void initOSCError(int port, std::string outIP = "");
    bool oscReceiving = false;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    int getEffectType() { return effectNum; }
    float getFXStorageValue01(int i) { return fxstorage->p[fx_param_remap[i]].get_value_f01(); }
    float getFXParamValue01(int i) { return *(fxParams[i]); }
    void setFXParamValue01(int i, float f) { *(fxParams[i]) = f; }

    void setFXParamTempoSync(int i, bool b)
    {
        int v = paramFeatures[i];
        if (b)
            v = v | kTempoSync;
        else
            v = v & ~kTempoSync;
        paramFeatures[i] = v;
    }

    bool getFXParamTempoSync(int i) { return (paramFeatures[i]) & kTempoSync; }
    void setFXStorageTempoSync(int i, bool b) { fxstorage->p[fx_param_remap[i]].temposync = b; }
    bool getFXStorageTempoSync(int i) { return fxstorage->p[fx_param_remap[i]].temposync; }
    bool canTempoSync(int i) { return fxstorage->p[fx_param_remap[i]].can_temposync(); }

    void setFXParamExtended(int i, bool b)
    {
        int v = (paramFeatures[i]);
        if (b)
            v = v | kExtended;
        else
            v = v & ~kExtended;
        paramFeatures[i] = v;
    }
    bool getFXParamExtended(int i) { return paramFeatures[i] & kExtended; }
    void setFXStorageExtended(int i, bool b)
    {
        fxstorage->p[fx_param_remap[i]].set_extend_range(b);
    }
    bool getFXStorageExtended(int i) { return fxstorage->p[fx_param_remap[i]].extend_range; }
    bool canExtend(int i) { return fxstorage->p[fx_param_remap[i]].can_extend_range(); }

    void setFXParamAbsolute(int i, bool b)
    {
        int v = paramFeatures[i];
        if (b)
            v = v | kAbsolute;
        else
            v = v & ~kAbsolute;
        paramFeatures[i] = v;
    }
    bool getFXParamAbsolute(int i) { return paramFeatures[i] & kAbsolute; }
    void setFXStorageAbsolute(int i, bool b) { fxstorage->p[fx_param_remap[i]].absolute = b; }
    bool getFXStorageAbsolute(int i) { return fxstorage->p[fx_param_remap[i]].absolute; }
    bool canAbsolute(int i) { return fxstorage->p[fx_param_remap[i]].can_be_absolute(); }

    void setFXParamDeactivated(int i, bool b)
    {
        int v = paramFeatures[i];
        if (b)
            v = v | kDeactivated;
        else
            v = v & ~kDeactivated;
        paramFeatures[i] = v;
    }
    bool getFXParamDeactivated(int i) { return paramFeatures[i] & kDeactivated; }
    void setFXStorageDeactivated(int i, bool b) { fxstorage->p[fx_param_remap[i]].deactivated = b; }
    bool getFXStorageDeactivated(int i) { return fxstorage->p[fx_param_remap[i]].deactivated; }
    bool getFXStorageAppearsDeactivated(int i)
    {
        return fxstorage->p[fx_param_remap[i]].appears_deactivated();
    }
    bool canDeactitvate(int i) { return fxstorage->p[fx_param_remap[i]].can_deactivate(); }

    virtual void parameterValueChanged(int parameterIndex, float newValue) override
    {
        if (supressParameterUpdates)
            return;

        if (!isUserEditing[parameterIndex])
        {
            // this order does matter
            changedParamsValue[parameterIndex] = newValue;
            changedParams[parameterIndex] = true;
            triggerAsyncUpdate();
        }
    }

    virtual void parameterGestureChanged(int parameterIndex, bool gestureStarting) override {}

    virtual void handleAsyncUpdate() override
    {
        paramChangeListener();
        for (int i = 0; i < n_fx_params; ++i)
            if (wasParamFeatureChanged[i])
            {
                wasParamFeatureChanged[i] = false;
            }
    }

    void setParameterChangeListener(std::function<void()> l) { paramChangeListener = l; }

    // Call this from the UI thread
    void copyChangeValues(bool *c, float *f)
    {
        for (int i = 0; i < n_fx_params + 1; ++i)
        {
            c[i] = changedParams[i];
            changedParams[i] = false;
            f[i] = changedParamsValue[i];
        }
    }

    virtual void setUserEditingFXParam(int i, bool isEd)
    {
        isUserEditing[i] = isEd;
        if (isEd)
        {
            fxBaseParams[i]->beginChangeGesture();
        }
        else
        {
            fxBaseParams[i]->endChangeGesture();
        }
    }

    virtual void setUserEditingParamFeature(int i, bool b)
    {
        if (b)
        {
            // Used to send a start change here but we don't have params any more
        }
        else
        {
            wasParamFeatureChanged[i] = true;
            triggerAsyncUpdate();
        }
    }

    int32_t paramFeatureFromParam(Parameter *p)
    {
        return (p->temposync ? kTempoSync : 0) + (p->extend_range ? kExtended : 0) +
               (p->absolute ? kAbsolute : 0) + (p->appears_deactivated() ? kDeactivated : 0);
    };

    void paramFeatureOntoParam(Parameter *p, int32_t features)
    {
        p->temposync = features & kTempoSync;
        p->set_extend_range(features & kExtended);
        p->absolute = features & kAbsolute;
        p->deactivated = features & kDeactivated;
    }

    // Information about parameter strings
    bool getParamEnabled(int i) { return fxstorage->p[fx_param_remap[i]].ctrltype != ct_none; }
    std::string getParamGroup(int i) { return group_names[i]; }

    std::string getParamName(int i)
    {
        if (fxstorage->p[fx_param_remap[i]].ctrltype == ct_none)
            return "-";

        return fxstorage->p[fx_param_remap[i]].get_name();
    }

    std::string getParamValue(int i)
    {
        if (fxstorage->p[fx_param_remap[i]].ctrltype == ct_none)
        {
            return "-";
        }

        return fxstorage->p[fx_param_remap[i]].get_display(false, 0);
    }

    std::string getParamValueFor(int idx, float f)
    {
        if (fxstorage->p[fx_param_remap[idx]].ctrltype == ct_none)
        {
            return "-";
        }

        return fxstorage->p[fx_param_remap[idx]].get_display(true, f);
    }

    std::string getParamValueFromFloat(int i, float f)
    {
        if (fxstorage->p[fx_param_remap[i]].ctrltype == ct_none)
        {
            return "-";
        }

        fxstorage->p[fx_param_remap[i]].set_value_f01(f);

        return fxstorage->p[fx_param_remap[i]].get_display(false, 0);
    }

    void updateJuceParamsFromStorage();

    void resetFxType(int t, bool updateJuceParams = true);
    void resetFxParams(bool updateJuceParams = true);

    // Members for the FX. If this looks a lot like surge-rack/SurgeFX.hpp that's not a coincidence
    std::unique_ptr<SurgeStorage> storage;

    std::atomic<bool> m_audioValid{true};
    std::string m_audioValidMessage{};

    sst::filters::HalfRate::HalfRateFilter halfbandIN{6, true};

  private:
    template <typename T, typename F> struct FXAudioParameter : public T
    {
        juce::String mutableName;
        template <typename... Args>
        FXAudioParameter(Args &&...args) : T(std::forward<Args>(args)...)
        {
            mutableName = T::getName(64);
        }

        FXAudioParameter<T, F> &operator=(F newValue)
        {
            T::operator=(newValue);
            return *this;
        }

        juce::String getName(int end) const override { return mutableName.substring(0, end); }

        std::function<juce::String(float, int)> getTextHandler;
        std::function<float(const juce::String &)> getTextToValue;
        juce::String getText(float f, int i) const override { return getTextHandler(f, i); }

        float getValueForText(const juce::String &text) const override
        {
            return getTextToValue(text);
        }
    };

    //==============================================================================
    juce::AudioProcessorParameter *fxBaseParams[2 * n_fx_params + 1];

    // These are just copyes of the pointer from above with the cast done to make the code look
    // nicer
    typedef FXAudioParameter<juce::AudioParameterFloat, float> float_param_t;
    typedef FXAudioParameter<juce::AudioParameterInt, int> int_param_t;
    float_param_t *fxParams[n_fx_params];
    int_param_t *fxType;

    enum ParamFeatureFlags
    {
        kTempoSync = 1U << 0U,
        kExtended = 1U << 1U,
        kAbsolute = 1U << 2U,
        kDeactivated = 1U << 3U
    };
    std::atomic<int> paramFeatures[n_fx_params];
    std::atomic<bool> changedParams[n_fx_params + 1];
    std::atomic<float> changedParamsValue[n_fx_params + 1];
    std::atomic<bool> isUserEditing[n_fx_params + 1];
    std::atomic<bool> wasParamFeatureChanged[n_fx_params];
    std::function<void()> paramChangeListener;

    float lastBPM = -1;
    bool supressParameterUpdates = false;
    struct SupressGuard
    {
        bool *s;
        SupressGuard(bool *sg)
        {
            s = sg;
            *s = true;
        }
        ~SupressGuard() { *s = false; }
    };

    std::shared_ptr<Effect> surge_effect;
    std::shared_ptr<Effect> audio_thread_surge_effect;
    std::atomic<bool> resettingFx;
    FxStorage *fxstorage;
    int storage_id_start, storage_id_end;

    int effectNum;

    int fx_param_remap[n_fx_params];
    std::string group_names[n_fx_params];

    void reorderSurgeParams();
    void copyGlobaldataSubset(int start, int end);
    void setupStorageRanges(Parameter *start, Parameter *endIncluding);

    std::atomic<bool> audioRunning{false};

  public:
    bool oscStartIn = false;
    int oscPortIn = 53290;

    void prepareParametersAbsentAudio();
    void setParameterByString(int i, const std::string &s);
    float getParameterValueForString(int i, const std::string &s);
    bool canSetParameterByString(int i);

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SurgefxAudioProcessor)
};

#endif // SURGE_SRC_SURGE_FX_SURGEFXPROCESSOR_H
