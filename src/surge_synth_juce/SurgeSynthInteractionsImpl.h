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
        juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon, title, message);
    }
    void promptInfo(const std::string &message, const std::string &title, SurgeGUIEditor *guiEditor)
    {
        juce::AlertWindow::showMessageBox(juce::AlertWindow::InfoIcon, title, message);
    }
    virtual Surge::UserInteractions::MessageResult
    promptOKCancel(const std::string &message, const std::string &title, SurgeGUIEditor *guiEditor)
    {
        // return Surge::UserInteractions::CANCEL;
        auto res = juce::AlertWindow::showOkCancelBox(juce::AlertWindow::InfoIcon, title, message,
                                                      "OK", "Cancel", nullptr, nullptr);
        if (res)
            return Surge::UserInteractions::OK;
        else
            return Surge::UserInteractions::CANCEL;
    }
    virtual void openURL(const std::string &url) { juce::URL(url).launchInDefaultBrowser(); };
    virtual void showHTML(const std::string &html)
    {
        static struct filesToDelete : juce::DeletedAtShutdown
        {
            ~filesToDelete()
            {
                for (const auto &d : deleteThese)
                {
                    d.deleteFile();
                }
            }
            std::vector<juce::File> deleteThese;
        } *byebyeOnExit = new filesToDelete();

        auto f = juce::File::createTempFile("_surge.html");
        f.replaceWithText(html);
        auto U = juce::URL(f);
        U.launchInDefaultBrowser();
        byebyeOnExit->deleteThese.push_back(f);
    };

    virtual void promptFileOpenDialog(const std::string &initialDirectory,
                                      const std::string &filterSuffix,
                                      const std::string &filterDescription,
                                      std::function<void(std::string)> callbackOnOpen,
                                      bool canSelectDirectories, bool canCreateDirectories,
                                      SurgeGUIEditor *guiEditor)
    {
        juce::FileChooser c("Select File", juce::File(initialDirectory));
        auto r = c.browseForFileToOpen();
        if (r)
        {
            auto res = c.getResult();
            callbackOnOpen(res.getFullPathName().toStdString());
        }
    }
};

#endif // SURGE_SURGESYNTHINTERACTIONSIMPL_H
