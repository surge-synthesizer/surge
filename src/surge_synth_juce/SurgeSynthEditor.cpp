/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "SurgeSynthEditor.h"
#include "SurgeSynthProcessor.h"
#include "SurgeImageStore.h"
#include "SurgeImage.h"
#include "SurgeGUIEditor.h"
#include "SurgeSynthFlavorExtensions.h"
#include "SurgeJUCELookAndFeel.h"
#include "RuntimeFont.h"
#include <version.h>

//==============================================================================
SurgeSynthEditor::SurgeSynthEditor(SurgeSynthProcessor &p) : AudioProcessorEditor(&p), processor(p)
{
    surgeLF = std::make_unique<SurgeJUCELookAndFeel>();

    juce::LookAndFeel::setDefaultLookAndFeel(surgeLF.get());

    addKeyListener(this);

    adapter = std::make_unique<SurgeGUIEditor>(this, processor.surge.get());

    keyboard = std::make_unique<juce::MidiKeyboardComponent>(
        processor.midiKeyboardState, juce::MidiKeyboardComponent::Orientation::horizontalKeyboard);

    auto mcValue = Surge::Storage::getUserDefaultValue(&(this->processor.surge->storage),
                                                       Surge::Storage::MiddleC, 1);

    keyboard->setOctaveForMiddleC(5 - mcValue);
    keyboard->setKeyPressBaseOctave(5);
    keyboard->setLowestVisibleKey(24);

    tempoTypein = std::make_unique<juce::TextEditor>("Tempo");
    tempoTypein->setFont(Surge::GUI::getFontManager()->getLatoAtSize(10));
    tempoTypein->setJustification(juce::Justification::centred);
    tempoTypein->setInputRestrictions(3, "0123456789");
    tempoTypein->setSelectAllWhenFocused(true);
    tempoTypein->onReturnKey = [this]() {
        // this is thread sloppy
        float newT = std::atof(tempoTypein->getText().toRawUTF8());

        processor.standaloneTempo = newT;
        tempoTypein->giveAwayKeyboardFocus();
    };

    tempoLabel = std::make_unique<juce::Label>("Tempo", "Tempo");

    addChildComponent(*keyboard);
    addChildComponent(*tempoLabel);
    addChildComponent(*tempoTypein);

    drawExtendedControls = adapter->getShowVirtualKeyboard();

    bool addTempo = processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
    int yExtra = 0;

    if (drawExtendedControls)
    {
        keyboard->setVisible(true);
        tempoLabel->setVisible(addTempo);
        tempoTypein->setVisible(addTempo);

        yExtra = extraYSpaceForVirtualKeyboard;
    }
    else
    {
        keyboard->setVisible(false);
        tempoLabel->setVisible(false);
        tempoTypein->setVisible(false);

        yExtra = 0;
    }

    setSize(BASE_WINDOW_SIZE_X, BASE_WINDOW_SIZE_Y + yExtra);
    setResizable(true, false); // For now

    adapter->open(nullptr);

    idleTimer = std::make_unique<IdleTimer>(this);
    idleTimer->startTimer(1000 / 60);

    SurgeSynthEditorSpecificExtensions(this, adapter.get());
}

SurgeSynthEditor::~SurgeSynthEditor()
{
    idleTimer->stopTimer();
    adapter->close();

    if (adapter->bitmapStore)
    {
        adapter->bitmapStore->clearAllLoadedBitmaps();
    }

    adapter.reset(nullptr);
}

void SurgeSynthEditor::handleAsyncUpdate() {}

void SurgeSynthEditor::paint(juce::Graphics &g)
{
    g.fillAll(findColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoBackgroundId));

    keyboard->setColour(juce::MidiKeyboardComponent::textLabelColourId,
                        findColour(SurgeJUCELookAndFeel::SurgeColourIds::vkbTextLabelId));
    keyboard->setColour(juce::MidiKeyboardComponent::shadowColourId,
                        findColour(SurgeJUCELookAndFeel::SurgeColourIds::vkbShadowId));
    keyboard->setColour(juce::MidiKeyboardComponent::blackNoteColourId,
                        findColour(SurgeJUCELookAndFeel::SurgeColourIds::vkbBlackKeyId));
    keyboard->setColour(juce::MidiKeyboardComponent::whiteNoteColourId,
                        findColour(SurgeJUCELookAndFeel::SurgeColourIds::vkbWhiteKeyId));
    keyboard->setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,
                        findColour(SurgeJUCELookAndFeel::SurgeColourIds::vkbKeySeparatorId));
    keyboard->setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
                        findColour(SurgeJUCELookAndFeel::SurgeColourIds::vkbMouseOverKeyOverlayId));
    keyboard->setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
                        findColour(SurgeJUCELookAndFeel::SurgeColourIds::vkbKeyDownOverlayId));
    keyboard->setColour(juce::MidiKeyboardComponent::upDownButtonBackgroundColourId,
                        findColour(SurgeJUCELookAndFeel::SurgeColourIds::vkbOctaveJogBackgroundId));
    keyboard->setColour(juce::MidiKeyboardComponent::upDownButtonArrowColourId,
                        findColour(SurgeJUCELookAndFeel::SurgeColourIds::vkbOctaveJogArrowId));

    tempoLabel->setColour(juce::Label::textColourId,
                          findColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoLabelId));

    tempoTypein->setColour(
        juce::TextEditor::backgroundColourId,
        findColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoTypeinBackgroundId));
    tempoTypein->setColour(juce::TextEditor::outlineColourId,
                           findColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoTypeinBorderId));
    tempoTypein->setColour(
        juce::TextEditor::highlightColourId,
        findColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoTypeinHighlightId));
    tempoTypein->setColour(juce::TextEditor::highlightedTextColourId,
                           findColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoTypeinTextId));
    tempoTypein->setColour(juce::TextEditor::textColourId,
                           findColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoTypeinTextId));
}

void SurgeSynthEditor::idle() { adapter->idle(); }

void SurgeSynthEditor::resized()
{
    drawExtendedControls = adapter->getShowVirtualKeyboard();

    auto w = getWidth();
    auto h = getHeight() - (drawExtendedControls ? extraYSpaceForVirtualKeyboard : 0);
    auto wR = 1.0 * w / adapter->getWindowSizeX();
    auto hR = 1.0 * h / adapter->getWindowSizeY();
    auto zfn = std::min(wR, hR);
    bool addTempo = processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone;

    if (drawExtendedControls)
    {
        auto y = getHeight() - extraYSpaceForVirtualKeyboard;
        auto x = addTempo ? 50 : 0;

        keyboard->setBounds(x, y, getWidth() - x, extraYSpaceForVirtualKeyboard);
        keyboard->setVisible(true);

        if (addTempo)
        {
            tempoLabel->setBounds(4, y + 4, x - 8, extraYSpaceForVirtualKeyboard / 2 - 8);
            tempoLabel->setFont(Surge::GUI::getFontManager()->getLatoAtSize(10));
            tempoLabel->setJustificationType(juce::Justification::centred);
            tempoLabel->setVisible(addTempo);

            tempoTypein->setBounds(4, y + 4 + extraYSpaceForVirtualKeyboard / 2, x - 8,
                                   extraYSpaceForVirtualKeyboard / 2 - 8);
            tempoTypein->setText(
                std::to_string((int)(processor.surge->storage.temposyncratio * 120)));
            tempoTypein->setFont(Surge::GUI::getFontManager()->getLatoAtSize(10));
            tempoTypein->setJustification(juce::Justification::centred);
            tempoTypein->setVisible(addTempo);
            tempoTypein->setColour(
                juce::TextEditor::highlightedTextColourId,
                findColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoTypeinTextId));
            tempoTypein->setColour(
                juce::TextEditor::textColourId,
                findColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoTypeinTextId));
        }
    }
    else
    {
        keyboard->setVisible(false);
        tempoLabel->setVisible(false);
        tempoTypein->setVisible(false);
    }

    if (zfn != 1.0)
    {
        adapter->setZoomFactor(adapter->getZoomFactor() * zfn, false);
    }
}

void SurgeSynthEditor::parentHierarchyChanged()
{
    for (auto *p = getParentComponent(); p != nullptr; p = p->getParentComponent())
    {
        if (auto dw = dynamic_cast<juce::DocumentWindow *>(p))
        {
            std::ostringstream oss;
            oss << "Surge XT - " << Surge::Build::FullVersionStr;
            dw->setName(oss.str());
        }
    }
}

void SurgeSynthEditor::IdleTimer::timerCallback() { ed->idle(); }

void SurgeSynthEditor::populateForStreaming(SurgeSynthesizer *s)
{
    if (adapter)
        adapter->populateDawExtraState(s);
}

void SurgeSynthEditor::populateFromStreaming(SurgeSynthesizer *s)
{
    if (adapter)
        adapter->loadFromDAWExtraState(s);
}

bool SurgeSynthEditor::isInterestedInFileDrag(const juce::StringArray &files)
{
    if (files.size() != 1)
        return false;

    for (auto i = files.begin(); i != files.end(); ++i)
    {
        std::cout << *i << std::endl;
        if (adapter->canDropTarget(i->toStdString()))
            return true;
    }
    return false;
}

void SurgeSynthEditor::filesDropped(const juce::StringArray &files, int x, int y)
{
    if (files.size() != 1)
        return;

    for (auto i = files.begin(); i != files.end(); ++i)
    {
        if (adapter->canDropTarget(i->toStdString()))
            adapter->onDrop(i->toStdString());
    }
}

void SurgeSynthEditor::beginParameterEdit(Parameter *p)
{
    auto par = processor.paramsByID[processor.surge->idForParameter(p)];
    par->beginChangeGesture();
}

void SurgeSynthEditor::endParameterEdit(Parameter *p)
{
    auto par = processor.paramsByID[processor.surge->idForParameter(p)];
    par->endChangeGesture();
}

void SurgeSynthEditor::beginMacroEdit(long macroNum)
{
    auto par = processor.macrosById[macroNum];
    par->beginChangeGesture();
}

void SurgeSynthEditor::endMacroEdit(long macroNum)
{
    auto par = processor.macrosById[macroNum];
    par->endChangeGesture();
}

juce::PopupMenu SurgeSynthEditor::hostMenuFor(Parameter *p)
{
    auto par = processor.paramsByID[processor.surge->idForParameter(p)];
#if SURGE_JUCE_HOST_CONTEXT
    if (auto *c = getHostContext())
        if (auto menuInfo = c->getContextMenuForParameterIndex(par))
            return menuInfo->getEquivalentPopupMenu();
#endif

    return juce::PopupMenu();
}

juce::PopupMenu SurgeSynthEditor::hostMenuForMacro(int macro)
{
    auto par = processor.macrosById[macro];
#if SURGE_JUCE_HOST_CONTEXT
    if (auto *c = getHostContext())
        if (auto menuInfo = c->getContextMenuForParameterIndex(par))
            return menuInfo->getEquivalentPopupMenu();
#endif

    return juce::PopupMenu();
}

bool SurgeSynthEditor::keyPressed(const juce::KeyPress &key, juce::Component *orig)
{
    if (adapter->getShowVirtualKeyboard() && orig != keyboard.get())
    {
        return keyboard->keyPressed(key);
    }

    return false;
}

bool SurgeSynthEditor::keyStateChanged(bool isKeyDown, juce::Component *originatingComponent)
{
    if (adapter->getShowVirtualKeyboard() && originatingComponent != keyboard.get())
    {
        return keyboard->keyStateChanged(isKeyDown);
    }

    return false;
}