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

// FIXME remove this
#if !ESCAPE_FROM_VSTGUI

#include "vstgui/vstgui.h"

union rgbpixel
{
    struct
    {
        unsigned char b, g, r, a;
    } ch;
    unsigned int rgba;
};

class CDIBitmap
{
  public:
    CDIBitmap(long width, long height);
    virtual ~CDIBitmap();

    void draw(VSTGUI::CDrawContext *pContext, VSTGUI::CRect &rect,
              const VSTGUI::CPoint &offset = VSTGUI::CPoint(0, 0));

    void clear(VSTGUI::CColor color);
    unsigned int ccol_to_int(VSTGUI::CColor col);
    VSTGUI::CColor int_to_ccol(unsigned int col);

    void clear(unsigned int color);

    void fillRect(VSTGUI::CRect r, unsigned int color);
    void setPixel(int x, int y, unsigned int color);

    inline int getWidth() { return _width; }
    inline int getHeight() { return _height; }

    void begin();
    void commit();

    int _width = 0, _height = 0;

  protected:
  private:
    VSTGUI::SharedPointer<VSTGUI::CBitmap> _bitmap;
    VSTGUI::SharedPointer<VSTGUI::CBitmapPixelAccess> _bitmapAccess;
};

#endif
