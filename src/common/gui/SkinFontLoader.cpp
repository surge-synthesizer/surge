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

#include "SkinSupport.h"
#include <iostream>
#if MAC
#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#endif

void Surge::UI::addFontSearchPathToSystem(const fs::path& p)
{
#if MAC
   // Mac is traverse for TTF files and add to the path
   for(  const fs::path& ent : fs::directory_iterator(p))
   {
      if( ent.extension().native() == ".ttf" )
      {
         CFURLRef url = nullptr;
         auto path = path_to_string(ent);
         url = CFURLCreateFromFileSystemRepresentation(NULL, (const unsigned char*)path.c_str(),
                                                       path.size(), false);
         if (url)
         {
            CTFontManagerRegisterFontsForURL(url, kCTFontManagerScopeProcess, NULL);
            CFRelease(url);
         }
      }
   }
#else
   std::cerr << "Unsupported font search path addition" << std::endl;
#endif
}