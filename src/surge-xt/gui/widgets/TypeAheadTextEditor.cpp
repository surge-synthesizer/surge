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
#include "MainFrame.h"

namespace Surge
{
namespace Widgets
{

void TypeAheadDataProvider::paintDataItem(int searchIndex, juce::Graphics &g, int width, int height,
                                          bool rowIsSelected)
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
    g.drawText(textBoxValueForIndex(searchIndex), 0, 0, width, height,
               juce::Justification::centredLeft);
}

struct TypeAheadListBoxModel : public juce::ListBoxModel
{
    TypeAheadDataProvider *provider;
    std::vector<int> search;
    TypeAhead *ta{nullptr};
    TypeAheadListBoxModel(TypeAhead *t, TypeAheadDataProvider *p) : ta(t), provider(p) {}

    void setSearch(const std::string &t) { search = provider->searchFor(t); }
    int getNumRows() override { return search.size(); }

    void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                          bool rowIsSelected) override
    {
        if (rowNumber >= 0 && rowNumber < search.size())
            provider->paintDataItem(search[rowNumber], g, width, height, rowIsSelected);
    }

    void returnKeyPressed(int lastRowSelected) override
    {
        if (lastRowSelected < 0 || lastRowSelected >= search.size())
        {
            ta->dismissWithoutValue();
            return;
        }
        ta->dismissWithValue(search[lastRowSelected],
                             provider->textBoxValueForIndex(search[lastRowSelected]));
    }
    void escapeKeyPressed() { ta->dismissWithoutValue(); }
};

struct TypeAheadListBox : public juce::ListBox
{
    TypeAheadListBox(const juce::String &s, juce::ListBoxModel *m) : juce::ListBox(s, m)
    {
        setOutlineThickness(1);
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

    void focusLost(FocusChangeType cause) override
    {
        if (auto m = dynamic_cast<TypeAheadListBoxModel *>(getModel()))
        {
            m->ta->focusLost(cause);
        }
    }
};

TypeAhead::TypeAhead(const std::string &l, TypeAheadDataProvider *p)
    : juce::TextEditor(l), lboxmodel(std::make_unique<TypeAheadListBoxModel>(this, p))
{
    addListener(this);
    lbox = std::make_unique<TypeAheadListBox>("TypeAhead", lboxmodel.get());
    lbox->setMultipleSelectionEnabled(false);
    lbox->setVisible(false);
    lbox->setRowHeight(p->getRowHeight());
    setColour(ColourIds::borderid, juce::Colours::black);
    setColour(ColourIds::emptyBackgroundId, juce::Colours::white);
}

TypeAhead::~TypeAhead() = default;

void TypeAhead::dismissWithValue(int providerIdx, const std::string &s)
{
    setText(s, juce::NotificationType::dontSendNotification);
    lbox->setVisible(false);
    grabKeyboardFocus();
    for (auto l : taList)
        l->itemSelected(providerIdx);
}

void TypeAhead::dismissWithoutValue()
{
    lbox->setVisible(false);
    grabKeyboardFocus();
    for (auto l : taList)
        l->typeaheadCanceled();
}

void TypeAhead::textEditorEscapeKeyPressed(juce::TextEditor &editor)
{
    lbox->setVisible(false);
    for (auto l : taList)
        l->typeaheadCanceled();
}

void TypeAhead::textEditorReturnKeyPressed(juce::TextEditor &editor)
{
    lbox->setVisible(false);

    if (setToElementZeroOnReturn && lboxmodel->getNumRows() > 0)
    {
        auto r = lboxmodel->provider->textBoxValueForIndex(lboxmodel->search[0]);
        setText(r, juce::NotificationType::dontSendNotification);
        for (auto l : taList)
        {
            l->itemSelected(0);
        }
    }
}

void TypeAhead::parentHierarchyChanged()
{
    TextEditor::parentHierarchyChanged();
    auto p = getParentComponent();
    while (p && !dynamic_cast<Surge::Widgets::MainFrame *>(p))
    {
        p = p->getParentComponent();
    }
    if (p)
        p->addChildComponent(*lbox);
}

void TypeAhead::showLbox()
{
    auto p = getParentComponent();
    while (p && !dynamic_cast<Surge::Widgets::MainFrame *>(p))
    {
        p = p->getParentComponent();
    }

    auto b =
        getLocalBounds()
            .translated(0, getHeight())
            .withHeight(
                lboxmodel->provider->getRowHeight() * lboxmodel->provider->getDisplayedRows() + 4);
    if (p)
    {
        b = p->getLocalArea(this, b);
        lbox->setBounds(b);
        lbox->setVisible(true);
        lbox->toFront(false);
    }
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

void TypeAhead::colourChanged()
{
    if (isColourSpecified(emptyBackgroundId))
        lbox->setColour(juce::ListBox::ColourIds::backgroundColourId,
                        findColour(ColourIds::emptyBackgroundId));
    if (isColourSpecified(ColourIds::borderid))
        lbox->setColour(juce::ListBox::ColourIds::outlineColourId, findColour(ColourIds::borderid));
}
void TypeAhead::focusLost(juce::Component::FocusChangeType type)
{
    if (hasKeyboardFocus(true))
        return;

    if (lbox->hasKeyboardFocus(true))
        return;

    lbox->setVisible(false);
    for (auto l : taList)
        l->typeaheadCanceled();
}
} // namespace Widgets
} // namespace Surge