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

#include <iostream>
#include "SurgeGUIEditor.h"
#include "SurgeGUIEditorTags.h"

#include "widgets/ParameterInfowindow.h"
#include "widgets/MainFrame.h"
#include "DebugHelpers.h"

void SurgeGUIEditor::showInfowindow(int ptag, juce::Rectangle<int> relativeTo,
                                    bool isEditingModulation)
{
    auto pid = ptag - start_paramtags;
    auto p = synth->storage.getPatch().param_ptr[pid];
    if ((p->ctrlstyle & Surge::ParamConfig::kNoPopup))
    {
        hideInfowindowNow();
        return;
    }

    updateInfowindowContents(ptag, isEditingModulation);
    paramInfowindow->getParentComponent()->toFront(false);
    paramInfowindow->toFront(false);
    paramInfowindow->setBoundsToAccompany(relativeTo, frame->getBounds());
    paramInfowindow->setCountdownToHide(-1);
    paramInfowindow->startFadein();
    paramInfowindow->setVisible(true);
}

void SurgeGUIEditor::showInfowindowSelfDismiss(int ptag, juce::Rectangle<int> relativeTo,
                                               bool isEditingModulation)
{
    auto pid = ptag - start_paramtags;
    auto p = synth->storage.getPatch().param_ptr[pid];
    if ((p->ctrlstyle & Surge::ParamConfig::kNoPopup))
    {
        hideInfowindowNow();
        return;
    }

    updateInfowindowContents(ptag, isEditingModulation);
    paramInfowindow->setBoundsToAccompany(relativeTo, frame->getBounds());
    paramInfowindow->setCountdownToHide(60);
    paramInfowindow->startFadein();
    paramInfowindow->setVisible(true);
}

void SurgeGUIEditor::hideInfowindowNow() { paramInfowindow->doHide(); }

void SurgeGUIEditor::hideInfowindowSoon() { paramInfowindow->doHide(5); }

void SurgeGUIEditor::idleInfowindow()
{
    if (infoQCountdown > 0 && infoQState == COUNTDOWN)
    {
        infoQCountdown--;
        if (infoQCountdown == 0)
        {
            infoQState = SHOWING;
            showInfowindow(infoQTag, infoQBounds, false);
            infoQCountdown = -1;
        }
    }
    paramInfowindow->idle();
}

std::string SurgeGUIEditor::getAccessibleModulationVoiceover(long ptag)
{
    auto pid = ptag - start_paramtags;
    auto p = synth->storage.getPatch().param_ptr[pid];

    ModulationDisplayInfoWindowStrings mss;
    char pdisp[TXT_SIZE];
    p->get_display_of_modulation_depth(
        pdisp, synth->getModDepth(pid, modsource, current_scene, modsource_index),
        synth->isBipolarModulation(modsource), Parameter::InfoWindow, &mss);

    return mss.val + " mod " + mss.dvalplus;
}

void SurgeGUIEditor::updateInfowindowContents(int ptag, bool isModulated)
{
    auto pid = ptag - start_paramtags;
    auto p = synth->storage.getPatch().param_ptr[pid];

    auto modValues = Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                                                         Surge::Storage::ModWindowShowsValues, 0);
    paramInfowindow->setExtendedMDIWS(modValues);

    if (isModulated)
    {
        char pname[1024], pdisp[TXT_SIZE], txt[TXT_SIZE];
        SurgeSynthesizer::ID ptagid;
        if (synth->fromSynthSideId(pid, ptagid))
            synth->getParameterName(ptagid, txt);
        auto mn = modulatorNameWithIndex(current_scene, modsource, modsource_index, true, false);

        sprintf(pname, "%s -> %s", mn.c_str(), txt);
        ModulationDisplayInfoWindowStrings mss;
        p->get_display_of_modulation_depth(
            pdisp, synth->getModDepth(pid, modsource, current_scene, modsource_index),
            synth->isBipolarModulation(modsource), Parameter::InfoWindow, &mss);
        if (mss.val != "")
        {
            paramInfowindow->setLabels(pname, pdisp);
            paramInfowindow->setMDIWS(mss);
        }
        else
        {
            paramInfowindow->setLabels(pname, pdisp);
            paramInfowindow->clearMDIWS();
        }
    }
    else
    {
        char pname[TXT_SIZE], pdisp[TXT_SIZE], pdispalt[TXT_SIZE];

        SurgeSynthesizer::ID ptagid;
        synth->fromSynthSideId(pid, ptagid);

        synth->getParameterDisplay(ptagid, pdisp);
        synth->getParameterDisplayAlt(ptagid, pdispalt);
        paramInfowindow->setLabels("", pdisp, pdispalt);
        paramInfowindow->clearMDIWS();
    }
}

void SurgeGUIEditor::repaintFrame() { frame->repaint(); }

void SurgeGUIEditor::enqueueFutureInfowindow(int ptag, const juce::Rectangle<int> &around,
                                             InfoQAction action)
{
    bool doIt = Surge::Storage::getUserDefaultValue(&(synth->storage),
                                                    Surge::Storage::InfoWindowPopupOnIdle, true);
    if (!doIt)
        return;

    if (action == START)
    {
        infoQBounds = around;
        infoQTag = ptag;

        if (infoQState == DISMISSED_BEFORE)
        {
            infoQCountdown = -1;
        }
        else if (infoQState == SHOWING)
        {
            // This means we have a mouse move while showing so
            hideInfowindowNow();
            infoQState = DISMISSED_BEFORE; // this means you only pop once
        }
        else
        {
            infoQCountdown = 40;
            infoQState = COUNTDOWN;
        }
    }
    else if (action == CANCEL)
    {
        hideInfowindowNow();
        infoQCountdown = -1;
        infoQState = DISMISSED_BEFORE;
    }
    else if (action == LEAVE)
    {
        hideInfowindowNow();
        infoQCountdown = -1;
        infoQState = NONE;
        infoQTag = -1;
    }
}