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

#include "widgets/MenuCustomComponents.h"

using namespace std::chrono_literals;
using std::placeholders::_1;

namespace Surge
{
namespace Overlays
{

static float freqToX(float freq, int width)
{
    static const float ratio = std::log(SpectrumDisplay::highFreq / SpectrumDisplay::lowFreq);
    float xNorm = std::log(freq / SpectrumDisplay::lowFreq) / ratio;
    return xNorm * (float)width;
}

// This could, in theory, divide by zero, so be careful with how you call it.
static float dbToY(float db, int height, float dbMin = -96.f, float dbMax = 0.f)
{
    float range = dbMax - dbMin;
    // If dbMax/dbMin change and the data hasn't been updated yet due to locking, can get weirdness.
    return std::max((float)height * (dbMax - db) / range, 0.f);
}

float WaveformDisplay::Parameters::counterSpeed() const
{
    return std::pow(10.f, -time_window * 5.f + 1.5f);
}

float WaveformDisplay::Parameters::triggerLevel() const { return trigger_level * 2.f - 1.f; }
float WaveformDisplay::Parameters::gain() const { return std::pow(10.f, amp_window * 4.f - 2.f); }

float SpectrumDisplay::Parameters::dbRange() const
{
    return std::max(std::floor(maxDb() - noiseFloor()), 0.f);
}

float SpectrumDisplay::Parameters::noiseFloor() const
{
    return std::floor((noise_floor - 2.f) * 48.f);
}

float SpectrumDisplay::Parameters::maxDb() const { return std::floor((max_db - 1.f) * 36.f); }

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
        // it's 0.1, we have, you guessed it, 10 incoming samples per pixel.
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

SpectrumDisplay::SpectrumDisplay(SurgeGUIEditor *e, SurgeStorage *s)
    : editor_(e), storage_(s), last_updated_time_(std::chrono::steady_clock::now())
{
    std::fill(incoming_scope_data_.begin(), incoming_scope_data_.end(), 0.f);
    std::fill(new_scope_data_.begin(), new_scope_data_.end(), -100.f);
}

const SpectrumDisplay::Parameters &SpectrumDisplay::getParameters() const { return params_; }

void SpectrumDisplay::setParameters(Parameters parameters)
{
    std::lock_guard l(data_lock_);
    // Check if the new params for noise floor/ceiling are different. If they are, consider the
    // display "dirty" (ie, stop interpolating distance, jump right to the new thing).
    bool changedVisible =
        (params_.dbRange() != parameters.dbRange()) || (params_.freeze != parameters.freeze);
    params_ = std::move(parameters);
    if (changedVisible)
    {
        display_dirty_ = true;
        recalculateScopeData();
    }
}

void SpectrumDisplay::paint(juce::Graphics &g)
{
    std::lock_guard l(data_lock_);

    if (params_.dbRange() == 0.0f)
        return;

    auto scopeRect = getLocalBounds().transformedBy(getTransform().inverted());
    auto width = scopeRect.getWidth();
    auto height = scopeRect.getHeight();
    auto curveColor = skin->getColor(Colors::MSEGEditor::Curve);

    auto path = juce::Path();
    bool started = false;
    float binHz = storage_->samplerate / static_cast<float>(internal::fftSize);
    float dbMin = params_.noiseFloor();
    float dbMax = params_.maxDb();
    float zeroPoint = dbToY(dbMin, height, dbMin, dbMax);
    float maxPoint = dbToY(dbMax, height, dbMin, dbMax);
    auto now = std::chrono::steady_clock::now();

    // Start path.
    path.startNewSubPath(freqToX(lowFreq, width), zeroPoint);
    {
        mtbs_ = std::chrono::duration<float>(1.f / binHz);

        for (int i = 0; i < internal::fftSize / 2; i++)
        {
            const float hz = binHz * static_cast<float>(i);

            if (hz < lowFreq || hz > highFreq)
            {
                continue;
            }

            const float x = freqToX(hz, width);
            const float y0 = displayed_data_[i];
            const float y1 = dbToY(new_scope_data_[i], height, dbMin, dbMax);
            const float y = params_.freeze ? y0 : (display_dirty_ ? y1 : interpolate(y0, y1, now));

            displayed_data_[i] = y;

            if (y >= zeroPoint)
            {
                path.lineTo(x, zeroPoint);
                path.closeSubPath();
                started = false;
            }
            else
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

    display_dirty_ = false;
}

void SpectrumDisplay::recalculateScopeData()
{
    // data_lock_ *must* be held by the caller.
    const float dbMin = params_.noiseFloor();
    const float dbMax = params_.maxDb();
    const float offset = juce::Decibels::gainToDecibels((float)internal::fftSize);
    std::transform(incoming_scope_data_.begin(), incoming_scope_data_.end(),
                   new_scope_data_.begin(), [=](const float f) {
                       return juce::jlimit(dbMin, dbMax,
                                           juce::Decibels::gainToDecibels(f) - offset);
                   });
}

void SpectrumDisplay::resized()
{
    auto scopeRect = getLocalBounds().transformedBy(getTransform().inverted());
    auto height = scopeRect.getHeight();
    std::fill(displayed_data_.begin(), displayed_data_.end(),
              // We're essentially filling with the calculated equivalent of the bottom of the
              // graph, so the fact that we're not calling with the actual noise floor or dB ceiling
              // is immaterial.
              dbToY(-96.f, height, -96.f, 0.f));
}

void SpectrumDisplay::updateScopeData(internal::FftScopeType::iterator begin,
                                      internal::FftScopeType::iterator end)
{
    // Data comes in as gain.
    std::lock_guard l(data_lock_);

    // Decay existing data, and move new data in if it's larger.
    const float decay = 1.f - sqrt(params_.decay_rate);

    std::transform(begin, end, incoming_scope_data_.begin(), incoming_scope_data_.begin(),
                   [decay](const float fn, const float f) { return std::max(f * decay, fn); });

    last_updated_time_ = std::chrono::steady_clock::now();

    if (!params_.freeze)
    {
        recalculateScopeData();
    }
}

float SpectrumDisplay::interpolate(const float y0, const float y1,
                                   std::chrono::time_point<std::chrono::steady_clock> t) const
{
    std::chrono::duration<float> distance = (t - last_updated_time_);
    float mu = juce::jlimit(0.f, 1.f, distance / mtbs_);
    return y0 * (1 - mu) + y1 * mu;
}

// TODO:
// (1) Give configuration to the user to choose FFT params (namely, desired Hz resolution).
Oscilloscope::Oscilloscope(SurgeGUIEditor *e, SurgeStorage *s)
    : editor_(e), storage_(s), forward_fft_(internal::fftOrder),
      window_(internal::fftSize, juce::dsp::WindowingFunction<float>::hann), pos_(0),
      complete_(false), fft_thread_(std::bind(std::mem_fn(&Oscilloscope::pullData), this)),
      channel_selection_(STEREO), scope_mode_(SPECTRUM), left_chan_button_("L"),
      right_chan_button_("R"), scope_mode_button_(*this), background_(s), spectrum_(e, s),
      spectrum_parameters_(e, s, this), waveform_(e, s), waveform_parameters_(e, s, this)
{
    setAccessible(true);
    setOpaque(true);

    background_.updateBackgroundType(WAVEFORM);

    auto onToggle = std::bind(std::mem_fn(&Oscilloscope::toggleChannel), this);

    left_chan_button_.setStorage(storage_);
    left_chan_button_.setToggleState(true);
    left_chan_button_.onToggle = onToggle;
    left_chan_button_.setBufferedToImage(true);
    left_chan_button_.setAccessible(true);
    left_chan_button_.setTitle("Left Channel");
    left_chan_button_.setDescription("Enable input from left channel.");
    left_chan_button_.setWantsKeyboardFocus(false);
    left_chan_button_.setTag(tag_input_l);
    left_chan_button_.addListener(this);
    right_chan_button_.setStorage(storage_);
    right_chan_button_.setToggleState(true);
    right_chan_button_.onToggle = onToggle;
    right_chan_button_.setBufferedToImage(true);
    right_chan_button_.setAccessible(true);
    right_chan_button_.setTitle("Right Channel");
    right_chan_button_.setDescription("Enable input from right channel.");
    right_chan_button_.setWantsKeyboardFocus(false);
    right_chan_button_.setTag(tag_input_r);
    right_chan_button_.addListener(this);
    scope_mode_button_.setStorage(storage_);
    scope_mode_button_.setRows(1);
    scope_mode_button_.setColumns(2);
    scope_mode_button_.setLabels({"Waveform", "Spectrum"});
    scope_mode_button_.setWantsKeyboardFocus(false);
    scope_mode_button_.setValue(0.f);
    scope_mode_button_.setTag(tag_scope_mode);
    scope_mode_button_.setDraggable(true);
    scope_mode_button_.addListener(this);
    spectrum_parameters_.setOpaque(true);
    waveform_parameters_.setOpaque(true);
    addAndMakeVisible(background_);
    addAndMakeVisible(left_chan_button_);
    addAndMakeVisible(right_chan_button_);
    addAndMakeVisible(scope_mode_button_);

    addChildComponent(spectrum_);
    addChildComponent(spectrum_parameters_);
    addChildComponent(waveform_);
    addChildComponent(waveform_parameters_);

    // Set the initial mode based on the DAW state.
    int mode = juce::jlimit(0, 1, s->getPatch().dawExtraState.editor.oscilloscopeOverlayState.mode);
    scope_mode_button_.setValue(static_cast<float>(mode));
    changeScopeType(static_cast<ScopeMode>(mode));

    storage_->audioOut.subscribe();
}

Oscilloscope::~Oscilloscope()
{
    // complete_ should come before any condition variables get signaled, to allow the data
    // thread to finish up.
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
    spectrum_.setSkin(skin, associatedBitmapStore);
    spectrum_parameters_.setSkin(skin, associatedBitmapStore);
    waveform_.setSkin(skin, associatedBitmapStore);
    waveform_parameters_.setSkin(skin, associatedBitmapStore);

    // for some reason this is necessary, but just for these two!
    left_chan_button_.repaint();
    right_chan_button_.repaint();
}

void Oscilloscope::paint(juce::Graphics &g) {}

int32_t Oscilloscope::controlModifierClicked(Surge::GUI::IComponentTagValue *pControl,
                                             const juce::ModifierKeys &button,
                                             bool isDoubleClickEvent)
{
    if (isDoubleClickEvent)
    {
        return 0;
    }

    int tag = pControl->getTag();

    // Basically all the menus are a list of options with values
    std::vector<std::pair<std::string, float>> options;
    std::string menuName = "";

    switch (tag)
    {
    case tag_scope_mode:
        menuName = "Oscilloscope Mode";
        options.push_back(std::make_pair("Waveform", 0));
        options.push_back(std::make_pair("Spectrum", 1));
        break;
    case tag_input_l:
        menuName = "Left Input";
        break;
    case tag_input_r:
        menuName = "Right Input";
        break;
    case tag_wf_dc_block:
        menuName = "DC Block";
        break;
    case tag_wf_freeze:
    case tag_sp_freeze:
        menuName = "Freeze";
        break;
    case tag_wf_sync:
        menuName = "Sync Redraw";
        break;
    case tag_wf_time_scaling:
        menuName = "Time Scaling";
        break;
    case tag_wf_amp_scaling:
        menuName = "Amplitude Scaling";
        break;
    case tag_wf_trigger_mode:
        menuName = "Trigger Mode";
        options.push_back(std::make_pair("Freerun", 0.0));
        options.push_back(std::make_pair("Rising Edge", 0.25));
        options.push_back(std::make_pair("Falling Edge", 0.5));
        options.push_back(std::make_pair("Internal Trigger", 1.0));
        break;
    case tag_wf_trigger_level:
        menuName = "Trigger Level";
        break;
    case tag_wf_retrigger_threshold:
        menuName = "Retrigger Threshold";
        break;
    case tag_wf_int_trigger_freq:
        menuName = "Trigger Frequency";
        break;
    case tag_sp_min_level:
        menuName = "Minimum Level";
        break;
    case tag_sp_max_level:
        menuName = "Maximum Level";
        break;
    case tag_sp_decay_rate:
        menuName = "Spectrum Decay Rate";
        break;
    default:
        break;
    }

    auto contextMenu = juce::PopupMenu();

    auto msurl = SurgeGUIEditor::helpURLForSpecial(storage_, "oscilloscope");
    auto hurl = SurgeGUIEditor::fullyResolvedHelpURL(msurl);
    auto tcomp = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>(menuName, hurl);

    tcomp->setSkin(skin, associatedBitmapStore);

    auto hment = tcomp->getTitle();

    contextMenu.addCustomItem(-1, std::move(tcomp), nullptr, hment);

    if (!options.empty())
    {
        contextMenu.addSeparator();

        for (auto op : options)
        {
            auto val = op.second;

            contextMenu.addItem(op.first, true, (val == pControl->getValue()),
                                [val, pControl, this]() {
                                    pControl->setValue(val);

                                    // The switch button is self-draw and has its own value change
                                    // so we need to handle that eventuality here
                                    auto sc = dynamic_cast<SwitchButton *>(pControl);
                                    if (sc)
                                        sc->valueChanged(pControl);
                                    else
                                        valueChanged(pControl);

                                    auto iv = pControl->asJuceComponent();

                                    if (iv)
                                    {
                                        iv->repaint();
                                    }
                                });
        }
    }

    contextMenu.showMenuAsync(editor_->popupMenuOptions(),
                              Surge::GUI::makeEndHoverCallback(pControl));

    return 1;
}

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
    // |    bottom params     |
    // ------------------------

    auto scopeRect = getScopeRect();
    auto w = getWidth();
    auto h = getHeight();
    auto rhs = scopeRect.getWidth();
    auto buttonSize = 14;

    background_.updateBounds(getLocalBounds(), getScopeRect());

    left_chan_button_.setBounds(rhs - 21, 4, buttonSize, buttonSize);
    right_chan_button_.setBounds(rhs - 5, 4, buttonSize, buttonSize);
    scope_mode_button_.setBounds(8, 4, 116, buttonSize);

    spectrum_.setBounds(scopeRect);
    waveform_.setBounds(scopeRect);

    juce::Rectangle<int> r;
    auto ph = juce::Rectangle<int>(0, paramsHeight).transformedBy(getTransform());
    auto phh = ph.getHeight();
    r = juce::Rectangle<int>(0, h - phh, w, h);
    spectrum_parameters_.setBounds(r.transformedBy(getTransform().inverted()));
    waveform_parameters_.setBounds(r.transformedBy(getTransform().inverted()));
}

Oscilloscope::WaveformParameters::WaveformParameters(SurgeGUIEditor *e, SurgeStorage *s,
                                                     Oscilloscope *parent)
    : editor_(e), storage_(s), parent_(parent), freeze_("Freeze"), dc_kill_("DC Block"),
      sync_draw_("Sync Redraw")
{
    // Initialize parameters from the DAW state.
    auto *state = &s->getPatch().dawExtraState.editor.oscilloscopeOverlayState;
    params_.trigger_speed = juce::jlimit(0.f, 1.f, state->trigger_speed);
    params_.trigger_level = juce::jlimit(0.f, 1.f, state->trigger_level);
    params_.trigger_limit = juce::jlimit(0.f, 1.f, state->trigger_limit);
    params_.time_window = juce::jlimit(0.f, 1.f, state->time_window);
    params_.amp_window = juce::jlimit(0.f, 1.f, state->amp_window);
    params_.trigger_type = static_cast<WaveformDisplay::TriggerType>(
        juce::jlimit(0, WaveformDisplay::kNumTriggerTypes - 1, state->trigger_type));
    params_.dc_kill = state->dc_kill;
    params_.sync_draw = state->sync_draw;

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

    trigger_speed_.setDefaultValue(params_.trigger_speed);
    trigger_level_.setDefaultValue(params_.trigger_level);
    trigger_limit_.setDefaultValue(params_.trigger_limit);
    time_window_.setDefaultValue(params_.time_window);
    amp_window_.setDefaultValue(params_.amp_window);

    trigger_speed_.setQuantitizedDisplayValue(params_.trigger_speed);
    trigger_level_.setQuantitizedDisplayValue(params_.trigger_level);
    trigger_limit_.setQuantitizedDisplayValue(params_.trigger_limit);
    time_window_.setQuantitizedDisplayValue(params_.time_window);
    amp_window_.setQuantitizedDisplayValue(params_.amp_window);

    trigger_speed_.setLabel("Trigger Frequency");
    trigger_level_.setLabel("Trigger Level");
    trigger_limit_.setLabel("Retrigger Threshold");
    time_window_.setLabel("Time Scaling");
    amp_window_.setLabel("Amplitude Scaling");

    trigger_speed_.setDescription("Rate at which the internal oscillator will run");
    trigger_level_.setDescription("Minimum value a waveform must rise/fall to trigger");
    trigger_limit_.setDescription("How fast to trigger again after a trigger happens");
    time_window_.setDescription("X axis (time) scale adjustment");
    amp_window_.setDescription("Y axis (amplitude) scale adjustment");

    trigger_speed_.setRange(0.441f, 139.4f);
    trigger_limit_.setRange(1, 10000);
    trigger_level_.setRange(-100, 100);
    time_window_.setRange(-100, 100);
    amp_window_.setRange(-100, 100);

    trigger_speed_.setUnit(" Hz");
    trigger_limit_.setUnit(" Samples");
    trigger_level_.setUnit(" %");
    time_window_.setUnit(" %");
    amp_window_.setUnit(" %");

    trigger_speed_.setPrecision(3);
    trigger_level_.setPrecision(2);
    trigger_limit_.setPrecision(0);
    time_window_.setPrecision(2);
    amp_window_.setPrecision(2);

    trigger_type_.setTag(tag_wf_trigger_mode);
    trigger_speed_.setTag(tag_wf_int_trigger_freq);
    trigger_level_.setTag(tag_wf_trigger_level);
    trigger_limit_.setTag(tag_wf_retrigger_threshold);
    time_window_.setTag(tag_wf_time_scaling);
    amp_window_.setTag(tag_wf_amp_scaling);

    trigger_type_.addListener(this);
    trigger_speed_.addListener(this);
    trigger_level_.addListener(this);
    trigger_limit_.addListener(this);
    time_window_.addListener(this);
    amp_window_.addListener(this);

    trigger_type_.setDraggable(true);

    auto updateParameter = [this](float &param, float &backer, float value) {
        std::lock_guard l(params_lock_);
        params_changed_ = true;
        param = value;
        backer = value; // Update DAW state.
    };

    auto updateAmpWindow = [this, state](float value) {
        std::lock_guard l(params_lock_);
        params_changed_ = true;
        params_.amp_window = value;
        state->amp_window = value; // Update DAW state.
        float gain = 1.f / params_.gain();
        trigger_level_.setRange(-gain, gain);
    };

    trigger_speed_.setOnUpdate(std::bind(updateParameter, std::ref(params_.trigger_speed),
                                         std::ref(state->trigger_speed), _1));
    trigger_level_.setOnUpdate(std::bind(updateParameter, std::ref(params_.trigger_level),
                                         std::ref(state->trigger_level), _1));
    trigger_limit_.setOnUpdate(std::bind(updateParameter, std::ref(params_.trigger_limit),
                                         std::ref(state->trigger_limit), _1));
    time_window_.setOnUpdate(std::bind(updateParameter, std::ref(params_.time_window),
                                       std::ref(state->time_window), _1));
    amp_window_.setOnUpdate(updateAmpWindow);

    trigger_speed_.setRootWindow(parent_);
    trigger_level_.setRootWindow(parent_);
    trigger_limit_.setRootWindow(parent_);
    time_window_.setRootWindow(parent_);
    amp_window_.setRootWindow(parent_);

    // These are not visible by default, since the default trigger type is Freerun
    trigger_speed_.setVisible(false);
    trigger_level_.setVisible(false);
    trigger_limit_.setVisible(false);

    addAndMakeVisible(trigger_speed_);
    addAndMakeVisible(trigger_level_);
    addAndMakeVisible(trigger_limit_);
    addAndMakeVisible(time_window_);
    addAndMakeVisible(amp_window_);

    trigger_type_.setRows(4);
    trigger_type_.setColumns(1);
    trigger_type_.setLabels({"Freerun", "Rising Edge", "Falling Edge", "Internal Trigger"});
    trigger_type_.setIntegerValue(static_cast<int>(params_.trigger_type));
    trigger_type_.setWantsKeyboardFocus(false);
    trigger_type_.setOnUpdate([this, state](int value) {
        if (value >= WaveformDisplay::kNumTriggerTypes)
        {
            std::cout << "Unexpected trigger type provided: " << value << std::endl;
            std::abort();
        }
        std::lock_guard l(params_lock_);
        params_changed_ = true;
        params_.trigger_type = static_cast<WaveformDisplay::TriggerType>(value);
        state->trigger_type = static_cast<int>(params_.trigger_type); // Update DAW state.

        if (params_.trigger_type == WaveformDisplay::kTriggerInternal)
        {
            trigger_speed_.setVisible(true);
        }
        else
        {
            trigger_speed_.setVisible(false);
        }

        if (params_.trigger_type == WaveformDisplay::kTriggerRising ||
            params_.trigger_type == WaveformDisplay::kTriggerFalling)
        {

            trigger_level_.setVisible(true);
            trigger_limit_.setVisible(true);
        }
        else
        {
            trigger_level_.setVisible(false);
            trigger_limit_.setVisible(false);
        }
    });

    // Explicitly inform the switch that the value has been updated, to take
    // our initial value into account and color the switch appropriately.
    trigger_type_.valueChanged(nullptr);

    addAndMakeVisible(trigger_type_);

    // The three toggle buttons.
    auto toggleParam = [this](bool &param, bool *backer) {
        std::lock_guard l(params_lock_);
        params_changed_ = true;
        param = !param;
        if (backer)
        {
            *backer = param; // Save to DAW state.
        }
    };

    dc_kill_.setValue(static_cast<float>(params_.dc_kill));
    sync_draw_.setValue(static_cast<float>(params_.sync_draw));

    dc_kill_.isToggled = params_.dc_kill;
    sync_draw_.isToggled = params_.sync_draw;

    freeze_.setWantsKeyboardFocus(false);
    dc_kill_.setWantsKeyboardFocus(false);
    sync_draw_.setWantsKeyboardFocus(false);

    freeze_.setTag(tag_wf_freeze);
    dc_kill_.setTag(tag_wf_dc_block);
    sync_draw_.setTag(tag_wf_sync);

    freeze_.addListener(this);
    dc_kill_.addListener(this);
    sync_draw_.addListener(this);

    freeze_.onToggle = std::bind(toggleParam, std::ref(params_.freeze), nullptr);
    dc_kill_.onToggle = std::bind(toggleParam, std::ref(params_.dc_kill), &state->dc_kill);
    sync_draw_.onToggle = std::bind(toggleParam, std::ref(params_.sync_draw), &state->sync_draw);

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

    auto font = skin->fontManager->getLatoAtSize(9, juce::Font::plain);

    trigger_speed_.setFont(font);
    trigger_level_.setFont(font);
    trigger_limit_.setFont(font);
    time_window_.setFont(font);
    amp_window_.setFont(font);
}

void Oscilloscope::WaveformParameters::paint(juce::Graphics &g)
{
    g.fillAll(skin->getColor(Colors::MSEGEditor::Panel));
}

void Oscilloscope::WaveformParameters::resized()
{
    auto t = getTransform().inverted();
    auto h = getHeight();
    auto w = getWidth();
    auto buttonWidth = 58;
    int labelHeight = 12;

    trigger_type_.setBounds(227, 14, 68, 52);

    trigger_speed_.setBounds(307, 28, 140, 26);
    trigger_level_.setBounds(307, 14, 140, 26);
    trigger_limit_.setBounds(307, 42, 140, 26);

    time_window_.setBounds(78, 14, 140, 26);
    amp_window_.setBounds(78, 42, 140, 26);

    dc_kill_.setBounds(8, 14, buttonWidth, 14);
    freeze_.setBounds(8, 33, buttonWidth, 14);
    sync_draw_.setBounds(8, 52, buttonWidth, 14);
}

Oscilloscope::SpectrumParameters::SpectrumParameters(SurgeGUIEditor *e, SurgeStorage *s,
                                                     Oscilloscope *parent)
    : editor_(e), storage_(s), parent_(parent), freeze_("Freeze")
{
    // Initialize parameters from the DAW state.
    auto *state = &s->getPatch().dawExtraState.editor.oscilloscopeOverlayState;
    params_.noise_floor = juce::jlimit(0.f, 1.f, state->noise_floor);
    params_.max_db = juce::jlimit(0.f, 1.f, state->max_db);
    params_.decay_rate = juce::jlimit(0.f, 1.f, state->decay_rate);

    noise_floor_.setOrientation(Surge::ParamConfig::kHorizontal);
    max_db_.setOrientation(Surge::ParamConfig::kHorizontal);
    decay_rate_.setOrientation(Surge::ParamConfig::kHorizontal);

    noise_floor_.setStorage(s);
    max_db_.setStorage(s);
    decay_rate_.setStorage(s);

    noise_floor_.setDefaultValue(params_.noise_floor);
    max_db_.setDefaultValue(params_.max_db);
    decay_rate_.setDefaultValue(params_.decay_rate);

    noise_floor_.setQuantitizedDisplayValue(params_.noise_floor);
    max_db_.setQuantitizedDisplayValue(params_.max_db);
    decay_rate_.setQuantitizedDisplayValue(params_.decay_rate);

    noise_floor_.setLabel("Min Level");
    max_db_.setLabel("Max Level ");
    decay_rate_.setLabel("Decay Rate");

    noise_floor_.setDescription("Bottom of the display.");
    max_db_.setDescription("Top of the display.");
    decay_rate_.setDescription("Control how fast the current data decays.");

    noise_floor_.setPrecision(0);
    max_db_.setPrecision(0);
    decay_rate_.setPrecision(2);

    noise_floor_.setRange(-96, -48);
    max_db_.setRange(-36, 0);
    decay_rate_.setRange(0, 100);

    noise_floor_.setUnit(" dB");
    max_db_.setUnit(" dB");
    decay_rate_.setUnit(" %");

    noise_floor_.setTag(tag_sp_min_level);
    max_db_.setTag(tag_sp_max_level);
    decay_rate_.setTag(tag_sp_decay_rate);

    noise_floor_.addListener(this);
    max_db_.addListener(this);
    decay_rate_.addListener(this);

    auto updateParameter = [this](float &param, float &backer, float value) {
        std::lock_guard l(params_lock_);
        params_changed_ = true;
        backer = value; // Save to DAW state.
        param = value;
    };

    noise_floor_.setOnUpdate(std::bind(updateParameter, std::ref(params_.noise_floor),
                                       std::ref(state->noise_floor), _1));
    max_db_.setOnUpdate(
        std::bind(updateParameter, std::ref(params_.max_db), std::ref(state->max_db), _1));
    decay_rate_.setOnUpdate(
        std::bind(updateParameter, std::ref(params_.decay_rate), std::ref(state->decay_rate), _1));

    noise_floor_.setRootWindow(parent_);
    max_db_.setRootWindow(parent_);
    decay_rate_.setRootWindow(parent_);

    addAndMakeVisible(noise_floor_);
    addAndMakeVisible(max_db_);
    addAndMakeVisible(decay_rate_);

    // The toggle button.
    auto toggleParam = [this](bool &param) {
        std::lock_guard l(params_lock_);
        params_changed_ = true;
        param = !param;
    };

    freeze_.setWantsKeyboardFocus(false);
    freeze_.setTag(tag_sp_freeze);
    freeze_.addListener(this);
    freeze_.onToggle = std::bind(toggleParam, std::ref(params_.freeze));

    addAndMakeVisible(freeze_);
}

std::optional<SpectrumDisplay::Parameters> Oscilloscope::SpectrumParameters::getParamsIfDirty()
{
    std::lock_guard l(params_lock_);

    if (params_changed_)
    {
        params_changed_ = false;
        return params_;
    }

    return std::nullopt;
}

void Oscilloscope::SpectrumParameters::onSkinChanged()
{
    noise_floor_.setSkin(skin, associatedBitmapStore);
    max_db_.setSkin(skin, associatedBitmapStore);
    decay_rate_.setSkin(skin, associatedBitmapStore);
    freeze_.setSkin(skin, associatedBitmapStore);

    auto font = skin->fontManager->getLatoAtSize(9, juce::Font::plain);

    noise_floor_.setFont(font);
    max_db_.setFont(font);
    decay_rate_.setFont(font);
}

void Oscilloscope::SpectrumParameters::paint(juce::Graphics &g)
{
    g.fillAll(skin->getColor(Colors::MSEGEditor::Panel));
}

void Oscilloscope::SpectrumParameters::resized()
{
    auto t = getTransform().inverted();
    auto h = getHeight();
    auto w = getWidth();
    auto buttonWidth = 58;

    noise_floor_.setBounds(78, 14, 140, 26);
    max_db_.setBounds(78, 42, 140, 26);
    decay_rate_.setBounds(219, 28, 140, 26);

    freeze_.setBounds(8, 33, buttonWidth, 14);
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
            auto params = spectrum_parameters_.getParamsIfDirty();
            if (params)
            {
                background_.updateParameters(*params);
                background_.repaint();
                spectrum_.setParameters(std::move(*params));
            }
            spectrum_.repaint();
        }
    }
}

void Oscilloscope::visibilityChanged()
{
    // Not sure aside from construction when visibility might be changed in Juce, so putting
    // this here for additional safety.
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
    window_.multiplyWithWindowingTable(fft_data_.data(), internal::fftSize);
    forward_fft_.performFrequencyOnlyForwardTransform(fft_data_.data());

    float binHz = storage_->samplerate / static_cast<float>(internal::fftSize);
    for (int i = 0; i < internal::fftSize / 2; i++)
    {
        float hz = binHz * static_cast<float>(i);
        if (hz < SpectrumDisplay::lowFreq || hz > SpectrumDisplay::highFreq)
        {
            scope_data_[i] = 0;
        }
        else
        {
            scope_data_[i] = fft_data_[i];
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
        spectrum_.setVisible(false);
        spectrum_parameters_.setVisible(false);
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
        std::fill(scope_data_.begin(), scope_data_.end(), 0.f);
        spectrum_.setVisible(true);
        spectrum_parameters_.setVisible(true);

        break;
    }
    default:
        skipUpdate = true;
        break;
    }

    if (!skipUpdate)
    {
        background_.updateBackgroundType(scope_mode_);
        // Save the scope mode to the DAW state.
        storage_->getPatch().dawExtraState.editor.oscilloscopeOverlayState.mode =
            static_cast<int>(scope_mode_);
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
                internal::fftSize / (mode == SPECTRUM ? 2.f : 4.f) / storage_->samplerate));
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
            if (pos_ + sz >= internal::fftSize)
            {
                int mv = internal::fftSize - pos_;
                int leftovers = sz - mv;
                std::move(dataL.begin(), dataL.begin() + mv, fft_data_.begin() + pos_);
                calculateSpectrumData();
                spectrum_.updateScopeData(scope_data_.begin(), scope_data_.end());
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
        paintSpectrumBackground(g);
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

void Oscilloscope::Background::updateParameters(SpectrumDisplay::Parameters params)
{
    spectrum_params_ = std::move(params);
}

void Oscilloscope::Background::updateParameters(WaveformDisplay::Parameters params)
{
    waveform_params_ = std::move(params);
}

void Oscilloscope::Background::paintSpectrumBackground(juce::Graphics &g)
{
    juce::Graphics::ScopedSaveState g1(g);

    g.fillAll(skin->getColor(Colors::MSEGEditor::Background));

    if (spectrum_params_.dbRange() == 0.0f)
        return;

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
                           3000.f, 4000.f, 6000.f, 8000.f, 10000.f, 20000.f, 25000.f})
        {
            const auto xPos = freqToX(freq, width);

            if (freq == 10.f || freq == 100.f || freq == 1000.f || freq == 10000.f ||
                freq == 25000.f)
            {
                g.setColour(primaryLine);
            }
            else
            {
                g.setColour(secondaryLine);
            }

            g.drawVerticalLine(xPos, 0, static_cast<float>(height));

            if (freq == 10.f || freq == 25000.f)
            {
                continue;
            }

            const bool over1000 = freq >= 1000.f;
            const auto freqString =
                juce::String(over1000 ? freq / 1000.f : freq) + (over1000 ? "k" : "");
            // Label will go past the end of the scopeRect.
            const auto labelRect =
                juce::Rectangle{SST_STRING_WIDTH_INT(font, freqString), labelHeight}.withCentre(
                    juce::Point<int>(xPos, height + 10));

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
        float minDb = spectrum_params_.noiseFloor();
        float maxDb = spectrum_params_.maxDb();
        float step = spectrum_params_.dbRange() / 8.f;
        float dB = minDb;

        for (int i = 0; i < 9; i++, dB += step)
        {
            const auto yPos = dbToY(dB, height, minDb, maxDb);

            if (std::abs(dB - minDb) < .005 || std::abs(dB - maxDb) < .005)
            {
                g.setColour(primaryLine);
            }
            else
            {
                g.setColour(secondaryLine);
            }

            g.drawHorizontalLine(yPos, 0, static_cast<float>(width + 1));

            std::ostringstream convert;
            convert << std::fixed << std::setprecision(1);
            convert << dB;

            std::string dbString = convert.str() + " dB";

            // Label will go past the end of the scopeRect.
            const auto labelRect =
                juce::Rectangle{SST_STRING_WIDTH_INT(font, dbString), labelHeight}
                    .withBottomY((int)(yPos + (labelHeight / 2)) + 1)
                    .withRightX(width + 32);

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
        const unsigned int prec = waveform_params_.gain() > 1.f ? 2 : 0;

        gain << std::fixed << std::setprecision(prec) << 1.f / waveform_params_.gain();

        g.drawSingleLineText(minus + gain.str(), width + 4, height + 2);
        g.drawSingleLineText((prec == 2 ? "0.00" : "0"), width + 4, height / 2 + 2);
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

        // Split the grid into multiple sections, starting from 0 and ending at wherever the counter
        // speed says we should end at.
        const unsigned int numLines = 11;
        const unsigned int numSections = numLines - 1;
        float counterSpeedInverse = 1.f / waveform_params_.counterSpeed();
        float sampleRateInverse = 1.f / static_cast<float>(storage_->samplerate);
        float endpoint = counterSpeedInverse * sampleRateInverse * static_cast<float>(width);
        std::string time_unit = (endpoint >= 1.f) ? " s" : " ms";

        for (int i = 0; i <= numSections; i++)
        {
            if (i == 0 || i == numSections)
            {
                g.setColour(primaryLine);
            }
            else
            {
                g.setColour(secondaryLine);
            }

            int xPos = (int)((float)width / (float)numSections * i);

            g.drawVerticalLine(xPos, 0, height + 1);

            float timef = (endpoint / numSections) * (float)i;

            if (endpoint < 1.f)
            {
                timef *= 1000;
            }

            std::stringstream time;
            time << std::fixed << std::setprecision(2) << timef;
            std::string timeString = time.str() + time_unit;

            // we don't need decimals for the very first notch on the horizontal axis
            if (timef == 0.f)
            {
                timeString = "0 " + time_unit;
            }

            // Label will go past the end of the scopeRect.
            auto labelWidth = SST_STRING_WIDTH_INT(font, timeString);
            auto labelRect = juce::Rectangle{labelWidth, labelHeight}.withCentre(
                juce::Point<int>(xPos, height + 10));
            auto justify = juce::Justification::bottom;

            // left-align the zero label
            if (timef == 0.f)
            {
                labelRect.setX(xPos);
                labelRect.setWidth(labelWidth);
                justify = juce::Justification::bottomLeft;
            }

            g.setColour(skin->getColor(Colors::MSEGEditor::Axis::SecondaryText));
            g.drawFittedText(timeString, labelRect, justify, 1);
        }
    }
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
