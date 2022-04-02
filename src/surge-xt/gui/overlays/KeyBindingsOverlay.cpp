/*
 ** Surge Synthesizer is Free and Open Source Software
 **
 ** Surge is made available under the Gnu General Public License, v3.0
 ** https://www.gnu.org/licenses/gpl-3.0.en.html
 **
 ** Copyright 2004-2022 by various individuals as described by the Git transaction log
 **
 ** All source at: https://github.com/surge-synthesizer/surge.git
 **
 ** Surge was a commercial product from 2004-2018, with Copyright and ownership
 ** in that period held by Claes Johanson at Vember Audio. Claes made Surge
 ** open source in September 2018.
 */

#include "KeyBindingsOverlay.h"
#include <SurgeJUCELookAndFeel.h>
#include "RuntimeFont.h"

namespace Surge
{
namespace Overlays
{

struct KeyBindingsListRow : public juce::Component
{
    SurgeGUIEditor *editor{nullptr};
    Surge::GUI::KeyboardActions action;

    KeyBindingsListRow(Surge::GUI::KeyboardActions a, SurgeGUIEditor *ed) : action(a), editor(ed)
    {
        active = std::make_unique<juce::ToggleButton>();
        active->setButtonText("");
        active->onStateChange = [this]() {
            editor->keyMapManager->bindings[action].active = active->getToggleState();
        };
        active->setAccessible(true);
        active->setToggleState(editor->keyMapManager->bindings[action].active,
                               juce::dontSendNotification);
        active->setTitle(std::string("Toggle ") + Surge::GUI::keyboardActionDescription(action));
        active->setDescription(std::string("Toggle ") +
                               Surge::GUI::keyboardActionDescription(action));
        addAndMakeVisible(*active);

        name = std::make_unique<juce::Label>("name", Surge::GUI::keyboardActionDescription(action));
        name->setColour(juce::Label::textColourId, juce::Colours::white);
        name->setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
        name->setAccessible(true);
        addAndMakeVisible(*name);

        std::string desc = "";
        keyDesc = std::make_unique<juce::Label>("keyDesc", desc);
        keyDesc->setColour(juce::Label::textColourId, juce::Colours::white);
        // TODO FIXME: Font should be juce::LNF_v4::getPopupMenuFont(), just for Mac symbols!
        keyDesc->setFont(10.0);
        keyDesc->setJustificationType(juce::Justification::right);
        keyDesc->setAccessible(true);
        addAndMakeVisible(*keyDesc);

        setFocusContainerType(juce::Component::FocusContainerType::focusContainer);

        resetValues();
    }

    void resetValues()
    {
        active->setToggleState(editor->keyMapManager->bindings[action].active,
                               juce::dontSendNotification);
        active->setTitle(std::string("Toggle ") + Surge::GUI::keyboardActionDescription(action));
        active->setDescription(std::string("Toggle ") +
                               Surge::GUI::keyboardActionDescription(action));

        name->setText(Surge::GUI::keyboardActionDescription(action), juce::dontSendNotification);

        const auto &binding = editor->keyMapManager->bindings[action];
        auto flags = juce::ModifierKeys::noModifiers;

        switch (binding.modifier)
        {
        case SurgeGUIEditor::keymap_t::SHIFT:
            flags = juce::ModifierKeys::shiftModifier;
            break;
        case SurgeGUIEditor::keymap_t::COMMAND:
            flags = juce::ModifierKeys::commandModifier;
            break;
        case SurgeGUIEditor::keymap_t::CONTROL:
            flags = juce::ModifierKeys::ctrlModifier;
            break;
        case SurgeGUIEditor::keymap_t::ALT:
            flags = juce::ModifierKeys::altModifier;
            break;
        default:
            break;
        }

        auto kp = juce::KeyPress(binding.keyCode, flags, binding.keyCode);

        if (binding.type == SurgeGUIEditor::keymap_t::Binding::TEXTCHAR)
        {
            kp = juce::KeyPress(binding.textChar, flags, binding.textChar);
        }

        auto desc = kp.getTextDescription().toStdString();

#if MAC
        Surge::Storage::findReplaceSubstring(desc, "command", u8"\U00002318");
        Surge::Storage::findReplaceSubstring(desc, "option", u8"\U00002325");
        Surge::Storage::findReplaceSubstring(desc, "shift", u8"\U000021E7");
        Surge::Storage::findReplaceSubstring(desc, "ctrl", u8"\U00002303");
#else
        desc[0] = std::toupper(desc[0]);
#endif
        keyDesc->setText(desc, juce::dontSendNotification);
    }

    void setAction(Surge::GUI::KeyboardActions a)
    {
        action = a;
        resetValues();
        repaint();
    }

    void resized()
    {
        auto r = getLocalBounds().reduced(2);
        auto a1 = r.withWidth(r.getHeight() * 1.5);
        active->setBounds(a1);
        auto a2 = r.withTrimmedLeft(r.getHeight() * 1.5);
        name->setBounds(a2.withWidth(300));
        keyDesc->setBounds(a2.withTrimmedLeft(300));
    }

    std::unique_ptr<juce::ToggleButton> active;
    std::unique_ptr<juce::Label> name, keyDesc;
};

struct KeyBindingsListBoxModel : public juce::ListBoxModel
{
    SurgeGUIEditor *editor;

    KeyBindingsListBoxModel(SurgeGUIEditor *ed) : editor(ed) {}

    int getNumRows() override { return SurgeGUIEditor::keymap_t::numFuncs; }

    void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                          bool rowIsSelected) override
    {
        if (rowNumber % 2 == 1)
        {
            g.fillAll(juce::Colour(40, 40, 40));
        }
    }

    juce::Component *refreshComponentForRow(int rowNumber, bool isRowSelected,
                                            juce::Component *existingComponentToUpdate) override
    {
        auto rc = dynamic_cast<KeyBindingsListRow *>(existingComponentToUpdate);

        if (!rc)
        {
            // Should never happen but
            if (existingComponentToUpdate)
            {
                delete existingComponentToUpdate;
            }

            rc = new KeyBindingsListRow((Surge::GUI::KeyboardActions)rowNumber, editor);
        }

        rc->setAction((Surge::GUI::KeyboardActions)rowNumber);

        return rc;
    }

    juce::String getNameForRow(int rowNumber) override
    {
        auto action = (Surge::GUI::KeyboardActions)rowNumber;

        return Surge::GUI::keyboardActionDescription(action);
    }
};

KeyBindingsOverlay::KeyBindingsOverlay(SurgeStorage *st, SurgeGUIEditor *ed)
    : storage(st), editor(ed)
{
    setAccessible(true);
    setTitle("Keyboard Shortcut Editor");
    setDescription("Keyboard Shortcut Editor");
    setFocusContainerType(juce::Component::FocusContainerType::focusContainer);

    okS = std::make_unique<Surge::Widgets::SelfDrawButton>("OK");

    okS->onClick = [this]() {
        editor->keyMapManager->streamToXML();
        editor->setupKeymapManager();
        editor->closeOverlay(SurgeGUIEditor::KEYBINDINGS_EDITOR);
    };

    okS->setStorage(storage);
    addAndMakeVisible(*okS);

    cancelS = std::make_unique<Surge::Widgets::SelfDrawButton>("Cancel");

    cancelS->onClick = [this]() {
        // We need to restore to prior state here
        editor->setupKeymapManager();
        editor->closeOverlay(SurgeGUIEditor::KEYBINDINGS_EDITOR);
    };

    cancelS->setStorage(storage);
    addAndMakeVisible(*cancelS);

    bindingListBoxModel = std::make_unique<KeyBindingsListBoxModel>(editor);
    bindingList = std::make_unique<juce::ListBox>("Keyboard Shortcuts", bindingListBoxModel.get());
    addAndMakeVisible(*bindingList);
}

KeyBindingsOverlay::~KeyBindingsOverlay() = default;

void KeyBindingsOverlay::paint(juce::Graphics &g)
{
    if (!skin)
    {
        g.fillAll(juce::Colours::black);
        return;
    }

    g.fillAll(skin->getColor(Colors::MSEGEditor::Background));

    auto r = getLocalBounds().withTrimmedTop(getHeight() - okCancelAreaHeight);
    g.setColour(skin->getColor(Colors::MSEGEditor::Panel));
    g.fillRect(r);
}

void KeyBindingsOverlay::resized()
{
    auto r = getLocalBounds().withTrimmedTop(getHeight() - okCancelAreaHeight);
    auto b1 = r.withTrimmedLeft(getWidth() - 90);
    cancelS->setBounds(b1.reduced(2));
    b1 = b1.translated(-90, 0);
    okS->setBounds(b1.reduced(2));

    auto l = getLocalBounds().withTrimmedBottom(okCancelAreaHeight).reduced(1);
    bindingList->setBounds(l);
}

void KeyBindingsOverlay::onSkinChanged()
{
    okS->setSkin(skin, associatedBitmapStore);
    cancelS->setSkin(skin, associatedBitmapStore);
    repaint();
}

} // namespace Overlays
} // namespace Surge