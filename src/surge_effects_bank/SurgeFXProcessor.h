/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "SurgeStorage.h"
#include <functional>
#include "dsp/effect/Effect.h"

#if MAC
#include <execinfo.h>
#endif

//==============================================================================
/**
*/
class SurgefxAudioProcessor  : public AudioProcessor,
    public AudioProcessorParameter::Listener,
    public AsyncUpdater
{
public:
    //==============================================================================
    SurgefxAudioProcessor();
    ~SurgefxAudioProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

    //==============================================================================
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;    


    int getEffectType()
    {
        return effectNum;
    }
    float getFXStorageValue01(int i)
    {
        return fxstorage->p[fx_param_remap[i]].get_value_f01();
    }
    float getFXParamValue01(int i)
    {
        return *(fxParams[i]);
    }
    void setFXParamValue01(int i, float f)
    {
        *(fxParams[i]) = f;
    }
    void setFXParamTempoSync(int i, bool b)
    {
        fxTempoSync[i]->setValueNotifyingHost( b );
    }
    bool getFXParamTempoSync(int i)
    {
        return *(fxTempoSync[i]);
    }
    void setFXStorageTempoSync(int i, bool b)
    {
        fxstorage->p[fx_param_remap[i]].temposync = b;
    }
    bool getFXStorageTempoSync(int i)
    {
        return fxstorage->p[fx_param_remap[i]].temposync;
    }
    bool canTempoSync(int i)
    {
        return fxstorage->p[fx_param_remap[i]].can_temposync();
    }
    
    virtual void parameterValueChanged(int parameterIndex, float newValue ) override {
        if( supressParameterUpdates ) return;
        
        if( ! isUserEditing[parameterIndex] )
        {
            // this order does matter
            changedParamsValue[parameterIndex] = newValue;
            changedParams[parameterIndex] = true;
            triggerAsyncUpdate();
        }
    }

    virtual void parameterGestureChanged(int parameterIndex, bool gestureStarting ) override {
    }


    virtual void handleAsyncUpdate() override {
        paramChangeListener();
        for( int i=0; i<n_fx_params; ++i )
            if( wasTempoSyncChanged[i] )
            {
                wasTempoSyncChanged[i] = false;
                fxTempoSync[i]->endChangeGesture();
            }
    }

    void setParameterChangeListener(std::function<void()> l) {
        paramChangeListener = l;
    }

    // Call this from the UI thread
    void copyChangeValues(bool *c, float *f)
    {
        for( int i=0; i<n_fx_params+1; ++i )
        {
            c[i] = changedParams[i];
            changedParams[i] = false;
            f[i] = changedParamsValue[i];
        }
    }

    virtual void setUserEditingFXParam(int i, bool isEd)
    {
        isUserEditing[i] = isEd;
        if(isEd)
        {
            fxBaseParams[i]->beginChangeGesture();
        }
        else
        {
            fxBaseParams[i]->endChangeGesture();
        }
    }

    virtual void setUserEditingTemposync(int i, bool b)
    {
        if(b)
        {
            fxTempoSync[i]->beginChangeGesture();
        }
        else
        {
            wasTempoSyncChanged[i] = true;
            triggerAsyncUpdate();
        }
    }

    // Information about parameter strings
    bool getParamEnabled(int i) {
        return fxstorage->p[fx_param_remap[i]].ctrltype != ct_none;
    }
    std::string getParamGroup(int i) {
        return group_names[i];
    }

    std::string getParamName(int i) {
        if( fxstorage->p[fx_param_remap[i]].ctrltype == ct_none )
            return "-";

        return fxstorage->p[fx_param_remap[i]].get_name();
    }

    std::string getParamValue(int i) {
        if( fxstorage->p[fx_param_remap[i]].ctrltype == ct_none )
        {
            return "-";
        }
        
        char txt[1024];
        fxstorage->p[fx_param_remap[i]].get_display(txt, false, 0);
        return txt;
    }

    std::string getParamValueFromFloat(int i, float f) {
        if( fxstorage->p[fx_param_remap[i]].ctrltype == ct_none )
        {
            return "-";
        }
        
        char txt[1024];
        fxstorage->p[fx_param_remap[i]].set_value_f01(f);
        fxstorage->p[fx_param_remap[i]].get_display(txt, false, 0);
        return txt;
    }

    void updateJuceParamsFromStorage();
    
    void resetFxType(int t, bool updateJuceParams = true);
    void resetFxParams( bool updateJuceParams = true );
    
private:
    //==============================================================================
    AudioProcessorParameter *fxBaseParams[2 * n_fx_params + 1];

    // These are just copyes of the pointer from above with the cast done to make the code look nicer
    AudioParameterFloat *fxParams[n_fx_params];
    AudioParameterInt *fxType;
    AudioParameterBool *fxTempoSync[n_fx_params];
    
    std::atomic<bool>    changedParams[2*n_fx_params+1];
    std::atomic<float>   changedParamsValue[2*n_fx_params+1];
    std::atomic<bool>    isUserEditing[2*n_fx_params+1];
    std::atomic<bool>    wasTempoSyncChanged[n_fx_params];
    std::function<void()>  paramChangeListener;
    float lastBPM = -1;
    bool supressParameterUpdates = false;
    struct SupressGuard {
        bool *s;
        SupressGuard( bool *sg ) {
            s = sg;
            *s = true;
        }
        ~SupressGuard() { *s = false; }
    };
    
    // Members for the FX. If this looks a lot like surge-rack/SurgeFX.hpp that's not a coincidence
    std::unique_ptr<SurgeStorage> storage;
    
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SurgefxAudioProcessor)
};
