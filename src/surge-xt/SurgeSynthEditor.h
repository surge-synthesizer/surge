/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2023, various authors, as described in the GitHub
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

#ifndef SURGE_SRC_SURGE_XT_SURGESYNTHEDITOR_H
#define SURGE_SRC_SURGE_XT_SURGESYNTHEDITOR_H

#include "SurgeSynthProcessor.h"
#include "SkinSupport.h"

#include "juce_audio_utils/juce_audio_utils.h"

#include <forward_list>

class SurgeGUIEditor;
class SurgeJUCELookAndFeel;

//==============================================================================
/**
 */
class SurgeSynthEditor : public juce::AudioProcessorEditor,
                         public juce::AsyncUpdater,
                         public juce::FileDragAndDropTarget,
                         public juce::KeyListener
{
  public:
    SurgeSynthEditor(SurgeSynthProcessor &);
    ~SurgeSynthEditor();

    static constexpr int extraYSpaceForVirtualKeyboard = 50;

    //==============================================================================
    void paint(juce::Graphics &) override;
    void resized() override;
    int rezoomGuard{0};
    struct BlockRezoom
    {
        BlockRezoom(SurgeSynthEditor *ed) : editor(ed) { editor->rezoomGuard++; }
        ~BlockRezoom() { editor->rezoomGuard--; }
        SurgeSynthEditor *editor{nullptr};
    };
    void parentHierarchyChanged() override;
    void resetWindowFromSkin();

    void paramsChangedCallback();
    void setEffectType(int i);

    void handleAsyncUpdate() override;

    void populateForStreaming(SurgeSynthesizer *s);
    void populateFromStreaming(SurgeSynthesizer *s);

    void beginParameterEdit(Parameter *p);
    void endParameterEdit(Parameter *p);

    void beginMacroEdit(long macroNum);
    void endMacroEdit(long macroNum);

    int forwardedKeypressCount{0};
    std::array<bool, 256> forwardedKeypresses;
    bool keyPressed(const juce::KeyPress &key, juce::Component *originatingComponent) override;
    bool keyStateChanged(bool isKeyDown, juce::Component *originatingComponent) override;

    void setVKBLayout(const std::string layout);

    void reapplySurgeComponentColours();

    struct IdleTimer : juce::Timer
    {
        IdleTimer(SurgeSynthEditor *ed) : ed(ed) {}
        ~IdleTimer() = default;
        void timerCallback() override;
        SurgeSynthEditor *ed;
    };
    void idle();
    std::unique_ptr<IdleTimer> idleTimer;

    bool drawExtendedControls{false};
    int midiKeyboardOctave{5};
    float midiKeyboardVelocity{127.f / 127.f}; // see issue #6409
    void setPitchModSustainGUI(int pitch, int mod, int sus);
    std::unique_ptr<juce::Component> pitchwheel, modwheel, suspedal;
    std::unique_ptr<juce::MidiKeyboardComponent> keyboard;
    std::unique_ptr<juce::Label> tempoLabel, sustainLabel;
    std::unique_ptr<juce::TextEditor> tempoTypein;

    /* Drag and drop */
    bool isInterestedInFileDrag(const juce::StringArray &files) override;
    void filesDropped(const juce::StringArray &files, int, int) override;

    juce::PopupMenu hostMenuFor(Parameter *p);
    juce::PopupMenu hostMenuForMacro(int macro);

    friend class SurgeGUIEditor;

    // clang-format off
    std::forward_list<std::pair<std::string, std::vector<int>>> vkbLayouts =
    {
        {"QWERTY",          {'a', 'w', 's', 'e', 'd', 'f', 't', 'g', 'y', 'h', 'u', 'j', 'k', 'o', 'l', 'p', 186, 219, 222, 221}},
        {"QWERTZ (DE)",     {'a', 'w', 's', 'e', 'd', 'f', 't', 'g', 'z', 'h', 'u', 'j', 'k', 'o', 'l', 'p', 192, 186, 222, 187, 191}},
        {"QWERTZ (Slavic)", {'a', 'w', 's', 'e', 'd', 'f', 't', 'g', 'z', 'h', 'u', 'j', 'k', 'o', 'l', 'p', 186, 219, 222, 221, 220}},
        {"AZERTY",          {'q', 'z', 's', 'e', 'd', 'f', 't', 'g', 'y', 'h', 'u', 'j', 'k', 'o', 'l', 'p', 'm'}},
        {"Dvorak",          {'a', ',', 'o', '.', 'e', 'u', 'y', 'i', 'f', 'd', 'g', 'h', 't', 'r', 'n', 'l', 's', 191, 189, 187}},
        {"Colemak",         {'a', 'w', 'r', 'f', 's', 't', 'g', 'd', 'j', 'h', 'l', 'n', 'e', 'y', 'i', 186, 'o', 219, 222, 221}},
        {"Workman",         {'a', 'd', 's', 'r', 'h', 't', 'b', 'g', 'j', 'y', 'f', 'n', 'e', 'p', 'o', 186, 'i', 219, 222, 221}}
    };
    // clang-format on

  private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SurgeSynthProcessor &processor;

    std::unique_ptr<SurgeGUIEditor> sge;
    std::unique_ptr<juce::Drawable> logo;

    std::unique_ptr<SurgeJUCELookAndFeel> surgeLF;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SurgeSynthEditor)
};

#endif // SURGE_SRC_SURGE_XT_SURGESYNTHEDITOR_H
