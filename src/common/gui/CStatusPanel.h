//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "SurgeStorage.h"
#include "vstcontrols.h"

class SurgeGUIEditor;

class CStatusPanel : public VSTGUI::CControl
{
public:

    typedef enum {
        mpeMode,
        tuningMode,
        numDisplayFeatures
    } DisplayFeatures;
    
    CStatusPanel(const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, long tag, SurgeStorage* storage)
        : VSTGUI::CControl(size, listener, tag, 0)
        {
                  for( auto i=0; i<numDisplayFeatures; ++i )
          dispfeatures[i] = false;

        }
    
    void setDisplayFeature( DisplayFeatures df, bool v )
        {
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
    
    virtual void draw(VSTGUI::CDrawContext* dc);
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& button);
    
protected:
    bool dispfeatures[numDisplayFeatures];
    SurgeStorage* storage = nullptr;
    SurgeGUIEditor *editor = nullptr;
    VSTGUI::CRect mpeBox, tuningBox, tuningLock;
    
    
    CLASS_METHODS(CStatusPanel, VSTGUI::CControl)
};
