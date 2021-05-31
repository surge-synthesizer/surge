/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "SurgeSynthEditor.h"
#include "SurgeSynthProcessor.h"
#include "SurgeBitmaps.h"
#include "CScalableBitmap.h"
#include "SurgeGUIEditor.h"
#include "SurgeSynthFlavorExtensions.h"
#include "SurgeJUCELookAndFeel.h"
#include "RuntimeFont.h"

//==============================================================================
SurgeSynthEditor::SurgeSynthEditor(SurgeSynthProcessor &p)
    : AudioProcessorEditor(&p), processor(p), EscapeFromVSTGUI::JuceVSTGUIEditorAdapter(this)
{
    surgeLF = std::make_unique<SurgeJUCELookAndFeel>();
    juce::LookAndFeel::setDefaultLookAndFeel(surgeLF.get());
    adapter = std::make_unique<SurgeGUIEditor>(this, processor.surge.get());

    int yExtra = 0;
    keyboard = std::make_unique<juce::MidiKeyboardComponent>(
        processor.midiKeyboardState, juce::MidiKeyboardComponent::Orientation::horizontalKeyboard);
    auto mcValue = Surge::Storage::getUserDefaultValue(&(this->processor.surge->storage),
                                                       Surge::Storage::MiddleC, 1);

    keyboard->setOctaveForMiddleC(3 + mcValue);
    keyboard->setLowestVisibleKey(60 - 30);
    tempoLabel = std::make_unique<juce::Label>("Tempo", "Tempo");
    tempoTypein = std::make_unique<juce::TextEditor>("Tempo");
    tempoTypein->setText(std::to_string((int)(processor.surge->storage.temposyncratio * 120)));
    tempoTypein->onReturnKey = [this]() {
        // this is thread sloppy
        float newT = std::atof(tempoTypein->getText().toRawUTF8());
        processor.standaloneTempo = newT;
    };
    tempoLabel->setFont(Surge::GUI::getFontManager()->getLatoAtSize(12));

    addChildComponent(*keyboard);
    addChildComponent(*tempoLabel);
    addChildComponent(*tempoTypein);
    drawExtendedControls = adapter->getShowVirtualKeyboard();
    bool addTempo = processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
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
        adapter->bitmapStore->clearAllLoadedBitmaps();
    adapter.reset(nullptr);
#if DEBUG_EFVG_MEMORY
    EscapeNS::Internal::showMemoryOutstanding();
#endif
}

void SurgeSynthEditor::handleAsyncUpdate() {}

void SurgeSynthEditor::paint(juce::Graphics &g) {}

void SurgeSynthEditor::idle() { adapter->idle(); }

void SurgeSynthEditor::resized()
{
    drawExtendedControls = adapter->getShowVirtualKeyboard();
    bool addTempo = processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
    if (drawExtendedControls)
    {
        auto y = getHeight() - extraYSpaceForVirtualKeyboard;
        auto x = addTempo ? 90 : 0;
        keyboard->setBounds(x, y, getWidth() - x, extraYSpaceForVirtualKeyboard);
        keyboard->setVisible(true);
        tempoLabel->setVisible(addTempo);
        tempoTypein->setVisible(addTempo);
        if (addTempo)
        {
            tempoLabel->setBounds(3, y + 3, x - 6, extraYSpaceForVirtualKeyboard / 2 - 3);
            tempoTypein->setBounds(3, y + 3 + extraYSpaceForVirtualKeyboard / 2, x - 6,
                                   extraYSpaceForVirtualKeyboard / 2 - 6);
        }
    }
    else
    {
        keyboard->setVisible(false);
        tempoLabel->setVisible(false);
        tempoTypein->setVisible(false);
    }
}

void SurgeSynthEditor::IdleTimer::timerCallback()
{
    VSTGUI::efvgIdle();
    ed->idle();
}

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

namespace Surge
{
namespace GUI
{

/*
** Return the backing scale factor. This is the scale factor which maps a phyiscal
** pixel to a logical pixel. It is in units that a high density display (so 4 physical
** pixels in a single pixel square) would have a backing scale factor of 2.0.
**
** We retain this value as a float and do not scale it by 100, like we do with
** user specified scales, to better match the OS API
*/
float getDisplayBackingScaleFactor(VSTGUI::CFrame *) { return 2; }

/*
** Return the screen dimensions of the best screen containing this frame. If the
** frame is not valid or has not yet been shown or so on, return a screen of
** size 0x0 at position 0,0.
*/
VSTGUI::CRect getScreenDimensions(VSTGUI::CFrame *)
{
    return VSTGUI::CRect(VSTGUI::CPoint(0, 0), VSTGUI::CPoint(1400, 700));
}
} // namespace GUI

} // namespace Surge
