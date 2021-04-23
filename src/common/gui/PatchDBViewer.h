//
// Created by Paul Walker on 4/15/21.
//

#ifndef SURGE_PATCHDBVIEWER_H
#define SURGE_PATCHDBVIEWER_H

#include <JuceHeader.h>
#include "SurgeStorage.h"

class PatchDBSQLTableModel;
class SurgeGUIEditor;

class PatchDBViewer : public juce::Component, public juce::TextEditor::Listener
{
  public:
    PatchDBViewer(SurgeGUIEditor *ed, SurgeStorage *s);
    ~PatchDBViewer();
    void createElements();
    void executeQuery();

    void textEditorTextChanged(juce::TextEditor &editor) override;
    std::unique_ptr<juce::TextEditor> nameTypein;
    std::unique_ptr<juce::TableListBox> table;
    std::unique_ptr<PatchDBSQLTableModel> tableModel;

    SurgeStorage *storage;
    SurgeGUIEditor *editor;
};

#endif // SURGE_PATCHDBVIEWER_H
