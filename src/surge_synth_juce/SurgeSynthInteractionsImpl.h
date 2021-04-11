//
// Created by Paul Walker on 4/10/21.
//

#ifndef SURGE_SURGESYNTHINTERACTIONSIMPL_H
#define SURGE_SURGESYNTHINTERACTIONSIMPL_H

#include <JuceHeader.h>
#include "SurgeSynthInteractionsInterface.h"
#include "SurgeGUIEditor.h" // bit of a hack

struct SurgeSynthInteractionsImpl : public SurgeSynthInteractionsInterface
{
    void promptError(const std::string &message, const std::string &title,
                     SurgeGUIEditor *guiEditor)
    {
        juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon, message, title);
    }
};

#endif // SURGE_SURGESYNTHINTERACTIONSIMPL_H
