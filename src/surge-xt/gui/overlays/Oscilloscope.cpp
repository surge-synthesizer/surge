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

#include "Oscilloscope.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include "RuntimeFont.h"
#include "SkinColors.h"

using namespace std::chrono_literals;

namespace Surge
{
namespace Overlays
{

float Oscilloscope::freqToX(float freq, int width)
{
    static const float ratio = std::log(highFreq / lowFreq);
    float xNorm = std::log(freq / lowFreq) / ratio;
    return xNorm * (float)width;
}

float Oscilloscope::dbToY(float db, int height) { return (float)height * (dbMax - db) / dbRange; }

// TODO:
// (1) Give configuration to the user to choose FFT params (namely, desired Hz resolution).
// (2) Provide a mode that shows waveshape, not spectrum.
Oscilloscope::Oscilloscope(SurgeGUIEditor *e, SurgeStorage *s)
    : editor_(e), storage_(s), forward_fft_(fftOrder),
      window_(fftSize, juce::dsp::WindowingFunction<float>::hann), pos_(0), complete_(false),
      fft_thread_(std::bind(std::mem_fn(&Oscilloscope::pullData), this)),
      channel_selection_(STEREO), left_chan_button_("L"), right_chan_button_("R"),
      spectrogram_(e, s)
{
    setAccessible(true);
    setOpaque(true);

    auto onToggle = std::bind(std::mem_fn(&Oscilloscope::toggleChannel), this);
    left_chan_button_.setStorage(storage_);
    left_chan_button_.setToggleState(true);
    left_chan_button_.onToggle = onToggle;
    left_chan_button_.setOpaque(true);
    left_chan_button_.setBufferedToImage(true);
    left_chan_button_.setAccessible(true);
    left_chan_button_.setTitle("L CHAN");
    left_chan_button_.setDescription("Enable input from left channel.");
    left_chan_button_.setWantsKeyboardFocus(false);
    right_chan_button_.setStorage(storage_);
    right_chan_button_.setToggleState(true);
    right_chan_button_.onToggle = onToggle;
    right_chan_button_.setOpaque(true);
    right_chan_button_.setBufferedToImage(true);
    right_chan_button_.setAccessible(true);
    right_chan_button_.setTitle("R CHAN");
    right_chan_button_.setDescription("Enable input from right channel.");
    right_chan_button_.setWantsKeyboardFocus(false);
    addAndMakeVisible(background_);
    addAndMakeVisible(left_chan_button_);
    addAndMakeVisible(right_chan_button_);
    addAndMakeVisible(spectrogram_);

    storage_->audioOut.subscribe();
}

Oscilloscope::~Oscilloscope()
{
    // complete_ should come before any condition variables get signaled, to allow the data thread
    // to finish up.
    complete_.store(true, std::memory_order_seq_cst);
    {
        std::lock_guard l(channel_selection_guard_);
        channel_selection_ = OFF;
        channels_off_.notify_all();
    }
    fft_thread_.join();
    // Data thread can perform subscriptions, so do a final unsubscribe after it's done.
    storage_->audioOut.unsubscribe();
}

void Oscilloscope::onSkinChanged()
{
    background_.setSkin(skin, associatedBitmapStore);
    left_chan_button_.setSkin(skin, associatedBitmapStore);
    right_chan_button_.setSkin(skin, associatedBitmapStore);
    spectrogram_.setSkin(skin, associatedBitmapStore);
}

void Oscilloscope::paint(juce::Graphics &g) {}

void Oscilloscope::resized()
{
    auto t = getTransform().inverted();
    auto h = getHeight();
    auto w = getWidth();
    t.transformPoint(w, h);

    background_.updateBounds(getLocalBounds(), getScopeRect());
    left_chan_button_.setBounds(8, 4, 15, 15);
    right_chan_button_.setBounds(23, 4, 15, 15);
    spectrogram_.setBounds(getScopeRect());
}

void Oscilloscope::updateDrawing()
{
    std::lock_guard l(channel_selection_guard_);
    if (channel_selection_ != OFF)
    {
        spectrogram_.repaint();
    }
}

void Oscilloscope::visibilityChanged()
{
    // Not sure aside from construction when visibility might be changed in Juce, so putting this
    // here for additional safety.
    if (isVisible())
    {
        storage_->audioOut.subscribe();
    }
    else
    {
        storage_->audioOut.unsubscribe();
    }
}

void Oscilloscope::calculateScopeData()
{
    window_.multiplyWithWindowingTable(fft_data_.data(), fftSize);
    forward_fft_.performFrequencyOnlyForwardTransform(fft_data_.data());

    float binHz = storage_->samplerate / static_cast<float>(fftSize);
    for (int i = 0; i < fftSize / 2; i++)
    {
        float hz = binHz * static_cast<float>(i);
        if (hz < lowFreq || hz > highFreq)
        {
            scope_data_[i] = dbMin;
        }
        else
        {
            scope_data_[i] = juce::jlimit(dbMin, dbMax,
                                          juce::Decibels::gainToDecibels(fft_data_[i]) -
                                              juce::Decibels::gainToDecibels((float)fftSize));
        }
    }
}

juce::Rectangle<int> Oscilloscope::getScopeRect()
{
    auto lb = getLocalBounds().transformedBy(getTransform().inverted());
    auto scopeRect = lb.withTrimmedBottom(15).withTrimmedTop(15).withTrimmedRight(30).reduced(8);
    return scopeRect;
}

void Oscilloscope::pullData()
{
    while (!complete_.load(std::memory_order_seq_cst))
    {
        std::unique_lock csl(channel_selection_guard_);
        if (channel_selection_ == OFF)
        {
            // We want to unsubscribe and sleep if we aren't going to be looking at the data, to
            // prevent useless accumulation and CPU usage.
            storage_->audioOut.unsubscribe();
            channels_off_.wait(csl, [this]() {
                return channel_selection_ != OFF || complete_.load(std::memory_order_seq_cst);
            });
            storage_->audioOut.subscribe();
            continue;
        }
        ChannelSelect cs = channel_selection_;
        csl.unlock();

        std::pair<std::vector<float>, std::vector<float>> data = storage_->audioOut.popall();
        std::vector<float> &dataL = data.first;
        std::vector<float> &dataR = data.second;
        if (dataL.empty())
        {
            // Sleep for long enough to accumulate about 4096 samples.
            std::this_thread::sleep_for(std::chrono::duration<float, std::chrono::seconds::period>(
                fftSize / 2.f / storage_->samplerate));
            continue;
        }

        // We'll use "dataL" as our storage regardless of the channel choice.
        if (cs == STEREO)
        {
            std::transform(dataL.cbegin(), dataL.cend(), dataR.cbegin(), dataL.begin(),
                           [](float x, float y) { return (x + y) / 2.f; });
        }
        else if (cs == RIGHT)
        {
            dataL = dataR;
        }

        int sz = dataL.size();
        if (pos_ + sz >= fftSize)
        {
            int mv = fftSize - pos_;
            int leftovers = sz - mv;
            std::move(dataL.begin(), dataL.begin() + mv, fft_data_.begin() + pos_);
            calculateScopeData();
            spectrogram_.updateScopeData(scope_data_.begin(), scope_data_.end());
            std::move(dataL.begin() + mv, dataL.end(), fft_data_.begin());
            pos_ = leftovers;
        }
        else
        {
            std::move(dataL.begin(), dataL.end(), fft_data_.begin() + pos_);
            pos_ += sz;
        }
    }
}

void Oscilloscope::toggleChannel()
{
    std::lock_guard l(channel_selection_guard_);
    if (left_chan_button_.getToggleState() && right_chan_button_.getToggleState())
    {
        channel_selection_ = STEREO;
    }
    else if (left_chan_button_.getToggleState())
    {
        channel_selection_ = LEFT;
    }
    else if (right_chan_button_.getToggleState())
    {
        channel_selection_ = RIGHT;
    }
    else
    {
        channel_selection_ = OFF;
    }
    channels_off_.notify_all();
}

Oscilloscope::Background::Background() { setOpaque(true); }

void Oscilloscope::Background::paint(juce::Graphics &g)
{
    juce::Graphics::ScopedSaveState g1(g);
    g.fillAll(skin->getColor(Colors::MSEGEditor::Background));

    auto scopeRect = scope_bounds_;
    auto width = scopeRect.getWidth();
    auto height = scopeRect.getHeight();
    auto labelHeight = 9;
    auto font = skin->fontManager->getLatoAtSize(7);
    auto primaryLine = skin->getColor(Colors::MSEGEditor::Grid::Primary);
    auto secondaryLine = skin->getColor(Colors::MSEGEditor::Grid::SecondaryVertical);

    // Horizontal grid.
    {
        auto gs = juce::Graphics::ScopedSaveState(g);
        g.addTransform(juce::AffineTransform().translated(scopeRect.getX(), scopeRect.getY()));
        g.setFont(font);

        // Draw frequency lines.
        for (float freq : {20.f, 30.f, 40.f, 60.f, 80.f, 100.f, 200.f, 300.f, 400.f, 600.f, 800.f,
                           1000.f, 2000.f, 3000.f, 4000.f, 6000.f, 8000.f, 10000.f, 20000.f})
        {
            const auto xPos = freqToX(freq, width);

            if (freq == 100.f || freq == 1000.f || freq == 10000.f)
            {
                g.setColour(primaryLine);
            }
            else
            {
                g.setColour(secondaryLine);
            }

            g.drawVerticalLine(xPos, 0, static_cast<float>(height));

            const bool over1000 = freq >= 1000.f;
            const auto freqString =
                juce::String(over1000 ? freq / 1000.f : freq) + (over1000 ? "k" : "");
            // Label will go past the end of the scopeRect.
            const auto labelRect = juce::Rectangle{font.getStringWidth(freqString), labelHeight}
                                       .withBottomY(height + 13)
                                       .withRightX((int)xPos);

            g.setColour(skin->getColor(Colors::MSEGEditor::Axis::Text));
            g.drawFittedText(freqString, labelRect, juce::Justification::bottom, 1);
        }
    }

    // Vertical grid.
    {
        auto gs = juce::Graphics::ScopedSaveState(g);
        g.addTransform(juce::AffineTransform().translated(scopeRect.getX(), scopeRect.getY()));
        g.setFont(font);

        // Draw dB lines.
        for (float dB :
             {-100.f, -90.f, -80.f, -70.f, -60.f, -50.f, -40.f, -30.f, -20.f, -10.f, 0.f})
        {
            const auto yPos = dbToY(dB, height);

            if (dB == 0.f)
            {
                g.setColour(primaryLine);
            }
            else
            {
                g.setColour(secondaryLine);
            }
            g.drawHorizontalLine(yPos, 0, static_cast<float>(width));

            const auto dbString = juce::String(dB) + " dB";
            // Label will go past the end of the scopeRect.
            const auto labelRect = juce::Rectangle{font.getStringWidth(dbString), labelHeight}
                                       .withBottomY((int)yPos)
                                       .withRightX(width + 30); // -2

            g.setColour(skin->getColor(Colors::MSEGEditor::Axis::SecondaryText));
            g.drawFittedText(dbString, labelRect, juce::Justification::right, 1);
        }
    }
}

void Oscilloscope::Background::updateBounds(juce::Rectangle<int> local_bounds,
                                            juce::Rectangle<int> scope_bounds)
{
    scope_bounds_ = std::move(scope_bounds);
    setBounds(local_bounds);
}

Oscilloscope::Spectrogram::Spectrogram(SurgeGUIEditor *e, SurgeStorage *s)
    : editor_(e), storage_(s), last_updated_time_(std::chrono::steady_clock::now())
{
    std::fill(new_scope_data_.begin(), new_scope_data_.end(), dbMin);
}

void Oscilloscope::Spectrogram::paint(juce::Graphics &g)
{
    auto scopeRect = getLocalBounds().transformedBy(getTransform().inverted());
    auto width = scopeRect.getWidth();
    auto height = scopeRect.getHeight();
    auto curveColor = skin->getColor(Colors::MSEGEditor::Curve);
    auto gs = juce::Graphics::ScopedSaveState(g);

    auto path = juce::Path();
    bool started = false;
    float binHz = storage_->samplerate / static_cast<float>(fftSize);
    float zeroPoint = dbToY(dbMin, height);
    float maxPoint = dbToY(dbMax, height);
    auto now = std::chrono::steady_clock::now();

    // Start path.
    path.startNewSubPath(freqToX(lowFreq, width), zeroPoint);
    {
        std::lock_guard l(data_lock_);
        mtbs_ = std::chrono::duration<float>(1.f / binHz);

        for (int i = 0; i < fftSize / 2; i++)
        {
            const float hz = binHz * static_cast<float>(i);
            if (hz < lowFreq || hz > highFreq)
            {
                continue;
            }

            const float x = freqToX(hz, width);
            const float y0 = displayed_data_[i];
            const float y1 = dbToY(new_scope_data_[i], height);
            const float y = interpolate(y0, y1, now);
            displayed_data_[i] = y;
            if (y > 0)
            {
                if (started)
                {
                    path.lineTo(x, y);
                }
                else
                {
                    path.startNewSubPath(x, zeroPoint);
                    path.lineTo(x, y);
                    started = true;
                }
            }
            else
            {
                path.lineTo(x, zeroPoint);
                path.closeSubPath();
                started = false;
            }
        }
    }
    // End path.
    if (started)
    {
        path.lineTo(freqToX(highFreq, width), zeroPoint);
        path.closeSubPath();
    }
    g.setColour(curveColor);
    g.fillPath(path);
}

void Oscilloscope::Spectrogram::resized()
{
    auto scopeRect = getLocalBounds().transformedBy(getTransform().inverted());
    auto height = scopeRect.getHeight();
    std::fill(displayed_data_.begin(), displayed_data_.end(), dbToY(-100, height));
}

void Oscilloscope::Spectrogram::updateScopeData(FftScopeType::iterator begin,
                                                FftScopeType::iterator end)
{
    // Data comes in as dB (from dbMin to dbMax).
    std::lock_guard l(data_lock_);
    std::move(begin, end, new_scope_data_.begin());
    last_updated_time_ = std::chrono::steady_clock::now();
}

float Oscilloscope::Spectrogram::interpolate(
    const float y0, const float y1, std::chrono::time_point<std::chrono::steady_clock> t) const
{
    std::chrono::duration<float> distance = (t - last_updated_time_);
    float mu = juce::jlimit(0.f, 1.f, distance / mtbs_);
    return y0 * (1 - mu) + y1 * mu;
}

} // namespace Overlays
} // namespace Surge
