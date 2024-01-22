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

#include "OpenSoundControlSettings.h"
#include "RuntimeFont.h"
#include "SurgeGUIEditor.h"
#include "SurgeGUIUtils.h"
#include "widgets/TypeAheadTextEditor.h"
#include "widgets/SurgeTextButton.h"
#include "AccessibleHelpers.h"

namespace Surge
{
namespace Overlays
{

OpenSoundControlSettings::OpenSoundControlSettings()
{
    setAccessible(true);
    setWantsKeyboardFocus(true);

    auto makeEd = [this](const std::string &n) {
        auto ed = std::make_unique<juce::TextEditor>(n);
        ed->setJustification(juce::Justification::centredLeft);
        ed->setWantsKeyboardFocus(true);
        Surge::Widgets::fixupJuceTextEditorAccessibility(*ed);
        ed->setTitle(n);
        ed->setSelectAllWhenFocused(true);
        ed->setWantsKeyboardFocus(true);
        addAndMakeVisible(*ed);
        return std::move(ed);
    };

    inPort = makeEd("In Port");
    inPort->addListener(this);
    addAndMakeVisible(*inPort);

    inPortL = std::make_unique<juce::Label>("In Port", "In Port");
    addAndMakeVisible(*inPortL);

    inPortReset = std::make_unique<Widgets::SurgeTextButton>("Reset In Port");
    inPortReset->addListener(this);
    addAndMakeVisible(*inPortReset);

    outPort = makeEd("Out Port");
    outPort->setText("456 fixme");
    outPort->addListener(this);
    addAndMakeVisible(*outPort);

    outPortL = std::make_unique<juce::Label>("Out Port", "Out Port");
    addAndMakeVisible(*outPortL);

    outPortReset = std::make_unique<Widgets::SurgeTextButton>("Reset Out Port");
    outPortReset->addListener(this);
    addAndMakeVisible(*outPortReset);

    outIP = makeEd("Out IP");
    outIP->setText("780 fixme");
    outIP->addListener(this);
    addAndMakeVisible(*outIP);

    outIPL = std::make_unique<juce::Label>("Out IP", "Out IP");
    addAndMakeVisible(*outIPL);

    outIPReset = std::make_unique<Widgets::SurgeTextButton>("Reset IP");
    outIPReset->addListener(this);
    addAndMakeVisible(*outIPReset);

    enableIn = std::make_unique<juce::ToggleButton>("Enable OSC In");
    enableIn->addListener(this);
    addAndMakeVisible(*enableIn);

    enableOut = std::make_unique<juce::ToggleButton>("Enable OSC Out");
    enableOut->addListener(this);
    addAndMakeVisible(*enableOut);

    showSpec = std::make_unique<Widgets::SurgeTextButton>("Show Open Sound Control Spec");
    showSpec->addListener(this);
    addAndMakeVisible(*showSpec);

    OSCHeader = std::make_unique<juce::Label>("OSC Header", "Open Sound Control Settings");
    OSCHeader->setJustificationType(juce::Justification::centredTop);
    addAndMakeVisible(*OSCHeader);
}

OpenSoundControlSettings::~OpenSoundControlSettings() = default;

void OpenSoundControlSettings::setStorage(SurgeStorage *s) { storage = s; }

void OpenSoundControlSettings::paint(juce::Graphics &g)
{
    g.fillAll(skin->getColor(Colors::Dialog::Background));
}

void OpenSoundControlSettings::setSurgeGUIEditor(SurgeGUIEditor *e)
{
    editor = e;
    setValuesFromEditor();
}

void OpenSoundControlSettings::shownInParent()
{
    if (inPort && inPort->isShowing())
    {
        inPort->grabKeyboardFocus();
    }
}

void OpenSoundControlSettings::onSkinChanged()
{
    auto resetColors = [this](const auto &typein) {
        typein->setFont(skin->getFont(Fonts::PatchStore::TextEntry));
        typein->setColour(juce::TextEditor::backgroundColourId,
                          skin->getColor(Colors::Dialog::Entry::Background));
        typein->setColour(juce::TextEditor::textColourId,
                          skin->getColor(Colors::Dialog::Entry::Text));
        typein->setColour(juce::TextEditor::highlightedTextColourId,
                          skin->getColor(Colors::Dialog::Entry::Text));
        typein->setColour(juce::TextEditor::highlightColourId,
                          skin->getColor(Colors::Dialog::Entry::Focus));
        typein->setColour(juce::TextEditor::outlineColourId,
                          skin->getColor(Colors::Dialog::Entry::Border));
        typein->setColour(juce::TextEditor::focusedOutlineColourId,
                          skin->getColor(Colors::Dialog::Entry::Border));

        typein->applyColourToAllText(skin->getColor(Colors::Dialog::Entry::Text));
    };

    auto resetLabel = [this](const auto &label) {
        label->setFont(skin->getFont(Fonts::PatchStore::Label));
        label->setColour(juce::Label::textColourId, skin->getColor(Colors::Dialog::Label::Text));
        label->setJustificationType(juce::Justification::centred);
    };

    resetColors(inPort);
    resetColors(outPort);
    resetColors(outIP);

    resetLabel(inPortL);
    resetLabel(outPortL);
    resetLabel(outIPL);
    resetLabel(OSCHeader);
    OSCHeader->setFont(OSCHeader->getFont().withHeight(30));

    enableIn->setColour(juce::ToggleButton::tickDisabledColourId,
                        skin->getColor(Colors::Dialog::Checkbox::Border));
    enableIn->setColour(juce::ToggleButton::tickColourId,
                        skin->getColor(Colors::Dialog::Checkbox::Tick));

    enableOut->setColour(juce::ToggleButton::tickDisabledColourId,
                         skin->getColor(Colors::Dialog::Checkbox::Border));
    enableOut->setColour(juce::ToggleButton::tickColourId,
                         skin->getColor(Colors::Dialog::Checkbox::Tick));

    inPortReset->setSkin(skin, associatedBitmapStore);
    outPortReset->setSkin(skin, associatedBitmapStore);
    outIPReset->setSkin(skin, associatedBitmapStore);
    showSpec->setSkin(skin, associatedBitmapStore);
}

void OpenSoundControlSettings::resized()
{

    // overall size set in SurgeGUIEditorOverlays.cpp when created
    auto uheight = 25;
    auto ushift = 28;
    auto col = getLocalBounds().withTrimmedTop(50).reduced(20, 0);
    col = col.withWidth(col.getWidth() / 3);

    OSCHeader->setBounds(getLocalBounds().withHeight(30).translated(0, 3));

    {
        auto lcol = col.withHeight(uheight).reduced(5, 0);
        enableIn->setBounds(lcol);
        lcol = lcol.translated(0, ushift);
        inPortL->setBounds(lcol);
        lcol = lcol.translated(0, ushift);
        inPort->setBounds(lcol);
        inPort->setIndents(4, (inPort->getHeight() - inPort->getTextHeight()) / 2);

        lcol = lcol.translated(0, ushift);
        inPortReset->setBounds(lcol);
    }

    col = col.translated(col.getWidth(), 0);
    {
        auto lcol = col.withHeight(uheight).reduced(5, 0);
        enableOut->setBounds(lcol);
        lcol = lcol.translated(0, ushift);
        outPortL->setBounds(lcol);
        lcol = lcol.translated(0, ushift);
        outPort->setBounds(lcol);
        outPort->setIndents(4, (outPort->getHeight() - outPort->getTextHeight()) / 2);

        lcol = lcol.translated(0, ushift);
        outPortReset->setBounds(lcol);
    }

    col = col.translated(col.getWidth(), 0);
    {
        auto lcol = col.withHeight(uheight).reduced(5, 0);
        lcol = lcol.translated(0, ushift);
        outIPL->setBounds(lcol);
        lcol = lcol.translated(0, ushift);
        outIP->setBounds(lcol);
        outIP->setIndents(4, (outIP->getHeight() - outIP->getTextHeight()) / 2);

        lcol = lcol.translated(0, ushift);
        outIPReset->setBounds(lcol);
    }

    auto spec = getLocalBounds().withHeight(uheight).translated(0, 6 * ushift).reduced(40, 0);
    showSpec->setBounds(spec);
}

void OpenSoundControlSettings::setValuesFromEditor()
{
    if (!editor)
        return;

    enableIn->setToggleState(editor->synth->storage.oscListenerRunning, juce::dontSendNotification);
    enableOut->setToggleState(editor->synth->storage.oscSending, juce::dontSendNotification);

    inPort->setText(std::to_string(editor->synth->storage.oscPortIn), juce::dontSendNotification);
    outPort->setText(std::to_string(editor->synth->storage.oscPortOut), juce::dontSendNotification);
    outIP->setText(editor->synth->storage.oscOutIP, juce::dontSendNotification);
}

#define LOG_CALLBACK std::cout << "OpenSoundControlSettings.cpp:" << __LINE__ << " "

void OpenSoundControlSettings::buttonClicked(juce::Button *button)
{
    if (!editor)
        return;

    if (!storage)
        return;

    auto synth = editor->synth;

    if (button == enableIn.get())
    {
        LOG_CALLBACK << "enableIn is now " << (enableIn->getToggleState() ? "on" : "off")
                     << std::endl;
    }
    if (button == enableOut.get())
    {
        LOG_CALLBACK << "enableOut is now " << (enableOut->getToggleState() ? "on" : "off")
                     << std::endl;
    }

    if (button == inPortReset.get())
    {
        LOG_CALLBACK << "Reset IN Port" << std::endl;
        // probably want to do something like set it up then setValuesFromEditor() after you do here
    }
    if (button == outPortReset.get())
    {
        LOG_CALLBACK << "Reset Out Port" << std::endl;
    }
    if (button == outIPReset.get())
    {
        LOG_CALLBACK << "Reset Out IP" << std::endl;
    }

    if (button == showSpec.get())
    {
        LOG_CALLBACK << "Show Spec" << std::endl;
    }
}

void OpenSoundControlSettings::textEditorTextChanged(juce::TextEditor &ed)
{
    if (!editor)
        return;

    if (!storage)
        return;

    auto synth = editor->synth;

    if (&ed == inPort.get())
    {
        LOG_CALLBACK << "inPort changed to '" << inPort->getText() << "'" << std::endl;
    }

    if (&ed == outPort.get())
    {
        LOG_CALLBACK << "outPort changed to '" << outPort->getText() << "'" << std::endl;
    }

    if (&ed == outIP.get())
    {
        LOG_CALLBACK << "outIP changed to '" << outIP->getText() << "'" << std::endl;
    }
}

} // namespace Overlays
} // namespace Surge