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

#include "LFOAndStepDisplay.h"
#include "SurgeBitmaps.h"
#include "SurgeGUIEditor.h"
#include "RuntimeFont.h"
#include <chrono>

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

LFOAndStepDisplay::LFOAndStepDisplay() {}
void LFOAndStepDisplay::paint(juce::Graphics &g)
{
    TimeB paint("outerPaint");
    auto outer = getLocalBounds();
    auto leftPanel = outer.withRight(outer.getX() + lpsize + margin);
    auto mainPanel = outer.withLeft(leftPanel.getRight());

    if (ss && lfodata->shape.val.i == lt_stepseq)
    {
        paintStepSeq(g, mainPanel);
    }
    else
    {
        paintWaveform(g, mainPanel);
    }

    paintTypeSelector(g, leftPanel);
}

void LFOAndStepDisplay::paintWaveform(juce::Graphics &g, const juce::Rectangle<int> &within)
{
    TimeB mainTimer("-- paintWaveform");
    auto mainPanel = within.withTrimmedLeft(4);
    auto pathPanel = mainPanel.withTrimmedTop(0).withTrimmedBottom(0);
    // g.setColour(juce::Colours::blue);
    // g.drawRect(mainPanel, 1);

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
    tlfo->attack();

    LFOStorage deactivateStorage;
    bool hasFullWave = false, waveIsAmpWave = false;
    if (lfodata->rate.deactivated)
    {
        hasFullWave = true;
        memcpy((void *)(&deactivateStorage), (void *)lfodata, sizeof(LFOStorage));
        memcpy((void *)tpd, (void *)tp, n_scene_params * sizeof(pdata));

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
            memcpy((void *)(&deactivateStorage), (void *)lfodata, sizeof(LFOStorage));
            memcpy((void *)tpd, (void *)tp, n_scene_params * sizeof(pdata));

            deactivateStorage.magnitude.val.f = 1.f;
            tpd[lfodata->magnitude.param_id_in_scene].f = 1.f;
            tFullWave = new LFOModulationSource();
            tFullWave->assign(storage, &deactivateStorage, tpd, 0, ss, ms, fs, true);
            tFullWave->attack();
        }
    }

    bool drawEnvelope{true};
    if (skin->hasColor(Colors::LFO::Waveform::Background))
    {
        g.setColour(skin->getColor(Colors::LFO::Waveform::Background));
        g.fillRect(pathPanel);
    }

    int minSamples = (1 << 0) * (int)(mainPanel.getWidth());
    int totalSamples = std::max((int)minSamples, (int)(totalEnvTime * samplerate / BLOCK_SIZE));
    float drawnTime = totalSamples * samplerate_inv * BLOCK_SIZE;

    // OK so let's assume we want about 1000 pixels worth tops in
    int averagingWindow = (int)(totalSamples / 1000.0) + 1;

    float valScale = 100.0;
    int susCountdown = -1;

    float priorval = 0.f, priorwval = 0.f;
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
                tFullWave->process_block();
            if (susCountdown < 0 && tlfo->env_state == lfoeg_stuck)
            {
                susCountdown = susTime * samplerate / BLOCK_SIZE;
            }
            else if (susCountdown == 0 && tlfo->env_state == lfoeg_stuck)
            {
                tlfo->release();
                if (tFullWave)
                    tFullWave->release();
            }
            else if (susCountdown > 0)
            {
                susCountdown--;
            }

            val += tlfo->output;
            if (tFullWave)
            {
                auto v = tFullWave->output;
                minwval = std::min(v, minwval);
                maxwval = std::max(v, maxwval);
                wval += v;
            }
            if (s == 0)
                firstval = tlfo->output;
            if (s == averagingWindow - 1)
                lastval = tlfo->output;
            minval = std::min(tlfo->output, minval);
            maxval = std::max(tlfo->output, maxval);
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
            if (!isUnipolar() && (lfodata->shape.val.i != lt_envelope))
                edpath.startNewSubPath(xc, edval);
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

    auto at = juce::AffineTransform()
                  .scale(pathPanel.getWidth() / valScale, pathPanel.getHeight() / valScale)
                  .translated(pathPanel.getTopLeft());

    for (int i = -1; i < 2; ++i)
    {
        float off = i * 0.4 + 0.5; // so that/s 0.1, 0.5, 0.9
        float x0 = 0, y0 = valScale * off, x1 = valScale, y1 = valScale * off;
        at.transformPoint(x0, y0);
        at.transformPoint(x1, y1);
        if (i == 0)
            g.setColour(skin->getColor(Colors::LFO::Waveform::Center));
        else
            g.setColour(skin->getColor(Colors::LFO::Waveform::Bounds));

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
            float esize = 0.5;
            float xoff = (xd == 0 ? esize : 0);
            g.setColour(pointColor);
            g.fillEllipse(dotPointX - esize, dotPointY - esize, esize, esize);
        }
    }

    if (drawEnvelope)
    {
        g.setColour(skin->getColor(Colors::LFO::Waveform::Envelope));
        g.strokePath(eupath, juce::PathStrokeType(1.f), at);
        g.strokePath(edpath, juce::PathStrokeType(1.f), at);
    }

    if (drawBeats)
    {
        // calculate beat grid related data
        auto bpm = storage->temposyncratio * 120;

        auto denFactor = 4.0 / tsDen;
        auto beatsPerSecond = bpm / 60.0;
        auto secondsPerBeat = 1 / beatsPerSecond;
        auto deltaBeat = secondsPerBeat / drawnTime * valScale * denFactor;

        int nBeats = drawnTime * beatsPerSecond / denFactor;

        auto measureLen = deltaBeat * tsNum;
        int everyMeasure = 1;

        while (measureLen < 4) // a hand selected parameter by playing around with it, tbh
        {
            measureLen *= 2;
            everyMeasure *= 2;
        }

        for (auto l = 0; l < nBeats; ++l)
        {
            auto xp = deltaBeat * l;

            if (l % (tsNum * everyMeasure) == 0)
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
                g.setColour(skin->getColor(Colors::LFO::Waveform::Ruler::Ticks));
                g.drawLine(sp.getX(), sp.getY(), ep.getX(), ep.getY(), 1.0);

                char s[TXT_SIZE];
                snprintf(s, TXT_SIZE, "%d", l + 1);

                juce::Point<int> tp(xp + 1, valScale * 0.0);
                tp = tp.transformedBy(at);
                g.setColour(skin->getColor(Colors::LFO::Waveform::Ruler::Text));
                g.setFont(Surge::GUI::getFontManager()->lfoTypeFont);
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
    std::vector<float> timeDeltas = {0.5, 1.0, 2.5, 5.0, 10.0};
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
        g.setFont(Surge::GUI::getFontManager()->lfoTypeFont);
        char txt[TXT_SIZE];
        float tv = delta * l;
        if (fabs(roundf(tv) - tv) < 0.05)
            snprintf(txt, TXT_SIZE, "%d s", (int)round(delta * l));
        else
            snprintf(txt, TXT_SIZE, "%.1f s", delta * l);
        g.drawText(txt, tp.x, tp.y, 20, 10, juce::Justification::topRight);

        auto sp = juce::Point<float>(xp, valScale * 0.95).transformedBy(at);
        auto ep = juce::Point<float>(xp, valScale * 0.9).transformedBy(at);

        g.setColour(skin->getColor(Colors::LFO::Waveform::Ruler::Ticks));
        g.drawLine(sp.x, sp.y, ep.x, ep.y, 1.0);
    }

    /*
     * In 1.8 I think we don't want to show the key release point in the simulated wave
     * with the MSEG but I wrote it to debug and we may change our mid so keeping this code
     * here
     */
    if (msegRelease && false)
    {
#if SHOW_RELEASE_TIMES
        float xp = msegReleaseAt / drawnTime * valScale;
        CPoint sp(xp, valScale * 0.9), ep(xp, valScale * 0.1);
        tf.transform(sp);
        tf.transform(ep);
        dc->setFrameColor(juce::Colours::red);
        dc->drawLine(sp, ep);
#endif
    }
}

void LFOAndStepDisplay::paintStepSeq(juce::Graphics &g, const juce::Rectangle<int> &mainPanel)
{
    auto ssbg = skin->getColor(Colors::LFO::StepSeq::Background);
    g.setColour(ssbg);
    g.fillRect(mainPanel);

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
        juce::AffineTransform().translated(mainPanel.getTopLeft()).translated(19, 0);

    {
        juce::Graphics::ScopedSaveState gs(g);
        g.addTransform(shiftTranslate);

        for (int i = 0; i < n_stepseqsteps; i++)
        {
            juce::Rectangle<float> rstep(1.f * i * scale, 1.f * margin, scale,
                                         mainPanel.getHeight() - margin2),
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
                uint64_t maski16 = ss->trigmask & (UINT64_C(1) << (i + 16));
                uint64_t maski32 = ss->trigmask & (UINT64_C(1) << (i + 32));

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
                auto cy = rstep.getCentreY() - 1;
                auto fr = rstep.withTop(cy).withHeight(1.f);
                fillr(fr, shadowcol);
            }
            // Now draw the value
            auto v = rstep;
            int p1, p2;
            if (isUnipolar())
            {
                auto sv = std::max(ss->steps[i], 0.f);
                v = v.withTrimmedTop((int)(v.getHeight() * (1 - sv)));
            }
            else
            {
                p1 = v.getBottom() -
                     (int)((float)0.5f + v.getHeight() * (0.50f + 0.5f * ss->steps[i]));
                p2 = v.getCentreY();
                auto ttop = std::min(p1, p2);
                auto tbottom = std::max(p1, p2);
                v = v.withTop(ttop).withBottom(tbottom);
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
        g.setColour(grabMarker);

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

    ss_shift_left = mainPanel.toFloat().withWidth(10).withHeight(32).translated(2, 0).withCentre(
        juce::Point<float>(mainPanel.getX() + 10, mainPanel.getCentreY()));

    g.setColour(skin->getColor(Colors::LFO::StepSeq::Button::Border));
    g.fillRect(ss_shift_left);
    ss_shift_left = ss_shift_left.reduced(1, 1).withBottom(ss_shift_left.getY() + 16);

    float triO = 2;
    if (ss_shift_hover == 1)
        g.setColour(skin->getColor(Colors::LFO::StepSeq::Button::Hover));
    else
        g.setColour(skin->getColor(Colors::LFO::StepSeq::Button::Background));
    g.fillRect(ss_shift_left);
    auto triL = juce::Path();
    triL.addTriangle(ss_shift_left.getTopRight().translated(-triO, triO),
                     ss_shift_left.getBottomRight().translated(-triO, -triO),
                     ss_shift_left.getCentre().withX(ss_shift_left.getX() + triO));
    g.setColour(skin->getColor(Colors::LFO::StepSeq::Button::Arrow));
    if (ss_shift_hover == 1 && skin->getVersion() >= 2)
    {
        g.setColour(skin->getColor(Colors::LFO::StepSeq::Button::ArrowHover));
    }

    g.fillPath(triL);

    ss_shift_right = ss_shift_left.translated(0, 16);
    if (ss_shift_hover == 2)
        g.setColour(skin->getColor(Colors::LFO::StepSeq::Button::Hover));
    else
        g.setColour(skin->getColor(Colors::LFO::StepSeq::Button::Background));
    g.fillRect(ss_shift_right);
    auto triR = juce::Path();
    triR.addTriangle(
        ss_shift_right.getTopLeft().translated(triO, triO),
        ss_shift_right.getBottomLeft().translated(triO, -triO),
        ss_shift_right.getCentre().withX(ss_shift_right.getX() + ss_shift_right.getWidth() - triO));
    g.setColour(skin->getColor(Colors::LFO::StepSeq::Button::Arrow));
    if (ss_shift_hover == 2 && skin->getVersion() >= 2)
    {
        g.setColour(skin->getColor(Colors::LFO::StepSeq::Button::ArrowHover));
    }
    g.fillPath(triR);

    // OK now we have everything drawn we want to draw the LFO. This is similar to the LFO
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
    tlfo->attack();
    auto boxo = rect_steps;

    int minSamples = (1 << 3) * boxo.getWidth();
    int totalSamples = std::max((int)minSamples, (int)(totalSampleTime * samplerate / BLOCK_SIZE));
    float cycleSamples = cyclesec * samplerate / BLOCK_SIZE;

    // OK so lets assume we want about 1000 pixels worth tops in
    int averagingWindow = (int)(totalSamples / 1000.0) + 1;

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
                susCountdown = susTime * samplerate / BLOCK_SIZE;
            }
            else if (susCountdown == 0 && tlfo->env_state == lfoeg_stuck)
            {
                tlfo->release();
            }
            else if (susCountdown > 0)
            {
                susCountdown--;
            }

            val += tlfo->output;
            if (s == 0)
                firstval = tlfo->output;
            if (s == averagingWindow - 1)
                lastval = tlfo->output;
            minval = std::min(tlfo->output, minval);
            maxval = std::max(tlfo->output, maxval);
            eval += tlfo->env_val * lfodata->magnitude.get_extended(lfodata->magnitude.val.f);
        }
        val = val / averagingWindow;
        eval = eval / averagingWindow;

        if (isUnipolar())
            val = val * 2.0 - 1.0;

        val = ((-val + 1.0f) * 0.5) * valScale;
        float euval = ((-eval + 1.0f) * 0.5) * valScale;
        float edval = ((eval + 1.0f) * 0.5) * valScale;

        float xc = valScale * i / (cycleSamples * n_stepseqsteps);
        if (i == 0)
        {
            path.startNewSubPath(xc, val);
            eupath.startNewSubPath(xc, euval);
            if (!isUnipolar())
                edpath.startNewSubPath(xc, edval);
        }
        else
        {
            path.lineTo(xc, val);
            eupath.lineTo(xc, euval);
            edpath.lineTo(xc, edval);
        }
    }
    delete tlfo;

    auto q = boxo;

    auto tf = juce::AffineTransform()
                  .scaled(boxo.getWidth() / valScale, boxo.getHeight() / valScale)
                  .translated(q.getTopLeft().x, q.getTopLeft().y);

    auto tfpath = tf;

    g.setColour(skin->getColor(Colors::LFO::StepSeq::Envelope));
    g.strokePath(eupath, juce::PathStrokeType(1.0), tfpath);
    g.strokePath(edpath, juce::PathStrokeType(1.0), tfpath);

    g.setColour(skin->getColor(Colors::LFO::StepSeq::Wave));
    g.strokePath(path, juce::PathStrokeType(1.0), tfpath);

    // draw the RMB drag line
    if (dragMode == ARROW)
    {
        auto l = juce::Line<float>{arrowStart, arrowEnd};
        juce::Point<float> p;

        auto th = juce::Line<float>{rect_steps.getTopLeft(), rect_steps.getTopRight()};
        auto bh = juce::Line<float>{rect_steps.getBottomLeft(), rect_steps.getBottomRight()};
        auto lv = juce::Line<float>{rect_steps.getTopLeft(), rect_steps.getBottomLeft()};
        auto rv = juce::Line<float>{rect_steps.getTopRight(), rect_steps.getBottomRight()};

        if (l.intersects(th, p))
        {
            arrowEnd = p;
        }
        else if (l.intersects(bh, p))
        {
            arrowEnd = p;
        }
        else if (l.intersects(lv, p))
        {
            arrowEnd = p;
        }
        else if (l.intersects(rv, p))
        {
            arrowEnd = p;
        }
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
        int prec = 2;

        if (storage)
        {
            int detailedMode = Surge::Storage::getUserDefaultValue(
                storage, Surge::Storage::HighPrecisionReadouts, 0);
            if (detailedMode)
            {
                prec = 6;
            }
        }

        float dragX, dragY;
        float dragW = (prec > 4 ? 60 : 40), dragH = (keyModMult ? 22 : 12);

        auto sr = steprect[draggedStep];

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
            dragY = 2;

        auto labelR = juce::Rectangle<float>(dragX, dragY, dragW, dragH);

        fillr(labelR, skin->getColor(Colors::LFO::StepSeq::InfoWindow::Border));
        labelR = labelR.reduced(1, 1);
        fillr(labelR, skin->getColor(Colors::LFO::StepSeq::InfoWindow::Background));

        labelR = labelR.withTrimmedLeft(1).withHeight(10);

        char txt[TXT_SIZE];
        snprintf(txt, TXT_SIZE, "%.*f %%", prec, ss->steps[draggedStep] * 100.f);

        g.setColour(skin->getColor(Colors::LFO::StepSeq::InfoWindow::Text));
        g.setFont(Surge::GUI::getFontManager()->lfoTypeFont);
        g.drawText(txt, labelR, juce::Justification::centredLeft);

        if (keyModMult > 0)
        {
            labelR = labelR.translated(0, 10);

            snprintf(txt, TXT_SIZE, "%d/%d",
                     (int)(floor(ss->steps[draggedStep] * keyModMult + 0.5)), keyModMult);
            g.drawText(txt, labelR, juce::Justification::centredLeft);
        }
    }
}

void LFOAndStepDisplay::paintTypeSelector(juce::Graphics &g, const juce::Rectangle<int> &within)
{
    if (!typeImg)
    {
        g.setColour(juce::Colours::crimson);
        g.fillRect(within);
        return;
    }

    auto leftpanel = within.translated(0, margin + 2);
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
void LFOAndStepDisplay::onSkinChanged()
{
    if (!(skin && skinControl && associatedBitmapStore))
    {
        return;
    }
    auto typesWithHover = skin->standardHoverAndHoverOnForIDB(IDB_LFO_TYPE, associatedBitmapStore);
    typeImg = typesWithHover[0];
    typeImgHover = typesWithHover[1];
    typeImgHoverOn = typesWithHover[2];
}

void LFOAndStepDisplay::mouseDown(const juce::MouseEvent &event)
{

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
            if (i != lfodata->shape.val.i)
            {
                auto prior = lfodata->shape.val.i;
                lfodata->shape.val.i = i;
                sge->refresh_mod();
                sge->forceautomationchangefor(&(lfodata->shape));
                repaint();
                sge->lfoShapeChanged(prior, i);
            }
            return;
        }
    }

    if (isMSEG() && sge)
    {
        sge->toggleMSEGEditor();
    }

    if (isFormula() && sge)
    {
        sge->toggleFormulaEditorDialog();
    }

    if (isStepSequencer())
    {
        dragMode = NONE;
        if (ss_shift_left.contains(event.position))
        {
            float t = ss->steps[0];
            for (int i = 0; i < (n_stepseqsteps - 1); i++)
            {
                ss->steps[i] = ss->steps[i + 1];
                assert((i >= 0) && (i < n_stepseqsteps));
            }
            ss->steps[n_stepseqsteps - 1] = t;
            ss->trigmask = (((ss->trigmask & 0x000000000000fffe) >> 1) |
                            (((ss->trigmask & 1) << 15) & 0xffff)) |
                           (((ss->trigmask & 0x00000000fffe0000) >> 1) |
                            (((ss->trigmask & 0x10000) << 15) & 0xffff0000)) |
                           (((ss->trigmask & 0x0000fffe00000000) >> 1) |
                            (((ss->trigmask & 0x100000000) << 15) & 0xffff00000000));

            repaint();
            return;
        }
        if (ss_shift_right.contains(event.position))
        {
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
            repaint();
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
        if (event.mods.isRightButtonDown())
        {
            dragMode = ARROW;
            arrowStart = event.position;
            arrowEnd = event.position;

            return;
        }

        for (auto r : steprect)
        {
            if (r.contains(event.position))
            {
                dragMode = VALUES;
                return;
            }
        }
        for (int i = 0; i < n_stepseqsteps; ++i)
        {
            auto r = gaterect[i];
            if (r.contains(event.position))
            {
                dragMode = TRIGGERS;
                uint64_t maski = ss->trigmask & (UINT64_C(1) << i);
                uint64_t maski16 = ss->trigmask & (UINT64_C(1) << (i + 16));
                uint64_t maski32 = ss->trigmask & (UINT64_C(1) << (i + 32));

                uint64_t maskOn = 0;
                uint64_t maskOff = 0xFFFFFFFFFFFFFFFF;
                if (maski)
                {
                    maskOn = (UINT64_C(1) << (i + 16));
                    maskOff = ~maski;
                    dragTrigger0 = UINT64_C(1) << 16;
                }
                else if (maski16)
                {
                    maskOn = (UINT64_C(1) << (i + 32));
                    maskOff = ~maski16;
                    dragTrigger0 = UINT64_C(1) << 32;
                }
                else if (maski32)
                {
                    maskOn = 0;
                    maskOff = ~maski32;
                    dragTrigger0 = 0;
                }
                else
                {
                    maskOn = (UINT64_C(1) << (i + 0));
                    maskOff = ss->trigmask;
                    dragTrigger0 = UINT64_C(1);
                }

                ss->trigmask &= maskOff;
                ss->trigmask |= maskOn;
                repaint();
                return;
            }
        }
    }
}
void LFOAndStepDisplay::mouseMove(const juce::MouseEvent &event)
{
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
        if (lfoTypeHover >= 0)
        {
            // We aver over the type so
            return;
        }
    }
}

void LFOAndStepDisplay::mouseDrag(const juce::MouseEvent &event)
{
    if (!isStepSequencer())
        return;

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
                ss->loop_start = loopStart;
                repaint();
            }
        }
        else
        {
            if (ss->loop_end != loopStart && loopStart >= 0)
            {
                ss->loop_end = loopStart;
                repaint();
            }
        }
        break;
    }
    case TRIGGERS:
    {
        for (int i = 0; i < n_stepseqsteps; ++i)
        {
            auto r = gaterect[i];
            auto otm = ss->trigmask;
            if (r.contains(event.position))
            {
                auto off = UINT64_MAX & ~(UINT64_C(1) << i) & ~(UINT64_C(1) << (i + 16)) &
                           ~(UINT64_C(1) << (i + 32));
                auto on = dragTrigger0 << i;
                ss->trigmask &= off;
                ss->trigmask |= on;
            }
            if (ss->trigmask != otm)
                repaint();
        }
        break;
    }
    case VALUES:
    {
        keyModMult = 0;
        int quantStep = 12;

        if (!storage->isStandardTuning && storage->currentScale.count > 1)
            quantStep = storage->currentScale.count;

        for (int i = 0; i < n_stepseqsteps; ++i)
        {
            auto r = steprect[i];
            if (r.contains(event.position))
            {
                draggedStep = i;
                float f;
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
                ss->steps[i] = f;
                repaint();
            }
        }
        break;
    }
    }
}

void LFOAndStepDisplay::mouseUp(const juce::MouseEvent &event)
{
    if (dragMode == ARROW)
    {
        auto rmStepStart = arrowStart;
        auto rmStepCurr = arrowEnd;

        int startStep = -1;
        int endStep = -1;

        // find out if a microtuning is loaded and store the scale length for Alt-drag
        // quantization to scale degrees
        keyModMult = 0;
        int quantStep = 12;

        if (!storage->isStandardTuning && storage->currentScale.count > 1)
            quantStep = storage->currentScale.count;

        for (int i = 0; i < 16; ++i)
        {
            if (steprect[i].contains(rmStepStart))
                startStep = i;
            if (steprect[i].contains(rmStepCurr))
                endStep = i;
        };

        int s = startStep, e = endStep;

        if (s >= 0 && e >= 0 && s != e) // s == e is the abort gesture
        {
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
        for (int i = 0; i < n_stepseqsteps; ++i)
        {
            if (steprect[i].contains(event.position))
            {
                ss->steps[i] = 0.f;
                repaint();
            }
        }
    }
}
void LFOAndStepDisplay::mouseWheelMove(const juce::MouseEvent &event,
                                       const juce::MouseWheelDetails &wheel)
{
    if (!isStepSequencer())
        return;

    float delta = wheel.deltaX - (wheel.isReversed ? 1 : -1) * wheel.deltaY;
#if MAC
    delta *= 1.3;
#endif

    if (event.mods.isShiftDown())
        delta *= 0.1;

    if (delta == 0)
        return;

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
            repaint();
        }
    }
}

} // namespace Widgets
} // namespace Surge