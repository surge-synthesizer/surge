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

    Oscilloscope(SurgeGUIEditor *e, SurgeStorage *s);
    virtual ~Oscilloscope();

    void onSkinChanged() override;
    void paint(juce::Graphics &g) override;
    void resized() override;
    void visibilityChanged() override;

  private:
    enum ChannelSelect
    {
        LEFT = 1,
        RIGHT = 2,
        STEREO = 3,
        OFF = 4,
    };

    float freqToX(float freq, int width) const;
    float xToFreq(float x, int width) const;
    float dbToY(float db, int height) const;

    void calculateScopeData();
    void pullData();
    void toggleChannel();

    static constexpr float lowFreq = 10;
    static constexpr float highFreq = 24000;
    static constexpr float dbMin = -100;
    static constexpr float dbMax = 0;
    static constexpr float dbRange = dbMax - dbMin;

    SurgeGUIEditor *editor_{nullptr};
    SurgeStorage *storage_{nullptr};
    juce::dsp::FFT forward_fft_;
    juce::dsp::WindowingFunction<float> window_;
    std::array<float, 2 * fftSize> fft_data_;
    int pos_;
    std::array<float, fftSize> scope_data_;
    std::mutex scope_data_guard_;
    ChannelSelect channel_selection_;
    std::mutex channel_selection_guard_;

    // Members for the data-pulling thread.
    std::thread fft_thread_;
    std::atomic_bool complete_;
    juce::WaitableEvent repainted_;
    std::condition_variable channels_off_;

    // Visual elements.
    Surge::Widgets::SelfDrawToggleButton left_chan_button_;
    Surge::Widgets::SelfDrawToggleButton right_chan_button_;
};

} // namespace Overlays
} // namespace Surge
