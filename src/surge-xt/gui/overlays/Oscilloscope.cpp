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
#include <limits>
#include "RuntimeFont.h"
#include "SkinColors.h"

using namespace std::chrono_literals;
using std::placeholders::_1;

namespace Surge
{
namespace Overlays
{

WaveformDisplay::WaveformDisplay(SurgeGUIEditor *e, SurgeStorage *s)
    : editor_(e), storage_(s), counter(1.0), max(std::numeric_limits<float>::min()),
      min(std::numeric_limits<float>::min())
{
}

const WaveformDisplay::Parameters &WaveformDisplay::getParameters() const { return params_; }

void WaveformDisplay::setParameters(Parameters parameters) { params_ = std::move(parameters); }

bool WaveformDisplay::frozen() const { return params_.freeze; }

void WaveformDisplay::paint(juce::Graphics &g)
{
#if 0
    CPoint offset(38, 16);

    CRect R(0, 0, getViewSize().getWidth(), getViewSize().getHeight());
    back->draw(pContext, R.offset(offset), offset);

    R(615 - getViewSize().left, 240 - getViewSize().top, 615 + heads->getWidth() - getViewSize().left, 240 + heads->getHeight() / 4 - getViewSize().top);
    heads->draw(pContext, R.offset(offset), CPoint(0, (display * heads->getHeight()) / 4));

    pContext->setDrawMode(CDrawMode(kAntiAliasing));

    // trig-line
    long triggerType = (long)(effect->getParameter(CSmartelectronixDisplay::kTriggerType) * CSmartelectronixDisplay::kNumTriggerTypes + 0.0001);

    if (triggerType == CSmartelectronixDisplay::kTriggerRising || triggerType == CSmartelectronixDisplay::kTriggerFalling) {
        long y = 1 + (long)((1.f - effect->getParameter(CSmartelectronixDisplay::kTriggerLevel)) * (getViewSize().getHeight() - 2));

        CColor grey(229, 229, 229);
        pContext->setFrameColor(grey);
        pContext->drawLine(CPoint(0, y).offset(offset), CPoint(getViewSize().getWidth() - 1, y).offset(offset));
    }

    // zero-line
    CColor orange(179, 111, 56);
    pContext->setFrameColor(orange);
    pContext->drawLine(CPoint(0, getViewSize().getHeight() * 0.5 - 1).offset(offset), CPoint(getViewSize().getWidth() - 1, getViewSize().getHeight() * 0.5 - 1).offset(offset));
#endif

    std::unique_lock l(lock_);
    auto curveColor = skin->getColor(Colors::MSEGEditor::Curve);
    auto path = juce::Path();

    // waveform
    const std::vector<juce::Point<float>> &points = (params_.sync_draw ? copy : peaks);
    double counterSpeedInverse = std::pow(10.f, params_.time_window * 5.f - 1.5f);

    g.setColour(curveColor);
    if (counterSpeedInverse < 1.0) // draw interpolated lines
    {
        // CColor blue(64, 148, 172);
        // pContext->setFrameColor(blue);
        double phase = counterSpeedInverse;
        double dphase = counterSpeedInverse;

        double prevxi = points[0].x;
        double prevyi = points[0].y;
        path.startNewSubPath(prevxi, prevyi);

        for (std::size_t i = 1; i < getWidth() - 1; i++)
        {
            int index = static_cast<int>(phase);
            double alpha = phase - static_cast<double>(index);

            double xi = i;
            double yi = (1.0 - alpha) * points[index * 2].y + alpha * points[(index + 1) * 2].y;
            path.lineTo(xi, yi);
            // pContext->drawLine(CPoint(prevxi, prevyi).offset(offset), CPoint(xi,
            // yi).offset(offset));
            prevxi = xi;
            prevyi = yi;

            phase += dphase;
        }
    }
    else
    {
        // CColor grey(118, 118, 118);
        // pContext->setFrameColor(grey);

        // juce::Point<float> &p1, &p2;
        path.startNewSubPath(points[0].x, points[0].y);
        for (std::size_t i = 0; i < points.size() - 1; i++)
        {
            path.lineTo(points[i].x, points[i].y);
            // g.drawLine(points[i].x, points[i].y, points[i+1].x, points[i+1].y);
            // p1 = points[i];
            // p2 = points[i + 1];
            //  pContext->drawLine(p1.offset(offset), p2.offset(offset));
        }
    }
    g.strokePath(path, juce::PathStrokeType(1.f));

#if 0
    //TODO clean this mess up...
    if (where.x != -1) {
        CPoint whereOffset = where;
        whereOffset.offsetInverse(offset);

        pContext->drawLine(CPoint(0, whereOffset.y).offset(offset), CPoint(getViewSize().getWidth() - 1, whereOffset.y).offset(offset));
        pContext->drawLine(CPoint(whereOffset.x, 0).offset(offset), CPoint(whereOffset.x, getViewSize().getHeight() - 1).offset(offset));

        float gain = powf(10.f, effect->getParameter(CSmartelectronixDisplay::kAmpWindow) * 6.f - 3.f);
        float y = (-2.f * ((float)whereOffset.y + 1.f) / (float)OSC_HEIGHT + 1.f) / gain;
        float x = (float)whereOffset.x * (float)counterSpeedInverse;
        char text[256];

        long lineSize = 10;

        CColor color(179, 111, 56);

        pContext->setFontColor(color);
        pContext->setFont(kNormalFontSmaller);

        readout->draw(pContext, CRect(508, 8, 508 + readout->getWidth(), 8 + readout->getHeight()).offset(offset), CPoint(0, 0));

        CRect textRect(512, 10, 652, 10 + lineSize);
        textRect.offset(offset);

        sprintf(text, "y = %.5f", y);
        pContext->drawString(text, textRect, kLeftText, true);
        textRect.offset(0, lineSize);

        sprintf(text, "y = %.5f dB", cf_lin2db(fabsf(y)));
        pContext->drawString(text, textRect, kLeftText, true);
        textRect.offset(0, lineSize * 2);

        sprintf(text, "x = %.2f samples", x);
        pContext->drawString(text, textRect, kLeftText, true);
        textRect.offset(0, lineSize);

        sprintf(text, "x = %.5f seconds", x / effect->getSampleRate());
        pContext->drawString(text, textRect, kLeftText, true);
        textRect.offset(0, lineSize);

        sprintf(text, "x = %.5f ms", 1000.f * x / effect->getSampleRate());
        pContext->drawString(text, textRect, kLeftText, true);
        textRect.offset(0, lineSize);

        if (x == 0)
            sprintf(text, "x = infinite Hz");
        else
            sprintf(text, "x = %.3f Hz", effect->getSampleRate() / x);

        pContext->drawString(text, textRect, kLeftText, true);
    }
#endif
}

void WaveformDisplay::process(std::vector<float> data)
{
    std::unique_lock l(lock_);
    float gain = std::pow(10.f, params_.amp_window * 6.f - 3.f);
    float triggerLevel = params_.trigger_level * 2.f - 1.f;
    int triggerLimit = static_cast<int>(std::pow(10.f, params_.trigger_limit * 4.f));
    double triggerSpeed = std::pow(10.0, 2.5 * params_.trigger_speed - 5.0);
    double counterSpeed = std::pow(10.0, -params_.time_window * 5. + 1.5); // [0=>10, 1=>0.001]
    double R = 1.0 - 250.0 / static_cast<double>(storage_->samplerate);

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
            if (sample >= triggerLevel && previousSample < triggerLevel)
            {
                trigger = true;
            }
            break;
        case kTriggerFalling:
            // trigger on a falling edge
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
                peaks[j].y = peaks[j + 1].y = getHeight() * 0.5f - 1.f;
            }
            // for (j = pos_+1; j < scope_data_.size(); j++)
            //{
            // scope_data_[j] = getHeight() * 0.5f - 1.f;
            // }

            // copy to a buffer for drawing!
            for (j = 0; j < getWidth() * 2; j++)
            {
                copy[j].y = peaks[j].y;
            }

            // reset everything
            index = 0;
            counter = 1.0;
            max = std::numeric_limits<float>::min();
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
        // the counter keeps track of how many samples/pixel we have.
        if (counter >= 1.0)
        {
            if (index < getWidth())
            {
                // scale here, better than in the graphics thread
                double max_Y = (getHeight() * 0.5 - max * 0.5 * getHeight()) - 1.0;
                double min_Y = (getHeight() * 0.5 - min * 0.5 * getHeight()) - 1.0;

                // thanks to David @ Plogue for this interesting hint!
                peaks[(index << 1)].y = lastIsMax ? min_Y : max_Y;
                peaks[(index << 1) + 1].y = lastIsMax ? max_Y : min_Y;

                index++;
            }

            max = std::numeric_limits<float>::min();
            min = std::numeric_limits<float>::max();
            counter -= 1.0;
        }

        // store for edge-triggers
        previousSample = sample;
        if (trigger)
        {
            repaint();
        }
    }
}

void WaveformDisplay::resized()
{
    peaks.clear();
    copy.clear();
    for (std::size_t j = 0; j < getWidth() * 2; j += 2)
    {
        juce::Point<float> point;
        point.x = static_cast<float>(j) / 2.f;
        point.y = static_cast<float>(getHeight()) * 0.5f - 1.f;
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
      right_chan_button_("R"), scope_mode_button_(*this), spectrogram_(e, s),
      spectrogram_parameters_(e, s), waveform_(e, s), waveform_parameters_(e, s)
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

Oscilloscope::WaveformParameters::WaveformParameters(SurgeGUIEditor *e, SurgeStorage *s)
    : editor_(e), storage_(s)
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
    trigger_speed_.setValue(0.5);
    trigger_level_.setValue(0.5);
    trigger_limit_.setValue(0.5);
    time_window_.setValue(0.75);
    amp_window_.setValue(0.5);
    trigger_speed_.setQuantitizedDisplayValue(0.5);
    trigger_level_.setQuantitizedDisplayValue(0.5);
    trigger_limit_.setQuantitizedDisplayValue(0.5);
    time_window_.setQuantitizedDisplayValue(0.75);
    amp_window_.setQuantitizedDisplayValue(0.5);
    trigger_speed_.setLabel("Trigger Speed");
    trigger_level_.setLabel("Trigger Level");
    trigger_limit_.setLabel("Trigger Limit");
    time_window_.setLabel("Time Window");
    amp_window_.setLabel("Ampl Window");
    trigger_speed_.setDescription("FIXME");
    trigger_level_.setDescription("FIXME");
    trigger_limit_.setDescription("FIXME");
    time_window_.setDescription("FIXME");
    amp_window_.setDescription("FIXME");
    trigger_speed_.setIsLightStyle(true);
    trigger_level_.setIsLightStyle(true);
    trigger_limit_.setIsLightStyle(true);
    time_window_.setIsLightStyle(true);
    amp_window_.setIsLightStyle(true);
    auto updateParameter = [this](float& param, float value) {
        std::lock_guard l(params_lock_);
        params_changed_ = true;
        param = value;
    };
    trigger_speed_.setOnUpdate(std::bind(updateParameter, params_.trigger_speed, _1));
    trigger_level_.setOnUpdate(std::bind(updateParameter, params_.trigger_level, _1));
    trigger_limit_.setOnUpdate(std::bind(updateParameter, params_.trigger_limit, _1));
    time_window_.setOnUpdate(std::bind(updateParameter, params_.time_window, _1));
    amp_window_.setOnUpdate(std::bind(updateParameter, params_.amp_window, _1));
    addAndMakeVisible(trigger_speed_);
    addAndMakeVisible(trigger_level_);
    addAndMakeVisible(trigger_limit_);
    addAndMakeVisible(time_window_);
    addAndMakeVisible(amp_window_);
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
    trigger_speed_.setFont(skin->fontManager->getLatoAtSize(7, juce::Font::plain));
    trigger_level_.setFont(skin->fontManager->getLatoAtSize(7, juce::Font::plain));
    trigger_limit_.setFont(skin->fontManager->getLatoAtSize(7, juce::Font::plain));
    time_window_.setFont(skin->fontManager->getLatoAtSize(7, juce::Font::plain));
    amp_window_.setFont(skin->fontManager->getLatoAtSize(7, juce::Font::plain));
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
}

void Oscilloscope::updateDrawing()
{
    std::lock_guard l(data_lock_);
    if (channel_selection_ != OFF)
    {
        if (scope_mode_ == WAVEFORM)
        {
            // waveform_.scroll();
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
            // FIXME: Normalize dataL.
            // waveform_.updateAudioData(dataL);
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

Oscilloscope::Background::Background() { setOpaque(true); }

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

        labelRect = juce::Rectangle{14, labelHeight}
                        .withBottomY((int)height + labelHeight / 2)
                        .withRightX(width + 16)
                        .translated(2, 0);

        g.drawText("-1.0", labelRect, juce::Justification::left, 1);

        labelRect = juce::Rectangle{14, labelHeight}
                        .withBottomY((int)(height / 2 + labelHeight / 2))
                        .withRightX(width + 16)
                        .translated(2, 0);

        g.drawText("0.0", labelRect, juce::Justification::left, 1);

        labelRect = juce::Rectangle{14, labelHeight}
                        .withBottomY(labelHeight / 2)
                        .withRightX(width + 16)
                        .translated(2, 0);

        g.drawText("+1.0", labelRect, juce::Justification::left, 1);
    }

    // Vertical grid.
    {
        auto gs = juce::Graphics::ScopedSaveState(g);
        g.addTransform(juce::AffineTransform().translated(scopeRect.getX(), scopeRect.getY()));
        g.setFont(font);

        for (int ms : {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100})
        {
            auto xPos = timeToX(std::chrono::milliseconds(ms), width);

            if (ms == 0 || ms == 100)
            {
                g.setColour(primaryLine);
            }
            else
            {
                g.setColour(secondaryLine);
            }

            g.drawVerticalLine(xPos, 0, height + 1);

            auto timeString = std::to_string(ms);

            if (ms == 100)
            {
                timeString += " ms";
            }

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

Oscilloscope::Waveform::Waveform(SurgeGUIEditor *e, SurgeStorage *s)
    : editor_(e), storage_(s), period_(100), period_float_(period_),
      period_samples_(static_cast<std::size_t>(s->samplerate * period_float_.count())),
      scope_data_(s->samplerate), last_sample_rate_(s->samplerate)
{
    std::fill(scope_data_.begin(), scope_data_.end(), 0);
}

void Oscilloscope::Waveform::paint(juce::Graphics &g)
{
    auto scopeRect = getLocalBounds().transformedBy(getTransform().inverted());
    auto width = scopeRect.getWidth();
    auto height = scopeRect.getHeight();
    auto curveColor = skin->getColor(Colors::MSEGEditor::Curve);

    auto path = juce::Path();

    // Start path.
    std::unique_lock l(data_lock_);
    for (int i = 0; i < period_samples_; i++)
    {
        const float x = juce::jmap(static_cast<float>(i), 0.f, static_cast<float>(period_samples_),
                                   0.f, static_cast<float>(width));
        const float y = juce::jmap(scope_data_[i], -1.f, 1.f, static_cast<float>(height), 0.f);
        if (i)
        {
            path.lineTo(x, y);
        }
        else
        {
            path.startNewSubPath(x, y);
        }
    }
    g.setColour(curveColor);
    g.strokePath(path, juce::PathStrokeType(1.f));
}

void Oscilloscope::Waveform::scroll()
{
    std::vector<float> data = upcoming_data_.popall();
    std::unique_lock l(data_lock_);
    // Check for sample rate changes. Have to just redo everything when that happens.
    if (last_sample_rate_ != storage_->samplerate)
    {
        last_sample_rate_ = storage_->samplerate;
        scope_data_.resize(last_sample_rate_);
        period_samples_ = static_cast<std::size_t>(last_sample_rate_ * period_float_.count());
    }
    std::rotate(scope_data_.begin(), scope_data_.begin() + data.size(), scope_data_.end());
    std::move(data.begin(), data.end(), scope_data_.end() - data.size());
    repaint();
}

void Oscilloscope::Waveform::updateAudioData(const std::vector<float> &buf)
{
    upcoming_data_.push(buf);
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
