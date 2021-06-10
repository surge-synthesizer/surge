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

#include "SurgeGUIEditor.h"
#include "SurgeGUIEditorTags.h"
#include "SurgeGUIUtils.h"

#include "CLFOGui.h"
#include "ModernOscillator.h"

#include "widgets/EffectChooser.h"
#include "widgets/MultiSwitch.h"
#include "widgets/MenuForDiscreteParams.h"
#include "widgets/ModulationSourceButton.h"
#include "widgets/NumberField.h"
#include "widgets/OscillatorWaveformDisplay.h"
#include "widgets/Switch.h"
#include "widgets/PatchSelector.h"
#include "widgets/ParameterInfowindow.h"
#include "widgets/XMLConfiguredMenus.h"

#include "overlays/TypeinParamEditor.h"

using namespace VSTGUI;

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
    if (tag == tag_mp_jogfx || tag == tag_mp_category || tag == tag_mp_patch || tag == tag_store)
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
            auto *cms = dynamic_cast<Surge::Widgets::ModulationSourceButton *>(control);
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

            if (cms->getHasAlternate())
            {
                int idOn = modsource;
                int idOff = cms->getAlternate();

                if (cms->getUseAlternate())
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

                bool activeMod = ((cms->getState() & 3) == 2);
                std::string offLab = "Switch to ";
                offLab += modsource_names[idOff];

                contextMenu.addItem(offLab, !activeMod, false, [this, modsource, cms]() {
                    cms->setUseAlternate(!cms->getUseAlternate());
                    modsource_is_alternate[modsource] = cms->getUseAlternate();
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
            if (cms->getHasAlternate())
            {
                possibleSources.push_back((modsources)(cms->getAlternate()));
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
                                auto msb =
                                    dynamic_cast<Surge::Widgets::ModulationSourceButton *>(control);
                                if (msb)
                                    msb->setLabel(
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
                            auto msb =
                                dynamic_cast<Surge::Widgets::ModulationSourceButton *>(control);
                            if (msb)
                                msb->setLabel(
                                    synth->storage.getPatch().CustomControllerLabel[ccid]);
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
                                 std::min(((subs - 1) * 20) + 20, 128) - 1);
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
                         std::min(((subs - 1) * 20) + 20, 128) - 1);
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
                contextMenu.addItem(
                    Surge::GUI::toOSCaseForMenu("Bipolar Mode"), true, ibp,
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
                        auto msb = dynamic_cast<Surge::Widgets::ModulationSourceButton *>(control);
                        if (msb)
                            msb->setBipolar(bp);
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
                                auto msb =
                                    dynamic_cast<Surge::Widgets::ModulationSourceButton *>(control);
                                if (msb)
                                    msb->setLabel(
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
                                std::min(((subs - 1) * 20) + 20, 128) - 1);
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
                    sprintf(name, "CC %d ... %d", 7 * 20, std::min((7 * 20) + 20, 128) - 1);
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
                auto *cms = gui_modsrc[modsource].get();
                modsources thisms = modsource;
                if (cms->getHasAlternate() && cms->getUseAlternate())
                    thisms = cms->getAlternate();

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
                oscWaveform->repaint();

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
                    if (oscWaveform && (p->ctrlgroup == cg_OSC))
                        oscWaveform->repaint();
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

    if (typeinParamEditor->isVisible())
    {
        typeinParamEditor->setVisible(false);
        frame->juceComponent()->removeChildComponent(typeinParamEditor.get());
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
        auto cms = dynamic_cast<Surge::Widgets::ModulationSourceButton *>(control);
        if (cms->getMouseMode() == Surge::Widgets::ModulationSourceButton::DRAG_VALUE)
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
            int state = cms->getState();
            modsources newsource = (modsources)(tag - tag_mod_source0);
            long buttons = 0; // context->getMouseButtons(); // temp fix vstgui 3.5
            if (cms->getMouseMode() == Surge::Widgets::ModulationSourceButton::CLICK)
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
        if (fxMenu)
        {
            auto jog = [this](int byThis) {
                this->selectedFX[this->current_fx] =
                    std::max(this->selectedFX[this->current_fx] + byThis, -1);
                if (!fxMenu->loadSnapshotByIndex(this->selectedFX[this->current_fx]))
                {
                    // Try and go back to 0. This is the wrong behavior for negative jog
                    this->selectedFX[this->current_fx] = 0;
                    fxMenu->loadSnapshotByIndex(0);
                }
            };

            if (fxMenu->selectedIdx >= 0 && fxMenu->selectedIdx != selectedFX[current_fx])
                selectedFX[current_fx] = fxMenu->selectedIdx;

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
        showStorePatchDialog();
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

            auto mci = dynamic_cast<Surge::Widgets::ModulatableControlInterface *>(control);
            if (modsource && mod_editor && synth->isValidModulation(p->id, modsource) && mci)
            {
                modsources thisms = modsource;
                if (gui_modsrc[modsource])
                {
                    auto cms = gui_modsrc[modsource].get();
                    if (cms->getHasAlternate() && cms->getUseAlternate())
                        thisms = cms->getAlternate();
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
                modulate = true;

                if (isCustomController(modsource))
                {
                    int ccid = modsource - ms_ctrl1;
                    char *lbl = synth->storage.getPatch().CustomControllerLabel[ccid];

                    if ((lbl[0] == '-') && !lbl[1])
                    {
                        strxcpy(lbl, p->get_name(), 15);
                        synth->storage.getPatch().CustomControllerLabel[ccid][15] = 0;
                        gui_modsrc[modsource]->setLabel(lbl);
                        gui_modsrc[modsource]->repaint();
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
                            splitpointControl->setControlMode(Surge::Skin::Parameters::NONE);
                            splitpointControl->repaint();
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
                // There was infowindow code here. Do I need it still?

                if (oscWaveform && ((p->ctrlgroup == cg_OSC) || (p->ctrltype == ct_character)))
                {
                    oscWaveform->repaint();
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

bool SurgeGUIEditor::setParameterFromString(Parameter *p, const std::string &s)
{
    if (p && p->set_value_from_string(s))
    {
        repushAutomationFor(p);
        synth->refresh_editor = true;
        return true;
    }

    return false;
}
bool SurgeGUIEditor::setParameterModulationFromString(Parameter *p, modsources ms,
                                                      const std::string &s)
{
    if (!p || ms == 0)
        return false;

    bool valid = false;
    auto mv = p->calculate_modulation_value_from_string(s, valid);

    if (!valid)
    {
        return false;
    }
    else
    {
        synth->setModulation(p->id, ms, mv);
        synth->refresh_editor = true;
    }
    return true;
}
bool SurgeGUIEditor::setControlFromString(modsources ms, const std::string &s)
{
    auto cms = ((ControllerModulationSource *)synth->storage.getPatch()
                    .scene[current_scene]
                    .modsources[ms]);
    bool bp = cms->is_bipolar();
    float val = std::atof(s.c_str()) / 100.0;
    if ((bp && val >= -1 && val <= 1) || (val >= 0 && val <= 1))
    {
        cms->output = val;
        cms->target = val;
        synth->refresh_editor = true;
        return true;
    }
    return false;
}
