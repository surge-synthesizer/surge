/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#pragma once
#include "vstcontrols.h"
#include "SurgeBitmaps.h"
#include "SkinSupport.h"

class CEffectSettings : public VSTGUI::CControl, public Surge::UI::SkinConsumingComponent
{
    enum MouseActionMode
    {
        none,
        click,
        drag
    } mouseActionMode = none;

    bool hovered = false;
    int current, currentHover = -1, dragSource = -1;
    int bypass, disabled, type[8];

    VSTGUI::CBitmap *bg, *labels;
    VSTGUI::CPoint dragStart, dragCurrent, dragCornerOff;

    // pixel offsets of all FX slots:     A IFX1, A IFX2, B IFX1,   B IFX2,
    //                                    Send 1, Send 2, Global 1, Global 2
    const int fxslotpos[n_fx_slots][2] = {{18, 1},  {44, 1},  {18, 41}, {44, 41},
                                          {18, 21}, {44, 21}, {89, 11}, {89, 31}};

    const int scenelabelbox[n_scenes][2] = {{1, 1}, {1, 41}};
    const char *scenename[n_scenes] = {"A", "B"};

    VSTGUI::CCoord scenelabelboxWidth = 11, scenelabelboxHeight = 11;
    VSTGUI::CCoord fxslotWidth = 19, fxslotHeight = 11;

  public:
    CEffectSettings(const VSTGUI::CRect &size, VSTGUI::IControlListener *listener, long tag,
                    int current, std::shared_ptr<SurgeBitmaps> bitmapStore);

    virtual void draw(VSTGUI::CDrawContext *dc) override;

    virtual VSTGUI::CMouseEventResult onMouseDown(
        VSTGUI::CPoint &where,
        const VSTGUI::CButtonState &buttons) override; ///< called when a mouse down event occurs

    virtual VSTGUI::CMouseEventResult onMouseUp(
        VSTGUI::CPoint &where,
        const VSTGUI::CButtonState &buttons) override; ///< called when a mouse up event occurs

    virtual VSTGUI::CMouseEventResult onMouseEntered(VSTGUI::CPoint &where,
                                                     const VSTGUI::CButtonState &buttons) override
    {
        hovered = true;
        invalid();
        return VSTGUI::kMouseEventHandled;
    }

    virtual VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint &where,
                                                    const VSTGUI::CButtonState &buttons) override
    {
        hovered = false;
        invalid();
        return VSTGUI::kMouseEventHandled;
    }

    virtual VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint &where,
                                                   const VSTGUI::CButtonState &buttons) override;

    // since graphics asset for FX icons (bmp00136) has frames ordered according to fxt enum
    // just return FX id
    int get_fxtype(int id) { return id; }

    void set_type(int id, int t)
    {
        type[id] = t;
        setDirty(true);
    }

    void set_bypass(int bid)
    {
        bypass = bid;
        invalid();
    }

    void set_disable(int did) { disabled = did; }

    int get_disable() { return disabled; }

    int get_current() { return current; }

  private:
    void setColorsForFXSlot(VSTGUI::CDrawContext *dc, VSTGUI::CRect rect, int fxslot);

    CLASS_METHODS(CEffectSettings, VSTGUI::CControl)
};
