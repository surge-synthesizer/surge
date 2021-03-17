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
#include "SurgeParamConfig.h"
#include "SkinSupport.h"
#include "CursorControlGuard.h"

class CSurgeSlider : public VSTGUI::CControl,
                     public Surge::UI::CursorControlAdapterWithMouseDelta<CSurgeSlider>,
                     public Surge::UI::SkinConsumingComponent
{
  public:
    CSurgeSlider(const VSTGUI::CPoint &loc, long style, VSTGUI::IControlListener *listener,
                 long tag, bool is_mod, std::shared_ptr<SurgeBitmaps> bitmapStore,
                 SurgeStorage *storage = nullptr);
    ~CSurgeSlider();
    virtual void draw(VSTGUI::CDrawContext *) override;
    // virtual void mouse (VSTGUI::CDrawContext *pContext, VSTGUI::CPoint &where, long buttons =
    // -1); virtual bool onWheel (VSTGUI::CDrawContext *pContext, const VSTGUI::CPoint &where, float
    // distance);
    virtual bool onWheel(const VSTGUI::CPoint &where, const float &distane,
                         const VSTGUI::CButtonState &buttons) override;

    virtual VSTGUI::CMouseEventResult onMouseDown(
        VSTGUI::CPoint &where,
        const VSTGUI::CButtonState &buttons) override; ///< called when a mouse down event occurs
    virtual VSTGUI::CMouseEventResult onMouseUp(
        VSTGUI::CPoint &where,
        const VSTGUI::CButtonState &buttons) override; ///< called when a mouse up event occurs

    virtual VSTGUI::CMouseEventResult onMouseEntered(VSTGUI::CPoint &where,
                                                     const VSTGUI::CButtonState &buttons) override;

    virtual VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint &where,
                                                    const VSTGUI::CButtonState &buttons) override;

    virtual double getMouseDeltaScaling(const VSTGUI::CPoint &where,
                                        const VSTGUI::CButtonState &buttons);
    virtual VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint &where,
                                                   const VSTGUI::CButtonState &buttons) override
    {
        return onMouseMovedCursorHelper(where, buttons);
    }

    virtual void onMouseMoveDelta(const VSTGUI::CPoint &where, const VSTGUI::CButtonState &buttons,
                                  double dx, double dy);

    virtual void setLabel(const char *txt);
    virtual void setDynamicLabel(std::function<std::string()> lf)
    {
        hasLabFn = true;
        labfn = lf;
    }
    bool hasLabFn = false;
    std::function<std::string()> labfn;

    virtual void setModValue(float val);
    virtual float getModValue() { return modval; }
    virtual void setModMode(int mode)
    {
        modmode = mode;
    } // 0 - absolute pos, 1 - absolute mod, 2 - relative mod
    virtual void setMoveRate(float rate) { moverate = rate; }
    virtual void setModPresent(bool b)
    {
        has_modulation = b;
    } // true if the slider has modulation routings assigned to it
    virtual void setModCurrent(bool isCurrent, bool isBipolarModulator)
    {
        has_modulation_current = isCurrent;
        modulation_is_bipolar = isBipolarModulator;
        invalid();
    } // - " " - for the currently selected modsource
    virtual void bounceValue(const bool keeprest = false);

    virtual bool isInMouseInteraction();

    virtual void setValue(float val) override;

    std::function<bool()> isBipolFn = []() { return false; };
    void setBipolarFunction(std::function<bool()> f)
    {
        isBipolFn = f;
        invalid();
    }

    std::function<bool()> isDeactivatedFn = []() { return false; };
    void setDeactivatedFn(std::function<bool()> f)
    {
        isDeactivatedFn = f;
        invalid();
    }

    virtual void setTempoSync(bool b) { is_temposync = b; }

    void SetQuantitizedDispValue(float f);

#if !TARGET_JUCE_UI
    void setFont(VSTGUI::CFontRef f)
    {
        if (labfont)
            labfont->forget();
        labfont = f;
        labfont->remember();
    }
#endif
    VSTGUI::CFontRef labfont = nullptr;

    CLASS_METHODS(CSurgeSlider, CControl)

    bool in_hover = false;
    bool is_mod;
    bool disabled; // means it can't be used unless something else changes
    bool hasBeenDraggedDuringMouseGesture = false;

    bool isStepped = false;
    int intRange = 0;

    int font_size, text_hoffset, text_voffset;
    VSTGUI::CHoriTxtAlign text_align;
    VSTGUI::CTxtFace font_style;

    SurgeStorage *storage = nullptr;

    enum MoveRateState
    {
        kUnInitialized = 0,
        kLegacy,
        kSlow,
        kMedium,
        kExact
    };

    virtual void onSkinChanged() override;

    static MoveRateState sliderMoveRateState;

  private:
    VSTGUI::CBitmap *pHandle = nullptr, *pTray = nullptr, *pModHandle = nullptr,
                    *pTempoSyncHandle = nullptr, *pHandleHover = nullptr,
                    *pTempoSyncHoverHandle = nullptr;
    VSTGUI::CRect handle_rect, handle_rect_orig;
    VSTGUI::CPoint offsetHandle;
    int range;
    int controlstate;
    long style;
    float modval, qdvalue;
    char label[256];
    int modmode;
    float moverate;
    int typex, typey;
    int typehx, typehy;
    bool has_modulation, has_modulation_current, modulation_is_bipolar = false;
    bool is_temposync = false;
    VSTGUI::CPoint draghandlecenter;
    VSTGUI::CPoint startPosition;
    VSTGUI::CPoint currentPosition;
    float oldVal, *edit_value;
    int drawcount_debug;

    float restvalue, restmodval;
    bool wheelInitiatedEdit = false;
};
