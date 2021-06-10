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

#ifndef SURGE_PATCHDBVIEWER_H
#define SURGE_PATCHDBVIEWER_H

#include <JuceHeader.h>
#include "SurgeStorage.h"

class SurgeGUIEditor;

namespace Surge
{
namespace Overlays
{
class PatchDBSQLTableModel;

class PatchDBViewer : public juce::Component, public juce::TextEditor::Listener
{
  public:
    PatchDBViewer(SurgeGUIEditor *ed, SurgeStorage *s);
    ~PatchDBViewer();
    void createElements();
    void executeQuery();

    void paint(juce::Graphics &g) override;

    void resized() override;

    void textEditorTextChanged(juce::TextEditor &editor) override;
    std::unique_ptr<juce::TextEditor> nameTypein;
    std::unique_ptr<juce::TableListBox> table;
    std::unique_ptr<PatchDBSQLTableModel> tableModel;

    SurgeStorage *storage;
    SurgeGUIEditor *editor;
};

} // namespace Overlays
} // namespace Surge

#endif // SURGE_PATCHDBVIEWER_H
