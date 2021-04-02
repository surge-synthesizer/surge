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
#include "CursorControlGuard.h"

#define OSC_MOD_ANIMATION 0

class COscillatorDisplay : public VSTGUI::CControl,
                           public Surge::UI::SkinConsumingComponent,
                           public Surge::UI::CursorControlAdapter<COscillatorDisplay>
{
  public:
    COscillatorDisplay(const VSTGUI::CRect &size, VSTGUI::IControlListener *l,
                       OscillatorStorage *oscdata, SurgeStorage *storage)
        : VSTGUI::CControl(size, l, 0, 0), Surge::UI::CursorControlAdapter<COscillatorDisplay>(
                                               storage)
    {
        this->oscdata = oscdata;
        this->storage = storage;
        controlstate = 0;

        this->scene = oscdata->type.scene - 1;
        this->osc_in_scene = oscdata->type.ctrlgroup_entry;

        if (storage->getPatch()
                .dawExtraState.editor.oscExtraEditState[scene][osc_in_scene]
                .hasCustomEditor)
        {
            openCustomEditor();
        }
    }
    virtual ~COscillatorDisplay() {}

    /*
     * Custom Editor Support where I can change this UI based on a type to allow edits
     * Currently only used by alias oscillator
     */
    struct CustomEditor
    {
        CustomEditor(COscillatorDisplay *d) : disp(d) {}
        virtual ~CustomEditor() = default;
        virtual void draw(VSTGUI::CDrawContext *dc) = 0;

        virtual VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint &where,
                                                      const VSTGUI::CButtonState &buttons)
        {
            return VSTGUI::kMouseEventNotHandled;
        };
        virtual VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint &where,
                                                    const VSTGUI::CButtonState &buttons)
        {
            return VSTGUI::kMouseEventNotHandled;
        };
        virtual VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint &where,
                                                       const VSTGUI::CButtonState &buttons)
        {
            return VSTGUI::kMouseEventNotHandled;
        };
        virtual VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint &where,
                                                        const VSTGUI::CButtonState &buttons)
        {
            return VSTGUI::kMouseEventNotHandled;
        };
        virtual VSTGUI::CMouseEventResult onMouseEntered(VSTGUI::CPoint &where,
                                                         const VSTGUI::CButtonState &buttons)
        {
            return VSTGUI::kMouseEventNotHandled;
        };

        virtual bool onWheel(const VSTGUI::CPoint &where, const float &distance,
                             const VSTGUI::CButtonState &buttons)
        {
            return false;
        }

        // Entered and Exited too
        COscillatorDisplay *disp;
    };
    bool canHaveCustomEditor();
    void openCustomEditor();
    void closeCustomEditor();
    std::shared_ptr<CustomEditor> customEditor; // I really want unique but that
    // clashes with the VSTGUI copy semantics
    bool customEditorActive = false, editButtonHover = false;
    VSTGUI::CRect customEditButtonRect;

    virtual void draw(VSTGUI::CDrawContext *dc) override;

    void drawExtraEditButton(VSTGUI::CDrawContext *dc, const std::string &label);

    void loadWavetable(int id);
    void loadWavetableFromFile();

    // virtual void mouse (CDrawContext *pContext, VSTGUI::CPoint &where, long button = -1);
    virtual VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint &where,
                                                  const VSTGUI::CButtonState &buttons) override;
    virtual VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint &where,
                                                const VSTGUI::CButtonState &buttons) override;
    virtual VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint &where,
                                                   const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint &where,
                                            const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseEntered(VSTGUI::CPoint &where,
                                             const VSTGUI::CButtonState &buttons) override;
    bool onWheel(const VSTGUI::CPoint &where, const float &distance,
                 const VSTGUI::CButtonState &buttons) override;

    void invalidateIfIdIsInRange(int id);

#if OSC_MOD_ANIMATION
    void setIsMod(bool b)
    {
        is_mod = b;
        mod_time = 0;
    }
    void setModSource(modsources m) { modsource = m; }
    void tickModTime() { mod_time += 1.0 / 30.0; }
#endif

  protected:
    void populateMenu(VSTGUI::COptionMenu *m, int selectedItem);
    bool populateMenuForCategory(VSTGUI::COptionMenu *parent, int categoryId, int selectedItem);

    OscillatorStorage *oscdata;
    SurgeStorage *storage;
    unsigned int controlstate;

    int scene, osc_in_scene;

    bool doingDrag = false;
    enum
    {
        NONE,
        PREV,
        MENU,
        NEXT
    } isWTHover = NONE;

    static constexpr float scaleDownBy = 0.235;

#if OSC_MOD_ANIMATION
    bool is_mod = false;
    modsources modsource = ms_original;
    float mod_time = 0;
#endif
    VSTGUI::CRect rnext, rprev, rmenu;
    VSTGUI::CPoint lastpos;
    CLASS_METHODS(COscillatorDisplay, VSTGUI::CControl)
};
