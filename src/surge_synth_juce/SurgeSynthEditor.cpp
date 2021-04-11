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

//==============================================================================
SurgeSynthEditor::SurgeSynthEditor(SurgeSynthProcessor &p)
    : AudioProcessorEditor(&p), processor(p), EscapeFromVSTGUI::JuceVSTGUIEditorAdapter(this)
{
    setSize(BASE_WINDOW_SIZE_X, BASE_WINDOW_SIZE_Y);
    setResizable(true, false); // For now

    adapter = std::make_unique<SurgeGUIEditor>(this, processor.surge.get());
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
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
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

namespace UserInteractions
{
/*
 * This means I have a link wrong
 */
void openFolderInFileBrowser(const std::string &folder) {}

} // namespace UserInteractions
} // namespace Surge
