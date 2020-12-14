// -*-c++-*-
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
#include "CursorControlGuard.h"
#include "SkinSupport.h"

class CMenuAsSlider : public VSTGUI::CControl, public Surge::UI::SkinConsumingComponent,
                      public Surge::UI::CursorControlAdapter<CMenuAsSlider>
{
public:
   CMenuAsSlider(const VSTGUI::CPoint& loc,
                 VSTGUI::IControlListener* listener,
                 long tag,
                 std::shared_ptr<SurgeBitmaps> bitmapStore,
                 SurgeStorage* storage = nullptr) : CMenuAsSlider( loc, VSTGUI::CPoint( 133, 22 ), listener, tag, bitmapStore, storage ) {}
   CMenuAsSlider(const VSTGUI::CPoint& loc,
                 const VSTGUI::CPoint& size,
                 VSTGUI::IControlListener* listener,
                 long tag,
                 std::shared_ptr<SurgeBitmaps> bitmapStore,
                 SurgeStorage* storage = nullptr);
   virtual ~CMenuAsSlider();
   virtual void draw(VSTGUI::CDrawContext*) override;
   void setLabel( const char* lab ) { label = lab; }

   virtual bool onWheel(const VSTGUI::CPoint& where, const float &distane, const VSTGUI::CButtonState& buttons) override;

   virtual VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override; ///< called when a mouse down event occurs
   virtual VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
   virtual VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;

   virtual VSTGUI::CMouseEventResult onMouseEntered(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override
      {
         isHover = true;
         wheelDistance = 0;
         invalid();
         return VSTGUI::kMouseEventHandled;
      }
   
   virtual VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override
      {
         isHover = false;
         wheelDistance = 0;
         invalid();
         return VSTGUI::kMouseEventHandled;
      }


   void setMinMax( int i, int x ) {
      iMin = i; iMax = x;
   }

   void setDeactivated( bool d ) { deactivated = d; }
   bool deactivated = false;
   
   CLASS_METHODS(CMenuAsSlider, CControl)
   bool in_hover = false;
   SurgeStorage* storage = nullptr;

   int bgid = IDB_MENU_AS_SLIDER;
   void setBackgroundID(int q) { bgid = q; onSkinChanged(); }

   bool filtermode = false;
   void setFilterMode(bool b) {
      filtermode = b;
      invalid();
   }
   
   virtual void onSkinChanged() override;

   VSTGUI::CRect dragRegion;
   bool hasDragRegion = false;
   VSTGUI::CPoint dragStart;
   void setDragRegion( const VSTGUI::CRect &dragRegion );

   int dglphyid = -1, dglyphsize = -1;
   void setDragGlyph( int id, int size = 18 ) {
      dglphyid = id; dglyphsize = size; onSkinChanged();
   }
   std::vector<std::pair<int,int>> glyphIndexMap;

   std::vector<int> intOrdering;
   float nextValueInOrder( float valueFrom, int inc );

private:
   VSTGUI::CBitmap *pBackground = nullptr, *pBackgroundHover = nullptr, *pGlyph = nullptr, *pGlyphHover = nullptr;
   std::string label = "";
   bool isHover = false;
   bool isDragRegionDrag = false;
   enum {
      unk,
      dirx,
      diry
   } dragDir = unk;
   float wheelDistance = 0;
   
   int iMin = 0, iMax = 10;
};
