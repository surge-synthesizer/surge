// SPDX-License-Identifier: GPL-3.0-or-later
// Included in the JUCE windowing translation unit, on the browser main thread.
namespace
{
struct BrowserAccessibleEntry
{
    int id;
    Component::SafePointer<Component> component;
    bool seen = false;
};
std::vector<BrowserAccessibleEntry> browserAccessibleEntries;
int browserAccessibleNextId = 0;
int browserAccessibleCompositionId = 0;
uint64_t browserAccessibleCompositionSerial = 0;
Component::SafePointer<Component> browserAccessibleCompositionComponent;
var browserAccessibleNode(AccessibilityHandler &handler)
{
    auto &component = handler.getComponent();
    if (!component.isShowing() || component.isCurrentlyBlockedByAnotherModalComponent()) return {};
    auto found = std::find_if(browserAccessibleEntries.begin(), browserAccessibleEntries.end(),
        [&component](const auto &entry) { return entry.component.getComponent() == &component; });
    if (found == browserAccessibleEntries.end())
    {
        browserAccessibleEntries.push_back({++browserAccessibleNextId, &component, true});
        found = std::prev(browserAccessibleEntries.end());
    }
    found->seen = true;
    auto *data = new DynamicObject();
    var result(data);
    data->setProperty("id", found->id);
    if (auto *peer = dynamic_cast<BrowserPeer *>(component.getPeer()))
        data->setProperty("peer", peer->accessibilityCanvasId());
    data->setProperty("role", static_cast<int>(handler.getRole()));
    data->setProperty("label", handler.getTitle());
    data->setProperty("description", handler.getDescription());
    data->setProperty("help", handler.getHelp());
    const auto state = handler.getCurrentState();
    data->setProperty("ignored", handler.isIgnored());
    data->setProperty("disabled", !component.isEnabled());
    data->setProperty("focusable", state.isFocusable());
    data->setProperty("nativeKeys", component.getProperties().getWithDefault("surgeNativeKeyNavigation", false));
    data->setProperty("codeEditorKeys", component.getProperties().getWithDefault("surgeCodeEditorKeys", false));
    data->setProperty("captureKeys", component.getProperties().getWithDefault("surgeNativeKeyCapture", false));
    data->setProperty("focused", component.hasKeyboardFocus(false));
    if (state.isCheckable()) data->setProperty("checked", state.isChecked());
    if (state.isSelectable()) data->setProperty("selected", state.isSelected());
    if (state.isExpandable()) data->setProperty("expanded", state.isExpanded());
    const auto bounds = component.getScreenBounds();
    data->setProperty("bounds", var(Array<var>{bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight()}));
    const auto &actions = handler.getActions();
    data->setProperty("press", actions.contains(AccessibilityActionType::press));
    data->setProperty("toggle", actions.contains(AccessibilityActionType::toggle));
    data->setProperty("menu", actions.contains(AccessibilityActionType::showMenu));
    if (auto *value = handler.getValueInterface())
    {
        data->setProperty("valueText", value->getCurrentValueAsString());
        data->setProperty("readonly", value->isReadOnly());
        const auto range = value->getRange();
        if (range.isValid())
        {
            data->setProperty("value", value->getCurrentValue());
            data->setProperty("min", range.getMinimumValue());
            data->setProperty("max", range.getMaximumValue());
            data->setProperty("step", range.getInterval());
        }
    }
    if (auto *text = handler.getTextInterface(); text && !text->isDisplayingProtectedText())
    {
        data->setProperty("text", text->getAllText());
        data->setProperty("textReadonly", text->isReadOnly());
        const auto selection = text->getSelection();
        data->setProperty("selectionStart", selection.getStart());
        data->setProperty("selectionEnd", selection.getEnd());
        auto *editor = dynamic_cast<TextEditor *>(&component);
        data->setProperty("multiline", !editor || editor->isMultiLine());
    }
    Array<var> children;
    for (auto *child : handler.getChildren())
        if (auto node = browserAccessibleNode(*child); !node.isVoid()) children.add(node);
    data->setProperty("children", var(children));
    return result;
}
}
extern "C" EMSCRIPTEN_KEEPALIVE const char *surge_accessibility_snapshot()
{
    for (auto &entry : browserAccessibleEntries) entry.seen = false;
    Array<var> roots;
    auto &desktop = Desktop::getInstance();
    for (int i = 0; i < desktop.getNumComponents(); ++i)
        if (auto *handler = desktop.getComponent(i)->getAccessibilityHandler())
            if (auto node = browserAccessibleNode(*handler); !node.isVoid()) roots.add(node);
    // Custom JUCE controls can contain visible editors without being focus
    // containers. Their handler's getChildren() then omits those editors.
    // Include these real text targets, preserving their own metadata/actions.
    const auto addMissingTextTargets = [&](auto &&self, Component &component) -> void {
        if (!component.isShowing()) return;
        if (dynamic_cast<TextInputTarget *>(&component))
        {
            const bool included = std::any_of(browserAccessibleEntries.begin(), browserAccessibleEntries.end(),
                [&component](const auto &entry) { return entry.seen && entry.component.getComponent() == &component; });
            if (!included)
                if (auto *handler = component.getAccessibilityHandler())
                    if (auto node = browserAccessibleNode(*handler); !node.isVoid()) roots.add(node);
        }
        for (auto *child : component.getChildren()) self(self, *child);
    };
    for (int i = 0; i < desktop.getNumComponents(); ++i)
        addMissingTextTargets(addMissingTextTargets, *desktop.getComponent(i));
    browserAccessibleEntries.erase(std::remove_if(browserAccessibleEntries.begin(), browserAccessibleEntries.end(),
        [](const auto &entry) { return !entry.seen || entry.component == nullptr; }), browserAccessibleEntries.end());
    static String json;
    json = JSON::toString(var(roots), true);
    return json.toRawUTF8();
}
extern "C" EMSCRIPTEN_KEEPALIVE int surge_accessibility_action(int id, int action, double value)
{
    auto found = std::find_if(browserAccessibleEntries.begin(), browserAccessibleEntries.end(),
        [id](const auto &entry) { return entry.id == id; });
    if (found == browserAccessibleEntries.end()) return 0;
    auto *component = found->component.getComponent();
    if (!component || !component->isShowing() || !component->isEnabled() || component->isCurrentlyBlockedByAnotherModalComponent()) return 0;
    auto *handler = component->getAccessibilityHandler();
    if (!handler || handler->isIgnored()) return 0;
    if (action == 5)
    {
        if (auto *peer = component->getPeer()) peer->handleFocusLoss();
        return 1;
    }
    if (action == 2)
    {
        if (handler->getRole() == AccessibilityRole::menuItem)
            return handler->getActions().invoke(AccessibilityActionType::focus) ? 1 : 0;
        Component::SafePointer<Component> alive(component);
        if (alive) alive->grabKeyboardFocus();
        if (alive)
            if (auto *peer = dynamic_cast<BrowserPeer *>(alive->getPeer())) peer->syncInput();
        return 1;
    }
    if (action == 6)
    {
        // Accessible popup actions can retire the focused DOM element. Keep
        // dismissal available, but only through a live, unblocked popup root.
        if (handler->getRole() != AccessibilityRole::popupMenu) return 0;
        return PopupMenu::dismissAllActiveMenus() ? 1 : 0;
    }
    if (action == 7)
    {
        if (handler->getRole() != AccessibilityRole::popupMenu) return 0;
        int key = 0;
        if (value == 13) key = KeyPress::returnKey;
        else if (value == 32) key = KeyPress::spaceKey;
        else if (value == 37) key = KeyPress::leftKey;
        else if (value == 38) key = KeyPress::upKey;
        else if (value == 39) key = KeyPress::rightKey;
        else if (value == 40) key = KeyPress::downKey;
        else return 0;
        return component->keyPressed(KeyPress(key, ModifierKeys{}, 0)) ? 1 : 0;
    }
    if (action == 4)
    {
        auto *interface = handler->getValueInterface();
        if (!interface || interface->isReadOnly() || !std::isfinite(value)) return 0;
        auto range = interface->getRange();
        if (!range.isValid() || value < range.getMinimumValue() || value > range.getMaximumValue()) return 0;
        interface->setValue(value);
        return 1;
    }
    if (action < 0 || action > 3) return 0;
    if ((action == 0 || action == 3) && handler->getRole() == AccessibilityRole::menuItem)
    {
        Component::SafePointer<Component> alive(component);
        handler->getActions().invoke(AccessibilityActionType::focus);
        if (!alive) return 0;
        handler = alive->getAccessibilityHandler();
        if (!handler) return 0;
    }
    return handler->getActions().invoke(static_cast<AccessibilityActionType>(action));
}

namespace
{
Component *browserAccessibleTextComponent(int id)
{
    for (auto &entry : browserAccessibleEntries)
        if (entry.id == id)
            if (auto *component = entry.component.getComponent())
                if (component->isShowing() && component->isEnabled() &&
                    !component->isCurrentlyBlockedByAnotherModalComponent() &&
                    Component::getCurrentlyFocusedComponent() == component &&
                    component->getPeer() && component->getPeer()->isFocused())
                    return component;
    return nullptr;
}
}
extern "C" EMSCRIPTEN_KEEPALIVE int surge_accessibility_text(int id, char *previous, char *replacement, int start, int end)
{
    const String before = String::fromUTF8(previous), after = String::fromUTF8(replacement);
    std::free(previous); std::free(replacement);
    auto *component = browserAccessibleTextComponent(id);
    if (!component) return 0;
    auto *handler = component->getAccessibilityHandler();
    auto *text = handler ? handler->getTextInterface() : nullptr;
    auto *target = dynamic_cast<TextInputTarget *>(component);
    if (!text || text->isDisplayingProtectedText()) return 0;
    if (before != text->getAllText() || start < 0 || end < start || end > after.length()) return 0;
    if (before == after) { text->setSelection({start, end}); return 1; }
    if (text->isReadOnly() || !target || !target->isTextInputActive()) return 0;
    if (before != after)
    {
        int prefix = 0, suffix = 0;
        const int beforeLength = before.length(), afterLength = after.length();
        auto a = before.getCharPointer(), b = after.getCharPointer();
        while (prefix < beforeLength && prefix < afterLength && *a == *b) { ++prefix; ++a; ++b; }
        a = before.getCharPointer().findTerminatingNull();
        b = after.getCharPointer().findTerminatingNull();
        while (suffix < beforeLength - prefix && suffix < afterLength - prefix)
        {
            --a; --b;
            if (*a != *b) break;
            ++suffix;
        }
        const bool composing = isBrowserCompositionActive(target);
        if (!composing) target->beginInputMethodTransaction();
        target->setHighlightedRegion({prefix, before.length()-suffix});
        target->insertTextAtCaret(after.substring(prefix, after.length()-suffix));
        if (!composing) target->beginInputMethodTransaction();
    }
    const int length = text->getTotalNumCharacters();
    target->setHighlightedRegion({jmin(start, length), jmin(end, length)});
    return 1;
}
extern "C" EMSCRIPTEN_KEEPALIVE void surge_accessibility_composition(int id, int begin)
{
    if (!begin)
    {
        if (id != browserAccessibleCompositionId) return;
        browserAccessibleCompositionId = 0;
        auto owner = browserAccessibleCompositionComponent;
        browserAccessibleCompositionComponent = nullptr;
        // A canvas EditContext may already own a different composition after
        // the old DOM editor was removed without a blur event.
        if (owner != nullptr && browserCompositionComponent == owner &&
            browserAccessibleCompositionSerial == browserCompositionSerial)
        {
            browserCompositionTarget = nullptr;
            browserCompositionComponent = nullptr;
            if (auto *target = dynamic_cast<TextInputTarget *>(owner.getComponent()))
                target->beginInputMethodTransaction();
        }
        return;
    }
    if (auto *component = browserAccessibleTextComponent(id))
        if (auto *target = dynamic_cast<TextInputTarget *>(component); target && target->isTextInputActive())
        {
            browserAccessibleCompositionSerial = ++browserCompositionSerial;
            browserCompositionTarget = nullptr;
            browserCompositionComponent = nullptr;
            target->beginInputMethodTransaction();
            browserAccessibleCompositionId = id;
            browserAccessibleCompositionComponent = component;
            browserCompositionTarget = target;
            browserCompositionComponent = component;
        }
}
extern "C" EMSCRIPTEN_KEEPALIVE int surge_accessibility_key(int id, int code, int flags)
{
    auto *component = browserAccessibleTextComponent(id);
    if (!component) return 0;
    auto *peer = component->getPeer();
    const auto previous = ModifierKeys::currentModifiers;
    if (flags & 8) flags = (flags & ~8) | ModifierKeys::commandModifier;
    ModifierKeys::currentModifiers = ModifierKeys(flags);
    const bool handled = peer->handleKeyPress(browserKeyCode(code), 0);
    ModifierKeys::currentModifiers = previous;
    return handled;
}

extern "C" EMSCRIPTEN_KEEPALIVE int surge_accessibility_character(int id, int character)
{
    auto *component = browserAccessibleTextComponent(id);
    if (!component || !static_cast<bool>(component->getProperties().getWithDefault("surgeCodeEditorKeys", false)))
        return 0;
    auto *target = dynamic_cast<TextInputTarget *>(component);
    auto *handler = component->getAccessibilityHandler();
    auto *text = handler ? handler->getTextInterface() : nullptr;
    if (!target || !target->isTextInputActive() || !text || text->isReadOnly() ||
        isBrowserCompositionActive(target) || character <= 0 || character > 0x10ffff ||
        (character >= 0xd800 && character <= 0xdfff))
        return 0;
    const auto previous = ModifierKeys::currentModifiers;
    ModifierKeys::currentModifiers = ModifierKeys();
    const bool handled = component->getPeer()->handleKeyPress(character, static_cast<juce_wchar>(character));
    ModifierKeys::currentModifiers = previous;
    return handled;
}

void browserPostAccessibilityAnnouncement(const String &text, AccessibilityHandler::AnnouncementPriority priority)
{
    auto announce = [text, priority] {
        EM_ASM({ globalThis.SurgeAccessibility?.announce(UTF8ToString($0), $1); },
            text.toRawUTF8(), static_cast<int>(priority));
    };
    if (MessageManager::getInstance()->isThisTheMessageThread()) announce();
    else MessageManager::callAsync(std::move(announce));
}
