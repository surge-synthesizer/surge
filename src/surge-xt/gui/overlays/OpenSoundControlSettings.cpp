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
#include "widgets/MenuCustomComponents.h"
#include "AccessibleHelpers.h"
#include "SurgeSharedBinary.h"
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

    inPort = makeEd("OSC Input Port");
    inPort->setJustification(juce::Justification::centred);
    inPort->addListener(this);
    inPort->setInputRestrictions(5, "0123456789");
    addAndMakeVisible(*inPort);

    inPortReset = std::make_unique<Widgets::SurgeTextButton>("Default Port");
    inPortReset->addListener(this);
    inPortReset->setTitle("Default OSC Input Port");
    addAndMakeVisible(*inPortReset);

    outPort = makeEd("OSC Output Port");
    outPort->setJustification(juce::Justification::centred);
    outPort->addListener(this);
    outPort->setInputRestrictions(5, "0123456789");
    addAndMakeVisible(*outPort);

    outPortReset = std::make_unique<Widgets::SurgeTextButton>("Default Port");
    outPortReset->addListener(this);
    outPortReset->setTitle("Default OSC Output Port");
    addAndMakeVisible(*outPortReset);

    outIP = makeEd("Out IP Address");
    outIP->setJustification(juce::Justification::centred);
    outIP->addListener(this);
    outIP->setInputRestrictions(15, "0123456789.");
    addAndMakeVisible(*outIP);

    inL = std::make_unique<juce::Label>("OSC In", "OSC In");
    addAndMakeVisible(*inL);

    outL = std::make_unique<juce::Label>("OSC Out", "OSC Out");
    addAndMakeVisible(*outL);

    outIPL = std::make_unique<juce::Label>("Out IP Address", "Out IP Address");
    addAndMakeVisible(*outIPL);

    outIPReset = std::make_unique<Widgets::SurgeTextButton>("Local Host");
    outIPReset->addListener(this);
    outIPReset->setTitle("Reset OSC Output IP Address to Local Host");
    addAndMakeVisible(*outIPReset);

    help = std::make_unique<Widgets::SurgeTextButton>("?");
    help->addListener(this);
    addAndMakeVisible(*help);

    apply = std::make_unique<Widgets::SurgeTextButton>("Apply");
    apply->addListener(this);
    apply->setEnabled(false);
    addAndMakeVisible(*apply);

    ok = std::make_unique<Widgets::SurgeTextButton>("OK");
    ok->addListener(this);
    ok->setEnabled(false);
    addAndMakeVisible(*ok);

    cancel = std::make_unique<Widgets::SurgeTextButton>("Cancel");
    cancel->addListener(this);
    addAndMakeVisible(*cancel);

    enableIn = std::make_unique<juce::ToggleButton>("");
    enableIn->addListener(this);
    enableIn->setTitle("OSC Input Enable");
    addAndMakeVisible(*enableIn);

    enableOut = std::make_unique<juce::ToggleButton>("");
    enableOut->addListener(this);
    enableOut->setTitle("OSC Output Enable");
    addAndMakeVisible(*enableOut);
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
        typein->setFont(skin->getFont(Fonts::System::Display));
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
        typein->applyFontToAllText(skin->getFont(Fonts::System::Display));
    };

    auto resetLabel = [this](const auto &label) {
        label->setFont(skin->getFont(Fonts::PatchStore::Label));
        label->setColour(juce::Label::textColourId, skin->getColor(Colors::Dialog::Label::Text));
    };

    resetColors(inPort);
    resetColors(outPort);
    resetColors(outIP);

    resetLabel(inL);
    resetLabel(outL);
    resetLabel(outIPL);

    outIPL->setJustificationType(juce::Justification::centred);

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
    help->setSkin(skin, associatedBitmapStore);
    apply->setSkin(skin, associatedBitmapStore);
    ok->setSkin(skin, associatedBitmapStore);
    cancel->setSkin(skin, associatedBitmapStore);
}

void OpenSoundControlSettings::resized()
{
    // overall size set in SurgeGUIEditorOverlays.cpp when created
    static constexpr int uheight = 18;
    static constexpr int ushift = 30;
    static constexpr int margin = 8;
    static constexpr int halfmargin = margin / 2;
    static constexpr int buttonHeight = 17;
    static constexpr int buttonWidth = 50;
    static constexpr int defButtonWidth = 64;
    static constexpr int checkboxWidth = 68;

    auto col = getLocalBounds();

    // left
    col = col.withWidth(col.getWidth() / 3);

    {
        auto lcol = col.withHeight(uheight);
        auto inb = lcol.translated(0, margin).withSizeKeepingCentre(checkboxWidth, buttonHeight);

        enableIn->setBounds(inb);

        inL->setBounds(inb.translated(20, 0));

        lcol = lcol.translated(0, ushift + halfmargin);
        inPort->setBounds(lcol.withSizeKeepingCentre(defButtonWidth, buttonHeight));
        inPort->setIndents(4, 0);

        lcol = lcol.translated(0, ushift - halfmargin);
        inPortReset->setBounds(lcol.withSizeKeepingCentre(defButtonWidth, buttonHeight));
    }

    // center
    col = col.translated(col.getWidth(), 0);

    {
        auto lcol = col.withHeight(uheight);
        auto outb = lcol.translated(0, margin).withSizeKeepingCentre(checkboxWidth, buttonHeight);

        enableOut->setBounds(outb);

        outL->setBounds(outb.translated(20, 0));

        lcol = lcol.translated(0, ushift + halfmargin);
        outPort->setBounds(lcol.withSizeKeepingCentre(defButtonWidth, buttonHeight));
        outPort->setIndents(4, 0);

        lcol = lcol.translated(0, ushift - halfmargin);
        outPortReset->setBounds(lcol.withSizeKeepingCentre(defButtonWidth, buttonHeight));
    }

    // right
    col = col.translated(col.getWidth(), 0);

    {
        auto lcol = col.withHeight(uheight);

        outIPL->setBounds(lcol.translated(0, margin));

        lcol = lcol.translated(0, ushift + halfmargin);
        outIP->setBounds(lcol.reduced(margin + halfmargin, 0));
        outIP->setIndents(4, 0);

        lcol = lcol.translated(0, ushift - halfmargin);
        outIPReset->setBounds(lcol.reduced(margin + halfmargin, 0));
    }

    // bottom row
    auto row = getLocalBounds();
    const auto yPos = getLocalBounds().getHeight() - buttonHeight - margin;

    help->setBounds(row.translated(outIPReset->getRight() - buttonHeight, yPos)
                        .withSize(buttonHeight, buttonHeight));

    row = row.translated(row.getWidth() / 2 - buttonWidth / 2, yPos);

    row.setSize(buttonWidth, buttonHeight);

    ok->setBounds(row);
    apply->setBounds(row.translated(-buttonWidth - margin, 0));
    cancel->setBounds(row.translated(buttonWidth + margin, 0));
}

void OpenSoundControlSettings::setValuesFromEditor()
{
    if (!editor)
    {
        return;
    }

    enableIn->setToggleState(editor->synth->storage.oscReceiving, juce::dontSendNotification);
    enableOut->setToggleState(editor->synth->storage.oscSending, juce::dontSendNotification);

    inPort->setText(std::to_string(editor->synth->storage.oscPortIn), juce::dontSendNotification);
    inPortReset->setEnabled(editor->synth->storage.oscPortIn != defaultOSCInPort);

    outPort->setText(std::to_string(editor->synth->storage.oscPortOut), juce::dontSendNotification);
    outIP->setText(editor->synth->storage.oscOutIP, juce::dontSendNotification);
    outPortReset->setEnabled(editor->synth->storage.oscPortOut != defaultOSCOutPort);
    outIPReset->setEnabled(editor->synth->storage.oscOutIP != defaultOSCOutIP);
}

void OpenSoundControlSettings::textEditorTextChanged(juce::TextEditor &ed) { setAllEnableds(); }

void OpenSoundControlSettings::textEditorEscapeKeyPressed(juce::TextEditor &ed) {}

void OpenSoundControlSettings::textEditorReturnKeyPressed(juce::TextEditor &ed)
{
    if (!editor || !storage)
    {
        return;
    }

    validateInputs(ed);
    updateAll();
    setAllEnableds();
}

void OpenSoundControlSettings::textEditorFocusLost(juce::TextEditor &ed)
{
    if (!editor || !storage)
    {
        return;
    }

    validateInputs(ed);
    ed.setHighlightedRegion(juce::Range(-1, -1));
}

void OpenSoundControlSettings::buttonClicked(juce::Button *button)
{
    if (!editor || !storage)
    {
        return;
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

    if (button == help.get())
    {
        auto menu = juce::PopupMenu();
        auto hu = editor->helpURLForSpecial("opensound-settings");
        auto lurl = hu;

        if (lurl != "")
        {
            lurl = editor->fullyResolvedHelpURL(hu);
        }

        auto tcomp = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>("OSC Settings", lurl);
        tcomp->setSkin(skin, associatedBitmapStore);
        auto hment = tcomp->getTitle();

        menu.addCustomItem(-1, std::move(tcomp), nullptr, hment);

        menu.addSeparator();

        menu.addItem(Surge::GUI::toOSCase("Show OSC Specification..."), [this]() {
            auto oscSpec = std::string(SurgeSharedBinary::oscspecification_html,
                                       SurgeSharedBinary::oscspecification_htmlSize) +
                           "\n";
            editor->showHTML(oscSpec);
        });

        menu.showMenuAsync(editor->popupMenuOptions());
    }

    if (button == apply.get())
    {
        updateAll();
    }

    if (button == ok.get())
    {
        if (updateAll())
        {
            editor->closeOverlay(SurgeGUIEditor::OPEN_SOUND_CONTROL_SETTINGS);
        }
    }

    if (button == cancel.get())
    {
        editor->closeOverlay(SurgeGUIEditor::OPEN_SOUND_CONTROL_SETTINGS);
    }

    setAllEnableds();
}

void OpenSoundControlSettings::setAllEnableds()
{
    // OK stays enabled after first enabling
    ok->setEnabled(ok->isEnabled() || isInputChanged() || isOutputChanged());
    apply->setEnabled(isInputChanged() || isOutputChanged());
    inPort->setEnabled(enableIn->getToggleState());
    outPort->setEnabled(enableOut->getToggleState());
    outIP->setEnabled(enableOut->getToggleState());
    inPortReset->setEnabled(editor->synth->storage.oscPortIn != defaultOSCInPort);
    outPortReset->setEnabled(editor->synth->storage.oscPortOut != defaultOSCOutPort);
    outIPReset->setEnabled(editor->synth->storage.oscOutIP != defaultOSCOutIP);

    auto color = skin->getColor(Colors::Dialog::Label::Text);

    inPort->applyColourToAllText(color.withAlpha(0.5f + (enableIn->getToggleState() * 0.5f)));
    outPort->applyColourToAllText(color.withAlpha(0.5f + (enableOut->getToggleState() * 0.5f)));
    outIP->applyColourToAllText(color.withAlpha(0.5f + (enableOut->getToggleState() * 0.5f)));
}

bool OpenSoundControlSettings::updateAll()
{
    SurgeSynthProcessor *ssp = &editor->juceEditor->processor;
    bool allgood = true;

    if (isInputChanged())
    {
        int newPort = std::stoi(inPort->getText().toStdString());

        if (enableIn->getToggleState())
        {
            // This call starts OSC in, if necessary:
            if (ssp->changeOSCInPort(newPort))
            {
                storage->oscPortIn = newPort;
                storage->oscReceiving = true;
            }
            else
            {
                ssp->initOSCError(newPort);
                allgood = false;
            }
        }
        else
        {
            ssp->oscHandler.stopListening();
            storage->oscPortIn = newPort;
            storage->oscReceiving = false;
        }
    }

    if (isOutputChanged())
    {
        int newPort = std::stoi(outPort->getText().toStdString());

        if (enableOut->getToggleState())
        {
            if (ssp->changeOSCOut(newPort, outIP->getText().toStdString()))
            {
                storage->oscPortOut = newPort;
                storage->oscOutIP = outIP->getText().toStdString();
                storage->oscSending = true;
            }
            else
            {
                ssp->initOSCError(newPort);
                allgood = false;
            }
        }
        else
        {
            ssp->stopOSCOut();
            storage->oscPortOut = newPort;
            storage->oscSending = false;
        }
    }

    return allgood;
}

bool OpenSoundControlSettings::isInputChanged()
{
    return ((enableIn->getToggleState() != editor->synth->storage.oscReceiving) ||
            (inPort->getText().toStdString() != std::to_string(editor->synth->storage.oscPortIn)));
}

bool OpenSoundControlSettings::isOutputChanged()
{
    return (
        (enableOut->getToggleState() != editor->synth->storage.oscSending) ||
        (outPort->getText().toStdString() != std::to_string(editor->synth->storage.oscPortOut)) ||
        (outIP->getText().toStdString() != editor->synth->storage.oscOutIP));
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
        msg << type << " port number must be between 1 and 65535!";
        storage->reportError(msg.str(), "Port Number Error");
        return 0;
    }

    newPort = std::stoi(portStr);

    if (newPort > 65535 || newPort < 1)
    {
        std::ostringstream msg;
        msg << type << " port number must be between 1 and 65535!";
        storage->reportError(msg.str(), "Port Number Error");
        return 0;
    }

    return newPort;
}

void OpenSoundControlSettings::validateInputs(juce::TextEditor &ed)
{
    std::string newStr = ed.getText().toStdString();

    if (&ed == inPort.get())
    {
        if (newStr != std::to_string(editor->synth->storage.oscPortIn))
        {
            int newPort = validPort(newStr, "Input");

            if (newPort < 1 || newPort == editor->synth->storage.oscPortOut)
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
            int newPort = validPort(newStr, "Output");

            if (newPort < 1 || newPort == editor->synth->storage.oscPortIn)
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
            if (!validateIPString(newStr))
            {
                ed.setText(editor->synth->storage.oscOutIP, juce::dontSendNotification);
            }
        }
    }

    setAllEnableds();
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