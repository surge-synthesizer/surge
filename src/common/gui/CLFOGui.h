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
#include "SurgeStorage.h"
#include "CDIBitmap.h"
#include "DspUtilities.h"
#include "SkinSupport.h"
#include "SkinColors.h"
#include "SurgeBitmaps.h"
#include "CScalableBitmap.h"
#include "CursorControlGuard.h"

class CScalableBitmap;

class CLFOGui : public VSTGUI::CControl,
                public Surge::UI::SkinConsumingComponent,
                public Surge::UI::CursorControlAdapter<CLFOGui>
{
    enum control_states
    {
        cs_null = 0,
        cs_shape,
        cs_steps,
        cs_trigtray_toggle,
        cs_loopstart,
        cs_loopend,
        cs_linedrag,
    };

  public:
    const static int margin = 2;
    const static int margin2 = 7;
    const static int lpsize = 50;
    const static int scale = 18;
    const static int splitpoint = lpsize + 20;

    void drawtri(VSTGUI::CRect r, VSTGUI::CDrawContext *context, int orientation);

    CLFOGui(const VSTGUI::CRect &size, bool trigmaskedit, VSTGUI::IControlListener *listener = 0,
            long tag = 0, LFOStorage *lfodata = 0, SurgeStorage *storage = 0,
            StepSequencerStorage *ss = 0, MSEGStorage *ms = 0, FormulaModulatorStorage *fs = 0,
            std::shared_ptr<SurgeBitmaps> ibms = nullptr)
        : VSTGUI::CControl(size, listener, tag, 0),
          bitmapStore(ibms), Surge::UI::CursorControlAdapter<CLFOGui>(storage)
    {
        this->lfodata = lfodata;
        this->storage = storage;
        this->ss = ss;
        this->ms = ms;
        this->fs = fs;
        edit_trigmask = trigmaskedit;
        controlstate = 0;

        for (int i = 0; i < 16; ++i)
            draggedIntoTrigTray[i] = false;

        typeImg = nullptr;
    }

    void resetColorTable() { auto d = skin->getColor(Colors::LFO::Waveform::Wave); }
    // virtual void mouse (CDrawContext *pContext, VSTGUI::CPoint &where, long buttons = -1);
    virtual VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint &where,
                                                  const VSTGUI::CButtonState &buttons) override;
    virtual VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint &where,
                                                const VSTGUI::CButtonState &buttons) override;
    virtual VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint &where,
                                                   const VSTGUI::CButtonState &buttons) override;
    virtual bool onWheel(const VSTGUI::CPoint &where, const float &distance,
                         const VSTGUI::CButtonState &buttons) override;

    virtual void onSkinChanged() override { resetColorTable(); }

    virtual ~CLFOGui() {}
    virtual void draw(VSTGUI::CDrawContext *dc) override;
    void drawStepSeq(VSTGUI::CDrawContext *dc, VSTGUI::CRect &maindisp, VSTGUI::CRect &leftpanel);

    void invalidateIfIdIsInRange(int id);
    void invalidateIfAnythingIsTemposynced();

    void setTimeSignature(int n, int d)
    {
        tsNum = n;
        tsDen = d;
    }

    void openPopup(VSTGUI::CPoint &where);

    bool insideTypeSelector(const VSTGUI::CPoint &where) { return rect_shapes.pointInside(where); }

    virtual VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint &where,
                                                    const VSTGUI::CButtonState &buttons) override;

  protected:
    LFOStorage *lfodata;
    StepSequencerStorage *ss;
    MSEGStorage *ms;
    FormulaModulatorStorage *fs;
    SurgeStorage *storage;
    std::shared_ptr<SurgeBitmaps> bitmapStore;
    int tsNum = 4, tsDen = 4;

    VSTGUI::CRect shaperect[n_lfo_types];
    VSTGUI::CRect steprect[n_stepseqsteps];
    VSTGUI::CRect gaterect[n_stepseqsteps];
    VSTGUI::CRect rect_ls, rect_le, rect_shapes, rect_steps, rect_steps_retrig;
    VSTGUI::CRect ss_shift_left, ss_shift_right;
    bool edit_trigmask;
    int controlstate;
    int selectedSSrow = -1;

    int draggedStep = -1;
    int keyModMult = 0;
    bool forcedCursorToMSEGHand = false;

    bool draggedIntoTrigTray[16];
    int mouseDownTrigTray = -1;
    VSTGUI::CButtonState trigTrayButtonState;
    VSTGUI::CPoint rmStepStart, rmStepCurr;
    VSTGUI::CPoint barDragTop;

    bool enqueueCursorHide = false;

    int ss_shift_hover = 0;
    int lfo_type_hover = -1;
    CScalableBitmap *typeImg, *typeImgHover, *typeImgHoverOn;

    CLASS_METHODS(CLFOGui, VSTGUI::CControl)
};
