#include "CAboutBox.h"
#include "resource.h"
#include <stdio.h>

//------------------------------------------------------------------------
// CAboutBox
//------------------------------------------------------------------------
// one click draw its pixmap, an another click redraw its parent
CAboutBox::CAboutBox (const CRect &size, IControlListener *listener, long tag,
                              CBitmap *background,
                              CRect   &toDisplay,
                              CPoint  &offset)
:	CControl (size, listener, tag, background), 
	toDisplay (toDisplay), offset (offset)
{
	CBM_about = new CBitmap (CResourceDescription(IDB_ABOUT));
	boxHide(false);
}

//------------------------------------------------------------------------
CAboutBox::~CAboutBox ()
{	
	CBM_about->forget(); 
}

//------------------------------------------------------------------------
void CAboutBox::draw (CDrawContext *pContext)
{
	if (value)
	{				
		//CBM_about->draw(pContext,size);
		CBM_about->draw(pContext,getViewSize(),CPoint(0,0),0xff);						
	}
	else
	{		
	}

	setDirty (false);
}

//------------------------------------------------------------------------
bool CAboutBox::hitTest (const CPoint& where, const CButtonState& buttons)
{
	bool result = CView::hitTest (where, buttons);
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
	CRect nilrect(0,0,0,0);
   setViewSize(nilrect);
   setMouseableArea(nilrect);
	value = 0.f;
	if(invalidateframe) getFrame()->invalid();
}

//void CAboutBox::mouse (CDrawContext *pContext, CPoint &where, long button)
CMouseEventResult CAboutBox::onMouseDown (CPoint &where, const CButtonState& button)											///< called when a mouse down event occurs
{	
	if (!(button & kLButton))
		return kMouseEventNotHandled;

	boxHide();		
/*
	CFrame *frame = getFrame();	
	if(!frame) return kMouseEventNotHandled;

	value = !value;

	if (value)
	{			
		if (frame->setModalView (this))
		{
			keepSize = size;
			size = toDisplay;
			mouseableArea = size;
			if (listener)
				listener->valueChanged (this);
		}		
		invalid();	
	}
	else
	{		
		frame->setModalView (NULL);				

		invalid();
		size = keepSize;
		mouseableArea = size;
		if (listener)
			listener->valueChanged (this);		
				
	}*/
	return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}

//------------------------------------------------------------------------
void CAboutBox::unSplash ()
{
	setDirty ();
	value = 0.f;

	setViewSize(keepSize);
	if (getFrame ())
	{
		if (getFrame ()->getModalView () == this)
		{
			getFrame ()->setModalView (NULL);
	// ctnvg		getFrame ()->redraw ();
		}
	}
}