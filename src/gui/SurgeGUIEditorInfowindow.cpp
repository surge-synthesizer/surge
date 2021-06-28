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

void SurgeGUIEditor::showInfowindow(int ptag, juce::Rectangle<int> relativeTo,
                                    bool isEditingModulation)
{
    auto pid = ptag - start_paramtags;
    auto p = synth->storage.getPatch().param_ptr[pid];
    if ((p->ctrlstyle & kNoPopup))
    {
        hideInfowindowNow();
        return;
    }

    updateInfowindowContents(ptag, isEditingModulation);
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
    if ((p->ctrlstyle & kNoPopup))
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

void SurgeGUIEditor::idleInfowindow() { paramInfowindow->idle(); }

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
        sprintf(pname, "%s -> %s", modulatorName(modsource, true).c_str(), txt);
        ModulationDisplayInfoWindowStrings mss;
        p->get_display_of_modulation_depth(pdisp, synth->getModDepth(pid, modsource),
                                           synth->isBipolarModulation(modsource),
                                           Parameter::InfoWindow, &mss);
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