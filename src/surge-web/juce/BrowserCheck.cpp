// SPDX-License-Identifier: GPL-3.0-or-later
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <emscripten.h>
extern "C" void surge_dispatch_messages();
static int callbackCount = 0, timerCallbacks = 0;
extern "C" EMSCRIPTEN_KEEPALIVE int surge_check_timer_callbacks() { return timerCallbacks; }
static double sliderValue = 50.;
extern "C" EMSCRIPTEN_KEEPALIVE double surge_check_slider() { return sliderValue; }
static juce::String selectedFile, selectedText;
extern "C" EMSCRIPTEN_KEEPALIVE const char *surge_check_selected_file() { return selectedFile.toRawUTF8(); }
extern "C" EMSCRIPTEN_KEEPALIVE const char *surge_check_selected_text() { return selectedText.toRawUTF8(); }
extern "C" EMSCRIPTEN_KEEPALIVE int surge_check_clicks() { return callbackCount; }
static juce::Component *checkButton;
extern "C" EMSCRIPTEN_KEEPALIVE void surge_check_button_state(int enabled, int visible)
{
    checkButton->setEnabled(enabled != 0);
    checkButton->setVisible(visible != 0);
}
static juce::TextEditor *checkText;
static juce::CodeEditorComponent *checkCode;
extern "C" EMSCRIPTEN_KEEPALIVE int surge_check_composing(int code)
{
    return juce::isBrowserCompositionActive(code ? static_cast<juce::TextInputTarget *>(checkCode)
                                                : static_cast<juce::TextInputTarget *>(checkText));
}
extern "C" EMSCRIPTEN_KEEPALIVE void surge_check_announce(const char *text, int priority)
{
    if (priority >= 0 && priority <= 2)
        juce::AccessibilityHandler::postAnnouncement(juce::String::fromUTF8(text),
            static_cast<juce::AccessibilityHandler::AnnouncementPriority>(priority));
}
static juce::String checkResult;
extern "C" EMSCRIPTEN_KEEPALIVE const char *surge_check_editor_text(int code)
{
    checkResult = code ? checkCode->getDocument().getAllContent() : checkText->getText();
    return checkResult.toRawUTF8();
}
extern "C" EMSCRIPTEN_KEEPALIVE void surge_check_editor_readonly(int code, int value)
{
    if (code) checkCode->setReadOnly(value != 0);
    else checkText->setReadOnly(value != 0);
}
class BrowserCheck final : public juce::Component, private juce::Timer
{
    void timerCallback() override { ++timerCallbacks; }
    juce::Slider slider;
    juce::TextEditor textEditor;
    juce::CodeDocument codeDocument;
    juce::CodeEditorComponent codeEditor{codeDocument, nullptr};
    juce::TextButton button{"Test JUCE callback"};
    juce::Label label;
    juce::TextButton importButton{"Import a file"}, exportButton{"Export a file"};
    std::unique_ptr<juce::FileChooser> chooser;

  public:
    BrowserCheck()
    {
        setName("JUCE WebAssembly platform verification");
        setSize(700, 300);
        addAndMakeVisible(slider);
        addAndMakeVisible(button);
        addAndMakeVisible(label);
        slider.onValueChange = [this] { sliderValue = slider.getValue(); };
        slider.setTitle("Verification value");
        checkButton = &button;
        slider.setRange(0, 100);
        slider.setValue(50);
        slider.setBounds(30, 70, 640, 60);
        button.setBounds(30, 160, 250, 40);
        label.setBounds(300, 160, 350, 40);
        button.onClick = [this] {
            label.setText("Callbacks: " + juce::String(++callbackCount),
                          juce::dontSendNotification);
        };
        addAndMakeVisible(importButton);
        addAndMakeVisible(exportButton);
        importButton.setBounds(30, 220, 250, 40);
        exportButton.setBounds(300, 220, 250, 40);
        importButton.onClick = [this] {
            chooser = std::make_unique<juce::FileChooser>("Import test file", juce::File{}, "*.txt");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [](const juce::FileChooser &selection) {
                    if (selection.getResults().isEmpty()) return;
                    selectedFile = selection.getResult().getFullPathName();
                    selectedText = selection.getResult().loadFileAsString();
                });
        };
        exportButton.onClick = [this] {
            chooser = std::make_unique<juce::FileChooser>("Export test file", juce::File("/user/test.txt"));
            chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [](const juce::FileChooser &selection) {
                    if (selection.getResults().isEmpty()) return;
                    selection.getResult().replaceWithText(juce::String::fromUTF8("JUCE export ✓"));
                });
        };
        textEditor.setTitle("Verification text");
        codeEditor.setTitle("Verification code");
        textEditor.setBounds(30, 268, 300, 28);
        textEditor.setText("initial text");
        codeEditor.setBounds(360, 268, 300, 28);
        codeDocument.replaceAllContent("return 1");
        addAndMakeVisible(textEditor);
        addAndMakeVisible(codeEditor);
        checkText = &textEditor;
        checkCode = &codeEditor;
        addToDesktop(0);
        setVisible(true);
        startTimer(20);
    }
    void paint(juce::Graphics &g) override
    {
        g.fillAll(juce::Colour(0xff202328));
        g.setColour(juce::Colours::orange);
        g.setFont(24);
        g.drawText("JUCE rendering in WebAssembly", 30, 15, 640, 40,
                   juce::Justification::centredLeft);
    }
};
int main()
{
    setenv("JUCE_FONT_PATH", "/fonts", 1);
    static juce::ScopedJuceInitialiser_GUI init;
    juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypefaceName("Lato");
    static BrowserCheck check;
    emscripten_set_main_loop(
        [] {
            surge_dispatch_messages();
            for (int i = 0; i < juce::ComponentPeer::getNumPeers(); ++i)
                juce::ComponentPeer::getPeer(i)->performAnyPendingRepaintsNow();
        },
        0, false);
}
