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

//==============================================================================
SurgeSynthEditor::SurgeSynthEditor(SurgeSynthProcessor &p)
    : AudioProcessorEditor(&p), processor(p), EscapeFromVSTGUI::JuceVSTGUIEditorAdapter(this)
{
    setSize(1200, 800);
    setResizable(false, false); // For now

    adapter = std::make_unique<SurgeGUIEditor>(this, processor.surge.get());
    adapter->open(nullptr);

    idleTimer = std::make_unique<IdleTimer>(this);
    idleTimer->startTimer(1000 / 60);
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
