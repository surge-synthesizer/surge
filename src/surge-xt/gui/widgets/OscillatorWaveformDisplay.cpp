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

#include "OscillatorWaveformDisplay.h"
#include "SurgeStorage.h"
#include "SurgeSynthProcessor.h"
#include "SurgeSynthEditor.h"
#include "Oscillator.h"
#include "OscillatorBase.h"
#include "RuntimeFont.h"
#include "SurgeGUIUtils.h"
#include "SurgeGUIEditor.h"
#include "StringOscillator.h"
#include "AliasOscillator.h"
#include "widgets/MenuCustomComponents.h"
#include "AccessibleHelpers.h"
#include "UserDefaults.h"
#include "fmt/core.h"

namespace Surge
{
namespace Widgets
{
OscillatorWaveformDisplay::OscillatorWaveformDisplay()
{
    setAccessible(true);
    setFocusContainerType(FocusContainerType::focusContainer);

    auto ol = std::make_unique<OverlayAsAccessibleButton<OscillatorWaveformDisplay>>(
        this, "Wavetable: (Unknown)", juce::AccessibilityRole::button);

    addChildComponent(*ol);

    ol->onPress = [this](OscillatorWaveformDisplay *d) { showWavetableMenu(); };
    ol->onMenuKey = [this](OscillatorWaveformDisplay *d) {
        showWavetableMenu();
        return true;
    };

    menuOverlays[0] = std::move(ol);

    ol = std::make_unique<OverlayAsAccessibleButton<OscillatorWaveformDisplay>>(
        this, "Wavetable: Previous", juce::AccessibilityRole::button);
    ol->onPress = [this](OscillatorWaveformDisplay *d) {
        auto id = storage->getAdjacentWaveTable(oscdata->wt.current_id, false);

        if (id >= 0)
        {
            std::string announce = "Loaded Wavetable ";
            announce += storage->wt_list[id].name;
            sge->enqueueAccessibleAnnouncement(announce);

            oscdata->wt.queue_id = id;
        }
    };
    ol->onReturnKey = [ov = ol.get()](OscillatorWaveformDisplay *d) {
        ov->onPress(d);
        return true;
    };

    addChildComponent(*ol);

    menuOverlays[1] = std::move(ol);

    ol = std::make_unique<OverlayAsAccessibleButton<OscillatorWaveformDisplay>>(
        this, "Wavetable: Next", juce::AccessibilityRole::button);

    addChildComponent(*ol);

    ol->onPress = [this](OscillatorWaveformDisplay *d) {
        auto id = storage->getAdjacentWaveTable(oscdata->wt.current_id, true);

        if (id >= 0)
        {
            std::string announce = "Loaded Wavetable ";
            announce += storage->wt_list[id].name;
            sge->enqueueAccessibleAnnouncement(announce);

            oscdata->wt.queue_id = id;
            auto new_name = storage->getCurrentWavetableName(oscdata);

            SurgeSynthProcessor *ssp = &sge->juceEditor->processor;
            ssp->paramChangeToListeners(nullptr, true, ssp->SCT_WAVETABLE, (float)scene,
                                        (float)oscInScene, (float)id, new_name);
        }
    };
    ol->onReturnKey = [ov = ol.get()](OscillatorWaveformDisplay *d) {
        ov->onPress(d);
        return true;
    };

    menuOverlays[2] = std::move(ol);

    ol = std::make_unique<OverlayAsAccessibleButton<OscillatorWaveformDisplay>>(
        this, customEditor ? "Close Custom Editor" : "Open Custom Editor",
        juce::AccessibilityRole::button);
    ol->setWantsKeyboardFocus(true);
    addChildComponent(*ol);

    ol->onPress = [this](OscillatorWaveformDisplay *d) {
        if (customEditor)
        {
            hideCustomEditor();
            d->customEditorAccOverlay->setDescription("Close Custom Editor");
            d->customEditorAccOverlay->setTitle("Close Custom Editor");
        }
        else
        {
            showCustomEditor();
            d->customEditorAccOverlay->setDescription("Open Custom Editor");
            d->customEditorAccOverlay->setTitle("Open Custom Editor");
        }
    };
    ol->onReturnKey = [this](OscillatorWaveformDisplay *d) {
        if (customEditor)
        {
            hideCustomEditor();
            d->customEditorAccOverlay->setDescription("Close Custom Editor");
            d->customEditorAccOverlay->setTitle("Close Custom Editor");
        }
        else
        {
            showCustomEditor();
            d->customEditorAccOverlay->setDescription("Open Custom Editor");
            d->customEditorAccOverlay->setTitle("Open Custom Editor");
        }
        return true;
    };

    customEditorAccOverlay = std::move(ol);
}

OscillatorWaveformDisplay::~OscillatorWaveformDisplay() = default;

void OscillatorWaveformDisplay::paint(juce::Graphics &g)
{
    bool skipEntireOscillator{false};

    if (supportsCustomEditor() && customEditor)
    {
        skipEntireOscillator = true;
    }

    if (!supportsCustomEditor() && customEditor)
    {
        hideCustomEditor();
    }

    bool usesWT = uses_wavetabledata(oscdata->type.val.i);

    if (!skipEntireOscillator)
    {
        auto osc = setupOscillator();

        if (!osc)
        {
            return;
        }

        int totalSamples = (1 << 3) * (int)getWidth();
        int averagingWindow = 4; // < and Mult of BlockSizeOS
        float disp_pitch_rs = disp_pitch + 12.0 * log2(storage->dsamplerate / 44100.0);

        if (!storage->isStandardTuning)
        {
            // OK so in this case we need to find a better version of the note which gets us
            // that pitch. Only way is to search really.
            auto pit = storage->note_to_pitch_ignoring_tuning(disp_pitch_rs);
            int bracket = -1;

            for (int i = 0; i < 128; ++i)
            {
                if (storage->note_to_pitch(i) < pit && storage->note_to_pitch(i + 1) > pit)
                {
                    bracket = i;

                    break;
                }
            }

            if (bracket >= 0)
            {
                float f1 = storage->note_to_pitch(bracket);
                float f2 = storage->note_to_pitch(bracket + 1);
                float frac = (pit - f1) / (f2 - f1);

                disp_pitch_rs = bracket + frac;
            }

            // That's a strange non-monotonic tuning. Oh well.
        }

        bool use_display = osc->allow_display();

        if (use_display)
        {
            osc->init(disp_pitch_rs, true, true);
        }

        int block_pos = BLOCK_SIZE;
        juce::Path wavePath;

        float oscTmp alignas(16)[2][BLOCK_SIZE_OS];
        sst::filters::HalfRate::HalfRateFilter hr(6, true);
        hr.load_coefficients();
        hr.reset();

        for (int i = 0; i < totalSamples; i += averagingWindow)
        {
            if (use_display && block_pos >= BLOCK_SIZE)
            {
                // Lock it even if we aren't wavetable. It's fine.
                storage->waveTableDataMutex.lock();
                osc->process_block(disp_pitch_rs);
                memcpy(oscTmp[0], osc->output, sizeof(oscTmp[0]));
                memcpy(oscTmp[1], osc->output, sizeof(oscTmp[1]));
                hr.process_block_D2(oscTmp[0], oscTmp[1], BLOCK_SIZE_OS);
                block_pos = 0;
                storage->waveTableDataMutex.unlock();
            }

            float val = 0.f;

            if (use_display)
            {
                for (int j = 0; j < averagingWindow; ++j)
                {
                    val += oscTmp[0][block_pos];
                    block_pos++;
                }

                val = val / averagingWindow;
            }

            float xc = 1.f * i / totalSamples;

            if (i == 0)
            {
                wavePath.startNewSubPath(xc, val);
            }
            else
            {
                wavePath.lineTo(xc, val);
            }
        }

        osc->~Oscillator();
        osc = nullptr;

        auto yMargin = 2 * usesWT;
        auto h = getHeight() - usesWT * wtbheight - 2 * yMargin;
        auto xMargin = 2;
        auto w = getWidth() - 2 * xMargin;
        auto downScale = 0.235;
        auto tf = juce::AffineTransform()
                      .scaled(w, -(1 - downScale) * h / 2)
                      .translated(xMargin, h / 2 + yMargin);

        // draw the lines
        auto tfLine = [tf](float x0, float y0, float x1, float y1) -> juce::Line<float> {
            auto p0 = juce::Point<float>(x0, y0).transformedBy(tf);
            auto p1 = juce::Point<float>(x1, y1).transformedBy(tf);
            return juce::Line<float>(p0, p1);
        };

        g.setColour(skin->getColor(Colors::Osc::Display::Bounds));
        g.drawLine(tfLine(0, -1, 1, -1));
        g.drawLine(tfLine(0, 1, 1, 1));

        g.setColour(skin->getColor(Colors::Osc::Display::Center));
        g.drawLine(tfLine(0, 0, 1, 0));

        // draw the dots
        g.setColour(skin->getColor(Colors::Osc::Display::Dots));

        int nxd = 21, nyd = 13;

        for (int xd = 0; xd < nxd; ++xd)
        {
            float normx = 1.f * xd / (nxd - 1);

            for (int yd = 0; yd < nyd; ++yd)
            {
                float normy = 2.f * yd / (nyd - 1) - 1;
                auto p = juce::Point<float>(normx, normy).transformedBy(tf);

                g.fillEllipse(p.x - 0.5, p.y - 0.5, 1, 1);
            }
        }

        // draw the waveform
        g.setColour(
            skin->getColor(Colors::Osc::Display::Wave).withMultipliedAlpha(isMuted ? 0.5f : 1.f));
        g.strokePath(wavePath, juce::PathStrokeType(1.3), tf);
    }

    if (usesWT)
    {
        if (oscdata->wt.current_id != lastWavetableId || forceWTRepaint)
        {
            if (forceWTRepaint)
            {
                forceWTRepaint = false;
            }

            auto nd = std::string("Wavetable: ") + storage->getCurrentWavetableName(oscdata);

            menuOverlays[0]->setTitle(nd);
            menuOverlays[0]->setDescription(nd);

            if (auto ah = menuOverlays[0]->getAccessibilityHandler())
            {
                ah->notifyAccessibilityEvent(juce::AccessibilityEvent::titleChanged);
            }

            if (customEditor)
            {
                // super unsatisfactory. TODO: fix later!
                showCustomEditor();
            }

            lastWavetableId = oscdata->wt.current_id;
        }

        auto fgcol = skin->getColor(Colors::Osc::Filename::Background);
        auto fgframe = skin->getColor(Colors::Osc::Filename::Frame);
        auto fgtext = skin->getColor(Colors::Osc::Filename::Text);

        auto fgcolHov = skin->getColor(Colors::Osc::Filename::BackgroundHover);
        auto fgframeHov = skin->getColor(Colors::Osc::Filename::FrameHover);
        auto fgtextHov = skin->getColor(Colors::Osc::Filename::TextHover);

        auto wtr = getLocalBounds().withTop(getHeight() - wtbheight).withTrimmedBottom(1).toFloat();

        g.setColour(juce::Colours::orange);
        g.fillRect(wtr);

        leftJog = wtr.withRight(wtbheight);
        rightJog = wtr.withLeft(wtr.getWidth() - wtbheight);
        waveTableName = wtr.withTrimmedLeft(wtbheight).withTrimmedRight(wtbheight);

        float triO = 2;

        g.setColour(isJogLHovered ? fgcolHov : fgcol);
        g.fillRect(leftJog);
        g.setColour(isJogLHovered ? fgframeHov : fgframe);
        g.drawRect(leftJog);
        g.setColour(isJogLHovered ? fgtextHov : fgtext);

        auto triL = juce::Path();

        triL.addTriangle(leftJog.getTopRight().translated(-triO, triO),
                         leftJog.getBottomRight().translated(-triO, -triO),
                         leftJog.getCentre().withX(leftJog.getX() + triO));

        g.fillPath(triL);
        g.setColour(isJogRHovered ? fgcolHov : fgcol);
        g.fillRect(rightJog);
        g.setColour(isJogRHovered ? fgframeHov : fgframe);
        g.drawRect(rightJog);
        g.setColour(isJogRHovered ? fgtextHov : fgtext);

        auto triR = juce::Path();

        triR.addTriangle(rightJog.getTopLeft().translated(triO, triO),
                         rightJog.getBottomLeft().translated(triO, -triO),
                         rightJog.getCentre().withX(rightJog.getX() + rightJog.getWidth() - triO));

        g.fillPath(triR);
        g.setColour(isWtNameHovered ? fgcolHov : fgcol);
        g.fillRect(waveTableName);
        g.setColour(isWtNameHovered ? fgframeHov : fgframe);
        g.drawRect(waveTableName);

        auto wtn = storage->getCurrentWavetableName(oscdata);

        g.setColour(isWtNameHovered ? fgtextHov : fgtext);
        g.setFont(skin->fontManager->getLatoAtSize(9));
        g.drawText(juce::String(wtn), waveTableName, juce::Justification::centred);
    }

    if (supportsCustomEditor())
    {
        drawEditorBox(g, customEditorActionLabel(customEditor == nullptr));
    }

    // Display the latency message in audio input.
    if (sge && sge->audioLatencyNotified)
    {
        auto osctype = oscdata->type.val.i;

        if (osctype == ot_audioinput ||
            (osctype == ot_string && oscdata->p[StringOscillator::str_exciter_mode].val.i ==
                                         StringOscillator::constant_audioin) ||
            (osctype == ot_alias &&
             oscdata->p[AliasOscillator::ao_wave].val.i == AliasOscillator::aow_audiobuffer))
        {
            auto lmsg = fmt::format("Input Delay: {} samples", BLOCK_SIZE);

            g.setColour(skin->getColor(Colors::Osc::Display::Wave));
            g.setFont(skin->fontManager->getLatoAtSize(7));
            g.drawText(lmsg.c_str(), getLocalBounds().reduced(2, 2).translated(0, 10),
                       juce::Justification::topLeft);
        }
    }
}

void OscillatorWaveformDisplay::setZoomFactor(int zoom)
{
    if (customEditor && uses_wavetabledata(oscdata->type.val.i))
    {
        customEditor->resized();
    }
}

void OscillatorWaveformDisplay::resized()
{
    auto wtr = getLocalBounds().withTop(getHeight() - wtbheight).withTrimmedBottom(1).toFloat();
    auto wtl = getLocalBounds().withTrimmedBottom(wtbheight);

    leftJog = wtr.withRight(wtbheight);
    menuOverlays[1]->setBounds(leftJog.toNearestInt());
    rightJog = wtr.withLeft(wtr.getWidth() - wtbheight);
    menuOverlays[2]->setBounds(rightJog.toNearestInt());
    waveTableName = wtr.withTrimmedRight(wtbheight).withTrimmedLeft(wtbheight);
    menuOverlays[0]->setBounds(waveTableName.toNearestInt());

    customEditorBox = getLocalBounds()
                          .withTop(getHeight() - wtbheight)
                          .withRight(60)
                          .withTrimmedBottom(1)
                          .toFloat();
    customEditorAccOverlay->setBounds(customEditorBox.toNearestInt());
}

void OscillatorWaveformDisplay::repaintIfIdIsInRange(int id)
{
    auto *firstOscParam = &oscdata->type;
    auto *lastOscParam = &oscdata->retrigger;

    bool oscInvalid = false;

    while (firstOscParam <= lastOscParam && !oscInvalid)
    {
        if (firstOscParam->id == id)
        {
            oscInvalid = true;
        }

        firstOscParam++;
    }

    if (oscInvalid)
    {
        repaint();
    }
}

void OscillatorWaveformDisplay::repaintBasedOnOscMuteState()
{
    bool oscInvalid = false;
    bool isMutedTest = false;

    // grab the mute/solo states
    auto *ms = &storage->getPatch().scene[scene].mute_o1;
    auto *ss = &storage->getPatch().scene[scene].solo_o1;
    bool statesMuteSolo[2][n_oscs];

    for (int i = 0; i < n_oscs; i++)
    {
        statesMuteSolo[0][i] = ms->val.i;
        statesMuteSolo[1][i] = ss->val.i;
        ms++;
        ss++;
    }

    // if current osc is muted but not soloed
    if (statesMuteSolo[0][oscInScene] == 1 && statesMuteSolo[1][oscInScene] == 0)
    {
        isMutedTest = true;
    }

    // if other oscs are soloed but current isn't, it is considered muted
    for (int i = 0; i < n_oscs; i++)
    {
        if (statesMuteSolo[1][i] && i != oscInScene)
        {
            isMutedTest = true;
        }
    }

    // if current osc is soloed it's always active, even if it's muted (LOL!)
    if (statesMuteSolo[1][oscInScene] == 1)
    {
        isMutedTest = false;
    }

    // only trigger a repaint if the mute test is a different result from the member isMuted
    if (isMutedTest != isMuted)
    {
        isMuted = isMutedTest;
        oscInvalid = true;
    }

    if (oscInvalid)
    {
        forceWTRepaint = true;
        repaint();
    }
}

::Oscillator *OscillatorWaveformDisplay::setupOscillator()
{
    tp[oscdata->pitch.param_id_in_scene].f = 0;

    for (int i = 0; i < n_osc_params; i++)
    {
        tp[oscdata->p[i].param_id_in_scene].i = oscdata->p[i].val.i;
    }

    return spawn_osc(oscdata->type.val.i, storage, oscdata, tp, tp, oscbuffer);
}

void OscillatorWaveformDisplay::populateMenu(juce::PopupMenu &contextMenu, int selectedItem,
                                             bool singleCategory)
{
    bool addUserLabel = false;
    int idx = 0;

    if (selectedItem >= 0 && selectedItem < storage->wt_list.size() && singleCategory)
    {
        populateMenuForCategory(contextMenu, storage->wt_list[selectedItem].category, selectedItem,
                                true);
    }
    else
    {
        for (auto c : storage->wtCategoryOrdering)
        {
            if (idx == storage->firstThirdPartyWTCategory)
            {
                Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(
                    contextMenu, "3RD PARTY WAVETABLES");
            }

            if (idx == storage->firstUserWTCategory &&
                storage->firstUserWTCategory != storage->wt_category.size())
            {
                addUserLabel = true;
            }

            idx++;

            // only add this section header if we actually have any factory WTs installed
            if (idx == 1)
            {
                Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(
                    contextMenu, "FACTORY WAVETABLES");
            }

            PatchCategory cat = storage->wt_category[c];

            if (cat.numberOfPatchesInCategoryAndChildren == 0)
            {
                continue;
            }

            if (addUserLabel)
            {
                Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(contextMenu,
                                                                                "USER WAVETABLES");
                addUserLabel = false;
            }

            if (cat.isRoot)
            {
                populateMenuForCategory(contextMenu, c, selectedItem);
            }
        }
    }

    contextMenu.addSeparator();

#if INCLUDE_WT_SCRIPTING_EDITOR
    auto owts = [this]() {
        if (sge)
            sge->showOverlay(SurgeGUIEditor::WT_EDITOR);
    };

    contextMenu.addItem(Surge::GUI::toOSCase("Wavetable Editor..."), owts);
    contextMenu.addSeparator();
#endif

    // add this option only if we have any wavetables in the list
    if (idx > 0)
    {
        auto refresh = [this]() { this->storage->refresh_wtlist(); };

        contextMenu.addItem(Surge::GUI::toOSCase("Refresh Wavetable List"), refresh);
    }

    auto rnaction = [this]() {
        auto c = this->oscdata->wavetable_display_name;

        if (sge)
        {
            sge->promptForMiniEdit(
                c, "Enter a new name:", "Wavetable Display Name", juce::Point<int>{},
                [this](const std::string &s) {
                    this->oscdata->wavetable_display_name = s;
                    this->repaint();
                },
                this);
        }
    };

    contextMenu.addItem(Surge::GUI::toOSCase("Rename Wavetable..."), rnaction);

    contextMenu.addSeparator();

    auto action = [this]() { this->loadWavetableFromFile(); };
    contextMenu.addItem(Surge::GUI::toOSCase("Load Wavetable from File..."), action);

    juce::PopupMenu xportMenu;
    for (bool isWav : {true, false})
    {
        auto exportAction = [this, isWav]() {
            int sc = this->scene;
            int o = this->oscInScene;
            std::string wtName = storage->getPatch().scene[sc].osc[o].wavetable_display_name;
            std::string baseName = "";

            if (wtName.compare(0, 6, "(Patch") == 0)
            {
                baseName = fmt::format("{} {}{}", storage->getPatch().name, (sc == 0 ? "A" : "B"),
                                       std::to_string(o + 1));
            }
            else
            {
                baseName = wtName;
            }
            auto path = sge->synth->storage.userDataPath / "Wavetables" / "Exported";

            try
            {
                fs::create_directories(path);
            }
            catch (const fs::filesystem_error &e)
            {
            }

            auto startNm = path / oscdata->wavetable_display_name;
            if (isWav)
                startNm = startNm.replace_extension(".wav");
            else
                startNm = startNm.replace_extension(".wt");

            auto nm = isWav ? "Export WAV Wavetable" : "Export WT Wavetable";
            auto that = this; // i hate msvc
            sge->fileChooser =
                std::make_unique<juce::FileChooser>(nm, juce::File(startNm.u8string().c_str()));
            sge->fileChooser->launchAsync(
                juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                    juce::FileBrowserComponent::warnAboutOverwriting,
                [w = that, isWav](const juce::FileChooser &c) {
                    auto result = c.getResults();
                    if (result.isEmpty() || result.size() > 1)
                    {
                        return;
                    }
                    auto fsp = fs::path{result[0].getFullPathName().toStdString()};

                    std::string metadata = w->storage->make_wt_metadata(w->oscdata);
                    if (isWav)
                    {
                        if (fsp.extension() != ".wav")
                        {
                            fsp = fsp.replace_extension(".wav");
                        }
                        auto fn =
                            w->storage->export_wt_wav_portable(fsp, &(w->oscdata->wt), metadata);
                    }
                    else
                    {
                        if (fsp.extension() != ".wt")
                        {
                            fsp = fsp.replace_extension(".wt");
                        }
                        if (!w->storage->export_wt_wt_portable(fsp, &(w->oscdata->wt), metadata))
                        {
                            w->storage->reportError("Unable to save wt to " + fsp.u8string(),
                                                    "WT Export");
                        }
                    }
                    w->storage->refresh_wtlist();
                });
        };
        if (isWav)
            xportMenu.addItem("To .WAV...", exportAction);
        else
            xportMenu.addItem("To .WT...", exportAction);
    }
    contextMenu.addSubMenu("Export Wavetable", xportMenu);

    contextMenu.addSeparator();

    createWTMenuItems(contextMenu);
}

void OscillatorWaveformDisplay::createWTMenu(const bool useComponentBounds = true)
{
    bool usesWT = uses_wavetabledata(oscdata->type.val.i);

    if (usesWT)
    {
        auto contextMenu = juce::PopupMenu();

        createWTMenuItems(contextMenu, true, true);

        contextMenu.showMenuAsync(sge->popupMenuOptions(useComponentBounds ? this : nullptr));
    }
}

void OscillatorWaveformDisplay::createWTMenuItems(juce::PopupMenu &contextMenu, bool centered,
                                                  bool add2D3Dswitch)
{
    if (sge)
    {
        auto hu = sge->helpURLForSpecial("wavetables");

        if (hu != "")
        {
            auto lurl = sge->fullyResolvedHelpURL(hu);
            auto tc = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>("Wavetables", lurl);

            tc->setSkin(skin, associatedBitmapStore);
            tc->setCentered(centered);

            auto hment = tc->getTitle();

            contextMenu.addCustomItem(-1, std::move(tc), nullptr, hment);
        }

        if (add2D3Dswitch)
        {
            contextMenu.addSeparator();

            auto action = [this]() {
                toggleCustomEditor();

                if (!customEditor)
                {
                    Surge::Storage::updateUserDefaultValue(
                        storage, Surge::Storage::Use3DWavetableView, false);
                }
            };

            auto text = fmt::format("Switch to {} Display", (customEditor) ? "2D" : "3D");

            contextMenu.addItem(Surge::GUI::toOSCase(text), action);

#if INCLUDE_WT_SCRIPTING_EDITOR
            contextMenu.addSeparator();

            auto owts = [this]() {
                if (sge)
                    sge->showOverlay(SurgeGUIEditor::WT_EDITOR);
            };

            contextMenu.addItem(Surge::GUI::toOSCase("Wavetable Editor..."), owts);
            contextMenu.addSeparator();
#endif

            contextMenu.addSeparator();

            Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(contextMenu, "INFO");

            // These are enabled simply so they show up in the screen reader
            contextMenu.addItem(
                Surge::GUI::toOSCase(fmt::format("Number of Frames: {}", oscdata->wt.n_tables)),
                true, false, nullptr);
            contextMenu.addItem(
                Surge::GUI::toOSCase(fmt::format("Frame Length: {} samples", oscdata->wt.size)),
                true, false, nullptr);
        }
    }
}

void OscillatorWaveformDisplay::createAliasOptionsMenu(const bool useComponentBounds,
                                                       const bool onlyHelpEntry)
{
    auto contextMenu = juce::PopupMenu();

    {
        auto msurl = SurgeGUIEditor::helpURLForSpecial(storage, "alias-shape");
        auto hurl = SurgeGUIEditor::fullyResolvedHelpURL(msurl);
        auto tc = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>(
            fmt::format("Alias Additive Editor{}", onlyHelpEntry ? "" : " Options"), hurl);

        auto hment = tc->getTitle();

        tc->setSkin(skin, associatedBitmapStore);

        contextMenu.addCustomItem(-1, std::move(tc), nullptr, hment);

        contextMenu.addSeparator();
    }

    if (!onlyHelpEntry)
    {
        {
            auto action = [this]() {
                sge->undoManager()->pushOscillatorExtraConfig(scene, oscInScene);

                for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                {
                    oscdata->extraConfig.data[qq] = (qq == 0) ? 1 : 0;
                }
                storage->getPatch().isDirty = true;

                repaint();
            };

            contextMenu.addItem("Sine", action);
        }

        {
            auto action = [this]() {
                sge->undoManager()->pushOscillatorExtraConfig(scene, oscInScene);

                for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                {
                    oscdata->extraConfig.data[qq] = (qq % 2 == 0) * 1.f / ((qq + 1) * (qq + 1));

                    if (qq % 4 == 2)
                    {
                        oscdata->extraConfig.data[qq] *= -1.f;
                    }
                }
                storage->getPatch().isDirty = true;

                repaint();
            };

            contextMenu.addItem("Triangle", action);
        }

        {
            auto action = [this]() {
                sge->undoManager()->pushOscillatorExtraConfig(scene, oscInScene);

                for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                {
                    oscdata->extraConfig.data[qq] = 1.f / (qq + 1);
                }
                storage->getPatch().isDirty = true;

                repaint();
            };

            contextMenu.addItem("Sawtooth", action);
        }

        {
            auto action = [this]() {
                sge->undoManager()->pushOscillatorExtraConfig(scene, oscInScene);

                for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                {
                    oscdata->extraConfig.data[qq] = (qq % 2 == 0) * 1.f / (qq + 1);
                }
                storage->getPatch().isDirty = true;

                repaint();
            };

            contextMenu.addItem("Square", action);
        }

        {
            auto action = [this]() {
                sge->undoManager()->pushOscillatorExtraConfig(scene, oscInScene);

                for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                {
                    oscdata->extraConfig.data[qq] = storage->rand_pm1();
                }
                storage->getPatch().isDirty = true;

                repaint();
            };

            contextMenu.addItem("Random", action);
        }

        contextMenu.addSeparator();

        {
            auto action = [this]() {
                sge->undoManager()->pushOscillatorExtraConfig(scene, oscInScene);

                for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                {
                    if (oscdata->extraConfig.data[qq] < 0)
                    {
                        oscdata->extraConfig.data[qq] *= -1;
                    }
                }
                storage->getPatch().isDirty = true;

                repaint();
            };

            contextMenu.addItem("Absolute", action);
        }

        {
            auto action = [this]() {
                sge->undoManager()->pushOscillatorExtraConfig(scene, oscInScene);

                for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                {
                    oscdata->extraConfig.data[qq] = -oscdata->extraConfig.data[qq];
                }
                storage->getPatch().isDirty = true;

                repaint();
            };

            contextMenu.addItem("Invert", action);
        }

        {
            auto action = [this]() {
                sge->undoManager()->pushOscillatorExtraConfig(scene, oscInScene);
                storage->getPatch().isDirty = true;

                float pdata[AliasOscillator::n_additive_partials];

                for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                {
                    pdata[qq] = oscdata->extraConfig.data[qq];
                }

                for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                {
                    oscdata->extraConfig.data[15 - qq] = pdata[qq];
                }

                repaint();
            };

            contextMenu.addItem("Reverse", action);
        }
    }

    contextMenu.showMenuAsync(sge->popupMenuOptions(useComponentBounds ? this : nullptr));
}

bool OscillatorWaveformDisplay::populateMenuForCategory(juce::PopupMenu &contextMenu,
                                                        int categoryId, int selectedItem,
                                                        bool intoTop)
{
    int sub = 0;
    bool selected = false;
    juce::PopupMenu subMenuLocal;
    juce::PopupMenu *subMenu = &subMenuLocal;
    PatchCategory cat = storage->wt_category[categoryId];

    if (intoTop)
    {
        subMenu = &contextMenu;
    }

    for (auto p : storage->wtOrdering)
    {
        if (storage->wt_list[p].category == categoryId)
        {
            auto action = [this, p]() { this->loadWavetable(p); };
            bool checked = false;

            if (p == selectedItem)
            {
                checked = true;
                selected = true;
            }

            subMenu->addItem(storage->wt_list[p].name, true, checked, action);

            sub++;

            if (sub != 0 && sub % 16 == 0)
            {
                subMenu->addColumnBreak();
            }
        }
    }

    for (auto child : cat.children)
    {
        if (child.numberOfPatchesInCategoryAndChildren > 0)
        {
            // this isn't the best approach but it works
            int cidx = 0;

            for (auto &cc : storage->wt_category)
            {
                if (cc.name == child.name)
                {
                    break;
                }

                cidx++;
            }

            bool checked = populateMenuForCategory(*subMenu, cidx, selectedItem);

            if (checked)
            {
                selected = true;
            }
        }
    }

    std::string name;

    if (!cat.isRoot)
    {
        std::string catName = storage->wt_category[categoryId].name;
        std::size_t sepPos = catName.find_last_of(PATH_SEPARATOR);

        if (sepPos != std::string::npos)
        {
            catName = catName.substr(sepPos + 1);
        }

        name = catName;
    }
    else
    {
        name = storage->wt_category[categoryId].name;
    }

    if (!intoTop)
    {
        contextMenu.addSubMenu(name, *subMenu, true, nullptr, selected);
    }

    return selected;
}

void OscillatorWaveformDisplay::loadWavetable(int id)
{
    if (id >= 0 && (id < storage->wt_list.size()))
    {
        if (sge)
        {
            sge->undoManager()->pushWavetable(scene, oscInScene);
            std::string announce = "Loaded Wavetable ";
            announce += storage->wt_list[id].name;
            sge->enqueueAccessibleAnnouncement(announce);
        }
        oscdata->wt.queue_id = id;
        auto new_name = storage->getCurrentWavetableName(oscdata);

        SurgeSynthProcessor *ssp = &sge->juceEditor->processor;
        ssp->paramChangeToListeners(nullptr, true, ssp->SCT_WAVETABLE, (float)scene,
                                    (float)oscInScene, (float)id, new_name);
    }
}

void OscillatorWaveformDisplay::loadWavetableFromFile()
{
    auto wtPath = storage->userWavetablesPath;

    wtPath = Surge::Storage::getUserDefaultPath(storage, Surge::Storage::LastWavetablePath, wtPath);

    if (!sge)
    {
        return;
    }

    sge->fileChooser = std::make_unique<juce::FileChooser>(
        "Select Wavetable to Load", juce::File(path_to_string(wtPath)), "*.wav, *.wt");
    sge->fileChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, wtPath](const juce::FileChooser &c) {
            auto ress = c.getResults();

            if (ress.size() != 1)
            {
                return;
            }

            auto res = c.getResult();
            auto rString = res.getFullPathName().toStdString();

            this->oscdata->wt.queue_filename = rString;

            auto dir = string_to_path(res.getParentDirectory().getFullPathName().toStdString());

            if (dir != wtPath)
            {
                Surge::Storage::updateUserDefaultPath(storage, Surge::Storage::LastWavetablePath,
                                                      dir);
            }
        });
}

void OscillatorWaveformDisplay::showWavetableMenu(bool singleCategory)
{
    bool usesWT = uses_wavetabledata(oscdata->type.val.i);

    if (usesWT)
    {
        int id = oscdata->wt.current_id;
        juce::PopupMenu menu;

        populateMenu(menu, id, singleCategory);

        auto where = sge->frame->getLocalPoint(this, menuOverlays[0]->getBounds().getBottomLeft());

        menu.showMenuAsync(sge->popupMenuOptions(where));
    }
}

void OscillatorWaveformDisplay::mouseDown(const juce::MouseEvent &event)
{
    if (event.mods.isMiddleButtonDown() && sge)
    {
        sge->frame->mouseDown(event);

        return;
    }

    mouseDownLongHold(event);

    bool usesWT = uses_wavetabledata(oscdata->type.val.i);

    if (usesWT)
    {
        int id = oscdata->wt.current_id;
        bool openMenu = false;

        if (leftJog.contains(event.position))
        {
            if (!event.mods.isPopupMenu())
            {
                if (sge)
                    sge->undoManager()->pushWavetable(scene, oscInScene);
                id = storage->getAdjacentWaveTable(oscdata->wt.current_id, false);

                if (id >= 0)
                {
                    if (sge)
                    {
                        std::string announce = "Loaded wavetable is: ";
                        announce += storage->wt_list[id].name;
                        sge->enqueueAccessibleAnnouncement(announce);
                    }

                    oscdata->wt.queue_id = id;
                    auto new_name = storage->getCurrentWavetableName(oscdata);
                    SurgeSynthProcessor *ssp = &sge->juceEditor->processor;
                    ssp->paramChangeToListeners(nullptr, true, ssp->SCT_WAVETABLE, (float)scene,
                                                (float)oscInScene, (float)id, new_name);
                }
            }
            else
            {
                openMenu = true;
            }
        }
        else if (rightJog.contains(event.position))
        {
            if (!event.mods.isPopupMenu())
            {
                if (sge)
                    sge->undoManager()->pushWavetable(scene, oscInScene);

                id = storage->getAdjacentWaveTable(oscdata->wt.current_id, true);

                if (id >= 0)
                {
                    if (sge)
                    {
                        std::string announce = "Loaded wavetable is: ";
                        announce += storage->wt_list[id].name;
                        sge->enqueueAccessibleAnnouncement(announce);
                    }

                    oscdata->wt.queue_id = id;
                    auto new_name = storage->getCurrentWavetableName(oscdata);
                    SurgeSynthProcessor *ssp = &sge->juceEditor->processor;
                    ssp->paramChangeToListeners(nullptr, true, ssp->SCT_WAVETABLE, (float)scene,
                                                (float)oscInScene, (float)id, new_name);
                }
            }
            else
            {
                openMenu = true;
            }
        }

        if (waveTableName.contains(event.position) || openMenu)
        {
            showWavetableMenu(event.mods.isRightButtonDown());
        }
    }

    if (supportsCustomEditor() && customEditorBox.contains(event.position))
    {
        if (event.mods.isPopupMenu())
        {
            // internally already queries if we're using a wavetable-based osc
            createWTMenu(false);

            // we're using Alias now. TODO: this can probably be better/clearer
            if (!usesWT)
            {
                createAliasOptionsMenu(false, true);
            }

            return;
        }

        toggleCustomEditor();
    }
}

void OscillatorWaveformDisplay::mouseEnter(const juce::MouseEvent &event) { isMousedOver = true; }

void OscillatorWaveformDisplay::mouseMove(const juce::MouseEvent &event)
{
    mouseMoveLongHold(event);

    if (supportsCustomEditor())
    {
        auto q = customEditorBox.contains(event.position);

        if (q != isCustomEditorHovered)
        {
            isCustomEditorHovered = q;
            repaint();
        }
    }
    else
    {
        isCustomEditorHovered = false;
    }

    bool usesWT = uses_wavetabledata(oscdata->type.val.i);

    if (usesWT)
    {
        auto nwt = waveTableName.contains(event.position);
        auto njl = leftJog.contains(event.position);
        auto nrj = rightJog.contains(event.position);

        if (nwt != isWtNameHovered)
        {
            isWtNameHovered = nwt;
            repaint();
        }

        if (njl != isJogLHovered)
        {
            isJogLHovered = njl;
            repaint();
        }

        if (nrj != isJogRHovered)
        {
            isJogRHovered = nrj;
            repaint();
        }
    }
    else
    {
        isJogRHovered = false;
        isJogRHovered = false;
        isWtNameHovered = false;
    }
}

void OscillatorWaveformDisplay::mouseUp(const juce::MouseEvent &event) { mouseUpLongHold(event); }

std::string OscillatorWaveformDisplay::customEditorActionLabel(bool isActionToOpen) const
{
    if (oscdata->type.val.i == ot_alias &&
        oscdata->p[AliasOscillator::ao_wave].val.i == AliasOscillator::aow_additive)
    {
        return isActionToOpen ? "EDIT" : "CLOSE";
    }

    return "";
}

bool OscillatorWaveformDisplay::isCustomEditAccessible() const
{
    // WT display is purely visual so it doesn't have screen reader support
    if (oscdata->type.val.i == ot_alias &&
        oscdata->p[AliasOscillator::ao_wave].val.i == AliasOscillator::aow_additive)
    {
        return true;
    }

    return false;
}

bool OscillatorWaveformDisplay::supportsCustomEditor()
{
    if ((oscdata->type.val.i == ot_alias &&
         oscdata->p[AliasOscillator::ao_wave].val.i == AliasOscillator::aow_additive) ||
        uses_wavetabledata(oscdata->type.val.i))
    {
        return true;
    }

    return false;
}

struct WaveTable3DEditor;
template <> void LongHoldMixin<WaveTable3DEditor>::onLongHold();

struct WaveTable3DEditor : public juce::Component,
                           public Surge::GUI::SkinConsumingComponent,
                           public LongHoldMixin<WaveTable3DEditor>
{
    OscillatorStorage *oscdata;
    SurgeStorage *storage;
    OscillatorWaveformDisplay *parent;
    SurgeGUIEditor *sge;

    std::unique_ptr<juce::Image> backingImage;

    /*
        For Profiling!

        TRUE
        All graphics will be produced by the oscillator.process_block.

        FALSE
        Wt data will be used and processed by the oscillator
    */

    const bool PROCESS_WITH_OSC = false;

    float samplesCached[128][128];
    float morphValue = 0.0;

    int currentBreakingPoint = -1;

    union ParamCached
    {
        int i;
        float f;
    };

    ParamCached paramCached[7];
    float zoomFactor = -1.0;
    bool hasResized = false;

    /* Values exported from
     * https://www.blancoberg.com/surgewt3d?json=%7B%22rendered_frames%22%3A40%2C%22rendered_samples%22%3A53%2C%22paddingX%22%3A0.017679932260795936%2C%22paddingY%22%3A0.2127688399661304%2C%22skew%22%3A0.3%2C%22perspective%22%3A0%2C%22amplitude%22%3A0.14970364098221847%2C%22adjustX%22%3A0%2C%22adjustY%22%3A0%2C%22thickness%22%3A0.3989839119390347%2C%22morph%22%3A0%7D
     */

    int w, h;
    float wf, hf;
    int wt_size = 0;
    int wt_nframes = 64;
    int rendered_frames = 40;
    int rendered_samples = 60;
    float paddingX = 0.017679932260795936;
    float paddingY = 0.2127688399661304;
    float skew = 0.3;
    float perspective = 0;
    float amplitude = 0.14970364098221847;
    float morph = 0;
    float thickness = 0.3191871295512278;
    float adjustX = 0;
    float adjustY = 0;

    //////////////////////////////////////////////////

    WaveTable3DEditor(OscillatorWaveformDisplay *pD, SurgeStorage *s, OscillatorStorage *osc,
                      SurgeGUIEditor *ed)
        : parent(pD), storage(s), oscdata(osc), sge(ed)
    {
        clearSampleCache();
    }

    void cacheParams()
    {
        for (int i = 0; i < 7; i++)
        {
            this->paramCached[i].i = oscdata->p[i].val.i;
            this->paramCached[i].f = oscdata->p[i].val.f;
        }
    }

    void clearSampleCache()
    {
        for (int i = 0; i < 128; i++)
        {
            samplesCached[i][0] = 0.f;
        }
    }

    bool paramsHaveChanged()
    {

        for (int i = 0; i < 7; i++)
        {
            if (oscdata->p[i].val.i != this->paramCached[i].i ||
                oscdata->p[i].val.f != this->paramCached[i].f)
            {
                cacheParams();
                return true;
            }
        }
        return false;
    }

    float getCurrentZoomFactor()
    {
        float zoom = (float)sge->getZoomFactor() * 0.01;
        return zoom;
    }

    void resized() override
    {
        float currentZoom = getCurrentZoomFactor();

        if (currentZoom != zoomFactor)
        {
            zoomFactor = currentZoom;
            hasResized = true;
            w = getWidth();
            h = getHeight();

            wf = (float)w * zoomFactor * 2;
            hf = (float)h * zoomFactor * 2;

            backingImage = nullptr;
            backingImage = std::make_unique<juce::Image>(juce::Image::PixelFormat::ARGB, (int)wf,
                                                         (int)(hf), true);
        }
    }

    /*
        create a interpolated frame based on a float frame value
    */

    void getInterpFrame(float *samples, float frame, float frames, bool processForReal,
                        bool useCache)
    {
        auto &wt = oscdata->wt;
        float actualFrame = (frame / (frames - 1.f)) * (float)(wt_nframes - 1);
        int frameFrom = (int)(actualFrame);
        float proc = actualFrame - (float)frameFrom;

        if (processForReal == false)
        {
            if (samplesCached[(int)frame][0] == 0.f || useCache == false)
            {
                int frameTo = frameFrom + 1 > wt_nframes - 1 ? frameFrom : frameFrom + 1;

                for (int i = 0; i < rendered_samples; i++)
                {

                    int pos = floor((((float)i) / (float)std::max(rendered_samples - 1, 1)) *
                                    ((float)wt_size - 1.f));

                    samples[i] = wt.TableF32WeakPointers[0][frameFrom][pos] * (1.f - proc) +
                                 wt.TableF32WeakPointers[0][frameTo][pos] * proc;
                    if (useCache)
                        samplesCached[(int)frame][i + 1] = samples[i];
                }

                samplesCached[(int)frame][0] = 1.f;
            }
            else
            {
                for (int i = 0; i < rendered_samples; i++)
                {
                    samples[i] = samplesCached[(int)frame][i + 1];
                }
            }
        }
    }

    void drawWavetable(juce::Graphics &g)
    {
        auto osc = parent->setupOscillator();
        osc->init(0, true, false);

        float opacity = 0.666;
        float opacityMultiplier = parent->isMuted ? 0.5f : 1.f;

        // draw the base layer
        if (paramsHaveChanged() || hasResized)
        {
            hasResized = false;
            juce::Colour color =
                juce::Colour((unsigned char)0, (unsigned char)0, (unsigned char)0, 0.f);

            backingImage->clear(
                juce::Rectangle(0, 0, int(zoomFactor * w * 2), int(zoomFactor * h * 2)), color);
            auto g = juce::Graphics(*backingImage);

            for (int i = 0; i < rendered_frames; i++)
            {
                float proc = (float)i / (float)rendered_frames;

                drawWaveform(
                    g, i, rendered_frames, skin->getColor(Colors::Osc::Display::WaveCurrent3D),
                    (0.9 - 0.7 * proc) * opacity * opacityMultiplier * 0.5, 1, osc, 1.f, true);
            }

            // on lower frames, draw the actual frames on top of the base shape
            if (wt_nframes <= 10 && wt_nframes > 1)
            {
                opacity = 0.5;

                for (int i = 0; i < wt_nframes; i++)
                {
                    float proc = (float)i / (float)rendered_frames;
                    int fr = (float)i / ((float)wt_nframes - 1) * ((float)rendered_frames - 1.f);

                    drawWaveform(g, fr, rendered_frames,
                                 skin->getColor(Colors::Osc::Display::WaveCurrent3D),
                                 (0.9 - 0.7 * proc) * opacity * opacityMultiplier * 0.5, 1.0, osc,
                                 1.f, true);
                }
            }

            // draw the wavetable position (morph)
            // do not use cache for a smoother motion
            float pos = morphValue;
            int position = pos * (128 - 1);

            drawWaveform(g, position, 128, skin->getColor(Colors::Osc::Display::WaveCurrent3D),
                         0.9 * opacityMultiplier, 1.5, osc, 1.f, false);
        }

        // draw backing image to graphics context
        g.drawImage(*backingImage, getLocalBounds().toFloat(),
                    juce::RectanglePlacement::fillDestination);
    }

    void drawWaveform(juce::Graphics &g, int i, int renderedFrames, juce::Colour color,
                      float opacity, float thick, ::Oscillator *osc, float scale, bool useCache)
    {

        bool processForReal = PROCESS_WITH_OSC;

        juce::Path p;
        float procFrames = ((float)i) / (float)(renderedFrames - 1);

        float samples[64];

        getInterpFrame(samples, (float)i, (float)renderedFrames, processForReal, useCache);

        osc->processSamplesForDisplay(samples, rendered_samples, processForReal);

        float xScaled = wf * scale;
        float YScaled = hf * scale;

        float skewCalc = skew;

        for (int k = 0; k < rendered_samples; k++)
        {

            float proc = (float)k / ((float)rendered_samples - 1.f);

            float x = xScaled *
                      (adjustX + 0.5 * perspective * procFrames + paddingX + skewCalc * procFrames +
                       proc * (1 - paddingX * 2.0 - skewCalc - perspective * procFrames));
            x += perspective * skewCalc * wf * 0.5;

            float y = adjustY * YScaled + YScaled - paddingY * YScaled -
                      (YScaled - paddingY * 2 * YScaled) * procFrames +
                      samples[k] * YScaled * -(amplitude * (1 - 0.2 * perspective * procFrames)) *
                          (1 - perspective * procFrames);

            if (k == 0)
            {
                p.startNewSubPath(x, y);
            }
            else
            {
                p.lineTo(x, y);
            }
        }

        g.setColour(color.withMultipliedAlpha(opacity));
        g.strokePath(p, juce::PathStrokeType(thickness * xScaled * 0.01 * 2 * thick));
    }

    void paint(juce::Graphics &g) override
    {
        auto &wt = oscdata->wt;
        wt_size = wt.size;
        wt_nframes = wt.n_tables;

        morphValue = oscdata->p[0].val.f;

        drawWavetable(g);
    }

    void mouseDown(const juce::MouseEvent &event) override
    {
        mouseDownLongHold(event);

        if (event.mods.isPopupMenu())
        {
            parent->createWTMenu(false);
            return;
        }

        juce::Timer::callAfterDelay(1, [that = juce::Component::SafePointer(parent)] {
            if (that)
            {
                that->hideCustomEditor();
                Surge::Storage::updateUserDefaultValue(that->storage,
                                                       Surge::Storage::Use3DWavetableView, false);
            }
        });
    }

    void mouseUp(const juce::MouseEvent &event) override { mouseUpLongHold(event); }

    void mouseEnter(const juce::MouseEvent &event) override
    {
        parent->isCustomEditorHovered = true;
    }
    void mouseExit(const juce::MouseEvent &event) override
    {
        parent->isCustomEditorHovered = false;
    }
};

template <> void LongHoldMixin<WaveTable3DEditor>::onLongHold() { asT()->parent->createWTMenu(); }

struct AliasAdditiveEditor;
template <> void LongHoldMixin<AliasAdditiveEditor>::onLongHold();

struct AliasAdditiveEditor : public juce::Component,
                             public Surge::GUI::SkinConsumingComponent,
                             public LongHoldMixin<AliasAdditiveEditor>,
                             public Surge::GUI::Hoverable
{
    AliasAdditiveEditor(OscillatorWaveformDisplay *pD, SurgeStorage *s, OscillatorStorage *osc,
                        SurgeGUIEditor *ed, int sc, int os)
        : parent(pD), storage(s), oscdata(osc), sge(ed), scene(sc), oscInScene(os)
    {
        for (int i = 0; i < AliasOscillator::n_additive_partials; ++i)
        {
            std::string sn = "Harmonic " + std::to_string(i + 1);
            auto q = std::make_unique<OverlayAsAccessibleSlider<AliasAdditiveEditor>>(this, sn);

            q->onGetValue = [this, i](auto *T) {
                auto v = limitpm1(oscdata->extraConfig.data[i]);
                return v;
            };

            q->onSetValue = [this, i](auto *T, float f) {
                sge->undoManager()->pushOscillatorExtraConfig(scene, oscInScene);

                oscdata->extraConfig.data[i] = limitpm1(f);
                storage->getPatch().isDirty = true;
                repaint();
            };

            q->onJogValue = [this, i](auto *t, int dir, bool isShift, bool isControl) {
                auto v = limitpm1(oscdata->extraConfig.data[i]);
                auto fac = 0.05;

                if (isShift)
                {
                    fac = 0.01;
                }

                sge->undoManager()->pushOscillatorExtraConfig(scene, oscInScene);
                oscdata->extraConfig.data[i] = limitpm1(v + fac * dir);
                storage->getPatch().isDirty = true;
                repaint();
            };

            q->onMinMaxDef = [this, i](auto *t, int mmd) {
                auto val = 0.f;

                if (mmd == 1)
                {
                    val = 1;
                }

                if (mmd == -1)
                {
                    val = -1;
                }

                sge->undoManager()->pushOscillatorExtraConfig(scene, oscInScene);
                oscdata->extraConfig.data[i] = limitpm1(val);
                storage->getPatch().isDirty = true;
                repaint();
            };

            q->onMenuKey = [this](auto *t) {
                parent->createAliasOptionsMenu();
                return true;
            };

            addAndMakeVisible(*q);

            sliderAccOverlays[i] = std::move(q);
        }

        setAccessible(true);
        setTitle("Alias Additive Editor");
        setDescription("Alias Additive Editor");
        setFocusContainerType(juce::Component::FocusContainerType::focusContainer);
    }

    OscillatorStorage *oscdata;
    OscillatorWaveformDisplay *parent;
    SurgeStorage *storage;
    SurgeGUIEditor *sge;
    int scene;
    int oscInScene;
    int topTrim = 11;

    std::array<juce::Rectangle<float>, AliasOscillator::n_additive_partials> sliders;
    std::array<std::unique_ptr<OverlayAsAccessibleSlider<AliasAdditiveEditor>>,
               AliasOscillator::n_additive_partials>
        sliderAccOverlays;

    void resized() override
    {
        auto w = 1.f * getWidth() / AliasOscillator::n_additive_partials;

        for (int i = 0; i < AliasOscillator::n_additive_partials; ++i)
        {
            auto p = juce::Rectangle<float>(i * w, topTrim, w, getHeight() - topTrim);

            sliders[i] = p;
            sliderAccOverlays[i]->setBounds(p.toNearestInt());
        }
    }

    void paint(juce::Graphics &g) override
    {
        auto w = 1.f * getWidth() / AliasOscillator::n_additive_partials;
        float halfHeight = sliders[0].getHeight() / 2.f;

        for (int i = 0; i < AliasOscillator::n_additive_partials; ++i)
        {
            auto v = limitpm1(oscdata->extraConfig.data[i]);
            auto p = sliders[i];
            auto bar = sliders[i];

            if (v < 0)
            {
                bar = p.withTop(halfHeight).withBottom(halfHeight * (1 - v));
            }
            else
            {
                bar = p.withBottom(halfHeight).withTop(halfHeight * (1 - v));
            }

            g.setColour(skin->getColor(Colors::Osc::Display::Wave)
                            .withMultipliedAlpha(parent->isMuted ? 0.5f : 1.f));
            g.fillRect(bar.translated(0, topTrim));
            g.setColour(skin->getColor(Colors::Osc::Display::Bounds));
            g.drawRect(sliders[i]);
        }

        g.drawLine(0.f, halfHeight + topTrim, getWidth(), halfHeight + topTrim);
    }

    void mouseDown(const juce::MouseEvent &event) override
    {
        mouseDownLongHold(event);

        if (event.mods.isPopupMenu())
        {
            parent->createAliasOptionsMenu(false);
            return;
        }

        if (event.mods.isLeftButtonDown())
        {
            int clickedSlider = -1;

            for (int i = 0; i < AliasOscillator::n_additive_partials; ++i)
            {
                if (sliders[i].contains(event.position))
                {
                    clickedSlider = i;
                }
            }

            if (clickedSlider >= 0)
            {
                sge->undoManager()->pushOscillatorExtraConfig(scene, oscInScene);
                storage->getPatch().isDirty = true;

                auto pos = (event.position.y - topTrim) / sliders[clickedSlider].getHeight();
                auto d = (-1.f * pos + 0.5) * 2 * (!event.mods.isCommandDown());

                oscdata->extraConfig.data[clickedSlider] = limitpm1(d);

                repaint();
            }
        }
    }

    void mouseUp(const juce::MouseEvent &event) override
    {
        mouseUpLongHold(event);

        if (event.mouseWasDraggedSinceMouseDown())
        {
            if (!Surge::GUI::showCursor(storage))
            {
                juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(
                    false);
            }
        }
    }

    void mouseDoubleClick(const juce::MouseEvent &event) override
    {
        if (event.mods.isMiddleButtonDown())
        {
            return;
        }

        int clickedSlider = -1;

        for (int i = 0; i < AliasOscillator::n_additive_partials; ++i)
        {
            if (sliders[i].contains(event.position))
            {
                clickedSlider = i;
            }
        }

        if (clickedSlider >= 0)
        {
            sge->undoManager()->pushOscillatorExtraConfig(scene, oscInScene);
            storage->getPatch().isDirty = true;

            oscdata->extraConfig.data[clickedSlider] = 0.f;

            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent &event) override
    {
        if (event.mods.isMiddleButtonDown())
        {
            return;
        }

        mouseDragLongHold(event);

        if (!Surge::GUI::showCursor(storage))
        {
            juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(true);
        }

        int draggedSlider = -1;
        bool yPosActivity = false;

        for (int i = 0; i < AliasOscillator::n_additive_partials; ++i)
        {
            if (event.position.x >= sliders[i].getX() &&
                event.position.x < sliders[i].getX() + sliders[i].getWidth())
            {
                draggedSlider = i;
            }

            if (event.position.y >= sliders[i].getY() &&
                event.position.y < sliders[i].getY() + sliders[i].getHeight())
            {
                yPosActivity = true;

                if (event.position.x < sliders[0].getX())
                {
                    draggedSlider = 0;
                }

                int lastPartial = AliasOscillator::n_additive_partials - 1;

                if (event.position.x >= sliders[lastPartial].getX())
                {
                    draggedSlider = lastPartial;
                }
            }
        }

        if (draggedSlider >= 0 || yPosActivity)
        {
            sge->undoManager()->pushOscillatorExtraConfig(scene, oscInScene);
            storage->getPatch().isDirty = true;

            auto pos = (event.position.y - topTrim) / sliders[draggedSlider].getHeight();
            auto d = (-1.f * pos + 0.5) * 2 * (!event.mods.isCommandDown());

            oscdata->extraConfig.data[draggedSlider] = limitpm1(d);

            repaint();
        }
    }

    void mouseWheelMove(const juce::MouseEvent &event,
                        const juce::MouseWheelDetails &wheel) override
    {
        // If I choose based on horiz/vert it only works on trackpads, so just add
        float delta = wheel.deltaX - (wheel.isReversed ? 1 : -1) * wheel.deltaY;

        if (delta == 0)
        {
            return;
        }

#if MAC
        float speed = 1.2;
#else
        float speed = 0.42666;
#endif

        if (event.mods.isShiftDown())
        {
            speed /= 10.f;
        }

        delta *= speed;

        int draggedSlider = -1;

        for (int i = 0; i < AliasOscillator::n_additive_partials; ++i)
        {
            if (sliders[i].contains(event.position))
            {
                draggedSlider = i;
            }
        }

        if (draggedSlider >= 0)
        {
            sge->undoManager()->pushOscillatorExtraConfig(scene, oscInScene);
            storage->getPatch().isDirty = true;

            auto d = oscdata->extraConfig.data[draggedSlider] + delta;

            oscdata->extraConfig.data[draggedSlider] = limitpm1(d);
            repaint();
        }
    }

    bool isHovered{false};
    void mouseEnter(const juce::MouseEvent &) override { isHovered = true; }
    void mouseExit(const juce::MouseEvent &) override { isHovered = false; }
    bool isCurrentlyHovered() override { return isHovered; }

    bool keyPressed(const juce::KeyPress &key) override
    {
        auto [action, mod] = Surge::Widgets::accessibleEditAction(key, storage);

        if (action == None)
            return false;

        if (action == OpenMenu)
        {
            parent->createAliasOptionsMenu();
            return true;
        }

        return false;
    }
};

template <> void LongHoldMixin<AliasAdditiveEditor>::onLongHold()
{
    asT()->parent->createAliasOptionsMenu();
}

void OscillatorWaveformDisplay::showCustomEditor()
{
    if (customEditor)
    {
        hideCustomEditor();
    }

    if (oscdata->type.val.i == ot_alias &&
        oscdata->p[AliasOscillator::ao_wave].val.i == AliasOscillator::aow_additive)
    {
        auto ed =
            std::make_unique<AliasAdditiveEditor>(this, storage, oscdata, sge, scene, oscInScene);
        ed->setSkin(skin, associatedBitmapStore);
        customEditor = std::move(ed);
    }

    if (uses_wavetabledata(oscdata->type.val.i))
    {
        auto ed = std::make_unique<WaveTable3DEditor>(this, storage, oscdata, sge);
        ed->setSkin(skin, associatedBitmapStore);
        customEditor = std::move(ed);

        Surge::Storage::updateUserDefaultValue(storage, Surge::Storage::Use3DWavetableView, true);
    }

    if (customEditor)
    {
        auto b = getLocalBounds().withTrimmedBottom(wtbheight);
        customEditor->setBounds(b);
        addAndMakeVisible(*customEditor);
        customEditor->resized(); // Resize needs to happen after it has been added to the scene.
                                 // Otherwise bounds will return 0
        repaint();

        customEditorAccOverlay->setTitle("Close Custom Editor");
        customEditorAccOverlay->setDescription("Close Custom Editor");
    }

    if (auto h = getAccessibilityHandler())
    {
        h->notifyAccessibilityEvent(juce::AccessibilityEvent::structureChanged);
    }
}

void OscillatorWaveformDisplay::hideCustomEditor()
{
    if (customEditor)
    {
        removeChildComponent(customEditor.get());
    }

    customEditor.reset(nullptr);

    customEditorAccOverlay->setTitle("Open Custom Editor");
    customEditorAccOverlay->setDescription("Open Custom Editor");

    if (auto h = getAccessibilityHandler())
    {
        h->notifyAccessibilityEvent(juce::AccessibilityEvent::structureChanged);
    }

    repaint();
}

void OscillatorWaveformDisplay::toggleCustomEditor()
{
    if (customEditor)
    {
        hideCustomEditor();
    }
    else
    {
        showCustomEditor();
    }
}

void OscillatorWaveformDisplay::drawEditorBox(juce::Graphics &g, const std::string &s)
{
    customEditorBox = getLocalBounds()
                          .withTop(getHeight() - wtbheight)
                          .withRight(60)
                          .withTrimmedBottom(1)
                          .toFloat();

    int fontsize = 9;
    bool makeTransparent = false;

    if (uses_wavetabledata(oscdata->type.val.i))
    {
        auto wtr = getLocalBounds().withTrimmedBottom(wtbheight).toFloat();

        customEditorBox = wtr;
        fontsize = 8;
        makeTransparent = true;
    }

    juce::Colour bg, outline, txt;

    if (isCustomEditorHovered)
    {
        bg = skin->getColor(Colors::Osc::Filename::BackgroundHover);
        outline = skin->getColor(Colors::Osc::Filename::FrameHover);
        txt = skin->getColor(Colors::Osc::Filename::TextHover);
    }
    else
    {
        bg = skin->getColor(Colors::Osc::Filename::Background);
        outline = skin->getColor(Colors::Osc::Filename::Frame);
        txt = skin->getColor(Colors::Osc::Filename::Text);
    }

    if (makeTransparent)
    {
        bg = bg.withAlpha(0.f);
        outline = outline.withAlpha(0.f);
        txt = txt.withAlpha(0.f);
    }

    g.setColour(bg);
    g.fillRect(customEditorBox);
    g.setColour(outline);
    g.drawRect(customEditorBox);
    g.setColour(txt);
    g.setFont(skin->fontManager->getLatoAtSize(fontsize));
    g.drawText(s, customEditorBox, juce::Justification::centred);
}

void OscillatorWaveformDisplay::mouseExit(const juce::MouseEvent &event)
{
    isCustomEditorHovered = false;
    isJogLHovered = false;
    isJogRHovered = false;
    isWtNameHovered = false;
    isMousedOver = false;
    repaint();
}

void OscillatorWaveformDisplay::onOscillatorTypeChanged()
{
    bool visWT = false;

    if (uses_wavetabledata(oscdata->type.val.i))
    {
        visWT = true;
    }

    auto visCC = isCustomEditAccessible();
    auto vis = visWT || visCC;

    setAccessible(vis);

    for (const auto &ao : menuOverlays)
    {
        ao->setVisible(visWT);
    }

    if (visWT)
    {
        bool is3D =
            Surge::Storage::getUserDefaultValue(storage, Surge::Storage::Use3DWavetableView, false);

        if (is3D)
        {
            showCustomEditor();
        }
        else
        {
            hideCustomEditor();
        }
    }
    else
    {
        if (customEditor)
        {
            hideCustomEditor();
        }
    }

    customEditorAccOverlay->setVisible(visCC);
}

std::unique_ptr<juce::AccessibilityHandler> OscillatorWaveformDisplay::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::group);
}

bool OscillatorWaveformDisplay::keyPressed(const juce::KeyPress &key)
{
    auto [action, mod] = Surge::Widgets::accessibleEditAction(key, storage);

    if (action == None)
    {
        return false;
    }

    if (action == OpenMenu)
    {
        if (isWtNameHovered || isJogLHovered || isJogRHovered)
        {
            showWavetableMenu();

            return true;
        }
        else
        {
            bool usesWT = uses_wavetabledata(oscdata->type.val.i);

            // internally already queries if we're using a wavetable-based osc
            createWTMenu();

            // we're using Alias now. TODO: this can probably be better/clearer
            if (!usesWT)
            {
                createAliasOptionsMenu(false, true);
            }

            return true;
        }
    }

    return false;
}

} // namespace Widgets
} // namespace Surge
