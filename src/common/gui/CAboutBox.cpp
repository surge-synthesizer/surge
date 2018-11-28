#include "CAboutBox.h"
#include "resource.h"
#include <stdio.h>

//------------------------------------------------------------------------
// CAboutBox
//------------------------------------------------------------------------
// one click draw its pixmap, an another click redraw its parent
CAboutBox::CAboutBox(const CRect& size,
                     IControlListener* listener,
                     long tag,
                     CBitmap* background,
                     CRect& toDisplay,
                     CPoint& offset,
                     CBitmap* aboutBitmap)
    : CControl(size, listener, tag, background), toDisplay(toDisplay), offset(offset)
{
   _aboutBitmap = aboutBitmap;
   boxHide(false);
}

//------------------------------------------------------------------------
CAboutBox::~CAboutBox()
{}

//------------------------------------------------------------------------
void CAboutBox::draw(CDrawContext* pContext)
{
   if (value)
   {
      _aboutBitmap->draw(pContext, getViewSize(), CPoint(0, 0), 0xff);
   }
   else
   {
   }

   setDirty(false);
}

//------------------------------------------------------------------------
bool CAboutBox::hitTest(const CPoint& where, const CButtonState& buttons)
{
   bool result = CView::hitTest(where, buttons);
   if (result && !(buttons & kLButton))
      return false;
   return result;
}

//------------------------------------------------------------------------

void CAboutBox::boxShow()
{
   setViewSize(toDisplay);
   setMouseableArea(toDisplay);
   value = 1.f;
   getFrame()->invalid();
}
void CAboutBox::boxHide(bool invalidateframe)
{
   CRect nilrect(0, 0, 0, 0);
   setViewSize(nilrect);
   setMouseableArea(nilrect);
   value = 0.f;
   if (invalidateframe)
      getFrame()->invalid();
}

// void CAboutBox::mouse (CDrawContext *pContext, CPoint &where, long button)
CMouseEventResult
CAboutBox::onMouseDown(CPoint& where,
                       const CButtonState& button) ///< called when a mouse down event occurs
{
   if (!(button & kLButton))
      return kMouseEventNotHandled;

   boxHide();

   return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}

//------------------------------------------------------------------------
void CAboutBox::unSplash()
{
   setDirty();
   value = 0.f;

   setViewSize(keepSize);
   if (getFrame())
   {
      if (getFrame()->getModalView() == this)
      {
         getFrame()->setModalView(NULL);
      }
   }
}