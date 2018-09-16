//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"

extern CFontRef surge_minifont;

class gui_fxlabel : public CControl
{
public:
	gui_fxlabel(const CRect &size)
	: CControl(size,0,0,0)
	{
	}

	virtual void draw (CDrawContext*dc)
	{
      CRect size = getViewSize();
		CRect bl(size);
		bl.top = bl.bottom-2;
		CColor gray = {106,106,106,255};
		dc->setFillColor(gray);
		dc->drawRect(bl, kDrawFilled);
		dc->setFontColor(gray);
		//dc->setFont(kNormalFontSmaller,8,kBoldFace);		
		dc->setFont(surge_minifont);		
		dc->drawString(label.c_str(),size,kLeftText, false);
		setDirty (false);
	}
	void setLabel(string s)
	{
		label = s;
	}
private:
	string label;
	
	CLASS_METHODS(gui_fxlabel, CControl)
};