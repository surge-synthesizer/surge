//
// Created by Paul Walker on 4/10/21.
//

#ifndef SURGE_SURGESYNTHINTERACTIONSINTERFACE_H
#define SURGE_SURGESYNTHINTERACTIONSINTERFACE_H

#include <string>
#include <atomic>
#include "UserInteractions.h"

class SurgeGUIEditor;
struct SurgeSynthInteractionsInterface
{
    static std::atomic<SurgeSynthInteractionsInterface *> impl;

    virtual ~SurgeSynthInteractionsInterface() = default;
    virtual void promptError(const std::string &message, const std::string &title,
                             SurgeGUIEditor *guiEditor) = 0;
    virtual void promptInfo(const std::string &message, const std::string &title,
                            SurgeGUIEditor *guiEditor) = 0;
    virtual Surge::UserInteractions::MessageResult promptOKCancel(const std::string &message,
                                                                  const std::string &title,
                                                                  SurgeGUIEditor *guiEditor) = 0;
    virtual void openURL(const std::string &url) = 0;
    virtual void showHTML(const std::string &html) = 0;
    virtual void promptFileOpenDialog(const std::string &initialDirectory,
                                      const std::string &filterSuffix,
                                      const std::string &filterDescription,
                                      std::function<void(std::string)> callbackOnOpen,
                                      bool canSelectDirectories = false,
                                      bool canCreateDirectories = false,
                                      SurgeGUIEditor *guiEditor = nullptr) = 0;
};

#endif // SURGE_SURGESYNTHINTERACTIONSINTERFACE_H
