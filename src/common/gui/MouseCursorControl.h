#pragma once

/*
 *  cursormanipulation.h
 *
 */

#if MAC
// emulation of windows cursor handling
//#include <CGRemoteOperation.h>
/*struct POINT
{
int x, y;
};			*/

static void ShowCursor(bool b)
{}
static void GetCursorPos(VSTGUI::CPoint& p)
{
   /*Point ptMouse;
   GetMouse( &ptMouse );
   p->x = ptMouse.v;
   p->y = ptMouse.h;*/
   p.x = 0;
   p.y = 0;
   // TODO VSTGUI4
   // TODO COCOA
}

static void SetCursorPos(int x, int y)
{}
#endif
