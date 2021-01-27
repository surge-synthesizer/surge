/*
** Information about the physical display upon which I reside
**
** As with UserInteractions, this is just a header; the actual implementations
** are in src/(mac|win|linux)/(Mac|Win|Linux)DisplayInformation.cpp
**
** Most of these APIs will require a reference to a UI object of some form
** so make each of the functions take a VSTGUI::CFrame *
*/

#pragma once

#include "vstcontrols.h"

namespace Surge
{
namespace GUI
{

/*
** Return the backing scale factor. This is the scale factor which maps a phyiscal
** pixel to a logical pixel. It is in units that a high density display (so 4 physical
** pixels in a single pixel square) would have a backing scale factor of 2.0.
**
** We retain this value as a float and do not scale it by 100, like we do with
** user specified scales, to better match the OS API
*/
float getDisplayBackingScaleFactor(VSTGUI::CFrame *);

/*
** Return the screen dimensions of the best screen containing this frame. If the
** frame is not valid or has not yet been shown or so on, return a screen of
** size 0x0 at position 0,0.
*/
VSTGUI::CRect getScreenDimensions(VSTGUI::CFrame *);
} // namespace GUI
} // namespace Surge
