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

// TODO:
// (1) Give configuration to the user to choose FFT params (namely, desired Hz resolution).
// (2) Provide a mode that shows waveshape, not spectrum.
Oscilloscope::Oscilloscope(SurgeGUIEditor *e, SurgeStorage *s)
    : editor_(e), storage_(s), forward_fft_(fftOrder),
      window_(fftSize, juce::dsp::WindowingFunction<float>::hann), pos_(0), complete_(false),
      fft_thread_(std::bind(std::mem_fn(&Oscilloscope::pullData), this)), repainted_(true),
      channel_selection_(STEREO), left_chan_button_("L"), right_chan_button_("R")
{
    setAccessible(true);

    auto onToggle = std::bind(std::mem_fn(&Oscilloscope::toggleChannel), this);
    left_chan_button_.setStorage(storage_);
    left_chan_button_.setToggleState(true);
    left_chan_button_.onToggle = onToggle;
    left_chan_button_.setAccessible(true);
    left_chan_button_.setTitle("L CHAN");
    left_chan_button_.setDescription("Enable input from left channel.");
    right_chan_button_.setStorage(storage_);
    right_chan_button_.setToggleState(true);
    right_chan_button_.onToggle = onToggle;
    right_chan_button_.setAccessible(true);
    right_chan_button_.setTitle("R CHAN");
    right_chan_button_.setDescription("Enable input from right channel.");
    addAndMakeVisible(left_chan_button_);
    addAndMakeVisible(right_chan_button_);

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
    repainted_.signal();
    fft_thread_.join();
    // Data thread can perform subscriptions, so do a final unsubscribe after it's done.
    storage_->audioOut.unsubscribe();
}

void Oscilloscope::onSkinChanged()
{
    left_chan_button_.setSkin(skin, associatedBitmapStore);
    right_chan_button_.setSkin(skin, associatedBitmapStore);
}

void Oscilloscope::paint(juce::Graphics &g)
{
    g.fillAll(skin->getColor(Colors::MSEGEditor::Background));

    auto lb = getLocalBounds().transformedBy(getTransform().inverted());
    auto scopeRect = lb.withTrimmedBottom(15).withTrimmedTop(15).withTrimmedRight(30).reduced(8);
    auto width = scopeRect.getWidth();
    auto height = scopeRect.getHeight();
    auto labelHeight = 9;
    auto font = skin->fontManager->getLatoAtSize(7);
    auto primaryLine = skin->getColor(Colors::MSEGEditor::Grid::Primary);
    auto secondaryLine = skin->getColor(Colors::MSEGEditor::Grid::SecondaryVertical);
    auto curveColor = skin->getColor(Colors::MSEGEditor::Curve);

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
            juce::Line line{juce::Point{xPos, 0.f}, juce::Point{xPos, (float)height}};

            if (freq == 100.f || freq == 1000.f || freq == 10000.f)
            {
                g.setColour(primaryLine);
            }
            else
            {
                g.setColour(secondaryLine);
            }

            g.drawLine(line);

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

            juce::Line line{juce::Point{0.f, yPos}, juce::Point{(float)width, yPos}};

            if (dB == 0.f)
            {
                g.setColour(primaryLine);
            }
            else
            {
                g.setColour(secondaryLine);
            }
            g.drawLine(line);

            const auto dbString = juce::String(dB) + " dB";
            // Label will go past the end of the scopeRect.
            const auto labelRect = juce::Rectangle{font.getStringWidth(dbString), labelHeight}
                                       .withBottomY((int)yPos)
                                       .withRightX(width + 30); // -2

            g.setColour(skin->getColor(Colors::MSEGEditor::Axis::SecondaryText));
            g.drawFittedText(dbString, labelRect, juce::Justification::right, 1);
        }
    }

    // Scope data.
    {
        auto gs = juce::Graphics::ScopedSaveState(g);
        g.addTransform(juce::AffineTransform().translated(scopeRect.getX(), scopeRect.getY()));

        auto path = juce::Path();
        bool started = false;
        float binHz = storage_->samplerate / static_cast<float>(fftSize);
        float zeroPoint = dbToY(-100, height);
        std::lock_guard<std::mutex> l(scope_data_guard_);
        // Start path.
        path.startNewSubPath(freqToX(lowFreq, width), zeroPoint);
        for (int i = 0; i < fftSize / 2; i++)
        {
            float hz = binHz * static_cast<float>(i);
            if (hz < lowFreq || hz > highFreq)
            {
                continue;
            }

            float x = freqToX(hz, width);
            float y = dbToY(scope_data_[i], height);
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
        // End path.
        if (started)
        {
            path.lineTo(freqToX(highFreq, width), zeroPoint);
            path.closeSubPath();
        }
        g.setColour(curveColor);
        g.fillPath(path);
        repainted_.signal();
    }
}

void Oscilloscope::resized()
{
    auto t = getTransform().inverted();
    auto h = getHeight();
    auto w = getWidth();
    t.transformPoint(w, h);

    left_chan_button_.setBounds(8, 4, 15, 15);
    right_chan_button_.setBounds(23, 4, 15, 15);
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

float Oscilloscope::freqToX(float freq, int width) const
{
    static const float ratio = std::log(highFreq / lowFreq);
    float xNorm = std::log(freq / lowFreq) / ratio;
    return xNorm * (float)width;
}

float Oscilloscope::xToFreq(float x, int width) const
{
    static const float ratio = std::log(highFreq / lowFreq);
    return lowFreq * std::exp(ratio * x / width);
}

float Oscilloscope::dbToY(float db, int height) const
{
    return (float)height * (dbMax - db) / dbRange;
}

void Oscilloscope::calculateScopeData()
{
    window_.multiplyWithWindowingTable(fft_data_.data(), fftSize);
    forward_fft_.performFrequencyOnlyForwardTransform(fft_data_.data());

    float binHz = storage_->samplerate / static_cast<float>(fftSize);
    std::lock_guard<std::mutex> l(scope_data_guard_);
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
                4096.f / storage_->samplerate));
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
            // We want to let at least one painting happen, so it drains the data we just queued up.
            repainted_.reset();
            // Is the following line safe, if the Window gets closed and the class gets
            // destructed?
            juce::MessageManager::getInstance()->callAsync([this]() { this->repaint(); });
            repainted_.wait();
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

} // namespace Overlays
} // namespace Surge
