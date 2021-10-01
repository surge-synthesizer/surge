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

#ifndef SURGE_TYPEAHEADTEXTEDITOR_H
#define SURGE_TYPEAHEADTEXTEDITOR_H

#include <string>
#include "juce_gui_basics/juce_gui_basics.h"

namespace Surge
{
namespace Widgets
{
struct TypeAheadListBoxModel;

struct TypeAheadDataProvider
{
    virtual ~TypeAheadDataProvider() = default;
    virtual std::vector<std::string> searchFor(const std::string &s) = 0;
};

struct TypeAhead : public juce::TextEditor, juce::TextEditor::Listener
{

    TypeAhead(const std::string &l, TypeAheadDataProvider *p); // does not take ownership
    ~TypeAhead();

    void dismissWithValue(const std::string &s);
    void dismissWithoutValue();

    std::unique_ptr<juce::ListBox> lbox;
    std::unique_ptr<TypeAheadListBoxModel> lboxmodel;

    void showLbox();
    void parentHierarchyChanged() override;
    void textEditorTextChanged(juce::TextEditor &editor) override;
    void textEditorReturnKeyPressed(juce::TextEditor &editor) override { lbox->setVisible(false); }
    void textEditorEscapeKeyPressed(juce::TextEditor &editor) override { lbox->setVisible(false); }

    bool keyPressed(const juce::KeyPress &press) override;
};

} // namespace Widgets
} // namespace Surge

#endif // SURGE_TYPEAHEADTEXTEDITOR_H
