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
#include "SurgeSynthEditor.h"
#include "SurgeSynthProcessor.h"
#include "SurgeGUIUtils.h"
#include "widgets/TypeAheadTextEditor.h"
#include "widgets/SurgeTextButton.h"
#include "AccessibleHelpers.h"
#include <regex>

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
    inPort->setJustification(juce::Justification::centred);
    inPort->addListener(this);
    addAndMakeVisible(*inPort);

    inPortL = std::make_unique<juce::Label>("In Port", "In Port");
    addAndMakeVisible(*inPortL);

    inPortReset = std::make_unique<Widgets::SurgeTextButton>("Reset to Default");
    inPortReset->addListener(this);
    inPortReset->setEnabled(false);
    addAndMakeVisible(*inPortReset);

    outPort = makeEd("Out Port");
    outPort->setJustification(juce::Justification::centred);
    outPort->addListener(this);
    addAndMakeVisible(*outPort);

    outPortL = std::make_unique<juce::Label>("Out Port", "Out Port");
    addAndMakeVisible(*outPortL);

    outPortReset = std::make_unique<Widgets::SurgeTextButton>("Reset to Default");
    outPortReset->addListener(this);
    outPortReset->setEnabled(false);
    addAndMakeVisible(*outPortReset);

    outIP = makeEd("Out IP");
    outIP->setJustification(juce::Justification::centred);
    outIP->addListener(this);
    addAndMakeVisible(*outIP);

    outIPL = std::make_unique<juce::Label>("Out IP", "Out IP");
    addAndMakeVisible(*outIPL);

    outIPReset = std::make_unique<Widgets::SurgeTextButton>("Reset to Default");
    outIPReset->addListener(this);
    outIPReset->setEnabled(false);
    addAndMakeVisible(*outIPReset);

    ok = std::make_unique<Widgets::SurgeTextButton>("OK");
    ok->addListener(this);
    ok->setEnabled(false);
    addAndMakeVisible(*ok);

    cancel = std::make_unique<Widgets::SurgeTextButton>("Cancel");
    cancel->addListener(this);
    addAndMakeVisible(*cancel);

    enableIn = std::make_unique<juce::ToggleButton>("Enable OSC In");
    enableIn->addListener(this);
    addAndMakeVisible(*enableIn);

    enableOut = std::make_unique<juce::ToggleButton>("Enable OSC Out");
    enableOut->addListener(this);
    addAndMakeVisible(*enableOut);

    /*
    showSpec = std::make_unique<Widgets::SurgeTextButton>("Show Open Sound Control Spec");
    showSpec->addListener(this);
    addAndMakeVisible(*showSpec);
    */

    OSCHeader = std::make_unique<juce::Label>("OSC Header", "Open Sound Control Settings");
    OSCHeader->setJustificationType(juce::Justification::centredTop);
    addAndMakeVisible(*OSCHeader);
}

OpenSoundControlSettings::~OpenSoundControlSettings() = default;

void OpenSoundControlSettings::setStorage(SurgeStorage *s)
{
    storage = s;
    defaultOSCInPort = Surge::Storage::getUserDefaultValue(storage, Surge::Storage::OSCPortIn,
                                                           DEFAULT_OSC_PORT_IN);
    defaultOSCOutPort = Surge::Storage::getUserDefaultValue(storage, Surge::Storage::OSCPortOut,
                                                            DEFAULT_OSC_PORT_OUT);
    defaultOSCOutIP = Surge::Storage::getUserDefaultValue(storage, Surge::Storage::OSCIPOut,
                                                          DEFAULT_OSC_IPADDR_OUT);
}

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
    // showSpec->setSkin(skin, associatedBitmapStore);
    ok->setSkin(skin, associatedBitmapStore);
    cancel->setSkin(skin, associatedBitmapStore);
}

void OpenSoundControlSettings::resized()
{
    // overall size set in SurgeGUIEditorOverlays.cpp when created
    auto uheight = 30;
    auto ushift = 28;
    auto buttonHeight = 26;
    auto buttonWidth = 70;

    auto col = getLocalBounds().withTrimmedTop(50).reduced(20, 0);
    col = col.withWidth(col.getWidth() / 3);

    OSCHeader->setBounds(getLocalBounds().withHeight(30).translated(0, 3));

    {
        auto lcol = col.withHeight(uheight);
        enableIn->setBounds(lcol.translated(30, 0));

        lcol = lcol.translated(0, ushift);
        inPortL->setBounds(lcol);

        lcol = lcol.translated(0, ushift);
        inPort->setBounds(lcol.reduced(36, 0));
        inPort->setIndents(4, 0);

        lcol = lcol.translated(0, ushift * 1.1);
        inPortReset->setBounds(lcol.reduced(40, 8));
    }

    col = col.translated(col.getWidth(), 0);
    {
        auto lcol = col.withHeight(uheight);
        enableOut->setBounds(lcol.reduced(30, 0));

        lcol = lcol.translated(0, ushift);
        outPortL->setBounds(lcol);

        lcol = lcol.translated(0, ushift);
        outPort->setBounds(lcol.reduced(36, 0));
        outPort->setIndents(4, 0);

        lcol = lcol.translated(0, ushift * 1.1);
        outPortReset->setBounds(lcol.reduced(40, 8));

        lcol = lcol.translated(0, ushift);
        ok->setBounds(lcol.withHeight(buttonHeight).translated(0, ushift).withWidth(buttonWidth));

        lcol = lcol.translated(buttonWidth * 1.25, 0);
        cancel->setBounds(
            lcol.withHeight(buttonHeight).translated(0, ushift).withWidth(buttonWidth));
    }

    col = col.translated(col.getWidth(), 0);
    {
        auto lcol = col.withHeight(uheight).reduced(5, 0);
        lcol = lcol.translated(0, ushift);
        outIPL->setBounds(lcol);

        lcol = lcol.translated(0, ushift);
        outIP->setBounds(lcol.reduced(20, 0));
        outIP->setIndents(4, 0);

        lcol = lcol.translated(0, ushift * 1.1);
        outIPReset->setBounds(lcol.reduced(36, 8));
    }
}

void OpenSoundControlSettings::setValuesFromEditor()
{
    if (!editor)
        return;

    enableIn->setToggleState(editor->synth->storage.oscReceiving, juce::dontSendNotification);
    enableOut->setToggleState(editor->synth->storage.oscSending, juce::dontSendNotification);

    inPort->setText(std::to_string(editor->synth->storage.oscPortIn), juce::dontSendNotification);
    outPort->setText(std::to_string(editor->synth->storage.oscPortOut), juce::dontSendNotification);
    outIP->setText(editor->synth->storage.oscOutIP, juce::dontSendNotification);
}

#define LOG_CALLBACK std::cout << "OpenSoundControlSettings.cpp:" << __LINE__ << " "

void OpenSoundControlSettings::textEditorEscapeKeyPressed(juce::TextEditor &ed) {}
void OpenSoundControlSettings::textEditorReturnKeyPressed(juce::TextEditor &ed) {}

void OpenSoundControlSettings::textEditorFocusLost(juce::TextEditor &ed)
{
    if (!editor)
        return;

    if (!storage)
        return;

    std::string newStr = ed.getText().toStdString();
    if (&ed == inPort.get())
    {
        if (newStr != std::to_string(editor->synth->storage.oscPortIn))
        {
            int newPort = validPort(newStr, "input");
            if (newPort > 0)
            {
                ok->setEnabled(true);
                inPortReset->setEnabled(newPort != defaultOSCInPort);
            }
            else
            {
                ed.setText(std::to_string(editor->synth->storage.oscPortIn),
                           juce::dontSendNotification);
            }
        }
    }

    if (&ed == outPort.get())
    {
        if (newStr != std::to_string(editor->synth->storage.oscPortOut))
        {
            int newPort = validPort(newStr, "output");
            if (newPort > 0)
            {
                ok->setEnabled(true);
                outPortReset->setEnabled(newPort != defaultOSCOutPort);
            }
            else
            {
                ed.setText(std::to_string(editor->synth->storage.oscPortOut),
                           juce::dontSendNotification);
            }
        }
    }

    if (&ed == outIP.get())
    {
        if (newStr != editor->synth->storage.oscOutIP)
        {
            if (validateIPString(newStr))
            {
                ok->setEnabled(true);
                {
                    outIPReset->setEnabled(newStr != defaultOSCOutIP);
                }
            }
            else
            {
                ed.setText(editor->synth->storage.oscOutIP, juce::dontSendNotification);
            }
        }
    }
}

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
        inPortReset->setEnabled(false);
        inPort->setText(std::to_string(defaultOSCInPort), juce::dontSendNotification);
    }
    if (button == outPortReset.get())
    {
        outPortReset->setEnabled(false);
        outPort->setText(std::to_string(defaultOSCOutPort), juce::dontSendNotification);
    }
    if (button == outIPReset.get())
    {
        outIPReset->setEnabled(false);
        outIP->setText(defaultOSCOutIP, juce::dontSendNotification);
    }
    if (button == ok.get())
    {
        // Check both "enable" controls and 3 text fields for any change
        bool inputChanged = false;
        bool outputChanged = false;
        bool curOSCReceiving = enableIn->getToggleState();
        bool curOSCSending = enableOut->getToggleState();
        std::string curInPort = inPort->getText().toStdString();
        std::string curOutPort = outPort->getText().toStdString();
        std::string curOutIP = outIP->getText().toStdString();
        SurgeSynthProcessor *ssp = &editor->juceEditor->processor;

        if ((curOSCReceiving != editor->synth->storage.oscReceiving) ||
            (curInPort != std::to_string(editor->synth->storage.oscPortIn)))
        {
            inputChanged = true;
        }
        if ((curOSCSending != editor->synth->storage.oscSending) ||
            (curOutPort != std::to_string(editor->synth->storage.oscPortOut)) ||
            (curOutIP != editor->synth->storage.oscOutIP))
        {
            outputChanged = true;
        }

        if (inputChanged)
        {
        }
        if (outputChanged)
        {
            int newPort = std::stoi(curOutPort);
            if (ssp->changeOSCOut(newPort, curOutIP))
            {
                storage->oscPortOut = newPort;
                storage->oscOutIP = curOutIP;
            }
            else
            {
                ssp->initOSCError(newPort);
            }
        }
    }

    if (button == cancel.get())
    {
        editor->closeOverlay(SurgeGUIEditor::OPEN_SOUND_CONTROL_SETTINGS);
    }
}

bool OpenSoundControlSettings::is_number(const std::string &s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}

// Returns new port integer value if portStr is valid, otherwise returns 0
int OpenSoundControlSettings::validPort(std::string portStr, std::string type)
{
    int newPort = 0;
    if (!is_number(portStr))
    {
        std::ostringstream msg;
        msg << "Value for " << type << " port number must be a valid number between 1 and 65535!";
        storage->reportError(msg.str(), "Port Number Error");
        return 0;
    }
    newPort = std::stoi(portStr);

    if (newPort > 65535 || newPort < 0)
    {
        std::ostringstream msg;
        msg << "Value for " << type << " port number must be between 1 and 65535!";
        storage->reportError(msg.str(), "Port Number Error");
        return 0;
    }
    return newPort;
}

bool OpenSoundControlSettings::validateIPString(std::string ipStr)
{
    // regular expression pattern for an IPv4 address
    std::regex ipPattern("^((25[0-5]|(2[0-4]|1[0-9]|[1-9]|)[0-9])(\\.(?!$)|$)){4}$");
    // Use regex_match to check if the input string matches the pattern
    if (!std::regex_match(ipStr, ipPattern))
    {
        storage->reportError("Please enter a valid IPv4 address, which consists of "
                             "four numbers between 0 and 255,\n"
                             "separated by periods.",
                             "IP Address Error");
        return false;
    }
    return true;
}

} // namespace Overlays
} // namespace Surge