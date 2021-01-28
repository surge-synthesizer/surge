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

#if NONSENSE_TEST
struct ARedBox : public VSTGUI::CView
{
    std::unique_ptr<SurgeBitmaps> bitmapStore;

    explicit ARedBox(const VSTGUI::CRect &r) : VSTGUI::CView(r)
    {
        bitmapStore = std::make_unique<SurgeBitmaps>();
        bitmapStore->setupBitmapsForFrame(nullptr);
    }
    ~ARedBox() = default;
    VSTGUI::CColor bg = VSTGUI::CColor(0, 0, 0);
    void draw(VSTGUI::CDrawContext *dc) override
    {
        auto s = getViewSize();

        dc->setFillColor(bg);
        dc->drawRect(s, VSTGUI::kDrawFilled);
        dc->setFrameColor(VSTGUI::kBlueCColor);
        dc->drawRect(s, VSTGUI::kDrawStroked);

        auto r = VSTGUI::CRect(VSTGUI::CPoint(20, 20), VSTGUI::CPoint(10, 15));

        dc->setFillColor(VSTGUI::kRedCColor);
        dc->setFrameColor(VSTGUI::CColor(0, 255, 0));
        dc->drawRect(r, VSTGUI::kDrawFilledAndStroked);

        dc->setFrameColor(VSTGUI::CColor(255, 180, 0));
        dc->drawLine(VSTGUI::CPoint(0, 0), VSTGUI::CPoint(40, 40));

        auto bmp = bitmapStore->getBitmap(IDB_FILTER_CONFIG);
        auto newLoc = mouseLoc;
        newLoc.x -= 15;
        newLoc.y -= 15;
        bmp->draw(dc, VSTGUI::CRect(newLoc, VSTGUI::CPoint(40, 40)), VSTGUI::CPoint(), 1);
    }

    VSTGUI::CMouseEventResult onMouseEntered(VSTGUI::CPoint &where,
                                             const VSTGUI::CButtonState &buttons) override
    {
        bg = VSTGUI::CColor(40, 20, 0);
        invalid();
        return VSTGUI::kMouseEventHandled;
    }
    VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint &where,
                                            const VSTGUI::CButtonState &buttons) override
    {
        bg = VSTGUI::CColor(0, 0, 0);
        invalid();
        return VSTGUI::kMouseEventHandled;
    }
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint &where,
                                          const VSTGUI::CButtonState &buttons) override
    {
        bg = VSTGUI::CColor(200, 100, 0);
        invalid();
        return VSTGUI::kMouseEventHandled;
    }
    VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint &where,
                                        const VSTGUI::CButtonState &buttons) override
    {
        bg = VSTGUI::CColor(70, 20, 0);
        invalid();
        return VSTGUI::kMouseEventHandled;
    }
    VSTGUI::CPoint mouseLoc;
    VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint &where,
                                           const VSTGUI::CButtonState &buttons) override
    {
        mouseLoc = where;
        invalid();
        return VSTGUI::kMouseEventHandled;
    }
};
#endif

//==============================================================================
SurgeSynthEditor::SurgeSynthEditor(SurgeSynthProcessor &p)
    : AudioProcessorEditor(&p), processor(p), EscapeFromVSTGUI::JuceVSTGUIEditorAdapter(this)
{
    setSize(1200, 800);
    setResizable(false, false); // For now

    adapter = std::make_unique<SurgeGUIEditor>(this, processor.surge.get());
    adapter->open(nullptr);

#if NONSENSE_TEST
    auto r = VSTGUI::CRect(getBounds());
    vstguiFrame = std::make_unique<VSTGUI::CFrame>(r, adapter.get());

    auto textRect = VSTGUI::CRect(VSTGUI::CPoint(10, 10), VSTGUI::CPoint(200, 10));
    auto text = new VSTGUI::CTextLabel(textRect, "Howdy");
    vstguiFrame->addView(text);

    auto myThing = new ARedBox(VSTGUI::CRect(40, 40, 700, 250));
    vstguiFrame->addView(myThing);

    int bds = 0;
    auto fn = BinaryData::getNamedResourceOriginalFilename("bmp00158_svg");
    auto bd = BinaryData::getNamedResource("bmp00158_svg", bds);
    logo = juce::Drawable::createFromImageData(bd, bds);
    // logo = juce::Drawable::createFromImageFile(juce::File("/Users/paul/Downloads/PREVIEW.svg"));
    logo->setBounds(40, 300, 500, 500);
    addAndMakeVisible(logo.get());

    auto b = new SurgeBitmaps();
#endif

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
