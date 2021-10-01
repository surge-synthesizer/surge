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

#include "TypeAheadTextEditor.h"

namespace Surge
{
namespace Widgets
{

struct TypeAheadListBoxModel : public juce::ListBoxModel
{
    TypeAheadDataProvider *provider;
    std::vector<std::string> search;
    TypeAhead *ta{nullptr};
    TypeAheadListBoxModel(TypeAhead *t, TypeAheadDataProvider *p) : ta(t), provider(p) {}

    void setSearch(const std::string &t) { search = provider->searchFor(t); }
    int getNumRows() override { return search.size(); }

    void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                          bool rowIsSelected) override
    {
        if (rowIsSelected)
        {
            g.fillAll(juce::Colours::wheat);
            g.setColour(juce::Colours::darkblue);
        }
        else
        {
            g.fillAll(juce::Colours::white);
            g.setColour(juce::Colours::black);
        }
        g.drawText(search[rowNumber], 0, 0, width, height, juce::Justification::centredLeft);
    }

    void returnKeyPressed(int lastRowSelected) override
    {
        ta->dismissWithValue(search[lastRowSelected]);
    }
    void escapeKeyPressed() { ta->dismissWithoutValue(); }
};

struct TypeAheadListBox : public juce::ListBox
{
    TypeAheadListBox(const juce::String &s, juce::ListBoxModel *m) : juce::ListBox(s, m)
    {
        setColour(outlineColourId, juce::Colours::black);
        setOutlineThickness(1);
        setColour(backgroundColourId, juce::Colours::white);
    }

    bool keyPressed(const juce::KeyPress &press) override
    {
        if (press.isKeyCode(juce::KeyPress::escapeKey))
        {
            if (auto m = dynamic_cast<TypeAheadListBoxModel *>(getModel()))
            {
                m->escapeKeyPressed();
                return true;
            }
        }
        return ListBox::keyPressed(press);
    }
};

TypeAhead::TypeAhead(const std::string &l, TypeAheadDataProvider *p)
    : juce::TextEditor(l), lboxmodel(std::make_unique<TypeAheadListBoxModel>(this, p))
{
    addListener(this);
    lbox = std::make_unique<TypeAheadListBox>("TypeAhead", lboxmodel.get());
    lbox->setMultipleSelectionEnabled(false);
    lbox->setVisible(false);
}

TypeAhead::~TypeAhead() = default;

void TypeAhead::dismissWithValue(const std::string &s)
{
    setText(s, juce::NotificationType::dontSendNotification);
    lbox->setVisible(false);
    grabKeyboardFocus();
}

void TypeAhead::dismissWithoutValue()
{
    lbox->setVisible(false);
    grabKeyboardFocus();
}

void TypeAhead::parentHierarchyChanged()
{
    TextEditor::parentHierarchyChanged();
    getParentComponent()->addChildComponent(*lbox);
}

void TypeAhead::showLbox()
{
    auto b = getBounds().translated(0, getHeight()).withHeight(140);
    lbox->setBounds(b);
    lbox->setVisible(true);
    lbox->toFront(false);
}

void TypeAhead::textEditorTextChanged(TextEditor &editor)
{
    lboxmodel->setSearch(editor.getText().toStdString());

    if (!lbox->isVisible())
    {
        showLbox();
    }
    lbox->updateContent();
    lbox->repaint();
}

bool TypeAhead::keyPressed(const juce::KeyPress &press)
{
    if (press.isKeyCode(juce::KeyPress::downKey))
    {
        if (!lbox->isVisible())
        {
            lboxmodel->setSearch("");
            showLbox();

            lbox->updateContent();
            lbox->repaint();
        }
        juce::SparseSet<int> r;
        r.addRange({0, 1});
        lbox->setSelectedRows(r);
        lbox->grabKeyboardFocus();
        return true;
    }
    return TextEditor::keyPressed(press);
}
} // namespace Widgets
} // namespace Surge