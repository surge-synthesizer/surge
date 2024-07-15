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

#include "SurgeGUIEditor.h"
#include "SurgeGUIEditorTags.h"
#include "SurgeGUIUtils.h"

#include "SurgeSynthEditor.h"

#include "fmt/core.h"

#include "ModernOscillator.h"
#include "StringOscillator.h"

#include "widgets/EffectChooser.h"
#include "widgets/LFOAndStepDisplay.h"
#include "widgets/MainFrame.h"
#include "widgets/MultiSwitch.h"
#include "widgets/MenuForDiscreteParams.h"
#include "widgets/ModulationSourceButton.h"
#include "widgets/MenuCustomComponents.h"
#include "widgets/NumberField.h"
#include "widgets/OscillatorWaveformDisplay.h"
#include "widgets/Switch.h"
#include "widgets/PatchSelector.h"
#include "widgets/ParameterInfowindow.h"
#include "widgets/WaveShaperSelector.h"
#include "widgets/XMLConfiguredMenus.h"

#include "overlays/ModulationEditor.h"
#include "overlays/PatchStoreDialog.h"
#include "overlays/TypeinParamEditor.h"

std::string decodeControllerID(int id)
{
    int type = (id & 0xffff0000) >> 16;
    int num = (id & 0xffff);
    std::string out;

    switch (type)
    {
    case 1:
        out = fmt::format("NRPN {:d}", num);
        break;
    case 2:
        out = fmt::format("RPN {:d}", num);
        break;
    default:
        out = fmt::format("CC {:d}", num);
        break;
    };

    return out;
}

void SurgeGUIEditor::addEnvTrigOptions(juce::PopupMenu &contextMenu, int current_scene)
{
    std::vector<std::string> labels = {"Reset to Zero", "Continue from Current Level"};
    std::vector<MonoVoiceEnvelopeMode> vals = {RESTART_FROM_ZERO, RESTART_FROM_LATEST};

    Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(contextMenu,
                                                                    "ENVELOPE RETRIGGER BEHAVIOR");

    for (int i = 0; i < 2; ++i)
    {
        auto value = vals[i];
        bool isChecked =
            (value == synth->storage.getPatch().scene[current_scene].monoVoiceEnvelopeMode);

        contextMenu.addItem(
            Surge::GUI::toOSCase(labels[i]), true, isChecked,
            [this, current_scene, value, isChecked]() {
                undoManager()->pushPatch();

                synth->storage.getPatch().scene[current_scene].monoVoiceEnvelopeMode = value;

                if (!isChecked)
                {
                    synth->storage.getPatch().isDirty = true;
                }
            });
    }
}

void SurgeGUIEditor::createMIDILearnMenuEntries(juce::PopupMenu &parentMenu,
                                                const LearnMode learn_mode, const int idx,
                                                Surge::GUI::IComponentTagValue *control)
{
    long tag;
    int ptag;
    Parameter *p;

    if (learn_mode != macro_cc)
    {
        tag = control->getTag();
        ptag = tag - start_paramtags;
        p = synth->storage.getPatch().param_ptr[ptag];
    }

    if (learn_mode != param_note)
    {
        // construct submenus for explicit controller mapping
        auto midiSub = juce::PopupMenu();

        // which MIDI channel to use for MIDI learn via menu
        int learnChan = (learn_mode == macro_cc) ? synth->storage.controllers_chan[idx]
                        : (ptag < n_global_params)
                            ? p->midichan
                            : synth->storage.getPatch().param_ptr[ptag]->midichan;

        // -1 means Omni but it's also the default value, so just double-check
        // if the user setting matches or if it's different, then use that
        if (learnChan == -1)
        {
            learnChan = Surge::Storage::getUserDefaultValue(
                &(this->synth->storage), Surge::Storage::MenuBasedMIDILearnChannel, -1);
        }

        for (int subs = 0; subs < 7; ++subs)
        {
            auto currentSub = juce::PopupMenu();
            bool isSubChecked = false;

            for (int item = 0; item < 20; ++item)
            {
                int mc = (subs * 20) + item;
                bool isChecked = false;
                bool isEnabled = true;

                // these CCs cannot be used for MIDI learn (see SurgeSynthesizer::channelController)
                if (mc == 0 || mc == 6 || mc == 32 || mc == 38 || mc == 64 || mc == 74 ||
                    (mc >= 98 && mc <= 101) || mc == 120 || mc == 123)
                {
                    isEnabled = false;
                }

                // we don't have any more CCs to cover, so break the loop
                if (mc > 127)
                {
                    break;
                }

                std::string name = fmt::format("CC {:d} ({:s}) {:s}", mc, midicc_names[mc],
                                               (!isEnabled ? "- RESERVED" : ""));

                switch (learn_mode)
                {
                case macro_cc:
                {
                    if (synth->storage.controllers[idx] == mc)
                    {
                        isChecked = true;
                        isSubChecked = true;
                    }

                    currentSub.addItem(name, isEnabled, isChecked, [this, idx, mc, learnChan]() {
                        synth->storage.controllers[idx] = mc;
                        synth->storage.controllers_chan[idx] = learnChan;
                    });
                    break;
                }
                case param_cc:
                {
                    if ((ptag < n_global_params && p->midictrl == mc) ||
                        (ptag > n_global_params &&
                         synth->storage.getPatch().param_ptr[ptag]->midictrl == mc))
                    {
                        isChecked = true;
                        isSubChecked = true;
                    }

                    currentSub.addItem(
                        name, isEnabled, isChecked, [this, p, ptag, mc, learnChan]() {
                            if (ptag < n_global_params)
                            {
                                p->midictrl = mc;
                                p->midichan = learnChan;
                            }
                            else
                            {
                                synth->storage.getPatch().param_ptr[ptag]->midictrl = mc;
                                synth->storage.getPatch().param_ptr[ptag]->midichan = learnChan;
                            }
                        });

                    break;
                }
                default:
                    break;
                }
            }

            std::string name =
                fmt::format("{:d} ... {:d}", (20 * subs), std::min((20 * subs) + 20, 128) - 1);

            midiSub.addSubMenu(name, currentSub, true, nullptr, isSubChecked);
        }

        // select channel for MIDI learn submenu
        auto chanSub = juce::PopupMenu();
        auto chanSubName = fmt::format("{}: {}", Surge::GUI::toOSCase("MIDI Channel"),
                                       learnChan == -1 ? "Omni" : std::to_string(learnChan + 1));

        chanSub.addItem("Omni", true, (learnChan == -1), [this, learn_mode, p, ptag, idx]() {
            if (learn_mode == macro_cc)
                this->synth->storage.controllers_chan[idx] = -1;
            else if (ptag < n_global_params)
                p->midichan = -1;
            else
                this->synth->storage.getPatch().param_ptr[ptag]->midichan = -1;
        });

        for (int ch = 0; ch < 16; ch++)
        {
            chanSub.addItem(fmt::format("Channel {}", ch + 1), true, (learnChan == ch),
                            [this, learn_mode, p, ptag, idx, ch]() {
                                if (learn_mode == macro_cc)
                                    this->synth->storage.controllers_chan[idx] = ch;
                                else if (ptag < n_global_params)
                                    p->midichan = ch;
                                else
                                    this->synth->storage.getPatch().param_ptr[ptag]->midichan = ch;
                            });
        }

        midiSub.addSeparator();

        midiSub.addSubMenu(chanSubName, chanSub);

        parentMenu.addSubMenu(Surge::GUI::toOSCase("Assign to MIDI CC"), midiSub);
    }

    bool cancellearn = false;

    int query;

    switch (learn_mode)
    {
    case macro_cc:
        query = synth->learn_macro_from_cc;
        break;
    case param_cc:
        query = synth->learn_param_from_cc;
        break;
    case param_note:
        query = synth->learn_param_from_note;
        break;
    default:
        break;
    }

    if (query > -1 && query == idx)
    {
        cancellearn = true;
    }

    std::string learntxt;

    if (cancellearn)
    {
        learntxt = "Abort MIDI Learn";
    }
    else
    {
        learntxt = "MIDI Learn...";
    }

    parentMenu.addItem(Surge::GUI::toOSCase(learntxt),
                       [this, p, cancellearn, control, idx, learn_mode] {
                           if (cancellearn)
                           {
                               hideMidiLearnOverlay();

                               switch (learn_mode)
                               {
                               case macro_cc:
                                   synth->learn_macro_from_cc = -1;
                                   break;
                               case param_cc:
                                   synth->learn_param_from_cc = -1;
                                   break;
                               case param_note:
                                   synth->learn_param_from_note = -1;
                                   break;
                               default:
                                   break;
                               }
                           }
                           else
                           {
                               showMidiLearnOverlay(control->asJuceComponent()->getBounds());

                               switch (learn_mode)
                               {
                               case macro_cc:
                                   synth->learn_macro_from_cc = idx;
                                   break;
                               case param_cc:
                                   synth->learn_param_from_cc = idx;
                                   break;
                               case param_note:
                                   synth->learn_param_from_note = idx;
                                   break;
                               default:
                                   break;
                               }
                           }
                       });

    switch (learn_mode)
    {
    case macro_cc:
    {
        if (synth->storage.controllers[idx] >= 0)
        {
            const int ch = synth->storage.controllers_chan[idx];
            std::string chtxt = ch == -1 ? "Omni" : fmt::format("Channel {:d}", ch + 1);
            std::string txt =
                fmt::format("{} ({}, {})", Surge::GUI::toOSCase("Clear Learned MIDI"),
                            decodeControllerID(synth->storage.controllers[idx]), chtxt);

            parentMenu.addItem(txt, [this, idx]() {
                synth->storage.controllers[idx] = -1;
                synth->storage.controllers_chan[idx] = -1;

                synth->storage.getPatch().dawExtraState.customcontrol_map[idx] = -1;
                synth->storage.getPatch().dawExtraState.customcontrol_chan_map[idx] = -1;
            });
        }

        break;
    }
    case param_cc:
    {
        if (p->midictrl >= 0)
        {
            const int ch = p->midichan;
            std::string chtxt = ch == -1 ? "Omni" : fmt::format("Channel {:d}", ch + 1);
            std::string txt = fmt::format("{} ({}, {})", Surge::GUI::toOSCase("Clear Learned MIDI"),
                                          decodeControllerID(p->midictrl), chtxt);

            parentMenu.addItem(txt, [this, p, ptag]() {
                if (ptag < n_global_params)
                {
                    p->midictrl = -1;
                    p->midichan = -1;

                    synth->storage.getPatch().dawExtraState.midictrl_map[ptag] = -1;
                    synth->storage.getPatch().dawExtraState.midichan_map[ptag] = -1;
                }
                else
                {
                    synth->storage.getPatch().param_ptr[ptag]->midictrl = -1;
                    synth->storage.getPatch().param_ptr[ptag]->midichan = -1;

                    synth->storage.getPatch().dawExtraState.midictrl_map[ptag] = -1;
                    synth->storage.getPatch().dawExtraState.midichan_map[ptag] = -1;
                }
            });
        }

        break;
    }
    case param_note:
    default:
        break;
    }
}

void SurgeGUIEditor::changeSelectedOsc(int value)
{
    auto tabPosMem = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::RememberTabPositionsPerScene, 0);

    if (tabPosMem)
    {
        current_osc[current_scene] = value;
    }
    else
    {
        for (int i = 0; i < n_scenes; i++)
        {
            current_osc[i] = value;
        }
    }

    queue_refresh = true;
}

void SurgeGUIEditor::changeSelectedScene(int value)
{
    current_scene = value;

    synth->release_if_latched[synth->storage.getPatch().scene_active.val.i] = true;
    synth->storage.getPatch().scene_active.val.i = current_scene;

    bool hasMSEG = isAnyOverlayPresent(MSEG_EDITOR);
    bool hasForm = isAnyOverlayPresent(FORMULA_EDITOR);

    if (hasForm || hasMSEG)
    {
        auto ld = &(synth->storage.getPatch()
                        .scene[current_scene]
                        .lfo[modsource_editor[current_scene] - ms_lfo1]);

        if (ld->shape.val.i == lt_mseg && hasMSEG)
        {
            refreshOverlayWithOpenClose(SurgeGUIEditor::MSEG_EDITOR);
        }
        else if (ld->shape.val.i == lt_formula && hasForm)
        {
            refreshOverlayWithOpenClose(SurgeGUIEditor::FORMULA_EDITOR);
        }
        else if (ld->shape.val.i == lt_mseg && hasForm)
        {
            refreshAndMorphOverlayWithOpenClose(FORMULA_EDITOR, MSEG_EDITOR);
        }
        else if (ld->shape.val.i == lt_formula && hasMSEG)
        {
            refreshAndMorphOverlayWithOpenClose(MSEG_EDITOR, FORMULA_EDITOR);
        }
        else
        {
            if (hasForm)
                closeOverlay(SurgeGUIEditor::FORMULA_EDITOR);
            if (hasMSEG)
                closeOverlay(SurgeGUIEditor::MSEG_EDITOR);
        }
    }

    refresh_mod();

    queue_refresh = true;
}

void SurgeGUIEditor::refreshSkin()
{
    bitmapStore.reset(new SurgeImageStore());
    bitmapStore->setupBuiltinBitmaps();

    if (!currentSkin->reloadSkin(bitmapStore))
    {
        auto db = Surge::GUI::SkinDB::get();
        std::string msg =
            "Unable to load skin! Reverting the skin to Surge Classic XT.\n\nSkin error:\n" +
            db->getAndResetErrorString();
        currentSkin = db->defaultSkin(&(synth->storage));
        currentSkin->reloadSkin(bitmapStore);
        synth->storage.reportError(msg, "Skin Loading Error");
    }

    reloadFromSkin();
    synth->refresh_editor = true;
}

int32_t SurgeGUIEditor::controlModifierClicked(Surge::GUI::IComponentTagValue *control,
                                               const juce::ModifierKeys &button,
                                               bool isDoubleClickEvent)
{
    if (!synth)
    {
        return 0;
    }

    if (!frame)
    {
        return 0;
    }

    if (!editor_open)
    {
        return 0;
    }

    long tag = control->getTag();

    if ((button.isCommandDown() || button.isMiddleButtonDown()) &&
        (tag == tag_mp_patch || tag == tag_mp_category))
    {
        undoManager()->pushPatch();
        synth->selectRandomPatch();

        return 1;
    }

    SelfModulationGuard modGuard(this);

    if (button.isMiddleButtonDown())
    {
        toggle_mod_editing();

        return 1;
    }

    juce::Rectangle<int> viewSize;
    auto bvf = control->asJuceComponent();

    if (bvf)
    {
        viewSize = bvf->getBounds();
    }
    else
    {
        jassert(false);
        std::cout << "WARNING: Control not castable to IBaseView.\n"
                  << "Is your JUCE-only inheritance hierarchy missing the tag class?" << std::endl;
    }

    /*
     * Alright why is this here? Well, when we show the macro menu we use the modal version to
     * stop changes happening underneath us. It's the only non-async menu we have and probably
     * should get fixed. But JUCE has a thing where RMB RMB means the modal menus release both
     * after the second RMB. So the end hover on RMB RMB doesn't work, leading to the bug in
     * #4874. A fix to this is to use an async menu. But I'm not sure why I didn't and there's an
     * incomplete comment that it was for good reason, so instead...
     */
    for (const auto &g : gui_modsrc)
    {
        if (g && g.get() != control)
        {
            g->endHover();
        }
    }

    // In these cases just move along with success. RMB does nothing on these switches
    if (tag == tag_mp_jogwaveshape || tag == tag_mp_jogfx || tag == tag_mp_category ||
        tag == tag_mp_patch || tag == tag_store || tag == tag_action_undo ||
        tag == tag_action_redo || tag == tag_main_vu_meter)
    {
        juce::PopupMenu contextMenu;
        std::string hu, txt;

        switch (tag)
        {
        case tag_mp_patch:
            hu = helpURLForSpecial("patch-navigation");
            txt = "Patch Navigation";
            break;
        case tag_mp_category:
            hu = helpURLForSpecial("patch-navigation");
            txt = "Category Navigation";
            break;
        case tag_mp_jogfx:
            hu = helpURLForSpecial("fx-presets");
            txt = "FX Preset Navigation";
            break;
        case tag_mp_jogwaveshape:
            hu = helpURLForSpecial("waveshaper");
            txt = "Waveshaper Navigation";
            break;
        case tag_action_undo:
        case tag_action_redo:
            hu = helpURLForSpecial("action-history");
            txt = "Action History";
            break;
        case tag_store:
            hu = helpURLForSpecial("save-dialog");
            txt = "Patch Save";
            break;
        case tag_main_vu_meter:
            hu = helpURLForSpecial("level-meter");
            txt = "Level Meter";
            break;
        default:
            break;
        }

        auto tcomp =
            std::make_unique<Surge::Widgets::MenuTitleHelpComponent>(txt, fullyResolvedHelpURL(hu));
        tcomp->setSkin(currentSkin, bitmapStore);

        auto hment = tcomp->getTitle();

        contextMenu.addCustomItem(-1, std::move(tcomp), nullptr, hment);

        if (tag == tag_main_vu_meter)
        {
            contextMenu.addSeparator();

            makeScopeEntry(contextMenu);

            bool cpumeter = Surge::Storage::getUserDefaultValue(
                &(synth->storage), Surge::Storage::ShowCPUUsage, false);

            contextMenu.addItem(Surge::GUI::toOSCase("Show CPU Usage"), true, cpumeter,
                                [this, cpumeter]() {
                                    Surge::Storage::updateUserDefaultValue(
                                        &(synth->storage), Surge::Storage::ShowCPUUsage, !cpumeter);
                                    frame->repaint();
                                });
        }

#ifdef DEBUG
        if (tag == tag_action_undo || tag == tag_action_redo)
        {
            auto undoStack = undoManager()->textStack(Surge::GUI::UndoManager::UNDO);
            auto redoStack = undoManager()->textStack(Surge::GUI::UndoManager::REDO);

            if (!redoStack.empty() || !undoStack.empty())
            {
                contextMenu.addSeparator();
            }

            if (!redoStack.empty())
            {
                Surge::Widgets::MenuCenteredBoldLabel::addToMenu(contextMenu, "REDO STACK");

                int nRedo = redoStack.size();

                for (auto s = redoStack.rbegin(); s != redoStack.rend(); ++s)
                {
                    contextMenu.addItem(*s, [this, tag, nRedo]() {
                        for (int q = nRedo; q > 0; --q)
                        {
                            undoManager()->redo();
                        }
                    });

                    nRedo--;
                }
            }

            if (!undoStack.empty())
            {
                Surge::Widgets::MenuCenteredBoldLabel::addToMenu(contextMenu, "UNDO STACK");

                int nUndo = 1;

                for (auto s : undoStack)
                {
                    contextMenu.addItem(s, [this, tag, nUndo]() {
                        for (int q = 0; q < nUndo; ++q)
                        {
                            undoManager()->undo();
                        }
                    });

                    nUndo++;
                }
            }
        }
#endif

        contextMenu.showMenuAsync(popupMenuOptions(control->asJuceComponent(), false),
                                  Surge::GUI::makeEndHoverCallback(control));

        return 1;
    }

    if (tag == tag_lfo_menu)
    {
        control->setValue(0);
        juce::Point<int> where = control->asJuceComponent()->getBounds().getBottomLeft();
        showLfoMenu(where, control);
        return 1;
    }

    if (tag == tag_status_zoom)
    {
        juce::Point<int> where = control->asJuceComponent()->getBounds().getBottomLeft();
        showZoomMenu(where, control);
        return 1;
    }

    if (tag == tag_status_mpe)
    {
        juce::Point<int> where = control->asJuceComponent()->getBounds().getBottomLeft();
        showMPEMenu(where, control);
        return 1;
    }

    if (tag == tag_status_tune)
    {
        juce::Point<int> where = control->asJuceComponent()->getBounds().getBottomLeft();
        showTuningMenu(where, control);
        return 1;
    }

    std::vector<std::string> clearControlTargetNames;

    auto cmensl = dynamic_cast<Surge::Widgets::MenuForDiscreteParams *>(control);

    if (cmensl && cmensl->getDeactivated())
    {
        return 0;
    }

    if (button.isPopupMenu())
    {
        if (tag == tag_settingsmenu)
        {
            useDevMenu = true;
            auto where = frame->getLocalPoint(nullptr, juce::Desktop::getMousePosition());
            showSettingsMenu(where, control);
            return 1;
        }

        if (tag == tag_osc_select)
        {
            auto r = control->asJuceComponent()->getBounds();

            int a = current_osc[current_scene];
            // int a = limit_range((int)((3 * (where.x - r.getX())) / r.getWidth()), 0, 2);

            std::string oscname = "Osc " + std::to_string(a + 1);

            auto contextMenu = juce::PopupMenu();
            auto hu = helpURLForSpecial("osc-select");
            char txt[TXT_SIZE];

            auto lurl = hu;
            if (lurl != "")
                lurl = fullyResolvedHelpURL(hu);
            auto tcomp = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>(
                fmt::format("Osc {:d}", a + 1), lurl);
            tcomp->setSkin(currentSkin, bitmapStore);
            auto hment = tcomp->getTitle();

            contextMenu.addCustomItem(-1, std::move(tcomp), nullptr, hment);

            contextMenu.addSeparator();

            contextMenu.addItem(Surge::GUI::toOSCase("Copy from ") + oscname, [this, a]() {
                synth->storage.clipboard_copy(cp_osc, current_scene, a);
            });

            contextMenu.addItem(
                Surge::GUI::toOSCase("Copy from ") + oscname +
                    Surge::GUI::toOSCase(" with Modulation"),
                [this, a]() { synth->storage.clipboard_copy(cp_oscmod, current_scene, a); });

            if (synth->storage.get_clipboard_type() == cp_osc)
            {
                contextMenu.addItem(Surge::GUI::toOSCase("Paste to ") + oscname, [this, a]() {
                    if (synth->storage.get_clipboard_type() & cp_modulator_target)
                    {
                        undoManager()->pushPatch();
                    }
                    else
                    {
                        undoManager()->pushOscillator(current_scene, a);
                    }
                    synth->clear_osc_modulation(current_scene, a);
                    synth->storage.clipboard_paste(cp_osc, current_scene, a);
                    synth->storage.getPatch().isDirty = true;
                    queue_refresh = true;
                });
            }

            contextMenu.showMenuAsync(popupMenuOptions(control->asJuceComponent(), false),
                                      Surge::GUI::makeEndHoverCallback(control));
            return 1;
        }

        if (tag == tag_scene_select)
        {
            auto contextMenu = juce::PopupMenu();
            auto hu = helpURLForSpecial("scene-select");
            auto lurl = hu;

            if (lurl != "")
            {
                lurl = fullyResolvedHelpURL(hu);
            }

            auto tcomp = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>(
                fmt::format("Scene {:c}", 'A' + current_scene), lurl);
            tcomp->setSkin(currentSkin, bitmapStore);
            auto hment = tcomp->getTitle();

            contextMenu.addCustomItem(-1, std::move(tcomp), nullptr, hment);

            contextMenu.addSeparator();

            contextMenu.addItem(Surge::GUI::toOSCase("Copy Scene"), [this]() {
                synth->storage.clipboard_copy(cp_scene, current_scene, -1);
            });

            contextMenu.addItem(
                Surge::GUI::toOSCase("Paste Scene"),
                synth->storage.get_clipboard_type() == cp_scene, // enabled
                false, [this]() {
                    undoManager()->pushPatch();
                    synth->storage.clipboard_paste(
                        cp_scene, current_scene, -1, ms_original,
                        [this](int p, modsources m) {
                            auto res = synth->isValidModulation(p, m);
                            return res;
                        },
                        [this](std::unique_ptr<Surge::FxClipboard::Clipboard> &f, int cge) {
                            Surge::FxClipboard::pasteFx(&(synth->storage), &synth->fxsync[cge], *f);
                            synth->fx_reload[cge] = true;
                        });

                    synth->load_fx_needed = true;
                    synth->storage.getPatch().isDirty = true;
                    queue_refresh = true;
                });

            contextMenu.showMenuAsync(popupMenuOptions(control->asJuceComponent(), false),
                                      Surge::GUI::makeEndHoverCallback(control));
            return 1;
        }
    }

    if (tag == tag_mseg_edit)
    {
        if (lfoDisplay->isMSEG() || lfoDisplay->isFormula())
        {
            std::string olname = lfoDisplay->isMSEG() ? "MSEG Editor" : "Formula Editor";
            std::string helpname = lfoDisplay->isMSEG() ? "mseg-editor" : "formula-editor";

            auto msurl = SurgeGUIEditor::helpURLForSpecial(&(synth->storage), helpname);
            auto hurl = SurgeGUIEditor::fullyResolvedHelpURL(msurl);

            auto hmen = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>(olname, hurl);
            hmen->setSkin(currentSkin, bitmapStore);
            auto hment = hmen->getTitle();

            auto contextMenu = juce::PopupMenu();
            contextMenu.addCustomItem(-1, std::move(hmen), nullptr, hment);
            contextMenu.showMenuAsync(popupMenuOptions(control->asJuceComponent(), false),
                                      Surge::GUI::makeEndHoverCallback(control));
        }
    }

    if (tag == tag_analyzewaveshape)
    {
        auto contextMenu = juce::PopupMenu();
        auto hu = helpURLForSpecial("waveshaper-analysis");
        auto lurl = hu;

        if (lurl != "")
            lurl = fullyResolvedHelpURL(hu);

        auto tcomp =
            std::make_unique<Surge::Widgets::MenuTitleHelpComponent>("Waveshaper Analysis", lurl);
        auto hment = tcomp->getTitle();

        tcomp->setSkin(currentSkin, bitmapStore);
        contextMenu.addCustomItem(-1, std::move(tcomp), nullptr, hment);

        contextMenu.showMenuAsync(popupMenuOptions(control->asJuceComponent(), false),
                                  Surge::GUI::makeEndHoverCallback(control));

        return 1;
    }

    if (tag == tag_analyzefilters)
    {
        auto contextMenu = juce::PopupMenu();
        auto hu = helpURLForSpecial("filter-analysis");
        auto lurl = hu;

        if (lurl != "")
            lurl = fullyResolvedHelpURL(hu);

        auto tcomp =
            std::make_unique<Surge::Widgets::MenuTitleHelpComponent>("Filter Analysis", lurl);
        auto hment = tcomp->getTitle();

        tcomp->setSkin(currentSkin, bitmapStore);
        contextMenu.addCustomItem(-1, std::move(tcomp), nullptr, hment);

        contextMenu.showMenuAsync(popupMenuOptions(control->asJuceComponent(), false),
                                  Surge::GUI::makeEndHoverCallback(control));

        return 1;
    }

    if ((tag >= tag_mod_source0) && (tag < tag_mod_source_end))
    {
        auto *cms = dynamic_cast<Surge::Widgets::ModulationSourceButton *>(control);
        modsources modsource = cms->getCurrentModSource();

        if (button.isPopupMenu())
        {
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

            auto lurl = hu;
            if (lurl != "")
                lurl = fullyResolvedHelpURL(lurl);
            auto hmen = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>(
                ModulatorName::modulatorNameWithIndex(&synth->storage, current_scene, modsource,
                                                      modsource_index, false, false),
                lurl);
            hmen->setSkin(currentSkin, bitmapStore);
            auto hment = hmen->getTitle();
            contextMenu.addCustomItem(-1, std::move(hmen), nullptr, hment);

            contextMenu.addSeparator();

            int n_total_md = synth->storage.getPatch().param_ptr.size();
            const int max_md = 4096;

            assert(max_md >= n_total_md);

            bool cancellearn = false;
            int ccid = 0;
            int detailedMode = Surge::Storage::getUserDefaultValue(
                &(this->synth->storage), Surge::Storage::HighPrecisionReadouts, 0);

            // should start at 0, but it started at 1 before
            // there might be a reason but I don't remember why
            std::vector<modsources> possibleSources;
            possibleSources.push_back(modsource);

            for (auto thisms : possibleSources)
            {
                bool first_destination = true;
                int n_md = 0;

                for (int md = 0; md < n_total_md; md++)
                {
                    auto activeScene = synth->storage.getPatch().scene_active.val.i;
                    Parameter *parameter = synth->storage.getPatch().param_ptr[md];

                    auto use_scene = 0;
                    bool allScenes = true;
                    if (this->synth->isModulatorDistinctPerScene(thisms))
                    {
                        allScenes = false;
                        use_scene = current_scene;
                    }

                    bool isGlobal = md < n_global_params;
                    // TODO: FIX SCENE ASSUMPTION
                    bool isActiveScene = parameter->scene - 1 == activeScene;

                    if ((isGlobal || isActiveScene || allScenes) &&
                        synth->isAnyActiveModulation(md, thisms, use_scene))
                    {
                        auto indices = synth->getModulationIndicesBetween(md, thisms, use_scene);
                        auto hasIdx = synth->supportsIndexedModulator(use_scene, thisms);
                        char modtxt[TXT_SIZE];

                        for (auto modidx : indices)
                        {
                            if (hasIdx && modidx != modsource_index)
                                continue;

                            if (first_destination)
                            {
                                contextMenu.addSeparator();

                                Surge::Widgets::MenuCenteredBoldLabel::addToMenu(contextMenu,
                                                                                 "MODULATIONS");
                            }

                            parameter->get_display_of_modulation_depth(
                                modtxt, synth->getModDepth(md, thisms, use_scene, modidx),
                                synth->isBipolarModulation(thisms), Parameter::Menu);
                            char tmptxt[1024]; // leave room for that ubuntu 20.0 error

                            std::string targetName;

                            if (parameter->ctrlgroup == cg_LFO)
                            {
                                char pname[TXT_SIZE];

                                parameter->create_fullname(
                                    parameter->get_name(), pname, parameter->ctrlgroup,
                                    parameter->ctrlgroup_entry,
                                    ModulatorName::modulatorName(&synth->storage,
                                                                 parameter->ctrlgroup_entry, true,
                                                                 current_scene)
                                        .c_str());
                                targetName = pname;
                            }
                            else
                            {
                                targetName = parameter->get_full_name();
                            }

                            if (allScenes && (!isGlobal && !isActiveScene))
                            {
                                targetName +=
                                    fmt::format(" (Scene {:c})", 'A' + (parameter->scene - 1));
                            }

                            auto clearOp = [this, first_destination, md, n_total_md, thisms, modidx,
                                            use_scene, bvf, control]() {
                                bool resetName = false;   // Should I reset the name?
                                std::string newName = ""; // And to what?
                                int ccid = thisms - ms_ctrl1;

                                if (first_destination)
                                {
                                    if (strncmp(
                                            synth->storage.getPatch().CustomControllerLabel[ccid],
                                            synth->storage.getPatch().param_ptr[md]->get_name(),
                                            CUSTOM_CONTROLLER_LABEL_SIZE - 1) == 0)
                                    {
                                        // So my modulator is named after my short name. I
                                        // haven't been renamed. So I want to reset at least to
                                        // "-" unless someone is after me
                                        resetName = true;
                                        newName = "-";

                                        // Now we have to find if there's another modulation
                                        // below me
                                        int nextmd = md + 1;

                                        while (nextmd < n_total_md &&
                                               !synth->isAnyActiveModulation(nextmd, thisms,
                                                                             current_scene))
                                        {
                                            nextmd++;
                                        }

                                        if (nextmd < n_total_md && strlen(synth->storage.getPatch()
                                                                              .param_ptr[nextmd]
                                                                              ->get_name()) > 1)
                                        {
                                            newName = synth->storage.getPatch()
                                                          .param_ptr[nextmd]
                                                          ->get_name();
                                        }
                                    }
                                }

                                pushModulationToUndoRedo(md, (modsources)thisms, use_scene, modidx,
                                                         Surge::GUI::UndoManager::UNDO);
                                synth->clearModulation(md, thisms, use_scene, modidx, false);
                                refresh_mod();

                                if (bvf)
                                {
                                    bvf->repaint();
                                }

                                if (resetName)
                                {
                                    // And this is where we apply the name refresh, of course.
                                    strxcpy(synth->storage.getPatch().CustomControllerLabel[ccid],
                                            newName.c_str(), CUSTOM_CONTROLLER_LABEL_SIZE - 1);
                                    synth->storage.getPatch()
                                        .CustomControllerLabel[ccid][CUSTOM_CONTROLLER_LABEL_SIZE -
                                                                     1] = 0;
                                    auto msb =
                                        dynamic_cast<Surge::Widgets::ModulationSourceButton *>(
                                            control);

                                    if (msb)
                                    {
                                        msb->setCurrentModLabel(
                                            synth->storage.getPatch().CustomControllerLabel[ccid]);
                                    }

                                    if (bvf)
                                    {
                                        bvf->repaint();
                                    }

                                    synth->updateDisplay();
                                }
                            };

                            auto cb = [this, parameter, use_scene, bvf, thisms, modidx,
                                       clearOp](Surge::Widgets::ModMenuCustomComponent::OpType op) {
                                auto ptag = parameter->id;
                                switch (op)
                                {
                                case Surge::Widgets::ModMenuCustomComponent::EDIT:
                                    this->promptForUserValueEntry(parameter, bvf, thisms, use_scene,
                                                                  modidx);
                                    break;
                                case Surge::Widgets::ModMenuCustomComponent::MUTE:
                                {
                                    pushModulationToUndoRedo(ptag, (modsources)thisms, use_scene,
                                                             modidx, Surge::GUI::UndoManager::UNDO);
                                    auto is = synth->isModulationMuted(ptag, (modsources)thisms,
                                                                       use_scene, modidx);

                                    synth->muteModulation(ptag, (modsources)thisms, use_scene,
                                                          modidx, !is);
                                    refresh_mod();
                                    synth->refresh_editor = true;
                                }
                                break;
                                case Surge::Widgets::ModMenuCustomComponent::CLEAR:
                                    clearOp();
                                    break;
                                }
                            };
                            auto modMenu = std::make_unique<Surge::Widgets::ModMenuCustomComponent>(
                                &synth->storage, targetName, modtxt, cb, true);
                            modMenu->setSkin(currentSkin, bitmapStore);
                            modMenu->setIsMuted(synth->isModulationMuted(
                                parameter->id, (modsources)thisms, use_scene, modidx));
                            auto mmt = modMenu->getTitle();
                            auto pm = modMenu->createAccessibleSubMenu();
                            contextMenu.addCustomItem(-1, std::move(modMenu),
                                                      pm.get() ? std::move(pm) : nullptr, mmt);
                            first_destination = false;
                            n_md++;
                        }
                    }
                }

                if (n_md > 1)
                {
                    auto use_scene = 0;
                    bool allScenes = true;

                    if (this->synth->isModulatorDistinctPerScene(thisms))
                    {
                        allScenes = false;
                        use_scene = current_scene;
                    }

                    auto cb = [this, thisms, use_scene, allScenes, n_total_md,
                               control](Surge::Widgets::ModMenuForAllComponent::AllAction action) {
                        switch (action)
                        {
                        case Surge::Widgets::ModMenuForAllComponent::CLEAR:
                        {
                            for (int md = 0; md < n_total_md; md++)
                            {
                                int scene = synth->storage.getPatch().param_ptr[md]->scene;
                                if (scene == 0 || allScenes || scene == current_scene + 1)
                                    for (auto idx :
                                         synth->getModulationIndicesBetween(md, thisms, use_scene))
                                    {
                                        pushModulationToUndoRedo(md, (modsources)thisms, use_scene,
                                                                 idx,
                                                                 Surge::GUI::UndoManager::UNDO);

                                        synth->clearModulation(md, thisms, use_scene, idx, false);
                                    }
                            }

                            refresh_mod();

                            // Also blank out the name and rebuild the UI
                            if (within_range(ms_ctrl1, thisms, ms_ctrl1 + n_customcontrollers - 1))
                            {
                                int ccid = thisms - ms_ctrl1;

                                synth->storage.getPatch().CustomControllerLabel[ccid][0] = '-';
                                synth->storage.getPatch().CustomControllerLabel[ccid][1] = 0;
                                auto msb =
                                    dynamic_cast<Surge::Widgets::ModulationSourceButton *>(control);
                                if (msb)
                                    msb->setCurrentModLabel(
                                        synth->storage.getPatch().CustomControllerLabel[ccid]);
                            }
                            if (control->asJuceComponent())
                                control->asJuceComponent()->repaint();
                            synth->updateDisplay();
                        }
                        break;
                        case Surge::Widgets::ModMenuForAllComponent::UNMUTE:
                        case Surge::Widgets::ModMenuForAllComponent::MUTE:
                        {
                            bool state = (action == Surge::Widgets::ModMenuForAllComponent::MUTE);
                            for (int md = 0; md < n_total_md; ++md)
                            {
                                for (int sc = 0; sc < n_scenes; ++sc)
                                {
                                    auto indices = synth->getModulationIndicesBetween(
                                        md, (modsources)thisms, sc);
                                    for (auto idx : indices)
                                        synth->muteModulation(md, (modsources)thisms, sc, idx,
                                                              state);
                                }
                            }
                        }
                        break;
                        }
                        refresh_mod();
                        synth->refresh_editor = true;
                    };

                    auto allThree = std::make_unique<Surge::Widgets::ModMenuForAllComponent>(
                        &synth->storage, cb, true);
                    allThree->setSkin(currentSkin, bitmapStore);
                    auto at = allThree->getTitle();
                    auto pm = allThree->createAccessibleSubMenu();
                    contextMenu.addCustomItem(-1, std::move(allThree),
                                              pm.get() ? std::move(pm) : nullptr, at);
                }
            }
            int sc = limit_range(synth->storage.getPatch().scene_active.val.i, 0, n_scenes - 1);

            // for macros only
            if (within_range(ms_ctrl1, modsource, ms_ctrl1 + n_customcontrollers - 1))
            {
                contextMenu.addSeparator();

                ccid = modsource - ms_ctrl1;

                createMIDILearnMenuEntries(contextMenu, macro_cc, ccid, control);

                auto cms = ((ControllerModulationSource *)synth->storage.getPatch()
                                .scene[current_scene]
                                .modsources[modsource]);

                contextMenu.addSeparator();

                std::string vtxt =
                    fmt::format("{:s}: {:.{}f} %", Surge::GUI::toOSCase("Edit Value"),
                                100 * cms->get_output(0), (detailedMode ? 6 : 2));
                contextMenu.addItem(vtxt, [this, bvf, modsource]() {
                    promptForUserValueEntry(nullptr, bvf, modsource,
                                            0,  // controllers aren't per scene
                                            0); // controllers aren't indexed
                });

                contextMenu.addSeparator();

                bool ibp = synth->storage.getPatch()
                               .scene[current_scene]
                               .modsources[ms_ctrl1 + ccid]
                               ->is_bipolar();

                contextMenu.addItem(
                    Surge::GUI::toOSCase("Bipolar Mode"), true, ibp, [this, control, ccid]() {
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
                                      ->get_output01(0);
                        control->setValue(f);

                        synth->storage.getPatch().isDirty = true;

                        auto msb = dynamic_cast<Surge::Widgets::ModulationSourceButton *>(control);

                        if (msb)
                        {
                            msb->setIsBipolar(bp);
                        }

                        refresh_mod();
                    });

                contextMenu.addItem(
                    Surge::GUI::toOSCase("Rename Macro..."), [this, control, ccid]() {
                        auto msb = dynamic_cast<Surge::Widgets::ModulationSourceButton *>(control);
                        auto pos = control->asJuceComponent()->getBounds().getTopLeft();

                        openMacroRenameDialog(ccid, pos, msb);
                    });
            }

            contextMenu.addSeparator();

            if (cms->needsHamburger())
            {
                auto hamSub = juce::PopupMenu();
                cms->buildHamburgerMenu(hamSub, true);
                contextMenu.addSubMenu(Surge::GUI::toOSCase("Switch To"), hamSub);
                contextMenu.addSeparator();
            }

            contextMenu.addSeparator();

            int lfo_id = isLFO(modsource) ? modsource - ms_lfo1 : -1;

            if (lfo_id >= 0)
            {
                contextMenu.addItem(
                    Surge::GUI::toOSCase("Rename Modulator..."), [this, control, lfo_id]() {
                        auto pos = control->asJuceComponent()->getBounds().getTopLeft();

                        openLFORenameDialog(lfo_id, pos, control->asJuceComponent());
                    });

                contextMenu.addSeparator();

                contextMenu.addItem(Surge::GUI::toOSCase("Copy Modulator"), [this, sc, lfo_id]() {
                    synth->storage.clipboard_copy(cp_lfo, sc, lfo_id);
                    mostRecentCopiedMSEGState = msegEditState[sc][lfo_id];
                });

                contextMenu.addItem(Surge::GUI::toOSCase("Copy Modulator with Targets"),
                                    [this, sc, lfo_id]() {
                                        synth->storage.clipboard_copy(cp_lfomod, sc, lfo_id);
                                        mostRecentCopiedMSEGState = msegEditState[sc][lfo_id];
                                    });

                contextMenu.addItem(Surge::GUI::toOSCase("Copy Targets"), [this, sc, lfo_id]() {
                    synth->storage.clipboard_copy(cp_modulator_target, sc, lfo_id);
                });

                if (synth->storage.get_clipboard_type() & cp_lfo ||
                    synth->storage.get_clipboard_type() & cp_modulator_target)
                {
                    auto t = synth->storage.get_clipboard_type();

                    contextMenu.addItem(Surge::GUI::toOSCase("Paste"), [this, sc, t, lfo_id]() {
                        if (synth->storage.get_clipboard_type() & cp_modulator_target)
                        {
                            // Just punt and save the whole patch in this edge case
                            undoManager()->pushPatch();
                        }
                        else
                        {
                            undoManager()->pushFullLFO(sc, lfo_id);
                        }
                        synth->storage.clipboard_paste(
                            t, sc, lfo_id, ms_original, [this](int p, modsources m) {
                                auto res = synth->isValidModulation(p, m);
                                return res;
                            });

                        if (t & cp_lfo)
                        {
                            msegEditState[sc][lfo_id] = mostRecentCopiedMSEGState;
                        }

                        synth->storage.getPatch().isDirty = true;
                        queue_refresh = true;
                    });
                }
            }
            else
            {
                contextMenu.addItem(Surge::GUI::toOSCase("Copy Targets"), [this, sc, modsource]() {
                    synth->storage.clipboard_copy(cp_modulator_target, sc, -1, modsource);
                });

                if (synth->storage.get_clipboard_type() & cp_modulator_target)
                {
                    contextMenu.addItem(Surge::GUI::toOSCase("Paste"), [this, sc, modsource]() {
                        undoManager()->pushPatch(); // we could do better here
                        synth->storage.clipboard_paste(
                            cp_modulator_target, sc, -1, modsource, [this](int p, modsources m) {
                                auto res = synth->isValidModulation(p, m);
                                return res;
                            });
                        synth->storage.getPatch().isDirty = true;
                        queue_refresh = true;
                    });
                }
            }

            // for macros only (VST3 host side menu)
            if (within_range(ms_ctrl1, modsource, ms_ctrl1 + n_customcontrollers - 1))
            {
                contextMenu.addSeparator();

                auto jpm = juceEditor->hostMenuForMacro(ccid);

                if (jpm.getNumItems() > 0)
                {
                    contextMenu.addSeparator();

                    juce::PopupMenu::MenuItemIterator iterator(jpm);

                    while (iterator.next())
                    {
                        contextMenu.addItem(iterator.getItem());
                    }
                }
            }

            contextMenu.showMenuAsync(popupMenuOptions(control->asJuceComponent(), false),
                                      Surge::GUI::makeEndHoverCallback(control));
            return 1;
        }

        return 0;
    }

    if (tag < start_paramtags)
    {
        return 0;
    }

    if (isDoubleClickEvent)
    {
        int ptag = tag - start_paramtags;

        if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
        {
            Parameter *p = synth->storage.getPatch().param_ptr[ptag];
            if (p->ctrltype == ct_filtertype || p->ctrltype == ct_wstype)
            {
                p->deactivated = !p->deactivated;
                synth->storage.getPatch().isDirty = true;
                synth->refresh_editor = true;
                return 0;
            }
        }
    }

    int ptag = tag - start_paramtags;

    if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
    {
        Parameter *p = synth->storage.getPatch().param_ptr[ptag];

        // don't show RMB context menu for filter subtype if it's hidden/not applicable
        auto f1type = synth->storage.getPatch().scene[current_scene].filterunit[0].type.val.i;
        auto f2type = synth->storage.getPatch().scene[current_scene].filterunit[1].type.val.i;

        if (tag == f1subtypetag &&
            (f1type == sst::filters::fut_none || f1type == sst::filters::fut_SNH))
        {
            return 1;
        }

        if (tag == f2subtypetag &&
            (f2type == sst::filters::fut_none || f2type == sst::filters::fut_SNH))
        {
            return 1;
        }

        // all the RMB context menus
        if (button.isPopupMenu())
        {
            juce::Point<int> menuRect{};

            auto contextMenu = juce::PopupMenu();
            std::string helpurl = helpURLFor(p);

            auto lurl = helpurl;
            if (lurl != "")
                lurl = fullyResolvedHelpURL(helpurl);
            auto hmen =
                std::make_unique<Surge::Widgets::MenuTitleHelpComponent>(p->get_full_name(), lurl);
            auto hment = hmen->getTitle();
            hmen->setSkin(currentSkin, bitmapStore);
            contextMenu.addCustomItem(-1, std::move(hmen), nullptr, hment);

            contextMenu.addSeparator();

            std::string txt, txt2;

            txt = p->get_display();

            if (!p->temposync)
            {
                txt2 = fmt::format("{:s}: {:s}", Surge::GUI::toOSCase("Edit Value"), txt);
            }
            else
            {
                txt2 = txt;
            }

            if (p->valtype == vt_float)
            {
                if (p->can_temposync() && p->temposync)
                {
                    auto tsMenuR = juce::PopupMenu();
                    auto tsMenuD = juce::PopupMenu();
                    auto tsMenuT = juce::PopupMenu();

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

                        tsMenuR.addItem(p->tempoSyncNotationValue(mul * ((float)i)),
                                        [p, i, this]() {
                                            p->val.f = (float)i;
                                            p->bound_value();
                                            synth->storage.getPatch().isDirty = true;
                                            synth->refresh_editor = true;
                                        });

                        tsMenuD.addItem(p->tempoSyncNotationValue(mul * ((float)i + dotlaboff)),
                                        [p, i, dotoff, this]() {
                                            p->val.f = (float)i + dotoff;
                                            p->bound_value();
                                            synth->storage.getPatch().isDirty = true;
                                            synth->refresh_editor = true;
                                        });

                        tsMenuT.addItem(p->tempoSyncNotationValue(mul * ((float)i + triplaboff)),
                                        [p, i, tripoff, this]() {
                                            p->val.f = (float)i + tripoff;
                                            p->bound_value();
                                            synth->storage.getPatch().isDirty = true;
                                            synth->refresh_editor = true;
                                        });
                    }

                    contextMenu.addItem(txt2, []() {});

                    contextMenu.addSeparator();

                    contextMenu.addSubMenu("Straight", tsMenuR);
                    contextMenu.addSubMenu("Dotted", tsMenuD);
                    contextMenu.addSubMenu("Triplet", tsMenuT);
                }
                else
                {
                    contextMenu.addItem(
                        txt2, [this, p, bvf]() { this->promptForUserValueEntry(p, bvf); });

                    contextMenu.addSeparator();
                }
            }
            else if (p->valtype == vt_bool)
            {
                if (p->ctrltype == ct_bool_keytrack || p->ctrltype == ct_bool_retrigger)
                {
                    std::vector<Parameter *> impactedParms;
                    std::vector<std::string> tgltxt = {"Enable", "Disable"};
                    std::string parname =
                        (p->ctrltype == ct_bool_retrigger) ? "Retrigger" : "Keytrack";

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

                    auto txt3 =
                        Surge::GUI::toOSCase(tgltxt[ktsw] + " " + parname + " for All Oscillators");

                    contextMenu.addItem(txt3, [this, ktsw, impactedParms]() {
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
                }
            }
            else if (p->valtype == vt_int)
            {
                if (p->can_setvalue_from_string())
                {

                    if (p->ctrltype == ct_midikey_or_channel)
                    {
                        auto sc_mode = synth->storage.getPatch().scenemode.val.i;

                        if (sc_mode == sm_split || sc_mode == sm_chsplit)
                        {
                            contextMenu.addItem(
                                txt2, [this, p, bvf]() { this->promptForUserValueEntry(p, bvf); });

                            contextMenu.addSeparator();

                            createMIDILearnMenuEntries(contextMenu, param_note, p->id, control);
                        }
                    }
                    else
                    {
                        contextMenu.addItem(
                            txt2, [this, p, bvf]() { this->promptForUserValueEntry(p, bvf); });

                        contextMenu.addSeparator();
                    }

                    if (p->can_extend_range())
                    {
                        contextMenu.addItem(Surge::GUI::toOSCase("Use Decimal Values"), true,
                                            p->extend_range, [this, p, control]() {
                                                auto wasExtended = p->extend_range;
                                                p->set_extend_range(!p->extend_range);
                                                if (p->ctrltype == ct_pbdepth)
                                                {
                                                    if (wasExtended)
                                                        p->val.i = p->val.i / 100;
                                                    else
                                                        p->val.i = p->val.i * 100;
                                                    // This requires an extra call to
                                                    // paramChangeToListeners()
                                                    juceEditor->processor.paramChangeToListeners(p);
                                                }

                                                synth->storage.getPatch().isDirty = true;
                                                synth->refresh_editor = true;
                                                juceEditor->processor.paramChangeToListeners(
                                                    p, true,
                                                    juceEditor->processor.SCT_EX_EXTENDRANGE,
                                                    (float)p->extend_range, .0, .0, "");
                                            });
                    }
                }
                else
                {
                    int incr = 1;

                    if (p->ctrltype == ct_vocoder_bandcount)
                    {
                        incr = 4;
                    }

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

                        if (ftype == sst::filters::fut_comb_pos ||
                            ftype == sst::filters::fut_comb_neg)
                        {
                            isCombOnSubtype = true;
                        }

                        max = sst::filters::fut_subcount[ftype] - 1;
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
                        {
                            std::sort(groupAppearanceOrder.begin(), groupAppearanceOrder.end());
                        }
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

                        for (auto grpN : groupAppearanceOrder)
                        {
                            auto grp = reorderMap[grpN];
                            bool isSubChecked = false;
                            juce::PopupMenu sub;

                            for (auto rp : grp)
                            {
                                int i = rp.second;
                                bool isChecked = false;
                                std::string displaytxt = dm->nameAtStreamedIndex(i);
                                juce::PopupMenu *addTo;

                                if (useSubMenus && grpN == "")
                                {
                                    addTo = &contextMenu;
                                }
                                else
                                {
                                    addTo = &sub;
                                }

                                if (i == p->val.i)
                                {
                                    isChecked = true;
                                    isSubChecked = true;
                                }

                                addTo->addItem(displaytxt, true, isChecked, [this, p, i, tag]() {
                                    float ef =
                                        Parameter::intScaledToFloat(i, p->val_max.i, p->val_min.i);
                                    undoManager()->pushParameterChange(p->id, p, p->val);
                                    synth->setParameter01(synth->idForParameter(p), ef, false,
                                                          false);

                                    if (p->ctrltype == ct_wstype)
                                    {
                                        updateWaveshaperOverlay();
                                    }
                                    if (p->ctrlgroup == cg_FILTER)
                                    {
                                        if (auto fo = getOverlayIfOpenAs<
                                                Surge::Overlays::OverlayComponent>(FILTER_ANALYZER))
                                        {
                                            fo->forceDataRefresh();
                                            fo->repaint();
                                        }
                                    }
                                    broadcastPluginAutomationChangeFor(p);
                                    synth->refresh_editor = true;
                                });
                            }

                            if (sub.getNumItems() > 0)
                            {
                                contextMenu.addSubMenu(grpN, sub, true, nullptr, isSubChecked, 0);
                            }
                        }
                    }
                    else
                    {
                        for (int i = p->val_min.i; i <= max; i += incr)
                        {
                            int ii = i;

                            if (p->ctrltype == ct_scenemode)
                            {
                                if (ii == 2)
                                {
                                    ii = 3;
                                }
                                else if (ii == 3)
                                {
                                    ii = 2;
                                }
                            }

                            float ef = (1.0f * ii - p->val_min.i) / (p->val_max.i - p->val_min.i);

                            contextMenu.addItem(
                                p->get_display(true, ef), true, (ii == p->val.i), [this, ef, p]() {
                                    undoManager()->pushParameterChange(p->id, p, p->val);
                                    synth->setParameter01(synth->idForParameter(p), ef, false,
                                                          false);
                                    if (p->ctrlgroup == cg_FILTER)
                                    {
                                        if (auto fo = getOverlayIfOpenAs<
                                                Surge::Overlays::OverlayComponent>(FILTER_ANALYZER))
                                        {
                                            fo->forceDataRefresh();
                                            fo->repaint();
                                        }
                                    }
                                    broadcastPluginAutomationChangeFor(p);
                                    synth->refresh_editor = true;
                                });
                        }

                        if (isCombOnSubtype)
                        {
                            contextMenu.addSeparator();

                            bool isChecked = synth->storage.getPatch().correctlyTuneCombFilter;

                            contextMenu.addItem(
                                Surge::GUI::toOSCase("Precise Tuning"), true, isChecked, [this]() {
                                    synth->storage.getPatch().correctlyTuneCombFilter =
                                        !synth->storage.getPatch().correctlyTuneCombFilter;
                                    synth->storage.getPatch().isDirty = true;
                                });
                        }

                        if (p->ctrltype == ct_polymode && (p->val.i == pm_poly))
                        {
                            std::vector<std::string> labels = {"Stack Multiple", "Reuse Single"};
                            std::vector<PolyVoiceRepeatedKeyMode> vals = {NEW_VOICE_EVERY_NOTEON,
                                                                          ONE_VOICE_PER_KEY};

                            Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(
                                contextMenu, "SAME KEY VOICE ALLOCATION");

                            for (int i = 0; i < vals.size(); ++i)
                            {
                                bool isChecked = (vals[i] == synth->storage.getPatch()
                                                                 .scene[current_scene]
                                                                 .polyVoiceRepeatedKeyMode);
                                contextMenu.addItem(Surge::GUI::toOSCase(labels[i]), true,
                                                    isChecked, [this, isChecked, vals, i]() {
                                                        synth->storage.getPatch()
                                                            .scene[current_scene]
                                                            .polyVoiceRepeatedKeyMode = vals[i];
                                                        if (!isChecked)
                                                            synth->storage.getPatch().isDirty =
                                                                true;
                                                    });
                            }
                        }

                        if (p->ctrltype == ct_polymode)
                        {
                            if (p->val.i == pm_mono || p->val.i == pm_mono_st ||
                                p->val.i == pm_mono_fp || p->val.i == pm_mono_st_fp)
                            {
                                {
                                    std::vector<std::string> labels = {"Last", "High", "Low",
                                                                       "Legacy"};
                                    std::vector<MonoVoicePriorityMode> vals = {
                                        ALWAYS_LATEST, ALWAYS_HIGHEST, ALWAYS_LOWEST,
                                        NOTE_ON_LATEST_RETRIGGER_HIGHEST};

                                    Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(
                                        contextMenu, "NOTE PRIORITY");

                                    for (int i = 0; i < 4; ++i)
                                    {
                                        bool isChecked = (vals[i] == synth->storage.getPatch()
                                                                         .scene[current_scene]
                                                                         .monoVoicePriorityMode);
                                        contextMenu.addItem(
                                            Surge::GUI::toOSCase(labels[i]), true, isChecked,
                                            [this, isChecked, vals, i]() {
                                                undoManager()->pushPatch();
                                                synth->storage.getPatch()
                                                    .scene[current_scene]
                                                    .monoVoicePriorityMode = vals[i];
                                                if (!isChecked)
                                                    synth->storage.getPatch().isDirty = true;
                                            });
                                    }
                                }

                                addEnvTrigOptions(contextMenu, current_scene);

                                contextMenu.addSeparator();

                                contextMenu.addSubMenu(
                                    Surge::GUI::toOSCase("Sustain Pedal in Mono Mode"),
                                    makeMonoModeOptionsMenu(menuRect, false));
                            }

                            if (p->val.i == pm_latch)
                            {
                                addEnvTrigOptions(contextMenu, current_scene);
                            }
                        }
                    }

                    if (p->can_deactivate())
                    {
                        contextMenu.addSeparator();

                        contextMenu.addItem("Enabled", true, !p->deactivated, [this, p]() {
                            p->deactivated = !p->deactivated;
                            synth->storage.getPatch().isDirty = true;
                            synth->refresh_editor = true;

                            // output updated value to OSC
                            juceEditor->processor.paramChangeToListeners(
                                p, true, juceEditor->processor.SCT_EX_ENABLE,
                                (float)!p->deactivated, .0, .0, "");
                        });
                    }
                }
            }

            if (p->valtype == vt_float)
            {
                if (p->can_temposync())
                {
                    if (!p->temposync)
                    {
                        contextMenu.addSeparator();
                    }

                    contextMenu.addItem(
                        Surge::GUI::toOSCase("Tempo Sync"), true, (p->temposync),
                        [this, p, control]() {
                            undoManager()->pushParameterChange(p->id, p, p->val);
                            p->temposync = !p->temposync;
                            synth->storage.getPatch().isDirty = true;
                            // output updated value to OSC
                            juceEditor->processor.paramChangeToListeners(
                                p, true, juceEditor->processor.SCT_EX_TEMPOSYNC,
                                (float)p->temposync, .0, .0, "");

                            if (p->temposync)
                            {
                                p->bound_value();
                            }
                            else if (control)
                            {
                                p->set_value_f01(control->getValue());
                            }

                            if (lfoDisplay)
                                lfoDisplay->repaint();
                            auto *css = dynamic_cast<Surge::Widgets::ModulatableControlInterface *>(
                                control);

                            if (css)
                            {
                                css->setTempoSync(p->temposync);
                                css->asJuceComponent()->repaint();
                            }
                        });

                    if (p->ctrlgroup == cg_ENV || p->ctrlgroup == cg_LFO)
                    {
                        std::string label;
                        std::string prefix;
                        std::string pars;
                        int a = p->ctrlgroup_entry;

                        if (p->ctrlgroup == cg_ENV)
                        {
                            prefix = (a == 0) ? "Amp EG" : "Filter EG";
                        }
                        else if (p->ctrlgroup == cg_LFO)
                        {
                            if (a >= ms_lfo1 && a <= ms_lfo1 + n_lfos_voice)
                            {
                                prefix = "Voice LFO " + std::to_string(a - ms_lfo1 + 1);
                            }
                            else if (a >= ms_slfo1 && a <= ms_slfo1 + n_lfos_scene)
                            {
                                prefix = "Scene LFO " + std::to_string(a - ms_slfo1 + 1);
                            }
                        }

                        bool setTSTo;

#if WINDOWS
                        pars = " parameters";
#else
                        pars = " Parameters";
#endif

                        if (p->temposync)
                        {
                            label =
                                Surge::GUI::toOSCase("Disable Tempo Sync for All ") + prefix + pars;
                            setTSTo = false;
                        }
                        else
                        {
                            label =
                                Surge::GUI::toOSCase("Enable Tempo Sync for All ") + prefix + pars;
                            setTSTo = true;
                        }

                        contextMenu.addItem(label, [this, p, setTSTo]() {
                            // There is surely a more efficient way but this is fine
                            for (auto iter = synth->storage.getPatch().param_ptr.begin();
                                 iter != synth->storage.getPatch().param_ptr.end(); iter++)
                            {
                                Parameter *pl = *iter;
                                if (pl->ctrlgroup_entry == p->ctrlgroup_entry &&
                                    pl->ctrlgroup == p->ctrlgroup && pl->can_temposync())
                                {
                                    pl->temposync = setTSTo;
                                    // output updated value to OSC
                                    juceEditor->processor.paramChangeToListeners(
                                        pl, true, juceEditor->processor.SCT_EX_TEMPOSYNC,
                                        (float)pl->temposync, .0, .0, "");

                                    if (setTSTo)
                                    {
                                        pl->bound_value();
                                    }
                                }
                            }

                            synth->storage.getPatch().isDirty = true;
                            synth->refresh_editor = true;
                        });
                    }
                }

                switch (p->ctrltype)
                {
                case ct_freq_audible_with_tunability:
                case ct_freq_audible_very_low_minval:
                    contextMenu.addItem(
                        Surge::GUI::toOSCase("Reset Filter Cutoff To Keytrack Root"),
                        [this, p, control] {
                            undoManager()->pushParameterChange(p->id, p, p->val);
                            auto kr = this->synth->storage.getPatch()
                                          .scene[current_scene]
                                          .keytrack_root.val.i;
                            p->set_value_f01(p->value_to_normalized(kr - 69));
                            control->setValue(p->get_value_f01());
                            auto mci = dynamic_cast<Surge::Widgets::ModulatableControlInterface *>(
                                control);
                            if (mci)
                            {
                                mci->setQuantitizedDisplayValue(p->get_value_f01());
                                synth->storage.getPatch().isDirty = true;
                            }
                            control->asJuceComponent()->repaint();
                        });
                    break;
                default:
                    break;
                }

                if (p->has_portaoptions())
                {
                    contextMenu.addSeparator();

                    Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(contextMenu,
                                                                                    "OPTIONS");

                    bool isChecked = p->porta_constrate;

                    contextMenu.addItem(Surge::GUI::toOSCase("Constant Rate"), true, isChecked,
                                        [this, p]() {
                                            undoManager()->pushParameterChange(p->id, p, p->val);
                                            p->porta_constrate = !p->porta_constrate;
                                            // output updated value to OSC
                                            juceEditor->processor.paramChangeToListeners(
                                                p, true, juceEditor->processor.SCT_EX_PORTA_CONRATE,
                                                (float)p->porta_constrate, .0, .0, "");
                                        });

                    isChecked = p->porta_gliss;

                    contextMenu.addItem(Surge::GUI::toOSCase("Glissando"), true, isChecked,
                                        [this, p]() {
                                            undoManager()->pushParameterChange(p->id, p, p->val);
                                            p->porta_gliss = !p->porta_gliss;
                                            // output updated value to OSC
                                            juceEditor->processor.paramChangeToListeners(
                                                p, true, juceEditor->processor.SCT_EX_PORTA_GLISS,
                                                (float)p->porta_gliss, .0, .0, "");
                                        });

                    isChecked = p->porta_retrigger;

                    contextMenu.addItem(Surge::GUI::toOSCase("Retrigger at Scale Degrees"), true,
                                        isChecked, [this, p]() {
                                            undoManager()->pushParameterChange(p->id, p, p->val);
                                            p->porta_retrigger = !p->porta_retrigger;
                                            // output updated value to OSC
                                            juceEditor->processor.paramChangeToListeners(
                                                p, true, juceEditor->processor.SCT_EX_PORTA_RETRIG,
                                                (float)p->porta_retrigger, .0, .0, "");
                                        });

                    Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(contextMenu,
                                                                                    "CURVE");

                    isChecked = (p->porta_curve == -1);

                    contextMenu.addItem(Surge::GUI::toOSCase("Logarithmic"), true, isChecked,
                                        [this, p]() {
                                            undoManager()->pushParameterChange(p->id, p, p->val);
                                            p->porta_curve = -1;
                                            // output updated value to OSC
                                            juceEditor->processor.paramChangeToListeners(
                                                p, true, juceEditor->processor.SCT_EX_PORTA_CURVE,
                                                (float)p->porta_curve, .0, .0, "");
                                        });

                    isChecked = (p->porta_curve == 0);

                    contextMenu.addItem(Surge::GUI::toOSCase("Linear"), true, isChecked,
                                        [this, p]() {
                                            undoManager()->pushParameterChange(p->id, p, p->val);
                                            p->porta_curve = 0;
                                            // output updated value to OSC
                                            juceEditor->processor.paramChangeToListeners(
                                                p, true, juceEditor->processor.SCT_EX_PORTA_CURVE,
                                                (float)p->porta_curve, .0, .0, "");
                                        });

                    isChecked = (p->porta_curve == 1);

                    contextMenu.addItem(Surge::GUI::toOSCase("Exponential"), true, isChecked,
                                        [this, p]() {
                                            undoManager()->pushParameterChange(p->id, p, p->val);
                                            p->porta_curve = 1;
                                            // output updated value to OSC
                                            juceEditor->processor.paramChangeToListeners(
                                                p, true, juceEditor->processor.SCT_EX_PORTA_CURVE,
                                                (float)p->porta_curve, .0, .0, "");
                                        });
                }

                if (p->has_deformoptions())
                {
                    switch (p->ctrltype)
                    {
                    case ct_freq_hpf:
                    {
                        contextMenu.addSeparator();

                        Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(contextMenu,
                                                                                        "SLOPE");

                        for (int i = 0; i < synth->n_hpBQ; i++)
                        {
                            std::string title = fmt::format("{:d} dB/oct", 12 * (i + 1));

                            bool isChecked = p->deform_type == i;

                            contextMenu.addItem(title, true, isChecked, [this, p, i]() {
                                undoManager()->pushParameterChange(p->id, p, p->val);
                                update_deform_type(p, i);
                                synth->storage.getPatch().isDirty = true;
                            });
                        }

                        break;
                    }
                    case ct_lfodeform:
                    {
                        auto q = modsource_editor[current_scene];
                        auto *lfodata =
                            &(synth->storage.getPatch().scene[current_scene].lfo[q - ms_lfo1]);

                        if (lt_num_deforms[lfodata->shape.val.i] > 1)
                        {
                            contextMenu.addSeparator();

                            Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(
                                contextMenu, "DEFORM");

                            for (int i = 0; i < lt_num_deforms[lfodata->shape.val.i]; i++)
                            {
                                std::string title = fmt::format("Type {:d}", (i + 1));
                                bool isChecked = p->deform_type == i;

                                contextMenu.addItem(
                                    Surge::GUI::toOSCase(title), true, isChecked,
                                    [this, isChecked, p, i]() {
                                        if (p->deform_type != i)
                                            undoManager()->pushParameterChange(p->id, p, p->val);
                                        update_deform_type(p, i);

                                        if (!isChecked)
                                        {
                                            synth->storage.getPatch().isDirty = true;
                                        }

                                        if (frame)
                                        {
                                            frame->repaint();
                                        }
                                    });
                            }
                        }

                        break;
                    }
                    case ct_modern_trimix:
                    {
                        contextMenu.addSeparator();

                        std::vector<int> waves = {ModernOscillator::mo_multitypes::momt_triangle,
                                                  ModernOscillator::mo_multitypes::momt_sine,
                                                  ModernOscillator::mo_multitypes::momt_square};

                        bool isChecked = false;

                        Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(contextMenu,
                                                                                        "WAVEFORM");

                        for (int m : waves)
                        {
                            isChecked = ((p->deform_type & 0x0F) == m);

                            contextMenu.addItem(
                                mo_multitype_names[m], true, isChecked, [this, isChecked, p, m]() {
                                    undoManager()->pushParameterChange(p->id, p, p->val);
                                    update_deform_type(p, (p->deform_type & 0xFFF0) | m);
                                    if (!isChecked)
                                    {
                                        synth->storage.getPatch().isDirty = true;
                                    }
                                    synth->refresh_editor = true;
                                });
                        }

                        int subosc = ModernOscillator::mo_submask::mo_subone;

                        isChecked = (p->deform_type & subosc);

                        Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(
                            contextMenu, "SUB-OSCILLATOR");

                        contextMenu.addItem(
                            Surge::GUI::toOSCase("Enabled"), true, isChecked, [p, subosc, this]() {
                                auto usubosc = subosc;
                                int usubskipsync =
                                    p->deform_type & ModernOscillator::mo_submask::mo_subskipsync;

                                if (p->deform_type & subosc)
                                {
                                    usubosc = 0;
                                }

                                undoManager()->pushParameterChange(p->id, p, p->val);
                                update_deform_type(p,
                                                   (p->deform_type & 0xF) | usubosc | usubskipsync);

                                synth->storage.getPatch().isDirty = true;
                                synth->refresh_editor = true;
                            });

                        int subskipsync = ModernOscillator::mo_submask::mo_subskipsync;

                        isChecked = (p->deform_type & subskipsync);

                        contextMenu.addItem(
                            Surge::GUI::toOSCase("Disable Hardsync"), true, isChecked,
                            [p, subskipsync, this]() {
                                auto usubskipsync = subskipsync;
                                int usubosc =
                                    p->deform_type & ModernOscillator::mo_submask::mo_subone;

                                if (p->deform_type & subskipsync)
                                {
                                    usubskipsync = 0;
                                }

                                undoManager()->pushParameterChange(p->id, p, p->val);
                                update_deform_type(p,
                                                   (p->deform_type & 0xF) | usubosc | usubskipsync);

                                synth->storage.getPatch().isDirty = true;
                                synth->refresh_editor = true;
                            });

                        break;
                    }
                    case ct_alias_mask:
                    {
                        contextMenu.addSeparator();

                        bool isChecked = (p->deform_type);

                        contextMenu.addItem(Surge::GUI::toOSCase("Ramp Not Masked Above Threshold"),
                                            true, isChecked, [p, this]() {
                                                undoManager()->pushParameterChange(p->id, p,
                                                                                   p->val);
                                                update_deform_type(p, !p->deform_type);
                                                synth->storage.getPatch().isDirty = true;
                                                synth->refresh_editor = true;
                                            });

                        break;
                    }
                    case ct_tape_drive:
                    {
                        contextMenu.addSeparator();

                        std::vector<std::string> tapeHysteresisModes = {"Normal", "Medium", "High",
                                                                        "Very High"};

                        Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(
                            contextMenu, "PRECISION");

                        for (int i = 0; i < tapeHysteresisModes.size(); ++i)
                        {
                            bool isChecked = p->deform_type == i;

                            contextMenu.addItem(Surge::GUI::toOSCase(tapeHysteresisModes[i]), true,
                                                isChecked, [this, isChecked, p, i]() {
                                                    if (p->deform_type != i)
                                                        undoManager()->pushParameterChange(p->id, p,
                                                                                           p->val);
                                                    update_deform_type(p, i);
                                                    if (!isChecked)
                                                    {
                                                        synth->storage.getPatch().isDirty = true;
                                                    }
                                                });
                        }

                        break;
                    }
                    case ct_dly_fb_clippingmodes:
                    {
                        contextMenu.addSeparator();

                        Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(contextMenu,
                                                                                        "LIMITING");

                        std::vector<std::string> fbClipModes = {
                            "Disabled (DANGER!)", "Soft Clip (cubic)", "Soft Clip (tanh)",
                            "Hard Clip at 0 dBFS", "Hard Clip at +18 dBFS"};

                        for (int i = 0; i < fbClipModes.size(); ++i)
                        {
                            bool isChecked = p->deform_type == i;

                            contextMenu.addItem(Surge::GUI::toOSCase(fbClipModes[i]), true,
                                                isChecked, [this, isChecked, p, i]() {
                                                    if (p->deform_type != i)
                                                        undoManager()->pushParameterChange(p->id, p,
                                                                                           p->val);
                                                    update_deform_type(p, i);
                                                    if (!isChecked)
                                                    {
                                                        synth->storage.getPatch().isDirty = true;
                                                    }
                                                });
                        }

                        contextMenu.addSeparator();

                        break;
                    }
                    case ct_percent_with_string_deform_hook:
                    case ct_percent_bipolar_with_string_filter_hook:
                    {
                        auto addDef = [this, p,
                                       &contextMenu](StringOscillator::deform_modes dm,
                                                     StringOscillator::deform_modes allMode,
                                                     const std::string &lb) {
                            auto ticked = p->deform_type & dm;

                            contextMenu.addItem(
                                Surge::GUI::toOSCase(lb), true, ticked,
                                [this, p, allMode, dm, ticked] {
                                    if (!ticked)
                                    {
                                        undoManager()->pushParameterChange(p->id, p, p->val);
                                        update_deform_type(p, (p->deform_type & ~allMode) | dm);
                                        synth->storage.getPatch().isDirty = true;
                                        frame->repaint();
                                    }
                                });
                        };

                        if (p->ctrltype == ct_percent_with_string_deform_hook)
                        {
                            contextMenu.addSeparator();

                            Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(
                                contextMenu, "OVERSAMPLING");

                            addDef(StringOscillator::os_onex, StringOscillator::os_all, "1x");
                            addDef(StringOscillator::os_twox, StringOscillator::os_all, "2x");

                            contextMenu.addSeparator();

                            Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(
                                contextMenu, "INTERPOLATION");

                            addDef(StringOscillator::interp_zoh, StringOscillator::interp_all,
                                   Surge::GUI::toOSCase("Zero Order Hold"));
                            addDef(StringOscillator::interp_lin, StringOscillator::interp_all,
                                   "Linear");
                            addDef(StringOscillator::interp_sinc, StringOscillator::interp_all,
                                   "Sinc");
                        }

                        if (p->ctrltype == ct_percent_bipolar_with_string_filter_hook)
                        {
                            contextMenu.addSeparator();

                            Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(
                                contextMenu, "STIFFNESS FILTER");

                            addDef(StringOscillator::filter_fixed, StringOscillator::filter_all,
                                   "Static");
                            addDef(StringOscillator::filter_keytrack, StringOscillator::filter_all,
                                   "Keytracked");
                            addDef(StringOscillator::filter_compensate,
                                   StringOscillator::filter_all,
                                   Surge::GUI::toOSCase("Keytracked and Pitch Compensated"));
                        }

                        break;
                    }
                    case ct_noise_color:
                    {
                        if (synth->storage.getPatch()
                                .scene[current_scene]
                                .filterblock_configuration.val.i == fc_wide)
                        {
                            contextMenu.addSeparator();
                            auto dt = p->deform_type;

                            Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(
                                contextMenu, "NOISE GENERATOR MODE");

                            contextMenu.addItem(
                                "Mono", true, dt == NoiseColorChannels::MONO, [this, p]() {
                                    undoManager()->pushParameterChange(p->id, p, p->val);
                                    update_deform_type(p, NoiseColorChannels::MONO);
                                    synth->storage.getPatch().isDirty = true;
                                    frame->repaint();
                                });
                            contextMenu.addItem(
                                "Stereo", true, dt == NoiseColorChannels::STEREO, [this, p]() {
                                    undoManager()->pushParameterChange(p->id, p, p->val);
                                    update_deform_type(p, NoiseColorChannels::STEREO);
                                    synth->storage.getPatch().isDirty = true;
                                    frame->repaint();
                                });
                        }

                        break;
                    }
                    case ct_amplitude_ringmod:
                    {
                        contextMenu.addSeparator();

                        auto dt = p->deform_type;
                        auto addEntry = [this, dt, p](juce::PopupMenu &menu, CombinatorMode cxm) {
                            menu.addItem(Surge::GUI::toOSCase(combinator_mode_names[cxm]), true,
                                         dt == cxm, [this, p, cxm]() {
                                             undoManager()->pushParameterChange(p->id, p, p->val);
                                             update_deform_type(p, cxm);
                                             synth->storage.getPatch().isDirty = true;
                                             frame->repaint();
                                         });
                        };

                        Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(
                            contextMenu, "COMBINATOR MODE");

                        addEntry(contextMenu, cxm_ring);
                        addEntry(contextMenu, cxm_cxor43_0);

                        contextMenu.addItem(
                            -1, Surge::GUI::toOSCase("Scale-Invariant Linear Modulation:"), false,
                            false);

                        auto subMenu = juce::PopupMenu();

                        addEntry(subMenu, cxm_cxor43_1);
                        addEntry(subMenu, cxm_cxor43_2);
                        addEntry(subMenu, cxm_cxor43_3_legacy);
                        addEntry(subMenu, cxm_cxor43_4_legacy);
                        addEntry(subMenu, cxm_cxor43_3);
                        addEntry(subMenu, cxm_cxor43_4);

                        contextMenu.addSubMenu(Surge::GUI::toOSCase("4 Gradients"), subMenu);

                        subMenu.clear();

                        addEntry(subMenu, cxm_cxor93_0);
                        addEntry(subMenu, cxm_cxor93_1);
                        addEntry(subMenu, cxm_cxor93_2);
                        addEntry(subMenu, cxm_cxor93_3);
                        addEntry(subMenu, cxm_cxor93_4);

                        contextMenu.addSubMenu(Surge::GUI::toOSCase("9 Gradients"), subMenu);
                        break;
                    }
                    case ct_bonsai_bass_boost:
                    {
                        contextMenu.addSeparator();
                        auto dt = p->deform_type;

                        Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(contextMenu,
                                                                                        "ROUTING");

                        contextMenu.addItem("Mono", true, dt == 1, [this, p]() {
                            undoManager()->pushParameterChange(p->id, p, p->val);
                            update_deform_type(p, 1);
                            synth->storage.getPatch().isDirty = true;
                            frame->repaint();
                        });
                        contextMenu.addItem("Stereo", true, dt == 0, [this, p]() {
                            undoManager()->pushParameterChange(p->id, p, p->val);
                            update_deform_type(p, 0);
                            synth->storage.getPatch().isDirty = true;
                            frame->repaint();
                        });
                        contextMenu.addSeparator();

                        break;
                    }
                    case ct_osc_feedback_negative:
                    {
                        contextMenu.addSeparator();
                        auto dt = p->deform_type;

                        Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(
                            contextMenu, "FEEDBACK TYPE");

                        contextMenu.addItem("Surge", true, dt == 0, [this, p]() {
                            undoManager()->pushParameterChange(p->id, p, p->val);
                            update_deform_type(p, 0);
                            synth->storage.getPatch().isDirty = true;
                            frame->repaint();
                        });
                        contextMenu.addItem("Vintage FM", true, dt == 1, [this, p]() {
                            undoManager()->pushParameterChange(p->id, p, p->val);
                            update_deform_type(p, 1);
                            synth->storage.getPatch().isDirty = true;
                            frame->repaint();
                        });
                        contextMenu.addSeparator();

                        break;
                    }
                    case ct_envtime_deformable:
                    {
                        contextMenu.addSeparator();

                        auto dt = p->deform_type;

                        contextMenu.addItem(Surge::GUI::toOSCase("Freeze Release at Sustain Level"),
                                            true, dt > 0, [this, p]() {
                                                undoManager()->pushParameterChange(p->id, p,
                                                                                   p->val);
                                                update_deform_type(p, !p->deform_type);
                                                synth->storage.getPatch().isDirty = true;
                                                frame->repaint();
                                            });

                        contextMenu.addSeparator();

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
                        case ct_freq_audible_very_low_minval:
                        {
                            auto hasmts = synth->storage.oddsound_mts_client &&
                                          synth->storage.oddsound_mts_active_as_client;

                            std::string tuningmode = hasmts ? "MTS" : "SCL/KBM";

                            txt = "Apply " + tuningmode + " Tuning to Filter Cutoff";
                            visible =
                                synth->storage.tuningApplicationMode == SurgeStorage::RETUNE_ALL;
                            break;
                        }
                        case ct_pitch_extendable_very_low_minval:
                        {
                            // yep folks - we're abusing extend_range to do absolutable! :)
                            txt = "Absolute";
                            break;
                        }
                        case ct_percent_oscdrift:
                            txt = "Randomize Initial Drift Phase";
                            break;
                        case ct_twist_aux_mix:
                            txt = "Pan Main and Auxiliary Signals";
                            break;
                        case ct_countedset_percent_extendable:
                            txt = "Continuous Morph";
                            break;
                        default:
                            break;
                        }

                        if (visible)
                        {
                            bool isChecked = p->extend_range;

                            contextMenu.addItem(
                                Surge::GUI::toOSCase(txt), enable, isChecked, [this, p]() {
                                    undoManager()->pushParameterChange(p->id, p, p->val);
                                    p->set_extend_range(!p->extend_range);
                                    synth->storage.getPatch().isDirty = true;
                                    synth->refresh_editor = true;

                                    // output updated value to OSC
                                    juceEditor->processor.paramChangeToListeners(
                                        p, true, juceEditor->processor.SCT_EX_EXTENDRANGE,
                                        (float)p->extend_range, .0, .0, "");
                                });
                        }
                    }
                }

                if (p->can_be_absolute())
                {
                    bool isChecked = p->absolute;

                    contextMenu.addItem("Absolute", true, isChecked, [this, p]() {
                        undoManager()->pushParameterChange(p->id, p, p->val);

                        p->absolute = !p->absolute;
                        synth->storage.getPatch().isDirty = true;

                        // FIXME : What's a better aprpoach?
                        if (p->ctrltype == ct_fmratio)
                        {
                            char txt[TXT_SIZE];
                            std::string ntxt;
                            memset(txt, 0, TXT_SIZE);
                            strxcpy(txt, p->get_name(), TXT_SIZE);

                            if (p->absolute)
                            {
                                ntxt = fmt::format("M{:c} Frequency", txt[1]);
                            }
                            else
                            {
                                // Ladies and gentlemen, MC Ratio!
                                ntxt = fmt::format("M{:c} Ratio", txt[1]);
                            }

                            p->set_name(ntxt.c_str());
                            synth->refresh_editor = true;
                        }

                        // output updated value to OSC
                        juceEditor->processor.paramChangeToListeners(
                            p, true, juceEditor->processor.SCT_EX_ABSOLUTE, (float)p->absolute, .0,
                            .0, "");
                    });
                }

                if (p->can_deactivate() || p->get_primary_deactivation_driver())
                {
                    auto q = p->get_primary_deactivation_driver();

                    if (!q)
                    {
                        q = p;
                    }

                    std::string txt;

                    bool isChecked = q->deactivated;

                    switch (q->ctrltype)
                    {
                    case ct_envtime_linkable_delay:
                        txt = Surge::GUI::toOSCase("Link to Left Channel");
                        break;
                    case ct_amplitude_clipper:
                        txt = Surge::GUI::toOSCase(
                            fmt::format("Mute Scene {:c}", 'A' + (p->scene - 1)));
                        break;
                    default:
                    {
                        isChecked = !isChecked;
                        txt = Surge::GUI::toOSCase("Enabled");
                    }
                    break;
                    }

                    contextMenu.addSeparator();

                    contextMenu.addItem(txt, true, isChecked, [this, q, p]() {
                        undoManager()->pushParameterChange(q->id, q, q->val);

                        q->deactivated = !q->deactivated;
                        synth->storage.getPatch().isDirty = true;
                        synth->refresh_editor = true;

                        // output updated value to OSC
                        juceEditor->processor.paramChangeToListeners(
                            p, true, juceEditor->processor.SCT_EX_ENABLE, (float)!p->deactivated,
                            .0, .0, "");
                    });
                }

                int n_ms = 0;

                for (int ms = 1; ms < n_modsources; ms++)
                {
                    std::vector<int> scenes;

                    int startScene = current_scene, endScene = current_scene + 1;

                    if (p->scene == 0)
                    {
                        startScene = 0;
                        endScene = n_scenes;
                    }

                    for (int sc = startScene; sc < endScene; ++sc)
                    {
                        if (synth->isAnyActiveModulation(ptag, (modsources)ms, sc))
                        {
                            n_ms++;
                        }
                    }
                }

                if (n_ms)
                {
                    contextMenu.addSeparator();
                    Surge::Widgets::MenuCenteredBoldLabel::addToMenu(contextMenu, "MODULATIONS");

                    for (int k = 1; k < n_modsources; k++)
                    {
                        modsources ms = (modsources)k;
                        int startScene = current_scene, endScene = current_scene + 1;
                        bool showScene = false;

                        if (p->scene == 0)
                        {
                            startScene = 0;
                            endScene = n_scenes;
                            showScene = true;
                        }

                        for (int sc = startScene; sc < endScene; ++sc)
                        {
                            auto indices = synth->getModulationIndicesBetween(ptag, ms, sc);

                            for (auto modidx : indices)
                            {
                                if (synth->isActiveModulation(ptag, ms, sc, modidx))
                                {
                                    char modtxt[TXT_SIZE];

                                    p->get_display_of_modulation_depth(
                                        modtxt, synth->getModDepth(ptag, ms, sc, modidx),
                                        synth->isBipolarModulation(ms), Parameter::Menu);

                                    std::string srctxt = ModulatorName::modulatorNameWithIndex(
                                        &synth->storage, sc, ms, modidx, true, showScene);

                                    auto comp =
                                        std::make_unique<Surge::Widgets::ModMenuCustomComponent>(
                                            &synth->storage, srctxt, modtxt,
                                            [this, ptag, p, sc, bvf, ms, modidx](
                                                Surge::Widgets::ModMenuCustomComponent::OpType ot) {
                                                switch (ot)
                                                {
                                                case Surge::Widgets::ModMenuCustomComponent::EDIT:
                                                {
                                                    this->promptForUserValueEntry(p, bvf, ms, sc,
                                                                                  modidx);
                                                }
                                                break;
                                                case Surge::Widgets::ModMenuCustomComponent::CLEAR:
                                                {
                                                    pushModulationToUndoRedo(
                                                        ptag, (modsources)ms, sc, modidx,
                                                        Surge::GUI::UndoManager::UNDO);
                                                    synth->clearModulation(ptag, (modsources)ms, sc,
                                                                           modidx, false);
                                                    refresh_mod();
                                                    synth->storage.getPatch().isDirty = true;
                                                    synth->refresh_editor = true;
                                                }
                                                break;
                                                case Surge::Widgets::ModMenuCustomComponent::MUTE:
                                                {
                                                    pushModulationToUndoRedo(
                                                        ptag, (modsources)ms, sc, modidx,
                                                        Surge::GUI::UndoManager::UNDO);
                                                    auto is = synth->isModulationMuted(
                                                        ptag, (modsources)ms, sc, modidx);

                                                    synth->muteModulation(ptag, (modsources)ms, sc,
                                                                          modidx, !is);
                                                    refresh_mod();
                                                    synth->storage.getPatch().isDirty = true;
                                                    synth->refresh_editor = true;
                                                }
                                                break;
                                                }
                                            });
                                    auto comptitle = comp->getTitle();
                                    comp->setSkin(currentSkin, bitmapStore);
                                    comp->setIsMuted(
                                        synth->isModulationMuted(ptag, (modsources)ms, sc, modidx));
                                    auto pm = comp->createAccessibleSubMenu();
                                    contextMenu.addCustomItem(-1, std::move(comp),
                                                              pm.get() ? std::move(pm) : nullptr,
                                                              comptitle);
                                }
                            }
                        }
                    }

                    if (n_ms > 1)
                    {
                        auto cb = [this,
                                   ptag](Surge::Widgets::ModMenuForAllComponent::AllAction action) {
                            switch (action)
                            {
                            case Surge::Widgets::ModMenuForAllComponent::CLEAR:
                            {
                                for (int ms = 1; ms < n_modsources; ms++)
                                {
                                    for (int sc = 0; sc < n_scenes; ++sc)
                                    {
                                        auto indices = synth->getModulationIndicesBetween(
                                            ptag, (modsources)ms, sc);
                                        for (auto idx : indices)
                                        {
                                            pushModulationToUndoRedo(ptag, (modsources)ms, sc, idx,
                                                                     Surge::GUI::UndoManager::UNDO);

                                            synth->clearModulation(ptag, (modsources)ms, sc, idx,
                                                                   false);
                                        }
                                    }
                                }
                            }
                            break;
                            case Surge::Widgets::ModMenuForAllComponent::UNMUTE:
                            case Surge::Widgets::ModMenuForAllComponent::MUTE:
                            {
                                bool state =
                                    (action == Surge::Widgets::ModMenuForAllComponent::MUTE);
                                for (int ms = 1; ms < n_modsources; ms++)
                                {
                                    for (int sc = 0; sc < n_scenes; ++sc)
                                    {
                                        auto indices = synth->getModulationIndicesBetween(
                                            ptag, (modsources)ms, sc);
                                        for (auto idx : indices)
                                            synth->muteModulation(ptag, (modsources)ms, sc, idx,
                                                                  state);
                                    }
                                }
                            }
                            break;
                            }
                            refresh_mod();
                            synth->refresh_editor = true;
                        };
                        auto allThree = std::make_unique<Surge::Widgets::ModMenuForAllComponent>(
                            &synth->storage, cb);
                        allThree->setSkin(currentSkin, bitmapStore);
                        auto comptitle = allThree->getTitle();
                        auto pm = allThree->createAccessibleSubMenu();
                        contextMenu.addCustomItem(-1, std::move(allThree),
                                                  pm.get() ? std::move(pm) : nullptr, comptitle);
                    }
                }

                contextMenu.addSeparator();

                // see if we have any modulators that are unassigned,
                // then create "Add Modulation from..." menu
                if (n_ms != n_modsources)
                {
                    auto addModSub = juce::PopupMenu();

                    auto addMacroSub = juce::PopupMenu();
                    auto addVLFOSub = juce::PopupMenu();
                    auto addSLFOSub = juce::PopupMenu();
                    auto addEGSub = juce::PopupMenu();
                    auto addMIDISub = juce::PopupMenu();
                    auto addMiscSub = juce::PopupMenu();

                    for (int k = 1; k < n_modsources; k++)
                    {
                        modsources ms = (modsources)modsource_display_order[k];

                        bool isAnyOn = synth->isAnyActiveModulation(ptag, ms, current_scene);
                        bool isValud = synth->isValidModulation(ptag, ms);
                        bool isIndexed = synth->supportsIndexedModulator(current_scene, ms);

                        if ((!synth->isAnyActiveModulation(ptag, ms, current_scene) || isIndexed) &&
                            synth->isValidModulation(ptag, ms))
                        {
                            auto modName = ModulatorName::modulatorName(&synth->storage, ms, false,
                                                                        current_scene);

                            auto *popMenu = &addMIDISub;
                            int modIdx = -1;

                            // TODO FIXME: Try not to be gross!
                            if (ms >= ms_ctrl1 && ms <= ms_ctrl1 + n_customcontrollers - 1)
                            {
                                popMenu = &addMacroSub;
                            }
                            else if (ms >= ms_lfo1 && ms <= ms_lfo1 + n_lfos_voice - 1)
                            {
                                popMenu = &addVLFOSub;
                                modIdx = ms;
                            }
                            else if (ms >= ms_slfo1 && ms <= ms_slfo1 + n_lfos_scene - 1)
                            {
                                popMenu = &addSLFOSub;
                                modIdx = ms;
                            }
                            else if (ms >= ms_ampeg && ms <= ms_filtereg)
                            {
                                popMenu = &addEGSub;
                            }
                            else if (ms >= ms_random_bipolar && ms <= ms_alternate_unipolar)
                            {
                                popMenu = &addMiscSub;
                            }

                            if (isIndexed)
                            {
                                int maxidx = synth->getMaxModulationIndex(current_scene, ms);
                                auto subm = juce::PopupMenu();

                                for (int i = 0; i < maxidx; ++i)
                                {
                                    auto subn = ModulatorName::modulatorNameWithIndex(
                                        &synth->storage, current_scene, ms, i, false, false);

                                    subm.addItem(subn, [this, p, bvf, ms, modIdx, i]() {
                                        this->promptForUserValueEntry(p, bvf, ms, current_scene, i);

                                        typeinParamEditor->activateModsourceAfterEnter = modIdx;
                                    });
                                }

                                popMenu->addSubMenu(modName, subm);
                            }
                            else
                            {
                                popMenu->addItem(modName, [this, p, bvf, ms, modIdx]() {
                                    this->promptForUserValueEntry(p, bvf, ms, current_scene, 0);

                                    typeinParamEditor->activateModsourceAfterEnter = modIdx;
                                });
                            }
                        }
                    }

                    if (addMacroSub.getNumItems() > 0)
                    {
                        addModSub.addSubMenu("Macros", addMacroSub);
                    }

                    if (addVLFOSub.getNumItems() > 0)
                    {
                        addModSub.addSubMenu("Voice LFOs", addVLFOSub);
                    }

                    if (addSLFOSub.getNumItems() > 0)
                    {
                        addModSub.addSubMenu("Scene LFOs", addSLFOSub);
                    }

                    if (addEGSub.getNumItems() > 0)
                    {
                        addModSub.addSubMenu("Envelopes", addEGSub);
                    }

                    if (addMIDISub.getNumItems() > 0)
                    {
                        addModSub.addSubMenu("MIDI", addMIDISub);
                    }

                    if (addMiscSub.getNumItems() > 0)
                    {
                        addModSub.addSubMenu("Internal", addMiscSub);
                    }

                    if (addModSub.getNumItems() > 0)
                    {
                        contextMenu.addSubMenu(Surge::GUI::toOSCase("Add Modulation from"),
                                               addModSub);
                    }
                }

                contextMenu.addSeparator();

                createMIDILearnMenuEntries(contextMenu, param_cc, p->id, control);

            } // end vt_float if statement

            if (p->ctrltype == ct_amplitude_clipper)
            {
                std::string sc = std::string("Scene ") + (char)('A' + current_scene);

                contextMenu.addSeparator();

                // FIXME - add unified menu here
                bool isChecked = (synth->storage.sceneHardclipMode[current_scene] ==
                                  SurgeStorage::BYPASS_HARDCLIP);

                contextMenu.addItem(Surge::GUI::toOSCase(sc + " Hard Clip Disabled"), true,
                                    isChecked, [this, isChecked]() {
                                        synth->storage.sceneHardclipMode[current_scene] =
                                            SurgeStorage::BYPASS_HARDCLIP;
                                        if (!isChecked)
                                            synth->storage.getPatch().isDirty = true;
                                    });

                isChecked = (synth->storage.sceneHardclipMode[current_scene] ==
                             SurgeStorage::HARDCLIP_TO_0DBFS);

                contextMenu.addItem(Surge::GUI::toOSCase(sc + " Hard Clip at 0 dBFS"), true,
                                    isChecked, [this, isChecked]() {
                                        synth->storage.sceneHardclipMode[current_scene] =
                                            SurgeStorage::HARDCLIP_TO_0DBFS;
                                        if (!isChecked)
                                            synth->storage.getPatch().isDirty = true;
                                    });

                isChecked = (synth->storage.sceneHardclipMode[current_scene] ==
                             SurgeStorage::HARDCLIP_TO_18DBFS);

                contextMenu.addItem(Surge::GUI::toOSCase(sc + " Hard Clip at +18 dBFS"), true,
                                    isChecked, [this, isChecked]() {
                                        synth->storage.sceneHardclipMode[current_scene] =
                                            SurgeStorage::HARDCLIP_TO_18DBFS;
                                        if (!isChecked)
                                            synth->storage.getPatch().isDirty = true;
                                    });
            }

            if (p->ctrltype == ct_decibel_attenuation_clipper)
            {
                contextMenu.addSeparator();

                // FIXME - add unified menu here
                bool isChecked = (synth->storage.hardclipMode == SurgeStorage::BYPASS_HARDCLIP);

                contextMenu.addItem(Surge::GUI::toOSCase("Global Hard Clip Disabled"), true,
                                    isChecked, [this, isChecked]() {
                                        synth->storage.hardclipMode = SurgeStorage::BYPASS_HARDCLIP;
                                        if (!isChecked)
                                            synth->storage.getPatch().isDirty = true;
                                    });

                isChecked = (synth->storage.hardclipMode == SurgeStorage::HARDCLIP_TO_0DBFS);

                contextMenu.addItem(Surge::GUI::toOSCase("Global Hard Clip at 0 dBFS"), true,
                                    isChecked, [this, isChecked]() {
                                        synth->storage.hardclipMode =
                                            SurgeStorage::HARDCLIP_TO_0DBFS;
                                        if (!isChecked)
                                            synth->storage.getPatch().isDirty = true;
                                    });

                isChecked = (synth->storage.hardclipMode == SurgeStorage::HARDCLIP_TO_18DBFS);

                contextMenu.addItem(Surge::GUI::toOSCase("Global Hard Clip at +18 dBFS"), true,
                                    isChecked, [this, isChecked]() {
                                        synth->storage.hardclipMode =
                                            SurgeStorage::HARDCLIP_TO_18DBFS;
                                        if (!isChecked)
                                            synth->storage.getPatch().isDirty = true;
                                    });
            }

            if (synth->storage.oscReceiving)
            {
                contextMenu.addSeparator();

                auto oscName = p->get_osc_name();

                auto i =
                    juce::PopupMenu::Item(fmt::format("OSC: {}", oscName))
                        .setAction(
                            [oscName]() { juce::SystemClipboard::copyTextToClipboard(oscName); })
                        .setColour(currentSkin->getColor(Colors::PopupMenu::Text).withAlpha(0.75f));

                contextMenu.addItem(i);
            }

            auto jpm = juceEditor->hostMenuFor(p);

            if (jpm.getNumItems() > 0)
            {
                contextMenu.addSeparator();

                juce::PopupMenu::MenuItemIterator iterator(jpm);

                while (iterator.next())
                {
                    contextMenu.addItem(iterator.getItem());
                }
            }

            contextMenu.showMenuAsync(popupMenuOptions(control->asJuceComponent(), false),
                                      Surge::GUI::makeEndHoverCallback(control));
            return 1;
        }
        // reset to default value
        else if (isDoubleClickEvent)
        {
            if (synth->isValidModulation(ptag, modsource) && mod_editor)
            {
                auto *cms = gui_modsrc[modsource].get();
                modsources thisms = modsource;

                auto use_scene = 0;

                if (this->synth->isModulatorDistinctPerScene(thisms))
                {
                    use_scene = current_scene;
                }

                pushModulationToUndoRedo(ptag, (modsources)thisms, use_scene, modsource_index,
                                         Surge::GUI::UndoManager::UNDO);

                synth->clearModulation(ptag, thisms, use_scene, modsource_index, false);
                auto ctrms = dynamic_cast<Surge::Widgets::ModulatableControlInterface *>(control);
                jassert(ctrms);

                if (ctrms)
                {
                    ctrms->setModValue(
                        synth->getModDepth01(p->id, thisms, use_scene, modsource_index));
                    ctrms->setModulationState(
                        synth->isModDestUsed(p->id),
                        synth->isActiveModulation(p->id, thisms, use_scene, modsource_index));
                    ctrms->setIsModulationBipolar(synth->isBipolarModulation(thisms));
                }

                synth->storage.getPatch().isDirty = true;

                oscWaveform->repaint();

                return 0;
            }
            else
            {
                switch (p->ctrltype)
                {
                case ct_lfotype:
                    /*
                    ** This code resets you to default if you double-click on control,
                    ** but on the LFO type widget this is undesirable; it means if you
                    *accidentally
                    ** Control-click on step sequencer, say, you go back to Sine and lose your
                    ** edits. So suppress it!
                    */
                    break;
                case ct_freq_audible_with_tunability:
                case ct_freq_audible_very_low_minval:
                {
                    if (p->extend_range || button.isAltDown())
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
                    {
                        bvf->repaint();
                    }

                    synth->storage.getPatch().isDirty = true;

                    return 0;
                }
                default:
                {
                    p->set_value_f01(p->get_default_value_f01());
                    control->setValue(p->get_value_f01());

                    if (oscWaveform && (p->ctrlgroup == cg_OSC))
                    {
                        oscWaveform->repaint();
                    }

                    if (lfoDisplay && (p->ctrlgroup == cg_LFO))
                    {
                        lfoDisplay->repaint();
                    }

                    if (bvf)
                    {
                        bvf->repaint();
                    }

                    synth->storage.getPatch().isDirty = true;

                    return 0;
                }
                }
            }
        }
        // exclusive mute/solo in the mixer
        else if (button.isCommandDown() || button.isMiddleButtonDown())
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

                synth->storage.getPatch().isDirty = true;
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

                synth->storage.getPatch().isDirty = true;
                synth->refresh_editor = true;
            }
            else
            {
                p->bound_value();
            }
        }
    }

    return 0;
}

void SurgeGUIEditor::update_deform_type(Parameter *p, int type)
{
    p->deform_type = type;
    juceEditor->processor.paramChangeToListeners(p, true, juceEditor->processor.SCT_EX_DEFORM,
                                                 (float)type, .0, .0, "");
}

void SurgeGUIEditor::valueChanged(Surge::GUI::IComponentTagValue *control)
{
    if (!frame)
    {
        return;
    }

    if (!editor_open)
    {
        return;
    }

    juce::Rectangle<int> viewSize;
    auto bvf = control->asJuceComponent();

    SelfModulationGuard modGuard(this);

    if (bvf)
    {
        viewSize = bvf->getBounds();
    }
    else
    {
        jassert(false);
        std::cout << "WARNING: Control not castable to a IBaseView.\n"
                  << "Is your JUCE-only inheritance hierarchy missing the tag class?" << std::endl;
    }

    long tag = control->getTag();

    if (typeinParamEditor->isVisible())
    {
        typeinParamEditor->setVisible(false);
        frame->removeChildComponent(typeinParamEditor.get());
    }

    if (tag == tag_status_zoom)
    {
        control->setValue(0);
        juce::Point<int> where = control->asJuceComponent()->getBounds().getBottomLeft();
        showZoomMenu(where, control);
        return;
    }

    if (tag == tag_action_undo)
    {
        undoManager()->undo();
        juce::Timer::callAfterDelay(25, [control]() { control->setValue(0); });
        return;
    }

    if (tag == tag_action_redo)
    {
        undoManager()->redo();
        juce::Timer::callAfterDelay(25, [control]() { control->setValue(0); });
        return;
    }

    if (tag == tag_lfo_menu)
    {
        control->setValue(0);
        juce::Point<int> where = control->asJuceComponent()->getBounds().getBottomLeft();
        showLfoMenu(where, control);
        return;
    }

    if (tag == tag_status_mpe)
    {
        toggleMPE();
        return;
    }
    if (tag == tag_status_tune)
    {
        if (synth->storage.isStandardTuningAndHasNoToggle())
        {
            juce::Point<int> where = control->asJuceComponent()->getBounds().getBottomLeft();
            showTuningMenu(where, control);
        }
        else
        {
            toggleTuning();
        }

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
                showOverlay(SurgeGUIEditor::MSEG_EDITOR);
            }
            else if (lfodata->shape.val.i == lt_formula)
            {
                showOverlay(FORMULA_EDITOR);
            }
        }
        else
        {
            closeOverlay(SurgeGUIEditor::MSEG_EDITOR);
            closeOverlay(FORMULA_EDITOR);
        }
        return;
    }

    if ((tag >= tag_mod_source0) && (tag < tag_mod_source_end))
    {
        auto cms = dynamic_cast<Surge::Widgets::ModulationSourceButton *>(control);
        if (cms->getMouseMode() == Surge::Widgets::ModulationSourceButton::DRAG_VALUE)
        {
            int t = (tag - tag_mod_source0);
            auto cmsrc = ((ControllerModulationSource *)synth->storage.getPatch()
                              .scene[current_scene]
                              .modsources[t]);
            undoManager()->pushMacroChange(t - ms_ctrl1, cmsrc->get_target01(0));
            cmsrc->set_target01(control->getValue(), false);
            synth->getParent()->surgeMacroUpdated(t - ms_ctrl1, control->getValue());

            lfoDisplay->repaint();
            return;
        }
        else
        {
            int state = cms->getState();
            modsources newsource = cms->getCurrentModSource();
            int newindex = cms->getCurrentModIndex();

            bool updated = false;
            bool change_mod_view = true;
            if (cms->getMouseMode() == Surge::Widgets::ModulationSourceButton::CLICK)
            {
                updated = true;
                switch (state & 3)
                {
                case 0:
                    modsource = newsource;
                    modsource_index = newindex;
                    if (mod_editor)
                        mod_editor = true;
                    else
                        mod_editor = false;
                    queue_refresh = true;
                    needsModUpdate = true;
                    break;
                case 1:
                    modsource = newsource;
                    modsource_index = newindex;
                    mod_editor = true;
                    needsModUpdate = true;
                    break;
                case 2:
                    modsource = newsource;
                    modsource_index = newindex;
                    mod_editor = false;
                    needsModUpdate = true;
                    break;
                };
            }
            else if (cms->getMouseMode() ==
                     Surge::Widgets::ModulationSourceButton::CLICK_TOGGLE_ARM)
            {
                modsource = newsource;
                modsource_index = newindex;
                if ((state & 3) == 0)
                {
                    mod_editor = true;
                    queue_refresh = true;
                }
                else
                {
                    mod_editor = !mod_editor;
                }
                refresh_mod();
            }
            else if (cms->getMouseMode() ==
                     Surge::Widgets::ModulationSourceButton::CLICK_SELECT_ONLY)
            {
                modsource = newsource;
                modsource_index = newindex;
                mod_editor = false;
                if ((state & 3) == 0)
                    queue_refresh = true;
                refresh_mod();

                if (lfoRateSlider)
                    lfoRateSlider->grabKeyboardFocus();
            }
            else if (cms->getMouseMode() == Surge::Widgets::ModulationSourceButton::HAMBURGER)
            {
                updated = true;
                modsource = newsource;
                modsource_index = newindex;
                refresh_mod();
            }
            else if (cms->getMouseMode() == Surge::Widgets::ModulationSourceButton::CLICK_ARROW ||
                     cms->getMouseMode() == Surge::Widgets::ModulationSourceButton::CTRL_CLICK)
            {
                change_mod_view = true;
                refresh_mod();
            }

            if (updated && lfoDisplay && lfoDisplay->modsource == modsource)
            {
                lfoDisplay->setModIndex(modsource_index);
                lfoDisplay->repaint();
            }

            if (isLFO(newsource) && change_mod_view)
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

                    bool hadExtendedOverlay = false;
                    bool wasTornOut = false;
                    auto wasTornOutTag = MSEG_EDITOR;
                    juce::Point<int> tearOutLoc;
                    for (auto otag : {MSEG_EDITOR, FORMULA_EDITOR})
                    {
                        if (isAnyOverlayPresent(otag))
                        {
                            auto c = getOverlayWrapperIfOpen(otag);
                            if (c)
                            {
                                wasTornOut = c->isTornOut();
                                tearOutLoc = c->currentTearOutLocation();
                                wasTornOutTag = otag;
                            }
                            if (!wasTornOut)
                            {
                                closeOverlay(otag);
                            }
                            hadExtendedOverlay = true;
                        }
                    }

                    if (hadExtendedOverlay)
                    {
                        auto ld = &(synth->storage.getPatch().scene[current_scene].lfo[newsource -
                                                                                       ms_lfo1]);
                        auto tag = MSEG_EDITOR;
                        bool go = false;
                        if (ld->shape.val.i == lt_mseg)
                        {
                            go = true;
                        }
                        if (ld->shape.val.i == lt_formula)
                        {
                            tag = FORMULA_EDITOR;
                            go = true;
                        }
                        if (go)
                        {
                            if (wasTornOut)
                            {
                                closeOverlay(wasTornOutTag);
                            }
                            showOverlay(tag);
                            if (wasTornOut)
                            {
                                auto c = getOverlayWrapperIfOpen(tag);
                                if (c)
                                {
                                    c->doTearOut(tearOutLoc);
                                }
                            }
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
        int nn = sst::filters::fut_subcount
            [synth->storage.getPatch().scene[current_scene].filterunit[idx].type.val.i];
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

            auto h = csc->getAccessibilityHandler();
            if (h)
                h->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
        }

        SurgeSynthesizer::ID ptagid;
        if (synth->fromSynthSideId(tag - start_paramtags, ptagid))
            synth->sendParameterAutomation(ptagid, synth->getParameter01(ptagid));

        if (bvf)
            bvf->repaint();
        synth->switch_toggled_queued = true;

        if (auto fo = getOverlayIfOpenAs<Surge::Overlays::OverlayComponent>(FILTER_ANALYZER))
        {
            fo->forceDataRefresh();
            fo->repaint();
        }
        return;
    }

    switch (tag)
    {
    case tag_scene_select:
    {
        changeSelectedScene((int)(control->getValue() * 1.f) + 0.5f);

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
        undoManager()->pushPatch();
        closeOverlay(SAVE_PATCH);

        loadPatchWithDirtyCheck((control->getValue() > 0.5f), true);

        return;
    }
    break;
    case tag_mp_patch:
    {
        undoManager()->pushPatch();
        closeOverlay(SAVE_PATCH);

        auto insideCategory = Surge::Storage::getUserDefaultValue(
            &(synth->storage), Surge::Storage::PatchJogWraparound, 1);

        loadPatchWithDirtyCheck((control->getValue() > 0.5f), false, insideCategory);

        return;
    }
    break;
    case tag_mp_jogwaveshape:
    {
        if (waveshaperSelector)
        {
            if (control->getValue() > 0.5f)
                waveshaperSelector->jog(1);
            else
                waveshaperSelector->jog(-1);
        }
    }
    break;
    case tag_analyzewaveshape:
    {
        if (this->waveshaperSelector)
        {
            if (control->getValue() > 0.5f)
                showOverlay(WAVESHAPER_ANALYZER);
            else
                closeOverlay(WAVESHAPER_ANALYZER);
        }
    }
    break;
    case tag_analyzefilters:
    {
        if (control->getValue() > 0.5f)
            showOverlay(FILTER_ANALYZER);
        else
            closeOverlay(FILTER_ANALYZER);
    }
    break;
    case tag_mp_jogfx:
    {
        if (fxMenu)
        {
            if (control->getValue() > 0.5f)
            {
                fxMenu->jogBy(1);
            }
            else
            {
                fxMenu->jogBy(-1);
            }

            if (fxMenu->selectedIdx >= 0 && fxMenu->selectedIdx != selectedFX[current_fx])
                selectedFX[current_fx] = fxMenu->selectedIdx;

            if (fxPresetLabel)
            {
                fxPresetLabel->setText(fxMenu->selectedName.c_str(), juce::dontSendNotification);
                fxPresetName[this->current_fx] = fxMenu->selectedName;
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
        juce::Point<int> cwhere = control->asJuceComponent()->getBounds().getTopLeft();
        useDevMenu = false;
        showSettingsMenu(cwhere, control);
    }
    break;
    case tag_osc_select:
    {
        changeSelectedOsc((int)(control->getValue() * 2.f + 0.5f));

        return;
    }
    break;
    case tag_fx_select:
    {
        auto fxc = dynamic_cast<Surge::Widgets::EffectChooser *>(control);
        if (!fxc)
            return;

        int cur_bitmask = synth->storage.getPatch().fx_disable.val.i;
        int d = fxc->getDeactivatedBitmask();
        synth->fx_suspend_bitmask = cur_bitmask ^ d;

        if (d != cur_bitmask)
        {
            juceEditor->processor.paramChangeToListeners(nullptr, true,
                                                         juceEditor->processor.SCT_FX_DEACT,
                                                         (float)cur_bitmask, (float)d, .0, "");
        }

        synth->storage.getPatch().fx_disable.val.i = d;
        fxc->setDeactivatedBitmask(d);

        int nfx = fxc->getCurrentEffect();
        if (current_fx != nfx)
        {
            current_fx = nfx;
            activateFromCurrentFx();

            queue_refresh = true;
        }

        return;
    }
    break;
    case tag_osc_menu:
    {
        synth->switch_toggled_queued = true;
        queue_refresh = true;
        synth->processAudioThreadOpsWhenAudioEngineUnavailable();
        return;
    }
    break;
    case tag_fx_menu:
    {
        synth->load_fx_needed = true;
        // queue_refresh = true;
        synth->fx_reload[limit_range(current_fx, 0, n_fx_slots - 1)] = true;
        synth->processAudioThreadOpsWhenAudioEngineUnavailable();

        if (fxMenu && fxMenu->selectedIdx >= 0)
        {
            selectedFX[current_fx] = fxMenu->selectedIdx;
            fxPresetName[current_fx] = fxMenu->selectedName;
        }
        else if (fxPresetLabel)
        {
            fxPresetLabel->setText(fxMenu->selectedName.c_str(), juce::dontSendNotification);
            fxPresetName[this->current_fx] = fxMenu->selectedName;
        }

        return;
    }
    break;
    case tag_store:
    {
        showOverlay(SurgeGUIEditor::SAVE_PATCH, [this](Surge::Overlays::OverlayComponent *co) {
            auto psd = dynamic_cast<Surge::Overlays::PatchStoreDialog *>(co);

            if (!psd)
            {
                return;
            }

            psd->setShowTagsField(juce::ModifierKeys::currentModifiers.isAltDown());
            psd->setShowFactoryOverwrite(juce::ModifierKeys::currentModifiers.isShiftDown() &&
                                         juce::ModifierKeys::currentModifiers.isAltDown());
        });
    }
    break;

    default:
    {
        int ptag = tag - start_paramtags;

        if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
        {
            Parameter *p = synth->storage.getPatch().param_ptr[ptag];

            // The UI reset the value; means soft takeover needs to try again
            p->miditakeover_status = sts_waiting_for_first_look;

            if (p->is_nonlocal_on_change())
            {
                frame->repaint();
            }

            char pname[TXT_SIZE], pdisp[TXT_SIZE], txt[TXT_SIZE];
            bool modulate = false;

            // This allows us to turn on and off the editor. FIXME MSEG check it
            if (p->ctrltype == ct_lfotype)
            {
                synth->refresh_editor = true;
            }

            auto mci = dynamic_cast<Surge::Widgets::ModulatableControlInterface *>(control);

            if (modsource && mod_editor && synth->isValidModulation(p->id, modsource) && mci)
            {
                modsources thisms = modsource;

                if (gui_modsrc[modsource])
                {
                    auto cms = gui_modsrc[modsource].get();
#if FIXME_ALTERNATES
                    if (cms->getHasAlternate() && cms->getUseAlternate())
                        thisms = cms->getAlternate();
#endif
                }

                bool quantize_mod = juce::ModifierKeys::currentModifiers.isCommandDown();
                float mv = mci->getModValue();

                if (quantize_mod)
                {
                    mv = p->quantize_modulation(mv);
                    // maybe setModValue here
                }

                auto use_scene = 0;

                if (this->synth->isModulatorDistinctPerScene(thisms))
                    use_scene = current_scene;

                synth->setModDepth01(ptag, thisms, use_scene, modsource_index, mv);

                mci->setModulationState(
                    synth->isModDestUsed(p->id),
                    synth->isActiveModulation(p->id, thisms, use_scene, modsource_index));
                mci->setIsModulationBipolar(synth->isBipolarModulation(thisms));

                SurgeSynthesizer::ID ptagid;

                if (synth->fromSynthSideId(ptag, ptagid))
                {
                    synth->getParameterName(ptagid, txt);
                }

                snprintf(pname, TXT_SIZE - 1, "%s -> %s",
                         ModulatorName::modulatorName(&synth->storage, thisms, true, current_scene)
                             .c_str(),
                         txt);

                ModulationDisplayInfoWindowStrings mss;

                p->get_display_of_modulation_depth(
                    pdisp, synth->getModDepth(ptag, thisms, use_scene, modsource_index),
                    synth->isBipolarModulation(thisms), Parameter::InfoWindow, &mss);
                modulate = true;

                if (isCustomController(modsource))
                {
                    int ccid = modsource - ms_ctrl1;
                    char *lbl = synth->storage.getPatch().CustomControllerLabel[ccid];

                    if ((lbl[0] == '-') && !lbl[1])
                    {
                        strxcpy(lbl, p->get_name(), CUSTOM_CONTROLLER_LABEL_SIZE - 1);
                        synth->storage.getPatch()
                            .CustomControllerLabel[ccid][CUSTOM_CONTROLLER_LABEL_SIZE - 1] = 0;
                        gui_modsrc[modsource]->setCurrentModLabel(lbl);
                        gui_modsrc[modsource]->repaint();
                    }
                }
            }
            else
            {
                auto val = control->getValue();

                if (p->ctrltype == ct_scenemode)
                {

                    // See the comment in the constructor of ct_scenemode above
                    auto cs2 = dynamic_cast<Surge::Widgets::MultiSwitch *>(control);
                    auto im = 0;

                    if (cs2)
                    {
                        im = cs2->getIntegerValue();

                        if (im == 3)
                        {
                            im = 2;
                        }
                        else if (im == 2)
                        {
                            im = 3;
                        }

                        val = Parameter::intScaledToFloat(im, n_scene_modes - 1);
                    }

                    // Now I also need to toggle the split key state
                    if (splitpointControl)
                    {
                        int cm = splitpointControl->getControlMode();

                        if (im == sm_chsplit && cm != Surge::Skin::Parameters::MIDICHANNEL_FROM_127)
                        {
                            splitpointControl->setControlMode(
                                Surge::Skin::Parameters::MIDICHANNEL_FROM_127);
                            splitpointControl->repaint();
                        }
                        else if (im == sm_split && cm != Surge::Skin::Parameters::NOTENAME)
                        {
                            splitpointControl->setControlMode(Surge::Skin::Parameters::NOTENAME);
                            splitpointControl->repaint();
                        }
                        else if ((im == sm_single || im == sm_dual) &&
                                 cm != Surge::Skin::Parameters::NONE)
                        {
                            // drop out of MIDI learn mode if active
                            if (synth->learn_param_from_note > -1)
                            {
                                synth->learn_param_from_note = -1;
                            }

                            splitpointControl->setControlMode(Surge::Skin::Parameters::NONE);
                            splitpointControl->repaint();
                        }
                    }
                }

                bool force_integer = juce::ModifierKeys::currentModifiers.isCommandDown();
                SurgeSynthesizer::ID ptagid;
                synth->fromSynthSideId(ptag, ptagid);

                if (synth->setParameter01(ptagid, val, false, force_integer))
                {
                    synth->sendParameterAutomation(ptagid, synth->getParameter01(ptagid));
                    queue_refresh = true;
                    return;
                }
                else
                {
                    synth->sendParameterAutomation(ptagid, synth->getParameter01(ptagid));

                    auto mci = dynamic_cast<Surge::Widgets::ModulatableControlInterface *>(control);

                    if (mci)
                    {
                        mci->setQuantitizedDisplayValue(p->get_value_f01());
                    }
                    else if (bvf)
                    {
                        bvf->repaint();
                    }

                    synth->getParameterName(ptagid, pname);
                    synth->getParameterDisplay(ptagid, pdisp);

                    char pdispalt[TXT_SIZE];

                    synth->getParameterDisplayAlt(ptagid, pdispalt);

                    if (p->ctrltype == ct_polymode)
                    {
                        modulate = true;
                    }
                }

                if (p->ctrltype == ct_bool_unipolar || p->ctrltype == ct_lfotype)
                {
                    // The green line might change so...
                    refresh_mod();
                }

                // Finally we may have to update the modulation editor
                auto me = getOverlayIfOpenAs<Surge::Overlays::ModulationEditor>(MODULATION_EDITOR);

                if (me)
                {
                    me->updateParameterById(ptagid);
                }
            }
            if (!queue_refresh)
            {
                // There was infowindow code here. Do I need it still?

                if (oscWaveform && ((p->ctrlgroup == cg_OSC) || (p->ctrltype == ct_character)))
                {
                    oscWaveform->repaint();
                }
                if (lfoDisplay && (p->ctrlgroup == cg_LFO))
                {
                    lfoDisplay->repaint();
                }
                for (const auto &[olTag, ol] : juceOverlays)
                {
                    auto olpc = ol->getPrimaryChildAsOverlayComponent();
                    if (olpc && olpc->shouldRepaintOnParamChange(getPatch(), p))
                    {
                        olpc->repaint();
                    }
                    else
                    {
                        // std::cout << "Skipping param repaint" << std::endl;
                    }
                }
            }
            if (p->ctrltype == ct_filtertype)
            {
                auto *subsw =
                    dynamic_cast<Surge::Widgets::Switch *>(filtersubtype[p->ctrlgroup_entry]);
                if (subsw)
                {
                    int sc = sst::filters::fut_subcount[p->val.i];

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
        int nn = sst::filters::fut_subcount
            [synth->storage.getPatch().scene[current_scene].filterunit[idx].type.val.i];
        if (a >= nn)
            a = 0;
        synth->storage.getPatch().scene[current_scene].filterunit[idx].subtype.val.i = a;
        if (!nn)
            ((Surge::Widgets::Switch *)filtersubtype[idx])->setIntegerValue(0);
        else
            ((Surge::Widgets::Switch *)filtersubtype[idx])->setIntegerValue(a + 1);

        auto h = ((Surge::Widgets::Switch *)filtersubtype[idx])->getAccessibilityHandler();
        if (h)
            h->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);

        filtersubtype[idx]->repaint();
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

    // TODO: If we hide waveshaper selector in skin but leave jog and analysis button in,
    // they stop working. Is there any way around this?
    if (waveshaperSelector && tag == waveshaperSelector->getTag())
    {
        updateWaveshaperOverlay();
    }
}

bool SurgeGUIEditor::setParameterFromString(Parameter *p, const std::string &s, std::string &errMsg)
{
    auto v = p->get_value_f01();

    if (p)
    {
        undoManager()->pushParameterChange(p->id, p, p->val);
        auto res = p->set_value_from_string(s, errMsg);

        if (!res)
        {
            return false;
        }

        if (v != p->get_value_f01())
            synth->storage.getPatch().isDirty = true;
        repushAutomationFor(p);
        synth->refresh_editor = true;
        return true;
    }

    return false;
}
bool SurgeGUIEditor::setParameterModulationFromString(Parameter *p, modsources ms,
                                                      int modsourceScene, int modidx,
                                                      const std::string &s, std::string &errMsg)
{
    if (!p || ms == 0)
        return false;

    bool valid = false;
    auto mv = p->calculate_modulation_value_from_string(s, errMsg, valid);

    if (!valid)
    {
        return false;
    }
    else
    {
        undoManager()->pushModulationChange(
            p->id, p, ms, modsourceScene, modidx,
            synth->getModDepth01(p->id, ms, modsourceScene, modidx),
            synth->isModulationMuted(p->id, ms, modsourceScene, modidx));
        synth->setModDepth01(p->id, ms, modsourceScene, modidx, mv);
        synth->refresh_editor = true;
        synth->storage.getPatch().isDirty = true;
    }

    if (ms >= ms_ctrl1 && ms <= ms_ctrl1 + n_customcontrollers)
    {
        if (strcmp(synth->storage.getPatch().CustomControllerLabel[ms - ms_ctrl1], "-") == 0)
        {
            strncpy(synth->storage.getPatch().CustomControllerLabel[ms - ms_ctrl1], p->get_name(),
                    CUSTOM_CONTROLLER_LABEL_SIZE - 1);
            synth->refresh_editor = true;
        }
    }
    return true;
}
bool SurgeGUIEditor::setControlFromString(modsources ms, const std::string &s)
{
    auto cms = ((ControllerModulationSource *)synth->storage.getPatch()
                    .scene[current_scene]
                    .modsources[ms]);
    if (ms >= ms_ctrl1 && ms <= ms_ctrl8)
    {
        undoManager()->pushMacroChange(ms - ms_ctrl1, cms->get_target01(0));
    }
    bool bp = cms->is_bipolar();
    float val = std::atof(s.c_str()) / 100.0;
    if ((bp && val >= -1 && val <= 1) || (val >= 0 && val <= 1))
    {
        cms->set_output(0, val);
        cms->target[0] = val;
        synth->refresh_editor = true;
        synth->storage.getPatch().isDirty = true;
        return true;
    }
    return false;
}

void SurgeGUIEditor::modSet(long ptag, modsources modsource, int modsourceScene, int index,
                            float value, bool isNew)
{
    if (!selfModulation)
        needsModUpdate = true;
}
void SurgeGUIEditor::modMuted(long ptag, modsources modsource, int modsourceScene, int index,
                              bool mute)
{
    if (!selfModulation)
        needsModUpdate = true;
}
void SurgeGUIEditor::modCleared(long ptag, modsources modsource, int modsourceScene, int index)
{
    if (!selfModulation)
        needsModUpdate = true;
}
