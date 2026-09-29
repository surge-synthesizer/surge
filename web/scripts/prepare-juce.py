#!/usr/bin/env python3
"""Create a build-local JUCE overlay; never modify the pinned submodule."""
import pathlib
import re
import sys

source, target = map(pathlib.Path, sys.argv[1:])
changes = {}

def edit(relative, old, new):
    text = changes.get(relative, (source / 'modules' / relative).read_text())
    if old not in text:
        raise RuntimeError(f'JUCE port anchor missing: {relative}: {old}')
    changes[relative] = text.replace(old, new)

edit('juce_core/juce_core.cpp', '#include "juce_core.h"', '#include "juce_core.h"\n#include "BrowserHeaders.h"')
edit('juce_core/juce_core.cpp', '#include "native/juce_SystemStats_wasm.cpp"', '#include "native/juce_SystemStats_wasm.cpp"\n #include "BrowserCore.cpp"')
# With pthreads Emscripten returns an epoch-sized monotonic clock. Converting
# that double directly to uint32 saturates in Wasm; truncate through uint64
# to preserve JUCE's intended wrapping millisecond counter.
edit('juce_core/native/juce_SystemStats_wasm.cpp',
     'return static_cast<uint32> (emscripten_get_now());',
     'return static_cast<uint32> (static_cast<uint64> (emscripten_get_now()));')
# Emscripten implements stat, mmap, and POSIX threading in its virtual filesystem.
edit('juce_core/native/juce_SharedCode_posix.h', '#if ! JUCE_WASM', '#if 1')
edit('juce_events/juce_events.cpp', '#elif JUCE_ANDROID\n #include "native/juce_Messaging_android.cpp"', '#elif JUCE_WASM\n #include "BrowserEvents.cpp"\n#elif JUCE_ANDROID\n #include "native/juce_Messaging_android.cpp"')
# Chrome owns the UI event loop. Pump JUCE timers from the same bounded frame
# dispatch instead of relying on a native timer thread to wake the browser.
edit('juce_events/timers/juce_Timer.cpp',
     '        if (! isThreadRunning())\n            startThread (Thread::Priority::high);',
     '       #if ! JUCE_WASM\n        if (! isThreadRunning())\n            startThread (Thread::Priority::high);\n       #endif')
edit('juce_events/timers/juce_Timer.cpp',
     '    void callTimersSynchronously()\n    {\n        callTimers();\n    }',
     '''    void callTimersSynchronously()
    {
       #if JUCE_WASM
        static auto lastTick = Time::getMillisecondCounter();
        const auto now = Time::getMillisecondCounter();
        getTimeUntilFirstTimer ((int) jmin (now - lastTick, (uint32) 0x7fffffff));
        lastTick = now;
       #endif
        callTimers();
    }''')
edit('juce_graphics/juce_graphics.cpp', '#elif JUCE_LINUX || JUCE_BSD', '#elif JUCE_LINUX || JUCE_BSD || JUCE_WASM')
edit('juce_gui_basics/juce_gui_basics.cpp', '// Depends on types defined in platform-specific windowing files', '#if JUCE_WASM\n #include "BrowserWindowing.cpp"\n#endif\n// Depends on types defined in platform-specific windowing files')
edit('juce_core/native/juce_ThreadPriorities_native.h', '#if JUCE_LINUX || JUCE_BSD', '#if JUCE_LINUX || JUCE_BSD || JUCE_WASM')
edit('juce_data_structures/app_properties/juce_PropertiesFile.cpp', '#elif JUCE_WINDOWS\n    auto dir', '#elif JUCE_WINDOWS || JUCE_WASM\n    auto dir')
edit('juce_core/threads/juce_Thread.h', '#if JUCE_ANDROID || JUCE_LINUX || JUCE_BSD', '#if JUCE_ANDROID || JUCE_LINUX || JUCE_BSD || JUCE_WASM')
edit('juce_graphics/images/juce_Image.cpp', '#if JUCE_LINUX || JUCE_BSD', '#if JUCE_LINUX || JUCE_BSD || JUCE_WASM')
edit('juce_audio_processors/utilities/juce_PluginHostType.cpp', '#elif JUCE_ANDROID\n   #else', '#elif JUCE_ANDROID || JUCE_WASM\n   #else')

# Chrome clipboard APIs are asynchronous. Adapt editor commands instead of
# blocking the UI thread or reusing stale cached clipboard text.
edit('juce_gui_basics/keyboard/juce_SystemClipboard.h',
     '    static String getTextFromClipboard();',
     '    static String getTextFromClipboard();\n    static void readTextAsync (std::function<void(bool, const String&)>);\n    static void writeTextAsync (const String&, std::function<void(bool)>);\n    static void reportClipboardError (const String&);')
edit('juce_gui_basics/widgets/juce_TextEditor.cpp',
     'void TextEditor::paste()\n{\n    if (! isReadOnly())\n    {\n        auto clip = SystemClipboard::getTextFromClipboard();\n\n        if (clip.isNotEmpty())\n            insertTextAtCaret (clip);\n    }\n}',
     '''void TextEditor::paste()
{
    if (! isReadOnly())
    {
        const auto before = getText();
        const auto selection = getHighlightedRegion();
        SystemClipboard::readTextAsync ([safe = Component::SafePointer<TextEditor>(this), before, selection](bool ok, const String& clip) {
            if (! ok || clip.isEmpty()) return;
            if (safe == nullptr || safe->isReadOnly() || safe->getText() != before
                || safe->getHighlightedRegion() != selection || ! safe->hasKeyboardFocus (true))
            {
                SystemClipboard::reportClipboardError ("Paste canceled because the editor changed. Try again.");
                return;
            }
            safe->newTransaction();
            safe->insertTextAtCaret (clip);
            safe->newTransaction();
        });
    }
}''')
edit('juce_gui_basics/widgets/juce_TextEditor.cpp',
     'bool TextEditor::cutToClipboard()\n{\n    newTransaction();\n    copy();\n    cut();\n    return true;\n}',
     '''bool TextEditor::cutToClipboard()
{
    if (passwordCharacter != 0) { cut(); return true; }
    if (isReadOnly()) return true;
    const auto before = getText();
    const auto selection = getHighlightedRegion();
    const auto selected = getTextInRange (selection);
    if (selected.isEmpty()) return true;
    SystemClipboard::writeTextAsync (selected, [safe = Component::SafePointer<TextEditor>(this), before, selection](bool ok) {
        if (! ok) return;
        if (safe == nullptr || safe->isReadOnly() || safe->getText() != before
            || safe->getHighlightedRegion() != selection || ! safe->hasKeyboardFocus (true))
        {
            SystemClipboard::reportClipboardError ("Text copied; cut canceled because the editor changed.");
            return;
        }
        safe->newTransaction();
        safe->cut();
        safe->newTransaction();
    });
    return true;
}''')
edit('juce_gui_extra/code_editor/juce_CodeEditorComponent.cpp',
     'bool CodeEditorComponent::pasteFromClipboard()\n{\n    newTransaction();\n    auto clip = SystemClipboard::getTextFromClipboard();\n\n    if (clip.isNotEmpty())\n        insertText (clip);\n\n    newTransaction();\n    return true;\n}',
     '''bool CodeEditorComponent::pasteFromClipboard()
{
    if (! isReadOnly())
    {
        const auto before = document.getAllContent();
        const auto selection = getHighlightedRegion();
        SystemClipboard::readTextAsync ([safe = Component::SafePointer<CodeEditorComponent>(this), before, selection](bool ok, const String& clip) {
            if (! ok || clip.isEmpty()) return;
            if (safe == nullptr || safe->isReadOnly() || safe->document.getAllContent() != before
                || safe->getHighlightedRegion() != selection || ! safe->hasKeyboardFocus (true))
            {
                SystemClipboard::reportClipboardError ("Paste canceled because the editor changed. Try again.");
                return;
            }
            safe->newTransaction();
            safe->insertText (clip);
            safe->newTransaction();
        });
    }
    return true;
}''')
edit('juce_gui_extra/code_editor/juce_CodeEditorComponent.cpp',
     'bool CodeEditorComponent::cutToClipboard()\n{\n    copyToClipboard();\n    cut();\n    newTransaction();\n    return true;\n}',
     '''bool CodeEditorComponent::cutToClipboard()
{
    if (isReadOnly()) return true;
    const auto before = document.getAllContent();
    const auto selection = getHighlightedRegion();
    const auto selected = getTextInRange (selection);
    if (selected.isEmpty()) return true;
    SystemClipboard::writeTextAsync (selected, [safe = Component::SafePointer<CodeEditorComponent>(this), before, selection](bool ok) {
        if (! ok) return;
        if (safe == nullptr || safe->isReadOnly() || safe->document.getAllContent() != before
            || safe->getHighlightedRegion() != selection || ! safe->hasKeyboardFocus (true))
        {
            SystemClipboard::reportClipboardError ("Text copied; cut canceled because the editor changed.");
            return;
        }
        safe->newTransaction();
        safe->cut();
        safe->newTransaction();
    });
    return true;
}''')

# Keep an IME composition as one JUCE undo transaction, including pauses.
edit('juce_gui_basics/keyboard/juce_TextInputTarget.h',
     '    virtual bool isTextInputActive() const = 0;',
     '    virtual bool isTextInputActive() const = 0;\n    virtual void beginInputMethodTransaction() {}')
edit('juce_gui_basics/keyboard/juce_TextInputTarget.h',
     '} // namespace juce',
     'bool isBrowserCompositionActive (const TextInputTarget*);\n} // namespace juce')
for header in ['juce_gui_basics/widgets/juce_TextEditor.h', 'juce_gui_extra/code_editor/juce_CodeEditorComponent.h']:
    edit(header, '    bool isTextInputActive() const override;',
         '    bool isTextInputActive() const override;\n    void beginInputMethodTransaction() override { newTransaction(); }')
for implementation, name in [('juce_gui_basics/widgets/juce_TextEditor.cpp', 'TextEditor'), ('juce_gui_extra/code_editor/juce_CodeEditorComponent.cpp', 'CodeEditorComponent')]:
    anchor = f'void {name}::newTransaction()\n{{'
    edit(implementation, anchor, anchor + '\n    if (isBrowserCompositionActive (this)) return;')
edit('juce_gui_extra/code_editor/juce_CodeEditorComponent.cpp',
     'bool CodeEditorComponent::isTextInputActive() const\n{\n    return true;\n}',
     'bool CodeEditorComponent::isTextInputActive() const\n{\n    return ! readOnly;\n}')

# Forward JUCE's portable announcements to Chrome live regions.
edit('juce_gui_basics/accessibility/juce_AccessibilityHandler.cpp',
     'void AccessibilityHandler::postAnnouncement (const String&, AnnouncementPriority) {}',
     'void AccessibilityHandler::postAnnouncement (const String& text, AnnouncementPriority priority) { browserPostAccessibilityAnnouncement(text, priority); }')

for path in (source / 'modules').rglob('*'):
    if not path.is_file():
        continue
    relative = path.relative_to(source / 'modules')
    destination = target / 'modules' / relative
    content = changes[str(relative)].encode() if str(relative) in changes else path.read_bytes()
    if relative.suffix == '.cpp' and 'native' not in relative.parts:
        if re.search(rb'SystemClipboard\s*::\s*getTextFromClipboard\s*\(', content):
            raise RuntimeError(f'Unported synchronous clipboard consumer: {relative}')
    if not destination.exists() or destination.read_bytes() != content:
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(content)
