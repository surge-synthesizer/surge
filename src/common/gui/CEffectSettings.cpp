#include "globals.h"
#include "SurgeStorage.h"
#include "CEffectSettings.h"
#include "CScalableBitmap.h"
#include "SurgeBitmaps.h"
#include "SurgeGUIEditor.h"

using namespace VSTGUI;

// positions of FX slots   A IFX1   A IFX2     B IFX1    B IFX2    Send 1    Send 2   Global 1  Global 2
const int blocks[8][2] = {{18, 1},  {44, 1},  {18, 41}, {44, 41}, {18, 21}, {44, 21}, {89, 11}, {89, 31}};

// since graphics asset for FX icons (bmp00136) has frames ordered according to fxt enum
// just return FX id
int get_fxtype(int id)
{
   return id;
}

CEffectSettings::CEffectSettings(const CRect& size,
                                 IControlListener* listener,
                                 long tag,
                                 int current,
                                 std::shared_ptr<SurgeBitmaps> bitmapStore)
    : CControl(size, listener, tag, 0)
{
   this->current = current;
   bg = bitmapStore->getBitmap(IDB_FX_GRID);
   labels = bitmapStore->getBitmap(IDB_FX_TYPE_ICONS);
   disabled = 0;
}

void CEffectSettings::draw(CDrawContext* dc)
{
   CRect size = getViewSize();

   if (bg)
   {
      bg->draw(dc, size, CPoint(0, 0), 0xff);
      // source position in bitmap
      // CPoint where (0, heightOfOneImage * (long)((value * (float)(rows*columns - 1) + 0.5f)));
      // pBackground->draw(dc,size,where,0xff);
   }
   if (labels)
   {
      for (int i = 0; i < n_fx_slots; i++)
      {
         CRect r(0, 0, 17, 9);
         r.offset(size.left, size.top);
         r.offset(blocks[i][0], blocks[i][1]);
         int stype = 0;
         if (disabled & (1 << i))
            stype = 4;
         switch (bypass)
         {
         case fxb_no_fx:
            stype = 2;
            break;
         case fxb_no_sends:
            if ((i == 4) || (i == 5))
               stype = 2;
            break;
         case fxb_scene_fx_only:
            if (i > 3)
               stype = 2;
            break;
         }
         if (i == current)
            stype += 1;
         labels->draw(dc, r, CPoint(17 * stype, 9 * get_fxtype(type[i])), 0xff);
      }
   }

   if (mouseActionMode == drag && dragSource >= 0 && dragSource < n_fx_slots)
   {
      auto vs = getViewSize();
      vs = vs.inset(1,1);
      auto r = CRect( dragCurrent - dragCornerOff, CPoint( 17, 9 ) );
      // r = vs.bound(r); this just pushes it inside; we need to keep constant size so
      float dx = 0, dy=0;
      if( r.top < vs.top ) { dy = vs.top - r.top; }
      if( r.bottom > vs.bottom ) { dy = vs.bottom - r.bottom; }
      if( r.left < vs.left ) { dx = vs.left - r.left; }
      if( r.right > vs.right ) { dx = vs.right - r.right; }
      r.top += dy;
      r.bottom += dy;
      r.right += dx;
      r.left += dx;

      auto q = r;
      q = q.extend(1,1);
      dc->setFillColor(skin->getColor( Colors::Effect::Grid::Border));
      dc->drawRect( q, kDrawFilled );
      labels->draw( dc, r, CPoint( 17, 9 * get_fxtype(type[dragSource])), 0xff );
   }
   setDirty(false);
}

CMouseEventResult CEffectSettings::onMouseDown(CPoint& where, const CButtonState& buttons)
{
   if (listener && (buttons & (kMButton | kButton4 | kButton5)))
   {
      listener->controlModifierClicked(this, buttons);
      return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
   }

   mouseActionMode = click;
   dragStart = where;
   dragSource = -1;
   for (int i = 0; i < n_fx_slots; i++)
   {
      CRect size = getViewSize();
      CRect r(0, 0, 17, 9);
      r.offset(size.left, size.top);
      r.offset(blocks[i][0], blocks[i][1]);
      if (r.pointInside(where))
      {
         dragSource = i;
         dragCornerOff = where - r.getTopLeft();
      }
   }
   return kMouseEventHandled;
}

CMouseEventResult CEffectSettings::onMouseUp(CPoint& where, const CButtonState& buttons)
{
   if (mouseActionMode == drag && dragSource >= 0 && dragSource < n_fx_slots)
   {
      int droppedOn = -1;
      for (int i = 0; i < n_fx_slots; i++)
      {
         CRect size = getViewSize();
         CRect r(0, 0, 17, 9);
         r.offset(size.left, size.top);
         r.offset(blocks[i][0], blocks[i][1]);
         if (r.pointInside(where))
         {
            droppedOn = i;
         }
      }
      auto sge = dynamic_cast<SurgeGUIEditor*>(listener);
      if( sge && droppedOn >= 0 )
      {
         auto m = SurgeSynthesizer::FXReorderMode::SWAP;
         if( buttons & kControl )
            m = SurgeSynthesizer::FXReorderMode::COPY;
         if( buttons & kShift )
            m = SurgeSynthesizer::FXReorderMode::MOVE;
         sge->swapFX(dragSource, droppedOn, m);

         current = droppedOn;
         if (listener)
            listener->valueChanged(this);
      }
      mouseActionMode = none;
      invalid();
   }
   else if( mouseActionMode == click )
   {
      mouseActionMode = none;
      if( ! ( ( buttons.getButtonState() & kLButton ) || ( buttons.getButtonState() & kRButton ) ) )
         return kMouseEventHandled;

      for (int i = 0; i < n_fx_slots; i++)
      {
         CRect size = getViewSize();
         CRect r(0, 0, 17, 9);
         r.offset(size.left, size.top);
         r.offset(blocks[i][0], blocks[i][1]);
         if (r.pointInside(where))
         {
            if (buttons.getButtonState() & kRButton )
               disabled ^= (1 << i);
            else
               current = i;
            invalid();
         }
      }

      if (listener)
         listener->valueChanged(this);
   }
   return kMouseEventHandled;
}

CMouseEventResult CEffectSettings::onMouseMoved(CPoint& where, const CButtonState& buttons)
{
   if( mouseActionMode == click )
   {
      float dx = dragStart.x - where.x;
      float dy = dragStart.y - where.y;
      float dist = sqrt( dx * dx + dy * dy );

      if (dist > 3 && dragSource >= 0 && dragSource < n_fx_slots)
      {
         mouseActionMode = drag;
      }
   }

   if( mouseActionMode == drag )
   {
      dragCurrent = where;
      invalid();
   }
   return kMouseEventHandled;
}
