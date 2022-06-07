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

#define UNDO_MODEL_IN_DTOR 0
#define USE_JUCE_TEXTBUTTON 0

namespace Surge
{
namespace Overlays
{

struct KeyBindingsListRow : public juce::Component
{
    SurgeGUIEditor *editor{nullptr};
    Surge::GUI::KeyboardActions action;
    KeyBindingsOverlay *overlay;

    KeyBindingsListRow(Surge::GUI::KeyboardActions a, KeyBindingsOverlay *o, SurgeGUIEditor *ed)
        : action(a), overlay(o), editor(ed)
    {
        active = std::make_unique<juce::ToggleButton>();
        active->setButtonText("");
        active->onStateChange = [this]() {
            editor->keyMapManager->bindings[action].active = active->getToggleState();
            resetValues();
        };
        active->setAccessible(true);
        active->setToggleState(editor->keyMapManager->bindings[action].active,
                               juce::dontSendNotification);
        active->setTitle(std::string("Toggle ") + Surge::GUI::keyboardActionDescription(action));
        active->setDescription(std::string("Toggle ") +
                               Surge::GUI::keyboardActionDescription(action));
        active->setColour(juce::ToggleButton::tickDisabledColourId,
                          editor->currentSkin->getColor(Colors::Dialog::Checkbox::Border));
        active->setColour(juce::ToggleButton::tickColourId,
                          editor->currentSkin->getColor(Colors::Dialog::Checkbox::Tick));

        addAndMakeVisible(*active);

        name = std::make_unique<juce::Label>("name", Surge::GUI::keyboardActionDescription(action));
        name->setColour(juce::Label::textColourId,
                        editor->currentSkin->getColor(Colors::MSEGEditor::Text));
        name->setFont(editor->currentSkin->fontManager->getLatoAtSize(9));
        name->setAccessible(true);
        addAndMakeVisible(*name);

        std::string desc = "";
        keyDesc = std::make_unique<juce::Label>("Key Binding", desc);
        keyDesc->setAccessible(true);
        keyDesc->setFont(juce::Font(10));
        addAndMakeVisible(*keyDesc);

        reset = std::make_unique<Surge::Widgets::SelfDrawButton>("Reset");
        reset->setSkin(editor->currentSkin);
        reset->setStorage(editor->getStorage());
        reset->setAccessible(true);
        reset->onClick = [this]() { resetToDefault(); };
        reset->setTitle(std::string("Reset ") + Surge::GUI::keyboardActionDescription(action));
        reset->setDescription(std::string("Reset ") +
                              Surge::GUI::keyboardActionDescription(action));

        addAndMakeVisible(*reset);

        learn = std::make_unique<Surge::Widgets::SelfDrawToggleButton>("Learn");
        learn->setSkin(editor->currentSkin);
        learn->setStorage(editor->getStorage());
        learn->setAccessible(true);
        learn->setTitle(std::string("Learn ") + Surge::GUI::keyboardActionDescription(action));
        learn->setDescription(std::string("Learn ") +
                              Surge::GUI::keyboardActionDescription(action));

        learn->onToggle = [this]() {
            if (learn->getValue() > 0.5)
            {
                overlay->isLearning = true;
                overlay->learnAction = action;
            }
            else
            {
                overlay->isLearning = false;
                overlay->learnAction = -1;
            }
        };
        addAndMakeVisible(*learn);

        setFocusContainerType(juce::Component::FocusContainerType::focusContainer);

        resetValues();
    }

    void resetValues()
    {
        name->setText(Surge::GUI::keyboardActionDescription(action), juce::dontSendNotification);

        auto act = editor->keyMapManager->bindings[action].active;
        active->setToggleState(act, juce::dontSendNotification);

        auto txtColor = editor->currentSkin->getColor(Colors::MSEGEditor::Text);

        if (act)
        {
            name->setColour(juce::Label::textColourId, txtColor);
            keyDesc->setColour(juce::Label::textColourId, txtColor);
        }
        else
        {
            keyDesc->setColour(juce::Label::textColourId, txtColor.withMultipliedAlpha(0.5));
            name->setColour(juce::Label::textColourId, txtColor.withMultipliedAlpha(0.5));
        }

        const auto &binding = editor->keyMapManager->bindings[action];
        const auto &dbinding = editor->keyMapManager->defaultBindings[action];

        reset->setDeactivated(binding == dbinding);

        int flags = juce::ModifierKeys::noModifiers;

        if (binding.modifier & SurgeGUIEditor::keymap_t::SHIFT)
            flags |= juce::ModifierKeys::shiftModifier;
        if (binding.modifier & SurgeGUIEditor::keymap_t::COMMAND)
            flags |= juce::ModifierKeys::commandModifier;
        if (binding.modifier & SurgeGUIEditor::keymap_t::CONTROL)
            flags |= juce::ModifierKeys::ctrlModifier;
        if (binding.modifier & SurgeGUIEditor::keymap_t::ALT)
            flags |= juce::ModifierKeys::altModifier;

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
        Surge::Storage::findReplaceSubstring(desc, "ctrl", "Ctrl");
        Surge::Storage::findReplaceSubstring(desc, "alt", "Alt");
        Surge::Storage::findReplaceSubstring(desc, "shift", "Shift");
#endif

        keyDesc->setText(desc, juce::dontSendNotification);
        learn->setValue(0);

        repaint();
    }

    void resetToDefault()
    {
        const auto &dbinding = editor->keyMapManager->defaultBindings[action];
        editor->keyMapManager->bindings[action] = dbinding;
        resetValues();
    }

    void setAction(Surge::GUI::KeyboardActions a)
    {
        action = a;
        resetValues();
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(2);
        auto a1 = r.withWidth(r.getHeight() * 1.5);
        auto at = r.withTrimmedLeft(r.getHeight() * 1.5);
        auto a2 = at.withTrimmedRight(120);
        auto a3 = at.withTrimmedLeft(at.getWidth() - 120);

        active->setBounds(a1);
        name->setBounds(a2.withWidth(220));
        keyDesc->setBounds(a2.withTrimmedLeft(220));
        learn->setBounds(a3.withWidth(60).reduced(1));
        reset->setBounds(a3.withTrimmedLeft(60).reduced(1));
    }

    std::unique_ptr<juce::ToggleButton> active;
    std::unique_ptr<juce::Label> name, keyDesc;

    std::unique_ptr<Surge::Widgets::SelfDrawButton> reset;
    std::unique_ptr<Surge::Widgets::SelfDrawToggleButton> learn;
};

struct KeyBindingsListBoxModel : public juce::ListBoxModel
{
    SurgeGUIEditor *editor;
    KeyBindingsOverlay *overlay;

    KeyBindingsListBoxModel(KeyBindingsOverlay *o, SurgeGUIEditor *ed) : overlay(o), editor(ed) {}

    int getNumRows() override { return SurgeGUIEditor::keymap_t::numFuncs; }

    void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                          bool rowIsSelected) override
    {
        if (rowNumber % 2 == 1)
        {
            g.fillAll(editor->currentSkin->getColor(Colors::FormulaEditor::Debugger::LightRow));
        }
        else
        {
            g.fillAll(editor->currentSkin->getColor(Colors::FormulaEditor::Debugger::Row));
        }
    }

    juce::Component *refreshComponentForRow(int rowNumber, bool isRowSelected,
                                            juce::Component *existingComponentToUpdate) override
    {
        if (rowNumber < SurgeGUIEditor::keymap_t::numFuncs)
        {
            auto rc = dynamic_cast<KeyBindingsListRow *>(existingComponentToUpdate);

            if (!rc)
            {
                // Should never happen but
                if (existingComponentToUpdate)
                {
                    delete existingComponentToUpdate;
                }

                rc =
                    new KeyBindingsListRow((Surge::GUI::KeyboardActions)rowNumber, overlay, editor);
            }

            rc->setAction((Surge::GUI::KeyboardActions)rowNumber);
            return rc;
        }

        if (existingComponentToUpdate)
        {
            delete existingComponentToUpdate;
        }

        return nullptr;
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

    resetAll = std::make_unique<Surge::Widgets::SelfDrawButton>("Reset All");
    resetAll->setSkin(editor->currentSkin);
    resetAll->setStorage(editor->getStorage());
    resetAll->setAccessible(true);
    resetAll->onClick = [this]() { this->resetAllToDefault(); };
    addAndMakeVisible(*resetAll);

    bindingListBoxModel = std::make_unique<KeyBindingsListBoxModel>(this, editor);
    bindingList = std::make_unique<juce::ListBox>("Keyboard Shortcuts", bindingListBoxModel.get());
    bindingList->updateContent();
    addAndMakeVisible(*bindingList);
}

void KeyBindingsOverlay::resetAllToDefault()
{
    for (auto const &[k, b] : editor->keyMapManager->bindings)
    {
        editor->keyMapManager->bindings[k] = editor->keyMapManager->defaultBindings[k];
    }

    bindingList->updateContent();
}

KeyBindingsOverlay::~KeyBindingsOverlay()
{
    bindingList.reset(nullptr);
    bindingListBoxModel.reset(nullptr); // order matters here
}

void KeyBindingsOverlay::paint(juce::Graphics &g)
{
    if (!skin)
    {
        return;
    }

    g.fillAll(skin->getColor(Colors::MSEGEditor::Background));

    auto r = getLocalBounds().withTrimmedTop(getHeight() - okCancelAreaHeight);
    g.setColour(skin->getColor(Colors::MSEGEditor::Panel));
    g.fillRect(r);
}

void KeyBindingsOverlay::resized()
{
    auto l = getLocalBounds().withTrimmedBottom(okCancelAreaHeight).reduced(margin);
    auto r = getLocalBounds().withTrimmedTop(getHeight() - okCancelAreaHeight);
    auto dialogCenter = getLocalBounds().getWidth() / 2;
    auto okRect = r.withTrimmedLeft(dialogCenter - btnWidth - margin)
                      .withWidth(btnWidth)
                      .withHeight(btnHeight);
    auto canRect =
        r.withTrimmedLeft(dialogCenter + margin).withWidth(btnWidth).withHeight(btnHeight);

    okRect.setCentre(okRect.getCentreX(), r.getY() + okCancelAreaHeight / 2);
    canRect.setCentre(canRect.getCentreX(), r.getY() + okCancelAreaHeight / 2);

    okS->setBounds(okRect);
    cancelS->setBounds(canRect);
    resetAll->setBounds(canRect.translated(175, 0).withWidth(58));

    bindingList->setBounds(l);
}

void KeyBindingsOverlay::onSkinChanged()
{
    okS->setSkin(skin, associatedBitmapStore);
    cancelS->setSkin(skin, associatedBitmapStore);
    resetAll->setSkin(skin, associatedBitmapStore);

    repaint();
}

bool KeyBindingsOverlay::keyPressed(const juce::KeyPress &key)
{
    if (isLearning)
    {
        auto &binding = editor->keyMapManager->bindings[(Surge::GUI::KeyboardActions)learnAction];

        binding.type = SurgeGUIEditor::keymap_t::Binding::KEYCODE;
        binding.keyCode = key.getKeyCode();
        binding.modifier = 0;
#if SST_COMMAND_CTRL_SAME_KEY
        if (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown())
            binding.modifier |= SurgeGUIEditor::keymap_t::Modifiers::COMMAND;
#else
        if (key.getModifiers().isCommandDown())
            binding.modifier |= SurgeGUIEditor::keymap_t::Modifiers::COMMAND;
        if (key.getModifiers().isCtrlDown())
            binding.modifier |= SurgeGUIEditor::keymap_t::Modifiers::CONTROL;
#endif
        if (key.getModifiers().isShiftDown())
            binding.modifier |= SurgeGUIEditor::keymap_t::Modifiers::SHIFT;
        if (key.getModifiers().isAltDown())
            binding.modifier |= SurgeGUIEditor::keymap_t::Modifiers::ALT;

        bindingList->updateContent();
        return true;
    }
    return Component::keyPressed(key);
}

} // namespace Overlays
} // namespace Surge