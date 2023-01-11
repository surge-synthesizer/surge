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
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <fmt/core.h>
#include "RuntimeFont.h"
#include "SkinColors.h"

using namespace std::chrono_literals;
using std::placeholders::_1;

namespace Surge
{
namespace Overlays
{

float WaveformDisplay::Parameters::counterSpeed() const
{
    return std::pow(10.f, -time_window * 5.f + 1.5f);
}
float WaveformDisplay::Parameters::triggerLevel() const { return trigger_level * 2.f - 1.f; }
float WaveformDisplay::Parameters::gain() const { return std::pow(10.f, amp_window * 6.f - 3.f); }

WaveformDisplay::WaveformDisplay(SurgeGUIEditor *e, SurgeStorage *s)
    : editor_(e), storage_(s), counter(1.0), max(std::numeric_limits<float>::min()),
      min(std::numeric_limits<float>::min())
{
}

const WaveformDisplay::Parameters &WaveformDisplay::getParameters() const { return params_; }

void WaveformDisplay::setParameters(Parameters parameters)
{
    std::lock_guard l(lock_);
    params_ = std::move(parameters);
}

void WaveformDisplay::mouseDown(const juce::MouseEvent &event)
{
    clickPoint = event.getEventRelativeTo(this).getMouseDownPosition();
}

void WaveformDisplay::paint(juce::Graphics &g)
{
    std::lock_guard l(lock_);
    auto curveColor = skin->getColor(Colors::MSEGEditor::Curve);
    auto path = juce::Path();

    // waveform
    std::vector<juce::Point<float>> &points = params_.sync_draw ? copy : peaks;
    float counterSpeedInverse = 1 / params_.counterSpeed();

    g.setColour(curveColor);
    if (counterSpeedInverse < 1.0) // draw interpolated lines
    {
        float phase = counterSpeedInverse;
        float dphase = counterSpeedInverse;

        float prevxi = points[0].x;
        float prevyi = points[0].y;
        path.startNewSubPath(prevxi, prevyi);

        for (std::size_t i = 1; i < getWidth() - 1; i++)
        {
            int index = static_cast<int>(phase);
            float alpha = phase - static_cast<float>(index);

            float xi = i;
            float yi = (1.0 - alpha) * points[index * 2].y + alpha * points[(index + 1) * 2].y;
            path.lineTo(xi, yi);
            prevxi = xi;
            prevyi = yi;

            phase += dphase;
        }
    }
    else
    {
        path.startNewSubPath(points[0].x, points[0].y);
        for (std::size_t i = 1; i < points.size(); i++)
        {
            path.lineTo(points[i].x, points[i].y);
        }
    }
    g.strokePath(path, juce::PathStrokeType(1.f));

#if 0
    //TODO: See about adding the readout / click point.
    if (where.x != -1) {
        CPoint whereOffset = where;
        whereOffset.offsetInverse(offset);

        pContext->drawLine(CPoint(0, whereOffset.y).offset(offset), CPoint(getViewSize().getWidth() - 1, whereOffset.y).offset(offset));
        pContext->drawLine(CPoint(whereOffset.x, 0).offset(offset), CPoint(whereOffset.x, getViewSize().getHeight() - 1).offset(offset));

        float gain = powf(10.f, effect->getParameter(CSmartelectronixDisplay::kAmpWindow) * 6.f - 3.f);
        float y = (-2.f * ((float)whereOffset.y + 1.f) / (float)OSC_HEIGHT + 1.f) / gain;
        float x = (float)whereOffset.x * (float)counterSpeedInverse;
        std::string text;

        long lineSize = 10;

        CColor color(179, 111, 56);

        pContext->setFontColor(color);
        pContext->setFont(kNormalFontSmaller);

        readout->draw(pContext, CRect(508, 8, 508 + readout->getWidth(), 8 + readout->getHeight()).offset(offset), CPoint(0, 0));

        CRect textRect(512, 10, 652, 10 + lineSize);
        textRect.offset(offset);

        text = fmt::format("y = {.5f}", y);
        pContext->drawString(text, textRect, kLeftText, true);
        textRect.offset(0, lineSize);

        text = fmt::format("y = {.5f} dB", cf_lin2db(fabsf(y)));
        pContext->drawString(text, textRect, kLeftText, true);
        textRect.offset(0, lineSize * 2);

        text = fmt::format("x = {.2f} spl", x);
        pContext->drawString(text, textRect, kLeftText, true);
        textRect.offset(0, lineSize);

        text = fmt::format("x = {.5f} s", x / effect->getSampleRate());
        pContext->drawString(text, textRect, kLeftText, true);
        textRect.offset(0, lineSize);

        text = fmt::format("x = {.5f} ms", 1000.f * x / effect->getSampleRate());
        pContext->drawString(text, textRect, kLeftText, true);
        textRect.offset(0, lineSize);

        if (x == 0)
            text = fmt::format("x = -inf Hz");
        else
            text = fmt::format("x = {.3f} Hz", effect->getSampleRate() / x);

        pContext->drawString(text, textRect, kLeftText, true);
    }
#endif
}

void WaveformDisplay::process(std::vector<float> data)
{
    std::unique_lock l(lock_);
    if (params_.freeze)
    {
        return;
    }

    float gain = params_.gain();
    float triggerLevel = params_.triggerLevel();
    int triggerLimit =
        static_cast<int>(std::pow(10.f, params_.trigger_limit * 4.f)); // 0=>1, 1=>10000
    float triggerSpeed = std::pow(10.f, 2.5f * params_.trigger_speed - 5.f);
    float counterSpeed = params_.counterSpeed();
    float R = 1.f - 250.f / static_cast<float>(storage_->samplerate);

    for (float &f : data)
    {
        // DC filter
        dcKill = f - dcFilterTemp + R * dcKill;
        dcFilterTemp = f;

        if (std::abs(dcKill) < 1e-10)
        {
            dcKill = 0.f;
        }

        // Gain
        float sample = params_.dc_kill ? static_cast<float>(dcKill) : f;
        sample = juce::jlimit(-1.f, 1.f, sample * gain);

        // Triggers
        bool trigger = false;
        switch (params_.trigger_type)
        {
        case kTriggerInternal:
            // internal oscillator, nothing fancy
            triggerPhase += triggerSpeed;
            if (triggerPhase >= 1.0)
            {
                triggerPhase -= 1.0;
                trigger = true;
            }
            break;
        case kTriggerRising:
            // trigger on a rising edge
            // fixme: something is wrong with this triggering mechanism
            if (sample >= triggerLevel && previousSample < triggerLevel)
            {
                trigger = true;
            }
            break;
        case kTriggerFalling:
            // trigger on a falling edge
            // fixme: something is wrong with this triggering mechanism
            if (sample <= triggerLevel && previousSample > triggerLevel)
            {
                trigger = true;
            }
            break;
        case kTriggerFree:
            // trigger when we've run out of the screen area
            if (index >= getWidth())
            {
                trigger = true;
            }
            break;
        default:
            // Should never happen.
            std::cout << "Invalid trigger type. This should never happen..." << std::endl;
            std::abort();
        }

        // if there's a retrigger, but too fast, kill it
        triggerLimitPhase++;
        if (trigger && triggerLimitPhase < triggerLimit && params_.trigger_type != kTriggerFree &&
            params_.trigger_type != kTriggerInternal)
        {
            trigger = false;
        }

        // @ trigger
        if (trigger)
        {
            std::size_t j;

            // zero peaks after the last one
            for (j = index * 2; j < getWidth() * 2; j += 2)
            {
                peaks[j].y = peaks[j + 1].y = juce::jmap<float>(0, -1, 1, getHeight(), 0);
            }

            // copy to a buffer for sync drawing
            for (j = 0; j < getWidth() * 2; j++)
            {
                copy[j].y = peaks[j].y;
            }

            // reset everything
            index = 0;
            counter = 1.0;
            max = std::numeric_limits<float>::lowest();
            min = std::numeric_limits<float>::max();
            triggerLimitPhase = 0;
        }

        // @ sample
        if (sample > max)
        {
            max = sample;
            lastIsMax = true;
        }

        if (sample < min)
        {
            min = sample;
            lastIsMax = false;
        }

        counter += counterSpeed;

        // @ counter
        // The counter keeps track of how many samples/pixel we have.
        //
        // How this works: counter is based off of a user parameter. When counter = 1, we have 1
        // incoming sample per pixel. When it's 10, we have 10 pixels per incoming sample. And when
        // it's 0.1, we have, you guessed it, 10 pixels per 1 incoming sample.
        //
        // JUCE can handle all the subpixel drawing no problem, but it's ungodly slow at it. So
        // instead we squash the data down here with maxes/mins per pixel.
        if (counter >= 1.0)
        {
            if (index < getWidth())
            {
                // Perform scaling here so we don't have to redo it over and over in painting.
                float max_Y = juce::jmap<float>(max, -1, 1, getHeight(), 0);
                float min_Y = juce::jmap<float>(min, -1, 1, getHeight(), 0);

                // thanks to David @ Plogue for this interesting hint!
                peaks[(index << 1)].y = lastIsMax ? min_Y : max_Y;
                peaks[(index << 1) + 1].y = lastIsMax ? max_Y : min_Y;

                index++;
            }

            max = std::numeric_limits<float>::lowest();
            min = std::numeric_limits<float>::max();
            counter -= 1.0;
        }

        // store for edge-triggers
        previousSample = sample;
    }
}

void WaveformDisplay::resized()
{
    std::lock_guard l(lock_);
    peaks.clear();
    copy.clear();
    for (std::size_t j = 0; j < getWidth() * 2; j += 2)
    {
        juce::Point<float> point;
        point.x = j / 2;
        point.y = juce::jmap<float>(0, -1, 1, getHeight(), 0);
        peaks.push_back(point);
        peaks.push_back(point);
        copy.push_back(point);
        copy.push_back(point);
    }
}

float Oscilloscope::freqToX(float freq, int width)
{
    static const float ratio = std::log(highFreq / lowFreq);
    float xNorm = std::log(freq / lowFreq) / ratio;
    return xNorm * (float)width;
}

float Oscilloscope::timeToX(std::chrono::milliseconds time, int width)
{
    static const float maxf = 100.f;
    const float timef = static_cast<float>(time.count());
    return juce::jmap(juce::jlimit(0.f, maxf, timef), 0.f, maxf, 0.f, static_cast<float>(width));
}

float Oscilloscope::dbToY(float db, int height) { return (float)height * (dbMax - db) / dbRange; }

// TODO:
// (1) Give configuration to the user to choose FFT params (namely, desired Hz resolution).
Oscilloscope::Oscilloscope(SurgeGUIEditor *e, SurgeStorage *s)
    : editor_(e), storage_(s), forward_fft_(fftOrder),
      window_(fftSize, juce::dsp::WindowingFunction<float>::hann), pos_(0), complete_(false),
      fft_thread_(std::bind(std::mem_fn(&Oscilloscope::pullData), this)),
      channel_selection_(STEREO), scope_mode_(SPECTRUM), left_chan_button_("L"),
      right_chan_button_("R"), scope_mode_button_(*this), background_(s), spectrogram_(e, s),
      spectrogram_parameters_(e, s), waveform_(e, s), waveform_parameters_(e, s, this)
{
    setAccessible(true);
    setOpaque(true);

    background_.updateBackgroundType(SPECTRUM);
    auto onToggle = std::bind(std::mem_fn(&Oscilloscope::toggleChannel), this);
    left_chan_button_.setStorage(storage_);
    left_chan_button_.setToggleState(true);
    left_chan_button_.onToggle = onToggle;
    left_chan_button_.setBufferedToImage(true);
    left_chan_button_.setAccessible(true);
    left_chan_button_.setTitle("L CHAN");
    left_chan_button_.setDescription("Enable input from left channel.");
    left_chan_button_.setWantsKeyboardFocus(false);
    right_chan_button_.setStorage(storage_);
    right_chan_button_.setToggleState(true);
    right_chan_button_.onToggle = onToggle;
    right_chan_button_.setBufferedToImage(true);
    right_chan_button_.setAccessible(true);
    right_chan_button_.setTitle("R CHAN");
    right_chan_button_.setDescription("Enable input from right channel.");
    right_chan_button_.setWantsKeyboardFocus(false);
    scope_mode_button_.setStorage(storage_);
    scope_mode_button_.setRows(1);
    scope_mode_button_.setColumns(2);
    scope_mode_button_.setLabels({"Waveform", "Spectrum"});
    scope_mode_button_.setWantsKeyboardFocus(false);
    scope_mode_button_.setValue(1.f);
    spectrogram_parameters_.setOpaque(true);
    waveform_parameters_.setOpaque(true);
    addAndMakeVisible(background_);
    addAndMakeVisible(left_chan_button_);
    addAndMakeVisible(right_chan_button_);
    addAndMakeVisible(scope_mode_button_);
    addAndMakeVisible(spectrogram_);
    addAndMakeVisible(spectrogram_parameters_);
    addChildComponent(waveform_);
    addChildComponent(waveform_parameters_);

    storage_->audioOut.subscribe();
}

Oscilloscope::~Oscilloscope()
{
    // complete_ should come before any condition variables get signaled, to allow the data thread
    // to finish up.
    complete_.store(true, std::memory_order_seq_cst);
    {
        std::lock_guard l(data_lock_);
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
    scope_mode_button_.setSkin(skin, associatedBitmapStore);
    spectrogram_.setSkin(skin, associatedBitmapStore);
    spectrogram_parameters_.setSkin(skin, associatedBitmapStore);
    waveform_.setSkin(skin, associatedBitmapStore);
    waveform_parameters_.setSkin(skin, associatedBitmapStore);
}

void Oscilloscope::paint(juce::Graphics &g) {}

void Oscilloscope::resized()
{
    // Scope looks like the following picture.
    // Parameters lie underneath the scope display and the x-axis scale. So:
    // ------------------------
    // |      top (15px)      |
    // |                      |
    // |    scope display     |
    // |    (8px reduced)     |
    // |  (30px space right)  |
    // |                      |
    // |    x-scale (15px)    |
    // |      bot params      |
    // ------------------------
    auto scopeRect = getScopeRect();
    auto t = getTransform().inverted();
    auto h = getHeight();
    auto w = getWidth();
    t.transformPoint(w, h);
    auto rhs = scopeRect.getWidth();

    background_.updateBounds(getLocalBounds(), getScopeRect());
    // Top buttons: in the first 15 pixels.
    left_chan_button_.setBounds(8, 4, 15, 15);
    right_chan_button_.setBounds(23, 4, 15, 15);
    scope_mode_button_.setBounds(rhs - 97, 4, 105, 15);
    // Spectrogram/waveform display: appears in scopeRect.
    spectrogram_.setBounds(scopeRect);
    waveform_.setBounds(scopeRect);
    // Bottom buttons: in the bottom paramsHeight pixels.
    spectrogram_parameters_.setBounds(0, h - paramsHeight, w, h);
    waveform_parameters_.setBounds(0, h - paramsHeight, w, h);
}

Oscilloscope::SpectrogramParameters::SpectrogramParameters(SurgeGUIEditor *e, SurgeStorage *s)
    : editor_(e), storage_(s)
{
}

void Oscilloscope::SpectrogramParameters::onSkinChanged()
{
    // FIXME: Implement.
}

void Oscilloscope::SpectrogramParameters::paint(juce::Graphics &g)
{
    g.fillAll(skin->getColor(Colors::MSEGEditor::Background));
}

void Oscilloscope::SpectrogramParameters::resized()
{
    // FIXME: Implement.
}

Oscilloscope::WaveformParameters::WaveformParameters(SurgeGUIEditor *e, SurgeStorage *s,
                                                     juce::Component *parent)
    : editor_(e), storage_(s), parent_(parent), freeze_("Freeze"), dc_kill_("DC-Kill"),
      sync_draw_("Sync")
{
    trigger_speed_.setOrientation(Surge::ParamConfig::kHorizontal);
    trigger_level_.setOrientation(Surge::ParamConfig::kHorizontal);
    trigger_limit_.setOrientation(Surge::ParamConfig::kHorizontal);
    time_window_.setOrientation(Surge::ParamConfig::kHorizontal);
    amp_window_.setOrientation(Surge::ParamConfig::kHorizontal);
    trigger_speed_.setStorage(s);
    trigger_level_.setStorage(s);
    trigger_limit_.setStorage(s);
    time_window_.setStorage(s);
    amp_window_.setStorage(s);
    trigger_speed_.setDefaultValue(0.5);
    trigger_level_.setDefaultValue(0.5);
    trigger_limit_.setDefaultValue(0.5);
    time_window_.setDefaultValue(0.75);
    amp_window_.setDefaultValue(0.5);
    trigger_speed_.setQuantitizedDisplayValue(0.5);
    trigger_level_.setQuantitizedDisplayValue(0.5);
    trigger_limit_.setQuantitizedDisplayValue(0.5);
    time_window_.setQuantitizedDisplayValue(0.75);
    amp_window_.setQuantitizedDisplayValue(0.5);
    trigger_speed_.setLabel("Internal Trigger Speed");
    trigger_level_.setLabel("Rise/Fall Trigger Level");
    trigger_limit_.setLabel("Retrigger Threshold");
    time_window_.setLabel("Time");
    amp_window_.setLabel("Amp");
    trigger_speed_.setDescription("Speed the internal oscillator will trigger with");
    trigger_level_.setDescription("Minimum value a waveform must rise/fall to trigger");
    trigger_limit_.setDescription("How fast to trigger again after a trigger happens");
    time_window_.setDescription("X (time) scale");
    amp_window_.setDescription("Y (amplitude) scale");
    trigger_speed_.setIsLightStyle(true);
    trigger_level_.setIsLightStyle(true);
    trigger_limit_.setIsLightStyle(true);
    time_window_.setIsLightStyle(true);
    amp_window_.setIsLightStyle(true);
    auto updateParameter = [this](float &param, float value) {
        std::lock_guard l(params_lock_);
        params_changed_ = true;
        param = value;
    };
    trigger_speed_.setOnUpdate(std::bind(updateParameter, std::ref(params_.trigger_speed), _1));
    trigger_level_.setOnUpdate(std::bind(updateParameter, std::ref(params_.trigger_level), _1));
    trigger_limit_.setOnUpdate(std::bind(updateParameter, std::ref(params_.trigger_limit), _1));
    time_window_.setOnUpdate(std::bind(updateParameter, std::ref(params_.time_window), _1));
    amp_window_.setOnUpdate(std::bind(updateParameter, std::ref(params_.amp_window), _1));
    trigger_speed_.setRootWindow(parent_);
    trigger_level_.setRootWindow(parent_);
    trigger_limit_.setRootWindow(parent_);
    time_window_.setRootWindow(parent_);
    amp_window_.setRootWindow(parent_);
    trigger_speed_.setPrecision(2);
    trigger_level_.setPrecision(2);
    trigger_limit_.setPrecision(2);
    time_window_.setPrecision(2);
    amp_window_.setPrecision(2);
    addAndMakeVisible(trigger_speed_);
    addAndMakeVisible(trigger_level_);
    addAndMakeVisible(trigger_limit_);
    addAndMakeVisible(time_window_);
    addAndMakeVisible(amp_window_);
    // The multiswitch.
    trigger_type_.setRows(4);
    trigger_type_.setColumns(1);
    trigger_type_.setLabels({"Free", "Rising", "Falling", "Internal"});
    trigger_type_.setValue(0.f);
    trigger_type_.setWantsKeyboardFocus(false);
    trigger_type_.setOnUpdate([this](int value) {
        if (value >= WaveformDisplay::kNumTriggerTypes)
        {
            std::cout << "Unexpected trigger type provided." << std::endl;
            std::abort();
        }
        std::lock_guard l(params_lock_);
        params_changed_ = true;
        params_.trigger_type = static_cast<WaveformDisplay::TriggerType>(value);
    });
    addAndMakeVisible(trigger_type_);
    // The two toggle buttons.
    auto toggleParam = [this](bool &param) {
        std::lock_guard l(params_lock_);
        params_changed_ = true;
        param = !param;
    };
    freeze_.setWantsKeyboardFocus(false);
    dc_kill_.setWantsKeyboardFocus(false);
    sync_draw_.setWantsKeyboardFocus(false);
    freeze_.onToggle = std::bind(toggleParam, std::ref(params_.freeze));
    dc_kill_.onToggle = std::bind(toggleParam, std::ref(params_.dc_kill));
    sync_draw_.onToggle = std::bind(toggleParam, std::ref(params_.sync_draw));
    addAndMakeVisible(freeze_);
    addAndMakeVisible(dc_kill_);
    addAndMakeVisible(sync_draw_);
}

std::optional<WaveformDisplay::Parameters> Oscilloscope::WaveformParameters::getParamsIfDirty()
{
    std::lock_guard l(params_lock_);
    if (params_changed_)
    {
        params_changed_ = false;
        return params_;
    }
    return std::nullopt;
}

void Oscilloscope::WaveformParameters::onSkinChanged()
{
    trigger_speed_.setSkin(skin, associatedBitmapStore);
    trigger_level_.setSkin(skin, associatedBitmapStore);
    trigger_limit_.setSkin(skin, associatedBitmapStore);
    time_window_.setSkin(skin, associatedBitmapStore);
    amp_window_.setSkin(skin, associatedBitmapStore);
    trigger_type_.setSkin(skin, associatedBitmapStore);
    freeze_.setSkin(skin, associatedBitmapStore);
    dc_kill_.setSkin(skin, associatedBitmapStore);
    sync_draw_.setSkin(skin, associatedBitmapStore);
    auto font = skin->fontManager->getLatoAtSize(7, juce::Font::plain);
    trigger_speed_.setFont(font);
    trigger_level_.setFont(font);
    trigger_limit_.setFont(font);
    time_window_.setFont(font);
    amp_window_.setFont(font);
}

void Oscilloscope::WaveformParameters::paint(juce::Graphics &g)
{
    g.fillAll(skin->getColor(Colors::MSEGEditor::Background));
}

void Oscilloscope::WaveformParameters::resized()
{
    auto t = getTransform().inverted();
    auto h = getHeight();
    auto w = getWidth();
    // Stack the trigger parameters top-to-bottom.
    trigger_speed_.setBounds(10, 0, 140, 26);
    trigger_level_.setBounds(10, 26, 140, 26);
    trigger_limit_.setBounds(10, 52, 140, 26);
    // Window parameters to the right of them, slightly offset since there's only two.
    time_window_.setBounds(160, 13, 140, 26);
    amp_window_.setBounds(160, 39, 140, 26);
    // Next over, the trigger mechanism.
    trigger_type_.setBounds(320, 13, 40, 50);
    // Next over, the three boolean switches.
    freeze_.setBounds(385, 19, 40, 13);
    dc_kill_.setBounds(385, 38, 40, 13);
    sync_draw_.setBounds(385, 57, 40, 13);
}

void Oscilloscope::updateDrawing()
{
    std::lock_guard l(data_lock_);
    if (channel_selection_ != OFF)
    {
        if (scope_mode_ == WAVEFORM)
        {
            auto params = waveform_parameters_.getParamsIfDirty();
            if (params)
            {
                background_.updateParameters(*params);
                background_.repaint();
                waveform_.setParameters(std::move(*params));
            }
            waveform_.repaint();
        }
        else
        {
            spectrogram_.repaint();
        }
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

// Lock for member variables must be held by the caller.
void Oscilloscope::calculateSpectrumData()
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

void Oscilloscope::changeScopeType(ScopeMode type)
{
    std::unique_lock l(data_lock_);
    bool skipUpdate = false;

    switch (type)
    {
    case WAVEFORM:
    {
        scope_mode_ = WAVEFORM;
        spectrogram_.setVisible(false);
        spectrogram_parameters_.setVisible(false);
        std::fill(scope_data_.begin(), scope_data_.end(), 0.f);
        waveform_.setVisible(true);
        waveform_parameters_.setVisible(true);

        break;
    }
    case SPECTRUM:
    {
        scope_mode_ = SPECTRUM;
        waveform_.setVisible(false);
        waveform_parameters_.setVisible(false);
        std::fill(scope_data_.begin(), scope_data_.end(), dbMin);
        spectrogram_.setVisible(true);
        spectrogram_parameters_.setVisible(true);

        break;
    }
    default:
        skipUpdate = true;
        break;
    }

    if (!skipUpdate)
    {
        background_.updateBackgroundType(scope_mode_);
    }
}

juce::Rectangle<int> Oscilloscope::getScopeRect()
{
    auto lb = getLocalBounds().transformedBy(getTransform().inverted());
    auto scopeRect = lb.withTrimmedBottom(15)             // x-scale on bottom
                         .withTrimmedBottom(paramsHeight) // params on bottom
                         .withTrimmedTop(15)              // params on top
                         .withTrimmedRight(30)            // y-scale on right
                         .reduced(8);
    return scopeRect;
}

void Oscilloscope::pullData()
{
    while (!complete_.load(std::memory_order_seq_cst))
    {
        std::unique_lock l(data_lock_);
        if (channel_selection_ == OFF)
        {
            // We want to unsubscribe and sleep if we aren't going to be looking at the data, to
            // prevent useless accumulation and CPU usage.
            storage_->audioOut.unsubscribe();
            channels_off_.wait(l, [this]() {
                return channel_selection_ != OFF || complete_.load(std::memory_order_seq_cst);
            });
            storage_->audioOut.subscribe();
            continue;
        }
        ChannelSelect cs = channel_selection_;

        std::pair<std::vector<float>, std::vector<float>> data = storage_->audioOut.popall();
        std::vector<float> &dataL = data.first;
        std::vector<float> &dataR = data.second;
        if (dataL.empty())
        {
            // Sleep for long enough to accumulate about 4096 samples, or half that in waveform
            // mode.
            ScopeMode mode = scope_mode_;
            l.unlock();
            std::this_thread::sleep_for(std::chrono::duration<float, std::chrono::seconds::period>(
                fftSize / (mode == SPECTRUM ? 2.f : 4.f) / storage_->samplerate));
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

        if (scope_mode_ == WAVEFORM)
        {
            waveform_.process(std::move(dataL));
        }
        else
        {
            int sz = dataL.size();
            if (pos_ + sz >= fftSize)
            {
                int mv = fftSize - pos_;
                int leftovers = sz - mv;
                std::move(dataL.begin(), dataL.begin() + mv, fft_data_.begin() + pos_);
                calculateSpectrumData();
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
}

void Oscilloscope::toggleChannel()
{
    std::lock_guard l(data_lock_);
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

Oscilloscope::Background::Background(SurgeStorage *s) : storage_(s) { setOpaque(true); }

void Oscilloscope::Background::paint(juce::Graphics &g)
{
    if (mode_ == WAVEFORM)
    {
        paintWaveformBackground(g);
    }
    else
    {
        paintSpectrogramBackground(g);
    }
}

void Oscilloscope::Background::updateBackgroundType(ScopeMode mode)
{
    mode_ = mode;
    repaint();
}

void Oscilloscope::Background::updateBounds(juce::Rectangle<int> local_bounds,
                                            juce::Rectangle<int> scope_bounds)
{
    scope_bounds_ = std::move(scope_bounds);
    setBounds(local_bounds);
}

void Oscilloscope::Background::updateParameters(WaveformDisplay::Parameters params)
{
    waveform_params_ = std::move(params);
}

void Oscilloscope::Background::paintSpectrogramBackground(juce::Graphics &g)
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
        for (float freq : {10.f,   20.f,   30.f,   40.f,   60.f,    80.f,    100.f,
                           200.f,  300.f,  400.f,  600.f,  800.f,   1000.f,  2000.f,
                           3000.f, 4000.f, 6000.f, 8000.f, 10000.f, 20000.f, 24000.f})
        {
            const auto xPos = freqToX(freq, width);

            if (freq == 10.f || freq == 100.f || freq == 1000.f || freq == 10000.f ||
                freq == 24000.f)
            {
                g.setColour(primaryLine);
            }
            else
            {
                g.setColour(secondaryLine);
            }

            g.drawVerticalLine(xPos, 0, static_cast<float>(height));

            if (freq == 10.f || freq == 24000.f)
            {
                continue;
            }

            const bool over1000 = freq >= 1000.f;
            const auto freqString =
                juce::String(over1000 ? freq / 1000.f : freq) + (over1000 ? "k" : "");
            // Label will go past the end of the scopeRect.
            const auto labelRect =
                juce::Rectangle{font.getStringWidth(freqString), labelHeight}.withCentre(
                    juce::Point<int>(xPos, height + 11));

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

            if (dB == 0.f || dB == -100.f)
            {
                g.setColour(primaryLine);
            }
            else
            {
                g.setColour(secondaryLine);
            }

            g.drawHorizontalLine(yPos, 0, static_cast<float>(width + 1));

            const auto dbString = juce::String(dB) + " dB";
            // Label will go past the end of the scopeRect.
            const auto labelRect = juce::Rectangle{font.getStringWidth(dbString), labelHeight}
                                       .withBottomY((int)(yPos + (labelHeight / 2)))
                                       .withRightX(width + 30);

            g.setColour(skin->getColor(Colors::MSEGEditor::Axis::SecondaryText));
            g.drawFittedText(dbString, labelRect, juce::Justification::right, 1);
        }
    }
}

void Oscilloscope::Background::paintWaveformBackground(juce::Graphics &g)
{
    g.fillAll(skin->getColor(Colors::MSEGEditor::Background));

    juce::Rectangle<int> labelRect;
    auto scopeRect = scope_bounds_;
    auto width = scopeRect.getWidth();
    auto height = scopeRect.getHeight();
    auto labelHeight = 9;
    auto font = skin->fontManager->getLatoAtSize(7);
    auto primaryLine = skin->getColor(Colors::MSEGEditor::Grid::Primary);
    auto secondaryLine = skin->getColor(Colors::MSEGEditor::Grid::SecondaryVertical);

    {
        auto gs = juce::Graphics::ScopedSaveState(g);

        g.addTransform(juce::AffineTransform().translated(scopeRect.getX(), scopeRect.getY()));
        g.setFont(font);

        // Draw top and bottom lines.
        g.setColour(primaryLine);
        g.drawHorizontalLine(0, 0, width);
        g.drawHorizontalLine(height, 0, width);
        g.drawHorizontalLine(height / 2.f, 0, width);

        // Axis labels will go past the end of the scopeRect.
        g.setColour(skin->getColor(Colors::MSEGEditor::Axis::Text));

        std::string minus = "-";
        std::string plus = "+";
        std::stringstream gain;
        gain << std::fixed << std::setprecision(2) << 1.f / waveform_params_.gain();

        g.drawSingleLineText(minus + gain.str(), width + 4, height + 2);
        g.drawSingleLineText("0.0", width + 4, height / 2 + 2);
        g.drawSingleLineText(plus + gain.str(), width + 4, 2);

        // Draw the trigger lines, if applicable.
        g.setColour(secondaryLine);
        if (waveform_params_.trigger_type == WaveformDisplay::kTriggerRising)
        {
            g.drawHorizontalLine(
                juce::jmap<float>(waveform_params_.triggerLevel(), -1, 1, height, 0), 0, width);
        }
        if (waveform_params_.trigger_type == WaveformDisplay::kTriggerFalling)
        {
            g.drawHorizontalLine(
                juce::jmap<float>(-waveform_params_.triggerLevel(), -1, 1, height, 0), 0, width);
        }
    }

    // Vertical grid.
    {
        auto gs = juce::Graphics::ScopedSaveState(g);
        g.addTransform(juce::AffineTransform().translated(scopeRect.getX(), scopeRect.getY()));
        g.setFont(font);

        // Split the grid into 7 sections, starting from 0 and ending at wherever the counter speed
        // says we should end at.
        float counterSpeedInverse = 1.f / waveform_params_.counterSpeed();
        float sampleRateInverse = 1.f / static_cast<float>(storage_->samplerate);
        float endpoint = counterSpeedInverse * sampleRateInverse * static_cast<float>(width);
        std::string time_unit = (endpoint >= 1.f) ? " s" : " ms";
        for (int i = 0; i < 7; i++)
        {
            if (i == 0 || i == 6)
            {
                g.setColour(primaryLine);
            }
            else
            {
                g.setColour(secondaryLine);
            }

            int xPos = static_cast<int>(static_cast<float>(width) / 6.f * i);
            ;
            g.drawVerticalLine(xPos, 0, height + 1);

            float timef = (endpoint / 6.f) * static_cast<float>(i);
            if (endpoint < 1.f)
            {
                timef *= 1000;
            }
            std::stringstream time;
            time << std::fixed << std::setprecision(2) << timef;
            std::string timeString = time.str() + time_unit;

            // Label will go past the end of the scopeRect.
            const auto labelRect =
                juce::Rectangle{font.getStringWidth(timeString), labelHeight}.withCentre(
                    juce::Point<int>(xPos, height + 13));

            g.setColour(skin->getColor(Colors::MSEGEditor::Axis::SecondaryText));
            g.drawFittedText(timeString, labelRect, juce::Justification::bottom, 1);
        }
    }
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

Oscilloscope::SwitchButton::SwitchButton(Oscilloscope &parent)
    : Surge::Widgets::MultiSwitchSelfDraw(), parent_(parent)
{
    addListener(this);
}

void Oscilloscope::SwitchButton::valueChanged(Surge::GUI::IComponentTagValue *p)
{
    ScopeMode type = SPECTRUM;

    if (p->getValue() < 0.5)
    {
        type = WAVEFORM;
    }

    parent_.changeScopeType(type);
}

} // namespace Overlays
} // namespace Surge
