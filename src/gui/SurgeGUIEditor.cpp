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

#include "COscillatorDisplay.h"
#include "CModulationSourceButton.h"
#include "CSnapshotMenu.h"
#include "CLFOGui.h"
#include "CTextButtonWithHover.h"
#include "SurgeBitmaps.h"
#include "CScalableBitmap.h"
#include "CNumberField.h"

#include "UserDefaults.h"
#include "SkinSupport.h"
#include "SkinColors.h"
#include "UIInstrumentation.h"
#include "SurgeGUIUtils.h"
#include "DebugHelpers.h"
#include "StringOps.h"
#include "ModulatorPresetManager.h"

#include "SurgeSynthEditor.h"

#include "overlays/AboutScreen.h"
#include "overlays/LuaEditors.h"
#include "overlays/MSEGEditor.h"
#include "overlays/ModulationEditor.h"
#include "overlays/PatchDBViewer.h"

#include "widgets/EffectLabel.h"
#include "widgets/EffectChooser.h"
#include "widgets/MenuForDiscreteParams.h"
#include "widgets/ModulatableSlider.h"
#include "widgets/MultiSwitch.h"
#include "widgets/ParameterInfowindow.h"
#include "widgets/PatchSelector.h"
#include "widgets/Switch.h"
#include "widgets/VerticalLabel.h"
#include "widgets/VuMeter.h"

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

enum special_tags
{
    tag_scene_select = 1,
    tag_osc_select,
    tag_osc_menu,
    tag_fx_select,
    tag_fx_menu,
    tag_patchname,
    tag_mp_category,
    tag_mp_patch,
    tag_store,
    tag_store_cancel,
    tag_store_ok,
    tag_store_name,
    tag_store_category,
    tag_store_creator,
    tag_store_comments,
    tag_store_tuning,
    tag_mod_source0,
    tag_mod_source_end = tag_mod_source0 + n_modsources,
    tag_settingsmenu,
    tag_mp_jogfx,
    tag_value_typein,
    tag_editor_overlay_close,
    tag_miniedit_ok,
    tag_miniedit_cancel,

    tag_status_mpe,
    tag_status_zoom,
    tag_status_tune,

    tag_mseg_edit,
    tag_lfo_menu,
    // tag_metaparam,
    // tag_metaparam_end = tag_metaparam+n_customcontrollers,
    start_paramtags,
};

int SurgeGUIEditor::start_paramtag_value = start_paramtags;

bool SurgeGUIEditor::fromSynthGUITag(SurgeSynthesizer *synth, int tag, SurgeSynthesizer::ID &q)
{
    // This is wrong for macros and params but is close
    return synth->fromSynthSideIdWithGuiOffset(tag, start_paramtags, tag_mod_source0 + ms_ctrl1, q);
}

SurgeGUIEditor::SurgeGUIEditor(SurgeSynthEditor *jEd, SurgeSynthesizer *synth) : super(jEd)
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

    rect.left = 0;
    rect.top = 0;

    rect.right = getWindowSizeX();
    rect.bottom = getWindowSizeY();

    editor_open = false;
    queue_refresh = false;
    memset(param, 0, n_paramslots * sizeof(void *));
    polydisp =
        0; // FIXME - when changing skins and rebuilding we need to reset these state variables too
    splitpointControl = 0;
    clear_infoview_countdown = -1;
    lfodisplay = 0;
    fxmenu = 0;
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

    /*
    ** See the comment in SurgeParamConfig.h
    */
    if ((int)Surge::ParamConfig::kHorizontal != (int)VSTGUI::CSlider::kHorizontal ||
        (int)Surge::ParamConfig::kVertical != (int)VSTGUI::CSlider::kVertical)
    {
        throw std::runtime_error("Software Error: Param Mismatch");
    }

    for (int i = 0; i < n_modsources; ++i)
        modsource_is_alternate[i] = false;

    currentSkin = Surge::GUI::SkinDB::get().defaultSkin(&(this->synth->storage));
    reloadFromSkin();

    auto des = &(synth->storage.getPatch().dawExtraState);
    if (des->isPopulated)
        loadFromDAWExtraState(synth);

    infowindow = std::make_unique<Surge::Widgets::ParameterInfowindow>();
    infowindow->setVisible(false);
}

SurgeGUIEditor::~SurgeGUIEditor()
{
    synth->storage.clearOkCancelProvider();
    auto isPop = synth->storage.getPatch().dawExtraState.isPopulated;
    populateDawExtraState(synth); // If I must die, leave my state for future generations
    synth->storage.getPatch().dawExtraState.isPopulated = isPop;
    if (frame)
    {
        frame->close();
    }

    frame->forget();
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
        hasIdleRun = true;
        if (firstIdleCountdown)
        {
            // Linux VST3 in JUCE Hosts (maybe others?) sets up the run loop out of order, it seems
            // sometimes missing the very first invalidation. Force a redraw on the first idle
            // isFirstIdle = false;
            firstIdleCountdown--;

            frame->invalid();
        }
        if (synth->learn_param < 0 && synth->learn_custom < 0 && midiLearnOverlay != nullptr)
        {
            hideMidiLearnOverlay();
        }
        for (auto c : removeFromFrame)
        {
            if (frame->isChild(c))
            {
                frame->removeView(c);
            }
        }
        removeFromFrame.clear();

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

        /*static CDrawContext drawContext
          (frame, NULL, systemWindow);*/
        // CDrawContext *drawContext = frame->createDrawContext();

        CView *v = frame->getFocusView();
        if (v && dynamic_cast<CControl *>(v) != nullptr)
        {
            int ptag = ((CControl *)v)->getTag() - start_paramtags;
            if (ptag >= 0)
            {
                synth->storage.modRoutingMutex.lock();

                for (int i = 1; i < n_modsources; i++)
                {
                    if (gui_modsrc[i])
                        ((CModulationSourceButton *)gui_modsrc[i])
                            ->update_rt_vals(synth->isActiveModulation(ptag, (modsources)i), 0,
                                             synth->isModsourceUsed((modsources)i));
                }
                synth->storage.modRoutingMutex.unlock();
            }
        }
        else
        {
            synth->storage.modRoutingMutex.lock();
            for (int i = 1; i < n_modsources; i++)
            {
                if (gui_modsrc[i])
                    ((CModulationSourceButton *)gui_modsrc[i])
                        ->update_rt_vals(false, 0, synth->isModsourceUsed((modsources)i));
            }
            synth->storage.modRoutingMutex.unlock();
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
            if (oscdisplay)
            {
                oscdisplay->invalid();
            }
        }

        if (typeinResetCounter > 0)
        {
            typeinResetCounter--;
            if (typeinResetCounter <= 0 && typeinDialog)
            {
                typeinLabel->setText(typeinResetLabel.c_str());
                typeinLabel->setFontColor(currentSkin->getColor(Colors::Slider::Label::Light));
                typeinLabel->invalid();
            }
        }

#if OSC_MOD_ANIMATION
        if (mod_editor && oscdisplay)
        {
            ((COscillatorDisplay *)oscdisplay)->tickModTime();
            oscdisplay->invalid();
        }
#endif

        if (polydisp)
        {
            CNumberField *cnpd = static_cast<CNumberField *>(polydisp);
            int prior = cnpd->getPoly();
            cnpd->setPoly(synth->polydisplay);
            if (prior != synth->polydisplay)
                cnpd->invalid();
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
                statusMPE->asBaseViewFunctions()->invalid();
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
                statusTune->asBaseViewFunctions()->invalid();
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
            vu[0]->invalid();

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

                    /*if(i == 0)
                      {
                      ((gui_pdisplay*)infowindow)->setLabel(pname,pdisp);
                      draw_infowindow(j, param[j], false, true);
                      clear_infoview_countdown = 40;
                      }*/

                    param[j]->asControlValueInterface()->setValue(
                        synth->refresh_ctrl_queue_value[i]);
                    frame->invalidRect(param[j]
                                           ->asControlValueInterface()
                                           ->asBaseViewFunctions()
                                           ->getControlViewSize());
                    // oscdisplay->invalid();

                    if (oscdisplay)
                    {
                        ((COscillatorDisplay *)oscdisplay)->invalidateIfIdIsInRange(j);
                    }

                    if (lfodisplay)
                    {
                        ((CLFOGui *)lfodisplay)->invalidateIfIdIsInRange(j);
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
            if (lfodisplay)
            {
                ((CLFOGui *)lfodisplay)
                    ->setTimeSignature(synth->time_data.timeSigNumerator,
                                       synth->time_data.timeSigDenominator);
                ((CLFOGui *)lfodisplay)->invalidateIfAnythingIsTemposynced();
            }
        }

        std::vector<int> refreshIndices;
        if (synth->refresh_overflow)
        {
            refreshIndices.resize(n_total_params);
            std::iota(std::begin(refreshIndices), std::end(refreshIndices), 0);
            frame->invalid();
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
                    param[j]->asControlValueInterface()->setValue(synth->getParameter01(jid));
                frame->invalidRect(param[j]
                                       ->asControlValueInterface()
                                       ->asBaseViewFunctions()
                                       ->getControlViewSize());

                if (oscdisplay)
                {
                    ((COscillatorDisplay *)oscdisplay)->invalidateIfIdIsInRange(j);
                }

                if (lfodisplay)
                {
                    ((CLFOGui *)lfodisplay)->invalidateIfIdIsInRange(j);
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
                CControlValueInterface *cc = nonmod_param[j];

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

                auto bvf = dynamic_cast<BaseViewFunctions *>(cc);
                if (bvf)
                {
                    bvf->invalid();
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
        clear_infoview_countdown += clear_infoview_peridle;
        if (clear_infoview_countdown == 0 && infowindow)
        {
            infowindow->setVisible(false);
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

        if (infowindow && infowindow->isVisible())
        {
            infowindow->setVisible(false);
        }
    }
}

void SurgeGUIEditor::refresh_mod()
{
    CModulationSourceButton *cms = gui_modsrc[modsource];

    modsources thisms = modsource;
    if (cms->hasAlternate && cms->useAlternate)
        thisms = (modsources)cms->alternateId;

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
            gui_modsrc[i]->used = synth->isModsourceUsed((modsources)i);
            gui_modsrc[i]->state = state;

            if (i < ms_ctrl1 || i > ms_ctrl8)
            {
                auto mn = modulatorName(i, true);
                gui_modsrc[i]->setlabel(mn.c_str());
            }
            gui_modsrc[i]->invalid();
        }
    }
}

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
                bitmapStore->setupBitmapsForFrame(frame);

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

class LastChanceEventCapture : public CControl
{
  public:
    LastChanceEventCapture(const CPoint &sz, SurgeGUIEditor *ed)
        : CControl(CRect(CPoint(0, 0), sz)), editor(ed)
    {
    }
    void draw(CDrawContext *pContext) override { return; }

    CMouseEventResult onMouseDown(CPoint &where, const CButtonState &buttons) override
    {
        if (buttons & (kMButton | kButton4 | kButton5))
        {
            if (editor)
            {
                editor->toggle_mod_editing();
            }

            return kMouseEventHandled;
        }

        if (buttons & kRButton)
        {
            if (editor)
            {
                CRect menuRect;
                menuRect.offset(where.x, where.y);

                editor->useDevMenu = false;
                editor->showSettingsMenu(menuRect);
            }

            return kMouseEventHandled;
        }

        return kMouseEventNotHandled;
    }

  private:
    SurgeGUIEditor *editor = nullptr;
    CLASS_METHODS(LastChanceEventCapture, CControl);
};

void SurgeGUIEditor::openOrRecreateEditor()
{
#ifdef INSTRUMENT_UI
    Surge::Debug::record("SurgeGUIEditor::openOrRecreateEditor");
#endif
    if (!synth)
        return;
    assert(frame);

    removeFromFrame.clear();

    editorOverlayTagAtClose = "";
    if (editor_open)
    {
        editorOverlayTagAtClose = topmostEditorTag();
        for (auto el : editorOverlay)
        {
            el.second->remember();
            frame->removeView(el.second);
        }

        close_editor();
    }
    CPoint nopoint(0, 0);
    CPoint sz(getWindowSizeX(), getWindowSizeY());
    // auto lcb = new LastChanceEventCapture(sz, this);
    // frame->addView(lcb);

    clear_infoview_peridle = -1;

    std::unordered_map<std::string, std::string> uiidToSliderLabel;

    /*
    ** There are a collection of member states we need to reset
    */
    polydisp = nullptr;
    lfodisplay = nullptr;
    fxmenu = nullptr;
    typeinDialog = nullptr;
    minieditOverlay = nullptr;
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

            gui_modsrc[ms] = new CModulationSourceButton(r, this, tag_mod_source0 + ms, state, ms,
                                                         bitmapStore, &(synth->storage));

            gui_modsrc[ms]->setSkin(currentSkin);
            gui_modsrc[ms]->update_rt_vals(false, 0, synth->isModsourceUsed(ms));

            if ((ms >= ms_ctrl1) && (ms <= ms_ctrl8))
            {
                // std::cout << synth->learn_custom << std::endl;
                gui_modsrc[ms]->setlabel(
                    synth->storage.getPatch().CustomControllerLabel[ms - ms_ctrl1]);
                gui_modsrc[ms]->set_ismeta(true);
                gui_modsrc[ms]->setBipolar(
                    synth->storage.getPatch().scene[current_scene].modsources[ms]->is_bipolar());
                gui_modsrc[ms]->setValue(((ControllerModulationSource *)synth->storage.getPatch()
                                              .scene[current_scene]
                                              .modsources[ms])
                                             ->get_target01());
            }
            else
            {
                gui_modsrc[ms]->setlabel(modulatorName(ms, true).c_str());

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

            frame->addView(gui_modsrc[ms]);
            if (ms >= ms_ctrl1 && ms <= ms_ctrl8 && synth->learn_custom == ms - ms_ctrl1)
            {
                showMidiLearnOverlay(r);
            }
        }
    }

    /*// Comments
      {
      CRect CommentsRect(6 + 150*4,528, getWindowSizeX(), getWindowSizeY());
      CTextLabel *Comments = new
      CTextLabel(CommentsRect,synth->storage.getPatch().comment.c_str());
      Comments->setTransparency(true);
      Comments->setFont(displayFont);
      Comments->setHoriAlign(kMultiLineCenterText);
      frame->addView(Comments);
      }*/

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
                    frame->juceComponent()->addAndMakeVisible(*vu[i + 1]);
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
                    frame->juceComponent()->addAndMakeVisible(*effectLabels[i]);
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
            auto od = new COscillatorDisplay(
                CRect(CPoint(skinCtrl->x, skinCtrl->y), CPoint(skinCtrl->w, skinCtrl->h)), this,
                &synth->storage.getPatch()
                     .scene[synth->storage.getPatch().scene_active.val.i]
                     .osc[current_osc[current_scene]],
                &synth->storage);
            od->setSkin(currentSkin, bitmapStore);
            frame->addView(od);
            oscdisplay = od;
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
            frame->juceComponent()->addAndMakeVisible(*lfoNameLabel);
            lfoNameLabel->setBounds(skinCtrl->getRect().asJuceIntRect());
            lfoNameLabel->setFont(Surge::GUI::getFontManager()->getLatoAtSize(10, kBoldFace));
            lfoNameLabel->setFontColour(currentSkin->getColor(Colors::LFO::Title::Text));
            break;
        }

        case Surge::Skin::Connector::NonParameterConnection::FXPRESET_LABEL:
        {
            // Room for improvement, obviously
            fxPresetLabel = new CTextLabel(skinCtrl->getRect(), "Preset");
            fxPresetLabel->setFontColor(currentSkin->getColor(Colors::Effect::Preset::Name));
            fxPresetLabel->setTransparency(true);
            fxPresetLabel->setFont(Surge::GUI::getFontManager()->displayFont);
            fxPresetLabel->setHoriAlign(kRightText);
            frame->addView(fxPresetLabel);
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

            frame->juceComponent()->addAndMakeVisible(*patchSelector);
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

            frame->juceComponent()->addAndMakeVisible(*fc);

            effectChooser = std::move(fc);
            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::MAIN_VU_METER:
        { // main vu-meter
            vu[0] = componentForSkinSession<Surge::Widgets::VuMeter>(skinCtrl->sessionid);
            vu[0]->setBounds(skinCtrl->getRect().asJuceIntRect());
            vu[0]->setSkin(currentSkin, bitmapStore);
            vu[0]->setType(vut_vu_stereo);
            frame->juceComponent()->addAndMakeVisible(*vu[0]);
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
                    showMidiLearnOverlay(param[p->id]
                                             ->asControlValueInterface()
                                             ->asBaseViewFunctions()
                                             ->getControlViewSize());
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

    // Make sure the infowindow is here
    infowindow->setVisible(false);
    frame->juceComponent()->addAndMakeVisible(infowindow.get());

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

            auto dcol = VSTGUI::CColor(255, 255, 255).withAlpha((uint8_t)0);
            auto bgcoln = currentSkin->propertyValue(l, Surge::Skin::Component::BACKGROUND_COLOR,
                                                     "#FFFFFF00");
            auto bgcol = currentSkin->getColor(bgcoln, dcol);

            auto frcoln =
                currentSkin->propertyValue(l, Surge::Skin::Component::FRAME_COLOR, "#FFFFFF00");
            auto frcol = currentSkin->getColor(frcoln, dcol);

            auto lb = new CTextLabel(CRect(CPoint(l->x, l->y), CPoint(l->w, l->h)),
                                     mtext.fromJust().c_str());
            lb->setTransparency((bgcol == dcol && frcol == dcol));
            lb->setHoriAlign(txtalign);

            lb->setFontColor(col);
            lb->setBackColor(bgcol);
            lb->setFrameColor(frcol);

            frame->addView(lb);
        }
        else
        {
            auto image = currentSkin->propertyValue(l, Surge::Skin::Component::IMAGE);
            if (image.isJust())
            {
                auto bmp = bitmapStore->getBitmapByStringID(image.fromJust());
                if (bmp)
                {
                    auto lb =
                        new CTextLabel(CRect(CPoint(l->x, l->y), CPoint(l->w, l->h)), nullptr, bmp);
                    frame->addView(lb);
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
    auto dl = std::string("D ") + Surge::Build::BuildTime + " " + Surge::Build::GitBranch;
    auto lb = new CTextLabel(CRect(CPoint(310, 39), CPoint(195, 15)), dl.c_str());
    lb->setTransparency(false);
    lb->setBackColor(juce::Colours::red);
    lb->setFontColor(juce::Colours::white);
    lb->setFont(Surge::GUI::getFontManager()->displayFont);
    lb->setHoriAlign(VSTGUI::kCenterText);
    lb->setAntialias(true);
    frame->addView(lb);
    debugLabel = lb;
#endif
    for (auto el : editorOverlay)
    {
        frame->addView(el.second);
        auto contents = editorOverlayContentsWeakReference[el.second];

        /*
         * This is a hack for 1.8 which we have to clean up in 1.9 when we do #3223
         */
        auto mse = dynamic_cast<RefreshableOverlay *>(contents);
        if (mse)
        {
            mse->forceRefresh();
        }
    }

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

    /* When we rebuild, the onMouseEntered event is not re-sent to the new component by VSTGui;
     * Maybe this is a bug in VSTG? But it screws up our hover states so force an onMouseEntered
     * if we are over a coponent
     */
    CPoint tr;
    frame->getCurrentMouseLocation(tr);

    // OK to *find* the component we need to look in transformed coordinates
    CPoint ttr = tr;
    frame->getTransform().transform(ttr);
    auto v = frame->getViewAt(ttr);

    if (v)
    {
        // but to *hover* the component we need to do it in 100-scale coordinates
        v->onMouseEntered(tr, 0);
    }

    frame->invalid();
}

void SurgeGUIEditor::close_editor()
{
    editor_open = false;
    lfodisplay = 0;
    frame->removeAll(true);
    frame->juceComponent()->removeAllChildren();
    setzero(param);
}

bool SurgeGUIEditor::open(void *parent)
{
    super::open(parent);

    int platformType = 0;
    float fzf = getZoomFactor() / 100.0;
    CRect wsize(0, 0, currentSkin->getWindowSizeX(), currentSkin->getWindowSizeY());

    CFrame *nframe = new CFrame(wsize, this);

    nframe->remember();

    bitmapStore.reset(new SurgeBitmaps());
    bitmapStore->setupBitmapsForFrame(nframe);
    currentSkin->reloadSkin(bitmapStore);
    nframe->setZoom(fzf);

    frame = nframe;

    frame->open(parent, platformType);

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
    for (auto el : editorOverlay)
    {
        frame->removeView(el.second);
        auto f = editorOverlayOnClose[el.second];
        f();
        editorOverlayTagAtClose = el.first;
    }
    super::close();
    hasIdleRun = false;
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

void decode_controllerid(char *txt, int id)
{
    int type = (id & 0xffff0000) >> 16;
    int num = (id & 0xffff);
    switch (type)
    {
    case 1:
        snprintf(txt, TXT_SIZE, "NRPN %i", num);
        break;
    case 2:
        snprintf(txt, TXT_SIZE, "RPN %i", num);
        break;
    default:
        snprintf(txt, TXT_SIZE, "CC %i", num);
        break;
    };
}

int32_t SurgeGUIEditor::controlModifierClicked(CControlValueInterface *control, CButtonState button)
{
    if (!synth)
        return 0;
    if (!frame)
        return 0;
    if (!editor_open)
        return 0;

    if (button & (kMButton | kButton4 | kButton5))
    {
        toggle_mod_editing();
        return 1;
    }

    CRect viewSize;
    auto bvf = dynamic_cast<BaseViewFunctions *>(control);
    if (bvf)
    {
        viewSize = bvf->getControlViewSize();
    }
    else
    {
        jassert(false);
        std::cout << "WARNING: Control not castable to a BaseViewFunctions.\n"
                  << "Is your JUCE-only inheritance hierarchy missing the tag class?" << std::endl;
    }

    long tag = control->getTag();

    if ((button & kControl) && (tag == tag_mp_patch || tag == tag_mp_category))
    {
        synth->selectRandomPatch();
        return 1;
    }

    // In these cases just move along with success. RMB does nothing on these switches
    if (tag == tag_mp_jogfx || tag == tag_mp_category || tag == tag_mp_patch || tag == tag_store ||
        tag == tag_store_cancel || tag == tag_store_ok)
        return 1;

    if (tag == tag_lfo_menu)
    {
        control->setValue(0);
        CPoint where;
        frame->getCurrentMouseLocation(where);
        frame->localToFrame(where);

        showLfoMenu(where);
        return 1;
    }

    if (tag == tag_status_zoom)
    {
        CPoint where;
        frame->getCurrentMouseLocation(where);
        frame->localToFrame(where);

        showZoomMenu(where);
        return 1;
    }
    if (tag == tag_status_mpe)
    {
        CPoint where;
        frame->getCurrentMouseLocation(where);
        frame->localToFrame(where);

        showMPEMenu(where);
        return 1;
    }
    if (tag == tag_status_tune)
    {
        CPoint where;
        frame->getCurrentMouseLocation(where);
        frame->localToFrame(where);

        showTuningMenu(where);
        return 1;
    }

    std::vector<std::string> clearControlTargetNames;

    auto cmensl = dynamic_cast<Surge::Widgets::MenuForDiscreteParams *>(control);

    if (cmensl && cmensl->getDeactivated())
        return 0;

    if (button & kRButton)
    {
        if (tag == tag_settingsmenu)
        {
            CRect r = viewSize;
            CRect menuRect;
            CPoint where;
            frame->getCurrentMouseLocation(where);
            frame->localToFrame(where);

            menuRect.offset(where.x, where.y);

            useDevMenu = true;
            showSettingsMenu(menuRect);
            return 1;
        }

        if (tag == tag_osc_select)
        {
            CRect r = viewSize;

            CPoint where;
            frame->getCurrentMouseLocation(where);
            frame->localToFrame(where);

            int a = limit_range((int)((3 * (where.x - r.left)) / r.getWidth()), 0, 2);

            auto contextMenu = juce::PopupMenu();
            auto hu = helpURLForSpecial("osc-select");
            char txt[TXT_SIZE];

            if (hu != "")
            {
                snprintf(txt, TXT_SIZE, "[?] Osc %d", a + 1);
                auto lurl = fullyResolvedHelpURL(hu);
                contextMenu.addItem(txt, [lurl]() { juce::URL(lurl).launchInDefaultBrowser(); });
            }
            else
            {
                snprintf(txt, TXT_SIZE, "Osc %d", a + 1);
                contextMenu.addItem(txt, []() {});
            }

            contextMenu.addSeparator();

            contextMenu.addItem(
                "Copy", [this, a]() { synth->storage.clipboard_copy(cp_osc, current_scene, a); });

            contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Copy With Modulation"), [this, a]() {
                synth->storage.clipboard_copy(cp_oscmod, current_scene, a);
            });

            if (synth->storage.get_clipboard_type() == cp_osc)
            {
                contextMenu.addItem("Paste", [this, a]() {
                    synth->clear_osc_modulation(current_scene, a);
                    synth->storage.clipboard_paste(cp_osc, current_scene, a);
                    queue_refresh = true;
                });
            }

            contextMenu.showMenuAsync(juce::PopupMenu::Options());

            return 1;
        }

        if (tag == tag_scene_select)
        {
            CRect r = viewSize;
            CRect menuRect;
            CPoint where;
            frame->getCurrentMouseLocation(where);
            frame->localToFrame(where);

            // Assume vertical alignment of the scene buttons
            int a = limit_range((int)((2 * (where.y - r.top)) / r.getHeight()), 0, n_scenes);

            if (auto aschsw = dynamic_cast<Surge::Widgets::MultiSwitch *>(control))
            {
                if (aschsw->getColumns() == n_scenes)
                {
                    // We are horizontal due to skins or whatever so
                    a = limit_range((int)((2 * (where.x - r.left)) / r.getWidth()), 0, n_scenes);
                }
            }

            menuRect.offset(where.x, where.y);

            auto contextMenu = juce::PopupMenu();

            auto hu = helpURLForSpecial("scene-select");
            char txt[TXT_SIZE];
            if (hu != "")
            {
                snprintf(txt, TXT_SIZE, "[?] Scene %c", 'A' + a);
                auto lurl = fullyResolvedHelpURL(hu);
                contextMenu.addItem(txt, [lurl]() { juce::URL(lurl).launchInDefaultBrowser(); });
            }
            else
            {
                snprintf(txt, TXT_SIZE, "Scene %c", 'A' + a);
                contextMenu.addItem(txt, []() {});
            }
            contextMenu.addSeparator();
            contextMenu.addItem("Copy",
                                [this, a]() { synth->storage.clipboard_copy(cp_scene, a, -1); });
            contextMenu.addItem("Paste",
                                synth->storage.get_clipboard_type() == cp_scene, // enabled
                                false, [this, a]() {
                                    synth->storage.clipboard_paste(cp_scene, a, -1);
                                    queue_refresh = true;
                                });

            contextMenu.showMenuAsync(juce::PopupMenu::Options());

            return 1;
        }
    }

    if ((tag >= tag_mod_source0) && (tag < tag_mod_source_end))
    {
        modsources modsource = (modsources)(tag - tag_mod_source0);

        if (button & kRButton)
        {
            CModulationSourceButton *cms = (CModulationSourceButton *)control;
            juce::PopupMenu contextMenu;

            std::string hu;
            if (modsource >= ms_ctrl1 && modsource <= ms_ctrl8)
            {
                hu = helpURLForSpecial("macro-modbutton");
            }
            else if (modsource >= ms_lfo1 && modsource <= ms_slfo6)
            {
                hu = helpURLForSpecial("lfo-modbutton");
            }
            else if ((modsource >= ms_ampeg && modsource <= ms_filtereg) ||
                     (modsource >= ms_random_bipolar && modsource <= ms_alternate_unipolar))
            {
                hu = helpURLForSpecial("internalmod-modbutton");
            }
            else
            {
                hu = helpURLForSpecial("other-modbutton");
            }

            if (cms->hasAlternate)
            {
                int idOn = modsource;
                int idOff = cms->alternateId;

                if (cms->useAlternate)
                {
                    auto t = idOn;
                    idOn = idOff;
                    idOff = t;
                }

                if (hu != "")
                {
                    auto lurl = fullyResolvedHelpURL(hu);
                    std::string hs = std::string("[?] ") + (char *)modsource_names[idOn];
                    addCallbackMenu(contextMenu, hs,
                                    [lurl]() { juce::URL(lurl).launchInDefaultBrowser(); });
                }
                else
                {
                    contextMenu.addItem(modsource_names[idOn], []() {});
                }

                bool activeMod = (cms->state & 3) == 2;
                std::string offLab = "Switch to ";
                offLab += modsource_names[idOff];

                contextMenu.addItem(offLab, !activeMod, false, [this, modsource, cms]() {
                    cms->setUseAlternate(!cms->useAlternate);
                    modsource_is_alternate[modsource] = cms->useAlternate;
                    this->refresh_mod();
                });
            }
            else
            {
                if (hu != "")
                {
                    auto lurl = fullyResolvedHelpURL(hu);
                    std::string hs = std::string("[?] ") + modulatorName(modsource, false);
                    addCallbackMenu(contextMenu, hs,
                                    [lurl]() { juce::URL(lurl).launchInDefaultBrowser(); });
                }
                else
                {
                    contextMenu.addItem(modulatorName(modsource, false), []() {});
                }
            }

            contextMenu.addSeparator();
            addCallbackMenu(contextMenu, Surge::GUI::toOSCaseForMenu("Modulation Editor"),
                            [this]() {
                                if (!isAnyOverlayPresent(MODULATION_EDITOR))
                                    showModulationEditorDialog();
                            });
            int n_total_md = synth->storage.getPatch().param_ptr.size();

            const int max_md = 4096;
            assert(max_md >= n_total_md);

            bool cancellearn = false;
            int ccid = 0;

            int detailedMode = Surge::Storage::getUserDefaultValue(
                &(this->synth->storage), Surge::Storage::HighPrecisionReadouts, 0);

            // should start at 0, but started at 1 before.. might be a reason but don't remember
            // why...
            std::vector<modsources> possibleSources;
            possibleSources.push_back(modsource);
            if (cms->hasAlternate)
            {
                possibleSources.push_back((modsources)(cms->alternateId));
            }

            for (auto thisms : possibleSources)
            {
                bool first_destination = true;
                int n_md = 0;

                for (int md = 0; md < n_total_md; md++)
                {
                    auto activeScene = synth->storage.getPatch().scene_active.val.i;
                    Parameter *parameter = synth->storage.getPatch().param_ptr[md];

                    if (((md < n_global_params) || ((parameter->scene - 1) == activeScene)) &&
                        synth->isActiveModulation(md, thisms))
                    {
                        char modtxt[TXT_SIZE];
                        auto pmd = synth->storage.getPatch().param_ptr[md];
                        pmd->get_display_of_modulation_depth(modtxt, synth->getModDepth(md, thisms),
                                                             synth->isBipolarModulation(thisms),
                                                             Parameter::Menu);
                        char tmptxt[1024]; // leave room for that ubuntu 20.0 error
                        if (pmd->ctrlgroup == cg_LFO)
                        {
                            char pname[TXT_SIZE];
                            pmd->create_fullname(pmd->get_name(), pname, pmd->ctrlgroup,
                                                 pmd->ctrlgroup_entry,
                                                 modulatorName(pmd->ctrlgroup_entry, true).c_str());
                            snprintf(tmptxt, TXT_SIZE, "Edit %s -> %s: %s",
                                     (char *)modulatorName(thisms, true).c_str(), pname, modtxt);
                        }
                        else
                        {
                            snprintf(tmptxt, TXT_SIZE, "Edit %s -> %s: %s",
                                     (char *)modulatorName(thisms, true).c_str(),
                                     pmd->get_full_name(), modtxt);
                        }

                        auto clearOp = [this, parameter, bvf, thisms]() {
                            this->promptForUserValueEntry(parameter, bvf, thisms);
                        };

                        if (first_destination)
                        {
                            contextMenu.addSeparator();
                            first_destination = false;
                        }

                        addCallbackMenu(contextMenu, tmptxt, clearOp);
                    }
                }

                first_destination = true;
                for (int md = 0; md < n_total_md; md++)
                {
                    auto activeScene = synth->storage.getPatch().scene_active.val.i;
                    Parameter *parameter = synth->storage.getPatch().param_ptr[md];

                    if (((md < n_global_params) || ((parameter->scene - 1) == activeScene)) &&
                        synth->isActiveModulation(md, thisms))
                    {
                        char tmptxt[1024];
                        if (parameter->ctrlgroup == cg_LFO)
                        {
                            char pname[512];
                            parameter->create_fullname(
                                parameter->get_name(), pname, parameter->ctrlgroup,
                                parameter->ctrlgroup_entry,
                                modulatorName(parameter->ctrlgroup_entry, true).c_str());
                            snprintf(tmptxt, TXT_SIZE, "Clear %s -> %s",
                                     (char *)modulatorName(thisms, true).c_str(), pname);
                        }
                        else
                        {
                            snprintf(tmptxt, TXT_SIZE, "Clear %s -> %s",
                                     (char *)modulatorName(thisms, true).c_str(),
                                     parameter->get_full_name());
                        }

                        auto clearOp = [this, first_destination, md, n_total_md, thisms, bvf,
                                        control]() {
                            bool resetName = false;   // Should I reset the name?
                            std::string newName = ""; // And to what?
                            int ccid = thisms - ms_ctrl1;

                            if (first_destination)
                            {
                                if (strncmp(synth->storage.getPatch().CustomControllerLabel[ccid],
                                            synth->storage.getPatch().param_ptr[md]->get_name(),
                                            15) == 0)
                                {
                                    // So my modulator is named after my short name. I haven't been
                                    // renamed. So I want to reset at least to "-" unless someone is
                                    // after me
                                    resetName = true;
                                    newName = "-";

                                    // Now we have to find if there's another modulation below me
                                    int nextmd = md + 1;
                                    while (nextmd < n_total_md &&
                                           !synth->isActiveModulation(nextmd, thisms))
                                        nextmd++;
                                    if (nextmd < n_total_md && strlen(synth->storage.getPatch()
                                                                          .param_ptr[nextmd]
                                                                          ->get_name()) > 1)
                                        newName =
                                            synth->storage.getPatch().param_ptr[nextmd]->get_name();
                                }
                            }

                            synth->clearModulation(md, thisms);
                            refresh_mod();
                            if (bvf)
                                bvf->invalid();

                            if (resetName)
                            {
                                // And this is where we apply the name refresh, of course.
                                strxcpy(synth->storage.getPatch().CustomControllerLabel[ccid],
                                        newName.c_str(), 15);
                                synth->storage.getPatch().CustomControllerLabel[ccid][15] = 0;
                                ((CModulationSourceButton *)control)
                                    ->setlabel(
                                        synth->storage.getPatch().CustomControllerLabel[ccid]);
                                if (bvf)
                                    bvf->invalid();
                                synth->updateDisplay();
                            }
                        };

                        if (first_destination)
                        {
                            contextMenu.addSeparator();
                            first_destination = false;
                        }

                        addCallbackMenu(contextMenu, tmptxt, clearOp);
                        n_md++;
                    }
                }
                if (n_md)
                {
                    std::string clearLab;
                    std::string modName;

                    // hacky, but works - we want to retain the capitalization for modulator names
                    // regardless of OS!
                    modName = modulatorName(thisms, false);
                    clearLab = Surge::GUI::toOSCaseForMenu("Clear All ") + modName +
                               Surge::GUI::toOSCaseForMenu(" Routings");

                    addCallbackMenu(contextMenu, clearLab, [this, n_total_md, thisms, control]() {
                        for (int md = 1; md < n_total_md; md++)
                            synth->clearModulation(md, thisms);
                        refresh_mod();

                        // Also blank out the name and rebuild the UI
                        if (within_range(ms_ctrl1, thisms, ms_ctrl1 + n_customcontrollers - 1))
                        {
                            int ccid = thisms - ms_ctrl1;

                            synth->storage.getPatch().CustomControllerLabel[ccid][0] = '-';
                            synth->storage.getPatch().CustomControllerLabel[ccid][1] = 0;
                            ((CModulationSourceButton *)control)
                                ->setlabel(synth->storage.getPatch().CustomControllerLabel[ccid]);
                        }
                        if (control->asBaseViewFunctions())
                            control->asBaseViewFunctions()->invalid();
                        synth->updateDisplay();
                    });
                }
            }
            int sc = limit_range(synth->storage.getPatch().scene_active.val.i, 0, n_scenes - 1);

            if (within_range(ms_ctrl1, modsource, ms_ctrl1 + n_customcontrollers - 1))
            {
                /*
                ** This is the menu for the controls
                */

                ccid = modsource - ms_ctrl1;

                auto cms = ((ControllerModulationSource *)synth->storage.getPatch()
                                .scene[current_scene]
                                .modsources[modsource]);

                contextMenu.addSeparator();
                char vtxt[1024];
                snprintf(vtxt, 1024, "%s: %.*f %%",
                         Surge::GUI::toOSCaseForMenu("Edit Value").c_str(), (detailedMode ? 6 : 2),
                         100 * cms->get_output());
                addCallbackMenu(contextMenu, vtxt, [this, bvf, modsource]() {
                    promptForUserValueEntry(nullptr, bvf, modsource);
                });

                contextMenu.addSeparator();

                char txt[TXT_SIZE];

                // Construct submenus for explicit controller mapping
                juce::PopupMenu midiSub;
                juce::PopupMenu currentSub;
                bool firstSub = true;

                bool isChecked = false;
                for (int subs = 0; subs < 7; ++subs)
                {
                    if (!firstSub)
                    {
                        char name[16];
                        snprintf(name, 16, "CC %d ... %d", (subs - 1) * 20,
                                 min(((subs - 1) * 20) + 20, 128) - 1);
                        midiSub.addSubMenu(name, currentSub, true, nullptr, isChecked);

                        currentSub = juce::PopupMenu();
                    }

                    isChecked = false;

                    for (int item = 0; item < 20; ++item)
                    {
                        int mc = (subs * 20) + item;
                        int disabled = 0;

                        // these CCs cannot be used for MIDI learn (see
                        // SurgeSynthesizer::channelController)
                        if (mc == 0 || mc == 6 || mc == 32 || mc == 38 || mc == 64 ||
                            (mc == 74 && synth->mpeEnabled) || (mc >= 98 && mc <= 101) ||
                            mc == 120 || mc == 123)
                            disabled = 1;

                        // we don't have any more CCs to cover, so break the loop
                        if (mc > 127)
                            break;

                        char name[128];

                        snprintf(name, 128, "CC %d (%s) %s", mc, midicc_names[mc],
                                 (disabled == 1 ? "- RESERVED" : ""));

                        bool thisIsChecked = false;
                        if (synth->storage.controllers[ccid] == mc)
                        {
                            thisIsChecked = true;
                            isChecked = true;
                        }

                        currentSub.addItem(name, true, thisIsChecked, [this, ccid, mc]() {
                            synth->storage.controllers[ccid] = mc;
                        });
                    }
                }

                int subs = 7;
                char name[16];
                snprintf(name, 16, "CC %d ... %d", (subs - 1) * 20,
                         min(((subs - 1) * 20) + 20, 128) - 1);
                midiSub.addSubMenu(name, currentSub, true, nullptr, isChecked);

                contextMenu.addSubMenu(Surge::GUI::toOSCaseForMenu("Assign Macro To..."), midiSub);

                if (synth->learn_custom > -1 && synth->learn_custom == ccid)
                    cancellearn = true;

                std::string learnTag =
                    cancellearn ? "Abort Macro MIDI Learn" : "MIDI Learn Macro...";
                addCallbackMenu(contextMenu, Surge::GUI::toOSCaseForMenu(learnTag),
                                [this, cancellearn, control, ccid, viewSize] {
                                    if (cancellearn)
                                    {
                                        hideMidiLearnOverlay();
                                        synth->learn_custom = -1;
                                    }
                                    else
                                    {
                                        showMidiLearnOverlay(viewSize);
                                        synth->learn_custom = ccid;
                                    }
                                });

                if (synth->storage.controllers[ccid] >= 0)
                {
                    char txt4[16];
                    decode_controllerid(txt4, synth->storage.controllers[ccid]);
                    snprintf(txt, TXT_SIZE, "Clear Learned MIDI (%s ", txt4);
                    addCallbackMenu(contextMenu,
                                    Surge::GUI::toOSCaseForMenu(txt) +
                                        midicc_names[synth->storage.controllers[ccid]] + ")",
                                    [this, ccid]() { synth->storage.controllers[ccid] = -1; });
                }

                contextMenu.addSeparator();

                bool ibp = synth->storage.getPatch()
                               .scene[current_scene]
                               .modsources[ms_ctrl1 + ccid]
                               ->is_bipolar();
                contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Bipolar Mode"), true, ibp,
                                    [this, control, ccid]() {
                                        bool bp = !synth->storage.getPatch()
                                                       .scene[current_scene]
                                                       .modsources[ms_ctrl1 + ccid]
                                                       ->is_bipolar();
                                        synth->storage.getPatch()
                                            .scene[current_scene]
                                            .modsources[ms_ctrl1 + ccid]
                                            ->set_bipolar(bp);

                                        float f = synth->storage.getPatch()
                                                      .scene[current_scene]
                                                      .modsources[ms_ctrl1 + ccid]
                                                      ->get_output01();
                                        control->setValue(f);
                                        ((CModulationSourceButton *)control)->setBipolar(bp);
                                        refresh_mod();
                                    });

                addCallbackMenu(
                    contextMenu, Surge::GUI::toOSCaseForMenu("Rename Macro..."),
                    [this, control, ccid]() {
                        std::string pval = synth->storage.getPatch().CustomControllerLabel[ccid];
                        if (pval == "-")
                            pval = "";
                        promptForMiniEdit(
                            pval, "Enter a new name for the macro:", "Rename Macro",
                            control->asBaseViewFunctions()->getControlViewSize().getTopLeft(),
                            [this, control, ccid](const std::string &s) {
                                auto useS = s;
                                if (useS == "")
                                    useS = "-";
                                strxcpy(synth->storage.getPatch().CustomControllerLabel[ccid],
                                        useS.c_str(), 16);
                                synth->storage.getPatch().CustomControllerLabel[ccid][15] =
                                    0; // to be sure
                                ((CModulationSourceButton *)control)
                                    ->setlabel(
                                        synth->storage.getPatch().CustomControllerLabel[ccid]);

                                if (control && control->asBaseViewFunctions())
                                    control->asBaseViewFunctions()->invalid();
                                synth->refresh_editor = true;
                                // synth->updateDisplay();
                            });
                    });
            }

            int lfo_id = isLFO(modsource) ? modsource - ms_lfo1 : -1;

            if (lfo_id >= 0)
            {
                contextMenu.addSeparator();
                addCallbackMenu(contextMenu, "Copy", [this, sc, lfo_id]() {
                    if (lfo_id >= 0)
                        synth->storage.clipboard_copy(cp_lfo, sc, lfo_id);
                    mostRecentCopiedMSEGState = msegEditState[sc][lfo_id];
                });

                if (synth->storage.get_clipboard_type() == cp_lfo)
                {
                    addCallbackMenu(contextMenu, "Paste", [this, sc, lfo_id]() {
                        if (lfo_id >= 0)
                            synth->storage.clipboard_paste(cp_lfo, sc, lfo_id);
                        msegEditState[sc][lfo_id] = mostRecentCopiedMSEGState;
                        queue_refresh = true;
                    });
                }
            }

            // FIXME: This one shouldn't be async yet since the macro can get swept underneath us
            contextMenu.showMenu(juce::PopupMenu::Options());
            return 1;
        }
        return 0;
    }

    if (tag < start_paramtags)
        return 0;

    if (!(button & (kDoubleClick | kRButton | kControl)))
        return 0;

    int ptag = tag - start_paramtags;

    if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
    {
        Parameter *p = synth->storage.getPatch().param_ptr[ptag];

        // don't show RMB context menu for filter subtype if it's hidden/not applicable
        auto f1type = synth->storage.getPatch().scene[current_scene].filterunit[0].type.val.i;
        auto f2type = synth->storage.getPatch().scene[current_scene].filterunit[1].type.val.i;

        if (tag == f1subtypetag && (f1type == fut_none || f1type == fut_SNH))
            return 1;
        if (tag == f2subtypetag && (f2type == fut_none || f2type == fut_SNH))
            return 1;

        bool blockForLFO = false;
        if (p->ctrltype == ct_lfotype)
        {
            blockForLFO = true;
            auto *clfo = dynamic_cast<CLFOGui *>(control);
            if (clfo)
            {
                CPoint where;
                frame->getCurrentMouseLocation(where);
                frame->localToFrame(where);

                blockForLFO = !clfo->insideTypeSelector(where);
            }
        }

        // all the RMB context menus
        if ((button & kRButton) && !blockForLFO)
        {
            CRect menuRect;
            CPoint where;
            frame->getCurrentMouseLocation(where);
            frame->localToFrame(where);

            menuRect.offset(where.x, where.y);

            COptionMenu *contextMenu = new COptionMenu(
                menuRect, 0, 0, 0, 0,
                VSTGUI::COptionMenu::kNoDrawStyle | VSTGUI::COptionMenu::kMultipleCheckStyle);
            int eid = 0;

            // FIXME - only fail at this once
            /* if( bitmapStore->getBitmapByPath( synth->storage.datapath +
            "skins/shared/help-24.svg" ) == nullptr )
            {
               std::cout << "LOADING BMP" << std::endl;
               bitmapStore->loadBitmapByPath( synth->storage.datapath + "skins/shared/help-24.svg"
            );
            }
            auto helpbmp = bitmapStore->getBitmapByPath( synth->storage.datapath +
            "skins/shared/help-24.svg" ); std::cout << "HELPBMP is " << helpbmp << std::endl; */
            // auto pp = IPlatformBitmap::createFromPath( (synth->storage.datapath +
            // "skins/shared/help-14.png" ).c_str() ); auto helpbmp = new CBitmap( pp );

            std::string helpurl = helpURLFor(p);

            if (helpurl == "")
            {
                contextMenu->addEntry((char *)p->get_full_name(), eid++);
            }
            else
            {
                std::string helpstr = "[?] ";
                auto lurl = fullyResolvedHelpURL(helpurl);
                addCallbackMenu(contextMenu, std::string(helpstr + p->get_full_name()).c_str(),
                                [lurl]() { juce::URL(lurl).launchInDefaultBrowser(); });
                eid++;
            }

            contextMenu->addSeparator(eid++);

            char txt[TXT_SIZE], txt2[512];
            p->get_display(txt);
            snprintf(txt2, 512, "%s: %s", Surge::GUI::toOSCaseForMenu("Edit Value").c_str(), txt);

            if (p->valtype == vt_float)
            {
                if (p->can_temposync() && p->temposync)
                {
                    COptionMenu *tsMenuR =
                        new COptionMenu(menuRect, 0, 0, 0, 0,
                                        VSTGUI::COptionMenu::kNoDrawStyle |
                                            VSTGUI::COptionMenu::kMultipleCheckStyle);
                    COptionMenu *tsMenuD =
                        new COptionMenu(menuRect, 0, 0, 0, 0,
                                        VSTGUI::COptionMenu::kNoDrawStyle |
                                            VSTGUI::COptionMenu::kMultipleCheckStyle);
                    COptionMenu *tsMenuT =
                        new COptionMenu(menuRect, 0, 0, 0, 0,
                                        VSTGUI::COptionMenu::kNoDrawStyle |
                                            VSTGUI::COptionMenu::kMultipleCheckStyle);

                    // have 0 1 2 then 0 + log2 1.5 1 + log2 1.5 then 0 + log2 1.33 and so on
                    for (int i = p->val_min.f; i <= p->val_max.f; ++i)
                    {
                        float mul = 1.0;
                        float triplaboff = log2(1.33333333);
                        float tripoff = triplaboff;
                        float dotlaboff = log2(1.5);
                        float dotoff = dotlaboff;
                        if (p->ctrltype == ct_lforate || p->ctrltype == ct_lforate_deactivatable)
                        {
                            mul = -1.0;
                            triplaboff = log2(1.5);
                            tripoff = triplaboff;
                            dotlaboff = log2(1.3333333333);
                            dotoff = dotlaboff;
                        }
                        addCallbackMenu(tsMenuR, p->tempoSyncNotationValue(mul * ((float)i)),
                                        [p, i, this]() {
                                            p->val.f = (float)i;
                                            p->bound_value();
                                            this->synth->refresh_editor = true;
                                        });
                        addCallbackMenu(tsMenuD,
                                        p->tempoSyncNotationValue(mul * ((float)i + dotlaboff)),
                                        [p, i, dotoff, this]() {
                                            p->val.f = (float)i + dotoff;
                                            p->bound_value();
                                            this->synth->refresh_editor = true;
                                        });
                        addCallbackMenu(tsMenuT,
                                        p->tempoSyncNotationValue(mul * ((float)i + triplaboff)),
                                        [p, i, tripoff, this]() {
                                            p->val.f = (float)i + tripoff;
                                            p->bound_value();
                                            this->synth->refresh_editor = true;
                                        });
                    }
                    contextMenu->addEntry(txt2);
                    eid++;
                    contextMenu->addSeparator();
                    eid++;
                    contextMenu->addEntry(tsMenuR, "Straight");
                    tsMenuR->forget();
                    eid++;
                    contextMenu->addEntry(tsMenuD, "Dotted");
                    tsMenuD->forget();
                    eid++;
                    contextMenu->addEntry(tsMenuT, "Triplet");
                    tsMenuT->forget();
                    eid++;
                    contextMenu->addSeparator();
                    eid++;
                }
                else
                {
                    addCallbackMenu(contextMenu, txt2,
                                    [this, p, bvf]() { this->promptForUserValueEntry(p, bvf); });
                    eid++;
                }
            }
            else if (p->valtype == vt_bool)
            {
                // FIXME - make this a checked toggle
                auto b = addCallbackMenu(contextMenu, txt2, [this, p, control]() {
                    SurgeSynthesizer::ID pid;
                    if (synth->fromSynthSideId(p->id, pid))
                    {
                        synth->setParameter01(pid, !p->val.b, false, false);
                        repushAutomationFor(p);
                        synth->refresh_editor = true;
                    }
                });

                eid++;

                if (p->val.b)
                {
                    b->setChecked(true);
                }

                if (p->ctrltype == ct_bool_keytrack || p->ctrltype == ct_bool_retrigger)
                {
                    std::vector<Parameter *> impactedParms;
                    std::vector<std::string> tgltxt = {"Enable", "Disable"};
                    std::string parname = "Keytrack";
                    if (p->ctrltype == ct_bool_retrigger)
                    {
                        parname = "Retrigger";
                    }

                    int currvals = 0;
                    // There is surely a more efficient way but this is fine
                    for (auto iter = this->synth->storage.getPatch().param_ptr.begin();
                         iter != this->synth->storage.getPatch().param_ptr.end(); iter++)
                    {
                        Parameter *pl = *iter;
                        if (pl->ctrltype == p->ctrltype && pl->scene == p->scene)
                        {
                            currvals += pl->val.b;
                            impactedParms.push_back(pl);
                        }
                    }

                    int ktsw = (currvals == n_oscs);

                    auto txt3 = Surge::GUI::toOSCaseForMenu(tgltxt[ktsw] + " " + parname +
                                                            " for All Oscillators");

                    auto b =
                        addCallbackMenu(contextMenu, txt3.c_str(), [this, ktsw, impactedParms]() {
                            for (auto *p : impactedParms)
                            {
                                SurgeSynthesizer::ID pid;

                                if (synth->fromSynthSideId(p->id, pid))
                                {
                                    synth->setParameter01(pid, 1 - ktsw, false, false);
                                    repushAutomationFor(p);
                                }
                            }

                            synth->refresh_editor = true;
                        });

                    eid++;
                }
            }
            else if (p->valtype == vt_int)
            {
                if (p->can_setvalue_from_string())
                {
                    addCallbackMenu(contextMenu, txt2,
                                    [this, p, bvf]() { this->promptForUserValueEntry(p, bvf); });
                    eid++;
                }
                else
                {
                    int incr = 1;

                    if (p->ctrltype == ct_vocoder_bandcount)
                        incr = 4;

                    // we have a case where the number of menu entries to be generated depends on
                    // another parameter so instead of using val_max.i directly, store it to local
                    // var and modify its value when required
                    int max = p->val_max.i;

                    // currently we only have this case with filter subtypes - different filter
                    // types have a different number of them so let's do this!
                    bool isCombOnSubtype = false;
                    if (p->ctrltype == ct_filtersubtype)
                    {
                        auto ftype = synth->storage.getPatch()
                                         .scene[current_scene]
                                         .filterunit[p->ctrlgroup_entry]
                                         .type.val.i;
                        if (ftype == fut_comb_pos || ftype == fut_comb_neg)
                            isCombOnSubtype = true;
                        max = fut_subcount[ftype] - 1;
                    }

                    auto dm = dynamic_cast<ParameterDiscreteIndexRemapper *>(p->user_data);
                    if (dm)
                    {
                        std::unordered_map<std::string, std::map<int, int>> reorderMap;
                        std::vector<std::string> groupAppearanceOrder;
                        for (int i = p->val_min.i; i <= max; i += incr)
                        {
                            int idx = dm->remapStreamedIndexToDisplayIndex(i);
                            if (idx >= 0)
                            {
                                auto gn = dm->groupNameAtStreamedIndex(i);
                                if (reorderMap.find(gn) == reorderMap.end())
                                {
                                    groupAppearanceOrder.push_back(gn);
                                }

                                reorderMap[gn][idx] = i;
                            }
                        }

                        if (dm->sortGroupNames())
                            std::sort(groupAppearanceOrder.begin(), groupAppearanceOrder.end());
                        else if (dm->useRemappedOrderingForGroupsIfNotSorted())
                        {
                            groupAppearanceOrder.clear();
                            std::map<int, std::string> gnr;
                            for (int i = p->val_min.i; i <= max; i += incr)
                            {
                                int idx = dm->remapStreamedIndexToDisplayIndex(i);
                                if (idx >= 0)
                                {
                                    gnr[idx] = dm->groupNameAtStreamedIndex(i);
                                }
                            }
                            std::unordered_set<std::string> dealt;
                            for (const auto &p : gnr)
                            {
                                if (dealt.find(p.second) == dealt.end())
                                {
                                    groupAppearanceOrder.push_back(p.second);
                                    dealt.insert(p.second);
                                }
                            }
                        }

                        bool useSubMenus = dm->hasGroupNames();
                        COptionMenu *sub = nullptr;
                        for (auto grpN : groupAppearanceOrder)
                        {
                            auto grp = reorderMap[grpN];
                            bool checkSub = false;
                            if (useSubMenus)
                            {
                                sub = new COptionMenu(menuRect, 0, 0, 0, 0,
                                                      VSTGUI::COptionMenu::kNoDrawStyle |
                                                          VSTGUI::COptionMenu::kMultipleCheckStyle);
                            }
                            for (auto rp : grp)
                            {
                                int i = rp.second;
                                std::string displaytxt = dm->nameAtStreamedIndex(i);
                                auto addTo = useSubMenus ? sub : contextMenu;
                                if (useSubMenus && grpN == "")
                                    addTo = contextMenu;

                                auto b =
                                    addCallbackMenu(addTo, displaytxt.c_str(), [this, p, i, tag]() {
                                        float ef = Parameter::intScaledToFloat(i, p->val_max.i,
                                                                               p->val_min.i);
                                        synth->setParameter01(synth->idForParameter(p), ef, false,
                                                              false);
                                        repushAutomationFor(p);
                                        synth->refresh_editor = true;
                                    });
                                eid++;
                                if (i == p->val.i)
                                {
                                    b->setChecked(true);
                                    checkSub = true;
                                }
                            }
                            if (useSubMenus && grpN != "")
                            {
                                auto e = contextMenu->addEntry(sub, grpN.c_str());
                                sub->forget();
                                sub = nullptr;
                                e->setChecked(checkSub);
                            }
                        }
                    }
                    else
                    {
                        for (int i = p->val_min.i; i <= max; i += incr)
                        {
                            char txt[256];
                            float ef = (1.0f * i - p->val_min.i) / (p->val_max.i - p->val_min.i);
                            p->get_display(txt, true, ef);

                            std::string displaytxt = txt;

                            auto b = addCallbackMenu(
                                contextMenu, displaytxt.c_str(), [this, ef, p, i]() {
                                    synth->setParameter01(synth->idForParameter(p), ef, false,
                                                          false);
                                    repushAutomationFor(p);
                                    synth->refresh_editor = true;
                                });
                            eid++;
                            if (i == p->val.i)
                                b->setChecked(true);
                        }
                        if (isCombOnSubtype)
                        {
                            contextMenu->addSeparator();
                            auto m = addCallbackMenu(
                                contextMenu, Surge::GUI::toOSCaseForMenu("Precise Tuning"),
                                [this]() {
                                    synth->storage.getPatch().correctlyTuneCombFilter =
                                        !synth->storage.getPatch().correctlyTuneCombFilter;
                                });
                            m->setChecked(synth->storage.getPatch().correctlyTuneCombFilter);
                        }

                        if (p->ctrltype == ct_polymode &&
                            (p->val.i == pm_mono || p->val.i == pm_mono_st ||
                             p->val.i == pm_mono_fp || p->val.i == pm_mono_st_fp))
                        {
                            std::vector<std::string> labels = {"Last", "High", "Low", "Legacy"};
                            std::vector<MonoVoicePriorityMode> vals = {
                                ALWAYS_LATEST, ALWAYS_HIGHEST, ALWAYS_LOWEST,
                                NOTE_ON_LATEST_RETRIGGER_HIGHEST};
                            contextMenu->addSeparator();
                            for (int i = 0; i < 4; ++i)
                            {
                                auto m = addCallbackMenu(
                                    contextMenu,
                                    Surge::GUI::toOSCaseForMenu(labels[i] + " Note Priority"),
                                    [this, vals, i]() {
                                        synth->storage.getPatch()
                                            .scene[current_scene]
                                            .monoVoicePriorityMode = vals[i];
                                    });
                                if (vals[i] == synth->storage.getPatch()
                                                   .scene[current_scene]
                                                   .monoVoicePriorityMode)
                                    m->setChecked(true);
                            }
                            contextMenu->addSeparator();
                            contextMenu->addEntry(
                                makeMonoModeOptionsMenu(menuRect, false),
                                Surge::GUI::toOSCaseForMenu("Sustain Pedal In Mono Mode"));
                        }
                    }
                }
            }
            bool cancellearn = false;

            // Modulation and Learn semantics only apply to vt_float types in Surge right now
            if (p->valtype == vt_float)
            {
                // if(p->can_temposync() || p->can_extend_range()) contextMenu->addSeparator(eid++);
                if (p->can_temposync())
                {
                    auto r = addCallbackMenu(
                        contextMenu, Surge::GUI::toOSCaseForMenu("Tempo Sync"),
                        [this, p, control]() {
                            p->temposync = !p->temposync;
                            if (p->temposync)
                                p->bound_value();
                            else if (control)
                                p->set_value_f01(control->getValue());

                            if (this->lfodisplay)
                                this->lfodisplay->invalid();
                            auto *css = dynamic_cast<Surge::Widgets::ModulatableControlInterface *>(
                                control);
                            if (css)
                            {
                                css->setTempoSync(p->temposync);
                                css->asJuceComponent()->repaint();
                            }
                        });
                    if (p->temposync)
                        r->setChecked(true);
                    eid++;

                    if (p->ctrlgroup == cg_ENV || p->ctrlgroup == cg_LFO)
                    {
                        char label[256];
                        char prefix[128];
                        char pars[32];

                        int a = p->ctrlgroup_entry;

                        if (p->ctrlgroup == cg_ENV)
                        {
                            if (a == 0)
                                sprintf(prefix, "Amp EG");
                            else
                                sprintf(prefix, "Filter EG");
                        }
                        else if (p->ctrlgroup == cg_LFO)
                        {
                            if (a >= ms_lfo1 && a <= ms_lfo1 + n_lfos_voice)
                                sprintf(prefix, "Voice LFO %i", a - ms_lfo1 + 1);
                            else if (a >= ms_slfo1 && a <= ms_slfo1 + n_lfos_scene)
                                sprintf(prefix, "Scene LFO %i", a - ms_slfo1 + 1);
                        }

                        bool setTSTo;

#if WINDOWS
                        snprintf(pars, 32, "parameters");
#else
                        snprintf(pars, 32, "Parameters");
#endif

                        if (p->temposync)
                        {
                            snprintf(
                                label, 256, "%s %s %s",
                                Surge::GUI::toOSCaseForMenu("Disable Tempo Sync for All").c_str(),
                                prefix, pars);
                            setTSTo = false;
                        }
                        else
                        {
                            snprintf(
                                label, 256, "%s %s %s",
                                Surge::GUI::toOSCaseForMenu("Enable Tempo Sync for All").c_str(),
                                prefix, pars);
                            setTSTo = true;
                        }

                        addCallbackMenu(contextMenu, label, [this, p, setTSTo]() {
                            // There is surely a more efficient way but this is fine
                            for (auto iter = this->synth->storage.getPatch().param_ptr.begin();
                                 iter != this->synth->storage.getPatch().param_ptr.end(); iter++)
                            {
                                Parameter *pl = *iter;
                                if (pl->ctrlgroup_entry == p->ctrlgroup_entry &&
                                    pl->ctrlgroup == p->ctrlgroup && pl->can_temposync())
                                {
                                    pl->temposync = setTSTo;
                                    if (setTSTo)
                                        pl->bound_value();
                                }
                            }
                            this->synth->refresh_editor = true;
                        });
                        eid++;
                    }
                }

                switch (p->ctrltype)
                {
                case ct_freq_audible_with_tunability:
                case ct_freq_audible_with_very_low_lowerbound:
                    addCallbackMenu(
                        contextMenu,
                        Surge::GUI::toOSCaseForMenu("Reset Filter Cutoff To Keytrack Root"),
                        [this, p, control] {
                            auto kr = this->synth->storage.getPatch()
                                          .scene[current_scene]
                                          .keytrack_root.val.i;
                            p->set_value_f01(p->value_to_normalized(kr - 69));
                            control->setValue(p->get_value_f01());
                        });
                    eid++;
                    break;

                default:
                    break;
                }

                if (p->has_portaoptions())
                {
                    contextMenu->addSeparator(eid++);

                    auto cr =
                        addCallbackMenu(contextMenu, Surge::GUI::toOSCaseForMenu("Constant Rate"),
                                        [this, p]() { p->porta_constrate = !p->porta_constrate; });
                    cr->setChecked(p->porta_constrate);
                    eid++;

                    auto gliss =
                        addCallbackMenu(contextMenu, Surge::GUI::toOSCaseForMenu("Glissando"),
                                        [this, p]() { p->porta_gliss = !p->porta_gliss; });
                    gliss->setChecked(p->porta_gliss);
                    eid++;

                    auto rtsd = addCallbackMenu(
                        contextMenu, Surge::GUI::toOSCaseForMenu("Retrigger at Scale Degrees"),
                        [this, p]() { p->porta_retrigger = !p->porta_retrigger; });
                    rtsd->setChecked(p->porta_retrigger);
                    eid++;

                    contextMenu->addSeparator(eid++);

                    auto log = addCallbackMenu(contextMenu,
                                               Surge::GUI::toOSCaseForMenu("Logarithmic Curve"),
                                               [this, p]() { p->porta_curve = -1; });
                    log->setChecked(p->porta_curve == -1);
                    eid++;

                    auto lin =
                        addCallbackMenu(contextMenu, Surge::GUI::toOSCaseForMenu("Linear Curve"),
                                        [this, p]() { p->porta_curve = 0; });
                    lin->setChecked(p->porta_curve == 0);
                    eid++;

                    auto exp = addCallbackMenu(contextMenu,
                                               Surge::GUI::toOSCaseForMenu("Exponential Curve"),
                                               [this, p]() { p->porta_curve = 1; });
                    exp->setChecked(p->porta_curve == 1);
                    eid++;
                }

                if (p->has_deformoptions())
                {
                    switch (p->ctrltype)
                    {

                    case ct_lfodeform:
                    {
                        auto q = modsource_editor[current_scene];
                        auto *lfodata =
                            &(synth->storage.getPatch().scene[current_scene].lfo[q - ms_lfo1]);

                        if (lt_num_deforms[lfodata->shape.val.i] > 1)
                        {
                            contextMenu->addSeparator(eid++);

                            for (int i = 0; i < lt_num_deforms[lfodata->shape.val.i]; i++)
                            {
                                char title[32];
                                sprintf(title, "Deform Type %d", (i + 1));

                                auto dm =
                                    addCallbackMenu(contextMenu, Surge::GUI::toOSCaseForMenu(title),
                                                    [this, p, i]() {
                                                        p->deform_type = i;
                                                        if (frame)
                                                            frame->invalid();
                                                    });
                                dm->setChecked(p->deform_type == i);
                                eid++;
                            }
                        }

                        break;
                    }
                    case ct_modern_trimix:
                    {
                        contextMenu->addSeparator();
                        eid++;

                        std::vector<int> waves = {ModernOscillator::mo_multitypes::momt_triangle,
                                                  ModernOscillator::mo_multitypes::momt_sine,
                                                  ModernOscillator::mo_multitypes::momt_square};

                        for (int m : waves)
                        {
                            auto mtm =
                                addCallbackMenu(contextMenu, mo_multitype_names[m], [p, m, this]() {
                                    // p->deform_type = m;
                                    p->deform_type = (p->deform_type & 0xFFF0) | m;
                                    synth->refresh_editor = true;
                                });

                            mtm->setChecked((p->deform_type & 0x0F) == m);
                            eid++;
                        }

                        contextMenu->addSeparator();
                        eid++;

                        int subosc = ModernOscillator::mo_submask::mo_subone;

                        auto mtm = addCallbackMenu(
                            contextMenu, Surge::GUI::toOSCaseForMenu("Sub-oscillator Mode"),
                            [p, subosc, this]() {
                                auto usubosc = subosc;
                                int usubskipsync =
                                    p->deform_type & ModernOscillator::mo_submask::mo_subskipsync;

                                if (p->deform_type & subosc)
                                {
                                    usubosc = 0;
                                }

                                p->deform_type = (p->deform_type & 0xF) | usubosc | usubskipsync;

                                synth->refresh_editor = true;
                            });

                        mtm->setChecked((p->deform_type & subosc));
                        eid++;

                        int subskipsync = ModernOscillator::mo_submask::mo_subskipsync;

                        auto skp = addCallbackMenu(
                            contextMenu,
                            Surge::GUI::toOSCaseForMenu("Disable Hardsync in Sub-oscillator Mode"),
                            [p, subskipsync, this]() {
                                auto usubskipsync = subskipsync;
                                int usubosc =
                                    p->deform_type & ModernOscillator::mo_submask::mo_subone;

                                if (p->deform_type & subskipsync)
                                {
                                    usubskipsync = 0;
                                }

                                p->deform_type = (p->deform_type & 0xF) | usubosc | usubskipsync;

                                synth->refresh_editor = true;
                            });

                        skp->setChecked((p->deform_type & subskipsync));
                        eid++;

                        break;
                    }
                    case ct_alias_mask:
                    {
                        contextMenu->addSeparator();
                        eid++;

                        auto trimask =
                            addCallbackMenu(contextMenu,
                                            Surge::GUI::toOSCaseForMenu(
                                                "Triangle not masked after threshold point"),
                                            [p, this]() {
                                                p->deform_type = !p->deform_type;

                                                synth->refresh_editor = true;
                                            });

                        trimask->setChecked(p->deform_type);
                        eid++;

                        break;
                    }
                    default:
                    {
                        break;
                    }
                    }
                }

                if (p->can_extend_range())
                {
                    if (!(p->ctrltype == ct_fmratio && p->can_be_absolute() && p->absolute))
                    {
                        bool enable = true;
                        bool visible = true;
                        std::string txt = "Extend Range";
                        switch (p->ctrltype)
                        {
                        case ct_reson_res_extendable:
                            txt = "Modulation Extends into Self-oscillation";
                            break;
                        case ct_freq_audible_with_tunability:
                        case ct_freq_audible_with_very_low_lowerbound:
                            txt = "Apply SCL/KBM Tuning to Filter Cutoff";
                            visible =
                                synth->storage.tuningApplicationMode == SurgeStorage::RETUNE_ALL;
                            break;
                        case ct_percent_oscdrift:
                            txt = "Randomize Initial Drift Phase";
                            break;
                        case ct_twist_aux_mix:
                            txt = "Pan Main and Auxilliary Signals";
                            break;
                        case ct_countedset_percent_extendable:
                            txt = "Continuous Morph";
                            break;
                        default:
                            break;
                        }

                        if (visible)
                        {
                            auto ee = addCallbackMenu(contextMenu, Surge::GUI::toOSCaseForMenu(txt),
                                                      [this, p]() {
                                                          p->extend_range = !p->extend_range;
                                                          this->synth->refresh_editor = true;
                                                      });
                            ee->setChecked(p->extend_range);
                            ee->setEnabled(enable);
                            eid++;
                        }
                    }
                }

                if (p->can_be_absolute())
                {
                    auto abs = addCallbackMenu(contextMenu, "Absolute", [this, p]() {
                        p->absolute = !p->absolute;

                        // FIXME : What's a better aprpoach?
                        if (p->ctrltype == ct_fmratio)
                        {
                            char txt[256], ntxt[256];
                            memset(txt, 0, 256);
                            strxcpy(txt, p->get_name(), 256);
                            if (p->absolute)
                            {
                                snprintf(ntxt, 256, "M%c Frequency", txt[1]);
                            }
                            else
                            {
                                snprintf(ntxt, 256, "M%c Ratio",
                                         txt[1]); // Ladies and gentlemen, MC Ratio!
                            }
                            p->set_name(ntxt);
                            synth->refresh_editor = true;
                        }
                    });
                    abs->setChecked(p->absolute);
                    eid++;
                }

                if (p->can_deactivate() || p->get_primary_deactivation_driver())
                {
                    auto q = p->get_primary_deactivation_driver();
                    if (!q)
                    {
                        q = p;
                    }

                    std::string txt;

                    if (q->deactivated)
                    {
                        if (q->ctrltype == ct_envtime_linkable_delay)
                        {
                            txt = Surge::GUI::toOSCaseForMenu("Unlink from Left Channel");
                        }
                        else
                        {
                            txt = Surge::GUI::toOSCaseForMenu("Activate");
                        }

                        addCallbackMenu(contextMenu, txt.c_str(), [this, q]() {
                            q->deactivated = false;
                            this->synth->refresh_editor = true;
                        });
                    }
                    else
                    {
                        if (q->ctrltype == ct_envtime_linkable_delay)
                        {
                            txt = Surge::GUI::toOSCaseForMenu("Link to Left Channel");
                        }
                        else
                        {
                            txt = Surge::GUI::toOSCaseForMenu("Deactivate");
                        }

                        addCallbackMenu(contextMenu, txt.c_str(), [this, q]() {
                            q->deactivated = true;
                            this->synth->refresh_editor = true;
                        });
                    }

                    eid++;
                }

                contextMenu->addSeparator(eid++);

                // Construct submenus for explicit controller mapping
                COptionMenu *midiSub = new COptionMenu(
                    menuRect, 0, 0, 0, 0,
                    VSTGUI::COptionMenu::kNoDrawStyle | VSTGUI::COptionMenu::kMultipleCheckStyle);
                COptionMenu *currentSub = nullptr;

                bool isChecked = false;

                for (int subs = 0; subs < 7; ++subs)
                {
                    if (currentSub)
                    {
                        char name[16];
                        sprintf(name, "CC %d ... %d", (subs - 1) * 20,
                                min(((subs - 1) * 20) + 20, 128) - 1);
                        auto added_to_menu = midiSub->addEntry(currentSub, name);
                        added_to_menu->setChecked(isChecked);
                        currentSub->forget();
                        currentSub = nullptr;
                    }
                    currentSub = new COptionMenu(menuRect, 0, 0, 0, 0,
                                                 VSTGUI::COptionMenu::kNoDrawStyle |
                                                     VSTGUI::COptionMenu::kMultipleCheckStyle);
                    isChecked = false;

                    for (int item = 0; item < 20; ++item)
                    {
                        int mc = (subs * 20) + item;
                        int disabled = 0;

                        // these CCs cannot be used for MIDI learn (see
                        // SurgeSynthesizer::channelController)
                        if (mc == 0 || mc == 6 || mc == 32 || mc == 38 || mc == 64 ||
                            (mc == 74 && synth->mpeEnabled) || (mc >= 98 && mc <= 101) ||
                            mc == 120 || mc == 123)
                            disabled = 1;

                        // we don't have any more CCs to cover, so break the loop
                        if (mc > 127)
                            break;

                        char name[128];

                        sprintf(name, "CC %d (%s) %s", mc, midicc_names[mc],
                                (disabled == 1 ? "- RESERVED" : ""));

                        auto cmd = std::make_shared<CCommandMenuItem>(CCommandMenuItem::Desc(name));

                        cmd->setActions([this, p, ptag, mc](CCommandMenuItem *men) {
                            if (ptag < n_global_params)
                                p->midictrl = mc;
                            else
                            {
                                int a = ptag;
                                if (ptag >= (n_global_params + n_scene_params))
                                    a -= ptag;

                                synth->storage.getPatch().param_ptr[a]->midictrl = mc;
                                synth->storage.getPatch().param_ptr[a + n_scene_params]->midictrl =
                                    mc;
                            }
                        });

                        auto added = currentSub->addEntry(cmd);
                        cmd->setEnabled(!disabled);

                        if ((ptag < n_global_params && p->midictrl == mc) ||
                            (ptag > n_global_params &&
                             synth->storage.getPatch().param_ptr[ptag]->midictrl == mc))
                        {
                            added->setChecked();
                            isChecked = true;
                        }
                    }
                }

                if (currentSub)
                {
                    char name[16];
                    sprintf(name, "CC %d ... %d", 7 * 20, min((7 * 20) + 20, 128) - 1);
                    auto added_to_menu = midiSub->addEntry(currentSub, name);
                    added_to_menu->setChecked(isChecked);
                    currentSub->forget();
                    currentSub = nullptr;
                }

                contextMenu->addEntry(midiSub,
                                      Surge::GUI::toOSCaseForMenu("Assign Parameter To..."));
                midiSub->forget();
                eid++;

                if (synth->learn_param > -1 && synth->learn_param == p->id)
                    cancellearn = true;

                std::string learnTag =
                    cancellearn ? "Abort Parameter MIDI Learn" : "MIDI Learn Parameter...";
                addCallbackMenu(contextMenu, Surge::GUI::toOSCaseForMenu(learnTag),
                                [this, cancellearn, viewSize, p] {
                                    if (cancellearn)
                                    {
                                        hideMidiLearnOverlay();
                                        synth->learn_param = -1;
                                    }
                                    else
                                    {
                                        showMidiLearnOverlay(viewSize);
                                        synth->learn_param = p->id;
                                    }
                                });
                eid++;

                if (p->midictrl >= 0)
                {
                    char txt4[16];
                    decode_controllerid(txt4, p->midictrl);
                    sprintf(txt, "Clear Learned MIDI (%s ", txt4);
                    addCallbackMenu(
                        contextMenu,
                        Surge::GUI::toOSCaseForMenu(txt) + midicc_names[p->midictrl] + ")",
                        [this, p, ptag]() {
                            if (ptag < n_global_params)
                            {
                                p->midictrl = -1;
                            }
                            else
                            {
                                int a = ptag;
                                if (ptag >= (n_global_params + n_scene_params))
                                    a -= n_scene_params;

                                synth->storage.getPatch().param_ptr[a]->midictrl = -1;
                                synth->storage.getPatch().param_ptr[a + n_scene_params]->midictrl =
                                    -1;
                            }
                        });
                    eid++;
                }

                int n_ms = 0;

                for (int ms = 1; ms < n_modsources; ms++)
                    if (synth->isActiveModulation(ptag, (modsources)ms))
                        n_ms++;

                // see if we have any modulators that are unassigned, then create "Add Modulation
                // from..." menu
                if (n_ms != n_modsources)
                {
                    COptionMenu *addModSub =
                        new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle);

                    COptionMenu *addMacroSub =
                        new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle);
                    COptionMenu *addVLFOSub =
                        new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle);
                    COptionMenu *addSLFOSub =
                        new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle);
                    COptionMenu *addEGSub =
                        new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle);
                    COptionMenu *addMIDISub =
                        new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle);
                    COptionMenu *addMiscSub =
                        new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle);

                    for (int k = 1; k < n_modsources; k++)
                    {
                        modsources ms = (modsources)modsource_display_order[k];

                        if (!synth->isActiveModulation(ptag, ms) &&
                            synth->isValidModulation(ptag, ms))
                        {
                            char tmptxt[512];
                            sprintf(tmptxt, "%s", modulatorName(ms, false).c_str());

                            // TODO FIXME: Try not to be gross!
                            if (ms >= ms_ctrl1 && ms <= ms_ctrl1 + n_customcontrollers - 1)
                            {
                                addCallbackMenu(addMacroSub, tmptxt, [this, p, bvf, ms]() {
                                    this->promptForUserValueEntry(p, bvf, ms);
                                });
                            }
                            else if (ms >= ms_lfo1 && ms <= ms_lfo1 + n_lfos_voice - 1)
                            {
                                addCallbackMenu(addVLFOSub, tmptxt, [this, p, bvf, ms]() {
                                    this->promptForUserValueEntry(p, bvf, ms);
                                });
                            }
                            else if (ms >= ms_slfo1 && ms <= ms_slfo1 + n_lfos_scene - 1)
                            {
                                addCallbackMenu(addSLFOSub, tmptxt, [this, p, bvf, ms]() {
                                    this->promptForUserValueEntry(p, bvf, ms);
                                });
                            }
                            else if (ms >= ms_ampeg && ms <= ms_filtereg)
                            {
                                addCallbackMenu(addEGSub, tmptxt, [this, p, bvf, ms]() {
                                    this->promptForUserValueEntry(p, bvf, ms);
                                });
                            }
                            else if (ms >= ms_random_bipolar && ms <= ms_alternate_unipolar)
                            {
                                addCallbackMenu(addMiscSub, tmptxt, [this, p, bvf, ms]() {
                                    this->promptForUserValueEntry(p, bvf, ms);
                                });
                            }
                            else
                            {
                                addCallbackMenu(addMIDISub, tmptxt, [this, p, bvf, ms]() {
                                    this->promptForUserValueEntry(p, bvf, ms);
                                });
                            }
                        }
                    }

                    if (addMacroSub->getNbEntries())
                    {
                        addModSub->addEntry(addMacroSub, "Macros");
                    }
                    else
                    {
                        /* This is a pain. Not needed in VSTGUI classic but definitely needed here
                         */
                        addMacroSub->remember();
                    }
                    if (addVLFOSub->getNbEntries())
                    {
                        addModSub->addEntry(addVLFOSub, "Voice LFOs");
                    }
                    else
                    {
                        addVLFOSub->remember();
                    }
                    if (addSLFOSub->getNbEntries())
                    {
                        addModSub->addEntry(addSLFOSub, "Scene LFOs");
                    }
                    else
                    {
                        addSLFOSub->remember();
                    }
                    if (addEGSub->getNbEntries())
                    {
                        addModSub->addEntry(addEGSub, "Envelopes");
                    }
                    else
                    {
                        addEGSub->remember();
                    }
                    if (addMIDISub->getNbEntries())
                    {
                        addModSub->addEntry(addMIDISub, "MIDI");
                    }
                    else
                    {
                        addMIDISub->remember();
                    }
                    if (addMiscSub->getNbEntries())
                    {
                        addModSub->addEntry(addMiscSub, "Internal");
                    }
                    else
                    {
                        addMiscSub->remember();
                    }

                    if (addModSub->getNbEntries())
                    {
                        contextMenu->addSeparator(eid++);
                        contextMenu->addEntry(addModSub,
                                              Surge::GUI::toOSCaseForMenu("Add Modulation from"));
                    }
                    else
                    {
                        addModSub->remember();
                    }

                    if (addMacroSub)
                    {
                        addMacroSub->forget();
                        addMacroSub = nullptr;
                    }
                    if (addVLFOSub)
                    {
                        addVLFOSub->forget();
                        addVLFOSub = nullptr;
                    }
                    if (addSLFOSub)
                    {
                        addSLFOSub->forget();
                        addSLFOSub = nullptr;
                    }
                    if (addEGSub)
                    {
                        addEGSub->forget();
                        addEGSub = nullptr;
                    }
                    if (addMIDISub)
                    {
                        addMIDISub->forget();
                        addMIDISub = nullptr;
                    }
                    if (addMiscSub)
                    {
                        addMiscSub->forget();
                        addMiscSub = nullptr;
                    }

                    if (addModSub)
                    {
                        addModSub->forget();
                        addModSub = nullptr;
                    }

                    eid++;
                }

                addCallbackMenu(contextMenu, Surge::GUI::toOSCaseForMenu("Modulation Overview..."),
                                [this]() {
                                    if (!isAnyOverlayPresent(MODULATION_EDITOR))
                                        showModulationEditorDialog();
                                });

                if (n_ms)
                {
                    contextMenu->addSeparator(eid++);

                    for (int k = 1; k < n_modsources; k++)
                    {
                        modsources ms = (modsources)k;
                        if (synth->isActiveModulation(ptag, ms))
                        {
                            char modtxt[256];
                            p->get_display_of_modulation_depth(modtxt, synth->getModDepth(ptag, ms),
                                                               synth->isBipolarModulation(ms),
                                                               Parameter::Menu);

                            char tmptxt[512];
                            sprintf(tmptxt, "Edit %s -> %s: %s",
                                    (char *)modulatorName(ms, true).c_str(), p->get_full_name(),
                                    modtxt);
                            addCallbackMenu(contextMenu, tmptxt, [this, p, bvf, ms]() {
                                this->promptForUserValueEntry(p, bvf, ms);
                            });
                            eid++;
                        }
                    }
                    contextMenu->addSeparator(eid++);
                    for (int k = 1; k < n_modsources; k++)
                    {
                        modsources ms = (modsources)k;
                        if (synth->isActiveModulation(ptag, ms))
                        {
                            char tmptxt[256];
                            snprintf(tmptxt, 256, "Clear %s -> %s",
                                     (char *)modulatorName(ms, true).c_str(), p->get_full_name());
                            // clear_ms[ms] = eid;
                            // contextMenu->addEntry(tmptxt, eid++);
                            addCallbackMenu(contextMenu, tmptxt, [this, ms, ptag]() {
                                synth->clearModulation(ptag, (modsources)ms);
                                refresh_mod();
                                /*
                                ** FIXME - this is a pretty big hammer to deal with
                                ** #1477 - can we be more parsimonious?
                                */
                                synth->refresh_editor = true;
                            });
                            eid++;
                        }
                    }
                    if (n_ms > 1)
                    {
                        addCallbackMenu(contextMenu, Surge::GUI::toOSCaseForMenu("Clear All"),
                                        [this, ptag]() {
                                            for (int ms = 1; ms < n_modsources; ms++)
                                            {
                                                synth->clearModulation(ptag, (modsources)ms);
                                            }
                                            refresh_mod();
                                            synth->refresh_editor = true;
                                        });
                        eid++;
                    }
                }
            } // end vt_float if statement

            if (p->ctrltype == ct_amplitude_clipper)
            {
                std::string sc = std::string("Scene ") + (char)('A' + current_scene);
                contextMenu->addSeparator(eid++);
                // FIXME - add unified menu here
                auto hcmen = addCallbackMenu(
                    contextMenu, Surge::GUI::toOSCaseForMenu(sc + " Hard Clip Disabled"), [this]() {
                        synth->storage.sceneHardclipMode[current_scene] =
                            SurgeStorage::BYPASS_HARDCLIP;
                    });
                hcmen->setChecked(synth->storage.sceneHardclipMode[current_scene] ==
                                  SurgeStorage::BYPASS_HARDCLIP);
                eid++;

                hcmen = addCallbackMenu(contextMenu,
                                        Surge::GUI::toOSCaseForMenu(sc + " Hard Clip at 0 dBFS"),
                                        [this]() {
                                            synth->storage.sceneHardclipMode[current_scene] =
                                                SurgeStorage::HARDCLIP_TO_0DBFS;
                                        });
                hcmen->setChecked(synth->storage.sceneHardclipMode[current_scene] ==
                                  SurgeStorage::HARDCLIP_TO_0DBFS);
                eid++;

                hcmen = addCallbackMenu(contextMenu,
                                        Surge::GUI::toOSCaseForMenu(sc + " Hard Clip at +18 dBFS"),
                                        [this]() {
                                            synth->storage.sceneHardclipMode[current_scene] =
                                                SurgeStorage::HARDCLIP_TO_18DBFS;
                                        });
                hcmen->setChecked(synth->storage.sceneHardclipMode[current_scene] ==
                                  SurgeStorage::HARDCLIP_TO_18DBFS);
                eid++;
            }

            if (p->ctrltype == ct_decibel_attenuation_clipper)
            {
                contextMenu->addSeparator(eid++);
                // FIXME - add unified menu here

                auto hcmen = addCallbackMenu(
                    contextMenu, Surge::GUI::toOSCaseForMenu("Global Hard Clip Disabled"),
                    [this]() { synth->storage.hardclipMode = SurgeStorage::BYPASS_HARDCLIP; });
                hcmen->setChecked(synth->storage.hardclipMode == SurgeStorage::BYPASS_HARDCLIP);
                eid++;

                hcmen = addCallbackMenu(
                    contextMenu, Surge::GUI::toOSCaseForMenu("Global Hard Clip at 0 dBFS"),
                    [this]() { synth->storage.hardclipMode = SurgeStorage::HARDCLIP_TO_0DBFS; });
                hcmen->setChecked(synth->storage.hardclipMode == SurgeStorage::HARDCLIP_TO_0DBFS);
                eid++;

                hcmen = addCallbackMenu(
                    contextMenu, Surge::GUI::toOSCaseForMenu("Global Hard Clip at +18 dBFS"),
                    [this]() { synth->storage.hardclipMode = SurgeStorage::HARDCLIP_TO_18DBFS; });
                hcmen->setChecked(synth->storage.hardclipMode == SurgeStorage::HARDCLIP_TO_18DBFS);
                eid++;
            }

            frame->addView(contextMenu); // add to frame
            contextMenu->popup();
            frame->removeView(contextMenu, true); // remove from frame and forget

            return 1;
        }
        // reset to default value
        else if (button & kDoubleClick)
        {
            if (synth->isValidModulation(ptag, modsource) && mod_editor)
            {
                CModulationSourceButton *cms = (CModulationSourceButton *)gui_modsrc[modsource];
                auto thisms = modsource;
                if (cms && cms->hasAlternate && cms->useAlternate)
                    thisms = (modsources)cms->alternateId;

                synth->clearModulation(ptag, thisms);
                auto ctrms = dynamic_cast<Surge::Widgets::ModulatableControlInterface *>(control);
                jassert(ctrms);
                if (ctrms)
                {
                    ctrms->setModValue(synth->getModulation(p->id, thisms));
                    ctrms->setModulationState(synth->isModDestUsed(p->id),
                                              synth->isActiveModulation(p->id, thisms));
                    ctrms->setIsModulationBipolar(synth->isBipolarModulation(thisms));
                }
                oscdisplay->invalid();
                return 0;
            }
            else
            {
                switch (p->ctrltype)
                {
                case ct_lfotype:
                    /*
                    ** This code resets you to default if you double click on control
                    ** but on the lfoshape UI this is undesirable; it means if you accidentally
                    ** control click on step sequencer, say, you go back to sin and lose your
                    ** edits. So supress
                    */
                    break;
                case ct_freq_audible_with_tunability:
                case ct_freq_audible_with_very_low_lowerbound:
                {
                    if (p->extend_range || button & VSTGUI::CButton::kAlt)
                    {
                        auto kr = this->synth->storage.getPatch()
                                      .scene[current_scene]
                                      .keytrack_root.val.i;
                        p->set_value_f01(p->value_to_normalized(kr - 69));
                        control->setValue(p->get_value_f01());
                    }
                    else
                    {
                        p->set_value_f01(p->get_default_value_f01());
                        control->setValue(p->get_value_f01());
                    }
                    if (bvf)
                        bvf->invalid();
                    return 0;
                }
                default:
                {
                    p->set_value_f01(p->get_default_value_f01());
                    control->setValue(p->get_value_f01());
                    if (oscdisplay && (p->ctrlgroup == cg_OSC))
                        oscdisplay->invalid();
                    if (lfodisplay && (p->ctrlgroup == cg_LFO))
                        lfodisplay->invalid();
                    if (bvf)
                        bvf->invalid();
                    return 0;
                }
                }
            }
        }
        // exclusive mute/solo in the mixer
        else if (button & kControl)
        {
            if (p->ctrltype == ct_bool_mute)
            {
                Parameter *o1 = &(synth->storage.getPatch().scene[current_scene].mute_o1);
                Parameter *r23 = &(synth->storage.getPatch().scene[current_scene].mute_ring_23);

                auto curr = o1;

                while (curr <= r23)
                {
                    if (curr->id == p->id)
                        curr->val.b = true;
                    else
                        curr->val.b = false;

                    curr++;
                }

                synth->refresh_editor = true;
            }
            else if (p->ctrltype == ct_bool_solo)
            {
                Parameter *o1 = &(synth->storage.getPatch().scene[current_scene].solo_o1);
                Parameter *r23 = &(synth->storage.getPatch().scene[current_scene].solo_ring_23);

                auto curr = o1;

                while (curr <= r23)
                {
                    if (curr->id == p->id)
                        curr->val.b = true;
                    else
                        curr->val.b = false;

                    curr++;
                }

                synth->refresh_editor = true;
            }
            else
                p->bound_value();
        }
    }
    return 0;
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

void SurgeGUIEditor::valueChanged(CControlValueInterface *control)
{
    if (!frame)
    {
        return;
    }

    if (!editor_open)
    {
        return;
    }

    CRect viewSize;
    auto bvf = dynamic_cast<BaseViewFunctions *>(control);
    if (bvf)
    {
        viewSize = bvf->getControlViewSize();
    }
    else
    {
        jassert(false);
        std::cout << "WARNING: Control not castable to a BaseViewFunctions.\n"
                  << "Is your JUCE-only inheritance hierarchy missing the tag class?" << std::endl;
    }

    long tag = control->getTag();

    if (typeinDialog != nullptr && tag != tag_value_typein)
    {
        typeinDialog->setVisible(false);
        removeFromFrame.push_back(typeinDialog);
        typeinDialog = nullptr;
        typeinResetCounter = -1;
        typeinEditTarget = nullptr;
        typeinMode = Inactive;
    }

    if (tag == tag_status_zoom)
    {
        control->setValue(0);
        CPoint where;
        frame->getCurrentMouseLocation(where);
        frame->localToFrame(where);

        showZoomMenu(where);
        return;
    }

    if (tag == tag_lfo_menu)
    {
        control->setValue(0);
        CPoint where;
        frame->getCurrentMouseLocation(where);
        frame->localToFrame(where);

        showLfoMenu(where);
        return;
    }

    if (tag == tag_status_mpe)
    {
        toggleMPE();
        return;
    }
    if (tag == tag_status_tune)
    {
        toggleTuning();
        return;
    }

    if (tag == tag_mseg_edit)
    {
        if (control->getValue() > 0.5)
        {
            auto q = modsource_editor[current_scene];
            auto *lfodata = &(synth->storage.getPatch().scene[current_scene].lfo[q - ms_lfo1]);
            if (lfodata->shape.val.i == lt_mseg)
            {
                showMSEGEditor();
            }
            else if (lfodata->shape.val.i == lt_formula)
            {
                showFormulaEditorDialog();
            }
        }
        else
        {
            closeMSEGEditor();
            closeFormulaEditorDialog();
        }
        return;
    }

    if ((tag >= tag_mod_source0) && (tag < tag_mod_source_end))
    {
        if (((CModulationSourceButton *)control)->event_is_drag)
        {
            int t = (tag - tag_mod_source0);
            ((ControllerModulationSource *)synth->storage.getPatch()
                 .scene[current_scene]
                 .modsources[t])
                ->set_target01(control->getValue(), false);

            SurgeSynthesizer::ID tid;
            if (synth->fromSynthSideId(t + metaparam_offset - ms_ctrl1, tid))
            {
                synth->sendParameterAutomation(tid, control->getValue());
            }
            return;
        }
        else
        {
            CModulationSourceButton *cms = (CModulationSourceButton *)control;
            int state = cms->get_state();
            modsources newsource = (modsources)(tag - tag_mod_source0);
            long buttons = 0; // context->getMouseButtons(); // temp fix vstgui 3.5
            bool ciep =
                ((CModulationSourceButton *)control)->click_is_editpart && (newsource >= ms_lfo1);
            if (!ciep)
            {
                switch (state & 3)
                {
                case 0:
                    modsource = newsource;
                    if (mod_editor)
                        mod_editor = true;
                    else
                        mod_editor = false;
                    queue_refresh = true;
                    refresh_mod();
                    break;
                case 1:
                    modsource = newsource;
                    mod_editor = true;
                    refresh_mod();
                    break;
                case 2:
                    modsource = newsource;
                    mod_editor = false;
                    refresh_mod();
                    break;
                };
            }

            if (isLFO(newsource) && !(buttons & kShift))
            {
                if (modsource_editor[current_scene] != newsource)
                {
                    auto tabPosMem = Surge::Storage::getUserDefaultValue(
                        &(this->synth->storage), Surge::Storage::RememberTabPositionsPerScene, 0);

                    if (tabPosMem)
                        modsource_editor[current_scene] = newsource;
                    else
                    {
                        for (int i = 0; i < n_scenes; i++)
                        {
                            modsource_editor[i] = newsource;
                        }
                    }

                    if (isAnyOverlayPresent(MSEG_EDITOR))
                    {
                        auto ld = &(synth->storage.getPatch().scene[current_scene].lfo[newsource -
                                                                                       ms_lfo1]);
                        if (ld->shape.val.i == lt_mseg)
                        {
                            showMSEGEditor();
                        }
                        else
                        {
                            closeMSEGEditor();
                        }
                    }

                    queue_refresh = true;
                }
            }
        }

        return;
    }

    if ((tag == f1subtypetag) || (tag == f2subtypetag))
    {
        int idx = (tag == f2subtypetag) ? 1 : 0;
        auto csc = dynamic_cast<Surge::Widgets::Switch *>(control);
        int valdir = csc->getValueDirection();
        csc->clearValueDirection();

        int a =
            synth->storage.getPatch().scene[current_scene].filterunit[idx].subtype.val.i + valdir;
        int t = synth->storage.getPatch().scene[current_scene].filterunit[idx].type.val.i;
        int nn =
            fut_subcount[synth->storage.getPatch().scene[current_scene].filterunit[idx].type.val.i];
        if (a >= nn)
            a = 0;
        if (a < 0)
            a = nn - 1;
        synth->storage.getPatch().scene[current_scene].filterunit[idx].subtype.val.i = a;
        synth->storage.subtypeMemory[current_scene][idx][t] = a;
        if (csc)
        {
            if (nn == 0)
                csc->setIntegerValue(0);
            else
                csc->setIntegerValue(a + 1);
        }
        if (bvf)
            bvf->invalid();
        synth->switch_toggled_queued = true;
        return;
    }

    switch (tag)
    {
    case tag_scene_select:
    {
        current_scene = (int)(control->getValue() * 1.f) + 0.5f;
        synth->release_if_latched[synth->storage.getPatch().scene_active.val.i] = true;
        synth->storage.getPatch().scene_active.val.i = current_scene;
        // synth->storage.getPatch().param_ptr[scene_select_pid]->set_value_f01(control->getValue());

        if (isAnyOverlayPresent(MSEG_EDITOR))
        {
            auto ld = &(synth->storage.getPatch()
                            .scene[current_scene]
                            .lfo[modsource_editor[current_scene] - ms_lfo1]);
            if (ld->shape.val.i == lt_mseg)
            {
                showMSEGEditor();
            }
            else
            {
                closeMSEGEditor();
            }
        }

        queue_refresh = true;
        return;
    }
    break;
    case tag_patchname:
    {
        auto psc = dynamic_cast<Surge::Widgets::PatchSelector *>(control);
        if (psc)
        {
            int id = psc->enqueue_sel_id;
            enqueuePatchId = id;
            flushEnqueuedPatchId();
        }
        return;
    }
    break;
    case tag_mp_category:
    {
        if (isAnyOverlayPresent(STORE_PATCH))
        {
            closeStorePatchDialog();
        }

        if (control->getValue() > 0.5f)
            synth->incrementCategory(true);
        else
            synth->incrementCategory(false);
        return;
    }
    break;
    case tag_mp_patch:
    {
        if (isAnyOverlayPresent(STORE_PATCH))
        {
            closeStorePatchDialog();
        }

        auto insideCategory = Surge::Storage::getUserDefaultValue(
            &(this->synth->storage), Surge::Storage::PatchJogWraparound, 1);

        if (control->getValue() > 0.5f)
            synth->incrementPatch(true, insideCategory);
        else
            synth->incrementPatch(false, insideCategory);
        return;
    }
    break;
    case tag_mp_jogfx:
    {
        CFxMenu *fxm = dynamic_cast<CFxMenu *>(fxmenu);
        auto jog = [this, fxm](int byThis) {
            this->selectedFX[this->current_fx] =
                std::max(this->selectedFX[this->current_fx] + byThis, -1);
            if (!fxm->loadSnapshotByIndex(this->selectedFX[this->current_fx]))
            {
                // Try and go back to 0. This is the wrong behavior for negative jog
                this->selectedFX[this->current_fx] = 0;
                fxm->loadSnapshotByIndex(0);
            }
        };

        if (fxm)
        {
            if (fxm->selectedIdx >= 0 && fxm->selectedIdx != selectedFX[current_fx])
                selectedFX[current_fx] = fxm->selectedIdx;

            if (control->getValue() > 0.5f)
            {
                jog(+1);
            }
            else
            {
                jog(-1);
            }

            if (fxPresetLabel)
            {
                fxPresetLabel->setText(fxm->selectedName.c_str());
                fxPresetName[this->current_fx] = fxm->selectedName;
            }
        }
        else
        {
        }
        return;
    }
    break;
    case tag_settingsmenu:
    {
        CRect r = viewSize;
        CRect menuRect;
        CPoint where;
        frame->getCurrentMouseLocation(where);
        frame->localToFrame(where);

        menuRect.offset(where.x, where.y);

        // settings is a special case since it uses value here.
        if (auto msc = dynamic_cast<Surge::Widgets::MultiSwitch *>(control))
        {
            msc->isHovered = false;
            msc->repaint();
        }

        useDevMenu = false;
        showSettingsMenu(menuRect);
    }
    break;
    case tag_osc_select:
    {
        auto tabPosMem = Surge::Storage::getUserDefaultValue(
            &(this->synth->storage), Surge::Storage::RememberTabPositionsPerScene, 0);

        if (tabPosMem)
            current_osc[current_scene] = (int)(control->getValue() * 2.f + 0.5f);
        else
        {
            for (int i = 0; i < n_scenes; i++)
            {
                current_osc[i] = (int)(control->getValue() * 2.f + 0.5f);
            }
        }

        queue_refresh = true;
        return;
    }
    break;
    case tag_fx_select:
    {
        auto fxc = dynamic_cast<Surge::Widgets::EffectChooser *>(control);
        if (!fxc)
            return;

        int d = fxc->getDeactivatedBitmask();
        synth->fx_suspend_bitmask = synth->storage.getPatch().fx_disable.val.i ^ d;
        synth->storage.getPatch().fx_disable.val.i = d;
        fxc->setDeactivatedBitmask(d);

        int nfx = fxc->getCurrentEffect();
        if (current_fx != nfx)
        {
            current_fx = nfx;
            queue_refresh = true;
        }

        return;
    }
    break;
    case tag_osc_menu:
    {
        synth->switch_toggled_queued = true;
        queue_refresh = true;
        synth->processThreadunsafeOperations();
        return;
    }
    break;
    case tag_fx_menu:
    {
        synth->load_fx_needed = true;
        // queue_refresh = true;
        synth->fx_reload[current_fx & 7] = true;
        synth->processThreadunsafeOperations();

        CFxMenu *fxm = dynamic_cast<CFxMenu *>(fxmenu);
        if (fxm && fxm->selectedIdx >= 0)
        {
            selectedFX[current_fx] = fxm->selectedIdx;
            fxPresetName[current_fx] = fxm->selectedName;
        }
        else if (fxPresetLabel)
        {
            fxPresetLabel->setText(fxm->selectedName.c_str());
            fxPresetName[this->current_fx] = fxm->selectedName;
        }

        return;
    }
    break;
    case tag_store:
    {
        patchdata p;

        p.name = synth->storage.getPatch().name;
        p.category = synth->storage.getPatch().category;
        p.author = synth->storage.getPatch().author;
        p.comments = synth->storage.getPatch().comment;

        string defaultAuthor = Surge::Storage::getUserDefaultValue(
            &(this->synth->storage), Surge::Storage::DefaultPatchAuthor, "");
        string defaultComment = Surge::Storage::getUserDefaultValue(
            &(this->synth->storage), Surge::Storage::DefaultPatchComment, "");
        string oldAuthor = "";

        if (!Surge::Storage::isValidUTF8(defaultAuthor))
        {
            defaultAuthor = "";
        }
        if (!Surge::Storage::isValidUTF8(defaultComment))
        {
            defaultComment = "";
        }

        if (p.author == "" && defaultAuthor != "")
        {
            p.author = defaultAuthor;
        }

        if (p.author != "" && defaultAuthor != "")
        {
            if (_stricmp(p.author.c_str(), defaultAuthor.c_str()))
            {
                oldAuthor = p.author;
                p.author = defaultAuthor;
            }
        }

        if (p.comments == "" && defaultComment != "")
        {
            p.comments = defaultComment;
        }

        if (oldAuthor != "")
        {
            if (p.comments == "")
                p.comments += "Original patch by " + oldAuthor;
            else
                p.comments += " (Original patch by " + oldAuthor + ")";
        }

        showStorePatchDialog();

        patchName->setText(p.name.c_str());
        patchCategory->setText(p.category.c_str());
        patchCreator->setText(p.author.c_str());
        patchComment->setText(p.comments.c_str());
    }
    break;
    case tag_store_ok:
    {
        // prevent duplicate execution of savePatch() by detecting if the Store Patch dialog is
        // displayed or not
        // FIXME: baconpaul will know a better and more correct way to fix this
        if (isAnyOverlayPresent(STORE_PATCH))
        {
            synth->storage.getPatch().name = patchName->getText();
            synth->storage.getPatch().author = patchCreator->getText();
            synth->storage.getPatch().category = patchCategory->getText();
            synth->storage.getPatch().comment = patchComment->getText();

            synth->storage.getPatch().patchTuning.tuningStoredInPatch =
                patchTuning->getValue() > 0.5;
            if (synth->storage.getPatch().patchTuning.tuningStoredInPatch)
            {
                if (synth->storage.isStandardScale)
                {
                    synth->storage.getPatch().patchTuning.scaleContents = "";
                }
                else
                {
                    synth->storage.getPatch().patchTuning.scaleContents =
                        synth->storage.currentScale.rawText;
                }
                if (synth->storage.isStandardMapping)
                {
                    synth->storage.getPatch().patchTuning.mappingContents = "";
                }
                else
                {
                    synth->storage.getPatch().patchTuning.mappingContents =
                        synth->storage.currentMapping.rawText;
                    synth->storage.getPatch().patchTuning.mappingName =
                        synth->storage.currentMapping.name;
                }
            }

            // Ignore whatever comes from the DAW
            synth->storage.getPatch().dawExtraState.isPopulated = false;

            synth->savePatch();

            closeStorePatchDialog();
            frame->invalid();
        }
    }
    break;
    case tag_store_cancel:
    {
        closeStorePatchDialog();
        frame->invalid();
    }
    break;
    case tag_editor_overlay_close:
    {
        // So can I find an editor overlay parent
        auto *p = dynamic_cast<CView *>(control);
        if (!p)
        {
            // If you hit this it means you are closing a juce-overlay and somethign is wrong
            jassert(false);
        }
        auto tagToNuke = NO_EDITOR;
        while (p)
        {
            p = p->getParentView();
            for (auto el : editorOverlay)
            {
                if (el.second == p)
                {
                    tagToNuke = el.first;
                    p = nullptr;
                    break;
                }
            }
        }
        if (tagToNuke != NO_EDITOR)
        {
            dismissEditorOfType(tagToNuke);
        }
    }
    break;
    case tag_miniedit_ok:
    case tag_miniedit_cancel:
    {
        if (minieditOverlay != nullptr)
        {
            if (minieditTypein)
            {
                auto q = minieditTypein->getText().getString();
                if (tag == tag_miniedit_ok)
                {
                    minieditOverlayDone(q.c_str());
                }
                minieditTypein->looseFocus();
            }
            minieditOverlay->setVisible(false);
            removeFromFrame.push_back(minieditOverlay);
            minieditOverlay = nullptr;
        }
    }
    break;
    case tag_value_typein:
    {
        if (typeinDialog && typeinMode != Inactive)
        {
            std::string t = typeinValue->getText().getString();
            bool isInvalid = false;
            if (typeinMode == Param && typeinEditTarget && typeinModSource > 0)
            {
                bool valid = false;
                auto mv = typeinEditTarget->calculate_modulation_value_from_string(t, valid);

                if (!valid)
                {
                    isInvalid = true;
                }
                else
                {
                    synth->setModulation(typeinEditTarget->id, (modsources)typeinModSource, mv);
                    synth->refresh_editor = true;

                    typeinDialog->setVisible(false);
                    removeFromFrame.push_back(typeinDialog);
                    typeinDialog = nullptr;
                    typeinResetCounter = -1;
                    typeinEditTarget = nullptr;
                    typeinMode = Inactive;
                }
            }
            else if (typeinMode == Param && typeinEditTarget &&
                     typeinEditTarget->set_value_from_string(t))
            {
                repushAutomationFor(typeinEditTarget);
                isInvalid = false;
                synth->refresh_editor = true;
                typeinDialog->setVisible(false);
                removeFromFrame.push_back(typeinDialog);
                typeinDialog = nullptr;
                typeinResetCounter = -1;
                typeinEditTarget = nullptr;
                typeinMode = Inactive;
            }
            else if (typeinMode == Control)
            {
                auto cms = ((ControllerModulationSource *)synth->storage.getPatch()
                                .scene[current_scene]
                                .modsources[typeinModSource]);
                bool bp = cms->is_bipolar();
                float val = std::atof(t.c_str()) / 100.0;
                if ((bp && val >= -1 && val <= 1) || (val >= 0 && val <= 1))
                {
                    cms->output = val;
                    cms->target = val;
                    // This doesn't seem to work so hammer away
                    if (typeinEditControl)
                    {
                        typeinEditControl->invalid();
                    }
                    synth->refresh_editor = true;
                }
                else
                {
                    isInvalid = true;
                }
            }
            else
            {
                isInvalid = true;
            }

            if (isInvalid)
            {
                auto l = typeinLabel->getText().getString();
                promptForUserValueEntry(typeinEditTarget, typeinEditControl, typeinModSource);
                typeinResetCounter = 20;
                typeinResetLabel = l;
                typeinLabel->setText("Invalid Entry");
                typeinValue->setText(t.c_str());
                typeinLabel->setFontColor(currentSkin->getColor(Colors::Dialog::Label::Error));
            }
        }
    }
    break;
    default:
    {
        int ptag = tag - start_paramtags;

        if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
        {
            Parameter *p = synth->storage.getPatch().param_ptr[ptag];
            if (p->is_nonlocal_on_change())
                frame->invalid();

            char pname[256], pdisp[128], txt[128];
            bool modulate = false;

            // This allows us to turn on and off the editor. FIXME mseg check it
            if (p->ctrltype == ct_lfotype)
                synth->refresh_editor = true;

            if ((p->ctrlstyle & kNoPopup))
            {
                if (infowindow && infowindow->isVisible())
                {
                    infowindow->setVisible(false);
                }
            }

            auto mci = dynamic_cast<Surge::Widgets::ModulatableControlInterface *>(control);
            if (modsource && mod_editor && synth->isValidModulation(p->id, modsource) && mci)
            {
                modsources thisms = modsource;
                if (gui_modsrc[modsource])
                {
                    CModulationSourceButton *cms = (CModulationSourceButton *)gui_modsrc[modsource];
                    if (cms->hasAlternate && cms->useAlternate)
                        thisms = (modsources)cms->alternateId;
                }
                bool quantize_mod = frame->getCurrentMouseButtons() & kControl;
                float mv = mci->getModValue();
                if (quantize_mod)
                {
                    mv = p->quantize_modulation(mv);
                    // maybe setModValue here
                }

                synth->setModulation(ptag, thisms, mv);
                mci->setModulationState(synth->isModDestUsed(p->id),
                                        synth->isActiveModulation(p->id, thisms));
                mci->setIsModulationBipolar(synth->isBipolarModulation(thisms));

                SurgeSynthesizer::ID ptagid;
                if (synth->fromSynthSideId(ptag, ptagid))
                    synth->getParameterName(ptagid, txt);
                sprintf(pname, "%s -> %s", modulatorName(thisms, true).c_str(), txt);
                ModulationDisplayInfoWindowStrings mss;
                p->get_display_of_modulation_depth(pdisp, synth->getModDepth(ptag, thisms),
                                                   synth->isBipolarModulation(thisms),
                                                   Parameter::InfoWindow, &mss);
                if (mss.val != "")
                {
                    infowindow->setLabels(pname, pdisp);
                    infowindow->setMDIWS(mss);
                }
                else
                {
                    infowindow->setLabels(pname, pdisp);
                    infowindow->clearMDIWS();
                }
                modulate = true;

                if (isCustomController(modsource))
                {
                    int ccid = modsource - ms_ctrl1;
                    char *lbl = synth->storage.getPatch().CustomControllerLabel[ccid];

                    if ((lbl[0] == '-') && !lbl[1])
                    {
                        strxcpy(lbl, p->get_name(), 15);
                        synth->storage.getPatch().CustomControllerLabel[ccid][15] = 0;
                        ((CModulationSourceButton *)gui_modsrc[modsource])->setlabel(lbl);
                        ((CModulationSourceButton *)gui_modsrc[modsource])->invalid();
                    }
                }
            }
            else
            {
                auto val = control->getValue();
                if (p->ctrltype == ct_scenemode)
                {

                    /*
                    ** See the comment in the constructor of ct_scenemode above
                    */
                    auto cs2 = dynamic_cast<Surge::Widgets::MultiSwitch *>(control);
                    auto im = 0;
                    if (cs2)
                    {
                        im = cs2->getIntegerValue();
                        if (im == 3)
                            im = 2;
                        else if (im == 2)
                            im = 3;
                        val = Parameter::intScaledToFloat(im, n_scene_modes - 1);
                    }

                    /*
                    ** Now I also need to toggle the split key state
                    */
                    auto nf = dynamic_cast<CNumberField *>(splitpointControl);
                    if (nf)
                    {
                        int cm = nf->getControlMode();
                        if (im == sm_chsplit && cm != cm_midichannel_from_127)
                        {
                            nf->setControlMode(cm_midichannel_from_127);
                            nf->invalid();
                        }
                        else if (im == sm_split && cm != cm_notename)
                        {
                            nf->setControlMode(cm_notename);
                            nf->invalid();
                        }
                        else if ((im == sm_single || im == sm_dual) && cm != cm_none)
                        {
                            nf->setControlMode(cm_none);
                            nf->invalid();
                        }
                    }
                }

                bool force_integer = frame->getCurrentMouseButtons() & kControl;
                SurgeSynthesizer::ID ptagid;
                synth->fromSynthSideId(ptag, ptagid);
                if (synth->setParameter01(ptagid, val, false, force_integer))
                {
                    queue_refresh = true;
                    return;
                }
                else
                {
                    synth->sendParameterAutomation(ptagid, synth->getParameter01(ptagid));

                    auto mci = dynamic_cast<Surge::Widgets::ModulatableControlInterface *>(control);
                    if (mci)
                        mci->setQuantitizedDisplayValue(p->get_value_f01());
                    else if (bvf)
                        bvf->invalid();
                    synth->getParameterName(ptagid, pname);
                    synth->getParameterDisplay(ptagid, pdisp);
                    char pdispalt[256];
                    synth->getParameterDisplayAlt(ptagid, pdispalt);
                    infowindow->setLabels("", pdisp, pdispalt);
                    infowindow->clearMDIWS();
                    if (p->ctrltype == ct_polymode)
                        modulate = true;
                }

                if (p->ctrltype == ct_bool_unipolar || p->ctrltype == ct_lfotype)
                {
                    // The green line might change so...
                    refresh_mod();
                }
            }
            if (!queue_refresh)
            {
                if (!(p->ctrlstyle & kNoPopup))
                {
                    draw_infowindow(ptag, bvf, modulate);
                }

                if (oscdisplay && ((p->ctrlgroup == cg_OSC) || (p->ctrltype == ct_character)))
                {
                    oscdisplay->invalid();
                }
                if (lfodisplay && (p->ctrlgroup == cg_LFO))
                {
                    lfodisplay->invalid();
                }
                for (auto el : editorOverlay)
                {
                    el.second->invalid();
                }
            }
            if (p->ctrltype == ct_filtertype)
            {
                auto *subsw =
                    dynamic_cast<Surge::Widgets::Switch *>(filtersubtype[p->ctrlgroup_entry]);
                if (subsw)
                {
                    int sc = fut_subcount[p->val.i];

                    subsw->setIntegerMax(sc);
                }
            }
        }

        break;
    }
    }

    if ((tag == (f1subtypetag - 1)) || (tag == (f2subtypetag - 1)))
    {
        int idx = (tag == (f2subtypetag - 1)) ? 1 : 0;

        int a = synth->storage.getPatch().scene[current_scene].filterunit[idx].subtype.val.i;
        int nn =
            fut_subcount[synth->storage.getPatch().scene[current_scene].filterunit[idx].type.val.i];
        if (a >= nn)
            a = 0;
        synth->storage.getPatch().scene[current_scene].filterunit[idx].subtype.val.i = a;
        if (!nn)
            ((Surge::Widgets::Switch *)filtersubtype[idx])->setIntegerValue(0);
        else
            ((Surge::Widgets::Switch *)filtersubtype[idx])->setIntegerValue(a + 1);

        filtersubtype[idx]->invalid();
    }

    if (tag == fmconfig_tag)
    {
        // FM depth control
        int i = synth->storage.getPatch().scene[current_scene].fm_depth.id;
        param[i]->setDeactivated(
            (synth->storage.getPatch().scene[current_scene].fm_switch.val.i == fm_off));

        param[i]->asJuceComponent()->repaint();
    }

    if (tag == filterblock_tag)
    {
        // pan2 control
        int i = synth->storage.getPatch().scene[current_scene].width.id;
        if (param[i])
            param[i]->setDeactivated(
                (synth->storage.getPatch().scene[current_scene].filterblock_configuration.val.i !=
                 fc_stereo) &&
                (synth->storage.getPatch().scene[current_scene].filterblock_configuration.val.i !=
                 fc_wide));

        param[i]->asJuceComponent()->repaint();

        // feedback control
        i = synth->storage.getPatch().scene[current_scene].feedback.id;
        if (param[i])
            param[i]->setDeactivated(
                (synth->storage.getPatch().scene[current_scene].filterblock_configuration.val.i ==
                 fc_serial1));

        param[i]->asJuceComponent()->repaint();
    }

    if (tag == fxbypass_tag) // still do the normal operation, that's why it's outside the
                             // switch-statement
    {
        if (effectChooser)
        {
            effectChooser->setBypass(synth->storage.getPatch().fx_bypass.val.i);
            effectChooser->repaint();
        }

        switch (synth->storage.getPatch().fx_bypass.val.i)
        {
        case fxb_no_fx:
            synth->fx_suspend_bitmask = synth->fx_suspend_bitmask | 0xff;
            break;
        case fxb_scene_fx_only:
            synth->fx_suspend_bitmask = synth->fx_suspend_bitmask | 0xf0;
            break;
        case fxb_no_sends:
            synth->fx_suspend_bitmask = synth->fx_suspend_bitmask | 0x30;
            break;
        case fxb_all_fx:
        default:
            break;
        }
    }
}

//------------------------------------------------------------------------------------------------

void SurgeGUIEditor::beginEdit(long tag)
{
    if (tag < start_paramtags)
    {
        return;
    }

    int synthidx = tag - start_paramtags;
    SurgeSynthesizer::ID did;

    if (synth->fromSynthSideId(synthidx, did))
    {
        super::beginEdit(did.getDawSideId());
    }
}

//------------------------------------------------------------------------------------------------

void SurgeGUIEditor::endEdit(long tag)
{
    if (tag < start_paramtags)
    {
        return;
    }

    int synthidx = tag - start_paramtags;
    SurgeSynthesizer::ID did;

    if (synth->fromSynthSideId(synthidx, did))
    {
        super::endEdit(did.getDawSideId());
    }
}

//------------------------------------------------------------------------------------------------

void SurgeGUIEditor::controlBeginEdit(VSTGUI::CControlValueInterface *control)
{
    long tag = control->getTag();
    int ptag = tag - start_paramtags;
    if (ptag >= 0 && ptag < synth->storage.getPatch().param_ptr.size())
    {
        juceEditor->beginParameterEdit(synth->storage.getPatch().param_ptr[ptag]);
    }
}

//------------------------------------------------------------------------------------------------

void SurgeGUIEditor::controlEndEdit(VSTGUI::CControlValueInterface *control)
{
    long tag = control->getTag();
    int ptag = tag - start_paramtags;
    if (ptag >= 0 && ptag < synth->storage.getPatch().param_ptr.size())
    {
        juceEditor->endParameterEdit(synth->storage.getPatch().param_ptr[ptag]);
    }

    if (infowindow->isVisible())
    {
        auto cs = dynamic_cast<Surge::Widgets::ModulatableControlInterface *>(control);
        if (!cs || cs->getEditTypeWas() == Surge::Widgets::ModulatableControlInterface::WHEEL ||
            cs->getEditTypeWas() == Surge::Widgets::ModulatableControlInterface::DOUBLECLICK)
        {
            clear_infoview_countdown = 15;
        }
        else
        {
            infowindow->setVisible(false);
        }
    }
}

//------------------------------------------------------------------------------------------------

void SurgeGUIEditor::draw_infowindow(int ptag, BaseViewFunctions *control, bool modulate,
                                     bool forceMB)
{
    long buttons = 1; // context?context->getMouseButtons():1;

    if (buttons && forceMB)
        return; // don't draw via CC if MB is down

    auto modValues = Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                                                         Surge::Storage::ModWindowShowsValues, 0);

    infowindow->setExtendedMDIWS(modValues);

    if (buttons || forceMB)
    {
        // make sure an infowindow doesn't appear twice
        if (infowindow->isVisible())
        {
            infowindow->setVisible(false);
        }

        infowindow->setBoundsToAccompany(control->getControlViewSize().asJuceIntRect(),
                                         getFrame()->juceComponent()->getLocalBounds());
        infowindow->setVisible(true);
        infowindow->repaint();
        // on Linux the infoview closes too soon
#if LINUX
        clear_infoview_countdown = 100;
#else
        clear_infoview_countdown = 40;
#endif
    }
    else
    {
        if (infowindow->isVisible())
        {
            auto b = infowindow->getBounds();
            infowindow->setVisible(false);
            frame->juceComponent()->repaint(b);
        }
    }
}

long SurgeGUIEditor::applyParameterOffset(long id) { return id - start_paramtags; }

long SurgeGUIEditor::unapplyParameterOffset(long id) { return id + start_paramtags; }

// Status Panel Callbacks
void SurgeGUIEditor::toggleMPE()
{
    this->synth->mpeEnabled = !this->synth->mpeEnabled;
    if (statusMPE)
    {
        statusMPE->setValue(this->synth->mpeEnabled ? 1 : 0);
        statusMPE->asBaseViewFunctions()->invalid();
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
        parentEd->setSize(currentSkin->getWindowSizeX(), currentSkin->getWindowSizeY() + yExtra);
    }
    parentEd->setScaleFactor(zoomFactor * 0.01);
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

    auto mpeSubMenu = makeMpeMenu(menuRect, false);
    settingsMenu.addSubMenu(Surge::GUI::toOSCaseForMenu("MPE Options"), mpeSubMenu);

    auto tuningSubMenu = makeTuningMenu(menuRect, false);
    settingsMenu.addSubMenu("Tuning", tuningSubMenu);

    auto zoomMenu = makeZoomMenu(menuRect, false);
    settingsMenu.addSubMenu("Zoom", zoomMenu);

    auto skinSubMenu = makeSkinMenu(menuRect);
    settingsMenu.addSubMenu("Skins", skinSubMenu);

    auto uiOptionsMenu = makeUserSettingsMenu(menuRect);
    settingsMenu.addSubMenu(Surge::GUI::toOSCaseForMenu("User Settings"), uiOptionsMenu);

    auto dataSubMenu = makeDataMenu(menuRect);
    settingsMenu.addSubMenu(Surge::GUI::toOSCaseForMenu("Data Folders"), dataSubMenu);

    auto midiSubMenu = makeMidiMenu(menuRect);
    settingsMenu.addSubMenu(Surge::GUI::toOSCaseForMenu("MIDI Settings"), midiSubMenu);

    if (useDevMenu)
    {
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

VSTGUI::COptionMenu *SurgeGUIEditor::makeMonoModeOptionsMenu(VSTGUI::CRect &menuRect,
                                                             bool updateDefaults)
{
    COptionMenu *monoSubMenu = new COptionMenu(menuRect, 0, 0, 0, 0,
                                               VSTGUI::COptionMenu::kNoDrawStyle |
                                                   VSTGUI::COptionMenu::kMultipleCheckStyle);

    auto mode = synth->storage.monoPedalMode;
    if (updateDefaults)
        mode = (MonoPedalMode)Surge::Storage::getUserDefaultValue(
            &(this->synth->storage), Surge::Storage::MonoPedalMode, (int)HOLD_ALL_NOTES);

    auto cb = addCallbackMenu(
        monoSubMenu,
        Surge::GUI::toOSCaseForMenu("Sustain Pedal Holds All Notes (No Note Off Retrigger)"),
        [this, updateDefaults]() {
            this->synth->storage.monoPedalMode = HOLD_ALL_NOTES;
            if (updateDefaults)
                Surge::Storage::updateUserDefaultValue(
                    &(this->synth->storage), Surge::Storage::MonoPedalMode, (int)HOLD_ALL_NOTES);
        });
    if (mode == HOLD_ALL_NOTES)
        cb->setChecked(true);

    cb = addCallbackMenu(monoSubMenu,
                         Surge::GUI::toOSCaseForMenu("Sustain Pedal Allows Note Off Retrigger"),
                         [this, updateDefaults]() {
                             this->synth->storage.monoPedalMode = RELEASE_IF_OTHERS_HELD;
                             if (updateDefaults)
                                 Surge::Storage::updateUserDefaultValue(
                                     &(this->synth->storage), Surge::Storage::MonoPedalMode,
                                     (int)RELEASE_IF_OTHERS_HELD);
                         });
    if (mode == RELEASE_IF_OTHERS_HELD)
        cb->setChecked(true);

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

juce::PopupMenu SurgeGUIEditor::makeUserSettingsMenu(VSTGUI::CRect &menuRect)
{
    auto uiOptionsMenu = juce::PopupMenu();

#if WINDOWS
#define SUPPORTS_TOUCH_MENU 1
#else
#define SUPPORTS_TOUCH_MENU 0
#endif

#if SUPPORTS_TOUCH_MENU
    bool touchMode = Surge::Storage::getUserDefaultValue(&(synth->storage),
                                                         Surge::Storage::TouchMouseMode, false);
#else
    bool touchMode = false;
#endif

    // Mouse behavior submenu
    auto mouseSubMenu = juce::PopupMenu();

    std::string mouseLegacy = "Legacy";
    std::string mouseSlow = "Slow";
    std::string mouseMedium = "Medium";
    std::string mouseExact = "Exact";

    bool checked = Surge::Widgets::ModulatableSlider::sliderMoveRateState ==
                   Surge::Widgets::ModulatableSlider::kLegacy;
    bool enabled = !touchMode;

    mouseSubMenu.addItem(mouseLegacy, enabled, checked, [this]() {
        Surge::Widgets::ModulatableSlider::sliderMoveRateState =
            Surge::Widgets::ModulatableSlider::kLegacy;
        Surge::Storage::updateUserDefaultValue(
            &(this->synth->storage), Surge::Storage::SliderMoveRateState,
            Surge::Widgets::ModulatableSlider::sliderMoveRateState);
    });

    checked = Surge::Widgets::ModulatableSlider::sliderMoveRateState ==
              Surge::Widgets::ModulatableSlider::kSlow;

    mouseSubMenu.addItem(mouseSlow, enabled, checked, [this]() {
        Surge::Widgets::ModulatableSlider::sliderMoveRateState =
            Surge::Widgets::ModulatableSlider::kSlow;
        Surge::Storage::updateUserDefaultValue(
            &(this->synth->storage), Surge::Storage::SliderMoveRateState,
            Surge::Widgets::ModulatableSlider::sliderMoveRateState);
    });

    checked = Surge::Widgets::ModulatableSlider::sliderMoveRateState ==
              Surge::Widgets::ModulatableSlider::kMedium;

    mouseSubMenu.addItem(mouseMedium, enabled, checked, [this]() {
        Surge::Widgets::ModulatableSlider::sliderMoveRateState =
            Surge::Widgets::ModulatableSlider::kMedium;
        Surge::Storage::updateUserDefaultValue(
            &(this->synth->storage), Surge::Storage::SliderMoveRateState,
            Surge::Widgets::ModulatableSlider::sliderMoveRateState);
    });

    checked = Surge::Widgets::ModulatableSlider::sliderMoveRateState ==
              Surge::Widgets::ModulatableSlider::kExact;

    mouseSubMenu.addItem(mouseExact, enabled, checked, [this]() {
        Surge::Widgets::ModulatableSlider::sliderMoveRateState =
            Surge::Widgets::ModulatableSlider::kExact;
        Surge::Storage::updateUserDefaultValue(
            &(this->synth->storage), Surge::Storage::SliderMoveRateState,
            Surge::Widgets::ModulatableSlider::sliderMoveRateState);
    });

    mouseSubMenu.addSeparator();

    bool tsMode = Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                                                      Surge::Storage::ShowCursorWhileEditing, true);

    mouseSubMenu.addItem(Surge::GUI::toOSCaseForMenu("Show Cursor While Editing"), enabled, tsMode,
                         [this, tsMode]() {
                             Surge::Storage::updateUserDefaultValue(
                                 &(this->synth->storage), Surge::Storage::ShowCursorWhileEditing,
                                 !tsMode);
                         });

#if SUPPORTS_TOUCH_MENU
    mouseSubMenu.addSeparator();

    mouseSubMenu.addItem(
        Surge::GUI::toOSCaseForMenu("Touchscreen Mode"), true, touchMode, [this, touchMode]() {
            Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                   Surge::Storage::TouchMouseMode, !touchMode);
        });
#endif

    uiOptionsMenu.addSubMenu(Surge::GUI::toOSCaseForMenu("Mouse Behavior"), mouseSubMenu);

    // patch defaults
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

    uiOptionsMenu.addSubMenu(Surge::GUI::toOSCaseForMenu("Patch Defaults"), patchDefMenu);

    // value displays
    auto dispDefMenu = juce::PopupMenu();

    // high precision value readouts
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
                            this->frame->invalid();
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

    uiOptionsMenu.addSubMenu(Surge::GUI::toOSCaseForMenu("Value Displays"), dispDefMenu);

    // workflow menu
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

    uiOptionsMenu.addSubMenu(Surge::GUI::toOSCaseForMenu("Workflow"), wfMenu);

    return uiOptionsMenu;
}

bool SurgeGUIEditor::getShowVirtualKeyboard()
{
    auto key = Surge::Storage::ShowVirtualKeyboard_Plugin;
    bool defaultVal = false;
    if (parentEd->processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        key = Surge::Storage::ShowVirtualKeyboard_Standalone;
        defaultVal = false;
    }
    return Surge::Storage::getUserDefaultValue(&(this->synth->storage), key, defaultVal);
}

void SurgeGUIEditor::setShowVirtualKeyboard(bool b)
{
    auto key = Surge::Storage::ShowVirtualKeyboard_Plugin;
    if (parentEd->processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
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
        this->bitmapStore->setupBitmapsForFrame(frame);

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
        CFxMenu::scanForUserPresets =
            true; // that's annoying now I see it side by side. But you know.

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

    infowindow->setSkin(currentSkin, bitmapStore);

    auto bg = currentSkin->customBackgroundImage();

    if (bg != "")
    {
        CScalableBitmap *cbm = bitmapStore->getBitmapByStringID(bg);
        if (cbm)
        {
            cbm->setExtraScaleFactor(getZoomFactor());
            frame->setBackground(cbm);
        }
    }
    else
    {
        CScalableBitmap *cbm = bitmapStore->getBitmap(IDB_MAIN_BG);
        cbm->setExtraScaleFactor(getZoomFactor());
        frame->setBackground(cbm);
    }

    auto c = currentSkin->getColor(Colors::Dialog::Entry::Focus);
    frame->setFocusColor(c);

    wsx = currentSkin->getWindowSizeX();
    wsy = currentSkin->getWindowSizeY();

    float sf = 1;

    frame->setSize(wsx * sf, wsy * sf);

    rect.right = wsx * sf;
    rect.bottom = wsy * sf;

    setZoomFactor(getZoomFactor(), true);

    // update MSEG editor if opened
    if (isAnyOverlayPresent(MSEG_EDITOR))
    {
        showMSEGEditor();
    }

    // update Store Patch dialog if opened
    if (isAnyOverlayPresent(STORE_PATCH))
    {
        auto pname = patchName->getText();
        auto pcat = patchCategory->getText();
        auto pauth = patchCreator->getText();
        auto pcom = patchComment->getText();

        showStorePatchDialog();

        patchName->setText(pname);
        patchCategory->setText(pcat);
        patchCreator->setText(pauth);
        patchComment->setText(pcom);
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

std::shared_ptr<VSTGUI::CCommandMenuItem>
SurgeGUIEditor::addCallbackMenu(VSTGUI::COptionMenu *toThis, std::string label,
                                std::function<void()> op)
{
    auto menu = std::make_shared<CCommandMenuItem>(CCommandMenuItem::Desc(label.c_str()));
    menu->setActions([op](CCommandMenuItem *m) { op(); });
    toThis->addEntry(menu);
    return menu;
}

void SurgeGUIEditor::forceautomationchangefor(Parameter *p)
{
    std::cout << "FIXME - REMOVE THIS" << __func__ << std::endl;
}
//------------------------------------------------------------------------------------------------

void SurgeGUIEditor::promptForUserValueEntry(Parameter *p, BaseViewFunctions *c, int ms)
{
    jassert(c);

    if (typeinDialog)
    {
        typeinDialog->setVisible(false);
        removeFromFrame.push_back(typeinDialog);
        typeinDialog = nullptr;
        typeinResetCounter = -1;
    }

    if (p)
    {
        typeinMode = Param;
    }
    else
    {
        typeinMode = Control;
    }

    bool ismod = p && ms > 0;
    int boxht = 56;
    auto cp = c->getControlViewSize();

    if (ismod)
        boxht += 22;

    CRect typeinSize(cp.left, cp.top - boxht, cp.left + 120, cp.top - boxht + boxht);

    if (cp.top - boxht < 0)
    {
        typeinSize = CRect(cp.left, cp.top + c->getControlHeight(), cp.left + 120,
                           cp.top + c->getControlHeight() + boxht);
    }

    typeinModSource = ms;
    typeinEditControl = dynamic_cast<CControl *>(c);
    if (typeinEditControl == nullptr)
    {
        jassert(false);
    }
    typeinResetCounter = -1;

    typeinDialog = new CViewContainer(typeinSize);
    typeinDialog->setBackgroundColor(currentSkin->getColor(Colors::Dialog::Border));
    typeinDialog->setVisible(false);
    frame->addView(typeinDialog);

    CRect innerSize(0, 0, typeinSize.getWidth(), typeinSize.getHeight());
    innerSize.inset(1, 1);
    auto inner = new CViewContainer(innerSize);
    CColor bggr = currentSkin->getColor(Colors::Dialog::Background);
    inner->setBackgroundColor(bggr);
    typeinDialog->addView(inner);

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

    typeinLabel = new CTextLabel(CRect(2, 2, 114, 14), lab.c_str());
    typeinLabel->setFontColor(currentSkin->getColor(Colors::Slider::Label::Dark));
    typeinLabel->setTransparency(true);
    typeinLabel->setFont(Surge::GUI::getFontManager()->displayFont);
    inner->addView(typeinLabel);

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

    if (ismod)
    {
        std::string mls = std::string("by ") + (char *)modulatorName(ms, true).c_str();
        auto ml = new CTextLabel(CRect(2, 10, 114, 27), mls.c_str());
        ml->setFontColor(currentSkin->getColor(Colors::Slider::Label::Dark));
        ml->setTransparency(true);
        ml->setFont(Surge::GUI::getFontManager()->displayFont);
        inner->addView(ml);
    }

    typeinPriorValueLabel = new CTextLabel(CRect(2, 29 - (ismod ? 0 : 23), 116, 36 + ismod), ptext);
    typeinPriorValueLabel->setFontColor(currentSkin->getColor(Colors::Slider::Label::Dark));
    typeinPriorValueLabel->setTransparency(true);
    typeinPriorValueLabel->setFont(Surge::GUI::getFontManager()->displayFont);
    inner->addView(typeinPriorValueLabel);

    if (ismod)
    {
        auto sl = new CTextLabel(CRect(2, 29 + 9, 116, 36 + 13), ptext2);
        sl->setFontColor(currentSkin->getColor(Colors::Slider::Label::Dark));
        sl->setTransparency(true);
        sl->setFont(Surge::GUI::getFontManager()->displayFont);
        inner->addView(sl);
    }

    typeinValue = new CTextEdit(CRect(4, 31 + (ismod ? 22 : 0), 114, 50 + (ismod ? 22 : 0)), this,
                                tag_value_typein, txt);
    typeinValue->setBackColor(currentSkin->getColor(Colors::Dialog::Entry::Background));
    typeinValue->setFontColor(currentSkin->getColor(Colors::Dialog::Entry::Text));

    // fix the text selection rectangle background overhanging the borders on Windows
#if WINDOWS
    typeinValue->setTextInset(CPoint(3, 0));
#endif

    if (p)
    {
        if (!p->can_setvalue_from_string())
        {
            typeinValue->setFontColor(juce::Colours::red);
            typeinValue->setText("Not available");
        }
    }

    inner->addView(typeinValue);
    typeinEditTarget = p;
    typeinDialog->setVisible(true);
    typeinValue->takeFocus();
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
    this->bitmapStore->setupBitmapsForFrame(frame);
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
            auto gms = dynamic_cast<CModulationSourceButton *>(gui_modsrc[k]);
            if (gms)
            {
                gms->setSecondaryHover(true);
            }
        }
    };
}
void SurgeGUIEditor::sliderHoverEnd(int tag)
{
    for (int k = 1; k < n_modsources; k++)
    {
        auto gms = dynamic_cast<CModulationSourceButton *>(gui_modsrc[k]);
        if (gms)
        {
            gms->setSecondaryHover(false);
        }
    }
}

void SurgeGUIEditor::dismissEditorOfType(OverlayTags ofType)
{
    if (editorOverlay.size() == 0)
        return;

    auto newO = editorOverlay;
    newO.clear();
    for (auto el : editorOverlay)
    {
        if (el.first == ofType)
        {
            el.second->setVisible(false);
            auto f = editorOverlayOnClose[el.second];
            f();

            editorOverlayOnClose.erase(el.second);
            editorOverlayContentsWeakReference.erase(el.second);

            removeFromFrame.push_back(el.second);
        }
        else
        {
            newO.push_back(el);
        }
    }
    editorOverlay = newO;
}

void SurgeGUIEditor::addEditorOverlay(VSTGUI::CView *c, std::string editorTitle,
                                      OverlayTags editorTag, const VSTGUI::CPoint &topLeft,
                                      bool modalOverlay, bool hasCloseButton,
                                      std::function<void()> onClose)
{
    dismissEditorOfType(editorTag);

    const int header = 18;
    const int buttonwidth = 18;

    if (!c)
        return;

    auto vs = c->getViewSize();

    // add a solid outline thing which is bigger than the control with the title to that
    auto containerSize = vs;
    containerSize.top -= header;

    if (!modalOverlay)
        containerSize.offset(-containerSize.left + topLeft.x, -containerSize.top + topLeft.y);

    auto fs = CRect(0, 0, getWindowSizeX(), getWindowSizeY());

    if (!modalOverlay)
        fs = containerSize;

    // add a screen size transparent thing into the editorOverlay
    auto editorOverlayC = new CViewContainer(fs);
    editorOverlayC->setBackgroundColor(currentSkin->getColor(Colors::Overlay::Background));
    editorOverlayC->setVisible(true);
    frame->addView(editorOverlayC);

    if (modalOverlay)
        containerSize = containerSize.centerInside(fs);
    else
        containerSize.moveTo(CPoint(0, 0));

    auto headerFont = Surge::GUI::getFontManager()->getLatoAtSize(9, kBoldFace);
    auto btnFont = Surge::GUI::getFontManager()->getLatoAtSize(8);

    auto outerc = new CViewContainer(containerSize);
    outerc->setBackgroundColor(currentSkin->getColor(Colors::Dialog::Border));
    editorOverlayC->addView(outerc);

    auto csz = containerSize;
    csz.bottom = csz.top + header;
    auto innerc = new CViewContainer(csz);
    innerc->setBackgroundColor(currentSkin->getColor(Colors::Dialog::Titlebar::Background));
    editorOverlayC->addView(innerc);

    auto tl = new CTextLabel(csz, editorTitle.c_str());
    tl->setBackColor(currentSkin->getColor(Colors::Dialog::Titlebar::Background));
    tl->setFrameColor(currentSkin->getColor(Colors::Dialog::Titlebar::Background));
    tl->setFontColor(currentSkin->getColor(Colors::Dialog::Titlebar::Text));
    tl->setFont(headerFont);
    innerc->addView(tl);

    auto iconrect = CRect(CPoint(3, 2), CPoint(15, 15));
    auto icon = new CViewContainer(iconrect);
    icon->setBackground(bitmapStore->getBitmap(IDB_SURGE_ICON));
    icon->setVisible(true);
    innerc->addView(icon);

    if (hasCloseButton)
    {
        auto btnbg = currentSkin->getColor(Colors::Dialog::Button::Background);
        auto btnborder = currentSkin->getColor(Colors::Dialog::Button::Border);
        auto btntext = currentSkin->getColor(Colors::Dialog::Button::Text);

        auto hovbtnbg = currentSkin->getColor(Colors::Dialog::Button::BackgroundHover);
        auto hovbtnborder = currentSkin->getColor(Colors::Dialog::Button::BorderHover);
        auto hovbtntext = currentSkin->getColor(Colors::Dialog::Button::TextHover);

        auto pressbtnbg = currentSkin->getColor(Colors::Dialog::Button::BackgroundPressed);
        auto pressbtnborder = currentSkin->getColor(Colors::Dialog::Button::BorderPressed);
        auto pressbtntext = currentSkin->getColor(Colors::Dialog::Button::TextPressed);

        VSTGUI::CGradient::ColorStopMap csm;
        VSTGUI::CGradient *cg = VSTGUI::CGradient::create(csm);
        cg->addColorStop(0, btnbg);

        VSTGUI::CGradient::ColorStopMap hovcsm;
        VSTGUI::CGradient *hovcg = VSTGUI::CGradient::create(hovcsm);
        hovcg->addColorStop(0, hovbtnbg);

        VSTGUI::CGradient::ColorStopMap presscsm;
        VSTGUI::CGradient *presscg = VSTGUI::CGradient::create(presscsm);
        presscg->addColorStop(0, pressbtnbg);

        csz.left = csz.right - buttonwidth;
        csz.inset(2, 3);
        csz.top--;
        csz.bottom += 1;
        auto b = new CTextButtonWithHover(csz, this, tag_editor_overlay_close, "X");
        b->setVisible(true);
        b->setFont(btnFont);
        b->setGradient(cg);
        b->setFrameColor(btnborder);
        b->setTextColor(btntext);
        b->setHoverGradient(hovcg);
        b->setHoverFrameColor(hovbtnborder);
        b->setHoverTextColor(hovbtntext);
        b->setGradientHighlighted(presscg);
        b->setHoverGradientHighlighted(presscg);
        b->setFrameColorHighlighted(pressbtnborder);
        b->setTextColorHighlighted(pressbtntext);
        b->setRoundRadius(CCoord(3.f));

        presscg->forget();
        hovcg->forget();
        cg->forget();

        innerc->addView(b);
    }

    // add the control inside that in an outline
    if (modalOverlay)
    {
        containerSize = vs;
        containerSize.top -= header;
        containerSize = containerSize.centerInside(fs);
        containerSize.top += header;
        containerSize.inset(3, 3);
    }
    else
    {
        containerSize = vs.moveTo(0, header).inset(2, 0);
        containerSize.bottom -= 2;
    }

    c->setViewSize(containerSize);
    c->setMouseableArea(containerSize); // sigh
    editorOverlayC->addView(c);

    // save the onClose function
    editorOverlay.push_back(std::make_pair(editorTag, editorOverlayC));
    editorOverlayOnClose[editorOverlayC] = onClose;
    editorOverlayContentsWeakReference[editorOverlayC] = c;
    editorOverlayContentsWeakReference[editorOverlayC] = c;
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
    auto fs = CRect(0, 0, getWindowSizeX(), getWindowSizeY());
    minieditOverlay = new CViewContainer(fs);
    minieditOverlay->setBackgroundColor(currentSkin->getColor(Colors::Overlay::Background));
    minieditOverlay->setVisible(true);
    frame->addView(minieditOverlay);

    auto where = iwhere;
    if (where.x < 0 || where.y < 0)
    {
        frame->getCurrentMouseLocation(where);
        frame->localToFrame(where);
    }

    int wd = 160;
    int ht = 80;

    // We want to center the text on where. The 0.4 just 'feels better' than 0.5
    where = where.offset(-wd * 0.4, -ht * 0.5);

    auto rr = CRect(CPoint(where.x - fs.left, where.y - fs.top), CPoint(wd, ht));

    if (rr.top < 0)
        rr = rr.offset(0, 10 - rr.top);
    if (rr.bottom > fs.getHeight())

        rr.offset(0, -rr.bottom + fs.getHeight() - 10);
    if (rr.left < 0)
        rr = rr.offset(10 - rr.left, 0);
    if (rr.right > fs.getWidth())

        rr.offset(-rr.right + fs.getWidth() - 10, 0);

    auto fnt = Surge::GUI::getFontManager()->getLatoAtSize(11);
    auto fnts = Surge::GUI::getFontManager()->getLatoAtSize(9);
    auto fntt = Surge::GUI::getFontManager()->getLatoAtSize(9, kBoldFace);

    auto window = new CViewContainer(rr);
    window->setBackgroundColor(currentSkin->getColor(Colors::Dialog::Border));
    window->setVisible(true);
    minieditOverlay->addView(window);

    auto titlebar = new CViewContainer(CRect(0, 0, wd, 18));
    titlebar->setBackgroundColor(currentSkin->getColor(Colors::Dialog::Titlebar::Background));
    titlebar->setVisible(true);
    window->addView(titlebar);

    auto titlerect = CRect(CPoint(0, 0), CPoint(wd, 16));
    auto titletxt = new CTextLabel(titlerect, title.c_str());
    titletxt->setTransparency(true);
    titletxt->setFontColor(currentSkin->getColor(Colors::Dialog::Titlebar::Text));
    titletxt->setFont(fntt);
    titlebar->addView(titletxt);

    auto iconrect = CRect(CPoint(3, 2), CPoint(15, 15));
    auto icon = new CViewContainer(iconrect);
    icon->setBackground(bitmapStore->getBitmap(IDB_SURGE_ICON));
    icon->setVisible(true);
    titlebar->addView(icon);

    auto bgrect = CRect(CPoint(1, 18), CPoint(wd - 2, ht - 19));
    auto bg = new CViewContainer(bgrect);
    bg->setBackgroundColor(currentSkin->getColor(Colors::Dialog::Background));
    bg->setVisible(true);
    window->addView(bg);

    auto msgrect = CRect(CPoint(0, 2), CPoint(wd, 14));
    msgrect.inset(5, 0);
    auto msgtxt = new CTextLabel(msgrect, prompt.c_str());
    msgtxt->setTransparency(true);
    msgtxt->setFontColor(currentSkin->getColor(Colors::Dialog::Label::Text));
    msgtxt->setFont(fnts);
    msgtxt->setHoriAlign(kLeftText);
    bg->addView(msgtxt);

    auto mer = CRect(CPoint(0, 18), CPoint(wd - 2, 22));
    mer.inset(4, 2);
    minieditTypein = new CTextEdit(mer, this, tag_miniedit_ok, value.c_str());
    minieditTypein->setBackColor(currentSkin->getColor(Colors::Dialog::Entry::Background));
    minieditTypein->setFrameColor(currentSkin->getColor(Colors::Dialog::Entry::Border));
    minieditTypein->setFontColor(currentSkin->getColor(Colors::Dialog::Entry::Text));
    minieditTypein->setFont(fnt);
#if WINDOWS
    minieditTypein->setTextInset(CPoint(3, 0));
#endif
    bg->addView(minieditTypein);
    minieditTypein->takeFocus();

    minieditOverlayDone = onOK;

    int bw = 44;
    auto b1r = CRect(CPoint(wd - (bw * 2) - 9, bgrect.bottom - 36), CPoint(bw, 13));
    auto b2r = CRect(CPoint(wd - bw - 6, bgrect.bottom - 36), CPoint(bw, 13));

    auto btnbg = currentSkin->getColor(Colors::Dialog::Button::Background);
    auto btnborder = currentSkin->getColor(Colors::Dialog::Button::Border);
    auto btntext = currentSkin->getColor(Colors::Dialog::Button::Text);

    auto hovbtnbg = currentSkin->getColor(Colors::Dialog::Button::BackgroundHover);
    auto hovbtnborder = currentSkin->getColor(Colors::Dialog::Button::BorderHover);
    auto hovbtntext = currentSkin->getColor(Colors::Dialog::Button::TextHover);

    auto pressbtnbg = currentSkin->getColor(Colors::Dialog::Button::BackgroundPressed);
    auto pressbtnborder = currentSkin->getColor(Colors::Dialog::Button::BorderPressed);
    auto pressbtntext = currentSkin->getColor(Colors::Dialog::Button::TextPressed);

    VSTGUI::CGradient::ColorStopMap csm;
    VSTGUI::CGradient *cg = VSTGUI::CGradient::create(csm);
    cg->addColorStop(0, btnbg);

    VSTGUI::CGradient::ColorStopMap hovcsm;
    VSTGUI::CGradient *hovcg = VSTGUI::CGradient::create(hovcsm);
    hovcg->addColorStop(0, hovbtnbg);

    VSTGUI::CGradient::ColorStopMap presscsm;
    VSTGUI::CGradient *presscg = VSTGUI::CGradient::create(presscsm);
    presscg->addColorStop(0, pressbtnbg);

    auto cb = new CTextButtonWithHover(b1r, this, tag_miniedit_cancel, "Cancel");
    cb->setVisible(true);
    cb->setFont(fnts);
    cb->setGradient(cg);
    cb->setFrameColor(btnborder);
    cb->setTextColor(btntext);
    cb->setHoverGradient(hovcg);
    cb->setHoverFrameColor(hovbtnborder);
    cb->setHoverTextColor(hovbtntext);
    cb->setGradientHighlighted(presscg);
    cb->setFrameColorHighlighted(pressbtnborder);
    cb->setTextColorHighlighted(pressbtntext);
    cb->setRoundRadius(CCoord(3.f));
    bg->addView(cb);

    auto kb = new CTextButtonWithHover(b2r, this, tag_miniedit_ok, "OK");
    kb->setVisible(true);
    kb->setFont(fnts);
    kb->setGradient(cg);
    kb->setFrameColor(btnborder);
    kb->setTextColor(btntext);
    kb->setHoverGradient(hovcg);
    kb->setHoverFrameColor(hovbtnborder);
    kb->setHoverTextColor(hovbtntext);
    kb->setGradientHighlighted(presscg);
    kb->setFrameColorHighlighted(pressbtnborder);
    kb->setTextColorHighlighted(pressbtntext);
    kb->setRoundRadius(CCoord(3.f));
    bg->addView(kb);

    presscg->forget();
    hovcg->forget();
    cg->forget();
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
        promptForUserValueEntry(p, sl->asControlValueInterface()->asBaseViewFunctions(), ms);
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

    // TODO: add skin connectors for all Store Patch dialog widgets
    // let's have fixed dialog size for now, once TODO is done use the commented out line instead
    CRect dialogSize(CPoint(0, 0), CPoint(390, 143));
    // CRect dialogSize(CPoint(0, 0), CPoint(skinCtrl->w, skinCtrl->h));

    auto saveDialog = new CViewContainer(dialogSize);
    saveDialog->setBackgroundColor(currentSkin->getColor(Colors::Dialog::Background));

    auto btnbg = currentSkin->getColor(Colors::Dialog::Button::Background);
    auto btnborder = currentSkin->getColor(Colors::Dialog::Button::Border);
    auto btntext = currentSkin->getColor(Colors::Dialog::Button::Text);

    auto hovbtnbg = currentSkin->getColor(Colors::Dialog::Button::BackgroundHover);
    auto hovbtnborder = currentSkin->getColor(Colors::Dialog::Button::BorderHover);
    auto hovbtntext = currentSkin->getColor(Colors::Dialog::Button::TextHover);

    auto pressbtnbg = currentSkin->getColor(Colors::Dialog::Button::BackgroundPressed);
    auto pressbtnborder = currentSkin->getColor(Colors::Dialog::Button::BorderPressed);
    auto pressbtntext = currentSkin->getColor(Colors::Dialog::Button::TextPressed);

    VSTGUI::CGradient::ColorStopMap csm;
    VSTGUI::CGradient *cg = VSTGUI::CGradient::create(csm);
    cg->addColorStop(0, btnbg);

    VSTGUI::CGradient::ColorStopMap hovcsm;
    VSTGUI::CGradient *hovcg = VSTGUI::CGradient::create(hovcsm);
    hovcg->addColorStop(0, hovbtnbg);

    VSTGUI::CGradient::ColorStopMap presscsm;
    VSTGUI::CGradient *presscg = VSTGUI::CGradient::create(presscsm);
    presscg->addColorStop(0, pressbtnbg);

    auto fnt = Surge::GUI::getFontManager()->getLatoAtSize(11);

    auto label = CRect(CPoint(10, 10), CPoint(47, 19));
    auto pnamelbl = new CTextLabel(label, "Name");
    pnamelbl->setTransparency(true);
    pnamelbl->setFont(fnt);
    pnamelbl->setFontColor(currentSkin->getColor(Colors::Dialog::Label::Text));
    pnamelbl->setHoriAlign(kRightText);

    label.offset(0, 24);
    auto pcatlbl = new CTextLabel(label, "Category");
    pcatlbl->setTransparency(true);
    pcatlbl->setFont(fnt);
    pcatlbl->setFontColor(currentSkin->getColor(Colors::Dialog::Label::Text));
    pcatlbl->setHoriAlign(kRightText);

    label.offset(0, 24);
    auto pauthlbl = new CTextLabel(label, "Author");
    pauthlbl->setTransparency(true);
    pauthlbl->setFont(fnt);
    pauthlbl->setFontColor(currentSkin->getColor(Colors::Dialog::Label::Text));
    pauthlbl->setHoriAlign(kRightText);

    label.offset(0, 24);
    auto pcomlbl = new CTextLabel(label, "Comment");
    pcomlbl->setTransparency(true);
    pcomlbl->setFont(fnt);
    pcomlbl->setFontColor(currentSkin->getColor(Colors::Dialog::Label::Text));
    pcomlbl->setHoriAlign(kRightText);

    patchName = new CTextEdit(CRect(CPoint(67, 10), CPoint(309, 19)), this, tag_store_name);
    patchCategory = new CTextEdit(CRect(CPoint(67, 34), CPoint(309, 19)), this, tag_store_category);
    patchCreator = new CTextEdit(CRect(CPoint(67, 58), CPoint(309, 19)), this, tag_store_creator);
    patchComment = new CTextEdit(CRect(CPoint(67, 82), CPoint(309, 19)), this, tag_store_comments);
    patchTuning = new CCheckBox(CRect(CPoint(67, 111), CPoint(200, 20)), this, tag_store_tuning,
                                "Save With Tuning");
    patchTuning->setFont(fnt);
    patchTuning->setFontColor(currentSkin->getColor(Colors::Dialog::Label::Text));
    patchTuning->setBoxFrameColor(currentSkin->getColor(Colors::Dialog::Checkbox::Border));
    patchTuning->setBoxFillColor(currentSkin->getColor(Colors::Dialog::Checkbox::Background));
    patchTuning->setCheckMarkColor(currentSkin->getColor(Colors::Dialog::Checkbox::Tick));
    patchTuning->sizeToFit();
    patchTuning->setValue(0);
    patchTuning->setMouseEnabled(true);
    patchTuning->setVisible(!synth->storage.isStandardTuning);

    // fix the text selection rectangle background overhanging the borders on Windows
#if WINDOWS
    patchName->setTextInset(CPoint(3, 0));
    patchCategory->setTextInset(CPoint(3, 0));
    patchCreator->setTextInset(CPoint(3, 0));
    patchComment->setTextInset(CPoint(3, 0));
#endif

    /*
     * There is, apparently, a bug in VSTGui that focus events don't fire reliably on some mac
     * hosts. This leads to the odd behaviour when you click out of a box that in some hosts - Logic
     * Pro for instance - there is no looseFocus event and so the value doesn't update. We could fix
     * that a variety of ways I imagine, but since we don't really mind the value being updated as
     * we go, we can just set the editors to immediate and correct the problem.
     *
     * See GitHub Issue #231 for an explanation of the behaviour without these changes as of Jan
     * 2019.
     */
    patchName->setImmediateTextChange(true);
    patchCategory->setImmediateTextChange(true);
    patchCreator->setImmediateTextChange(true);
    patchComment->setImmediateTextChange(true);

    patchName->setBackColor(currentSkin->getColor(Colors::Dialog::Entry::Background));
    patchCategory->setBackColor(currentSkin->getColor(Colors::Dialog::Entry::Background));
    patchCreator->setBackColor(currentSkin->getColor(Colors::Dialog::Entry::Background));
    patchComment->setBackColor(currentSkin->getColor(Colors::Dialog::Entry::Background));

    patchName->setFontColor(currentSkin->getColor(Colors::Dialog::Entry::Text));
    patchCategory->setFontColor(currentSkin->getColor(Colors::Dialog::Entry::Text));
    patchCreator->setFontColor(currentSkin->getColor(Colors::Dialog::Entry::Text));
    patchComment->setFontColor(currentSkin->getColor(Colors::Dialog::Entry::Text));

    patchName->setFrameColor(currentSkin->getColor(Colors::Dialog::Entry::Border));
    patchCategory->setFrameColor(currentSkin->getColor(Colors::Dialog::Entry::Border));
    patchCreator->setFrameColor(currentSkin->getColor(Colors::Dialog::Entry::Border));
    patchComment->setFrameColor(currentSkin->getColor(Colors::Dialog::Entry::Border));

    auto b1r = CRect(CPoint(266, 111), CPoint(50, 20));
    auto cb = new CTextButtonWithHover(b1r, this, tag_store_cancel, "Cancel");
    cb->setFont(Surge::GUI::getFontManager()->aboutFont);
    cb->setGradient(cg);
    cb->setFrameColor(btnborder);
    cb->setTextColor(btntext);
    cb->setHoverGradient(hovcg);
    cb->setHoverFrameColor(hovbtnborder);
    cb->setHoverTextColor(hovbtntext);
    cb->setGradientHighlighted(presscg);
    cb->setFrameColorHighlighted(pressbtnborder);
    cb->setTextColorHighlighted(pressbtntext);
    cb->setRoundRadius(CCoord(3.f));

    auto b2r = CRect(CPoint(326, 111), CPoint(50, 20));
    auto kb = new CTextButtonWithHover(b2r, this, tag_store_ok, "OK");
    kb->setFont(Surge::GUI::getFontManager()->aboutFont);
    kb->setGradient(cg);
    kb->setFrameColor(btnborder);
    kb->setTextColor(btntext);
    kb->setHoverGradient(hovcg);
    kb->setHoverFrameColor(hovbtnborder);
    kb->setHoverTextColor(hovbtntext);
    kb->setGradientHighlighted(presscg);
    kb->setFrameColorHighlighted(pressbtnborder);
    kb->setTextColorHighlighted(pressbtntext);
    kb->setRoundRadius(CCoord(3.f));

    cg->forget();
    presscg->forget();
    hovcg->forget();

    saveDialog->addView(pnamelbl);
    saveDialog->addView(pcatlbl);
    saveDialog->addView(pauthlbl);
    saveDialog->addView(pcomlbl);
    saveDialog->addView(patchName);
    saveDialog->addView(patchCategory);
    saveDialog->addView(patchCreator);
    saveDialog->addView(patchComment);
    saveDialog->addView(patchTuning);
    saveDialog->addView(patchTuningLabel);
    saveDialog->addView(cb);
    saveDialog->addView(kb);

    addEditorOverlay(saveDialog, "Store Patch", STORE_PATCH, CPoint(skinCtrl->x, skinCtrl->y),
                     false, false, [this]() {});
}

VSTGUI::CControlValueInterface *
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
            frame->juceComponent()->addAndMakeVisible(*hs);
            juceSkinComponents[skinCtrl->sessionid] = std::move(hs);

            return dynamic_cast<CControlValueInterface *>(
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
        frame->juceComponent()->addAndMakeVisible(*hs);
        juceSkinComponents[skinCtrl->sessionid] = std::move(hs);

        return dynamic_cast<CControlValueInterface *>(
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

            frame->juceComponent()->addAndMakeVisible(*hsw);

            juceSkinComponents[skinCtrl->sessionid] = std::move(hsw);

            if (paramIndex >= 0)
                nonmod_param[paramIndex] = dynamic_cast<CControlValueInterface *>(
                    juceSkinComponents[skinCtrl->sessionid].get());

            return dynamic_cast<CControlValueInterface *>(
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
            frame->juceComponent()->addAndMakeVisible(*hsw);

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

            return dynamic_cast<CControlValueInterface *>(
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
            CLFOGui *slfo = new CLFOGui(
                rect, lfo_id >= 0 && lfo_id <= (ms_lfo6 - ms_lfo1), this, p->id + start_paramtags,
                &synth->storage.getPatch().scene[current_scene].lfo[lfo_id], &synth->storage,
                &synth->storage.getPatch().stepsequences[current_scene][lfo_id],
                &synth->storage.getPatch().msegs[current_scene][lfo_id],
                &synth->storage.getPatch().formulamods[current_scene][lfo_id], bitmapStore);
            slfo->setSkin(currentSkin, bitmapStore, skinCtrl);
            lfodisplay = slfo;
            frame->addView(slfo);
            nonmod_param[paramIndex] = slfo;
            return slfo;
        }
    }

    if (skinCtrl->ultimateparentclassname == "COSCMenu")
    {
        CRect rect(0, 0, skinCtrl->w, skinCtrl->h);
        rect.offset(skinCtrl->x, skinCtrl->y);
        auto hsw = new COscMenu(
            rect, this, tag_osc_menu, &synth->storage,
            &synth->storage.getPatch().scene[current_scene].osc[current_osc[current_scene]],
            bitmapStore);

        hsw->setSkin(currentSkin, bitmapStore);
        hsw->setMouseableArea(rect);

        hsw->text_allcaps = Surge::GUI::Skin::setAllCapsProperty(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_ALL_CAPS, "false"));
        hsw->font_style = Surge::GUI::Skin::setFontStyleProperty(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::FONT_STYLE, "normal"));
        hsw->text_align = Surge::GUI::Skin::setTextAlignProperty(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_ALIGN, "center"));
        hsw->font_size = std::atoi(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::FONT_SIZE, "8").c_str());
        hsw->text_hoffset = std::atoi(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_HOFFSET, "0")
                .c_str());
        hsw->text_voffset = std::atoi(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_VOFFSET, "0")
                .c_str());

        if (p)
            hsw->setValue(p->get_value_f01());
        // TODO: This was not on before skinnification. Why?
        // if( paramIndex >= 0 ) nonmod_param[paramIndex] = hsw;

        frame->addView(hsw);
        return hsw;
    }
    if (skinCtrl->ultimateparentclassname == "CFXMenu")
    {
        CRect rect(0, 0, skinCtrl->w, skinCtrl->h);
        rect.offset(skinCtrl->x, skinCtrl->y);
        // CControl *m = new
        // CFxMenu(rect,this,tag_fx_menu,&synth->storage,&synth->storage.getPatch().fx[current_fx],current_fx);
        CControl *m = new CFxMenu(rect, this, tag_fx_menu, &synth->storage,
                                  &synth->storage.getPatch().fx[current_fx],
                                  &synth->fxsync[current_fx], current_fx);
        m->setMouseableArea(rect);
        ((CFxMenu *)m)->setSkin(currentSkin, bitmapStore);
        ((CFxMenu *)m)->selectedIdx = this->selectedFX[current_fx];
        fxPresetLabel->setText(this->fxPresetName[current_fx].c_str());
        m->setValue(p->get_value_f01());
        frame->addView(m);
        fxmenu = m;
        return m;
    }

    if (skinCtrl->ultimateparentclassname == "CNumberField")
    {
        CRect rect(0, 0, skinCtrl->w, skinCtrl->h);
        rect.offset(skinCtrl->x, skinCtrl->y);
        CNumberField *pbd = new CNumberField(rect, this, tag, nullptr, &(synth->storage));
        pbd->setSkin(currentSkin, bitmapStore, skinCtrl);
        pbd->setMouseableArea(rect);

        // TODO extra from properties
        auto nfcm = currentSkin->propertyValue(
            skinCtrl, Surge::Skin::Component::NUMBERFIELD_CONTROLMODE, std::to_string(cm_none));
        pbd->setControlMode(std::atoi(nfcm.c_str()));

        pbd->setValue(p->get_value_f01());
        frame->addView(pbd);
        nonmod_param[paramIndex] = pbd;

        if (p && p->ctrltype == ct_midikey_or_channel)
        {
            auto sm = this->synth->storage.getPatch().scenemode.val.i;

            switch (sm)
            {
            case sm_single:
            case sm_dual:
                pbd->setControlMode(cm_none);
                break;
            case sm_split:
                pbd->setControlMode(cm_notename);
                break;
            case sm_chsplit:
                pbd->setControlMode(cm_midichannel_from_127);
                break;
            }
        }

        // Save some of these for later reference
        switch (p->ctrltype)
        {
        case ct_polylimit:
            polydisp = pbd;
            break;
        case ct_midikey_or_channel:
            splitpointControl = pbd;
            break;
        default:
            break;
        }
        return pbd;
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

        frame->juceComponent()->addAndMakeVisible(*hsw);
        nonmod_param[paramIndex] = hsw.get();

        juceSkinComponents[skinCtrl->sessionid] = std::move(hsw);

        return dynamic_cast<CControlValueInterface *>(
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
    frame->invalid();
}

void SurgeGUIEditor::closeStorePatchDialog()
{
    dismissEditorOfType(STORE_PATCH);

    // Have to update all that state too for the newly orphaned items
    patchName = nullptr;
    patchCategory = nullptr;
    patchCreator = nullptr;
    patchComment = nullptr;
}

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
    auto *c = new CViewContainer(CRect(CPoint(0, 0), CPoint(750, 450)));
    auto pt = std::make_unique<PatchDBViewer>(this, &(this->synth->storage));
    c->juceComponent()->addAndMakeVisible(*pt);
    pt->setBounds(0, 0, 750, 450);
    c->takeOwnership(std::move(pt));
    addEditorOverlay(c, "Patch Browser", PATCH_BROWSER, CPoint(50, 50), false, true, [this]() {});
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
    auto *c = new CViewContainer(CRect(CPoint(0, 0), CPoint(750, 450)));
    auto pt = std::make_unique<ModulationEditor>(this, this->synth);
    c->juceComponent()->addAndMakeVisible(*pt);
    pt->setBounds(0, 0, 750, 450);
    c->takeOwnership(std::move(pt));
    addEditorOverlay(c, "Modulation Overview", MODULATION_EDITOR, CPoint(50, 50), false, true,
                     [this]() {});
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

    auto *c = new CViewContainer(CRect(CPoint(0, 0), CPoint(skinCtrl->w, skinCtrl->h)));
    auto lfo_id = modsource_editor[current_scene] - ms_lfo1;
    auto fs = &synth->storage.getPatch().formulamods[current_scene][lfo_id];

    auto pt =
        std::make_unique<FormulaModulatorEditor>(this, &(this->synth->storage), fs, currentSkin);
    pt->setBounds(0, 0, skinCtrl->w, skinCtrl->h);
    pt->setSkin(currentSkin, bitmapStore);
    c->juceComponent()->addAndMakeVisible(*pt);
    c->takeOwnership(std::move(pt));

    addEditorOverlay(c, "Formula Editor", FORMULA_EDITOR, CPoint(skinCtrl->x, skinCtrl->y), false,
                     true, [this]() {});
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
    auto *c = new CViewContainer(CRect(CPoint(0, 0), CPoint(w, h)));
    auto os = &synth->storage.getPatch().scene[current_scene].osc[current_osc[current_scene]];

    auto pt =
        std::make_unique<WavetableEquationEditor>(this, &(this->synth->storage), os, currentSkin);
    pt->setBounds(0, 0, w, h);
    pt->setSkin(currentSkin, bitmapStore);
    c->juceComponent()->addAndMakeVisible(*pt);
    c->takeOwnership(std::move(pt));

    addEditorOverlay(c, "Wavetable Editor", FORMULA_EDITOR, CPoint(px, py), false, true,
                     [this]() {});
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
    auto mse = new MSEGEditor(&(synth->storage), lfodata, ms, &msegEditState[current_scene][lfo_id],
                              currentSkin, bitmapStore);
    auto vs = mse->getViewSize().getWidth();
    float xp = (currentSkin->getWindowSizeX() - (vs + 8)) * 0.5;

    std::string title = modsource_names[modsource_editor[current_scene]];
    title += " Editor";
    Surge::Storage::findReplaceSubstring(title, std::string("LFO"), std::string("MSEG"));

    auto npc = Surge::Skin::Connector::NonParameterConnection::MSEG_EDITOR_WINDOW;
    auto conn = Surge::Skin::Connector::connectorByNonParameterConnection(npc);
    auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);

    addEditorOverlay(mse, title, MSEG_EDITOR, CPoint(skinCtrl->x, skinCtrl->y), false, true,
                     [this]() {
                         if (msegEditSwitch)
                         {
                             msegEditSwitch->setValue(0.0);
                             msegEditSwitch->asBaseViewFunctions()->invalid();
                         }
                     });

    if (msegEditSwitch)
    {
        msegEditSwitch->setValue(1.0);
        msegEditSwitch->asBaseViewFunctions()->invalid();
    }
}

void SurgeGUIEditor::repushAutomationFor(Parameter *p)
{
    auto id = synth->idForParameter(p);
    synth->sendParameterAutomation(id, synth->getParameter01(id));
}

void SurgeGUIEditor::showAboutScreen(int devModeGrid)
{
    aboutScreen = std::make_unique<Surge::Widgets::AboutScreen>();
    aboutScreen->setEditor(this);
    aboutScreen->setHostProgram(synth->hostProgram);
    aboutScreen->setWrapperType(synth->juceWrapperType);
    aboutScreen->setStorage(&(this->synth->storage));
    aboutScreen->setSkin(currentSkin, bitmapStore);

    aboutScreen->populateData();

    aboutScreen->setBounds(frame->juceComponent()->getLocalBounds());
    frame->juceComponent()->addAndMakeVisible(*aboutScreen);

    /*
    CRect wsize(0, 0, getWindowSizeX(), getWindowSizeY());
    aboutbox = new CAboutBox(wsize, this, &(synth->storage), synth->hostProgram,
                             synth->juceWrapperType, currentSkin, bitmapStore, devModeGrid);
    aboutbox->setVisible(true);
    getFrame()->addView(aboutbox);
     */
}

void SurgeGUIEditor::hideAboutScreen()
{
    frame->juceComponent()->removeChildComponent(aboutScreen.get());
}

void SurgeGUIEditor::showMidiLearnOverlay(const VSTGUI::CRect &r)
{
    midiLearnOverlay = bitmapStore->getDrawable(IDB_MIDI_LEARN)->createCopy();
    midiLearnOverlay->setInterceptsMouseClicks(false, false);
    midiLearnOverlay->setBounds(r.asJuceIntRect());
    getFrame()->juceComponent()->addAndMakeVisible(midiLearnOverlay.get());
}

void SurgeGUIEditor::hideMidiLearnOverlay()
{
    if (midiLearnOverlay)
    {
        getFrame()->juceComponent()->removeChildComponent(midiLearnOverlay.get());
    }
}

void SurgeGUIEditor::toggleAlternateFor(VSTGUI::CControl *c)
{
    auto cms = dynamic_cast<CModulationSourceButton *>(c);
    if (cms)
    {
        modsource = (modsources)(cms->getTag() - tag_mod_source0);
        cms->setUseAlternate(!cms->useAlternate);
        modsource_is_alternate[modsource] = cms->useAlternate;
        this->refresh_mod();
    }
}
void SurgeGUIEditor::onSurgeError(const string &msg, const string &title)
{
    std::lock_guard<std::mutex> g(errorItemsMutex);
    errorItems.emplace_back(msg, title);
    errorItemCount++;
}
