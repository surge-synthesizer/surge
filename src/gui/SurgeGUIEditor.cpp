/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "SurgeGUIEditor.h"
#include "resource.h"

#include "SurgeBitmaps.h"
#include "CScalableBitmap.h"

#include "UserDefaults.h"
#include "SkinSupport.h"
#include "SkinColors.h"
#include "UIInstrumentation.h"
#include "SurgeGUIUtils.h"
#include "DebugHelpers.h"
#include "StringOps.h"
#include "ModulatorPresetManager.h"

#include "SurgeSynthEditor.h"

#include "SurgeGUIEditorTags.h"

#include "overlays/AboutScreen.h"
#include "overlays/LuaEditors.h"
#include "overlays/MiniEdit.h"
#include "overlays/MSEGEditor.h"
#include "overlays/ModulationEditor.h"
#include "overlays/PatchDBViewer.h"
#include "overlays/TypeinParamEditor.h"
#include "overlays/OverlayWrapper.h"
#include "overlays/PatchStoreDialog.h"

#include "widgets/EffectLabel.h"
#include "widgets/EffectChooser.h"
#include "widgets/LFOAndStepDisplay.h"
#include "widgets/MainFrame.h"
#include "widgets/MenuForDiscreteParams.h"
#include "widgets/ModulationSourceButton.h"
#include "widgets/ModulatableSlider.h"
#include "widgets/MultiSwitch.h"
#include "widgets/NumberField.h"
#include "widgets/OscillatorWaveformDisplay.h"
#include "widgets/ParameterInfowindow.h"
#include "widgets/PatchSelector.h"
#include "widgets/Switch.h"
#include "widgets/VerticalLabel.h"
#include "widgets/VuMeter.h"
#include "widgets/XMLConfiguredMenus.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <stack>
#include <numeric>
#include <unordered_map>
#include <codecvt>
#include "version.h"
#include "ModernOscillator.h"
#include "libMTSClient.h"

#include "filesystem/import.h"
#include "RuntimeFont.h"

const int yofs = 10;

using namespace VSTGUI;
using namespace std;

int SurgeGUIEditor::start_paramtag_value = start_paramtags;

bool SurgeGUIEditor::fromSynthGUITag(SurgeSynthesizer *synth, int tag, SurgeSynthesizer::ID &q)
{
    // This is wrong for macros and params but is close
    return synth->fromSynthSideIdWithGuiOffset(tag, start_paramtags, tag_mod_source0 + ms_ctrl1, q);
}

SurgeGUIEditor::SurgeGUIEditor(SurgeSynthEditor *jEd, SurgeSynthesizer *synth)
{
    synth->storage.addErrorListener(this);
    synth->storage.okCancelProvider = [this](const std::string &msg, const std::string &title,
                                             SurgeStorage::OkCancel def) {
        // think about threading one day probably
        auto res = juce::AlertWindow::showOkCancelBox(juce::AlertWindow::InfoIcon, title, msg, "OK",
                                                      "Cancel", nullptr, nullptr);

        if (res)
            return SurgeStorage::OK;
        return SurgeStorage::CANCEL;
    };
#ifdef INSTRUMENT_UI
    Surge::Debug::record("SurgeGUIEditor::SurgeGUIEditor");
#endif
    frame = 0;

    blinktimer = 0.f;
    blinkstate = false;
    midiLearnOverlay = nullptr;
    patchCountdown = -1;

    mod_editor = false;

    // init the size of the plugin
    initialZoomFactor =
        Surge::Storage::getUserDefaultValue(&(synth->storage), Surge::Storage::DefaultZoom, 100);
    int instanceZoomFactor = synth->storage.getPatch().dawExtraState.editor.instanceZoomFactor;
    if (instanceZoomFactor > 0)
    {
        // dawExtraState zoomFactor wins defaultZoom
        initialZoomFactor = instanceZoomFactor;
    }

    editor_open = false;
    queue_refresh = false;
    memset(param, 0, n_paramslots * sizeof(void *));
    polydisp =
        0; // FIXME - when changing skins and rebuilding we need to reset these state variables too
    splitpointControl = 0;
    for (int i = 0; i < n_fx_slots; ++i)
    {
        selectedFX[i] = -1;
        fxPresetName[i] = "";
    }

    juceEditor = jEd;
    this->synth = synth;

    minimumZoom = 50;
#if LINUX
    minimumZoom = 100; // See github issue #628
#endif

    zoom_callback = [](SurgeGUIEditor *f, bool) {};
    setZoomFactor(initialZoomFactor);
    zoomInvalid = (initialZoomFactor != 100);

    for (int i = 0; i < n_modsources; ++i)
        modsource_is_alternate[i] = false;

    currentSkin = Surge::GUI::SkinDB::get().defaultSkin(&(this->synth->storage));
    reloadFromSkin();

    auto des = &(synth->storage.getPatch().dawExtraState);
    if (des->isPopulated)
        loadFromDAWExtraState(synth);

    paramInfowindow = std::make_unique<Surge::Widgets::ParameterInfowindow>();
    paramInfowindow->setVisible(false);

    typeinParamEditor = std::make_unique<Surge::Overlays::TypeinParamEditor>();
    typeinParamEditor->setVisible(false);
    typeinParamEditor->setSurgeGUIEditor(this);

    miniEdit = std::make_unique<Surge::Overlays::MiniEdit>();
    miniEdit->setVisible(false);
}

SurgeGUIEditor::~SurgeGUIEditor()
{
    synth->storage.clearOkCancelProvider();
    auto isPop = synth->storage.getPatch().dawExtraState.isPopulated;
    populateDawExtraState(synth); // If I must die, leave my state for future generations
    synth->storage.getPatch().dawExtraState.isPopulated = isPop;
    synth->storage.removeErrorListener(this);
}

void SurgeGUIEditor::idle()
{
    if (!synth)
        return;

    if (pause_idle_updates)
        return;

    if (errorItemCount)
    {
        std::vector<std::pair<std::string, std::string>> cp;
        {
            std::lock_guard<std::mutex> g(errorItemsMutex);
            cp = errorItems;
            errorItems.clear();
        }
        for (const auto &p : cp)
        {
            juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon, p.second, p.first);
        }
    }

    if (editor_open && frame && !synth->halt_engine)
    {
        idleInfowindow();
        /*
         * USEFUL for testing stress patch changes
         *
        static int runct = 0;
        if( runct++ == 5 )
        {
           synth->patchid_queue = rand() % 1800;
           runct = 0;
        }
          */
        if (firstIdleCountdown)
        {
            // Linux VST3 in JUCE Hosts (maybe others?) sets up the run loop out of order, it seems
            // sometimes missing the very first invalidation. Force a redraw on the first idle
            // isFirstIdle = false;
            firstIdleCountdown--;

            frame->repaint();
        }
        if (synth->learn_param < 0 && synth->learn_custom < 0 && midiLearnOverlay != nullptr)
        {
            hideMidiLearnOverlay();
        }

        {
            bool expected = true;
            if (synth->rawLoadNeedsUIDawExtraState.compare_exchange_weak(expected, true) &&
                expected)
            {
                std::lock_guard<std::mutex> g(synth->rawLoadQueueMutex);
                synth->rawLoadNeedsUIDawExtraState = false;
                loadFromDAWExtraState(synth);
            }
        }

        if (patchCountdown >= 0 && !pause_idle_updates)
        {
            patchCountdown--;
            if (patchCountdown < 0 && synth->patchid_queue >= 0)
            {
                std::ostringstream oss;

                oss << "Loading patch " << synth->patchid_queue
                    << " has not occured after 200 idle cycles. This means"
                    << "that the audio system is not running in your host, or that your system is "
                       "delayed while"
                    << " loading many patches in a row. The audio system has to be "
                    << "running in order to load Surge patches. If the audio system is working, "
                       "you can probably"
                    << " ignore this message and continue once Surge catches up.";
                synth->storage.reportError(oss.str(), "Patch Loading Error");
            }
        }

        if (zoomInvalid)
        {
            setZoomFactor(getZoomFactor());
            zoomInvalid = false;
        }

        if (showMSEGEditorOnNextIdleOrOpen)
        {
            showMSEGEditor();
            showMSEGEditorOnNextIdleOrOpen = false;
        }

        if (synth->storage.getPatch()
                .scene[current_scene]
                .osc[current_osc[current_scene]]
                .wt.refresh_display)
        {
            synth->storage.getPatch()
                .scene[current_scene]
                .osc[current_osc[current_scene]]
                .wt.refresh_display = false;
            if (oscWaveform)
            {
                oscWaveform->repaint();
            }
        }

        if (polydisp)
        {
            int prior = polydisp->getPlayingVoiceCount();
            if (prior != synth->polydisplay)
            {
                polydisp->setPlayingVoiceCount(synth->polydisplay);
                polydisp->repaint();
            }
        }

        bool patchChanged = false;
        if (patchSelector)
        {
            patchChanged = patchSelector->sel_id != synth->patchid;
        }

        if (statusMPE)
        {
            auto v = statusMPE->getValue();
            if ((v < 0.5 && synth->mpeEnabled) || (v > 0.5 && !synth->mpeEnabled))
            {
                statusMPE->setValue(synth->mpeEnabled ? 1 : 0);
                statusMPE->asJuceComponent()->repaint();
            }
        }

        if (statusTune)
        {
            auto v = statusTune->getValue();
            if ((v < 0.5 && !synth->storage.isStandardTuning) ||
                (v > 0.5 && synth->storage.isStandardTuning))
            {
                bool hasmts =
                    synth->storage.oddsound_mts_client && synth->storage.oddsound_mts_active;
                statusTune->setValue(!synth->storage.isStandardTuning || hasmts);
                statusTune->asJuceComponent()->repaint();
            }
        }

        if (patchChanged)
        {
            for (int i = 0; i < n_fx_slots; ++i)
            {
                fxPresetName[i] = "";
            }
        }

        if (queue_refresh || synth->refresh_editor || patchChanged)
        {
            queue_refresh = false;
            synth->refresh_editor = false;

            if (frame)
            {
                if (synth->patch_loaded)
                    mod_editor = false;
                synth->patch_loaded = false;

                openOrRecreateEditor();
            }
            if (patchSelector)
            {
                patchSelector->sel_id = synth->patchid;
                patchSelector->setLabel(synth->storage.getPatch().name);
                patchSelector->setCategory(synth->storage.getPatch().category);
                patchSelector->setAuthor(synth->storage.getPatch().author);
                patchSelector->repaint();
            }
        }

        bool vuInvalid = false;
        if (synth->vu_peak[0] != vu[0]->getValue())
        {
            vuInvalid = true;
            vu[0]->setValue(synth->vu_peak[0]);
        }
        if (synth->vu_peak[1] != vu[0]->getValueR())
        {
            vu[0]->setValueR(synth->vu_peak[1]);
            vuInvalid = true;
        }
        if (vuInvalid)
            vu[0]->repaint();

        for (int i = 0; i < n_fx_slots; i++)
        {
            assert(i + 1 < Effect::KNumVuSlots);
            if (vu[i + 1] && synth->fx[current_fx])
            {
                vu[i + 1]->setValue(synth->fx[current_fx]->vu[(i << 1)]);
                vu[i + 1]->setValueR(synth->fx[current_fx]->vu[(i << 1) + 1]);
                vu[i + 1]->repaint();
            }
        }

        for (int i = 0; i < 8; i++)
        {
            if (synth->refresh_ctrl_queue[i] >= 0)
            {
                int j = synth->refresh_ctrl_queue[i];
                synth->refresh_ctrl_queue[i] = -1;

                if (param[j])
                {
                    char pname[256], pdisp[256];
                    SurgeSynthesizer::ID jid;
                    if (synth->fromSynthSideId(j, jid))
                    {
                        synth->getParameterName(jid, pname);
                        synth->getParameterDisplay(jid, pdisp);
                    }

                    param[j]->asControlValueInterface()->setValue(
                        synth->refresh_ctrl_queue_value[i]);
                    param[j]->setQuantitizedDisplayValue(synth->refresh_ctrl_queue_value[i]);
                    param[j]->asJuceComponent()->repaint();
                    // oscdisplay->invalid();

                    if (oscWaveform)
                    {
                        oscWaveform->repaintIfIdIsInRange(j);
                    }

                    if (lfoDisplay)
                    {
                        lfoDisplay->invalidateIfIdIsInRange(j);
                    }
                }
            }
        }

        if (lastTempo != synth->time_data.tempo || lastTSNum != synth->time_data.timeSigNumerator ||
            lastTSDen != synth->time_data.timeSigDenominator)
        {
            lastTempo = synth->time_data.tempo;
            lastTSNum = synth->time_data.timeSigNumerator;
            lastTSDen = synth->time_data.timeSigDenominator;
            if (lfoDisplay)
            {
                lfoDisplay->setTimeSignature(synth->time_data.timeSigNumerator,
                                             synth->time_data.timeSigDenominator);
                lfoDisplay->invalidateIfAnythingIsTemposynced();
            }
        }

        std::vector<int> refreshIndices;
        if (synth->refresh_overflow)
        {
            refreshIndices.resize(n_total_params);
            std::iota(std::begin(refreshIndices), std::end(refreshIndices), 0);
            frame->repaint();
        }
        else
        {
            for (int i = 0; i < 8; ++i)
                if (synth->refresh_parameter_queue[i] >= 0)
                    refreshIndices.push_back(synth->refresh_parameter_queue[i]);
        }

        synth->refresh_overflow = false;
        for (int i = 0; i < 8; ++i)
            synth->refresh_parameter_queue[i] = -1;

        for (auto j : refreshIndices)
        {
            if ((j < n_total_params) && param[j])
            {
                SurgeSynthesizer::ID jid;
                if (synth->fromSynthSideId(j, jid))
                {
                    param[j]->asControlValueInterface()->setValue(synth->getParameter01(jid));
                    param[j]->setQuantitizedDisplayValue(synth->getParameter01(jid));
                    param[j]->asJuceComponent()->repaint();
                }

                if (oscWaveform)
                {
                    oscWaveform->repaintIfIdIsInRange(j);
                }

                if (lfoDisplay)
                {
                    lfoDisplay->invalidateIfIdIsInRange(j);
                }
            }
            else if ((j >= metaparam_offset) && (j < (metaparam_offset + n_customcontrollers)))
            {
                int cc = j - metaparam_offset;
                gui_modsrc[ms_ctrl1 + cc]->setValue(
                    ((ControllerModulationSource *)synth->storage.getPatch()
                         .scene[current_scene]
                         .modsources[ms_ctrl1 + cc])
                        ->get_target01());
            }
            else if ((j >= 0) && (j < n_total_params) && nonmod_param[j])
            {
                /*
                ** What the heck is this NONMOD_PARAM thing?
                **
                ** There are a set of params - like discrete things like
                ** octave and filter type - which are not LFO modulatable
                ** and aren't in the params[] array. But they are exposed
                ** properties, so you can control them from a DAW. The
                ** DAW control works - everything up to this path (as described
                ** in #160) works fine and sets the value but since there's
                ** no CControl in param the above bails out. But ading
                ** all these controls to param[] would have the unintended
                ** side effect of giving them all the other param[] behaviours.
                ** So have a second array and drop select items in here so we
                ** can actually get them redrawing when an external param set occurs.
                */
                auto *cc = nonmod_param[j];

                /*
                ** Some state changes enable and disable sliders. If this is one of those state
                *changes and a value has changed, then
                ** we need to invalidate them. See #2056.
                */
                auto tag = cc->getTag();
                SurgeSynthesizer::ID jid;

                auto sv = 0.f;
                if (synth->fromSynthSideId(j, jid))
                    sv = synth->getParameter01(jid);
                auto cv = cc->getValue();

                if ((sv != cv) && ((tag == fmconfig_tag || tag == filterblock_tag)))
                {
                    std::unordered_map<int, bool> resetMap;

                    if (tag == fmconfig_tag)
                    {
                        auto targetTag =
                            synth->storage.getPatch().scene[current_scene].fm_depth.id +
                            start_paramtags;
                        auto targetState =
                            (Parameter::intUnscaledFromFloat(sv, n_fm_routings - 1) == fm_off);
                        resetMap[targetTag] = targetState;
                    }

                    if (tag == filterblock_tag)
                    {
                        auto pval = Parameter::intUnscaledFromFloat(sv, n_filter_configs - 1);

                        auto targetTag =
                            synth->storage.getPatch().scene[current_scene].feedback.id +
                            start_paramtags;
                        auto targetState = (pval == fc_serial1);
                        resetMap[targetTag] = targetState;

                        targetTag = synth->storage.getPatch().scene[current_scene].width.id +
                                    start_paramtags;
                        targetState = (pval != fc_stereo && pval != fc_wide);
                        resetMap[targetTag] = targetState;
                    }

                    for (int i = 0; i < n_paramslots; i++)
                    {
                        if (param[i] &&
                            (resetMap.find(param[i]->asControlValueInterface()->getTag()) !=
                             resetMap.end()))
                        {
                            param[i]->setDeactivated(
                                resetMap[param[i]->asControlValueInterface()->getTag()]);
                            param[i]->asJuceComponent()->repaint();
                        }
                    }
                }

                if (synth->storage.getPatch().param_ptr[j]->ctrltype == ct_scenemode)
                {
                    /*
                     * This is gross hack for our reordering of scenemode. Basically take the
                     * automation value and turn it into the UI value
                     */
                    auto pval = Parameter::intUnscaledFromFloat(sv, n_scene_modes - 1);
                    if (pval == sm_dual)
                        pval = sm_chsplit;
                    else if (pval == sm_chsplit)
                        pval = sm_dual;
                    sv = Parameter::intScaledToFloat(pval, n_scene_modes - 1);
                }

                if (sv != cv)
                {
                    cc->setValue(sv);
                }

                // Integer switches also work differently
                auto assw = dynamic_cast<Surge::Widgets::Switch *>(cc);
                if (assw)
                {
                    if (assw->isMultiIntegerValued())
                    {
                        assw->setIntegerValue(synth->storage.getPatch().param_ptr[j]->val.i + 1);
                    }
                }

                auto bvf = cc->asJuceComponent();
                if (bvf)
                {
                    bvf->repaint();
                }
                else
                {
                    jassert(false);
                }
            }
#if 0
         /*
         ** A set of special things which invalidate the entire UI.
         ** See #2226 for why this is currently off.
         */
         else if( j == synth->storage.getPatch().scene_active.id )
         {
            if( current_scene != synth->storage.getPatch().scene_active.val.i )
            {
               synth->refresh_editor = true;
               current_scene = synth->storage.getPatch().scene_active.val.i;
            }

         }
#endif
            else
            {
                // printf( "Bailing out of all possible refreshes on %d\n", j );
                // This is not really a problem
            }
        }
        for (int i = 0; i < n_customcontrollers; i++)
        {
            if (((ControllerModulationSource *)synth->storage.getPatch()
                     .scene[current_scene]
                     .modsources[ms_ctrl1 + i])
                    ->has_changed(true))
            {
                gui_modsrc[ms_ctrl1 + i]->setValue(
                    ((ControllerModulationSource *)synth->storage.getPatch()
                         .scene[current_scene]
                         .modsources[ms_ctrl1 + i])
                        ->get_target01());
            }
        }
    }

#if BUILD_IS_DEBUG
    if (debugLabel)
    {
        /*
         * We can do debuggy stuff here to get an indea about internal state on the screen
         */

        /*
          auto t = std::to_string( synth->storage.songpos );
          debugLabel->setText(t.c_str());
          debugLabel->invalid();
          */
    }
#endif
}

void SurgeGUIEditor::toggle_mod_editing()
{
    bool doModEditing = true;

    if (currentSkin->getVersion() >= 2)
    {
        auto skinCtrl = currentSkin->controlForUIID("controls.modulation.panel");

        if (!skinCtrl)
        {
            skinCtrl = currentSkin->getOrCreateControlForConnector(
                Surge::Skin::Connector::connectorByID("controls.modulation.panel"));
        }

        if (skinCtrl->classname == Surge::GUI::NoneClassName)
        {
            doModEditing = false;
        }
    }

    if (doModEditing)
    {
        mod_editor = !mod_editor;
        refresh_mod();
    }
}

void SurgeGUIEditor::refresh_mod()
{
    auto *cms = gui_modsrc[modsource].get();
    if (!cms)
        return;

    modsources thisms = modsource;
    if (cms->getHasAlternate() && cms->getUseAlternate())
        thisms = cms->getAlternate();

    synth->storage.modRoutingMutex.lock();
    for (int i = 0; i < n_paramslots; i++)
    {
        if (param[i])
        {
            auto s = param[i];

            if (s->getIsValidToModulate())
            {
                s->setIsEditingModulation(mod_editor);
                s->setModulationState(synth->isModDestUsed(i),
                                      synth->isActiveModulation(i, thisms));
                s->setIsModulationBipolar(synth->isBipolarModulation(thisms));
                s->setModValue(synth->getModulation(i, thisms));
            }
            else
            {
                s->setIsEditingModulation(false);
            }
            s->asJuceComponent()->repaint();
        }
    }
#if OSC_MOD_ANIMATION
    if (oscdisplay)
    {
        ((COscillatorDisplay *)oscdisplay)->setIsMod(mod_editor);
        ((COscillatorDisplay *)oscdisplay)->setModSource(thisms);
        oscdisplay->invalid();
    }
#endif

    synth->storage.modRoutingMutex.unlock();

    for (int i = 1; i < n_modsources; i++)
    {
        int state = 0;

        if (i == modsource)
            state = mod_editor ? 2 : 1;

        if (i == modsource_editor[current_scene] && lfoNameLabel)
        {
            state |= 4;

            // update the LFO title label
            std::string modname = modulatorName(modsource_editor[current_scene], true);

            lfoNameLabel->setText(modname.c_str());
        }

        if (gui_modsrc[i])
        {
            // this could change if I cleared the last one
            gui_modsrc[i]->setUsed(synth->isModsourceUsed((modsources)i));
            gui_modsrc[i]->setState(state);

            if (i < ms_ctrl1 || i > ms_ctrl8)
            {
                auto mn = modulatorName(i, true);
                gui_modsrc[i]->setLabel(mn.c_str());
            }
            gui_modsrc[i]->repaint();
        }
    }
}

#if PORTED_TO_JUCE
int32_t SurgeGUIEditor::onKeyDown(const VstKeyCode &code, CFrame *frame)
{
#if RESOLVED_ISSUE_4383
    if (code.virt != 0)
    {
        switch (code.virt)
        {
        case VKEY_F5:
            if (Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                                                    Surge::Storage::SkinReloadViaF5, 0))
            {
                bitmapStore.reset(new SurgeBitmaps());
                bitmapStore->setupBuiltinBitmaps(frame);

                if (!currentSkin->reloadSkin(bitmapStore))
                {
                    auto &db = Surge::GUI::SkinDB::get();
                    auto msg = std::string("Unable to load skin! Reverting the skin to Surge "
                                           "Classic.\n\nSkin error:\n") +
                               db.getAndResetErrorString();
                    currentSkin = db.defaultSkin(&(synth->storage));
                    currentSkin->reloadSkin(bitmapStore);
                    synth->storage.reportError(msg, "Skin Loading Error");
                }

                reloadFromSkin();
                synth->refresh_editor = true;
            }

            return 1;
            break;
        case VKEY_TAB:
            if ((topmostEditorTag() == STORE_PATCH) || (typeinDialog && typeinDialog->isVisible()))
            {
                /*
                ** SaveDialog gets access to the tab key to switch between fields if it is open
                */
                return -1;
            }
            toggle_mod_editing();
            return 1;
            break;
        case VKEY_ESCAPE:
            if (typeinDialog && typeinDialog->isVisible())
            {
                // important to do this first since it basically stops us listening to changes
                typeinEditTarget = nullptr;

                typeinDialog->setVisible(false);
                removeFromFrame.push_back(typeinDialog);
                typeinDialog = nullptr;
                typeinMode = Inactive;
                typeinResetCounter = -1;

                return 1;
            }
            break;
        case VKEY_RETURN:
            if (typeinDialog && typeinDialog->isVisible())
            {
                typeinDialog->setVisible(false);
            }
            break;
        }
    }
#endif
    return -1;
}

int32_t SurgeGUIEditor::onKeyUp(const VstKeyCode &keyCode, CFrame *frame) { return -1; }
#endif

bool SurgeGUIEditor::isControlVisible(ControlGroup controlGroup, int controlGroupEntry)
{
    switch (controlGroup)
    {
    case cg_OSC:
        return (controlGroupEntry == current_osc[current_scene]);
    case cg_LFO:
        return (controlGroupEntry == modsource_editor[current_scene]);
    case cg_FX:
        return (controlGroupEntry == current_fx);
    default:
        return true;
    }
}

CRect SurgeGUIEditor::positionForModulationGrid(modsources entry)
{
    bool isMacro = isCustomController(entry);
    int gridX = modsource_grid_xy[entry][0];
    int gridY = modsource_grid_xy[entry][1];
    int width = isMacro ? 93 : 74;

    // to ensure the same gap between the modbuttons,
    // make the first and last non-macro button wider 2px
    if ((!isMacro) && ((gridX == 0) || (gridX == 9)))
        width += 2;

    auto skinCtrl = currentSkin->controlForUIID("controls.modulation.panel");

    if (!skinCtrl)
    {
        skinCtrl = currentSkin->getOrCreateControlForConnector(
            Surge::Skin::Connector::connectorByID("controls.modulation.panel"));
    }

    if (skinCtrl->classname == Surge::GUI::NoneClassName && currentSkin->getVersion() >= 2)
    {
        return CRect();
    }

    CRect r(CPoint(skinCtrl->x, skinCtrl->y), CPoint(width - 1, 14));

    if (isMacro)
        r.bottom += 8;

    int offsetX = 1;

    for (int i = 0; i < gridX; i++)
    {
        if ((!isMacro) && (i == 0))
            offsetX += 2;

        // gross hack for accumulated 2 px horizontal offsets from the previous if clause
        // needed to align the last column nicely
        if ((!isMacro) && (i == 8))
            offsetX -= 18;

        offsetX += width;
    }

    r.offset(offsetX, (8 * gridY));

    return r;
}

void SurgeGUIEditor::setDisabledForParameter(Parameter *p,
                                             Surge::Widgets::ModulatableControlInterface *s)
{
    if (p->id == synth->storage.getPatch().scene[current_scene].fm_depth.id)
    {
        s->setDeactivated(!(synth->storage.getPatch().scene[current_scene].fm_switch.val.i));
    }
}

void SurgeGUIEditor::openOrRecreateEditor()
{
#ifdef INSTRUMENT_UI
    Surge::Debug::record("SurgeGUIEditor::openOrRecreateEditor");
#endif
    if (!synth)
        return;
    assert(frame);

    editorOverlayTagAtClose = "";
    if (editor_open)
    {
        editorOverlayTagAtClose = topmostEditorTag();
        close_editor();
    }
    CPoint nopoint(0, 0);
    CPoint sz(getWindowSizeX(), getWindowSizeY());
    // auto lcb = new LastChanceEventCapture(sz, this);
    // frame->addView(lcb);

    std::unordered_map<std::string, std::string> uiidToSliderLabel;

    /*
    ** There are a collection of member states we need to reset
    */
    polydisp = nullptr;
    msegEditSwitch = nullptr;
    lfoNameLabel = nullptr;
    midiLearnOverlay = nullptr;

    for (int i = 0; i < 16; ++i)
        vu[i] = nullptr;

    current_scene = synth->storage.getPatch().scene_active.val.i;

    /*
     * In Surge 1.8, the skin engine can change the position of this panel as a whole
     * but not anything else about it. The skin query happens inside positionForModulationGrid
     */
    for (int k = 1; k < n_modsources; k++)
    {
        if ((k != ms_random_unipolar) && (k != ms_alternate_unipolar))
        {
            modsources ms = (modsources)k;

            CRect r = positionForModulationGrid(ms);

            int state = 0;

            if (ms == modsource)
                state = mod_editor ? 2 : 1;
            if (ms == modsource_editor[current_scene])
                state |= 4;

            if (!gui_modsrc[ms])
            {
                gui_modsrc[ms] = std::make_unique<Surge::Widgets::ModulationSourceButton>();
            }
            gui_modsrc[ms]->setModSource(ms);
            gui_modsrc[ms]->setBounds(r.asJuceIntRect());
            gui_modsrc[ms]->setTag(tag_mod_source0 + ms);
            gui_modsrc[ms]->addListener(this);
            gui_modsrc[ms]->setSkin(currentSkin, bitmapStore);
            gui_modsrc[ms]->setStorage(&(synth->storage));

            gui_modsrc[ms]->update_rt_vals(false, 0, synth->isModsourceUsed(ms));

            if ((ms >= ms_ctrl1) && (ms <= ms_ctrl8))
            {
                // std::cout << synth->learn_custom << std::endl;
                gui_modsrc[ms]->setLabel(
                    synth->storage.getPatch().CustomControllerLabel[ms - ms_ctrl1]);
                gui_modsrc[ms]->setIsMeta(true);
                gui_modsrc[ms]->setBipolar(
                    synth->storage.getPatch().scene[current_scene].modsources[ms]->is_bipolar());
                gui_modsrc[ms]->setValue(((ControllerModulationSource *)synth->storage.getPatch()
                                              .scene[current_scene]
                                              .modsources[ms])
                                             ->get_target01());
            }
            else
            {
                gui_modsrc[ms]->setLabel(modulatorName(ms, true).c_str());

                if (ms == ms_random_bipolar)
                {
                    gui_modsrc[ms]->setAlternate(ms_random_unipolar,
                                                 modsource_names_button[ms_random_unipolar]);
                    gui_modsrc[ms]->setUseAlternate(modsource_is_alternate[ms]);
                }

                if (ms == ms_alternate_bipolar)
                {
                    gui_modsrc[ms]->setAlternate(ms_alternate_unipolar,
                                                 modsource_names_button[ms_alternate_unipolar]);
                    gui_modsrc[ms]->setUseAlternate(modsource_is_alternate[ms]);
                }
            }

            frame->addAndMakeVisible(*gui_modsrc[ms]);
            if (ms >= ms_ctrl1 && ms <= ms_ctrl8 && synth->learn_custom == ms - ms_ctrl1)
            {
                showMidiLearnOverlay(r);
            }
        }
    }

    // fx vu-meters & labels. This is all a bit hacky still
    {
        std::lock_guard<std::mutex> g(synth->fxSpawnMutex);

        if (synth->fx[current_fx])
        {
            auto fxpp = currentSkin->getOrCreateControlForConnector("fx.param.panel");
            CRect fxRect = CRect(CPoint(fxpp->x, fxpp->y), CPoint(123, 13));
            for (int i = 0; i < 15; i++)
            {
                int t = synth->fx[current_fx]->vu_type(i);
                if (t)
                {
                    CRect vr(fxRect); // FIXME (vurect);
                    vr.offset(6, yofs * synth->fx[current_fx]->vu_ypos(i));
                    vr.offset(0, -14);
                    if (!vu[i + 1])
                    {
                        vu[i + 1] = std::make_unique<Surge::Widgets::VuMeter>();
                    }
                    vu[i + 1]->setBounds(vr.asJuceIntRect());
                    vu[i + 1]->setSkin(currentSkin, bitmapStore);
                    vu[i + 1]->setType(t);
                    frame->addAndMakeVisible(*vu[i + 1]);
                }
                else
                {
                    vu[i + 1] = 0;
                }

                const char *label = synth->fx[current_fx]->group_label(i);

                if (label)
                {
                    CRect vr(fxRect); // (vurect);
                    vr.top += 1;
                    vr.right += 5;
                    vr.offset(5, -12);
                    vr.offset(0, yofs * synth->fx[current_fx]->group_label_ypos(i));
                    if (!effectLabels[i])
                    {
                        effectLabels[i] = std::make_unique<Surge::Widgets::EffectLabel>();
                    }
                    effectLabels[i]->setBounds(vr.asJuceIntRect());
                    effectLabels[i]->setLabel(label);
                    effectLabels[i]->setSkin(currentSkin, bitmapStore);
                    frame->addAndMakeVisible(*effectLabels[i]);
                }
                else
                {
                    effectLabels[i].reset(nullptr);
                }
            }
        }
    }

    /*
     * Loop over the non-associated controls
     */
    for (auto i = Surge::Skin::Connector::NonParameterConnection::PARAMETER_CONNECTED + 1;
         i < Surge::Skin::Connector::NonParameterConnection::N_NONCONNECTED; ++i)
    {
        Surge::Skin::Connector::NonParameterConnection npc =
            (Surge::Skin::Connector::NonParameterConnection)i;
        auto conn = Surge::Skin::Connector::connectorByNonParameterConnection(npc);
        auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);
        currentSkin->resolveBaseParentOffsets(skinCtrl);

        if (!skinCtrl)
        {
            std::cout << "Unable to find SkinCtrl" << std::endl;
            continue;
        }
        if (skinCtrl->classname == Surge::GUI::NoneClassName)
            continue;

        /*
         * Many of the controls are special and so require non-generalizable constructors
         * handled here. Some are standard and so once we know the tag we can use
         * layoutComponentForSkin but it's not worth generalizing the OSCILLATOR_DISPLAY beyond
         * this, say.
         */
        switch (npc)
        {
        case Surge::Skin::Connector::NonParameterConnection::OSCILLATOR_DISPLAY:
        {
            if (!oscWaveform)
            {
                oscWaveform = std::make_unique<Surge::Widgets::OscillatorWaveformDisplay>();
            }
            oscWaveform->setBounds(skinCtrl->getRect().asJuceIntRect());
            oscWaveform->setSkin(currentSkin, bitmapStore);
            oscWaveform->setStorage(&(synth->storage));
            oscWaveform->setOscStorage(&(synth->storage.getPatch()
                                             .scene[synth->storage.getPatch().scene_active.val.i]
                                             .osc[current_osc[current_scene]]));
            oscWaveform->setSurgeGUIEditor(this);
            frame->addAndMakeVisible(*oscWaveform);
            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::SURGE_MENU:
        {
            auto q = layoutComponentForSkin(skinCtrl, tag_settingsmenu);
            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::OSCILLATOR_SELECT:
        {
            auto oscswitch = layoutComponentForSkin(skinCtrl, tag_osc_select);
            oscswitch->setValue((float)current_osc[current_scene] / 2.0f);
            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::JOG_PATCHCATEGORY:
        {
            layoutComponentForSkin(skinCtrl, tag_mp_category);
            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::JOG_PATCH:
        {
            layoutComponentForSkin(skinCtrl, tag_mp_patch);
            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::JOG_FX:
        {
            layoutComponentForSkin(skinCtrl, tag_mp_jogfx);
            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::STATUS_MPE:
        {
            statusMPE = layoutComponentForSkin(skinCtrl, tag_status_mpe);
            statusMPE->setValue(synth->mpeEnabled ? 1 : 0);
            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::STATUS_TUNE:
        {
            // FIXME - drag and drop onto this?
            statusTune = layoutComponentForSkin(skinCtrl, tag_status_tune);
            auto hasmts = synth->storage.oddsound_mts_client && synth->storage.oddsound_mts_active;
            statusTune->setValue(synth->storage.isStandardTuning ? hasmts : 1);

            auto csc = dynamic_cast<Surge::Widgets::Switch *>(statusTune);
            if (csc && synth->storage.isStandardTuning)
            {
                csc->setUnValueClickable(!synth->storage.isToggledToCache);
            }
            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::STATUS_ZOOM:
        {
            statusZoom = layoutComponentForSkin(skinCtrl, tag_status_zoom);
            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::STORE_PATCH:
        {
            layoutComponentForSkin(skinCtrl, tag_store);
            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::MSEG_EDITOR_OPEN:
        {
            msegEditSwitch = layoutComponentForSkin(skinCtrl, tag_mseg_edit);
            auto msejc = dynamic_cast<juce::Component *>(msegEditSwitch);
            jassert(msejc);
            msejc->setVisible(false);
            msegEditSwitch->setValue(isAnyOverlayPresent(MSEG_EDITOR) ||
                                     isAnyOverlayPresent(FORMULA_EDITOR));
            auto q = modsource_editor[current_scene];
            if ((q >= ms_lfo1 && q <= ms_lfo6) || (q >= ms_slfo1 && q <= ms_slfo6))
            {
                auto *lfodata = &(synth->storage.getPatch().scene[current_scene].lfo[q - ms_lfo1]);
                if (lfodata->shape.val.i == lt_mseg || lfodata->shape.val.i == lt_formula)
                    msejc->setVisible(true);
            }
            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::LFO_MENU:
        {
            layoutComponentForSkin(skinCtrl, tag_lfo_menu);

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::LFO_LABEL:
        {
            // Room for improvement, obviously
            lfoNameLabel =
                componentForSkinSession<Surge::Widgets::VerticalLabel>(skinCtrl->sessionid);
            frame->addAndMakeVisible(*lfoNameLabel);
            lfoNameLabel->setBounds(skinCtrl->getRect().asJuceIntRect());
            lfoNameLabel->setFont(
                Surge::GUI::getFontManager()->getLatoAtSize(10, juce::Font::bold));
            lfoNameLabel->setFontColour(currentSkin->getColor(Colors::LFO::Title::Text));
            break;
        }

        case Surge::Skin::Connector::NonParameterConnection::FXPRESET_LABEL:
        {
            // Room for improvement, obviously
            if (!fxPresetLabel)
            {
                fxPresetLabel = std::make_unique<juce::Label>("FxPreset label");
            }

            fxPresetLabel->setColour(juce::Label::textColourId,
                                     currentSkin->getColor(Colors::Effect::Preset::Name));
            fxPresetLabel->setFont(Surge::GUI::getFontManager()->displayFont);
            fxPresetLabel->setJustificationType(juce::Justification::centredRight);

            fxPresetLabel->setText(fxPresetName[current_fx], juce::dontSendNotification);

            frame->addAndMakeVisible(*fxPresetLabel);
            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::PATCH_BROWSER:
        {
            patchSelector =
                componentForSkinSession<Surge::Widgets::PatchSelector>(skinCtrl->sessionid);
            patchSelector->addListener(this);
            patchSelector->setStorage(&(this->synth->storage));
            patchSelector->setTag(tag_patchname);
            patchSelector->setSkin(currentSkin, bitmapStore);
            patchSelector->setLabel(synth->storage.getPatch().name);
            patchSelector->setCategory(synth->storage.getPatch().category);
            patchSelector->setIDs(synth->current_category_id, synth->patchid);
            patchSelector->setAuthor(synth->storage.getPatch().author);
            patchSelector->setBounds(skinCtrl->getRect().asJuceIntRect());

            frame->addAndMakeVisible(*patchSelector);
            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::FX_SELECTOR:
        {
            auto fc = componentForSkinSession<Surge::Widgets::EffectChooser>(skinCtrl->sessionid);
            fc->addListener(this);
            fc->setBounds(skinCtrl->getRect().asJuceIntRect());
            fc->setTag(tag_fx_select);
            fc->setSkin(currentSkin, bitmapStore);
            fc->setBackgroundDrawable(bitmapStore->getDrawable(IDB_FX_GRID));
            fc->setCurrentEffect(current_fx);

            for (int fxi = 0; fxi < n_fx_slots; fxi++)
            {
                fc->setEffectType(fxi, synth->storage.getPatch().fx[fxi].type.val.i);
            }
            fc->setBypass(synth->storage.getPatch().fx_bypass.val.i);
            fc->setDeactivatedBitmask(synth->storage.getPatch().fx_disable.val.i);

            frame->addAndMakeVisible(*fc);

            effectChooser = std::move(fc);
            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::MAIN_VU_METER:
        { // main vu-meter
            vu[0] = componentForSkinSession<Surge::Widgets::VuMeter>(skinCtrl->sessionid);
            vu[0]->setBounds(skinCtrl->getRect().asJuceIntRect());
            vu[0]->setSkin(currentSkin, bitmapStore);
            vu[0]->setType(vut_vu_stereo);
            frame->addAndMakeVisible(*vu[0]);
            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::PARAMETER_CONNECTED:
        case Surge::Skin::Connector::NonParameterConnection::STORE_PATCH_DIALOG:
        case Surge::Skin::Connector::NonParameterConnection::MSEG_EDITOR_WINDOW:
        case Surge::Skin::Connector::NonParameterConnection::N_NONCONNECTED:
            break;
        }
    }

    memset(param, 0, n_paramslots * sizeof(void *));
    memset(nonmod_param, 0, n_paramslots * sizeof(void *));
    int i = 0;
    vector<Parameter *>::iterator iter;
    for (iter = synth->storage.getPatch().param_ptr.begin();
         iter != synth->storage.getPatch().param_ptr.end(); iter++)
    {
        if (i == n_paramslots)
        {
            // This would only happen if a dev added params.
            synth->storage.reportError(
                "INTERNAL ERROR: List of parameters is larger than maximum number of parameter "
                "slots. Increase n_paramslots in SurgeGUIEditor.h!",
                "Error");
        }
        Parameter *p = *iter;
        bool paramIsVisible = ((p->scene == (current_scene + 1)) || (p->scene == 0)) &&
                              isControlVisible(p->ctrlgroup, p->ctrlgroup_entry) &&
                              (p->ctrltype != ct_none);

        auto conn = Surge::Skin::Connector::connectorByID(p->ui_identifier);
        std::string uiid = p->ui_identifier;

        long style = p->ctrlstyle;

        if (p->ctrltype == ct_fmratio)
        {
            if (p->extend_range || p->absolute)
                p->val_default.f = 16.f;
            else
                p->val_default.f = 1.f;
        }

        if (p->hasSkinConnector &&
            conn.payload->defaultComponent != Surge::Skin::Components::None && paramIsVisible)
        {
            /*
             * Some special cases where we don't add a control
             */
            bool addControl = true;

            // Case: Analog envelopes have no shapers
            if (p->ctrltype == ct_envshape || p->ctrltype == ct_envshape_attack)
            {
                addControl = (synth->storage.getPatch()
                                  .scene[current_scene]
                                  .adsr[p->ctrlgroup_entry]
                                  .mode.val.i == emt_digital);
            }

            if (addControl)
            {
                auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);
                currentSkin->resolveBaseParentOffsets(skinCtrl);
                layoutComponentForSkin(skinCtrl, p->id + start_paramtags, i, p,
                                       style | conn.payload->controlStyleFlags);

                uiidToSliderLabel[p->ui_identifier] = p->get_name();
                if (p->id == synth->learn_param)
                {
                    showMidiLearnOverlay(
                        param[p->id]->asControlValueInterface()->asJuceComponent()->getBounds());
                }
            }
        }
        i++;
    }

    // resonance link mode
    if (synth->storage.getPatch().scene[current_scene].f2_link_resonance.val.b)
    {
        i = synth->storage.getPatch().scene[current_scene].filterunit[1].resonance.id;
        if (param[i])
            param[i]->setDeactivated(true);
    }
    else
    {
        i = synth->storage.getPatch().scene[current_scene].filterunit[1].resonance.id;
        if (param[i])
            param[i]->setDeactivated(false);
    }

    // feedback control
    if (synth->storage.getPatch().scene[current_scene].filterblock_configuration.val.i ==
        fc_serial1)
    {
        i = synth->storage.getPatch().scene[current_scene].feedback.id;
        bool curr = param[i]->getDeactivated();
        param[i]->setDeactivated(true);
        if (!curr)
        {
            param[i]->asJuceComponent()->repaint();
        }
    }

    // pan2 control
    if ((synth->storage.getPatch().scene[current_scene].filterblock_configuration.val.i !=
         fc_stereo) &&
        (synth->storage.getPatch().scene[current_scene].filterblock_configuration.val.i != fc_wide))
    {
        i = synth->storage.getPatch().scene[current_scene].width.id;
        if (param[i])
        {
            bool curr = param[i]->getDeactivated();
            param[i]->setDeactivated(true);
            if (!curr)
            {
                param[i]->asJuceComponent()->repaint();
            }
        }
    }

    // Make sure the infowindow typein
    paramInfowindow->setVisible(false);
    frame->addChildComponent(*paramInfowindow);

    int menuX = 844, menuY = 550, menuW = 50, menuH = 15;
    CRect menurect(menuX, menuY, menuX + menuW, menuY + menuH);

    // Mouse behavior
    if (Surge::Widgets::ModulatableSlider::sliderMoveRateState ==
        Surge::Widgets::ModulatableSlider::kUnInitialized)
        Surge::Widgets::ModulatableSlider::sliderMoveRateState =
            (Surge::Widgets::ModulatableSlider::MoveRateState)Surge::Storage::getUserDefaultValue(
                &(synth->storage), Surge::Storage::SliderMoveRateState,
                (int)Surge::Widgets::ModulatableSlider::kLegacy);

    /*
    ** Skin Labels
    */
    auto labels = currentSkin->getLabels();

    for (auto &l : labels)
    {
        auto mtext = currentSkin->propertyValue(l, Surge::Skin::Component::TEXT);
        auto ctext = currentSkin->propertyValue(l, Surge::Skin::Component::CONTROL_TEXT);
        if (ctext.isJust() && uiidToSliderLabel.find(ctext.fromJust()) != uiidToSliderLabel.end())
        {

            mtext = Surge::Maybe<std::string>(uiidToSliderLabel[ctext.fromJust()]);
        }

        if (mtext.isJust())
        {
            VSTGUI::CHoriTxtAlign txtalign = Surge::GUI::Skin::setTextAlignProperty(
                currentSkin->propertyValue(l, Surge::Skin::Component::TEXT_ALIGN, "left"));

            auto fs = currentSkin->propertyValue(l, Surge::Skin::Component::FONT_SIZE, "12");
            auto fsize = std::atof(fs.c_str());

            auto fstyle = Surge::GUI::Skin::setFontStyleProperty(
                currentSkin->propertyValue(l, Surge::Skin::Component::FONT_STYLE, "normal"));

            auto coln =
                currentSkin->propertyValue(l, Surge::Skin::Component::TEXT_COLOR, "#FF0000");
            auto col = currentSkin->getColor(coln, juce::Colours::red);

            auto dcol = juce::Colour(255, 255, 255).withAlpha((uint8_t)0);
            auto bgcoln = currentSkin->propertyValue(l, Surge::Skin::Component::BACKGROUND_COLOR,
                                                     "#FFFFFF00");
            auto bgcol = currentSkin->getColor(bgcoln, dcol);

            auto frcoln =
                currentSkin->propertyValue(l, Surge::Skin::Component::FRAME_COLOR, "#FFFFFF00");
            auto frcol = currentSkin->getColor(frcoln, dcol);

            auto lb = componentForSkinSession<juce::Label>(l->sessionid);
            jassert(false);
#if PORTED_TO_JUCE
            lb->setHoriAlign(txtalign);

            lb->setFontColor(col);
            lb->setBackColor(bgcol);
            lb->setFrameColor(frcol);

            frame->addView(lb);
#endif
        }
        else
        {
            auto image = currentSkin->propertyValue(l, Surge::Skin::Component::IMAGE);
            if (image.isJust())
            {
                auto bmp = bitmapStore->getDrawableByStringID(image.fromJust());
                if (bmp)
                {
                    jassert(false);
#if PORTED_TO_JUCE
                    // layout and add
#endif
                }
            }
        }
    }
#if BUILD_IS_DEBUG
    /*
     * This code is here JUST because baconpaul keeps developing surge and then swapping
     * to make music and wondering why LPX is stuttering. Please don't remove it!
     *
     * UPDATE: Might as well keep a reference to the object though so we can touch it in idle
     */
    auto dl = std::string("Debug ") + Surge::Build::BuildTime + " " + Surge::Build::GitBranch;
    if (!debugLabel)
    {
        debugLabel = std::make_unique<juce::Label>("debugLabel");
    }
    debugLabel->setBounds(310, 39, 195, 15);
    debugLabel->setText(dl, juce::dontSendNotification);
    debugLabel->setColour(juce::Label::backgroundColourId,
                          juce::Colours::red.withAlpha((float)0.8));
    debugLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    debugLabel->setFont(Surge::GUI::getFontManager()->getFiraMonoAtSize(9));
    debugLabel->setJustificationType(juce::Justification::centred);
    frame->addAndMakeVisible(*debugLabel);

#endif

    if (showMSEGEditorOnNextIdleOrOpen)
    {
        showMSEGEditor();
        showMSEGEditorOnNextIdleOrOpen = false;
    }

    // We need this here in case we rebuild when opening a new patch.
    // closeMSEGEditor does nothing if the mseg editor isn't open
    auto lfoidx = modsource_editor[current_scene] - ms_lfo1;
    if (lfoidx >= 0 && lfoidx <= n_lfos)
    {
        auto ld = &(synth->storage.getPatch().scene[current_scene].lfo[lfoidx]);
        if (ld->shape.val.i != lt_mseg)
        {
            closeMSEGEditor();
        }
    }

    refresh_mod();

    editor_open = true;
    queue_refresh = false;

    frame->repaint();
}

void SurgeGUIEditor::close_editor()
{
    editor_open = false;
    frame->removeAllChildren();
    setzero(param);
}

bool SurgeGUIEditor::open(void *parent)
{
    int platformType = 0;
    float fzf = getZoomFactor() / 100.0;
    CRect wsize(0, 0, currentSkin->getWindowSizeX(), currentSkin->getWindowSizeY());

    frame = std::make_unique<Surge::Widgets::MainFrame>();
    frame->setBounds(0, 0, currentSkin->getWindowSizeX(), currentSkin->getWindowSizeY());
    frame->setSurgeGUIEditor(this);
    juceEditor->addAndMakeVisible(*frame);
    /*
     * SET UP JUCE EDITOR BETTER
     */

    bitmapStore.reset(new SurgeBitmaps());
    bitmapStore->setupBuiltinBitmaps();
    currentSkin->reloadSkin(bitmapStore);

    reloadFromSkin();
    openOrRecreateEditor();

    if (getZoomFactor() != 100)
    {
        zoom_callback(this, true);
        zoomInvalid = true;
    }

    auto *des = &(synth->storage.getPatch().dawExtraState);

    if (des->isPopulated && des->editor.isMSEGOpen)
        showMSEGEditor();

    return true;
}

void SurgeGUIEditor::close()
{
    populateDawExtraState(synth);
    firstIdleCountdown = 0;
}

void SurgeGUIEditor::setParameter(long index, float value)
{
    if (!frame)
        return;
    if (!editor_open)
        return;
    if (index > synth->storage.getPatch().param_ptr.size())
        return;

    // if(param[index])
    {
        // param[index]->setValue(value);
        int j = 0;
        while (j < 7)
        {
            if ((synth->refresh_ctrl_queue[j] > -1) && (synth->refresh_ctrl_queue[j] != index))
                j++;
            else
                break;
        }
        synth->refresh_ctrl_queue[j] = index;
        synth->refresh_ctrl_queue_value[j] = value;
    }
}

void SurgeGUIEditor::effectSettingsBackgroundClick(int whichScene)
{
    auto fxGridMenu = juce::PopupMenu();

    auto msurl = SurgeGUIEditor::helpURLForSpecial("fx-selector");
    auto hurl = SurgeGUIEditor::fullyResolvedHelpURL(msurl);

    fxGridMenu.addItem("[?] Effect Settings",
                       [hurl]() { juce::URL(hurl).launchInDefaultBrowser(); });

    fxGridMenu.addSeparator();

    std::string sc = std::string("Scene ") + (char)('A' + whichScene);

    fxGridMenu.addItem(
        sc + Surge::GUI::toOSCaseForMenu(" Hard Clip Disabled"), true,
        (synth->storage.sceneHardclipMode[whichScene] == SurgeStorage::BYPASS_HARDCLIP),
        [this, whichScene]() {
            this->synth->storage.sceneHardclipMode[whichScene] = SurgeStorage::BYPASS_HARDCLIP;
        });

    fxGridMenu.addItem(
        sc + Surge::GUI::toOSCaseForMenu(" Hard Clip at 0 dBFS"), true,
        (synth->storage.sceneHardclipMode[whichScene] == SurgeStorage::HARDCLIP_TO_0DBFS),
        [this, whichScene]() {
            this->synth->storage.sceneHardclipMode[whichScene] = SurgeStorage::HARDCLIP_TO_0DBFS;
        });

    fxGridMenu.addItem(
        sc + Surge::GUI::toOSCaseForMenu(" Hard Clip at +18 dBFS"), true,
        (synth->storage.sceneHardclipMode[whichScene] == SurgeStorage::HARDCLIP_TO_18DBFS),
        [this, whichScene]() {
            this->synth->storage.sceneHardclipMode[whichScene] = SurgeStorage::HARDCLIP_TO_18DBFS;
        });

    fxGridMenu.showMenuAsync(juce::PopupMenu::Options());
}

void SurgeGUIEditor::controlBeginEdit(Surge::GUI::IComponentTagValue *control)
{
    long tag = control->getTag();
    int ptag = tag - start_paramtags;
    if (ptag >= 0 && ptag < synth->storage.getPatch().param_ptr.size())
    {
        juceEditor->beginParameterEdit(synth->storage.getPatch().param_ptr[ptag]);
    }
}

//------------------------------------------------------------------------------------------------

void SurgeGUIEditor::controlEndEdit(Surge::GUI::IComponentTagValue *control)
{
    long tag = control->getTag();
    int ptag = tag - start_paramtags;
    if (ptag >= 0 && ptag < synth->storage.getPatch().param_ptr.size())
    {
        juceEditor->endParameterEdit(synth->storage.getPatch().param_ptr[ptag]);
    }
}

//------------------------------------------------------------------------------------------------

long SurgeGUIEditor::applyParameterOffset(long id) { return id - start_paramtags; }

long SurgeGUIEditor::unapplyParameterOffset(long id) { return id + start_paramtags; }

// Status Panel Callbacks
void SurgeGUIEditor::toggleMPE()
{
    this->synth->mpeEnabled = !this->synth->mpeEnabled;
    if (statusMPE)
    {
        statusMPE->setValue(this->synth->mpeEnabled ? 1 : 0);
        statusMPE->asJuceComponent()->repaint();
    }
}
void SurgeGUIEditor::showZoomMenu(VSTGUI::CPoint &where)
{
    CRect menuRect;
    menuRect.offset(where.x, where.y);
    auto m = makeZoomMenu(menuRect, true);

    m.showMenuAsync(juce::PopupMenu::Options());
}

void SurgeGUIEditor::showMPEMenu(VSTGUI::CPoint &where)
{
    CRect menuRect;
    menuRect.offset(where.x, where.y);
    auto m = makeMpeMenu(menuRect, true);

    m.showMenuAsync(juce::PopupMenu::Options());
}
void SurgeGUIEditor::showLfoMenu(VSTGUI::CPoint &where)
{
    CRect menuRect;
    menuRect.offset(where.x, where.y);
    auto m = makeLfoMenu(menuRect);

    m.showMenuAsync(juce::PopupMenu::Options());
}

void SurgeGUIEditor::toggleTuning()
{
    synth->storage.toggleTuningToCache();

    if (statusTune)
    {
        bool hasmts = synth->storage.oddsound_mts_client && synth->storage.oddsound_mts_active;
        statusTune->setValue(this->synth->storage.isStandardTuning ? hasmts : 1);
    }

    this->synth->refresh_editor = true;
}
void SurgeGUIEditor::showTuningMenu(VSTGUI::CPoint &where)
{
    CRect menuRect;
    menuRect.offset(where.x, where.y);
    auto m = makeTuningMenu(menuRect, true);

    m.showMenuAsync(juce::PopupMenu::Options());
}

void SurgeGUIEditor::scaleFileDropped(std::string fn)
{
    try
    {
        this->synth->storage.retuneToScale(Tunings::readSCLFile(fn));
        this->synth->refresh_editor = true;
    }
    catch (Tunings::TuningError &e)
    {
        synth->storage.reportError(e.what(), "SCL Error");
    }
}

void SurgeGUIEditor::mappingFileDropped(std::string fn)
{
    try
    {
        this->synth->storage.remapToKeyboard(Tunings::readKBMFile(fn));
        this->synth->refresh_editor = true;
    }
    catch (Tunings::TuningError &e)
    {
        synth->storage.reportError(e.what(), "KBM Error");
    }
}

bool SurgeGUIEditor::doesZoomFitToScreen(float zf, float &correctedZf)
{
#if !LINUX
    correctedZf = zf;
    return true;
#endif

    auto screenDim =
        CRect(juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->totalArea);

    float baseW = getWindowSizeX();
    float baseH = getWindowSizeY();

    /*
    ** Window decoration takes up some of the screen so don't zoom to full screen dimensions.
    ** This heuristic seems to work on Windows 10 and macOS 10.14 well enough.
    ** Keep these as integers to be consistent with the other zoom factors, and to make
    ** the error message cleaner.
    */
    int maxScreenUsage = 90;

    /*
    ** In the startup path we may not have a clean window yet to give us a trustworthy
    ** screen dimension; so allow callers to suppress this check with an optional
    ** variable and set it only in the constructor of SurgeGUIEditor
    */
    if (zf != 100.0 && zf > 100 && screenDim.getHeight() > 0 && screenDim.getWidth() > 0 &&
        ((baseW * zf / 100.0) > maxScreenUsage * screenDim.getWidth() / 100.0 ||
         (baseH * zf / 100.0) > maxScreenUsage * screenDim.getHeight() / 100.0))
    {
        correctedZf = findLargestFittingZoomBetween(100.0, zf, 5, maxScreenUsage, baseW, baseH);
        return false;
    }
    else
    {
        correctedZf = zf;
        return true;
    }
}

void SurgeGUIEditor::resizeWindow(float zf) { setZoomFactor(zf, true); }

void SurgeGUIEditor::setZoomFactor(float zf) { setZoomFactor(zf, false); }

void SurgeGUIEditor::setZoomFactor(float zf, bool resizeWindow)
{
    zoomFactor = std::max(zf, 25.f);
    if (currentSkin && resizeWindow)
    {
        int yExtra = 0;
        if (getShowVirtualKeyboard())
        {
            yExtra = SurgeSynthEditor::extraYSpaceForVirtualKeyboard;
        }
        juceEditor->setSize(currentSkin->getWindowSizeX(), currentSkin->getWindowSizeY() + yExtra);
    }
    juceEditor->setScaleFactor(zoomFactor * 0.01);
}

void SurgeGUIEditor::setBitmapZoomFactor(float zf)
{
    float dbs = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->scale;
    int fullPhysicalZoomFactor = (int)(zf * dbs);
    if (bitmapStore != nullptr)
        bitmapStore->setPhysicalZoomFactor(fullPhysicalZoomFactor);
}

void SurgeGUIEditor::showMinimumZoomError() const
{
    std::ostringstream oss;
    oss << "The smallest zoom level possible on your platform is " << minimumZoom
        << "%. Sorry, you cannot make Surge any smaller!";
    synth->storage.reportError(oss.str(), "Zoom Level Error");
}

void SurgeGUIEditor::showTooLargeZoomError(double width, double height, float zf) const
{
#if !LINUX
    std::ostringstream msg;
    msg << "Surge adjusts the maximum zoom level in order to prevent the interface becoming larger "
           "than available screen area. "
        << "Your screen resolution is " << width << "x" << height << " "
        << "for which the target zoom level of " << zf << "% would be too large." << std::endl
        << std::endl;
    if (currentSkin && currentSkin->hasFixedZooms())
    {
        msg << "Surge chose the largest fitting fixed zoom which is provided by this skin.";
    }
    else
    {
        msg << "Surge chose the largest fitting zoom level of " << zf << "%.";
    }
    synth->storage.reportError(msg.str(), "Zoom Level Adjusted");
#endif
}

void SurgeGUIEditor::showSettingsMenu(CRect &menuRect)
{
    auto settingsMenu = juce::PopupMenu();

    auto zoomMenu = makeZoomMenu(menuRect, false);
    settingsMenu.addSubMenu("Zoom", zoomMenu);

    auto skinSubMenu = makeSkinMenu(menuRect);
    settingsMenu.addSubMenu("Skins", skinSubMenu);

    auto valueDispMenu = makeValueDisplaysMenu(menuRect);
    settingsMenu.addSubMenu(Surge::GUI::toOSCaseForMenu("Value Displays"), valueDispMenu);

    settingsMenu.addSeparator();

    auto dataSubMenu = makeDataMenu(menuRect);
    settingsMenu.addSubMenu(Surge::GUI::toOSCaseForMenu("Data Folders"), dataSubMenu);

    auto mouseMenu = makeMouseBehaviorMenu(menuRect);
    settingsMenu.addSubMenu(Surge::GUI::toOSCaseForMenu("Mouse Behavior"), mouseMenu);

    auto patchDefMenu = makePatchDefaultsMenu(menuRect);
    settingsMenu.addSubMenu(Surge::GUI::toOSCaseForMenu("Patch Defaults"), patchDefMenu);

    auto wfMenu = makeWorkflowMenu(menuRect);
    settingsMenu.addSubMenu(Surge::GUI::toOSCaseForMenu("Workflow"), wfMenu);

    settingsMenu.addSeparator();

    auto mpeSubMenu = makeMpeMenu(menuRect, false);
    settingsMenu.addSubMenu(Surge::GUI::toOSCaseForMenu("MPE Settings"), mpeSubMenu);

    auto midiSubMenu = makeMidiMenu(menuRect);
    settingsMenu.addSubMenu(Surge::GUI::toOSCaseForMenu("MIDI Settings"), midiSubMenu);

    auto tuningSubMenu = makeTuningMenu(menuRect, false);
    settingsMenu.addSubMenu("Tuning", tuningSubMenu);

    settingsMenu.addSeparator();

    if (useDevMenu)
    {
        settingsMenu.addSeparator();

        auto devSubMenu = makeDevMenu(menuRect);
        settingsMenu.addSubMenu(Surge::GUI::toOSCaseForMenu("Developer Options"), devSubMenu);
    }

    settingsMenu.addSeparator();

    settingsMenu.addItem(Surge::GUI::toOSCaseForMenu("Reach the Developers..."), []() {
        juce::URL("https://surge-synthesizer.github.io/feedback").launchInDefaultBrowser();
    });

    settingsMenu.addItem(Surge::GUI::toOSCaseForMenu("Read the Code..."), []() {
        juce::URL("https://github.com/surge-synthesizer/surge/").launchInDefaultBrowser();
    });

    settingsMenu.addItem(Surge::GUI::toOSCaseForMenu("Download Additional Content..."), []() {
        juce::URL("https://github.com/surge-synthesizer/"
                  "surge-synthesizer.github.io/wiki/Additional-Content")
            .launchInDefaultBrowser();
    });

    settingsMenu.addItem(Surge::GUI::toOSCaseForMenu("Surge Website..."), []() {
        juce::URL("https://surge-synthesizer.github.io/").launchInDefaultBrowser();
    });

    settingsMenu.addItem(Surge::GUI::toOSCaseForMenu("Surge Manual..."), []() {
        juce::URL("https://surge-synthesizer.github.io/manual/").launchInDefaultBrowser();
    });

    settingsMenu.addSeparator();

    settingsMenu.addItem("About Surge", [this]() { this->showAboutScreen(); });

    settingsMenu.showMenuAsync(juce::PopupMenu::Options());
}

juce::PopupMenu SurgeGUIEditor::makeLfoMenu(VSTGUI::CRect &menuRect)
{
    int currentLfoId = modsource_editor[current_scene] - ms_lfo1;
    int shapev = synth->storage.getPatch().scene[current_scene].lfo[currentLfoId].shape.val.i;
    std::string what;

    switch (shapev)
    {
    case lt_mseg:
        what = "MSEG";
        break;
    case lt_stepseq:
        what = "Step Seq";
        break;
    case lt_envelope:
        what = "Envelope";
        break;
    case lt_formula:
        what = "Formula";
        break;
    default:
        what = "LFO";
        break;
    }

    auto msurl = SurgeGUIEditor::helpURLForSpecial("lfo-presets");
    auto hurl = SurgeGUIEditor::fullyResolvedHelpURL(msurl);

    auto lfoSubMenu = juce::PopupMenu();

    lfoSubMenu.addItem("[?] LFO Presets", [hurl]() { juce::URL(hurl).launchInDefaultBrowser(); });
    lfoSubMenu.addSeparator();

    lfoSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Save " + what + " As..."), [this, currentLfoId,
                                                                                what]() {
        // Prompt for a name
        promptForMiniEdit(
            "preset", "Enter the name for " + what + " preset:", what + " Preset Name",
            CPoint(-1, -1), [this, currentLfoId](const std::string &s) {
                Surge::ModulatorPreset::savePresetToUser(string_to_path(s), &(this->synth->storage),
                                                         current_scene, currentLfoId);
            });
        // and save
    });

    auto presetCategories = Surge::ModulatorPreset::getPresets(&(synth->storage));
    if (!presetCategories.empty())
    {
        lfoSubMenu.addSeparator();
    }

    std::function<void(juce::PopupMenu & m, const Surge::ModulatorPreset::Category &cat)>
        recurseCat;
    recurseCat = [this, currentLfoId, presetCategories,
                  &recurseCat](juce::PopupMenu &m, const Surge::ModulatorPreset::Category &cat) {
        for (const auto &p : cat.presets)
        {
            auto action = [this, p, currentLfoId]() {
                Surge::ModulatorPreset::loadPresetFrom(p.path, &(this->synth->storage),
                                                       current_scene, currentLfoId);

                auto newshape = this->synth->storage.getPatch()
                                    .scene[current_scene]
                                    .lfo[currentLfoId]
                                    .shape.val.i;

                if (isAnyOverlayPresent(MSEG_EDITOR))
                {
                    closeMSEGEditor();

                    if (newshape == lt_mseg)
                    {
                        showMSEGEditor();
                    }
                }

                this->synth->refresh_editor = true;
            };
            m.addItem(p.name, action);
        }
        bool haveD = false;
        for (const auto &sc : presetCategories)
        {
            if (sc.parentPath == cat.path)
            {
                if (!haveD)
                    m.addSeparator();
                haveD = true;
                juce::PopupMenu subMenu;
                recurseCat(subMenu, sc);
                m.addSubMenu(sc.name, subMenu);
            }
        }
    };

    for (auto tlc : presetCategories)
    {
        if (tlc.parentPath.empty())
        {
            juce::PopupMenu sm;
            recurseCat(sm, tlc);
            lfoSubMenu.addSubMenu(tlc.name, sm);
        }
    }

    lfoSubMenu.addSeparator();
    lfoSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Rescan Presets"),
                       []() { Surge::ModulatorPreset::forcePresetRescan(); });

    return lfoSubMenu;
}

juce::PopupMenu SurgeGUIEditor::makeMpeMenu(VSTGUI::CRect &menuRect, bool showhelp)
{
    auto mpeSubMenu = juce::PopupMenu();

    auto hu = helpURLForSpecial("mpe-menu");

    if (hu != "" && showhelp)
    {
        auto lurl = fullyResolvedHelpURL(hu);

        mpeSubMenu.addItem("[?] MPE", [lurl]() { juce::URL(lurl).launchInDefaultBrowser(); });
        mpeSubMenu.addSeparator();
    }

    std::string endis = "Enable MPE";

    if (synth->mpeEnabled)
    {
        endis = "Disable MPE";
    }

    mpeSubMenu.addItem(endis.c_str(),
                       [this]() { this->synth->mpeEnabled = !this->synth->mpeEnabled; });

    mpeSubMenu.addSeparator();

    std::ostringstream oss;
    oss << "Change MPE Pitch Bend Range (Current: " << synth->storage.mpePitchBendRange
        << " Semitones)";

    mpeSubMenu.addItem(Surge::GUI::toOSCaseForMenu(oss.str().c_str()), [this, menuRect]() {
        // FIXME! This won't work on Linux
        const auto c{std::to_string(int(synth->storage.mpePitchBendRange))};
        promptForMiniEdit(c, "Enter new MPE pitch bend range:", "MPE Pitch Bend Range",
                          menuRect.getTopLeft(), [this](const std::string &c) {
                              int newVal = ::atoi(c.c_str());
                              this->synth->storage.mpePitchBendRange = newVal;
                          });
    });

    std::ostringstream oss2;
    int def = Surge::Storage::getUserDefaultValue(&(synth->storage),
                                                  Surge::Storage::MPEPitchBendRange, 48);
    oss2 << "Change Default MPE Pitch Bend Range (Current: " << def << " Semitones)";

    mpeSubMenu.addItem(Surge::GUI::toOSCaseForMenu(oss2.str().c_str()), [this, menuRect]() {
        // FIXME! This won't work on linux
        const auto c{std::to_string(int(synth->storage.mpePitchBendRange))};
        promptForMiniEdit(c, "Enter default MPE pitch bend range:", "Default MPE Pitch Bend Range",
                          menuRect.getTopLeft(), [this](const std::string &s) {
                              int newVal = ::atoi(s.c_str());
                              Surge::Storage::updateUserDefaultValue(
                                  &(this->synth->storage), Surge::Storage::MPEPitchBendRange,
                                  newVal);
                              this->synth->storage.mpePitchBendRange = newVal;
                          });
    });

    auto smoothMenu = makeSmoothMenu(menuRect, Surge::Storage::PitchSmoothingMode,
                                     (int)ControllerModulationSource::SmoothingMode::DIRECT,
                                     [this](auto md) { this->resetPitchSmoothing(md); });

    mpeSubMenu.addSubMenu(Surge::GUI::toOSCaseForMenu("MPE Pitch Bend Smoothing"), smoothMenu);

    return mpeSubMenu;
}

juce::PopupMenu SurgeGUIEditor::makeMonoModeOptionsMenu(VSTGUI::CRect &menuRect,
                                                        bool updateDefaults)
{
    auto monoSubMenu = juce::PopupMenu();

    auto mode = synth->storage.monoPedalMode;

    if (updateDefaults)
    {
        mode = (MonoPedalMode)Surge::Storage::getUserDefaultValue(
            &(this->synth->storage), Surge::Storage::MonoPedalMode, (int)HOLD_ALL_NOTES);
    }

    bool isChecked = (mode == HOLD_ALL_NOTES);

    monoSubMenu.addItem(
        Surge::GUI::toOSCaseForMenu("Sustain Pedal Holds All Notes (No Note Off Retrigger)"), true,
        isChecked, [this, updateDefaults]() {
            this->synth->storage.monoPedalMode = HOLD_ALL_NOTES;
            if (updateDefaults)
            {
                Surge::Storage::updateUserDefaultValue(
                    &(this->synth->storage), Surge::Storage::MonoPedalMode, (int)HOLD_ALL_NOTES);
            }
        });

    isChecked = (mode == RELEASE_IF_OTHERS_HELD);

    monoSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Sustain Pedal Allows Note Off Retrigger"),
                        true, isChecked, [this, updateDefaults]() {
                            this->synth->storage.monoPedalMode = RELEASE_IF_OTHERS_HELD;
                            if (updateDefaults)
                            {
                                Surge::Storage::updateUserDefaultValue(
                                    &(this->synth->storage), Surge::Storage::MonoPedalMode,
                                    (int)RELEASE_IF_OTHERS_HELD);
                            }
                        });

    return monoSubMenu;
}

juce::PopupMenu SurgeGUIEditor::makeTuningMenu(VSTGUI::CRect &menuRect, bool showhelp)
{
    auto tuningSubMenu = juce::PopupMenu();

    auto hu = helpURLForSpecial("tun-menu");

    if (hu != "" && showhelp)
    {
        auto lurl = fullyResolvedHelpURL(hu);

        tuningSubMenu.addItem("[?] Tuning ",
                              [lurl]() { juce::URL(lurl).launchInDefaultBrowser(); });

        tuningSubMenu.addSeparator();
    }

    bool isTuningEnabled = !synth->storage.isStandardTuning;
    bool isScaleEnabled = !synth->storage.isStandardScale;
    bool isMappingEnabled = !synth->storage.isStandardMapping;

    if (isScaleEnabled)
    {
        auto tuningLabel = Surge::GUI::toOSCaseForMenu("Current Tuning: ");

        if (synth->storage.currentScale.description.empty())
        {
            tuningLabel += path_to_string(fs::path(synth->storage.currentScale.name).stem());
        }
        else
        {
            tuningLabel += synth->storage.currentScale.description;
        }

        tuningSubMenu.addItem(tuningLabel, false, false, []() {});
    }

    if (isMappingEnabled)
    {
        auto mappingLabel = Surge::GUI::toOSCaseForMenu("Current Keyboard Mapping: ");
        mappingLabel += path_to_string(fs::path(synth->storage.currentMapping.name).stem());

        tuningSubMenu.addItem(mappingLabel, false, false, []() {});
    }

    if (isTuningEnabled || isMappingEnabled)
    {
        tuningSubMenu.addSeparator();
    }

    tuningSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Set to Standard Tuning"),
                          (!this->synth->storage.isStandardTuning), false, [this]() {
                              this->synth->storage.retuneTo12TETScaleC261Mapping();
                              this->synth->refresh_editor = true;
                          });

    tuningSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Set to Standard Scale (12-TET)"),
                          (!this->synth->storage.isStandardScale), false, [this]() {
                              this->synth->storage.retuneTo12TETScale();
                              this->synth->refresh_editor = true;
                          });

    tuningSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Set to Standard Mapping (Concert C)"),
                          (!this->synth->storage.isStandardMapping), false, [this]() {
                              this->synth->storage.remapToConcertCKeyboard();
                              this->synth->refresh_editor = true;
                          });

    tuningSubMenu.addSeparator();

    tuningSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Load .scl Scale..."), [this]() {
        auto cb = [this](std::string sf) {
            std::string sfx = ".scl";
            if (sf.length() >= sfx.length())
            {
                if (sf.compare(sf.length() - sfx.length(), sfx.length(), sfx) != 0)
                {
                    synth->storage.reportError("Please select only .scl files!", "Invalid Choice");
                    std::cout << "FILE is [" << sf << "]" << std::endl;
                    return;
                }
            }
            try
            {
                auto sc = Tunings::readSCLFile(sf);

                if (!this->synth->storage.retuneToScale(sc))
                {
                    synth->storage.reportError("This .scl file is not valid!", "File Format Error");
                    return;
                }
                this->synth->refresh_editor = true;
            }
            catch (Tunings::TuningError &e)
            {
                synth->storage.reportError(e.what(), "Loading Error");
            }
        };

        auto scl_path =
            Surge::Storage::appendDirectory(this->synth->storage.datapath, "tuning-library", "SCL");

        juce::FileChooser c("Select SCL Scale", juce::File(scl_path), "*.scl");

        auto r = c.browseForFileToOpen();

        if (r)
        {
            auto res = c.getResult();
            auto rString = res.getFullPathName().toStdString();
            cb(rString);
        }
    });

    tuningSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Load .kbm Keyboard Mapping..."), [this]() {
        auto cb = [this](std::string sf) {
            std::string sfx = ".kbm";
            if (sf.length() >= sfx.length())
            {
                if (sf.compare(sf.length() - sfx.length(), sfx.length(), sfx) != 0)
                {
                    synth->storage.reportError("Please select only .kbm files!", "Invalid Choice");
                    std::cout << "FILE is [" << sf << "]" << std::endl;
                    return;
                }
            }
            try
            {
                auto kb = Tunings::readKBMFile(sf);

                if (!this->synth->storage.remapToKeyboard(kb))
                {
                    synth->storage.reportError("This .kbm file is not valid!", "File Format Error");
                    return;
                }

                this->synth->refresh_editor = true;
            }
            catch (Tunings::TuningError &e)
            {
                synth->storage.reportError(e.what(), "Loading Error");
            }
        };

        auto kbm_path = Surge::Storage::appendDirectory(this->synth->storage.datapath,
                                                        "tuning-library", "KBM Concert Pitch");
        juce::FileChooser c("Select KBM Mapping", juce::File(kbm_path), "*.kbm");

        auto r = c.browseForFileToOpen();

        if (r)
        {
            auto res = c.getResult();
            auto rString = res.getFullPathName().toStdString();
            cb(rString);
        }
    });

    int oct = 5 - Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                                                      Surge::Storage::MiddleC, 1);
    string middle_A = "A" + to_string(oct);

    tuningSubMenu.addItem(
        Surge::GUI::toOSCaseForMenu("Remap " + middle_A + " (MIDI Note 69) Directly to..."),
        [this, middle_A, menuRect]() {
            char ma[256];
            sprintf(ma, "Remap %s Frequency", middle_A.c_str());

            char c[256];
            snprintf(c, 256, "440.0");
            promptForMiniEdit(c, "Remap MIDI note 69 frequency to: ", ma, menuRect.getTopLeft(),
                              [this](const std::string &s) {
                                  float freq = ::atof(s.c_str());
                                  auto kb = Tunings::tuneA69To(freq);
                                  kb.name = "Note 69 Retuned 440 to " + std::to_string(freq);
                                  if (!this->synth->storage.remapToKeyboard(kb))
                                  {
                                      synth->storage.reportError("This .kbm file is not valid!",
                                                                 "File Format Error");
                                      return;
                                  }
                              });
        });

    tuningSubMenu.addSeparator();

    tuningSubMenu.addItem(
        Surge::GUI::toOSCaseForMenu("Apply Tuning at MIDI Input"), true,
        (synth->storage.tuningApplicationMode == SurgeStorage::RETUNE_MIDI_ONLY), [this]() {
            this->synth->storage.setTuningApplicationMode(SurgeStorage::RETUNE_MIDI_ONLY);
        });

    tuningSubMenu.addItem(
        Surge::GUI::toOSCaseForMenu("Apply Tuning After Modulation"), true,
        (synth->storage.tuningApplicationMode == SurgeStorage::RETUNE_ALL),
        [this]() { this->synth->storage.setTuningApplicationMode(SurgeStorage::RETUNE_ALL); });

    tuningSubMenu.addSeparator();

    bool tsMode = Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                                                      Surge::Storage::UseODDMTS, false);
    std::string txt = "Use ODDSound" + Surge::GUI::toOSCaseForMenu(" MTS-ESP (if Loaded in DAW)");

    tuningSubMenu.addItem(txt, true, tsMode, [this, tsMode]() {
        Surge::Storage::updateUserDefaultValue(&(this->synth->storage), Surge::Storage::UseODDMTS,
                                               !tsMode);
        if (tsMode)
        {
            // We toggled to false
            this->synth->storage.deinitialize_oddsound();
        }
        else
        {
            this->synth->storage.initialize_oddsound();
        }
    });

    if (tsMode && !this->synth->storage.oddsound_mts_client)
    {
        tuningSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Reconnect to MTS-ESP"),
                              [this]() { this->synth->storage.initialize_oddsound(); });
    }

    if (this->synth->storage.oddsound_mts_active && this->synth->storage.oddsound_mts_client)
    {
        tuningSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Disconnect from MTS-ESP"), [this]() {
            auto q = this->synth->storage.oddsound_mts_client;
            this->synth->storage.oddsound_mts_active = false;
            this->synth->storage.oddsound_mts_client = nullptr;
            MTS_DeregisterClient(q);
        });

        tuningSubMenu.addItem(
            Surge::GUI::toOSCaseForMenu("Query Tuning at Note On Only"), true,
            (this->synth->storage.oddsoundRetuneMode == SurgeStorage::RETUNE_NOTE_ON_ONLY),
            [this]() {
                if (this->synth->storage.oddsoundRetuneMode == SurgeStorage::RETUNE_CONSTANT)
                {
                    this->synth->storage.oddsoundRetuneMode = SurgeStorage::RETUNE_NOTE_ON_ONLY;
                }
                else
                {
                    this->synth->storage.oddsoundRetuneMode = SurgeStorage::RETUNE_CONSTANT;
                }
            });

        tuningSubMenu.addItem(Surge::GUI::toOSCaseForMenu("MTS-ESP is Active"), false, false,
                              []() {});

        std::string mtsScale = MTS_GetScaleName(synth->storage.oddsound_mts_client);

        tuningSubMenu.addItem(mtsScale, false, false, []() {});

        return tuningSubMenu;
    }

    tuningSubMenu.addSeparator();

    tuningSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Show Current Tuning Information..."),
                          (!this->synth->storage.isStandardTuning), false,
                          [this]() { showHTML(this->tuningToHtml()); });

    tuningSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Factory Tuning Library..."), [this]() {
        auto dpath =
            Surge::Storage::appendDirectory(this->synth->storage.datapath, "tuning-library");

        juce::URL(juce::File(dpath)).launchInDefaultBrowser();
    });

    return tuningSubMenu;
}

juce::PopupMenu SurgeGUIEditor::makeZoomMenu(VSTGUI::CRect &menuRect, bool showhelp)
{
    auto zoomSubMenu = juce::PopupMenu();

    auto hu = helpURLForSpecial("zoom-menu");

    if (hu != "" && showhelp)
    {
        auto lurl = fullyResolvedHelpURL(hu);

        zoomSubMenu.addItem("[?] Zoom", [lurl]() { juce::URL(lurl).launchInDefaultBrowser(); });

        zoomSubMenu.addSeparator();
    }

    std::vector<int> zoomTos = {{100, 125, 150, 175, 200, 300, 400}};
    bool isFixed = false;

    if (currentSkin->hasFixedZooms())
    {
        isFixed = true;
        zoomTos = currentSkin->getFixedZooms();
    }

    for (auto s : zoomTos) // These are somewhat arbitrary reasonable defaults
    {
        std::string lab = "Zoom to " + std::to_string(s) + "%";

        zoomSubMenu.addItem(lab, true, (s == zoomFactor), [this, s]() { resizeWindow(s); });
    }

    zoomSubMenu.addSeparator();

    if (isFixed)
    {
        /*
        DO WE WANT SOMETHING LIKE THIS?
        zoomSubMenu.addItem(Surge::GUI::toOSCaseForMenu( "About fixed zoom skins..." ), []() {});
        */
    }
    else
    {
        for (auto jog : {-25, -10, 10, 25}) // These are somewhat arbitrary reasonable defaults also
        {
            std::string lab;

            if (jog > 0)
            {
                lab = "Grow by " + std::to_string(jog);
            }
            else
            {
                lab = "Shrink by " + std::to_string(-jog);
            }

            zoomSubMenu.addItem(lab + "%", [this, jog]() { resizeWindow(getZoomFactor() + jog); });
        }

        zoomSubMenu.addSeparator();

        zoomSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Zoom to Largest"), [this]() {
            // regarding that 90 value, see comment in setZoomFactor
            int newZF = findLargestFittingZoomBetween(100.0, 500.0, 5, 90, getWindowSizeX(),
                                                      getWindowSizeY());
            resizeWindow(newZF);
        });

        zoomSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Zoom to Smallest"),
                            [this]() { resizeWindow(minimumZoom); });

        zoomSubMenu.addSeparator();

        auto dzf = Surge::Storage::getUserDefaultValue(&(synth->storage),
                                                       Surge::Storage::DefaultZoom, zoomFactor);

        std::string dss = "Zoom to Default (" + std::to_string(dzf) + "%)";

        zoomSubMenu.addItem(dss, [this, dzf]() { resizeWindow(dzf); });
    }

    zoomSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Set Current Zoom Level as Default"), [this]() {
        Surge::Storage::updateUserDefaultValue(&(synth->storage), Surge::Storage::DefaultZoom,
                                               zoomFactor);
    });

    if (!isFixed)
    {
        zoomSubMenu.addItem(
            Surge::GUI::toOSCaseForMenu("Set Default Zoom Level to..."), [this, menuRect]() {
                char c[256];
                snprintf(c, 256, "%d", (int)zoomFactor);
                promptForMiniEdit(c, "Enter a default zoom level value:", "Set Default Zoom Level",
                                  menuRect.getTopLeft(), [this](const std::string &s) {
                                      int newVal = ::atoi(s.c_str());
                                      Surge::Storage::updateUserDefaultValue(
                                          &(synth->storage), Surge::Storage::DefaultZoom, newVal);
                                      resizeWindow(newVal);
                                  });
            });
    }

    return zoomSubMenu;
}

juce::PopupMenu SurgeGUIEditor::makeMouseBehaviorMenu(VSTGUI::CRect &menuRect)
{
    bool touchMode = Surge::Storage::getUserDefaultValue(&(synth->storage),
                                                         Surge::Storage::TouchMouseMode, false);

    auto mouseMenu = juce::PopupMenu();

    std::string mouseLegacy = "Legacy";
    std::string mouseSlow = "Slow";
    std::string mouseMedium = "Medium";
    std::string mouseExact = "Exact";

    bool checked = Surge::Widgets::ModulatableSlider::sliderMoveRateState ==
                   Surge::Widgets::ModulatableSlider::kLegacy;
    bool enabled = !touchMode;

    mouseMenu.addItem(mouseLegacy, enabled, checked, [this]() {
        Surge::Widgets::ModulatableSlider::sliderMoveRateState =
            Surge::Widgets::ModulatableSlider::kLegacy;
        Surge::Storage::updateUserDefaultValue(
            &(this->synth->storage), Surge::Storage::SliderMoveRateState,
            Surge::Widgets::ModulatableSlider::sliderMoveRateState);
    });

    checked = Surge::Widgets::ModulatableSlider::sliderMoveRateState ==
              Surge::Widgets::ModulatableSlider::kSlow;

    mouseMenu.addItem(mouseSlow, enabled, checked, [this]() {
        Surge::Widgets::ModulatableSlider::sliderMoveRateState =
            Surge::Widgets::ModulatableSlider::kSlow;
        Surge::Storage::updateUserDefaultValue(
            &(this->synth->storage), Surge::Storage::SliderMoveRateState,
            Surge::Widgets::ModulatableSlider::sliderMoveRateState);
    });

    checked = Surge::Widgets::ModulatableSlider::sliderMoveRateState ==
              Surge::Widgets::ModulatableSlider::kMedium;

    mouseMenu.addItem(mouseMedium, enabled, checked, [this]() {
        Surge::Widgets::ModulatableSlider::sliderMoveRateState =
            Surge::Widgets::ModulatableSlider::kMedium;
        Surge::Storage::updateUserDefaultValue(
            &(this->synth->storage), Surge::Storage::SliderMoveRateState,
            Surge::Widgets::ModulatableSlider::sliderMoveRateState);
    });

    checked = Surge::Widgets::ModulatableSlider::sliderMoveRateState ==
              Surge::Widgets::ModulatableSlider::kExact;

    mouseMenu.addItem(mouseExact, enabled, checked, [this]() {
        Surge::Widgets::ModulatableSlider::sliderMoveRateState =
            Surge::Widgets::ModulatableSlider::kExact;
        Surge::Storage::updateUserDefaultValue(
            &(this->synth->storage), Surge::Storage::SliderMoveRateState,
            Surge::Widgets::ModulatableSlider::sliderMoveRateState);
    });

    mouseMenu.addSeparator();

    bool tsMode = Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                                                      Surge::Storage::ShowCursorWhileEditing, true);

    mouseMenu.addItem(Surge::GUI::toOSCaseForMenu("Show Cursor While Editing"), enabled, tsMode,
                      [this, tsMode]() {
                          Surge::Storage::updateUserDefaultValue(
                              &(this->synth->storage), Surge::Storage::ShowCursorWhileEditing,
                              !tsMode);
                      });

    mouseMenu.addSeparator();

    mouseMenu.addItem(Surge::GUI::toOSCaseForMenu("Touchscreen Mode"), true, touchMode,
                      [this, touchMode]() {
                          Surge::Storage::updateUserDefaultValue(
                              &(this->synth->storage), Surge::Storage::TouchMouseMode, !touchMode);
                      });

    return mouseMenu;
}

juce::PopupMenu SurgeGUIEditor::makePatchDefaultsMenu(VSTGUI::CRect &menuRect)
{
    auto patchDefMenu = juce::PopupMenu();

    patchDefMenu.addItem(Surge::GUI::toOSCaseForMenu("Set Default Patch Author..."), [this,
                                                                                      menuRect]() {
        string s = Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                                                       Surge::Storage::DefaultPatchAuthor, "");
        char txt[256];
        txt[0] = 0;
        if (Surge::Storage::isValidUTF8(s))
            strxcpy(txt, s.c_str(), 256);
        promptForMiniEdit(txt, "Enter default patch author name:", "Set Default Patch Author",
                          menuRect.getTopLeft(), [this](const std::string &s) {
                              Surge::Storage::updateUserDefaultValue(
                                  &(this->synth->storage), Surge::Storage::DefaultPatchAuthor, s);
                          });
    });

    patchDefMenu.addItem(Surge::GUI::toOSCaseForMenu("Set Default Patch Comment..."), [this,
                                                                                       menuRect]() {
        string s = Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                                                       Surge::Storage::DefaultPatchComment, "");
        char txt[256];
        txt[0] = 0;
        if (Surge::Storage::isValidUTF8(s))
            strxcpy(txt, s.c_str(), 256);
        promptForMiniEdit(txt, "Enter default patch comment text:", "Set Default Patch Comment",
                          menuRect.getTopLeft(), [this](const std::string &s) {
                              Surge::Storage::updateUserDefaultValue(
                                  &(this->synth->storage), Surge::Storage::DefaultPatchComment, s);
                          });
    });

    patchDefMenu.addSeparator();

    auto catCurId =
        &(this->synth->storage).patch_category[patchSelector->getCurrentCategoryId()].name;
    auto patchCurId = &(this->synth->storage).patch_list[patchSelector->getCurrentPatchId()].name;

    patchDefMenu.addItem(
        Surge::GUI::toOSCaseForMenu("Set Current Patch as Default"),
        [this, catCurId, patchCurId]() {
            Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                   Surge::Storage::InitialPatchName, *patchCurId);

            Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                   Surge::Storage::InitialPatchCategory, *catCurId);
        });

    return patchDefMenu;
}

juce::PopupMenu SurgeGUIEditor::makeValueDisplaysMenu(VSTGUI::CRect &menuRect)
{
    auto dispDefMenu = juce::PopupMenu();

    bool precReadout = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::HighPrecisionReadouts, false);

    dispDefMenu.addItem(Surge::GUI::toOSCaseForMenu("High Precision Value Readouts"), true,
                        precReadout, [this, precReadout]() {
                            Surge::Storage::updateUserDefaultValue(
                                &(this->synth->storage), Surge::Storage::HighPrecisionReadouts,
                                !precReadout);
                        });

    // modulation value readout shows bounds
    bool modValues = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::ModWindowShowsValues, false);

    dispDefMenu.addItem(Surge::GUI::toOSCaseForMenu("Modulation Value Readout Shows Bounds"), true,
                        modValues, [this, modValues]() {
                            Surge::Storage::updateUserDefaultValue(
                                &(this->synth->storage), Surge::Storage::ModWindowShowsValues,
                                !modValues);
                        });

    bool lfoone = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::ShowGhostedLFOWaveReference, true);

    dispDefMenu.addItem(Surge::GUI::toOSCaseForMenu("Show Ghosted LFO Waveform Reference"), true,
                        lfoone, [this, lfoone]() {
                            Surge::Storage::updateUserDefaultValue(
                                &(this->synth->storage),
                                Surge::Storage::ShowGhostedLFOWaveReference, !lfoone);
                            this->frame->repaint();
                        });

    // Middle C submenu
    auto middleCSubMenu = juce::PopupMenu();

    auto mcValue =
        Surge::Storage::getUserDefaultValue(&(this->synth->storage), Surge::Storage::MiddleC, 1);

    middleCSubMenu.addItem("C3", true, (mcValue == 2), [this]() {
        Surge::Storage::updateUserDefaultValue(&(this->synth->storage), Surge::Storage::MiddleC, 2);
        synth->refresh_editor = true;
    });

    middleCSubMenu.addItem("C4", true, (mcValue == 1), [this]() {
        Surge::Storage::updateUserDefaultValue(&(this->synth->storage), Surge::Storage::MiddleC, 1);
        synth->refresh_editor = true;
    });

    middleCSubMenu.addItem("C5", true, (mcValue == 0), [this]() {
        Surge::Storage::updateUserDefaultValue(&(this->synth->storage), Surge::Storage::MiddleC, 0);
        synth->refresh_editor = true;
    });

    dispDefMenu.addSubMenu("Middle C", middleCSubMenu);

    return dispDefMenu;
}

juce::PopupMenu SurgeGUIEditor::makeWorkflowMenu(VSTGUI::CRect &menuRect)
{
    auto wfMenu = juce::PopupMenu();

    // activate individual scene outputs
    wfMenu.addItem(Surge::GUI::toOSCaseForMenu("Activate Individual Scene Outputs"), true,
                   (synth->activateExtraOutputs), [this]() {
                       this->synth->activateExtraOutputs = !this->synth->activateExtraOutputs;
                       Surge::Storage::updateUserDefaultValue(
                           &(this->synth->storage), Surge::Storage::ActivateExtraOutputs,
                           this->synth->activateExtraOutputs ? 1 : 0);
                   });

    bool msegSnapMem = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::RestoreMSEGSnapFromPatch, true);

    wfMenu.addItem(Surge::GUI::toOSCaseForMenu("Load MSEG Snap State from Patch"), true,
                   msegSnapMem, [this, msegSnapMem]() {
                       Surge::Storage::updateUserDefaultValue(
                           &(this->synth->storage), Surge::Storage::RestoreMSEGSnapFromPatch,
                           !msegSnapMem);
                   });

    // wrap around browsing patches within current category
    bool patchJogWrap = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::PatchJogWraparound, true);

    wfMenu.addItem(
        Surge::GUI::toOSCaseForMenu("Previous/Next Patch Constrained to Current Category"), true,
        patchJogWrap, [this, patchJogWrap]() {
            Surge::Storage::updateUserDefaultValue(
                &(this->synth->storage), Surge::Storage::PatchJogWraparound, !patchJogWrap);
        });

    // remember tab positions per scene
    bool tabPosMem = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::RememberTabPositionsPerScene, false);

    wfMenu.addItem(Surge::GUI::toOSCaseForMenu("Remember Tab Positions Per Scene"), true, tabPosMem,
                   [this, tabPosMem]() {
                       Surge::Storage::updateUserDefaultValue(
                           &(this->synth->storage), Surge::Storage::RememberTabPositionsPerScene,
                           !tabPosMem);
                   });

    bool showVirtualKeyboard = getShowVirtualKeyboard();

    wfMenu.addItem(Surge::GUI::toOSCaseForMenu("Show Virtual Keyboard"), true, showVirtualKeyboard,
                   [this, showVirtualKeyboard]() {
                       setShowVirtualKeyboard(!showVirtualKeyboard);
                       resizeWindow(zoomFactor);
                   });

    return wfMenu;
}

bool SurgeGUIEditor::getShowVirtualKeyboard()
{
    auto key = Surge::Storage::ShowVirtualKeyboard_Plugin;
    bool defaultVal = false;
    if (juceEditor->processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        key = Surge::Storage::ShowVirtualKeyboard_Standalone;
        defaultVal = false;
    }
    return Surge::Storage::getUserDefaultValue(&(this->synth->storage), key, defaultVal);
}

void SurgeGUIEditor::setShowVirtualKeyboard(bool b)
{
    auto key = Surge::Storage::ShowVirtualKeyboard_Plugin;
    if (juceEditor->processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        key = Surge::Storage::ShowVirtualKeyboard_Standalone;
    }
    Surge::Storage::updateUserDefaultValue(&(this->synth->storage), key, b);
}

juce::PopupMenu SurgeGUIEditor::makeSkinMenu(VSTGUI::CRect &menuRect)
{
    auto skinSubMenu = juce::PopupMenu();

    auto &db = Surge::GUI::SkinDB::get();
    bool hasTests = false;

    // TODO: Later allow nesting
    std::map<std::string, std::vector<Surge::GUI::SkinDB::Entry>> entryByCategory;

    for (auto &entry : db.getAvailableSkins())
    {
        entryByCategory[entry.category].push_back(entry);
    }

    for (auto pr : entryByCategory)
    {
        auto catMen = juce::PopupMenu();
        auto addToThis = &skinSubMenu;
        auto cat = pr.first;

        if (cat != "")
        {
            addToThis = &catMen;
        }

        for (auto &entry : pr.second)
        {
            auto dname = entry.displayName;

            if (useDevMenu)
            {
                dname += " (";
                if (entry.root.find(synth->storage.datapath) != std::string::npos)
                {
                    dname += "factory";
                }
                else if (entry.root.find(synth->storage.userDataPath) != std::string::npos)
                {
                    dname += "user";
                }
                else
                {
                    dname += "other";
                }

                dname += PATH_SEPARATOR + entry.name + ")";
            }

            auto checked = entry.matchesSkin(currentSkin);

            addToThis->addItem(dname, true, checked, [this, entry]() {
                setupSkinFromEntry(entry);
                this->synth->refresh_editor = true;
                Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                       Surge::Storage::DefaultSkin, entry.name);
                Surge::Storage::updateUserDefaultValue(
                    &(this->synth->storage), Surge::Storage::DefaultSkinRootType, entry.rootType);
            });
        }

        if (cat != "")
        {
            skinSubMenu.addSubMenu(cat, *addToThis);
        }
    }

    skinSubMenu.addSeparator();

    if (useDevMenu)
    {
        auto f5Value = Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                                                           Surge::Storage::SkinReloadViaF5, 0);

        skinSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Use F5 To Reload Current Skin"), true,
                            (f5Value == 1), [this, f5Value]() {
                                Surge::Storage::updateUserDefaultValue(
                                    &(this->synth->storage), Surge::Storage::SkinReloadViaF5,
                                    f5Value ? 0 : 1);
                            });

        int pxres = Surge::Storage::getUserDefaultValue(&(synth->storage),
                                                        Surge::Storage::LayoutGridResolution, 16);

        auto m = std::string("Show Layout Grid (") + std::to_string(pxres) + " px)";

        skinSubMenu.addItem(Surge::GUI::toOSCaseForMenu(m),
                            [this, pxres]() { this->showAboutScreen(pxres); });

        skinSubMenu.addItem(
            Surge::GUI::toOSCaseForMenu("Change Layout Grid Resolution..."), [this, pxres]() {
                this->promptForMiniEdit(
                    std::to_string(pxres), "Enter new resolution:", "Layout Grid Resolution",
                    CPoint(400, 400), [this](const std::string &s) {
                        Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                               Surge::Storage::LayoutGridResolution,
                                                               std::atoi(s.c_str()));
                    });
            });

        skinSubMenu.addSeparator();
    }

    skinSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Reload Current Skin"), [this]() {
        this->bitmapStore.reset(new SurgeBitmaps());
        this->bitmapStore->setupBuiltinBitmaps();

        if (!this->currentSkin->reloadSkin(this->bitmapStore))
        {
            auto &db = Surge::GUI::SkinDB::get();
            auto msg = std::string("Unable to load skin! Reverting the skin to "
                                   "Surge Classic.\n\nSkin error:\n") +
                       db.getAndResetErrorString();
            this->currentSkin = db.defaultSkin(&(this->synth->storage));
            this->currentSkin->reloadSkin(this->bitmapStore);
            synth->storage.reportError(msg, "Skin Loading Error");
        }

        reloadFromSkin();
        this->synth->refresh_editor = true;
    });

    skinSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Rescan Skins"), [this]() {
        auto r = this->currentSkin->root;
        auto n = this->currentSkin->name;

        auto &db = Surge::GUI::SkinDB::get();
        db.rescanForSkins(&(this->synth->storage));

        // So go find the skin
        auto e = db.getEntryByRootAndName(r, n);
        if (e.isJust())
        {
            setupSkinFromEntry(e.fromJust());
        }
        else
        {
            setupSkinFromEntry(db.getDefaultSkinEntry());
        }
        this->synth->refresh_editor = true;
    });

    skinSubMenu.addSeparator();

    if (useDevMenu)
    {
        skinSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Open Current Skin Folder..."), [this]() {
            Surge::GUI::openFileOrFolder(this->currentSkin->root + this->currentSkin->name);
        });
    }
    else
    {
        skinSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Install a New Skin..."), [this]() {
            std::string skinspath =
                Surge::Storage::appendDirectory(this->synth->storage.userDataPath, "Skins");
            // make it if it isn't there
            fs::create_directories(string_to_path(skinspath));
            Surge::GUI::openFileOrFolder(skinspath);
        });
    }

    skinSubMenu.addSeparator();

    skinSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Show Skin Inspector..."),
                        [this]() { showHTML(skinInspectorHtml()); });

    skinSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Skin Development Guide..."), []() {
        juce::URL("https://surge-synthesizer.github.io/skin-manual.html");
    });

    return skinSubMenu;
}

juce::PopupMenu SurgeGUIEditor::makeDataMenu(VSTGUI::CRect &menuRect)
{
    auto dataSubMenu = juce::PopupMenu();

    dataSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Open Factory Data Folder..."),
                        [this]() { Surge::GUI::openFileOrFolder(this->synth->storage.datapath); });

    dataSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Open User Data Folder..."), [this]() {
        // make it if it isn't there
        fs::create_directories(string_to_path(this->synth->storage.userDataPath));
        Surge::GUI::openFileOrFolder(this->synth->storage.userDataPath);
    });

    dataSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Set Custom User Data Folder..."), [this]() {
        auto cb = [this](std::string f) {
            // FIXME - check if f is a path
            this->synth->storage.userDataPath = f;
            Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                   Surge::Storage::UserDataPath, f);
            this->synth->storage.refresh_wtlist();
            this->synth->storage.refresh_patchlist();
        };
        /*
         * TODO: Implement JUCE directory picker
        Surge::UserInteractions::promptFileOpenDialog(this->synth->storage.userDataPath,
        "", "", cb, true, true);
                                                      */
    });

    dataSubMenu.addSeparator();

    dataSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Rescan All Data Folders"), [this]() {
        this->synth->storage.refresh_wtlist();
        this->synth->storage.refresh_patchlist();
        this->scannedForMidiPresets = false;

        Surge::FxUserPreset::forcePresetRescan(&(this->synth->storage));
        Surge::ModulatorPreset::forcePresetRescan();

        // Rescan for skins
        auto r = this->currentSkin->root;
        auto n = this->currentSkin->name;

        auto &db = Surge::GUI::SkinDB::get();
        db.rescanForSkins(&(this->synth->storage));

        // So go find the skin
        auto e = db.getEntryByRootAndName(r, n);

        if (e.isJust())
        {
            setupSkinFromEntry(e.fromJust());
        }
        else
        {
            setupSkinFromEntry(db.getDefaultSkinEntry());
        }

        // Will need to rebuild the FX menu also so...
        this->synth->refresh_editor = true;
    });

    return dataSubMenu;
}

// builds a menu for setting controller smoothing, used in ::makeMidiMenu and ::makeMpeMenu
// key is the key given to getUserDefaultValue,
// default is a value to default to,
// setSmooth is a function called to set the smoothing value
juce::PopupMenu SurgeGUIEditor::makeSmoothMenu(
    VSTGUI::CRect &menuRect, const Surge::Storage::DefaultKey &key, int defaultValue,
    std::function<void(ControllerModulationSource::SmoothingMode)> setSmooth)
{
    auto smoothMenu = juce::PopupMenu();

    int smoothing = Surge::Storage::getUserDefaultValue(&(synth->storage), key, defaultValue);

    auto asmt = [this, &smoothMenu, smoothing,
                 setSmooth](const char *label, ControllerModulationSource::SmoothingMode md) {
        smoothMenu.addItem(label, true, (smoothing == md), [setSmooth, md]() { setSmooth(md); });
    };

    asmt("Legacy", ControllerModulationSource::SmoothingMode::LEGACY);
    asmt("Slow Exponential", ControllerModulationSource::SmoothingMode::SLOW_EXP);
    asmt("Fast Exponential", ControllerModulationSource::SmoothingMode::FAST_EXP);
    asmt("Fast Linear", ControllerModulationSource::SmoothingMode::FAST_LINE);
    asmt("No Smoothing", ControllerModulationSource::SmoothingMode::DIRECT);

    return smoothMenu;
};

juce::PopupMenu SurgeGUIEditor::makeMidiMenu(VSTGUI::CRect &menuRect)
{
    auto midiSubMenu = juce::PopupMenu();

    auto smen = makeSmoothMenu(menuRect, Surge::Storage::SmoothingMode,
                               (int)ControllerModulationSource::SmoothingMode::LEGACY,
                               [this](auto md) { this->resetSmoothing(md); });
    midiSubMenu.addSubMenu(Surge::GUI::toOSCaseForMenu("Controller Smoothing"), smen);

    // TODO: include this after going through the parameter RMB context menu forest!
    // auto mmom = makeMonoModeOptionsMenu(menuRect, true);
    // midiSubMenu->addEntry(mmom, Surge::GUI::toOSCaseForMenu("Sustain Pedal In Mono Mode"));

    midiSubMenu.addSeparator();

    midiSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Save MIDI Mapping As..."), [this, menuRect]() {
        this->scannedForMidiPresets = false; // force a rescan
        char msn[256];
        msn[0] = 0;
        promptForMiniEdit(
            msn, "MIDI Mapping Name", "Save MIDI Mapping", menuRect.getTopLeft(),
            [this](const std::string &s) { this->synth->storage.storeMidiMappingToName(s); });
    });

    midiSubMenu.addItem(
        Surge::GUI::toOSCaseForMenu("Set Current MIDI Mapping as Default"),
        [this]() { this->synth->storage.write_midi_controllers_to_user_default(); });

    midiSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Clear Current MIDI Mapping"), [this]() {
        int n = n_global_params + n_scene_params;

        for (int i = 0; i < n; i++)
        {
            this->synth->storage.getPatch().param_ptr[i]->midictrl = -1;
            if (i > n_global_params)
                this->synth->storage.getPatch().param_ptr[i + n_scene_params]->midictrl = -1;
        }
    });

    midiSubMenu.addSeparator();

    midiSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Show Current MIDI Mapping..."),
                        [this]() { showHTML(this->midiMappingToHtml()); });

    if (!scannedForMidiPresets)
    {
        scannedForMidiPresets = true;
        synth->storage.rescanUserMidiMappings();
    }

    bool gotOne = false;

    for (const auto &p : synth->storage.userMidiMappingsXMLByName)
    {
        if (!gotOne)
        {
            gotOne = true;
            midiSubMenu.addSeparator();
        }

        midiSubMenu.addItem(p.first,
                            [this, p] { this->synth->storage.loadMidiMappingByName(p.first); });
    }

    return midiSubMenu;
}

void SurgeGUIEditor::reloadFromSkin()
{
    if (!frame || !bitmapStore.get())
        return;

    float dbs = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->scale;
    bitmapStore->setPhysicalZoomFactor(getZoomFactor() * dbs);

    paramInfowindow->setSkin(currentSkin, bitmapStore);

    auto bg = currentSkin->customBackgroundImage();

    if (bg != "")
    {
        auto *cbm = bitmapStore->getDrawableByStringID(bg);
        frame->setBackground(cbm);
    }
    else
    {
        auto *cbm = bitmapStore->getDrawable(IDB_MAIN_BG);
        frame->setBackground(cbm);
    }

    wsx = currentSkin->getWindowSizeX();
    wsy = currentSkin->getWindowSizeY();

    float sf = 1;

    frame->setSize(wsx * sf, wsy * sf);

    setZoomFactor(getZoomFactor(), true);

    // update MSEG editor if opened
    if (isAnyOverlayPresent(MSEG_EDITOR))
    {
        showMSEGEditor();
    }

    // adjust JUCE look and feel colors
    auto setJUCEColour = [this](int id, juce::Colour x) {
        juceEditor->getLookAndFeel().setColour(id, x);
    };

    setJUCEColour(juce::DocumentWindow::backgroundColourId, juce::Colour(48, 48, 48));
    setJUCEColour(juce::TextButton::buttonColourId, juce::Colour(32, 32, 32));
    setJUCEColour(juce::TextEditor::backgroundColourId, juce::Colour(32, 32, 32));
    setJUCEColour(juce::ListBox::backgroundColourId, juce::Colour(32, 32, 32));
    setJUCEColour(juce::ListBox::backgroundColourId, juce::Colour(32, 32, 32));
    setJUCEColour(juce::ScrollBar::thumbColourId, juce::Colour(212, 212, 212));
    setJUCEColour(juce::ScrollBar::trackColourId, juce::Colour(128, 128, 128));
    setJUCEColour(juce::Slider::thumbColourId, juce::Colour(212, 212, 212));
    setJUCEColour(juce::Slider::trackColourId, juce::Colour(128, 128, 128));
    setJUCEColour(juce::Slider::backgroundColourId, juce::Colour(0x15FFFFFF));
    setJUCEColour(juce::ComboBox::backgroundColourId, juce::Colour(32, 32, 32));
    setJUCEColour(juce::PopupMenu::backgroundColourId, juce::Colour(48, 48, 48));
    setJUCEColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(96, 96, 96));
}

juce::PopupMenu SurgeGUIEditor::makeDevMenu(VSTGUI::CRect &menuRect)
{
    auto devSubMenu = juce::PopupMenu();

#if WINDOWS
    devSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Show Debug Console..."),
                       []() { Surge::Debug::toggleConsole(); });
#endif

#ifdef INSTRUMENT_UI
    devSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Show UI Instrumentation..."),
                       []() { Surge::Debug::report(); });
#endif

    return devSubMenu;
}

int SurgeGUIEditor::findLargestFittingZoomBetween(
    int zoomLow,                     // bottom of range
    int zoomHigh,                    // top of range
    int zoomQuanta,                  // step size
    int percentageOfScreenAvailable, // How much to shrink actual screen
    float baseW, float baseH)
{
    // Here is a very crude implementation
    int result = zoomHigh;
    CRect screenDim =
        CRect(juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->totalArea);
    float sx = screenDim.getWidth() * percentageOfScreenAvailable / 100.0;
    float sy = screenDim.getHeight() * percentageOfScreenAvailable / 100.0;

    while (result > zoomLow)
    {
        if (result * baseW / 100.0 <= sx && result * baseH / 100.0 <= sy)
            break;
        result -= zoomQuanta;
    }
    if (result < zoomLow)
        result = zoomLow;

    return result;
}

void SurgeGUIEditor::forceautomationchangefor(Parameter *p)
{
    std::cout << "FIXME - REMOVE THIS" << __func__ << std::endl;
}
//------------------------------------------------------------------------------------------------

void SurgeGUIEditor::promptForUserValueEntry(Parameter *p, juce::Component *c, int ms)
{
    if (typeinParamEditor->isVisible())
    {
        typeinParamEditor->setVisible(false);
    }

    typeinParamEditor->setSkin(currentSkin, bitmapStore);

    bool ismod = p && ms > 0;

    jassert(c);
    if (p)
    {
        if (!p->can_setvalue_from_string())
        {
            return;
        }
    }

    if (p)
    {
        typeinParamEditor->setTypeinMode(Surge::Overlays::TypeinParamEditor::Param);
    }
    else
    {
        typeinParamEditor->setTypeinMode(Surge::Overlays::TypeinParamEditor::Control);
    }

    std::string lab = "";
    if (p)
    {
        if (p->ctrlgroup == cg_LFO)
        {
            char pname[1024];
            p->create_fullname(p->get_name(), pname, p->ctrlgroup, p->ctrlgroup_entry,
                               modulatorName(p->ctrlgroup_entry, true).c_str());
            lab = pname;
        }
        else
        {
            lab = p->get_full_name();
        }
    }
    else
    {
        lab = modulatorName(ms, false);
    }

    typeinParamEditor->setMainLabel(lab);

    char txt[256];
    char ptext[1024], ptext2[1024];
    ptext2[0] = 0;
    if (p)
    {
        if (ismod)
        {
            char txt2[256];
            p->get_display_of_modulation_depth(txt, synth->getModDepth(p->id, (modsources)ms),
                                               synth->isBipolarModulation((modsources)ms),
                                               Parameter::TypeIn);
            p->get_display(txt2);
            sprintf(ptext, "mod: %s", txt);
            sprintf(ptext2, "current: %s", txt2);
        }
        else
        {
            p->get_display(txt);
            sprintf(ptext, "current: %s", txt);
        }
    }
    else
    {
        int detailedMode = Surge::Storage::getUserDefaultValue(
            &(this->synth->storage), Surge::Storage::HighPrecisionReadouts, 0);
        auto cms = ((ControllerModulationSource *)synth->storage.getPatch()
                        .scene[current_scene]
                        .modsources[ms]);
        sprintf(txt, "%.*f %%", (detailedMode ? 6 : 2), 100.0 * cms->get_output());
        sprintf(ptext, "current: %s", txt);
    }

    typeinParamEditor->setValueLabels(ptext, ptext2);
    typeinParamEditor->setEditableText(txt);

    if (ismod)
    {
        std::string mls = std::string("by ") + (char *)modulatorName(ms, true).c_str();
        typeinParamEditor->setModByLabel(mls);
    }

    typeinParamEditor->setEditedParam(p);
    typeinParamEditor->setModulation(p && ms > 0, (modsources)ms);

    if (frame->getIndexOfChildComponent(typeinParamEditor.get()) < 0)
    {
        frame->addAndMakeVisible(*typeinParamEditor);
    }
    typeinParamEditor->setBoundsToAccompany(c->getBounds(), frame->getBounds());
    typeinParamEditor->setVisible(true);
    typeinParamEditor->grabFocus();
}

std::string SurgeGUIEditor::modulatorName(int i, bool button)
{
    if ((i >= ms_lfo1 && i <= ms_slfo6))
    {
        int idx = i - ms_lfo1;
        bool isS = idx >= 6;
        int fnum = idx % 6;
        auto *lfodata = &(synth->storage.getPatch().scene[current_scene].lfo[i - ms_lfo1]);

        if (lfodata->shape.val.i == lt_envelope)
        {
            char txt[64];
            if (button)
                sprintf(txt, "%sENV %d", (isS ? "S-" : ""), fnum + 1);
            else
                sprintf(txt, "%s Envelope %d", (isS ? "Scene" : "Voice"), fnum + 1);
            return std::string(txt);
        }
        else if (lfodata->shape.val.i == lt_stepseq)
        {
            char txt[64];
            if (button)
                sprintf(txt, "%sSEQ %d", (isS ? "S-" : ""), fnum + 1);
            else
                sprintf(txt, "%s Step Sequencer %d", (isS ? "Scene" : "Voice"), fnum + 1);
            return std::string(txt);
        }
        else if (lfodata->shape.val.i == lt_mseg)
        {
            char txt[64];
            if (button)
                sprintf(txt, "%sMSEG %d", (isS ? "S-" : ""), fnum + 1);
            else
                sprintf(txt, "%s MSEG %d", (isS ? "Scene" : "Voice"), fnum + 1);
            return std::string(txt);
        }
        else if (lfodata->shape.val.i == lt_formula)
        {
            char txt[64];

            // TODO FIXME: When function LFO type is added, uncomment the second sprintf and remove
            // the first one!
            if (button)
                sprintf(txt, "%sFORM %d", (isS ? "S-" : ""), fnum + 1);
            else
                sprintf(txt, "%s Formula %d", (isS ? "Scene" : "Voice"), fnum + 1);
            return std::string(txt);
        }
    }

    if (i >= ms_ctrl1 && i <= ms_ctrl8)
    {
        std::string ccl =
            std::string(synth->storage.getPatch().CustomControllerLabel[i - ms_ctrl1]);
        if (ccl == "-")
        {
            return std::string(modsource_names[i]);
        }
        else
        {
            return ccl + " (" + modsource_names[i] + ")";
        }
    }
    if (button)
        return std::string(modsource_names_button[i]);
    else
        return std::string(modsource_names[i]);
}

std::string SurgeGUIEditor::helpURLFor(Parameter *p)
{
    auto storage = &(synth->storage);
#if 0 // useful debug
   static bool once = false;
   if( ! once )
   {
      once = true;
      for( auto hp : storage->helpURL_paramidentifier )
      {
         auto k = hp.first;
         bool found = false;
         for (auto iter = synth->storage.getPatch().param_ptr.begin();
              iter != synth->storage.getPatch().param_ptr.end(); iter++)
         {
            Parameter* q = *iter;
            if( ! q ) break;
            if( q->ui_identifier == k )
            {
               found = true;
               break;
            }
         }
         if( ! found )
            std::cout << "UNFOUND : " << k << std::endl;
      }
   }
#endif
    std::string id = p->ui_identifier;
    int type = -1;
    if (p->ctrlgroup == cg_OSC)
    {
        type = storage->getPatch().scene[current_scene].osc[current_osc[current_scene]].type.val.i;
    }
    if (p->ctrlgroup == cg_FX)
    {
        type = storage->getPatch().fx[current_fx].type.val.i;
    }
    if (type >= 0)
    {
        auto key = std::make_pair(id, type);
        if (storage->helpURL_paramidentifier_typespecialized.find(key) !=
            storage->helpURL_paramidentifier_typespecialized.end())
        {
            auto r = storage->helpURL_paramidentifier_typespecialized[key];
            if (r != "")
                return r;
        }
    }
    if (storage->helpURL_paramidentifier.find(id) != storage->helpURL_paramidentifier.end())
    {
        auto r = storage->helpURL_paramidentifier[id];
        if (r != "")
            return r;
    }
    if (storage->helpURL_controlgroup.find(p->ctrlgroup) != storage->helpURL_controlgroup.end())
    {
        auto r = storage->helpURL_controlgroup[p->ctrlgroup];
        if (r != "")
            return r;
    }
    return "";
}

std::string SurgeGUIEditor::helpURLForSpecial(std::string key)
{
    auto storage = &(synth->storage);
    return helpURLForSpecial(storage, key);
}

std::string SurgeGUIEditor::helpURLForSpecial(SurgeStorage *storage, std::string key)
{
    if (storage->helpURL_specials.find(key) != storage->helpURL_specials.end())
    {
        auto r = storage->helpURL_specials[key];
        if (r != "")
            return r;
    }
    return "";
}
std::string SurgeGUIEditor::fullyResolvedHelpURL(std::string helpurl)
{
    std::string lurl = helpurl;
    if (helpurl[0] == '#')
    {
        lurl = "https://surge-synthesizer.github.io/manual/" + helpurl;
    }
    return lurl;
}

void SurgeGUIEditor::setupSkinFromEntry(const Surge::GUI::SkinDB::Entry &entry)
{
    auto &db = Surge::GUI::SkinDB::get();
    auto s = db.getSkin(entry);
    this->currentSkin = s;
    this->bitmapStore.reset(new SurgeBitmaps());
    this->bitmapStore->setupBuiltinBitmaps();
    if (!this->currentSkin->reloadSkin(this->bitmapStore))
    {
        std::ostringstream oss;
        oss << "Unable to load " << entry.root << entry.name
            << " skin! Reverting the skin to Surge Classic.\n\nSkin Error:\n"
            << db.getAndResetErrorString();

        auto msg = std::string(oss.str());
        this->currentSkin = db.defaultSkin(&(this->synth->storage));
        this->currentSkin->reloadSkin(this->bitmapStore);
        synth->storage.reportError(msg, "Skin Loading Error");
    }
    reloadFromSkin();
}

void SurgeGUIEditor::sliderHoverStart(int tag)
{
    int ptag = tag - start_paramtags;
    for (int k = 1; k < n_modsources; k++)
    {
        modsources ms = (modsources)k;
        if (synth->isActiveModulation(ptag, ms))
        {
            if (gui_modsrc[k])
            {
                gui_modsrc[k]->setSecondaryHover(true);
            }
        }
    };
}
void SurgeGUIEditor::sliderHoverEnd(int tag)
{
    for (int k = 1; k < n_modsources; k++)
    {
        if (gui_modsrc[k])
        {
            gui_modsrc[k]->setSecondaryHover(false);
        }
    }
}

void SurgeGUIEditor::dismissEditorOfType(OverlayTags ofType)
{
    if (juceOverlays.empty())
        return;

    if (juceOverlays.find(ofType) != juceOverlays.end())
    {
        if (juceOverlays[ofType])
        {
            frame->removeChildComponent(juceOverlays[ofType].get());
        }
        juceOverlays.erase(ofType);
    }
}

void SurgeGUIEditor::addJuceEditorOverlay(
    std::unique_ptr<juce::Component> c,
    std::string editorTitle, // A window display title - whatever you want
    OverlayTags editorTag,   // A tag by editor class. Please unique, no spaces.
    const juce::Rectangle<int> &containerSize, bool showCloseButton, std::function<void()> onClose)
{
    auto ol = std::make_unique<Surge::Overlays::OverlayWrapper>();
    ol->setBounds(containerSize);
    ol->setTitle(editorTitle);
    ol->setSkin(currentSkin, bitmapStore);
    ol->setSurgeGUIEditor(this);
    ol->setIcon(bitmapStore->getDrawable(IDB_SURGE_ICON));
    ol->setShowCloseButton(showCloseButton);
    ol->setCloseOverlay([this, editorTag, onClose]() {
        this->dismissEditorOfType(editorTag);
        onClose();
    });

    ol->addAndTakeOwnership(std::move(c));
    frame->addAndMakeVisible(*ol);
    juceOverlays[editorTag] = std::move(ol);
}

std::string SurgeGUIEditor::getDisplayForTag(long tag)
{
    if (tag < start_paramtags)
        return "Non-param tag";

    int ptag = tag - start_paramtags;
    if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
    {
        Parameter *p = synth->storage.getPatch().param_ptr[ptag];
        if (p)
        {
            char txt[1024];
            p->get_display(txt);
            return txt;
        }
    }

    return "Unknown";
}

void SurgeGUIEditor::promptForMiniEdit(const std::string &value, const std::string &prompt,
                                       const std::string &title, const VSTGUI::CPoint &iwhere,
                                       std::function<void(const std::string &)> onOK)
{
    miniEdit->setSkin(currentSkin, bitmapStore);
    if (frame->getIndexOfChildComponent(miniEdit.get()) < 0)
    {
        frame->addChildComponent(*miniEdit);
    }
    miniEdit->setTitle(title);
    miniEdit->setLabel(prompt);
    miniEdit->setValue(value);
    miniEdit->callback = std::move(onOK);
    miniEdit->setBounds(0, 0, getWindowSizeX(), getWindowSizeY());
    miniEdit->setVisible(true);
    miniEdit->grabFocus();
}

void SurgeGUIEditor::modSourceButtonDroppedAt(Surge::Widgets::ModulationSourceButton *msb,
                                              const juce::Point<int> &pt)
{
    // We need to do this search vs componentAt because componentAt will return self since I am
    // there being dropped
    juce::Component *target = nullptr;
    for (auto kid : frame->getChildren())
    {
        if (kid && kid->isVisible() && kid != msb && kid->getBounds().contains(pt))
        {
            target = kid;
            // break;
        }
    }
    if (!target)
        return;
    auto tMSB = dynamic_cast<Surge::Widgets::ModulationSourceButton *>(target);
    auto tMCI = dynamic_cast<Surge::Widgets::ModulatableControlInterface *>(target);
    if (msb->isMeta && tMSB && tMSB->isMeta)
    {
        swapControllers(msb->getTag(), tMSB->getTag());
    }
    else if (tMCI)
    {
        openModTypeinOnDrop(msb->getTag(), tMCI, tMCI->asControlValueInterface()->getTag());
    }
}
void SurgeGUIEditor::swapControllers(int t1, int t2)
{
    synth->swapMetaControllers(t1 - tag_mod_source0 - ms_ctrl1, t2 - tag_mod_source0 - ms_ctrl1);
}

void SurgeGUIEditor::openModTypeinOnDrop(int modt, Surge::Widgets::ModulatableControlInterface *sl,
                                         int slidertag)
{
    auto p = synth->storage.getPatch().param_ptr[slidertag - start_paramtags];
    int ms = modt - tag_mod_source0;

    if (synth->isValidModulation(p->id, (modsources)ms))
        promptForUserValueEntry(p, sl->asControlValueInterface()->asJuceComponent(), ms);
}

void SurgeGUIEditor::resetSmoothing(ControllerModulationSource::SmoothingMode t)
{
    // Reset the default value and tell the synth it is updated
    Surge::Storage::updateUserDefaultValue(&(synth->storage), Surge::Storage::SmoothingMode,
                                           (int)t);
    synth->changeModulatorSmoothing(t);
}

void SurgeGUIEditor::resetPitchSmoothing(ControllerModulationSource::SmoothingMode t)
{
    // Reset the default value and update it in storage for newly created voices to use
    Surge::Storage::updateUserDefaultValue(&(synth->storage), Surge::Storage::PitchSmoothingMode,
                                           (int)t);
    synth->storage.pitchSmoothingMode = t;
}

void SurgeGUIEditor::makeStorePatchDialog()
{
    auto npc = Surge::Skin::Connector::NonParameterConnection::STORE_PATCH_DIALOG;
    auto conn = Surge::Skin::Connector::connectorByNonParameterConnection(npc);
    auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);

    std::string name = synth->storage.getPatch().name;
    std::string category = synth->storage.getPatch().category;
    std::string author = synth->storage.getPatch().author;
    std::string comments = synth->storage.getPatch().comment;

    auto defaultAuthor = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::DefaultPatchAuthor, "");
    auto defaultComment = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::DefaultPatchComment, "");
    auto oldAuthor = std::string("");

    if (!Surge::Storage::isValidUTF8(defaultAuthor))
    {
        defaultAuthor = "";
    }
    if (!Surge::Storage::isValidUTF8(defaultComment))
    {
        defaultComment = "";
    }

    if (author == "" && defaultAuthor != "")
    {
        author = defaultAuthor;
    }

    if (author != "" && defaultAuthor != "")
    {
        if (_stricmp(author.c_str(), defaultAuthor.c_str()))
        {
            oldAuthor = author;
            author = defaultAuthor;
        }
    }

    if (comments == "" && defaultComment != "")
    {
        comments = defaultComment;
    }

    if (oldAuthor != "")
    {
        if (comments == "")
            comments += "Original patch by " + oldAuthor;
        else
            comments += " (Original patch by " + oldAuthor + ")";
    }

    auto pb = std::make_unique<Surge::Overlays::PatchStoreDialog>();
    pb->setSkin(currentSkin);
    pb->setName(name);
    pb->setAuthor(author);
    pb->setCategory(category);
    pb->setComment(comments);
    pb->setSurgeGUIEditor(this);

    addJuceEditorOverlay(std::move(pb), "Store Patch", STORE_PATCH,
                         skinCtrl->getRect().asJuceIntRect(), false);
}

Surge::GUI::IComponentTagValue *
SurgeGUIEditor::layoutComponentForSkin(std::shared_ptr<Surge::GUI::Skin::Control> skinCtrl,
                                       long tag, int paramIndex, Parameter *p, int style)
{
    // Special cases to preserve things
    if (p && p->ctrltype == ct_fmconfig)
    {
        fmconfig_tag = tag;
    }
    if (p && p->ctrltype == ct_fbconfig)
    {
        filterblock_tag = tag;
    }
    if (p && p->ctrltype == ct_fxbypass)
    {
        fxbypass_tag = tag;
    }
    // Basically put this in a function
    if (skinCtrl->ultimateparentclassname == "CSurgeSlider")
    {
        if (!p)
        {
            // FIXME ERROR
            return nullptr;
        }

        CPoint loc(skinCtrl->x, skinCtrl->y + p->posy_offset * yofs);

        if (p->is_discrete_selection())
        {
            loc.offset(2, 4);
            auto hs =
                componentForSkinSession<Surge::Widgets::MenuForDiscreteParams>(skinCtrl->sessionid);

            /*auto hs = new CMenuAsSlider(loc, this, p->id + start_paramtags, bitmapStore,
                                        &(synth->storage));*/
            hs->setTag(p->id + start_paramtags);
            hs->addListener(this);
            hs->setStorage(&(synth->storage));
            hs->setBounds(loc.x, loc.y, 133, 22);
            hs->setSkin(currentSkin, bitmapStore);
            hs->setValue(p->get_value_f01());
            hs->setMinMax(p->val_min.i, p->val_max.i);
            hs->setLabel(p->get_name());
            p->ctrlstyle = p->ctrlstyle | kNoPopup;
            if (p->can_deactivate())
                hs->setDeactivated(p->deactivated);

            auto *parm = dynamic_cast<ParameterDiscreteIndexRemapper *>(p->user_data);
            if (parm && parm->supportsTotalIndexOrdering())
                hs->setIntOrdering(parm->totalIndexOrdering());

            auto dbls = currentSkin->standardHoverAndHoverOnForIDB(IDB_MENU_AS_SLIDER, bitmapStore);
            hs->setBackgroundDrawable(dbls[0]);
            hs->setHoverBackgroundDrawable(dbls[1]);
            param[p->id] = hs.get();
            frame->addAndMakeVisible(*hs);
            juceSkinComponents[skinCtrl->sessionid] = std::move(hs);

            return dynamic_cast<Surge::GUI::IComponentTagValue *>(
                juceSkinComponents[skinCtrl->sessionid].get());
        }
        else
        {
            p->ctrlstyle = p->ctrlstyle & ~kNoPopup;
        }

        auto hs = componentForSkinSession<Surge::Widgets::ModulatableSlider>(skinCtrl->sessionid);

        hs->setIsValidToModulate(p && synth->isValidModulation(p->id, modsource));

        int styleW = (style & Surge::ParamConfig::kHorizontal) ? 140 : 22;
        int styleH = (style & Surge::ParamConfig::kHorizontal) ? 26 : 84;
        hs->setOrientation((style & Surge::ParamConfig::kHorizontal)
                               ? Surge::ParamConfig::kHorizontal
                               : Surge::ParamConfig::kVertical);
        hs->setIsSemitone(style & kSemitone);
        hs->setIsLightStyle(style & kWhite);
        hs->setIsMiniVertical(style & kMini);

        hs->setBounds(skinCtrl->x, skinCtrl->y + p->posy_offset * yofs, styleW, styleH);
        hs->setTag(tag);
        hs->addListener(this);
        hs->setStorage(&(this->synth->storage));
        hs->setSkin(currentSkin, bitmapStore, skinCtrl);
        hs->setMoveRate(p->moverate);

        if (p->can_temposync())
            hs->setTempoSync(p->temposync);
        else
            hs->setTempoSync(false);
        hs->setValue(p->get_value_f01());
        hs->setQuantitizedDisplayValue(hs->getValue());

        if (p->supportsDynamicName() && p->dynamicName)
        {
            hs->setDynamicLabel([p]() { return std::string(p->get_name()); });
        }
        else
        {
            hs->setLabel(p->get_name());
        }

        hs->setBipolarFn([p]() { return p->is_bipolar(); });
        hs->setFontStyle(Surge::GUI::Skin::setFontStyleProperty(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::FONT_STYLE, "normal")));

        hs->setTextAlign(Surge::GUI::Skin::setTextAlignProperty(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_ALIGN, "right")));

        // Control is using labfont = Surge::GUI::getFontManager()->displayFont, which is
        // currently 9 pt in size
        // TODO: Pull the default font size from some central location at a later date
        hs->setFont(Surge::GUI::getFontManager()->displayFont);

        hs->setFontSize(std::atoi(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::FONT_SIZE, "9").c_str()));

        hs->setTextHOffset(std::atoi(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_HOFFSET, "0")
                .c_str()));

        hs->setTextVOffset(std::atoi(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_VOFFSET, "0")
                .c_str()));

        hs->setDeactivated(false);
        hs->setDeactivatedFn([p]() { return p->appears_deactivated(); });

        auto ff = currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::FONT_FAMILY, "");
        if (ff.size() > 0)
        {
#if 0
            if (currentSkin->typeFaces.find(ff) != currentSkin->typeFaces.end())
                hs->setFont(juce::Font(currentSkin->typeFaces[ff]).withPointHeight(hs->font_size));
#endif
            jassert(false);
        }

        if (p->valtype == vt_int || p->valtype == vt_bool)
        {
            hs->setIsStepped(true);
            hs->setIntStepRange(p->val_max.i - p->val_min.i);
        }
        else
        {
            hs->setIsStepped(false);
        }

        setDisabledForParameter(p, hs.get());

        hs->setIsEditingModulation(mod_editor);
        hs->setModulationState(synth->isModDestUsed(p->id),
                               synth->isActiveModulation(p->id, modsource));
        if (synth->isValidModulation(p->id, modsource))
        {
            hs->setModValue(synth->getModulation(p->id, modsource));
            hs->setIsModulationBipolar(synth->isBipolarModulation(modsource));
        }

        param[p->id] = hs.get();
        frame->addAndMakeVisible(*hs);
        juceSkinComponents[skinCtrl->sessionid] = std::move(hs);

        return dynamic_cast<Surge::GUI::IComponentTagValue *>(
            juceSkinComponents[skinCtrl->sessionid].get());
    }
    if (skinCtrl->ultimateparentclassname == "CHSwitch2")
    {
        CRect rect(0, 0, skinCtrl->w, skinCtrl->h);
        rect.offset(skinCtrl->x, skinCtrl->y);

        // Make this a function on skin
        auto drawables = currentSkin->standardHoverAndHoverOnForControl(skinCtrl, bitmapStore);

        if (std::get<0>(drawables))
        {
            // Special case that scene select parameter is "odd"
            if (p && p->ctrltype == ct_scenesel)
                tag = tag_scene_select;

            auto frames = currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::FRAMES, "1");
            auto rows = currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::ROWS, "1");
            auto cols = currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::COLUMNS, "1");
            auto frameoffset =
                currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::FRAME_OFFSET, "0");
            auto drgb = currentSkin->propertyValue(skinCtrl,
                                                   Surge::Skin::Component::DRAGGABLE_HSWITCH, "1");
            auto hsw = componentForSkinSession<Surge::Widgets::MultiSwitch>(skinCtrl->sessionid);
            hsw->setRows(std::atoi(rows.c_str()));
            hsw->setColumns(std::atoi(cols.c_str()));
            hsw->setTag(tag);
            hsw->addListener(this);
            hsw->setDraggable(std::atoi(drgb.c_str()));
            hsw->setHeightOfOneImage(skinCtrl->h);
            hsw->setFrameOffset(std::atoi(frameoffset.c_str()));

            hsw->setSwitchDrawable(std::get<0>(drawables));
            hsw->setHoverSwitchDrawable(std::get<1>(drawables));
            hsw->setHoverOnSwitchDrawable(std::get<2>(drawables));

            hsw->setBounds(rect.asJuceIntRect());
            hsw->setSkin(currentSkin, bitmapStore, skinCtrl);

            if (p)
            {
                auto fval = p->get_value_f01();

                if (p->ctrltype == ct_scenemode)
                {
                    /*
                    ** SceneMode is special now because we have a streaming vs UI difference.
                    ** The streamed integer value is 0, 1, 2, 3 which matches the scene_mode
                    ** SurgeStorage enum. But our display would look gross in that order, so
                    ** our display order is single, split, channel split, dual which is 0, 1, 3, 2.
                    ** Fine. So just deal with that here.
                    */
                    auto guiscenemode = p->val.i;
                    if (guiscenemode == 3)
                        guiscenemode = 2;
                    else if (guiscenemode == 2)
                        guiscenemode = 3;
                    fval = Parameter::intScaledToFloat(guiscenemode, n_scene_modes - 1);
                }
                hsw->setValue(fval);
            }

            frame->addAndMakeVisible(*hsw);

            juceSkinComponents[skinCtrl->sessionid] = std::move(hsw);

            if (paramIndex >= 0)
                nonmod_param[paramIndex] = dynamic_cast<Surge::GUI::IComponentTagValue *>(
                    juceSkinComponents[skinCtrl->sessionid].get());

            return dynamic_cast<Surge::GUI::IComponentTagValue *>(
                juceSkinComponents[skinCtrl->sessionid].get());
            ;
        }
        else
        {
            std::cout << "Can't get a CHSwitch2 BG" << std::endl;
        }
    }
    if (skinCtrl->ultimateparentclassname == "CSwitchControl")
    {
        CRect rect(CPoint(skinCtrl->x, skinCtrl->y), CPoint(skinCtrl->w, skinCtrl->h));
        auto drawables = currentSkin->standardHoverAndHoverOnForControl(skinCtrl, bitmapStore);

        if (std::get<0>(drawables))
        {
            auto hsw = componentForSkinSession<Surge::Widgets::Switch>(skinCtrl->sessionid);
            frame->addAndMakeVisible(*hsw);

            hsw->setSkin(currentSkin, bitmapStore, skinCtrl);
            hsw->setBounds(rect.asJuceIntRect());
            hsw->setTag(tag);
            hsw->addListener(this);

            hsw->setSwitchDrawable(std::get<0>(drawables));
            hsw->setHoverSwitchDrawable(std::get<1>(drawables));

            if (paramIndex >= 0)
                nonmod_param[paramIndex] = hsw.get();
            if (p)
            {
                hsw->setValue(p->get_value_f01());

                // Carry over this filter type special case from the default control path
                if (p->ctrltype == ct_filtersubtype)
                {
                    auto filttype = synth->storage.getPatch()
                                        .scene[current_scene]
                                        .filterunit[p->ctrlgroup_entry]
                                        .type.val.i;
                    auto stc = fut_subcount[filttype];
                    hsw->setIsMultiIntegerValued(true);
                    hsw->setIntegerMax(stc);
                    hsw->setIntegerValue(std::min(p->val.i + 1, stc));
                    if (fut_subcount[filttype] == 0)
                        hsw->setIntegerValue(0);

                    if (p->ctrlgroup_entry == 1)
                    {
                        f2subtypetag = p->id + start_paramtags;
                        filtersubtype[1] = hsw.get();
                    }
                    else
                    {
                        f1subtypetag = p->id + start_paramtags;
                        filtersubtype[0] = hsw.get();
                    }
                }
            }

            // frame->deferredJuceAdds.push_back(hsw.get());
            juceSkinComponents[skinCtrl->sessionid] = std::move(hsw);

            return dynamic_cast<Surge::GUI::IComponentTagValue *>(
                juceSkinComponents[skinCtrl->sessionid].get());
        }
    }
    if (skinCtrl->ultimateparentclassname == "CLFOGui")
    {
        if (!p)
            return nullptr;
        if (p->ctrltype != ct_lfotype)
        {
            // FIXME - warning?
        }
        CRect rect(0, 0, skinCtrl->w, skinCtrl->h);
        rect.offset(skinCtrl->x, skinCtrl->y);
        int lfo_id = p->ctrlgroup_entry - ms_lfo1;
        if ((lfo_id >= 0) && (lfo_id < n_lfos))
        {
            lfoDisplay =
                componentForSkinSession<Surge::Widgets::LFOAndStepDisplay>(skinCtrl->sessionid);
            lfoDisplay->setBounds(skinCtrl->getRect().asJuceIntRect());
            lfoDisplay->setSkin(currentSkin, bitmapStore, skinCtrl);
            lfoDisplay->setTag(p->id + start_paramtags);
            lfoDisplay->setLFOStorage(&synth->storage.getPatch().scene[current_scene].lfo[lfo_id]);
            lfoDisplay->setStorage(&synth->storage);
            lfoDisplay->setStepSequencerStorage(
                &synth->storage.getPatch().stepsequences[current_scene][lfo_id]);
            lfoDisplay->setMSEGStorage(&synth->storage.getPatch().msegs[current_scene][lfo_id]);
            lfoDisplay->setFormulaStorage(
                &synth->storage.getPatch().formulamods[current_scene][lfo_id]);
            lfoDisplay->setCanEditEnvelopes(lfo_id >= 0 && lfo_id <= (ms_lfo6 - ms_lfo1));

            lfoDisplay->addListener(this);
            frame->addAndMakeVisible(*lfoDisplay);
            nonmod_param[paramIndex] = lfoDisplay.get();
            return lfoDisplay.get();
        }
    }

    if (skinCtrl->ultimateparentclassname == "COSCMenu")
    {
        if (!oscMenu)
            oscMenu = std::make_unique<Surge::Widgets::OscillatorMenu>();
        oscMenu->setTag(tag_osc_menu);
        oscMenu->addListener(this);
        oscMenu->setStorage(&(synth->storage));
        oscMenu->setSkin(currentSkin, bitmapStore, skinCtrl);
        oscMenu->setBackgroundDrawable(bitmapStore->getDrawable(IDB_OSC_MENU));
        auto id = currentSkin->hoverImageIdForResource(IDB_OSC_MENU, Surge::GUI::Skin::HOVER);
        auto bhov = bitmapStore->getDrawableByStringID(id);
        oscMenu->setHoverBackgroundDrawable(bhov);
        oscMenu->setBounds(skinCtrl->x, skinCtrl->y, skinCtrl->w, skinCtrl->h);
        oscMenu->setOscillatorStorage(
            &synth->storage.getPatch().scene[current_scene].osc[current_osc[current_scene]]);
        oscMenu->populate();

        oscMenu->text_allcaps = Surge::GUI::Skin::setAllCapsProperty(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_ALL_CAPS, "false"));
        oscMenu->font_style = Surge::GUI::Skin::setFontStyleProperty(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::FONT_STYLE, "normal"));
        oscMenu->text_align = Surge::GUI::Skin::setJuceTextAlignProperty(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_ALIGN, "center"));
        oscMenu->font_size = std::atoi(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::FONT_SIZE, "8").c_str());
        oscMenu->text_hoffset = std::atoi(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_HOFFSET, "0")
                .c_str());
        oscMenu->text_voffset = std::atoi(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_VOFFSET, "0")
                .c_str());
        frame->addAndMakeVisible(*oscMenu);
        return oscMenu.get();
    }
    if (skinCtrl->ultimateparentclassname == "CFXMenu")
    {
        if (!fxMenu)
            fxMenu = std::make_unique<Surge::Widgets::FxMenu>();
        fxMenu->setTag(tag_fx_menu);
        fxMenu->addListener(this);
        fxMenu->setStorage(&(synth->storage));
        fxMenu->setSkin(currentSkin, bitmapStore, skinCtrl);
        fxMenu->setBackgroundDrawable(bitmapStore->getDrawable(IDB_MENU_AS_SLIDER));
        auto id = currentSkin->hoverImageIdForResource(IDB_MENU_AS_SLIDER, Surge::GUI::Skin::HOVER);
        auto bhov = bitmapStore->getDrawableByStringID(id);
        fxMenu->setHoverBackgroundDrawable(bhov);
        fxMenu->setBounds(skinCtrl->x, skinCtrl->y, skinCtrl->w, skinCtrl->h);
        fxMenu->setFxStorage(&synth->storage.getPatch().fx[current_fx]);
        fxMenu->setFxBuffer(&synth->fxsync[current_fx]);
        fxMenu->setCurrentFx(current_fx);
        fxMenu->selectedIdx = selectedFX[current_fx];
        // TODO set the fxs fxb, cfx

        fxMenu->populate();
        frame->addAndMakeVisible(*fxMenu);
        return fxMenu.get();
    }

    if (skinCtrl->ultimateparentclassname == "CNumberField")
    {
        auto pbd = componentForSkinSession<Surge::Widgets::NumberField>(skinCtrl->sessionid);
        pbd->addListener(this);
        pbd->setSkin(currentSkin, bitmapStore, skinCtrl);
        pbd->setTag(tag);
        pbd->setStorage(&synth->storage);

        auto images = currentSkin->standardHoverAndHoverOnForControl(skinCtrl, bitmapStore);
        pbd->setBackgroundDrawable(images[0]);
        pbd->setHoverBackgroundDrawable(images[1]);

        // TODO extra from properties
        auto nfcm =
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::NUMBERFIELD_CONTROLMODE,
                                       std::to_string(Surge::Skin::Parameters::NONE));
        pbd->setControlMode(
            (Surge::Skin::Parameters::NumberfieldControlModes)std::atoi(nfcm.c_str()));
        pbd->setValue(p->get_value_f01());
        pbd->setBounds(skinCtrl->getRect().asJuceIntRect());

        auto colorName = currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_COLOR,
                                                    Colors::NumberField::Text.name);
        auto hoverColorName =
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_HOVER_COLOR,
                                       Colors::NumberField::TextHover.name);
        pbd->setTextColour(currentSkin->getColor(colorName));
        pbd->setHoverTextColour(currentSkin->getColor(hoverColorName));

        frame->addAndMakeVisible(*pbd);
        nonmod_param[paramIndex] = pbd.get();

        if (p && p->ctrltype == ct_midikey_or_channel)
        {
            auto sm = this->synth->storage.getPatch().scenemode.val.i;

            switch (sm)
            {
            case sm_single:
            case sm_dual:
                pbd->setControlMode(Surge::Skin::Parameters::NONE);
                break;
            case sm_split:
                pbd->setControlMode(Surge::Skin::Parameters::NOTENAME);
                break;
            case sm_chsplit:
                pbd->setControlMode(Surge::Skin::Parameters::MIDICHANNEL_FROM_127);
                break;
            }
        }

        // Save some of these for later reference
        switch (p->ctrltype)
        {
        case ct_polylimit:
            polydisp = std::move(pbd);
            return polydisp.get();
            break;
        case ct_midikey_or_channel:
            splitpointControl = std::move(pbd);
            return splitpointControl.get();
            break;
        default:
            juceSkinComponents[skinCtrl->sessionid] = std::move(pbd);
            return dynamic_cast<Surge::GUI::IComponentTagValue *>(
                juceSkinComponents[skinCtrl->sessionid].get());
            break;
        }
        return nullptr;
    }
    if (skinCtrl->ultimateparentclassname == "FilterSelector")
    {
        // Obviously exposing this widget as a controllable widget would be better
        if (!p)
            return nullptr;

        auto rect = skinCtrl->getRect();
        auto hsw =
            componentForSkinSession<Surge::Widgets::MenuForDiscreteParams>(skinCtrl->sessionid);
        hsw->addListener(this);
        hsw->setSkin(currentSkin, bitmapStore, skinCtrl);
        hsw->setTag(p->id + start_paramtags);
        hsw->setStorage(&(synth->storage));
        hsw->setBounds(rect.asJuceIntRect());
        hsw->setValue(p->get_value_f01());
        p->ctrlstyle = p->ctrlstyle | kNoPopup;

        auto *parm = dynamic_cast<ParameterDiscreteIndexRemapper *>(p->user_data);
        if (parm && parm->supportsTotalIndexOrdering())
            hsw->setIntOrdering(parm->totalIndexOrdering());

        hsw->setMinMax(0, n_fu_types - 1);
        hsw->setLabel(p->get_name());
        hsw->setDeactivated(false);

        auto pv = currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::BACKGROUND);
        if (pv.isJust())
        {
            hsw->setBackgroundDrawable(bitmapStore->getDrawableByStringID(pv.fromJust()));
            jassert(false); // hover
        }
        else
        {
            hsw->setBackgroundDrawable(bitmapStore->getDrawable(IDB_FILTER_MENU));
            auto id =
                currentSkin->hoverImageIdForResource(IDB_FILTER_MENU, Surge::GUI::Skin::HOVER);
            auto bhov = bitmapStore->getDrawableByStringID(id);
            hsw->setHoverBackgroundDrawable(bhov);
        }

        bool activeGlyph = true;
        if (currentSkin->getVersion() >= 2)
        {
            auto pv =
                currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::GLPYH_ACTIVE, "true");
            if (pv == "false")
                activeGlyph = false;
        }

        hsw->setGlyphMode(true);

        if (activeGlyph)
        {
            for (int i = 0; i < n_fu_types; i++)
                hsw->addGlyphIndexMapEntry(fut_glyph_index[i][0], fut_glyph_index[i][1]);

            auto glpc = currentSkin->propertyValue(skinCtrl,
                                                   Surge::Skin::Component::GLYPH_PLACEMENT, "left");
            auto glw = std::atoi(
                currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::GLYPH_W, "18")
                    .c_str());
            auto glh = std::atoi(
                currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::GLYPH_H, "18")
                    .c_str());
            auto gli =
                currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::GLYPH_IMAGE, "");
            auto glih =
                currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::GLYPH_HOVER_IMAGE, "");

            // These are the V1 hardcoded defaults
            if (glw == 18 && glh == 18 && glpc == "left" && gli == "")
            {
                auto drr = rect;
                drr.right = drr.left + 18;
                hsw->setDragRegion(drr.asJuceIntRect());
                hsw->setDragGlyph(bitmapStore->getDrawable(IDB_FILTER_ICONS), 18);
                hsw->setDragGlyphHover(
                    bitmapStore->getDrawableByStringID(currentSkin->hoverImageIdForResource(
                        IDB_FILTER_ICONS, Surge::GUI::Skin::HOVER)));
            }
            else
            {
                jassert(false);
                // hsw->setGlyphSettings(gli, glih, glpc, glw, glh);
            }
        }

        frame->addAndMakeVisible(*hsw);
        nonmod_param[paramIndex] = hsw.get();

        juceSkinComponents[skinCtrl->sessionid] = std::move(hsw);

        return dynamic_cast<Surge::GUI::IComponentTagValue *>(
            juceSkinComponents[skinCtrl->sessionid].get());
    }
    if (skinCtrl->ultimateparentclassname != Surge::GUI::NoneClassName)
        std::cout << "Unable to make control with upc " << skinCtrl->ultimateparentclassname
                  << std::endl;
    return nullptr;
}

bool SurgeGUIEditor::canDropTarget(const std::string &fname)
{
    static std::unordered_set<std::string> extensions;
    if (extensions.empty())
    {
        extensions.insert(".scl");
        extensions.insert(".kbm");
        extensions.insert(".wav");
        extensions.insert(".wt");
        extensions.insert(".fxp");
    }

    fs::path fPath(fname);
    std::string fExt(path_to_string(fPath.extension()));
    std::transform(fExt.begin(), fExt.end(), fExt.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (extensions.find(fExt) != extensions.end())
        return true;
    return false;
}
bool SurgeGUIEditor::onDrop(const std::string &fname)
{
    fs::path fPath(fname);
    std::string fExt(path_to_string(fPath.extension()));
    std::transform(fExt.begin(), fExt.end(), fExt.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (fExt == ".wav" || fExt == ".wt")
    {
        strxcpy(synth->storage.getPatch()
                    .scene[current_scene]
                    .osc[current_osc[current_scene]]
                    .wt.queue_filename,
                fname.c_str(), 255);
    }
    else if (fExt == ".scl")
    {
        scaleFileDropped(fname);
    }
    else if (fExt == ".kbm")
    {
        mappingFileDropped(fname);
    }
    else if (fExt == ".fxp")
    {
        queuePatchFileLoad(fname);
    }

    return true;
}

void SurgeGUIEditor::swapFX(int source, int target, SurgeSynthesizer::FXReorderMode m)
{
    if (source < 0 || source >= n_fx_slots || target < 0 || target >= n_fx_slots)
        return;

    auto t = fxPresetName[target];
    fxPresetName[target] = fxPresetName[source];
    if (m == SurgeSynthesizer::SWAP)
        fxPresetName[source] = t;
    if (m == SurgeSynthesizer::MOVE)
        fxPresetName[source] = "";

    synth->reorderFx(source, target, m);
}

void SurgeGUIEditor::lfoShapeChanged(int prior, int curr)
{
    if (prior != curr || prior == lt_mseg || curr == lt_mseg || prior == lt_formula ||
        curr == lt_formula)
    {
        if (msegEditSwitch)
        {
            auto msejc = dynamic_cast<juce::Component *>(msegEditSwitch);
            msejc->setVisible(curr == lt_mseg || curr == lt_formula);
        }
    }

    if (curr == lt_mseg && isAnyOverlayPresent(MSEG_EDITOR))
    {
        // We have the MSEGEditor open and have swapped to the MSEG here
        showMSEGEditor();
    }
    else if (prior == lt_mseg && curr != lt_mseg && isAnyOverlayPresent(MSEG_EDITOR))
    {
        // We can choose to not do this too; if we do we are editing an MSEG which isn't used though
        closeMSEGEditor();
    }

    if (curr == lt_formula && isAnyOverlayPresent(FORMULA_EDITOR))
    {
        // We have the MSEGEditor open and have swapped to the MSEG here
        showFormulaEditorDialog();
    }
    else if (prior == lt_formula && curr != lt_formula && isAnyOverlayPresent(FORMULA_EDITOR))
    {
        // We can choose to not do this too; if we do we are editing an MSEG which isn't used though
        closeFormulaEditorDialog();
    }

    // update the LFO title label
    std::string modname = modulatorName(modsource_editor[current_scene], true);
    lfoNameLabel->setText(modname.c_str());
    lfoNameLabel->repaint();

    // And now we have dynamic labels really anything
    frame->repaint();
}

void SurgeGUIEditor::closeStorePatchDialog() { dismissEditorOfType(STORE_PATCH); }

void SurgeGUIEditor::showStorePatchDialog() { makeStorePatchDialog(); }

void SurgeGUIEditor::closeMSEGEditor()
{
    if (isAnyOverlayPresent(MSEG_EDITOR))
    {
        broadcastMSEGState();
        dismissEditorOfType(MSEG_EDITOR);
    }
}
void SurgeGUIEditor::toggleMSEGEditor()
{
    if (isAnyOverlayPresent(MSEG_EDITOR))
    {
        closeMSEGEditor();
    }
    else
    {
        showMSEGEditor();
    }
}

void SurgeGUIEditor::showPatchBrowserDialog()
{
    auto pt = std::make_unique<Surge::Overlays::PatchDBViewer>(this, &(this->synth->storage));
    addJuceEditorOverlay(std::move(pt), "Patch Browser", PATCH_BROWSER,
                         juce::Rectangle<int>(50, 50, 750, 450));
}

void SurgeGUIEditor::closePatchBrowserDialog()
{
    if (isAnyOverlayPresent(PATCH_BROWSER))
    {
        dismissEditorOfType(PATCH_BROWSER);
    }
}

void SurgeGUIEditor::showModulationEditorDialog()
{
    auto pt = std::make_unique<Surge::Overlays::ModulationEditor>(this, this->synth);
    addJuceEditorOverlay(std::move(pt), "Modulation Overview", MODULATION_EDITOR,
                         juce::Rectangle<int>(50, 50, 750, 450));
}

void SurgeGUIEditor::closeModulationEditorDialog()
{
    if (isAnyOverlayPresent(MODULATION_EDITOR))
    {
        dismissEditorOfType(MODULATION_EDITOR);
    }
}

void SurgeGUIEditor::showFormulaEditorDialog()
{
    // For now, follow the MSEG Sizing
    auto npc = Surge::Skin::Connector::NonParameterConnection::MSEG_EDITOR_WINDOW;
    auto conn = Surge::Skin::Connector::connectorByNonParameterConnection(npc);
    auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);

    auto lfo_id = modsource_editor[current_scene] - ms_lfo1;
    auto fs = &synth->storage.getPatch().formulamods[current_scene][lfo_id];

    auto pt = std::make_unique<Surge::Overlays::FormulaModulatorEditor>(
        this, &(this->synth->storage), fs, currentSkin);
    pt->setSkin(currentSkin, bitmapStore);

    addJuceEditorOverlay(std::move(pt), "Formula Editor", FORMULA_EDITOR,
                         skinCtrl->getRect().asJuceIntRect(), true, [this]() {});
}

void SurgeGUIEditor::closeFormulaEditorDialog()
{
    if (isAnyOverlayPresent(FORMULA_EDITOR))
    {
        dismissEditorOfType(FORMULA_EDITOR);
    }
}

void SurgeGUIEditor::showWavetableScripter()
{
    int w = 800, h = 520;
    auto px = (getWindowSizeX() - w) / 2;
    auto py = (getWindowSizeY() - h) / 2;
    auto r = juce::Rectangle<int>(px, py, w, h);

    auto os = &synth->storage.getPatch().scene[current_scene].osc[current_osc[current_scene]];

    auto pt = std::make_unique<Surge::Overlays::WavetableEquationEditor>(
        this, &(this->synth->storage), os, currentSkin);
    pt->setSkin(currentSkin, bitmapStore);

    addJuceEditorOverlay(std::move(pt), "Wavetable Editor", WAVETABLESCRIPTING_EDITOR, r, true,
                         [this]() { frame->repaint(); });
}

void SurgeGUIEditor::closeWavetableScripter()
{
    if (isAnyOverlayPresent(WAVETABLESCRIPTING_EDITOR))
    {
        dismissEditorOfType(WAVETABLESCRIPTING_EDITOR);
    }
}

void SurgeGUIEditor::toggleFormulaEditorDialog()
{
    if (isAnyOverlayPresent(FORMULA_EDITOR))
    {
        closeFormulaEditorDialog();
    }
    else
    {
        showFormulaEditorDialog();
    }
}

/*
 * The edit state is independent per LFO. We want to sync some of ti as if it is not
 * so this is called at the appropriate time.
 */
void SurgeGUIEditor::broadcastMSEGState()
{
    if (msegIsOpenFor >= 0 && msegIsOpenInScene >= 0)
    {
        for (int s = 0; s < n_scenes; ++s)
        {
            for (int lf = 0; lf < n_lfos; ++lf)
            {
                msegEditState[s][lf].timeEditMode =
                    msegEditState[msegIsOpenInScene][msegIsOpenFor].timeEditMode;
            }
        }
    }
    msegIsOpenFor = -1;
    msegIsOpenInScene = -1;
}
void SurgeGUIEditor::showMSEGEditor()
{
    broadcastMSEGState();

    auto lfo_id = modsource_editor[current_scene] - ms_lfo1;
    msegIsOpenFor = lfo_id;
    msegIsOpenInScene = current_scene;

    auto lfodata = &synth->storage.getPatch().scene[current_scene].lfo[lfo_id];
    auto ms = &synth->storage.getPatch().msegs[current_scene][lfo_id];
    auto mse = std::make_unique<Surge::Overlays::MSEGEditor>(&(synth->storage), lfodata, ms,
                                                             &msegEditState[current_scene][lfo_id],
                                                             currentSkin, bitmapStore);

    std::string title = modsource_names[modsource_editor[current_scene]];
    title += " Editor";
    Surge::Storage::findReplaceSubstring(title, std::string("LFO"), std::string("MSEG"));

    auto npc = Surge::Skin::Connector::NonParameterConnection::MSEG_EDITOR_WINDOW;
    auto conn = Surge::Skin::Connector::connectorByNonParameterConnection(npc);
    auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);

    addJuceEditorOverlay(std::move(mse), title, MSEG_EDITOR, skinCtrl->getRect().asJuceIntRect(),
                         true, [this]() {
                             if (msegEditSwitch)
                             {
                                 msegEditSwitch->setValue(0.0);
                                 msegEditSwitch->asJuceComponent()->repaint();
                             }
                         });

    if (msegEditSwitch)
    {
        msegEditSwitch->setValue(1.0);
        msegEditSwitch->asJuceComponent()->repaint();
    }
}

void SurgeGUIEditor::repushAutomationFor(Parameter *p)
{
    auto id = synth->idForParameter(p);
    synth->sendParameterAutomation(id, synth->getParameter01(id));
}

void SurgeGUIEditor::showAboutScreen(int devModeGrid)
{
    aboutScreen = std::make_unique<Surge::Overlays::AboutScreen>();
    aboutScreen->setEditor(this);
    aboutScreen->setHostProgram(synth->hostProgram);
    aboutScreen->setWrapperType(synth->juceWrapperType);
    aboutScreen->setStorage(&(this->synth->storage));
    aboutScreen->setSkin(currentSkin, bitmapStore);

    aboutScreen->populateData();

    aboutScreen->setBounds(frame->getLocalBounds());
    frame->addAndMakeVisible(*aboutScreen);
}

void SurgeGUIEditor::hideAboutScreen() { frame->removeChildComponent(aboutScreen.get()); }

void SurgeGUIEditor::showMidiLearnOverlay(const VSTGUI::CRect &r)
{
    midiLearnOverlay = bitmapStore->getDrawable(IDB_MIDI_LEARN)->createCopy();
    midiLearnOverlay->setInterceptsMouseClicks(false, false);
    midiLearnOverlay->setBounds(r.asJuceIntRect());
    frame->addAndMakeVisible(midiLearnOverlay.get());
}

void SurgeGUIEditor::hideMidiLearnOverlay()
{
    if (midiLearnOverlay)
    {
        frame->removeChildComponent(midiLearnOverlay.get());
    }
}

void SurgeGUIEditor::onSurgeError(const string &msg, const string &title)
{
    std::lock_guard<std::mutex> g(errorItemsMutex);
    errorItems.emplace_back(msg, title);
    errorItemCount++;
}
