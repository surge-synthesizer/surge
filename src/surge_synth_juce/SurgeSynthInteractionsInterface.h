//
// Created by Paul Walker on 4/10/21.
//

#ifndef SURGE_SURGESYNTHINTERACTIONSINTERFACE_H
#define SURGE_SURGESYNTHINTERACTIONSINTERFACE_H

#include <string>
#include <atomic>

class SurgeGUIEditor;
struct SurgeSynthInteractionsInterface
{
    static std::atomic<SurgeSynthInteractionsInterface *> impl;

    virtual ~SurgeSynthInteractionsInterface() = default;
    virtual void promptError(const std::string &message, const std::string &title,
                             SurgeGUIEditor *guiEditor) = 0;
};

#endif // SURGE_SURGESYNTHINTERACTIONSINTERFACE_H
