//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
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
        numDisplayFeatures
    } DisplayFeatures;
    
    CStatusPanel(const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, long tag,
                 SurgeStorage* storage, std::shared_ptr<SurgeBitmaps> ibms)
       : VSTGUI::CControl(size, listener, tag, 0), bitmapStore(ibms) {
        for( auto i=0; i<numDisplayFeatures; ++i )
            dispfeatures[i] = false;
        doingDrag = false;
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
    VSTGUI::CRect mpeBox, tuningBox, tuningLock;
    std::shared_ptr<SurgeBitmaps> bitmapStore;
    
    CLASS_METHODS(CStatusPanel, VSTGUI::CControl)
};
