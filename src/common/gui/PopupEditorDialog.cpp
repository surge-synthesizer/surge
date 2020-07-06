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

#include "PopupEditorSpawner.h"

#if MAC

#include "cocoa_utils.h"

void spawn_miniedit_text(char* c, int maxchars, const char *prompt, const char *title)
{
  // Bounce this over to objective C by using the CocoaUtils class
   CocoaUtils::miniedit_text_impl( c, maxchars, prompt, title );
}

#elif WIN32
#include "stdafx.h"
/*#ifndef _WINDEF_
#include "windef.h"
#endif*/
#include "resource.h"

#include "PopupEditorDialog.h"

using namespace std;

extern CAppModule _Module;

#if 0
float spawn_miniedit_float(float f, int ctype)
{
   PopupEditorDialog me;
   me.SetFValue(f);
   me.DoModal(::GetActiveWindow(), NULL);
   if (me.updated)
      return me.fvalue;
   return f;
}

int spawn_miniedit_int(int i, int ctype)
{
   PopupEditorDialog me;
   me.SetValue(i);
   me.irange = 16777216;
   me.DoModal(::GetActiveWindow(), NULL);
   if (me.updated)
      return me.ivalue;
   return i;
}
#endif

void spawn_miniedit_text(char* c, int maxchars, const char* prompt, const char* title)
{
   PopupEditorDialog me;
   strcpy( me.prompt, prompt );
   strcpy( me.title, title );
   me.SetText(c);
   me.DoModal(::GetActiveWindow(), NULL);
   if (me.updated)
   {
      strncpy(c, me.textdata, maxchars);
      c[maxchars-1] = 0;
   }
}
#elif LINUX

#include "UserInteractions.h"
#include <string.h>

void spawn_miniedit_text(char* c, int maxchars, const char* prompt, const char* title)
{
   // FIXME: Implement text edit popup on Linux.
   char cmd[1024];
   snprintf(cmd, 1024, "zenity --entry --entry-text \"%s\" --text \"%s\" --title \"%s\"", c, prompt, title );
   FILE *z = popen( cmd, "r" );
   if( ! z )
   {
      // leave c unchanged - output == input
      return;
   }
   char buffer[ 1024 ];
   if (!fscanf(z, "%1024s", buffer))
   {
      // leave c unchanged - output == input
      return;
   }
   pclose(z);

   // copy c and leave room for a terminator
   strncpy( c, buffer, maxchars-1);
   c[maxchars-1] = 0;

}

#endif
