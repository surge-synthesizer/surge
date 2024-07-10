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

#include "TypeAheadTextEditor.h"
#include "MainFrame.h"
#include "AccessibleHelpers.h"

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

    juce::String getNameForRow(int row) override
    {
        if (row >= 0 && row < search.size())
        {
            return provider->accessibleTextForIndex(search[row]);
        }
        else
        {
            return juce::String("Row ") + juce::String(row);
        }
    }

    void returnKeyPressed(int lastRowSelected) override
    {
        auto m = juce::ModifierKeys::getCurrentModifiers();

        if (lastRowSelected < 0 || lastRowSelected >= search.size())
        {
            ta->dismissWithoutValue();

            return;
        }

        ta->dismissWithValue(search[lastRowSelected],
                             provider->textBoxValueForIndex(search[lastRowSelected]), m);
    }

    void escapeKeyPressed() { ta->dismissWithoutValue(); }

    void listBoxItemClicked(int row, const juce::MouseEvent &event) override
    {
        returnKeyPressed(row);
    }

    // if we have the listbox sticking around after patch load,
    // make sure we dismiss it on double click!
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent &event) override
    {
        ta->dismissWithoutValue();
    }

    struct TARow : juce::Label
    {
        int row{0};
        bool isSelected{false};
        TypeAheadListBoxModel *model{nullptr};
        TARow(TypeAheadListBoxModel *m) : model(m) { setWantsKeyboardFocus(true); }

        void paint(juce::Graphics &g) override
        {
            model->paintListBoxItem(row, g, getWidth(), getHeight(), isSelected);
        }

        void updateAccessibility()
        {
            setAccessible(true);

            if (row >= 0 && row < model->search.size())
            {
                setText(model->provider->accessibleTextForIndex(model->search[row]),
                        juce::dontSendNotification);
                setTitle(getText());
                setDescription(getText());
            }
            else
            {
                setText("", juce::dontSendNotification);
            }

            if (auto h = getAccessibilityHandler())
            {
                h->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
                h->notifyAccessibilityEvent(juce::AccessibilityEvent::textChanged);
            }

#if WINDOWS
            if (auto p = getParentComponent())
            {
                if (auto ph = p->getAccessibilityHandler())
                {
                    ph->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
                    ph->notifyAccessibilityEvent(juce::AccessibilityEvent::textChanged);
                }
            }
#endif
        }

        void mouseDown(const juce::MouseEvent &e) override { model->returnKeyPressed(row); }

        void mouseDoubleClick(const juce::MouseEvent &e) override { model->escapeKeyPressed(); }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TARow);
    };

    void selectedRowsChanged(int lastRowSelected) override
    {
        if (lastRowSelected >= 0 && lastRowSelected < search.size())
            ta->updateSelected(search[lastRowSelected]);
    }

    juce::Component *refreshComponentForRow(int rowNumber, bool isRowSelected,
                                            juce::Component *existingComponentToUpdate) override
    {
        TARow *tarow{nullptr};

        if (existingComponentToUpdate)
        {
            tarow = dynamic_cast<TARow *>(existingComponentToUpdate);

            if (!tarow)
            {
                delete existingComponentToUpdate;
            }
        }

        if (!tarow)
        {
            tarow = new TARow(this);
        }

        tarow->row = rowNumber;
        tarow->isSelected = isRowSelected;
        tarow->updateAccessibility();

        return tarow;
    }
};

struct TypeAheadListBox : public juce::ListBox
{
    TypeAheadListBox(const juce::String &s, juce::ListBoxModel *m) : juce::ListBox(s, m)
    {
        setOutlineThickness(1);
        setAccessible(true);
    }

    ~TypeAheadListBox() { setModel(nullptr); }

    void paintOverChildren(juce::Graphics &graphics) override
    {
        juce::ListBox::paintOverChildren(graphics);

        if (auto m = dynamic_cast<TypeAheadListBoxModel *>(getListBoxModel()))
        {
            m->provider->paintOverChildren(graphics,
                                           getLocalBounds().reduced(getOutlineThickness()));
        }
    }

    bool keyPressed(const juce::KeyPress &press) override
    {
        if (press.isKeyCode(juce::KeyPress::escapeKey))
        {
            if (auto m = dynamic_cast<TypeAheadListBoxModel *>(getListBoxModel()))
            {
                m->escapeKeyPressed();

                return true;
            }
        }

        return ListBox::keyPressed(press);
    }

    void focusLost(FocusChangeType cause) override
    {
        if (auto m = dynamic_cast<TypeAheadListBoxModel *>(getListBoxModel()))
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
    setTitle(l);
    lbox->setTitle(l);
    setColour(ColourIds::borderid, juce::Colours::black);
    setColour(ColourIds::emptyBackgroundId, juce::Colours::white);
    fixupJuceTextEditorAccessibility(*this);
}

TypeAhead::~TypeAhead() = default;

void TypeAhead::dismissWithValue(int providerIdx, const std::string &s,
                                 const juce::ModifierKeys &mod)
{
    bool cmd = mod.isCommandDown() || mod.isCtrlDown();
    bool doDismiss = (dismissMode == DISMISS_ON_RETURN) ||
                     (dismissMode == DISMISS_ON_RETURN_RETAIN_ON_CMD_RETURN && !cmd) ||
                     (dismissMode == DISMISS_ON_CMD_RETURN_RETAIN_ON_RETURN && cmd) ||
                     lboxmodel->getNumRows() <= 1;

    setText(s, juce::NotificationType::dontSendNotification);

    if (doDismiss)
    {
        lbox->setVisible(false);

        if (isVisible())
        {
            grabKeyboardFocus();
        }
    }

    lbox->selectRow(providerIdx);
    lbox->repaint();
    for (auto l : taList)
    {
        l->itemSelected(providerIdx);
    }
}

void TypeAhead::dismissWithoutValue()
{
    lbox->setVisible(false);

    if (isShowing())
        grabKeyboardFocus();

    for (auto l : taList)
    {
        l->typeaheadCanceled();
    }
}

void TypeAhead::updateSelected(int providerIdx)
{
    for (auto l : taList)
    {
        l->itemFocused(providerIdx);
    }
}

void TypeAhead::textEditorEscapeKeyPressed(juce::TextEditor &editor)
{
    lbox->setVisible(false);

    for (auto l : taList)
    {
        l->typeaheadCanceled();
    }
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
    {
        auto mf = dynamic_cast<Surge::Widgets::MainFrame *>(p);

        if (mf)
        {
            mf->addChildComponentThroughEditor(*lbox);
        }
    }
}

void TypeAhead::searchAndShowLBox()
{
    lboxmodel->setSearch(getText().toStdString());

    showLbox();

    lbox->updateContent();
    lbox->repaint();
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
    lastSearch = editor.getText().toStdString();
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
            lastSearch = "";
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
    {
        lbox->setColour(juce::ListBox::ColourIds::backgroundColourId,
                        findColour(ColourIds::emptyBackgroundId));
    }

    if (isColourSpecified(ColourIds::borderid))
    {
        lbox->setColour(juce::ListBox::ColourIds::outlineColourId, findColour(ColourIds::borderid));
    }
}

void TypeAhead::focusLost(juce::Component::FocusChangeType type)
{
    if (hasKeyboardFocus(true))
    {
        return;
    }

    if (lbox->hasKeyboardFocus(true))
    {
        return;
    }

    lbox->setVisible(false);

    for (auto l : taList)
    {
        l->typeaheadCanceled();
    }

    setHighlightedRegion(juce::Range(-1, -1));
}
} // namespace Widgets
} // namespace Surge