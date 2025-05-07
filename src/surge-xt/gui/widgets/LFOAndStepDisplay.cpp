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

#include "LFOAndStepDisplay.h"
#include "SurgeImageStore.h"
#include "SurgeImage.h"
#include "SurgeGUIEditor.h"
#include "SurgeGUIUtils.h"
#include "SurgeJUCEHelpers.h"
#include "RuntimeFont.h"
#include <algorithm>
#include <chrono>
#include "widgets/MenuCustomComponents.h"
#include "AccessibleHelpers.h"
#include "overlays/TypeinParamEditor.h"

namespace Surge
{
namespace Widgets
{

struct TimeB
{
    TimeB(std::string i) : tag(i) { start = std::chrono::high_resolution_clock::now(); }

    ~TimeB()
    {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = end - start;

        auto durationFloat =
            std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        // std::cout << "Block in " << tag << " took " << durationFloat << "us" << std::endl;
        // FIX - record this so I can report
    }
    std::string tag;
    std::chrono::time_point<std::chrono::high_resolution_clock> start;
};

LFOAndStepDisplay::LFOAndStepDisplay(SurgeGUIEditor *e)
    : juce::Component(), WidgetBaseMixin<LFOAndStepDisplay>(this), guiEditor(e)
{
    setTitle("LFO Type And Display");
    setAccessible(true);
    setFocusContainerType(juce::Component::FocusContainerType::focusContainer);

    memset(paramsFromLastDrawCall, 0, sizeof(paramsFromLastDrawCall));
    memset(settingsFromLastDrawCall, 0, sizeof(settingsFromLastDrawCall));

    backingImage = std::make_unique<juce::Image>(juce::Image::PixelFormat::ARGB, 50, 50, true);
    waveformIsUpdated = true;

    typeLayer = std::make_unique<OverlayAsAccessibleContainer>("LFO Type");
    addAndMakeVisible(*typeLayer);
    for (int i = 0; i < n_lfo_types; ++i)
    {
        auto q = std::make_unique<OverlayAsAccessibleButtonWithValue<LFOAndStepDisplay>>(
            this, lt_names[i]);
        q->onPress = [this, i](auto *t) { updateShapeTo(i); };
        q->onReturnKey = [this, i](auto *t) {
            updateShapeTo(i);
            return true;
        };
        q->onGetValue = [this, i](auto *t) { return (i == lfodata->shape.val.i) ? 1 : 0; };
        typeLayer->addAndMakeVisible(*q);
        typeAccOverlays[i] = std::move(q);
    }

    stepLayer = std::make_unique<OverlayAsAccessibleContainer>("Step Sequencer");
    addChildComponent(*stepLayer);

    auto b0 = std::make_unique<OverlayAsAccessibleButton<LFOAndStepDisplay>>(
        this, "Rotate Sequence Right", juce::AccessibilityRole::button);
    b0->onPress = [this](auto *p) { shiftRight(); };
    b0->onReturnKey = [this](auto *p) {
        shiftRight();
        return false;
    };
    stepLayer->addChildComponent(*b0);
    stepJogOverlays[0] = std::move(b0);
    auto b1 = std::make_unique<OverlayAsAccessibleButton<LFOAndStepDisplay>>(
        this, "Rotate Sequence Left", juce::AccessibilityRole::button);
    b1->onPress = [this](auto *p) { shiftLeft(); };
    b1->onReturnKey = [this](auto *p) {
        shiftLeft();
        return false;
    };
    stepLayer->addChildComponent(*b1);
    stepJogOverlays[1] = std::move(b1);

    for (int i = 0; i < n_stepseqsteps; ++i)
    {
        std::string sn = "Step Value " + std::to_string(i + 1);
        auto q = std::make_unique<OverlayAsAccessibleSlider<LFOAndStepDisplay>>(this, sn);

        q->onReturnPressed = [this, i](auto *) { showStepTypein(i); };

        q->onGetValue = [this, i](auto *T) { return ss->steps[i]; };
        q->onValueToString = [this, i](auto *T, float f) -> std::string {
            auto q = f * 12.f;

            if (fabs(q - std::round(q)) < 0.001)
            {
                auto twl = std::string("twelfths");

                if ((int)fabs(std::round(q)) == 1)
                    twl = "twelfth";

                auto res = fmt::format("{:.3f} ({} {})", f, (int)std::round(q), twl);

                return res;
            }

            return fmt::format("{:.3f}", f);
        };

        q->onSetValue = [this, i](auto *T, float f) {
            auto bscg = BeginStepGuard(this);
            ss->steps[i] = f;
            stepSeqDirty();
            repaint();
            return;
        };

        q->onJogValue = [this, i](auto *t, int dir, bool isShift, bool isControl) {
            auto bscg = BeginStepGuard(this);
            int step = i;

            if (step >= 0)
            {
                auto f = ss->steps[step];
                auto delt = 0.05;

                if (isControl)
                    delt = 0.01;
                if (isShift)
                    delt = 1.0 / 12.0;
                if (dir < 0)
                    delt *= -1;

                if (isUnipolar())
                    f = limit01(f + delt);
                else
                    f = limitpm1(f + delt);

                ss->steps[step] = f;
                stepSeqDirty();
                repaint();
            }
        };

        q->onMinMaxDef = [this, i](auto *t, int mmd) {
            auto bscg = BeginStepGuard(this);

            if (mmd == 1)
                ss->steps[i] = 1.f;
            if (mmd == -1)
                ss->steps[i] = isUnipolar() ? 0.f : -1.f;
            if (mmd == 0)
                ss->steps[i] = 0.f;

            stepSeqDirty();
            repaint();
        };

        q->onMenuKey = [this, i](auto *t) { showStepRMB(i); };

        stepLayer->addChildComponent(*q);
        stepSliderOverlays[i] = std::move(q);
    }

    for (int i = 0; i < n_stepseqsteps; ++i)
    {
        std::string sn = "Trigger Envelopes " + std::to_string(i + 1);
        auto q = std::make_unique<OverlayAsAccessibleSlider<LFOAndStepDisplay>>(this, sn);
        q->min = 0;
        q->max = 3;
        q->step = 1;

        q->onGetValue = [this, i](auto *) {
            uint64_t maski = ss->trigmask & (UINT64_C(1) << i);
            uint64_t maski16 = ss->trigmask & (UINT64_C(1) << (i + n_stepseqsteps));
            uint64_t maski32 = ss->trigmask & (UINT64_C(1) << (i + (n_stepseqsteps * 2)));

            if (maski)
                return 1.f;
            if (maski16)
                return 2.f;
            if (maski32)
                return 3.f;
            return 0.f;
        };

        q->onSetValue = [this, i](auto *, float f) {
            int q = round(f);
            uint64_t b1 = (UINT64_C(1) << i);
            uint64_t b2 = (UINT64_C(1) << (i + n_stepseqsteps));
            uint64_t b3 = (UINT64_C(1) << (i + (n_stepseqsteps * 2)));
            uint64_t maskOff = 0xFFFFFFFFFFFFFFFF;
            uint64_t maskOn = 0;

            if (q == 0)
            {
                maskOff = maskOff & ~b1 & ~b2 & ~b3;
            }
            if (q == 1)
            {
                maskOff = maskOff & ~b2 & ~b3;
                maskOn = b1;
            }
            if (q == 2)
            {
                maskOff = maskOff & ~b1 & ~b3;
                maskOn = b2;
            }
            if (q == 3)
            {
                maskOff = maskOff & ~b1 & ~b2;
                maskOn = b3;
            }
            ss->trigmask &= maskOff;
            ss->trigmask |= maskOn;
            repaint();
        };

        q->onJogValue = [this, i](auto *, int dir, auto, auto) {
            auto s = dynamic_cast<OverlayAsAccessibleSlider<LFOAndStepDisplay> *>(
                stepTriggerOverlays[i].get());
            if (s)
            {
                auto v = s->onGetValue(this);
                v = v + dir;
                if (v < 0)
                    v = 3;
                if (v > 3)
                    v = 0;
                s->onSetValue(this, v);
            }
        };

        q->onValueToString = [](auto *, float v) {
            auto q = (int)round(v);
            switch (q)
            {
            case 1:
                return "Trigger Filter env and Amp env";
            case 2:
                return "Trigger Filter env";
            case 3:
                return "Trigger Amp env";
            }
            return "No Triggers";
        };

        stepLayer->addChildComponent(*q);
        stepTriggerOverlays[i] = std::move(q);
    }

    auto l0 =
        std::make_unique<OverlayAsAccessibleSlider<LFOAndStepDisplay>>(this, "Loop Start Point");
    l0->min = 0;
    l0->max = n_stepseqsteps;
    l0->step = 1;
    l0->onGetValue = [this](auto *) { return ss->loop_start; };
    l0->onSetValue = [this](auto *, float f) {
        auto bscg = BeginStepGuard(this);
        ss->loop_start = (int)round(f);
        stepSeqDirty();
        repaint();
    };
    l0->onJogValue = [this](auto *, int dir, bool, bool) {
        auto bscg = BeginStepGuard(this);
        auto n = limit_range(ss->loop_start + dir, 0, 15);
        ss->loop_start = n;
        stepSeqDirty();
        repaint();
    };
    l0->onMinMaxDef = [this](auto *, int mmd) {
        auto bscg = BeginStepGuard(this);
        if (mmd == 1)
            ss->loop_start = ss->loop_end;
        else
            ss->loop_start = 0;
        stepSeqDirty();
        repaint();
    };
    loopEndOverlays[0] = std::move(l0);
    stepLayer->addChildComponent(*loopEndOverlays[0]);

    l0 = std::make_unique<OverlayAsAccessibleSlider<LFOAndStepDisplay>>(this, "Loop End Point");
    l0->min = 0;
    l0->max = n_stepseqsteps;
    l0->step = 1;
    l0->onGetValue = [this](auto *) { return ss->loop_end; };
    l0->onSetValue = [this](auto *, float f) {
        auto bscg = BeginStepGuard(this);
        ss->loop_end = (int)round(f);
        stepSeqDirty();
        repaint();
    };
    l0->onJogValue = [this](auto *, int dir, bool, bool) {
        auto bscg = BeginStepGuard(this);
        auto n = limit_range(ss->loop_end + dir, 0, 15);
        ss->loop_end = n;
        stepSeqDirty();
        repaint();
    };
    l0->onMinMaxDef = [this](auto *, int mmd) {
        auto bscg = BeginStepGuard(this);
        if (mmd == -1)
            ss->loop_end = ss->loop_end;
        else
            ss->loop_end = 15;
        stepSeqDirty();
        repaint();
    };
    loopEndOverlays[1] = std::move(l0);
    stepLayer->addChildComponent(*loopEndOverlays[1]);
}

void LFOAndStepDisplay::resized()
{
    outer = getLocalBounds();
    left_panel = outer.withRight(outer.getX() + lpsize + margin);
    main_panel = outer.withLeft(left_panel.getRight());
    waveform_display = main_panel.withTrimmedLeft(19 - margin).withTrimmedRight(margin);

    ss_shift_bg = main_panel.toFloat().withWidth(10).withHeight(32).translated(2, 0).withCentre(
        juce::Point<float>(main_panel.getX() + 10, main_panel.getCentreY()));
    ss_shift_left = ss_shift_bg.reduced(1, 1).withBottom(ss_shift_bg.getY() + 16);
    ss_shift_right = ss_shift_left.translated(0, 16);

    typeLayer->setBounds(getLocalBounds());
    stepLayer->setBounds(getLocalBounds());

    for (int i = 0; i < n_lfo_types; ++i)
    {
        int xp = (i % 2) * 25 + left_panel.getX();
        int yp = (i / 2) * 15 + left_panel.getY() + margin + 2;

        shaperect[i] = juce::Rectangle<int>(xp, yp, 25, 15);
        typeAccOverlays[i]->setBounds(shaperect[i]);
    }
    stepJogOverlays[0]->setBounds(ss_shift_right.toNearestInt());
    stepJogOverlays[1]->setBounds(ss_shift_left.toNearestInt());
    loopEndOverlays[0]->setBounds(left_panel.getX(), waveform_display.getHeight() - 10, 10, 10);
    loopEndOverlays[1]->setBounds(left_panel.getX() + 10, waveform_display.getHeight() - 10, 10,
                                  10);

    auto wfw = waveform_display.getWidth() * 1.f / n_stepseqsteps;
    auto ssr = waveform_display.withWidth(wfw).withTrimmedTop(10);
    bool showtrig = false;

    if (lfoid < n_lfos_voice)
    {
        showtrig = true;
    }

    for (const auto &q : stepSliderOverlays)
    {
        q->setBounds(ssr);

        if (lfodata && lfodata->shape.val.i == lt_stepseq)
        {
            q->setVisible(true);
        }
        else
        {
            q->setVisible(false);
        }

        ssr = ssr.translated(wfw, 0);
    }

    ssr = waveform_display.withWidth(wfw).withHeight(10);

    for (const auto &q : stepTriggerOverlays)
    {
        q->setBounds(ssr);

        if (lfodata && lfodata->shape.val.i == lt_stepseq)
        {
            q->setVisible(showtrig);
        }
        else
        {
            q->setVisible(false);
        }

        ssr = ssr.translated(wfw, 0);
    }
}

void LFOAndStepDisplay::paint(juce::Graphics &g)
{
    TimeB paint("outerPaint");

    if (ss && lfodata->shape.val.i == lt_stepseq)
    {
        paintStepSeq(g);
    }
    else
    {

        if (!paramsHasChanged())
        {
            g.drawImage(*backingImage, getLocalBounds().toFloat(),
                        juce::RectanglePlacement::fillDestination);
            paintTypeSelector(g);
            return;
        }
        else
        {
            juce::Colour color =
                juce::Colour((unsigned char)0, (unsigned char)0, (unsigned char)0, 0.f);

            backingImage->clear(backingImage->getBounds());
        }

        juce::Graphics gr = juce::Graphics(*backingImage);
        float zoomFloat = (float)zoomFactor / 100.f;

        gr.addTransform(juce::AffineTransform().scale(zoomFloat * 2.f));

        paintWaveform(gr);

        g.drawImage(*backingImage, getLocalBounds().toFloat(),
                    juce::RectanglePlacement::fillDestination);
        waveformIsUpdated = false;
    }

    paintTypeSelector(g);
}

bool LFOAndStepDisplay::paramsHasChanged()
{
    bool hasChanged = false;

    auto *p = &lfodata->rate;      // look in the definition of LFOStorage for which param is first
    while (p <= &lfodata->release) // and last
    {

        if (paramsFromLastDrawCall[p->param_id_in_scene].i != p->val.i)
            hasChanged = true;

        paramsFromLastDrawCall[p->param_id_in_scene].i = p->val.i;
        ++p;
    };

    if (lfodata->rate.deactivated != settingsFromLastDrawCall[0].b)
    {
        settingsFromLastDrawCall[0].b = lfodata->rate.deactivated;
        hasChanged = true;
    }

    if (lfodata->deform.deform_type != paramsFromLastDrawCall[1].i)
    {
        paramsFromLastDrawCall[1].i = lfodata->deform.deform_type;
        hasChanged = true;
    }

    if (lfodata->rate.temposync != paramsFromLastDrawCall[2].b)
    {
        paramsFromLastDrawCall[2].b = lfodata->rate.temposync;
        hasChanged = true;
    }

    if (lfoStorageFromLastDrawingCall != lfodata)
        hasChanged = true;

    if (forceRepaint)
    {
        hasChanged = true;
        forceRepaint = false;
    }

    lfoStorageFromLastDrawingCall = lfodata;

    return hasChanged;
}

void LFOAndStepDisplay::setZoomFactor(int zoom)
{

    if (zoomFactor != zoom)
    {
        float zoomFloat = (float)zoom / 100.f;
        forceRepaint = true;
        backingImage = std::make_unique<juce::Image>(juce::Image::PixelFormat::ARGB,
                                                     outer.getWidth() * zoomFloat * 2,
                                                     outer.getHeight() * zoomFloat * 2, true);
    }

    zoomFactor = zoom;
}

void LFOAndStepDisplay::paintWaveform(juce::Graphics &g)
{
    TimeB mainTimer("-- paintWaveform");

    juce::Path deactPath, path, eupath, edpath;
    bool drawBeats = isAnythingTemposynced();

    pdata tp[n_scene_params], tpd[n_scene_params];

    tp[lfodata->delay.param_id_in_scene].i = lfodata->delay.val.i;
    tp[lfodata->attack.param_id_in_scene].i = lfodata->attack.val.i;
    tp[lfodata->hold.param_id_in_scene].i = lfodata->hold.val.i;
    tp[lfodata->decay.param_id_in_scene].i = lfodata->decay.val.i;
    tp[lfodata->sustain.param_id_in_scene].i = lfodata->sustain.val.i;
    tp[lfodata->release.param_id_in_scene].i = lfodata->release.val.i;

    tp[lfodata->magnitude.param_id_in_scene].i = lfodata->magnitude.val.i;
    tp[lfodata->rate.param_id_in_scene].i = lfodata->rate.val.i;
    tp[lfodata->shape.param_id_in_scene].i = lfodata->shape.val.i;
    tp[lfodata->start_phase.param_id_in_scene].i = lfodata->start_phase.val.i;
    tp[lfodata->deform.param_id_in_scene].i = lfodata->deform.val.i;
    tp[lfodata->trigmode.param_id_in_scene].i = lm_keytrigger;

    float susTime = 0.5;
    bool msegRelease = false;
    float msegReleaseAt = 0;
    float lfoEnvelopeDAHDTime = pow(2.0f, lfodata->delay.val.f) + pow(2.0f, lfodata->attack.val.f) +
                                pow(2.0f, lfodata->hold.val.f) + pow(2.0f, lfodata->decay.val.f);

    if (lfodata->shape.val.i == lt_mseg)
    {
        // We want the sus time to get us through at least one loop
        if (ms->loopMode == MSEGStorage::GATED_LOOP && ms->editMode == MSEGStorage::ENVELOPE &&
            ms->loop_end >= 0)
        {
            float loopEndsAt = ms->segmentEnd[ms->loop_end];
            susTime = std::max(0.5f, loopEndsAt - lfoEnvelopeDAHDTime);
            msegReleaseAt = lfoEnvelopeDAHDTime + susTime;
            msegRelease = true;
        }
    }

    float totalEnvTime = lfoEnvelopeDAHDTime + std::min(pow(2.0f, lfodata->release.val.f), 4.f) +
                         0.5; // susTime; this is now 0.5 to keep the envelope fixed in gate mode

    float rateInHz = pow(2.0, (double)lfodata->rate.val.f);
    if (lfodata->rate.temposync)
        rateInHz *= storage->temposyncratio;

    /*
     * What we want is no more than 50 wavelengths. So
     *
     * totalEnvTime * rateInHz < 50
     *
     * totalEnvTime < 50 / rateInHz
     *
     * so
     */
    // std::cout << _D(totalEnvTime);
    totalEnvTime = std::min(totalEnvTime, 50.f / rateInHz);
    // std::cout << _D(totalEnvTime) << std::endl;
    // std::cout << _D(rateInHz) << _D(1.0/rateInHz) << _D(totalEnvTime*rateInHz) << std::endl;

    LFOModulationSource *tlfo = new LFOModulationSource();
    LFOModulationSource *tFullWave = nullptr;
    tlfo->assign(storage, lfodata, tp, 0, ss, ms, fs, true);
    populateLFOMS(tlfo);
    tlfo->attack();

    LFOStorage deactivateStorage;
    bool hasFullWave = false, waveIsAmpWave = false;

    if (lfodata->rate.deactivated)
    {
        hasFullWave = true;
        deactivateStorage = *lfodata;
        std::copy(std::begin(tp), std::end(tp), std::begin(tpd));

        auto desiredRate = log2(1.f / totalEnvTime);
        if (lfodata->shape.val.i == lt_mseg)
        {
            desiredRate = log2(ms->totalDuration / totalEnvTime);
        }

        deactivateStorage.rate.deactivated = false;
        deactivateStorage.rate.val.f = desiredRate;
        deactivateStorage.start_phase.val.f = 0;
        tpd[lfodata->start_phase.param_id_in_scene].f = 0;
        tpd[lfodata->rate.param_id_in_scene].f = desiredRate;
        tFullWave = new LFOModulationSource();
        tFullWave->assign(storage, &deactivateStorage, tpd, 0, ss, ms, fs, true);
        populateLFOMS(tFullWave);
        tFullWave->attack();
    }
    else if (lfodata->magnitude.val.f != lfodata->magnitude.val_max.f && skin->getVersion() >= 2)
    {
        bool useAmpWave = Surge::Storage::getUserDefaultValue(
            storage, Surge::Storage::ShowGhostedLFOWaveReference, 1);
        if (useAmpWave)
        {
            hasFullWave = true;
            waveIsAmpWave = true;
            deactivateStorage = *lfodata;
            std::copy(std::begin(tp), std::end(tp), std::begin(tpd));

            deactivateStorage.magnitude.val.f = 1.f;
            tpd[lfodata->magnitude.param_id_in_scene].f = 1.f;
            tFullWave = new LFOModulationSource();
            tFullWave->assign(storage, &deactivateStorage, tpd, 0, ss, ms, fs, true);
            populateLFOMS(tFullWave);
            tFullWave->attack();
        }
    }

    if (lfodata->shape.val.i == lt_formula)
    {
        if (!tlfo->formulastate.useEnvelope)
        {
            totalEnvTime = 5.5;
        }
    }

    bool drawEnvelope = !lfodata->delay.deactivated;

    if (skin->hasColor(Colors::LFO::Waveform::Background))
    {
        g.setColour(skin->getColor(Colors::LFO::Waveform::Background));
        g.fillRect(waveform_display);
    }

    int minSamples = (1 << 0) * (int)(waveform_display.getWidth());
    int totalSamples =
        std::max((int)minSamples, (int)(totalEnvTime * storage->samplerate / BLOCK_SIZE));
    float drawnTime = totalSamples * storage->samplerate_inv * BLOCK_SIZE;

    // OK so let's assume we want about 1000 pixels worth tops in
    int averagingWindow = (int)(totalSamples / 1000.0) + 1;

    float valScale = 100.0;
    int susCountdown = -1;

    float priorval = 0.f, priorwval = 0.f;

    bool warnForInvalid{false};
    std::string invalidMessage;

    for (int i = 0; i < totalSamples; i += averagingWindow)
    {
        float val = 0;
        float wval = 0;
        float eval = 0;
        float minval = 1000000, minwval = 1000000;
        float maxval = -1000000, maxwval = -1000000;
        float firstval;
        float lastval;

        for (int s = 0; s < averagingWindow; s++)
        {
            tlfo->process_block();

            if (tFullWave)
            {
                tFullWave->process_block();
            }

            if (lfodata->shape.val.i == lt_formula)
            {
                if (!tlfo->formulastate.isFinite ||
                    (tFullWave && !tFullWave->formulastate.isFinite))
                {
                    warnForInvalid = true;
                    invalidMessage = "Formula produced nan or inf";
                }
            }

            if (susCountdown < 0 && tlfo->env_state == lfoeg_stuck)
            {
                susCountdown = susTime * storage->samplerate / BLOCK_SIZE;
            }
            else if (susCountdown == 0 && tlfo->env_state == lfoeg_stuck)
            {
                tlfo->release();

                if (tFullWave)
                {
                    tFullWave->release();
                }
            }
            else if (susCountdown > 0)
            {
                susCountdown--;
            }

            val += tlfo->get_output(modIndex);

            if (tFullWave)
            {
                auto v = tFullWave->get_output(modIndex);

                minwval = std::min(v, minwval);
                maxwval = std::max(v, maxwval);
                wval += v;
            }

            if (s == 0)
            {
                firstval = tlfo->get_output(modIndex);
            }

            if (s == averagingWindow - 1)
            {
                lastval = tlfo->get_output(modIndex);
            }

            minval = std::min(tlfo->get_output(modIndex), minval);
            maxval = std::max(tlfo->get_output(modIndex), maxval);
            eval += tlfo->env_val * lfodata->magnitude.get_extended(lfodata->magnitude.val.f);
        }

        val = val / averagingWindow;
        wval = wval / averagingWindow;
        eval = eval / averagingWindow;
        val = ((-val + 1.0f) * 0.5 * 0.8 + 0.1) * valScale;
        wval = ((-wval + 1.0f) * 0.5 * 0.8 + 0.1) * valScale;
        minwval = ((-minwval + 1.0f) * 0.5 * 0.8 + 0.1) * valScale;
        maxwval = ((-maxwval + 1.0f) * 0.5 * 0.8 + 0.1) * valScale;

        float euval = ((-eval + 1.0f) * 0.5 * 0.8 + 0.1) * valScale;
        float edval = ((eval + 1.0f) * 0.5 * 0.8 + 0.1) * valScale;
        float xc = valScale * i / totalSamples;

        if (i == 0)
        {
            path.startNewSubPath(xc, val);
            eupath.startNewSubPath(xc, euval);

            if (!isUnipolar())
            {
                edpath.startNewSubPath(xc, edval);
            }

            if (tFullWave)
            {
                deactPath.startNewSubPath(xc, wval);
            }

            priorval = val;
            priorwval = wval;
        }
        else
        {
            minval = ((-minval + 1.0f) * 0.5 * 0.8 + 0.1) * valScale;
            maxval = ((-maxval + 1.0f) * 0.5 * 0.8 + 0.1) * valScale;
            // Windows is sensitive to out-of-order line draws in a way which causes spikes.
            // Make sure we draw one closest to prior first. See #1438
            float firstval = minval;
            float secondval = maxval;

            if (priorval - minval < maxval - priorval)
            {
                firstval = maxval;
                secondval = minval;
            }

            path.lineTo(xc - 0.1 * valScale / totalSamples, firstval);
            path.lineTo(xc + 0.1 * valScale / totalSamples, secondval);

            priorval = val;
            eupath.lineTo(xc, euval);
            edpath.lineTo(xc, edval);

            // We can skip the ordering thing since we know we have set rate here to a low rate
            if (tFullWave)
            {
                firstval = minwval;
                secondval = maxwval;
                if (priorwval - minwval < maxwval - priorwval)
                {
                    firstval = maxwval;
                    secondval = minwval;
                }
                deactPath.lineTo(xc - 0.1 * valScale / totalSamples, firstval);
                deactPath.lineTo(xc + 0.1 * valScale / totalSamples, secondval);
                priorwval = wval;
            }
        }
    }

    if (lfodata->shape.val.i == lt_formula)
    {
        drawEnvelope = tlfo->formulastate.useEnvelope;
    }

    tlfo->completedModulation();

    delete tlfo;

    if (tFullWave)
    {
        tFullWave->completedModulation();
        delete tFullWave;
    }

    auto at =
        juce::AffineTransform()
            .scale(waveform_display.getWidth() / valScale, waveform_display.getHeight() / valScale)
            .translated(waveform_display.getTopLeft());

    for (int i = -1; i < 2; ++i)
    {
        float off = i * 0.4 + 0.5; // so that/s 0.1, 0.5, 0.9
        float x0 = 0, y0 = valScale * off, x1 = valScale, y1 = valScale * off;

        at.transformPoint(x0, y0);
        at.transformPoint(x1, y1);

        if (i == 0)
        {
            g.setColour(skin->getColor(Colors::LFO::Waveform::Center));
        }
        else
        {
            g.setColour(skin->getColor(Colors::LFO::Waveform::Bounds));
        }

        g.drawLine(x0, y0, x1, y1, 1);
    }

    auto pointColor = skin->getColor(Colors::LFO::Waveform::Dots);
    int nxd = 61, nyd = 9;

    for (int xd = 0; xd < nxd; xd++)
    {
        float normx = 1.f * xd / (nxd - 1) * 0.99;
        for (int yd = 1; yd < nyd - 1; yd++)
        {
            if (yd == (nyd - 1) / 2)
                continue;

            float normy = 1.f * yd / (nyd - 1);
            float dotPointX = normx * valScale, dotPointY = (0.8 * normy + 0.1) * valScale;
            at.transformPoint(dotPointX, dotPointY);
            float esize = 1.f;
            float xoff = (xd == 0 ? esize : 0);
            g.setColour(pointColor);
            g.fillEllipse(dotPointX - esize, dotPointY - esize, esize, esize);
        }
    }

    if (drawEnvelope)
    {
        g.setColour(skin->getColor(Colors::LFO::Waveform::Envelope));
        g.strokePath(eupath, juce::PathStrokeType(1.f), at);

        if (!isUnipolar())
        {
            g.strokePath(edpath, juce::PathStrokeType(1.f), at);
        }
    }

    if (drawBeats)
    {
        // calculate beat grid related data
        auto bpm = storage->temposyncratio * 120;

        auto denFactor = 4.0 / tsDen;
        auto beatsPerSecond = bpm / 60.0;
        auto secondsPerBeat = 1 / beatsPerSecond;
        auto deltaBeat = secondsPerBeat / drawnTime * valScale * denFactor;

        int nBeats = std::max(ceil(drawnTime * beatsPerSecond / denFactor), 1.0);

        auto measureLen = deltaBeat * tsNum;
        int everyMeasure = 1;

        while (measureLen < 4) // a hand selected parameter by playing around with it, tbh
        {
            measureLen *= 2;
            everyMeasure *= 2;
        }

        for (auto l = 0; l <= nBeats; ++l)
        {
            auto xp = deltaBeat * l;

            if (l % (tsNum * everyMeasure) == 0 || nBeats <= tsDen)
            {
                auto soff = 0.0;
                if (l > 10)
                    soff = 0.0;
                float vruleSX = xp, vruleSY = valScale * .15, vruleEX = xp,
                      vruleEY = valScale * .85;
                at.transformPoint(vruleSX, vruleSY);
                at.transformPoint(vruleEX, vruleEY);
                // major beat divisions on the LFO waveform bg
                g.setColour(skin->getColor(Colors::LFO::Waveform::Ruler::ExtendedTicks));
                g.drawLine(vruleSX, vruleSY, vruleEX, vruleEY, 1);

                juce::Point<float> sp(xp, valScale * (.01)), ep(xp, valScale * (.1));
                sp = sp.transformedBy(at);
                ep = ep.transformedBy(at);
                // ticks for major beats
                if (l % tsNum == 0)
                    g.setColour(skin->getColor(Colors::LFO::Waveform::Ruler::Ticks));
                else
                    // small ticks for the ruler for nbeats case
                    g.setColour(skin->getColor(Colors::LFO::Waveform::Ruler::SmallTicks));

                g.drawLine(sp.getX(), sp.getY(), ep.getX(), ep.getY(), 1.0);

                std::string s = fmt::format("{:d}", l + 1);

                juce::Point<int> tp(xp + 1, valScale * 0.0);
                tp = tp.transformedBy(at);
                g.setColour(skin->getColor(Colors::LFO::Waveform::Ruler::Text));
                g.setFont(skin->fontManager->lfoTypeFont);
                g.drawText(s, tp.x, tp.y, 20, 10, juce::Justification::bottomLeft);
            }
            else if (everyMeasure == 1)
            {
                auto sp = juce::Point<float>(xp, valScale * 0.06).transformedBy(at);
                auto ep = juce::Point<float>(xp, valScale * 0.1).transformedBy(at);

                if (l % tsNum == 0)
                    g.setColour(juce::Colours::black);
                else
                    // small ticks for the ruler
                    g.setColour(skin->getColor(Colors::LFO::Waveform::Ruler::SmallTicks));
                g.drawLine(sp.x, sp.y, ep.x, ep.y, 0.5);
            }
        }
    }

    if (hasFullWave)
    {
        if (waveIsAmpWave)
        {
            g.setColour(skin->getColor(Colors::LFO::Waveform::GhostedWave));
            auto dotted = juce::Path();
            auto st = juce::PathStrokeType(0.3, juce::PathStrokeType::beveled,
                                           juce::PathStrokeType::butt);
            float dashLength[2] = {4.f, 2.f};
            st.createDashedStroke(dotted, deactPath, dashLength, 2, at);
            g.strokePath(dotted, st);
        }
        else
        {
            g.setColour(skin->getColor(Colors::LFO::Waveform::DeactivatedWave));
            g.strokePath(deactPath,
                         juce::PathStrokeType(0.5f, juce::PathStrokeType::beveled,
                                              juce::PathStrokeType::butt),
                         at);
        }
    }

    g.setColour(skin->getColor(Colors::LFO::Waveform::Wave));
    g.strokePath(
        path, juce::PathStrokeType(1.f, juce::PathStrokeType::beveled, juce::PathStrokeType::butt),
        at);

    // lower ruler calculation
    // find time delta
    int maxNumLabels = 5;
    std::vector<float> timeDeltas = {0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0};
    auto currDelta = timeDeltas.begin();
    while (currDelta != timeDeltas.end() && (drawnTime / *currDelta) > maxNumLabels)
    {
        currDelta++;
    }
    float delta = timeDeltas.back();
    if (currDelta != timeDeltas.end())
        delta = *currDelta;

    int nLabels = (int)(drawnTime / delta) + 1;
    float dx = delta / drawnTime * valScale;

    for (int l = 0; l < nLabels; ++l)
    {
        float xp = dx * l;
        float yp = valScale * 0.9;
        float typ = yp;
        auto tp = juce::Point<float>(xp + 0.5, typ + 0.5).transformedBy(at);

        g.setColour(skin->getColor(Colors::LFO::Waveform::Ruler::Text));
        g.setFont(skin->fontManager->lfoTypeFont);

        std::string txt;
        float tv = delta * l;

        if (fabs(roundf(tv) - tv) < 0.05)
            txt = fmt::format("{:d} s", (int)round(delta * l));
        else if (delta < 0.1)
            txt = fmt::format("{:.2f} s", delta * l);
        else
            txt = fmt::format("{:.1f} s", delta * l);

        g.drawText(txt, tp.x, tp.y, 30, 10, juce::Justification::topLeft);

        auto sp = juce::Point<float>(xp, valScale * 0.95).transformedBy(at);
        auto ep = juce::Point<float>(xp, valScale * 0.9).transformedBy(at);

        g.setColour(skin->getColor(Colors::LFO::Waveform::Ruler::Ticks));
        g.drawLine(sp.x, sp.y, ep.x, ep.y, 1.0);
    }

    /*
     * In 1.8 I think we don't want to show the key release point in the simulated wave
     * with the MSEG but I wrote it to debug and we may change our mind so keeping this code
     * here
     */
    if (msegRelease && false)
    {
#if SHOW_RELEASE_TIMES
        float xp = msegReleaseAt / drawnTime * valScale;
        was a vstgui Point sp(xp, valScale * 0.9), ep(xp, valScale * 0.1);
        tf.transform(sp);
        tf.transform(ep);
        dc->setFrameColor(juce::Colours::red);
        dc->drawLine(sp, ep);
#endif
    }

    if (warnForInvalid)
    {
        g.setColour(skin->getColor(Colors::LFO::Waveform::Wave));
        g.setFont(skin->fontManager->getLatoAtSize(14, juce::Font::bold));
        g.drawText(invalidMessage, waveform_display.withTrimmedBottom(30),
                   juce::Justification::centred);
    }
}

void LFOAndStepDisplay::paintStepSeq(juce::Graphics &g)
{
    auto shadowcol = skin->getColor(Colors::LFO::StepSeq::ColumnShadow);
    auto stepMarker = skin->getColor(Colors::LFO::StepSeq::Step::Fill);
    auto disStepMarker = skin->getColor(Colors::LFO::StepSeq::Step::FillOutside);
    auto noLoopHi = skin->getColor(Colors::LFO::StepSeq::Loop::OutsidePrimaryStep);
    auto noLoopLo = skin->getColor(Colors::LFO::StepSeq::Loop::OutsideSecondaryStep);
    auto loopRegionHi = skin->getColor(Colors::LFO::StepSeq::Loop::PrimaryStep);
    auto loopRegionLo = skin->getColor(Colors::LFO::StepSeq::Loop::SecondaryStep);
    auto grabMarker = skin->getColor(Colors::LFO::StepSeq::Loop::Marker);
    auto grabMarkerHi = skin->getColor(Colors::LFO::StepSeq::TriggerClick);

    auto fillr = [&g](juce::Rectangle<float> r, juce::Colour c) {
        g.setColour(c);
        g.fillRect(r);
    };

    // set my coordinate system so that 0,0 is at splitpoint moved over with room for jogs
    auto shiftTranslate =
        juce::AffineTransform().translated(main_panel.getTopLeft()).translated(19, 0);

    {
        juce::Graphics::ScopedSaveState gs(g);
        g.addTransform(shiftTranslate);

        for (int i = 0; i < n_stepseqsteps; i++)
        {
            juce::Rectangle<float> rstep(1.f * i * scale, 1.f * margin, scale,
                                         main_panel.getHeight() - margin2),
                gstep;

            // Draw an outline in the shadow color
            fillr(rstep, shadowcol);

            // Now draw the actual step
            int sp = std::min(ss->loop_start, ss->loop_end);
            int ep = std::max(ss->loop_start, ss->loop_end);

            juce::Colour stepcolor, valuecolor, knobcolor;

            if ((i >= sp) && (i <= ep))
            {
                stepcolor = (i & 3) ? loopRegionHi : loopRegionLo;
                knobcolor = stepMarker;
                valuecolor = stepMarker;
            }
            else
            {
                stepcolor = (i & 3) ? noLoopHi : noLoopLo;
                knobcolor = disStepMarker;
                valuecolor = disStepMarker;
            }

            // Code to put in place an editor for the envelope retrigger
            if (edit_trigmask)
            {
                auto nstep = rstep.withTrimmedTop(margin2);
                gstep = rstep.withBottom(nstep.getY());
                rstep = nstep;
                gaterect[i] = gstep;
                auto gatecolor = knobcolor;
                auto gatebgcolor = stepcolor;

                uint64_t maski = ss->trigmask & (UINT64_C(1) << i);
                uint64_t maski16 = ss->trigmask & (UINT64_C(1) << (i + n_stepseqsteps));
                uint64_t maski32 = ss->trigmask & (UINT64_C(1) << (i + (n_stepseqsteps * 2)));

                gstep = gstep.withTrimmedTop(1).withTrimmedLeft(1).withTrimmedRight(
                    i == n_stepseqsteps - 1 ? 1 : 0);

                if (maski)
                {
                    fillr(gstep, gatecolor);
                }
                else if (maski16)
                {
                    // FIXME - an A or an F would be nice eh?
                    fillr(gstep, gatebgcolor);
                    auto qrect = gstep.withTrimmedRight(gstep.getWidth() / 2);
                    fillr(qrect, gatecolor);
                }
                else if (maski32)
                {
                    fillr(gstep, gatebgcolor);
                    auto qrect = gstep.withTrimmedLeft(gstep.getWidth() / 2);
                    fillr(qrect, gatecolor);
                }
                else
                {
                    fillr(gstep, gatebgcolor);
                }
            }

            fillr(rstep.withTrimmedLeft(1).reduced(0, 1).withTrimmedRight(
                      i == n_stepseqsteps - 1 ? 1 : 0),
                  stepcolor);

            steprect[i] = rstep;

            // draw the midpoint line
            if (!isUnipolar())
            {
                auto cy = rstep.toFloat().getCentreY() - 0.5;
                auto fr = rstep.toFloat().withTop(cy).withHeight(1.f);
                fillr(fr, shadowcol);
            }

            // Now draw the value
            auto v = rstep;
            int p1, p2;

            if (isUnipolar())
            {
                auto sv = std::max(ss->steps[i], 0.f);
                v = v.withTrimmedTop((int)(v.getHeight() * (1 - sv))).withTrimmedRight(1);
            }
            else
            {
                p1 = v.getBottom() -
                     (int)((float)0.5f + v.getHeight() * (0.50f + 0.5f * ss->steps[i]));
                p2 = v.getCentreY();

                auto ttop = std::min(p1, p2);
                auto tbottom = std::max(p1, p2);

                v = v.withTop(ttop).withBottom(tbottom).withTrimmedRight(1).translated(1, 0);
            }

            if (lfodata->rate.deactivated &&
                (int)(lfodata->start_phase.val.f * n_stepseqsteps) % n_stepseqsteps == i)
            {
                auto scolor = juce::Colour(std::min(255, (int)(valuecolor.getRed() * 1.3)),
                                           std::min(255, (int)(valuecolor.getGreen() * 1.3)),
                                           std::min(255, (int)(valuecolor.getBlue() * 1.3)));
                valuecolor = skin->getColor(Colors::LFO::StepSeq::Step::FillDeactivated);
            }

            fillr(v, valuecolor);
        }

        loopStartRect = steprect[0]
                            .withTrimmedBottom(-margin2 + 1)
                            .withTop(steprect[0].getBottom())
                            .withWidth(margin2);
        loopEndRect = loopStartRect.translated(-margin2 + 1, 0);

        loopStartRect = loopStartRect.translated(scale * ss->loop_start - 1, 0);
        loopEndRect = loopEndRect.translated(scale * (ss->loop_end + 1) - 1, 0);

        fillr(loopStartRect, grabMarker);
        fillr(loopEndRect, grabMarker);

        // continue drawing loop marker vertical lines using the same grabMarker color
        g.drawLine(loopStartRect.getX() + 1, steprect[0].getY(), loopStartRect.getX() + 1,
                   loopStartRect.getY());
        g.drawLine(loopEndRect.getRight(), steprect[0].getY(), loopEndRect.getRight(),
                   loopEndRect.getY());
    }

    // These data structures are used for mouse hit detection so have to translate them back to
    // screen
    for (auto i = 0; i < n_stepseqsteps; ++i)
    {
        steprect[i] = steprect[i].transformedBy(shiftTranslate);
        gaterect[i] = gaterect[i].transformedBy(shiftTranslate);
    }

    loopEndRect = loopEndRect.transformedBy(shiftTranslate);
    loopStartRect = loopStartRect.transformedBy(shiftTranslate);

    rect_steps = steprect[0].withRight(steprect[n_stepseqsteps - 1].getRight());
    rect_steps_retrig = gaterect[0].withRight(gaterect[n_stepseqsteps - 1].getRight());

    // step seq shift background frame
    g.setColour(skin->getColor(Colors::LFO::StepSeq::Button::Border));
    g.fillRect(ss_shift_bg);

    // step seq shift left
    if (stepSeqShiftHover == 0)
        g.setColour(skin->getColor(Colors::LFO::StepSeq::Button::Hover));
    else
        g.setColour(skin->getColor(Colors::LFO::StepSeq::Button::Background));

    g.fillRect(ss_shift_left);

    float triO = 2;
    auto triL = juce::Path();
    auto triR = juce::Path();

    triL.addTriangle(ss_shift_left.getTopRight().translated(-triO, triO),
                     ss_shift_left.getBottomRight().translated(-triO, -triO),
                     ss_shift_left.getCentre().withX(ss_shift_left.getX() + triO));

    g.setColour(skin->getColor(Colors::LFO::StepSeq::Button::Arrow));

    if (stepSeqShiftHover == 0 && skin->getVersion() >= 2)
    {
        g.setColour(skin->getColor(Colors::LFO::StepSeq::Button::ArrowHover));
    }

    g.fillPath(triL);

    // step seq shift right
    if (stepSeqShiftHover == 1)
        g.setColour(skin->getColor(Colors::LFO::StepSeq::Button::Hover));
    else
        g.setColour(skin->getColor(Colors::LFO::StepSeq::Button::Background));

    g.fillRect(ss_shift_right);

    triR.addTriangle(
        ss_shift_right.getTopLeft().translated(triO, triO),
        ss_shift_right.getBottomLeft().translated(triO, -triO),
        ss_shift_right.getCentre().withX(ss_shift_right.getX() + ss_shift_right.getWidth() - triO));

    g.setColour(skin->getColor(Colors::LFO::StepSeq::Button::Arrow));

    if (stepSeqShiftHover == 1 && skin->getVersion() >= 2)
    {
        g.setColour(skin->getColor(Colors::LFO::StepSeq::Button::ArrowHover));
    }

    g.fillPath(triR);

    // OK now we have everything drawn we want to draw the step seq. This is similar to the LFO
    // code above but with very different scaling in time since we need to match the steps no
    // matter the rate

    pdata tp[n_scene_params];
    tp[lfodata->delay.param_id_in_scene].i = lfodata->delay.val.i;
    tp[lfodata->attack.param_id_in_scene].i = lfodata->attack.val.i;
    tp[lfodata->hold.param_id_in_scene].i = lfodata->hold.val.i;
    tp[lfodata->decay.param_id_in_scene].i = lfodata->decay.val.i;
    tp[lfodata->sustain.param_id_in_scene].i = lfodata->sustain.val.i;
    tp[lfodata->release.param_id_in_scene].i = lfodata->release.val.i;

    tp[lfodata->magnitude.param_id_in_scene].i = lfodata->magnitude.val.i;

    // Min out the rate. Be careful with temposync.
    float floorrate = -1.2; // can't be const - we mod it
    float displayRate = lfodata->rate.val.f;
    const float twotofloor = powf(2.0, -1.2); // so copy value here

    if (lfodata->rate.temposync)
    {
        /*
         * So frequency = temposyncration * 2^rate
         * We want floor of frequency to be 2^-3.5 (that's the check below)
         * So 2^rate = temposyncratioinb 2^-3.5;
         * rate = log2( 2^-3.5 * tsratioinb )
         */
        floorrate = std::max(floorrate, log2(twotofloor * storage->temposyncratio_inv));
    }

    if (lfodata->rate.val.f < floorrate)
    {
        tp[lfodata->rate.param_id_in_scene].f = floorrate;
        displayRate = floorrate;
    }
    else
    {
        tp[lfodata->rate.param_id_in_scene].i = lfodata->rate.val.i;
    }

    tp[lfodata->shape.param_id_in_scene].i = lfodata->shape.val.i;
    tp[lfodata->start_phase.param_id_in_scene].i = lfodata->start_phase.val.i;
    tp[lfodata->deform.param_id_in_scene].i = lfodata->deform.val.i;
    tp[lfodata->trigmode.param_id_in_scene].i = lm_keytrigger;

    /*
    ** First big difference - the total env time is basically 16 * the time of the rate
    */
    float freq = pow(2.0, displayRate); // frequency in hz
    if (lfodata->rate.temposync)
    {
        freq *= storage->temposyncratio;
    }

    float cyclesec = 1.0 / freq;
    float totalSampleTime = cyclesec * n_stepseqsteps;
    float susTime = 4.0 * cyclesec;

    LFOModulationSource *tlfo = new LFOModulationSource();
    tlfo->assign(storage, lfodata, tp, 0, ss, ms, fs, true);
    populateLFOMS(tlfo);
    tlfo->attack();
    auto boxo = rect_steps;

    int minSamples = (1 << 4) * boxo.getWidth();
    int totalSamples =
        std::max((int)minSamples, (int)(totalSampleTime * storage->samplerate / BLOCK_SIZE));
    float cycleSamples = cyclesec * storage->samplerate / BLOCK_SIZE;

    // OK so lets assume we want about 1000 pixels worth tops in
    int averagingWindow = (int)(totalSamples / 2000.0) + 1;

    float valScale = 100.0;
    int susCountdown = -1;

    juce::Path path, eupath, edpath;

    for (int i = 0; i < totalSamples; i += averagingWindow)
    {
        float val = 0;
        float eval = 0;
        float minval = 1000000;
        float maxval = -1000000;
        float firstval;
        float lastval;

        for (int s = 0; s < averagingWindow; s++)
        {
            tlfo->process_block();

            if (susCountdown < 0 && tlfo->env_state == lfoeg_stuck)
            {
                susCountdown = susTime * storage->samplerate / BLOCK_SIZE;
            }
            else if (susCountdown == 0 && tlfo->env_state == lfoeg_stuck)
            {
                tlfo->release();
            }
            else if (susCountdown > 0)
            {
                susCountdown--;
            }

            val += tlfo->get_output(0);

            if (s == 0)
            {
                firstval = tlfo->get_output(0);
            }

            if (s == averagingWindow - 1)
            {
                lastval = tlfo->get_output(0);
            }

            minval = std::min(tlfo->get_output(0), minval);
            maxval = std::max(tlfo->get_output(0), maxval);
            eval += tlfo->env_val * lfodata->magnitude.get_extended(lfodata->magnitude.val.f);
        }

        val = val / averagingWindow;
        eval = eval / averagingWindow;

        if (isUnipolar())
        {
            val = val * 2.0 - 1.0;
        }

        val = ((-val + 1.0f) * 0.5) * valScale;

        float euval = ((-eval + 1.0f) * 0.5) * valScale;
        float edval = ((eval + 1.0f) * 0.5) * valScale;
        float xc = valScale * i / (cycleSamples * n_stepseqsteps);

        if (i == 0)
        {
            path.startNewSubPath(xc, val);
            eupath.startNewSubPath(xc, euval);

            if (!isUnipolar())
            {
                edpath.startNewSubPath(xc, edval);
            }
        }
        else
        {
            path.lineTo(xc, val);
            eupath.lineTo(xc, euval);

            if (!isUnipolar())
            {
                edpath.lineTo(xc, edval);
            }
        }
    }

    delete tlfo;

    auto q = boxo;

    const auto tfpath = juce::AffineTransform()
                            .scaled(boxo.getWidth() / valScale, boxo.getHeight() / valScale)
                            .translated(q.getTopLeft().x, q.getTopLeft().y);

    g.setColour(skin->getColor(Colors::LFO::StepSeq::Envelope));

    g.strokePath(eupath, juce::PathStrokeType(1.0), tfpath);

    if (!isUnipolar())
    {
        g.strokePath(edpath, juce::PathStrokeType(1.0), tfpath);
    }

    g.setColour(skin->getColor(Colors::LFO::StepSeq::Wave));
    g.strokePath(path, juce::PathStrokeType(1.0), tfpath);

    // draw the RMB drag line
    if (dragMode == ARROW)
    {
        auto l = juce::Line<float>{arrowStart, arrowEnd};

        Surge::GUI::constrainPointOnLineWithinRectangle(rect_steps, l, arrowEnd);

        l = juce::Line<float>{arrowStart, arrowEnd};

        auto ph = juce::Path();
        ph.addArrow(l, 1.5, 4, 6);
        g.setColour(skin->getColor(Colors::LFO::StepSeq::DragLine));
        g.fillPath(ph);
        auto eo = arrowStart.translated(-1, -1);
        g.fillEllipse(eo.x, eo.y, 3, 3);
    }

    // Finally draw the drag label
    if (dragMode == VALUES && draggedStep >= 0 && draggedStep < n_stepseqsteps)
    {
        const int displayPrecision = Surge::Storage::getValueDisplayPrecision(storage);

        g.setFont(skin->fontManager->lfoTypeFont);

        std::string txt =
            fmt::format("{:.{}f} %", ss->steps[draggedStep] * 100.f, displayPrecision);

        int sw = SST_STRING_WIDTH_INT(g.getCurrentFont(), txt);

        auto sr = steprect[draggedStep];

        float dragX = sr.getRight(), dragY;
        float dragW = 6 + sw, dragH = (keyModMult ? 22 : 12);

        // Draw to the right in the second half of the seq table
        if (draggedStep < n_stepseqsteps / 2)
        {
            dragX = sr.getRight();
        }
        else
        {
            dragX = sr.getX() - dragW;
        }

        float yTop;

        if (lfodata->unipolar.val.b)
        {
            auto sv = std::max(ss->steps[draggedStep], 0.f);
            yTop = sr.getBottom() - (sr.getHeight() * sv);
        }
        else
        {
            yTop = sr.getBottom() -
                   ((float)0.5f + sr.getHeight() * (0.5f + 0.5f * ss->steps[draggedStep]));
        }

        if (yTop > sr.getHeight() / 2)
        {
            dragY = yTop - dragH;
        }
        else
        {
            dragY = yTop;
        }

        if (dragY < 2)
        {
            dragY = 2;
        }

        auto labelR = juce::Rectangle<float>(dragX, dragY, dragW, dragH);

        fillr(labelR, skin->getColor(Colors::LFO::StepSeq::InfoWindow::Border));
        labelR = labelR.reduced(1, 1);
        fillr(labelR, skin->getColor(Colors::LFO::StepSeq::InfoWindow::Background));

        labelR = labelR.withTrimmedLeft(2).withHeight(10);

        g.setColour(skin->getColor(Colors::LFO::StepSeq::InfoWindow::Text));
        g.drawText(txt, labelR, juce::Justification::centredLeft);

        if (keyModMult > 0)
        {
            labelR = labelR.translated(0, 10);
            txt = fmt::format("{:d}/{:d}", (int)(floor(ss->steps[draggedStep] * keyModMult + 0.5)),
                              keyModMult);
            g.drawText(txt, labelR, juce::Justification::centredLeft);
        }
    }
}

void LFOAndStepDisplay::paintTypeSelector(juce::Graphics &g)
{
    if (!typeImg)
    {
        g.setColour(juce::Colours::crimson);
        g.fillRect(left_panel);
        return;
    }

    auto leftpanel = left_panel.translated(0, margin + 2);
    auto type = lfodata->shape.val.i;
    auto off = lfodata->shape.val.i * 76;
    {
        juce::Graphics::ScopedSaveState gs(g);
        auto at = juce::AffineTransform()
                      .translated(leftpanel.getX(), leftpanel.getY())
                      .translated(0, -off);
        g.reduceClipRegion(leftpanel.getX(), leftpanel.getY(), 51, 76);
        typeImg->draw(g, 1.0, at);
    }

    for (int i = 0; i < n_lfo_types; ++i)
    {
        int xp = (i % 2) * 25 + leftpanel.getX();
        int yp = (i / 2) * 15 + leftpanel.getY();
        shaperect[i] = juce::Rectangle<int>(xp, yp, 25, 15);
    }

    if (lfoTypeHover >= 0)
    {
        auto off = lfoTypeHover * 76;
        auto dt = typeImgHover;

        if (lfoTypeHover == type)
        {
            dt = typeImgHoverOn;
        }

        if (dt)
        {
            juce::Graphics::ScopedSaveState gs(g);
            auto at = juce::AffineTransform()
                          .translated(leftpanel.getX(), leftpanel.getY())
                          .translated(0, -off);

            g.reduceClipRegion(leftpanel.getX(), leftpanel.getY(), 51, 76);

            dt->draw(g, 1.0, at);
        }
    }
}

void LFOAndStepDisplay::setStepToDefault(const juce::MouseEvent &event)
{
    for (int i = 0; i < n_stepseqsteps; ++i)
    {
        draggedStep = -1;

        if (event.mouseWasDraggedSinceMouseDown())
        {
            auto r = steprect[i];

            float rx0 = r.getX();
            float rx1 = r.getX() + r.getWidth();
            float ry0 = r.getY();
            float ry1 = r.getY() + r.getHeight();

            if (event.position.x >= rx0 && event.position.x < rx1)
            {
                draggedStep = i;
            }
        }

        if (draggedStep >= 0 || (event.mouseWasClicked() && steprect[i].contains(event.position)))
        {
            auto bscg = BeginStepGuard(this);

            if (draggedStep == -1) // mouse down only case
            {
                ss->steps[i] = 0.f;
            }
            else // we are dragging
            {
                int startStep = draggedStep;

                for (int i = 0; i < n_stepseqsteps; ++i)
                {
                    if (steprect[i].contains(event.mouseDownPosition))
                    {
                        startStep = i;
                        break;
                    }
                }

                if (startStep > draggedStep)
                {
                    std::swap(startStep, draggedStep);
                }

                for (int i = startStep; i <= draggedStep; i++)
                {
                    ss->steps[i] = 0.f;
                }
            }

            stepSeqDirty();
            repaint();
        }
    }
}

void LFOAndStepDisplay::setStepValue(const juce::MouseEvent &event)
{
    keyModMult = 0;

    int quantStep = 12;

    if (!storage->isStandardTuning && storage->currentScale.count > 1)
    {
        quantStep = storage->currentScale.count;
    }

    draggedStep = -1;

    bool yPosActivity = false;

    for (int i = 0; i < n_stepseqsteps; ++i)
    {
        auto r = steprect[i];

        float rx0 = r.getX();
        float rx1 = r.getX() + r.getWidth();
        float ry0 = r.getY();
        float ry1 = r.getY() + r.getHeight();

        if ((event.position.x >= rx0 && event.position.x < rx1) || yPosActivity)
        {
            draggedStep = i;
        }

        if (event.position.y >= ry0 && event.position.y < ry1)
        {
            if (event.position.x < steprect[0].getX())
            {
                draggedStep = 0;
                yPosActivity = true;
            }

            if (event.position.x >= steprect[n_stepseqsteps - 1].getX())
            {
                draggedStep = n_stepseqsteps - 1;
                yPosActivity = true;
            }
        }

        if (draggedStep >= 0 || yPosActivity)
        {
            float f;
            auto bscg = BeginStepGuard(this);

            if (isUnipolar())
            {
                f = limit01((r.getBottom() - event.position.y) / r.getHeight());
            }
            else
            {
                f = limitpm1(2 * (r.getCentreY() - event.position.y) / r.getHeight());
            }

            if (event.mods.isShiftDown())
            {
                keyModMult = quantStep; // only shift pressed

                if (event.mods.isAltDown())
                {
                    keyModMult = quantStep * 2; // shift+alt pressed

                    f *= quantStep * 2;
                    f = floor(f + 0.5);
                    f *= (1.f / (quantStep * 2));
                }
                else
                {
                    f *= quantStep;
                    f = floor(f + 0.5);
                    f *= (1.f / quantStep);
                }
            }

            ss->steps[draggedStep] = f;
            stepSeqDirty();

            repaint();
        }
    }
}

void LFOAndStepDisplay::onSkinChanged()
{
    if (!(skin && skinControl && associatedBitmapStore))
    {
        return;
    }
    forceRepaint = true;
    auto typesWithHover = skin->standardHoverAndHoverOnForIDB(IDB_LFO_TYPE, associatedBitmapStore);
    typeImg = typesWithHover[0];
    typeImgHover = typesWithHover[1];
    typeImgHoverOn = typesWithHover[2];
}

void LFOAndStepDisplay::mouseDown(const juce::MouseEvent &event)
{
    if (forwardedMainFrameMouseDowns(event))
    {
        return;
    }

    auto sge = firstListenerOfType<SurgeGUIEditor>();

    for (int i = 0; i < n_lfo_types; ++i)
    {
        if (shaperect[i].contains(event.position.toInt()))
        {
            if (event.mods.isPopupMenu())
            {
                notifyControlModifierClicked(event.mods);

                return;
            }

            updateShapeTo(i);

            return;
        }
    }

    mouseDownLongHold(event);

    if (waveform_display.contains(event.position.toInt()) && sge)
    {
        if (isMSEG() || isFormula())
        {
            auto tag = isMSEG() ? SurgeGUIEditor::MSEG_EDITOR : SurgeGUIEditor::FORMULA_EDITOR;

            if (event.mods.isPopupMenu())
            {
                showLFODisplayPopupMenu(tag);
            }
            else
            {
                sge->toggleOverlay(tag);
            }
        }
    }

    if (isStepSequencer())
    {
        dragMode = NONE;

        if (event.mods.isCommandDown())
        {
            setStepToDefault(event);
            dragMode = RESET_VALUE;

            return;
        }

        if (ss_shift_left.contains(event.position))
        {
            shiftLeft();
            return;
        }

        if (ss_shift_right.contains(event.position))
        {
            shiftRight();
            return;
        }

        if (loopStartRect.contains(event.position))
        {
            dragMode = LOOP_START;
            return;
        }

        if (loopEndRect.contains(event.position))
        {
            dragMode = LOOP_END;
            return;
        }

        for (auto r : steprect)
        {
            if (r.contains(event.position))
            {
                if (event.mods.isRightButtonDown())
                {
                    dragMode = ARROW;
                    arrowStart = event.position;
                    arrowEnd = event.position;
                    juce::Timer::callAfterDelay(
                        1000, [w = juce::Component::SafePointer(this), event] {
                            if (w && w->dragMode == ARROW &&
                                w->arrowStart.getDistanceSquaredFrom(w->arrowEnd) < 2)
                                w->showStepRMB(event);
                        });
                }
                else
                {
                    setStepValue(event);
                    dragMode = VALUES;
                }
            }
        }

        for (int i = 0; i < n_stepseqsteps; ++i)
        {
            auto r = gaterect[i];

            if (r.contains(event.position))
            {
                auto bscg = BeginStepGuard(this);
                dragMode = TRIGGERS;

                uint64_t maski = ss->trigmask & (UINT64_C(1) << i);
                uint64_t maski16 = ss->trigmask & (UINT64_C(1) << (i + n_stepseqsteps));
                uint64_t maski32 = ss->trigmask & (UINT64_C(1) << (i + (n_stepseqsteps * 2)));
                uint64_t maskOn = 0;
                uint64_t maskOff = 0xFFFFFFFFFFFFFFFF;

                if (!event.mods.isAnyModifierKeyDown())
                {
                    if (maski || maski16 || maski32)
                    {
                        maskOn = 0;
                        maskOff = ~maski & ~maski16 & ~maski32;
                        dragTrigger0 = 0;
                    }
                    else
                    {
                        maskOn = (UINT64_C(1) << (i + 0));
                        maskOff = ss->trigmask;
                        dragTrigger0 = UINT64_C(1);
                    }
                }

                if (event.mods.isRightButtonDown() || event.mods.isShiftDown())
                {
                    if (maski || ~maski)
                    {
                        maskOn = (UINT64_C(1) << (i + n_stepseqsteps));
                        maskOff = ~maski;
                        dragTrigger0 = UINT64_C(1) << n_stepseqsteps;
                    }

                    if (maski16)
                    {
                        maskOn = (UINT64_C(1) << (i + (n_stepseqsteps * 2)));
                        maskOff = ~maski16;
                        dragTrigger0 = UINT64_C(1) << (n_stepseqsteps * 2);
                    }
                }

                ss->trigmask &= maskOff;
                ss->trigmask |= maskOn;

                stepSeqDirty();
                repaint();

                return;
            }
        }
    }
}

void LFOAndStepDisplay::shiftLeft()
{
    auto bscg = BeginStepGuard(this);
    float t = ss->steps[0];

    for (int i = 0; i < (n_stepseqsteps - 1); i++)
    {
        ss->steps[i] = ss->steps[i + 1];
        assert((i >= 0) && (i < n_stepseqsteps));
    }

    ss->steps[n_stepseqsteps - 1] = t;
    ss->trigmask =
        (((ss->trigmask & 0x000000000000fffe) >> 1) | (((ss->trigmask & 1) << 15) & 0xffff)) |
        (((ss->trigmask & 0x00000000fffe0000) >> 1) |
         (((ss->trigmask & 0x10000) << 15) & 0xffff0000)) |
        (((ss->trigmask & 0x0000fffe00000000) >> 1) |
         (((ss->trigmask & 0x100000000) << 15) & 0xffff00000000));

    stepSeqDirty();
    repaint();
}

void LFOAndStepDisplay::shiftRight()
{
    auto bscg = BeginStepGuard(this);
    float t = ss->steps[n_stepseqsteps - 1];

    for (int i = (n_stepseqsteps - 2); i >= 0; i--)
    {
        ss->steps[i + 1] = ss->steps[i];
        assert((i >= 0) && (i < n_stepseqsteps));
    }

    ss->steps[0] = t;
    ss->trigmask = (((ss->trigmask & 0x0000000000007fff) << 1) |
                    (((ss->trigmask & 0x0000000000008000) >> 15) & 0xffff)) |
                   (((ss->trigmask & 0x000000007fff0000) << 1) |
                    (((ss->trigmask & 0x0000000080000000) >> 15) & 0xffff0000)) |
                   (((ss->trigmask & 0x00007fff00000000) << 1) |
                    (((ss->trigmask & 0x0000800000000000) >> 15) & 0xffff00000000));

    stepSeqDirty();
    repaint();
}

void LFOAndStepDisplay::mouseExit(const juce::MouseEvent &event)
{
    if (overWaveform)
    {
        enterExitWaveform(false);
    }

    lfoTypeHover = -1;
    overWaveform = -1;
    stepSeqShiftHover = -1;

    endHover();
}

void LFOAndStepDisplay::enterExitWaveform(bool isInWF)
{
    if (isInWF)
    {
        if (lfodata->shape.val.i == lt_mseg || lfodata->shape.val.i == lt_formula)
        {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        }
    }
    else
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
}

void LFOAndStepDisplay::mouseMove(const juce::MouseEvent &event)
{
    mouseMoveLongHold(event);

    int nextHover = -1;

    for (int i = 0; i < n_lfo_types; ++i)
    {
        if (shaperect[i].contains(event.position.toInt()))
        {
            nextHover = i;
        }
    }

    if (nextHover != lfoTypeHover)
    {
        lfoTypeHover = nextHover;

        repaint();

        // We hover over the type so
        if (lfoTypeHover >= 0)
        {
            return;
        }
    }

    // reset for the step hover
    nextHover = -1;
    if (ss_shift_left.contains(event.position))
    {
        nextHover = 0;
    }

    if (ss_shift_right.contains(event.position))
    {
        nextHover = 1;
    }

    if (nextHover != stepSeqShiftHover)
    {
        stepSeqShiftHover = nextHover;

        repaint();

        // We hover over the stepseq jog so
        if (stepSeqShiftHover >= 0)
        {
            return;
        }
    }

    if (waveform_display.contains(event.position.toInt()))
    {
        if (!overWaveform)
            enterExitWaveform(true);
        overWaveform = true;
    }
    else
    {
        if (overWaveform)
            enterExitWaveform(false);
        overWaveform = false;
    }
}

void LFOAndStepDisplay::mouseDrag(const juce::MouseEvent &event)
{
    if (supressMainFrameMouseEvent(event))
    {
        return;
    }

    auto sge = firstListenerOfType<SurgeGUIEditor>();

    if (!isStepSequencer() || dragMode == NONE)
    {
        for (int i = 0; i < n_lfo_types; ++i)
        {
            if (shaperect[i].contains(event.position.toInt()))
            {
                if (event.mods.isPopupMenu())
                {
                    notifyControlModifierClicked(event.mods);

                    return;
                }

                if (i != lfodata->shape.val.i)
                {
                    auto prior = lfodata->shape.val.i;

                    lfodata->shape.val.i = i;

                    sge->refresh_mod();
                    sge->broadcastPluginAutomationChangeFor(&(lfodata->shape));

                    repaint();

                    sge->lfoShapeChanged(prior, i);
                }

                return;
            }
        }
    }

    if (!isStepSequencer())
    {
        return;
    }

    mouseDragLongHold(event);

    if (dragMode != NONE && event.getDistanceFromDragStart() > 0)
    {
        if (!Surge::GUI::showCursor(storage))
        {
            juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(true);
        }
    }

    switch (dragMode)
    {
    case NONE:
        break;
    case ARROW:
    {
        arrowEnd = event.position;
        repaint();
        break;
    }
    case LOOP_START:
    case LOOP_END:
    {
        int sign = dragMode == LOOP_START ? 1 : -1;
        int loopStart = -1;

        for (int i = 0; i < n_stepseqsteps; ++i)
        {
            auto r = steprect[i];

            if (r.contains(event.position.x + sign * r.getWidth() * 0.5, r.getCentreY()))
            {
                loopStart = i;
            }
        }

        if (dragMode == LOOP_START)
        {
            if (ss->loop_start != loopStart && loopStart >= 0)
            {
                auto bscg = BeginStepGuard(this);
                ss->loop_start = loopStart;
                stepSeqDirty();
                repaint();
            }
        }
        else
        {
            if (ss->loop_end != loopStart && loopStart >= 0)
            {
                auto bscg = BeginStepGuard(this);
                ss->loop_end = loopStart;
                stepSeqDirty();
                repaint();
            }
        }
        break;
    }
    case RESET_VALUE:
    {
        if (event.mods.isCommandDown())
        {
            setStepToDefault(event);
            return;
        }
        else
        {
            dragMode = VALUES;
            setStepValue(event);
            return;
        }
        break;
    }
    case TRIGGERS:
    {
        for (int i = 0; i < n_stepseqsteps; ++i)
        {
            auto r = gaterect[i];
            auto otm = ss->trigmask;

            if (event.position.x >= r.getX() && event.position.x < r.getX() + r.getWidth())
            {
                auto off = UINT64_MAX & ~(UINT64_C(1) << i) &
                           ~(UINT64_C(1) << (i + n_stepseqsteps)) &
                           ~(UINT64_C(1) << (i + (n_stepseqsteps * 2)));
                auto on = dragTrigger0 << i;

                ss->trigmask &= off;
                ss->trigmask |= on;
            }

            if (ss->trigmask != otm)
            {
                repaint();
            }
        }
        break;
    }
    case VALUES:
    {
        if (event.mods.isCommandDown())
        {
            dragMode = RESET_VALUE;
            setStepToDefault(event);
        }
        else
        {
            setStepValue(event);
        }
        break;
    }
    }
}

void LFOAndStepDisplay::mouseUp(const juce::MouseEvent &event)
{
    mouseUpLongHold(event);

    if (event.mouseWasDraggedSinceMouseDown())
    {
        if (!Surge::GUI::showCursor(storage))
        {
            juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(false);
        }
    }

    if (dragMode == ARROW && (!event.mouseWasDraggedSinceMouseDown() ||
                              (arrowStart.getDistanceSquaredFrom(arrowEnd) < 2)))
    {
        showStepRMB(event);
    }
    else if (dragMode == ARROW)
    {
        auto l = juce::Line<float>{arrowStart, arrowEnd};

        Surge::GUI::constrainPointOnLineWithinRectangle(rect_steps, l, arrowEnd);

        auto rmStepStart = arrowStart;
        auto rmStepCurr = arrowEnd;

        int startStep = -1;
        int endStep = -1;

        // find out if a microtuning is loaded and store the scale length for Alt-drag
        // quantization to scale degrees
        keyModMult = 0;

        int quantStep = 12;

        if (!storage->isStandardTuning && storage->currentScale.count > 1)
        {
            quantStep = storage->currentScale.count;
        }

        for (int i = 0; i < n_stepseqsteps; ++i)
        {
            if (steprect[i].contains(rmStepStart))
            {
                startStep = i;
            }

            // we expand steprect so that we accept edge points on the rectangle
            // since .contains() only checks if a point is within the rect, not ON its edges
            if (steprect[i].expanded(1, 1).contains(rmStepCurr))
            {
                endStep = i;
            }
        };

        int s = startStep, e = endStep;

        if (s >= 0 && e >= 0 && s != e) // s == e is the abort gesture
        {
            auto bscg = BeginStepGuard(this);
            float fs = (float)(steprect[s].getBottom() - rmStepStart.y) / steprect[s].getHeight();
            float fe = (float)(steprect[e].getBottom() - rmStepCurr.y) / steprect[e].getHeight();

            if (e < s)
            {
                std::swap(e, s);
                std::swap(fe, fs);
            }

            if (lfodata->unipolar.val.b)
            {
                fs = limit_range(fs, 0.f, 1.f);
                fe = limit_range(fe, 0.f, 1.f);
            }
            else
            {
                fs = limit_range(fs * 2.f - 1.f, -1.f, 1.f);
                fe = limit_range(fe * 2.f - 1.f, -1.f, 1.f);
            }

            ss->steps[s] = fs;

            if (s != e)
            {
                float ds = (fs - fe) / (s - e);

                for (int q = s; q <= e; q++)
                {
                    float f = ss->steps[s] + (q - s) * ds;

                    if (event.mods.isShiftDown())
                    {
                        keyModMult = quantStep; // only shift pressed

                        if (event.mods.isAltDown())
                        {
                            keyModMult = quantStep * 2; // shift+alt pressed

                            f *= quantStep * 2;
                            f = floor(f + 0.5);
                            f *= (1.f / (quantStep * 2));
                        }
                        else
                        {
                            f *= quantStep;
                            f = floor(f + 0.5);
                            f *= (1.f / quantStep);
                        }
                    }

                    ss->steps[q] = f;
                }
            }

            stepSeqDirty();
            repaint();
        }
    }

    dragMode = NONE;
    draggedStep = -1;

    repaint();
}

void LFOAndStepDisplay::mouseDoubleClick(const juce::MouseEvent &event)
{
    if (isStepSequencer())
    {
        setStepToDefault(event);
    }
}

void LFOAndStepDisplay::mouseWheelMove(const juce::MouseEvent &event,
                                       const juce::MouseWheelDetails &wheel)
{
    if (!isStepSequencer())
    {
        return;
    }

    auto bscg = BeginStepGuard(this);

    float delta = wheel.deltaX - (wheel.isReversed ? 1 : -1) * wheel.deltaY;
#if MAC
    delta *= 1.3;
#endif

    if (event.mods.isShiftDown())
    {
        delta *= 0.1;
    }

    if (delta == 0)
    {
        return;
    }

    for (int i = 0; i < n_stepseqsteps; ++i)
    {
        if (steprect[i].contains(event.position))
        {
            auto v = ss->steps[i] + delta;

            if (isUnipolar())
            {
                v = limit01(v);
            }
            else
            {
                v = limitpm1(v);
            }

            ss->steps[i] = v;
            stepSeqDirty();
            repaint();
        }
    }
}

void LFOAndStepDisplay::showLFODisplayPopupMenu(SurgeGUIEditor::OverlayTags tag)
{
    auto contextMenu = juce::PopupMenu();

    std::string olname = isMSEG() ? "MSEG Editor" : "Formula Editor";
    std::string helpname = isMSEG() ? "mseg-editor" : "formula-editor";

    auto msurl = storage ? SurgeGUIEditor::helpURLForSpecial(storage, helpname) : std::string();
    auto hurl = SurgeGUIEditor::fullyResolvedHelpURL(msurl);

    auto hmen = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>(olname, hurl);
    hmen->setSkin(skin, associatedBitmapStore);
    auto hment = hmen->getTitle();

    contextMenu.addCustomItem(-1, std::move(hmen), nullptr, hment);

    contextMenu.addSeparator();

    auto sge = firstListenerOfType<SurgeGUIEditor>();

    if (!sge)
    {
        return;
    }

    std::string openname = (sge->isAnyOverlayPresent(tag)) ? "Close " : "Open ";

    Surge::GUI::addMenuWithShortcut(contextMenu, Surge::GUI::toOSCase(openname + olname + "..."),
                                    sge->showShortcutDescription("Alt+E", "⌥E"),
                                    [this, sge, tag]() {
                                        if (sge)
                                        {
                                            sge->toggleOverlay(tag);
                                        }
                                    });

    if (isMSEG())
    {
        contextMenu.addSeparator();

        bool isChecked = ms->loopMode == MSEGStorage::ONESHOT;

        contextMenu.addItem(Surge::GUI::toOSCase("No Looping"), true, isChecked,
                            [this, isChecked, sge]() {
                                if (isChecked)
                                {
                                    storage->getPatch().isDirty = true;
                                }

                                ms->loopMode = MSEGStorage::LoopMode::ONESHOT;

                                if (sge && sge->isAnyOverlayPresent(SurgeGUIEditor::MSEG_EDITOR))
                                {
                                    sge->closeOverlay(SurgeGUIEditor::MSEG_EDITOR);
                                    sge->showOverlay(SurgeGUIEditor::MSEG_EDITOR);
                                }

                                repaint();
                            });

        isChecked = ms->loopMode == MSEGStorage::LOOP;

        contextMenu.addItem(Surge::GUI::toOSCase("Loop Always"), true, isChecked,
                            [this, isChecked, sge]() {
                                if (isChecked)
                                {
                                    storage->getPatch().isDirty = true;
                                }

                                ms->loopMode = MSEGStorage::LoopMode::LOOP;

                                if (sge && sge->isAnyOverlayPresent(SurgeGUIEditor::MSEG_EDITOR))
                                {
                                    sge->closeOverlay(SurgeGUIEditor::MSEG_EDITOR);
                                    sge->showOverlay(SurgeGUIEditor::MSEG_EDITOR);
                                }

                                repaint();
                            });

        isChecked = ms->loopMode == MSEGStorage::GATED_LOOP;

        contextMenu.addItem(Surge::GUI::toOSCase("Loop Until Release"), true, isChecked,
                            [this, isChecked, sge]() {
                                if (isChecked)
                                {
                                    storage->getPatch().isDirty = true;
                                }

                                ms->loopMode = MSEGStorage::LoopMode::GATED_LOOP;

                                if (sge && sge->isAnyOverlayPresent(SurgeGUIEditor::MSEG_EDITOR))
                                {
                                    sge->closeOverlay(SurgeGUIEditor::MSEG_EDITOR);
                                    sge->showOverlay(SurgeGUIEditor::MSEG_EDITOR);
                                }

                                repaint();
                            });
    }

    contextMenu.showMenuAsync(guiEditor->popupMenuOptions());
}

void LFOAndStepDisplay::populateLFOMS(LFOModulationSource *s)
{
    if (lfoid < n_lfos_voice)
        s->setIsVoice(true);
    else
        s->setIsVoice(false);

    if (s->isVoice)
        s->formulastate.velocity = 100;

    Surge::Formula::setupEvaluatorStateFrom(s->formulastate, storage->getPatch(),
                                            guiEditor->current_scene);
}

void LFOAndStepDisplay::updateShapeTo(int i)
{
    auto sge = firstListenerOfType<SurgeGUIEditor>();

    if (i != lfodata->shape.val.i)
    {
        auto prior = lfodata->shape.val.i;

        lfodata->shape.val.i = i;

        setupAccessibility();

        sge->refresh_mod();
        sge->broadcastPluginAutomationChangeFor(&(lfodata->shape));
        forceRepaint = true;
        repaint();

        sge->lfoShapeChanged(prior, i);
    }
}

void LFOAndStepDisplay::setupAccessibility()
{
    bool showStepSliders{false}, showTriggers{false};

    if (lfodata->shape.val.i == lt_stepseq)
    {
        if (lfoid < n_lfos_voice)
        {
            showTriggers = true;
        }

        showStepSliders = true;
    }

    stepLayer->setVisible(showStepSliders);

    for (int i = 0; i < n_lfo_types; ++i)
    {
        auto t = std::string(lt_names[i]);
        if (i == lfodata->shape.val.i)
            t += "  active";
        auto pt = typeAccOverlays[i]->getTitle();
        typeAccOverlays[i]->setTitle(t);
        typeAccOverlays[i]->setDescription(t);

        if (t != pt)
        {
            if (auto h = typeAccOverlays[i]->getAccessibilityHandler())
            {
                h->notifyAccessibilityEvent(juce::AccessibilityEvent::titleChanged);
            }
        }
    }

    for (const auto &s : stepSliderOverlays)
        if (s)
            s->setVisible(showStepSliders);

    for (const auto &s : loopEndOverlays)
        if (s)
            s->setVisible(showStepSliders);

    for (const auto &s : stepJogOverlays)
        if (s)
            s->setVisible(showStepSliders);

    for (const auto &s : stepTriggerOverlays)
        if (s)
            s->setVisible(showTriggers);
}
template <> struct DiscreteAHRange<LFOAndStepDisplay>
{
    static int iMaxV(LFOAndStepDisplay *t) { return n_lfo_types; }
    static int iMinV(LFOAndStepDisplay *t) { return 0; }
};

template <> struct DiscreteAHStringValue<LFOAndStepDisplay>
{
    static std::string stringValue(LFOAndStepDisplay *comp, double ahValue)
    {
        return lt_names[comp->lfodata->shape.val.i];
    }
};

template <> struct DiscreteRO<LFOAndStepDisplay>
{
    static bool isReadOnly(LFOAndStepDisplay *comp) { return true; }
};
std::unique_ptr<juce::AccessibilityHandler> LFOAndStepDisplay::createAccessibilityHandler()
{
    return std::make_unique<DiscreteAH<LFOAndStepDisplay, juce::AccessibilityRole::group>>(this);
}

bool LFOAndStepDisplay::keyPressed(const juce::KeyPress &key)
{
    auto [action, mod] = Surge::Widgets::accessibleEditAction(key, storage);

    if (action == None)
        return false;

    if (action == OpenMenu)
    {
        notifyControlModifierClicked(juce::ModifierKeys(), true);
        return true;
    }

    return false;
}

void LFOAndStepDisplay::prepareForEdit()
{
    jassert(stepDirtyCount == 0);
    stepDirtyCount++;
    undoStorageCopy = *ss;
}

void LFOAndStepDisplay::stepSeqDirty()
{
    jassert(stepDirtyCount == 1);
    storage->getPatch().isDirty = true;
    guiEditor->undoManager()->pushStepSequencer(scene, lfoid, undoStorageCopy);
}

void LFOAndStepDisplay::showStepRMB(const juce::MouseEvent &event)
{
    dragMode = NONE;

    for (int i = 0; i < n_stepseqsteps; ++i)
    {
        if (steprect[i].contains(event.position))
        {
            showStepRMB(i);
        }
    }
}

void LFOAndStepDisplay::showStepRMB(int i)
{
    auto contextMenu = juce::PopupMenu();

    std::string olname = "Step Sequencer";
    std::string helpname = "step-sequencer";

    auto msurl = storage ? SurgeGUIEditor::helpURLForSpecial(storage, helpname) : std::string();
    auto hurl = SurgeGUIEditor::fullyResolvedHelpURL(msurl);

    auto hmen = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>(olname, hurl);
    hmen->setSkin(skin, associatedBitmapStore);
    auto hment = hmen->getTitle();

    contextMenu.addCustomItem(-1, std::move(hmen), nullptr, hment);

    contextMenu.addSeparator();

    const int precision = Surge::Storage::getValueDisplayPrecision(storage);

    auto msg = fmt::format("Edit Step {} Value: {:.{}f} %", i + 1, ss->steps[i] * 100.f, precision);

    contextMenu.addItem(Surge::GUI::toOSCase(msg), true, false, [this, i]() { showStepTypein(i); });

    contextMenu.showMenuAsync(guiEditor->popupMenuOptions());
}

void LFOAndStepDisplay::showStepTypein(int i)
{
    const bool isDetailed = Surge::Storage::getValueDisplayIsHighPrecision(storage);
    const int precision = Surge::Storage::getValueDisplayPrecision(storage);

    auto handleTypein = [this, i](const std::string &s) {
        auto divPos = s.find('/');
        float v = 0.f;

        if (divPos != std::string::npos)
        {
            auto n = s.substr(0, divPos);
            auto d = s.substr(divPos + 1);
            auto nv = std::atof(n.c_str());
            auto dv = std::atof(d.c_str());

            if (dv == 0)
            {
                return false;
            }

            v = (nv / dv) * 100.f;
        }
        else
        {
            v = std::atof(s.c_str());
        }

        ss->steps[i] = std::clamp(v * 0.01f, -1.f, 1.f);

        repaint();

        return true;
    };

    if (!stepEditor)
    {
        stepEditor = std::make_unique<Surge::Overlays::TypeinLambdaEditor>(handleTypein);
    }

    if (getParentComponent()->getIndexOfChildComponent(stepEditor.get()) < 0)
    {
        getParentComponent()->addChildComponent(*stepEditor);
    }

    stepEditor->callback = handleTypein;
    stepEditor->setMainLabel(fmt::format("Edit Step {} Value", std::to_string(i + 1)));
    stepEditor->setValueLabels(fmt::format("current: {:.{}f} %", ss->steps[i] * 100.f, precision),
                               "");
    stepEditor->setSkin(skin, associatedBitmapStore);
    stepEditor->setEditableText(fmt::format("{:.{}f} %", ss->steps[i] * 100.f, precision));
    stepEditor->setReturnFocusTarget(stepSliderOverlays[i].get());

    auto topOfControl = getY();
    auto pb = getBounds();
    auto cx = steprect[i].getCentreX() + getX();

    auto r = stepEditor->getRequiredSize();
    cx -= r.getWidth() / 2;
    r = r.withBottomY(topOfControl).withX(cx);
    stepEditor->setBounds(r);

    stepEditor->setVisible(true);
    stepEditor->grabFocus();
}

} // namespace Widgets
} // namespace Surge
