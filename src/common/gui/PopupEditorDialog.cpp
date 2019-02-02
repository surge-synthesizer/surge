//-------------------------------------------------------------------------------------------------------
//
//	Shortcircuit
//
//	Copyright 2004 Claes Johanson
//
//-------------------------------------------------------------------------------------------------------
#if MAC_CARBON

pascal OSStatus meinWindowCloseHandler(EventHandlerCallRef inRef, EventRef inEvent, void* userData)
{
   ControlRef control;
   UInt32 cmd;
   // get control hit by event
   GetEventParameter(inEvent, kEventParamDirectObject, typeControlRef, NULL, sizeof(ControlRef),
                     NULL, &control);
   WindowRef window = GetControlOwner(control);
   GetControlCommandID(control, &cmd);

   switch (cmd)
   {
   case kHICommandCancel:
   case kHICommandOK:
      UInt32 result = (cmd == kHICommandOK) ? 1 : 0;
      SetWindowProperty(window, 'VmbA', 'Rslt', sizeof(UInt32), &result);
      QuitApplicationEventLoop();
      break;
   }

   return noErr;
}

void spawn_miniedit_text(char* c, int maxchars)
{
   OSStatus err;
   IBNibRef nibRef = 0;
   WindowRef NibWindow;

   static EventTypeSpec closeEvent = {kEventClassControl, kEventControlHit};

   // setup interface from nib file
   CreateNibReferenceWithCFBundle((CFBundleRef)VSTGUI::gBundleRef, CFSTR("surgesave"), &nibRef);
   if (!nibRef)
      return;
   CreateWindowFromNib(nibRef, CFSTR("TextEntry"), &NibWindow);
   DisposeNibReference(nibRef);
   // install event handler for window to handle close box
   InstallWindowEventHandler(NibWindow, NewEventHandlerUPP(meinWindowCloseHandler), 1, &closeEvent,
                             0, NULL);

   // set data
   ControlID inID;
   inID.signature = 'VmbA';
   ControlRef* outControl;

   HIViewRef hiroot = HIViewGetRoot(NibWindow);

   HIViewRef nameedit;
   inID.id = 1;
   err = HIViewFindByID(hiroot, inID, &nameedit);
   if (nameedit)
   {
      CFStringRef s = CFStringCreateWithCString(NULL, c, kCFStringEncodingUTF8);
      HIViewSetText(nameedit, s);
      CFRelease(s);
   }

   ShowWindow(NibWindow);

   RunApplicationEventLoop();

   // check wheter save or cancel was clicked
   UInt32 result = 0;
   err = GetWindowProperty(NibWindow, 'VmbA', 'Rslt', sizeof(UInt32), 0, &result);
   if (err)
      result = 0;
   if (result)
   {
      char tmp[256];
      // retrieve data
      if (nameedit)
      {
         CFStringRef s = HIViewCopyText(nameedit);
         err = CFStringGetCString(s, c, sizeof(char) * maxchars, kCFStringEncodingUTF8);
         CFRelease(s);
      }
   }
   CFRelease(nameedit);

   HideWindow(NibWindow);
   DisposeWindow(NibWindow);
}
#elif MAC_COCOA

#include "cocoa_utils.h"

void spawn_miniedit_text(char* c, int maxchars)
{
  // Bounce this over to objective C by using the CocoaUtils class
  CocoaUtils::miniedit_text_impl( c, maxchars );
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

void spawn_miniedit_text(char* c, int maxchars)
{
   PopupEditorDialog me;
   me.SetText(c);
   me.DoModal(::GetActiveWindow(), NULL);
   if (me.updated)
   {
      strncpy(c, me.textdata, maxchars);
   }
}
#elif LINUX

#include "UserInteractions.h"

void spawn_miniedit_text(char* c, int maxchars)
{
   // FIXME: Implement text edit popup on Linux.
   Surge::UserInteractions::promptError( "miniedit_text is not implemented on linux", "Unimplemented Feature" );
}

#endif
