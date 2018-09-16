//-------------------------------------------------------------------------------------------------------
//
//	Shortcircuit
//
//	Copyright 2004 Claes Johanson
//
//-------------------------------------------------------------------------------------------------------
#pragma once

#include <vstgui/vstgui.h>

union rgbpixel
{	
	struct{
		unsigned char b,g,r,a;
	} ch;
	unsigned int rgba;
};

class CDIBitmap
{
public:	
	CDIBitmap (long width, long height);
	virtual ~CDIBitmap ();

	void draw (CDrawContext *pContext, CRect &rect, const CPoint &offset = CPoint (0, 0));	

	void clear(CColor color);
	unsigned int ccol_to_int(CColor col);	
	CColor int_to_ccol(unsigned int col);		

	void clear(unsigned int color);

	void fillRect(CRect r, unsigned int color);
	void setPixel(int x, int y, unsigned int color);

   inline int getWidth() { return _width;  }
   inline int getHeight() { return _height; }

   void begin();
   void commit();

	int _width=0,_height=0;
protected:
private:
   OwningPointer<CBitmap> _bitmap;
   OwningPointer<CBitmapPixelAccess> _bitmapAccess;
};