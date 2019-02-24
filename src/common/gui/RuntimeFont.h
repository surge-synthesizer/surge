#pragma once

#include "vstgui/vstgui.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/platform/iplatformfont.h"

namespace Surge
{
namespace GUI
{

/**
 * initializeRuntimeFont
 *
 * The role of this function is to load a font from the bundle, dll, or some other
 * source. Once the font file is loaded, it is used to create the VSTGUI 9 and 14 point 
 * fonts in global variables "displayFont" and "patchNameFont" if they are NULL
 * at calltime. The "displayFont" is used for most text rendering and the
 * "patchNameFont" is used for the patchname.
 *
 * The implementation is OS Specific.
 */
void initializeRuntimeFont();

}
}

/*
** The two extern globals we need to initialize, which are defined/created
** in SurgeGuieditor
*/
extern VSTGUI::CFontRef displayFont;
extern VSTGUI::CFontRef patchNameFont;
extern VSTGUI::CFontRef lfoTypeFont;
