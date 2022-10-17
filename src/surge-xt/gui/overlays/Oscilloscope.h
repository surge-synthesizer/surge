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
#include "widgets/MultiSwitch.h"

#include "juce_core/juce_core.h"
#include "juce_dsp/juce_dsp.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include "sst/cpputils.h"

namespace Surge
{
namespace Overlays
{

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
    Waveform waveform_;
};

} // namespace Overlays
} // namespace Surge
