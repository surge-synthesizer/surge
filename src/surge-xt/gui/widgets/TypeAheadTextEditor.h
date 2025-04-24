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

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_TYPEAHEADTEXTEDITOR_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_TYPEAHEADTEXTEDITOR_H

#include <string>
#include <set>
#include "juce_gui_basics/juce_gui_basics.h"
#include "SkinSupport.h"

namespace Surge
{
namespace Widgets
{
struct TypeAheadListBox;
struct TypeAheadListBoxModel;

struct TypeAheadDataProvider
{
    virtual ~TypeAheadDataProvider() = default;
    virtual std::vector<int> searchFor(const std::string &s) = 0;
    virtual std::string textBoxValueForIndex(int idx) = 0;
    virtual std::string accessibleTextForIndex(int idx) { return textBoxValueForIndex(idx); }
    virtual int getRowHeight() { return 15; }
    virtual int getDisplayedRows() { return 9; }
    virtual void paintDataItem(int searchIndex, juce::Graphics &g, int width, int height,
                               bool rowIsSelected);
    virtual void paintOverChildren(juce::Graphics &g, const juce::Rectangle<int> &bounds) {}
};

struct TypeAhead : public juce::TextEditor, juce::TextEditor::Listener
{
    enum ColourIds
    {
        emptyBackgroundId = 0x370F001,
        borderid
    };

    enum DismissMode
    {
        DISMISS_ON_RETURN,
        DISMISS_ON_RETURN_RETAIN_ON_CMD_RETURN,
        DISMISS_ON_CMD_RETURN_RETAIN_ON_RETURN
    } dismissMode{DISMISS_ON_RETURN};
    std::string lastSearch{""};

    TypeAhead(const std::string &l, TypeAheadDataProvider *p); // does not take ownership
    ~TypeAhead();

    struct TypeAheadListener
    {
        virtual ~TypeAheadListener() = default;
        virtual void itemFocused(int providerIndex) {}
        virtual void itemSelected(int providerIndex) = 0;
        virtual void typeaheadCanceled() = 0;
    };

    void colourChanged() override;

    std::set<TypeAheadListener *> taList;
    void addTypeAheadListener(TypeAheadListener *l) { taList.insert(l); }
    void removeTypeAheadListener(TypeAheadListener *l) { taList.erase(l); }

    void dismissWithValue(int providerIdx, const std::string &s, const juce::ModifierKeys &mod);
    void dismissWithoutValue();
    void updateSelected(int providerIdx);

    std::unique_ptr<juce::ListBox> lbox;
    std::unique_ptr<TypeAheadListBoxModel> lboxmodel;

    bool isRowMouseOver(int row);
    void searchAndShowLBox();
    void showLbox();
    void parentHierarchyChanged() override;
    void textEditorTextChanged(juce::TextEditor &editor) override;

    void focusLost(FocusChangeType type) override;

    bool setToElementZeroOnReturn{false};
    void textEditorReturnKeyPressed(juce::TextEditor &editor) override;
    void textEditorEscapeKeyPressed(juce::TextEditor &editor) override;

    bool keyPressed(const juce::KeyPress &press) override;
};

} // namespace Widgets
} // namespace Surge

#endif // SURGE_TYPEAHEADTEXTEDITOR_H
