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

#ifndef SURGE_SRC_SURGE_XT_GUI_OVERLAYS_PATCHDBVIEWER_H
#define SURGE_SRC_SURGE_XT_GUI_OVERLAYS_PATCHDBVIEWER_H

#include "SurgeStorage.h"

#include "juce_gui_basics/juce_gui_basics.h"
#include "OverlayComponent.h"

class SurgeGUIEditor;

namespace Surge
{
namespace Overlays
{
class PatchDBFiltersDisplay;
class PatchDBSQLTableModel;
class PatchDBSQLTreeViewItem;

class PatchDBViewer : public OverlayComponent,
                      public juce::TextEditor::Listener,
                      public juce::Button::Listener
{
  public:
    PatchDBViewer(SurgeGUIEditor *ed, SurgeStorage *s);
    ~PatchDBViewer();
    void createElements();
    void executeQuery();
    void checkJobsOverlay();

    void paint(juce::Graphics &g) override;

    void resized() override;

    void textEditorTextChanged(juce::TextEditor &editor) override;
    std::unique_ptr<juce::TextEditor> nameTypein;
    std::unique_ptr<juce::TableListBox> table;
    std::unique_ptr<PatchDBSQLTableModel> tableModel;

    SurgeStorage *storage;
    SurgeGUIEditor *editor;

    std::unique_ptr<juce::TreeView> treeView;
    std::unique_ptr<PatchDBSQLTreeViewItem> treeRoot;
    std::unique_ptr<PatchDBFiltersDisplay> filters;

    std::unique_ptr<juce::Button> doDebug;
    void buttonClicked(juce::Button *button) override;

    std::unique_ptr<juce::Label> jobCountdown;
    std::unique_ptr<juce::Timer> countdownClock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchDBViewer);
};

} // namespace Overlays
} // namespace Surge

#endif // SURGE_PATCHDBVIEWER_H
