/*
 ** Surge Synthesizer is Free and Open Source Software
 **
 ** Surge is made available under the Gnu General Public License, v3.0
 ** https://www.gnu.org/licenses/gpl-3.0.en.html
 **
 ** Copyright 2004-2021 by various individuals as described by the Git transaction log
 **
 ** All source at: https://github.com/surge-synthesizer/surge.git
 **
 ** Surge was a commercial product from 2004-2018, with Copyright and ownership
 ** in that period held by Claes Johanson at Vember Audio. Claes made Surge
 ** open source in September 2018.
 */

#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "OverlayComponent.h"
#include "SkinSupport.h"
#include "SurgeGUICallbackInterfaces.h"
#include "SurgeGUIEditor.h"
#include "SurgeStorage.h"
#include "widgets/ModulatableSlider.h"
#include "widgets/MultiSwitch.h"

#include "juce_core/juce_core.h"
#include "juce_dsp/juce_dsp.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include "sst/cpputils.h"

namespace Surge
{
namespace Overlays
{

// Waveform-specific display taken from s(m)exoscope GPL code and adapted to use with Surge.
class WaveformDisplay : public juce::Component, public Surge::GUI::SkinConsumingComponent
{
  public:
    enum TriggerType
    {
        kTriggerFree = 0,
        kTriggerRising,
        kTriggerFalling,
        kTriggerInternal,
        kNumTriggerTypes
    };

    struct Parameters
    {
        float trigger_speed = 0.5f;              // internal trigger speed, knob
        TriggerType trigger_type = kTriggerFree; // trigger type, selection
        float trigger_level = 0.5f;              // trigger level, slider
        float trigger_limit = 0.5f;              // retrigger threshold, knob
        float time_window = 0.75f;               // X-range, knob
        float amp_window = 0.5f;                 // Y-range, knob
        bool sync_draw = false;                  // sync redraw, on/off
        bool freeze = false;                     // freeze display, on/off
        bool dc_kill = false;                    // kill DC, on/off
    };

    WaveformDisplay(SurgeGUIEditor *e, SurgeStorage *s);

    const Parameters &getParameters() const;
    void setParameters(Parameters parameters);

    bool frozen() const;

    void paint(juce::Graphics &g) override;
    void resized() override;
    void process(std::vector<float> data);

  private:
    SurgeGUIEditor *editor_;
    SurgeStorage *storage_;
    Parameters params_;

    // Global lock for the class.
    std::mutex lock_;

    std::vector<juce::Point<float>> peaks;
    std::vector<juce::Point<float>> copy;
    std::vector<float> scope_data_;
    std::size_t pos_;

    // Index into the peak-array.
    std::size_t index;

    // counter which is used to set the amount of samples/pixel.
    double counter;

    // max/min peak in this block.
    float max, min, maxR, minR;

    // the last peak we encountered was a maximum?
    bool lastIsMax;

    // the previous sample (for edge-triggers)
    float previousSample;

    // the internal trigger oscillator
    double triggerPhase;

    // trigger limiter
    int triggerLimitPhase;

    // DC killer.
    double dcKill, dcFilterTemp;
};

class Oscilloscope : public OverlayComponent,
                     public Surge::GUI::SkinConsumingComponent,
                     public Surge::GUI::Hoverable
{
  public:
    static constexpr int fftOrder = 13;
    static constexpr int fftSize = 8192;
    static constexpr float lowFreq = 10;
    static constexpr float highFreq = 24000;
    static constexpr float dbMin = -100;
    static constexpr float dbMax = 0;
    static constexpr float dbRange = dbMax - dbMin;

    Oscilloscope(SurgeGUIEditor *e, SurgeStorage *s);
    virtual ~Oscilloscope();

    void onSkinChanged() override;
    void paint(juce::Graphics &g) override;
    void resized() override;
    void updateDrawing();
    void visibilityChanged() override;

  private:
    enum ChannelSelect
    {
        LEFT = 1,
        RIGHT = 2,
        STEREO = 3,
        OFF = 4,
    };

    enum ScopeMode
    {
        WAVEFORM = 1,
        SPECTRUM = 2,
    };

    // Really wish span was available.
    using FftScopeType = std::array<float, fftSize / 2>;

    // Really wish std::chrono didn't suck so badly.
    using FloatSeconds = std::chrono::duration<float>;

    // Child component for handling the drawing of the background. Done as a separate child instead
    // of in the Oscilloscope class so the display, which is repainting at 20-30 hz, doesn't mark
    // this as dirty as it repaints. It sucks up a ton of CPU otherwise.
    class Background : public juce::Component, public Surge::GUI::SkinConsumingComponent
    {
      public:
        explicit Background();
        void paint(juce::Graphics &g) override;
        void updateBackgroundType(ScopeMode mode);
        void updateBounds(juce::Rectangle<int> local_bounds, juce::Rectangle<int> scope_bounds);

      private:
        void paintSpectrogramBackground(juce::Graphics &g);
        void paintWaveformBackground(juce::Graphics &g);

        ScopeMode mode_;
        juce::Rectangle<int> scope_bounds_;
    };

    // Child component for handling the drawing of the spectrogram.
    class Spectrogram : public juce::Component, public Surge::GUI::SkinConsumingComponent
    {
      public:
        Spectrogram(SurgeGUIEditor *e, SurgeStorage *s);

        void paint(juce::Graphics &g) override;
        void resized() override;
        void updateScopeData(FftScopeType::iterator begin, FftScopeType::iterator end);

      private:
        float interpolate(const float y0, const float y1,
                          std::chrono::time_point<std::chrono::steady_clock> t) const;

        SurgeGUIEditor *editor_;
        SurgeStorage *storage_;
        std::chrono::duration<float> mtbs_;
        std::chrono::time_point<std::chrono::steady_clock> last_updated_time_;
        std::mutex data_lock_;
        FftScopeType new_scope_data_;
        FftScopeType displayed_data_;
    };

    // Child component for handling drawing of the waveform.
    class Waveform : public juce::Component, public Surge::GUI::SkinConsumingComponent
    {
      public:
        Waveform(SurgeGUIEditor *e, SurgeStorage *s);

        void paint(juce::Graphics &g) override;
        void scroll();
        void updateAudioData(const std::vector<float> &buf);

      private:
        static constexpr const float refreshRate = 60.f;

        SurgeGUIEditor *editor_;
        SurgeStorage *storage_;
        std::mutex data_lock_;
        std::chrono::milliseconds period_;
        FloatSeconds period_float_;
        std::size_t period_samples_;
        // scope_data_ will buffer an entire second of data, not just what's displayed.
        std::vector<float> scope_data_;
        sst::cpputils::SimpleRingBuffer<float, fftSize> upcoming_data_;
        int last_sample_rate_;
    };

    class SpectrogramParameters : public juce::Component, public Surge::GUI::SkinConsumingComponent
    {
      public:
        SpectrogramParameters(SurgeGUIEditor *e, SurgeStorage *s);

        void onSkinChanged() override;
        void paint(juce::Graphics &g) override;
        void resized() override;

      private:
        SurgeGUIEditor *editor_;
        SurgeStorage *storage_;
    };

    class WaveformParameters : public juce::Component, public Surge::GUI::SkinConsumingComponent
    {
      public:
        WaveformParameters(SurgeGUIEditor *e, SurgeStorage *s);

        std::optional<WaveformDisplay::Parameters> getParamsIfDirty();

        void onSkinChanged() override;
        void paint(juce::Graphics &g) override;
        void resized() override;

      private:
        SurgeGUIEditor *editor_;
        SurgeStorage *storage_;
        WaveformDisplay::Parameters params_;
        bool params_changed_;
        std::mutex params_lock_;

        Surge::Widgets::SelfUpdatingModulatableSlider trigger_speed_;
        Surge::Widgets::SelfUpdatingModulatableSlider trigger_level_;
        Surge::Widgets::SelfUpdatingModulatableSlider trigger_limit_;
        Surge::Widgets::SelfUpdatingModulatableSlider time_window_;
        Surge::Widgets::SelfUpdatingModulatableSlider amp_window_;
        //WaveformTriggerTypeButton trigger_type_;
    };

    #if 0
    class WaveformTriggerTypeButton : public Surge::Widgets::MultiSwitchSelfDraw,
                                      public Surge::GUI::IComponentTagValue::Listener
    {
      public:
        explicit WaveformTriggerTypeButton(WaveformDisplay &display);
        void valueChanged(Surge::GUI::IcomponentTagValue *p) override;

      private:
        WaveformDisplay &display_;
    };
    #endif

    // Child component for handling the switch between waveform/spectrum.
    class SwitchButton : public Surge::Widgets::MultiSwitchSelfDraw,
                         public Surge::GUI::IComponentTagValue::Listener
    {
      public:
        explicit SwitchButton(Oscilloscope &parent);
        void valueChanged(Surge::GUI::IComponentTagValue *p) override;

      private:
        Oscilloscope &parent_;
    };

    static float freqToX(float freq, int width);
    static float timeToX(std::chrono::milliseconds time, int width);
    static float dbToY(float db, int height);
    static float magToY(float mag, int height);

    // Height of parameter window, in pixels.
    static constexpr const int paramsHeight = 78;

    void calculateSpectrumData();
    void changeScopeType(ScopeMode type);
    juce::Rectangle<int> getScopeRect();
    void pullData();
    void toggleChannel();

    SurgeGUIEditor *editor_{nullptr};
    SurgeStorage *storage_{nullptr};
    juce::dsp::FFT forward_fft_;
    juce::dsp::WindowingFunction<float> window_;
    std::array<float, 2 * fftSize> fft_data_;
    int pos_;
    FftScopeType scope_data_;
    ChannelSelect channel_selection_;
    ScopeMode scope_mode_;
    // Global lock for all data members accessed concurrently.
    std::mutex data_lock_;

    // Members for the data-pulling thread.
    std::thread fft_thread_;
    std::atomic_bool complete_;
    std::condition_variable channels_off_;

    // Visual elements.
    Surge::Widgets::SelfDrawToggleButton left_chan_button_;
    Surge::Widgets::SelfDrawToggleButton right_chan_button_;
    SwitchButton scope_mode_button_;
    Background background_;
    Spectrogram spectrogram_;
    // Waveform waveform_;
    WaveformDisplay waveform_;
    SpectrogramParameters spectrogram_parameters_;
    WaveformParameters waveform_parameters_;
};

} // namespace Overlays
} // namespace Surge
