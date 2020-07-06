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
#include "SurgeStorage.h"
#include "vstcontrols.h"
#include "SkinSupport.h"
#include "SurgeBitmaps.h"

class SurgeGUIEditor;

class CStatusPanel : public VSTGUI::CControl, public VSTGUI::IDropTarget, public Surge::UI::SkinConsumingComponnt
{
public:

    typedef enum {
        mpeMode,
        tuningMode,
        zoomOptions,
        numDisplayFeatures
    } DisplayFeatures;
    
    CStatusPanel(const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, long tag,
                 SurgeStorage* storage, std::shared_ptr<SurgeBitmaps> ibms)
       : VSTGUI::CControl(size, listener, tag, 0), bitmapStore(ibms) {
        for( auto i=0; i<numDisplayFeatures; ++i )
            dispfeatures[i] = false;
        doingDrag = false;
        this->storage = storage;
    }
    
    void setDisplayFeature( DisplayFeatures df, bool v ) {
        if( dispfeatures[df] != v )
        {
            dispfeatures[df] = v;
            invalid();
        }
    }

    void setEditor(SurgeGUIEditor *e)
    {
        editor = e;
    }
    
    virtual void draw(VSTGUI::CDrawContext* dc) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& button) override;

    virtual VSTGUI::CMouseEventResult onMouseMoved( VSTGUI::CPoint &where, const VSTGUI::CButtonState &buttons ) override;

    virtual VSTGUI::DragOperation onDragEnter(VSTGUI::DragEventData data) override
    {
        doingDrag = true;
        return VSTGUI::DragOperation::Copy;
    }
    virtual VSTGUI::DragOperation onDragMove(VSTGUI::DragEventData data) override
    {
        return VSTGUI::DragOperation::Copy;
    }
    virtual void onDragLeave(VSTGUI::DragEventData data) override
    {
        doingDrag = false;
    }
    virtual bool onDrop(VSTGUI::DragEventData data) override;
    virtual VSTGUI::SharedPointer<VSTGUI::IDropTarget> getDropTarget () override { return this; }

protected:
    bool doingDrag = false;
    bool dispfeatures[numDisplayFeatures];
    SurgeStorage* storage = nullptr;
    SurgeGUIEditor *editor = nullptr;
    VSTGUI::CRect mpeBox, tuningBox, tuningLock, zoomBox;
    std::shared_ptr<SurgeBitmaps> bitmapStore;
    
    CLASS_METHODS(CStatusPanel, VSTGUI::CControl)
};
