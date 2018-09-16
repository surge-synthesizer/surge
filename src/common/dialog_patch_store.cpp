//-------------------------------------------------------------------------------------------------------
//
//	Shortcircuit
//
//	Copyright 2004 Claes Johanson
//
//-------------------------------------------------------------------------------------------------------

#include "miniedit_spawners.h"

#if MAC_CARBON

#include <Carbon/Carbon.h>

extern void* gBundleRef;

pascal OSStatus myWindowCloseHandler(  EventHandlerCallRef inRef,
									   EventRef inEvent,
									   void* userData);

bool spawn_patchstore_dialog(patchdata *p, vector<patchlist_category> *patch_category, int startcategory)
{	
	OSStatus err;
	IBNibRef     nibRef=0;
    WindowRef     NibWindow;
    
    static EventTypeSpec closeEvent={kEventClassControl,kEventControlHit};
    
    //setup interface from nib file
    CreateNibReferenceWithCFBundle ((CFBundleRef)VSTGUI::gBundleRef,CFSTR("surgesave"), &nibRef);
	if(!nibRef) return false;
    CreateWindowFromNib(nibRef, CFSTR("SavePatch"), &NibWindow);
    DisposeNibReference(nibRef);
    //install event handler for window to handle close box 
    InstallWindowEventHandler(  NibWindow,
                                NewEventHandlerUPP(myWindowCloseHandler),
                                1,&closeEvent,0,NULL);
	
    // set data	
	ControlID inID;
	inID.signature = 'VmbA';	
	
	HIViewRef hiroot = HIViewGetRoot (NibWindow);
	
	HIViewRef nameedit;	
	inID.id = 1;	err = HIViewFindByID (hiroot,inID,&nameedit);
	if(nameedit)
	{
		CFStringRef s = CFStringCreateWithCString(NULL,p->name.c_str(), kCFStringEncodingUTF8);
		HIViewSetText(nameedit,s);
		CFRelease(s);
	}

	HIViewRef authoredit;	
	inID.id = 2;	err = HIViewFindByID (hiroot,inID,&authoredit);
	if(authoredit)
	{
		CFStringRef s = CFStringCreateWithCString(NULL,p->author.c_str(), kCFStringEncodingUTF8);
		HIViewSetText(authoredit,s);
		CFRelease(s);
	}
	HIViewRef commentedit;	
	inID.id = 4;	err = HIViewFindByID (hiroot,inID,&commentedit);
	if(commentedit)
	{
		CFStringRef s = CFStringCreateWithCString(NULL,p->comments.c_str(), kCFStringEncodingUTF8);
		HIViewSetText(commentedit,s);
		CFRelease(s);
	}
	
	HIViewRef categorycombo;	
	inID.id = 3;	err = HIViewFindByID (hiroot,inID,&categorycombo);
	if(categorycombo)
	{
		CFStringRef s = CFStringCreateWithCString(NULL,p->category.c_str(), kCFStringEncodingUTF8);
		HIViewSetText(categorycombo,s);
		CFRelease(s);
		UInt n = patch_category->size();
		for (UInt i=startcategory; i<n; i++) 
		{
			CFStringRef s0 = CFStringCreateWithCString(NULL,patch_category->at(i).name.c_str(), kCFStringEncodingUTF8);
			err	= HIComboBoxAppendTextItem (categorycombo,s0,NULL);
			CFRelease(s0);
		}
	}
	
	ShowWindow(NibWindow);
    
    RunApplicationEventLoop();
	
	// check wheter save or cancel was clicked
	UInt32 result = 0;
	err = GetWindowProperty(NibWindow,'VmbA','Rslt', sizeof(UInt32),0,&result);
	if(err) result = 0;
	if(result)
	{
		char tmp[256];
		// retrieve data
		if(nameedit)
		{
			CFStringRef s = HIViewCopyText(nameedit);
			err = CFStringGetCString(s,tmp,sizeof(char)*256,kCFStringEncodingUTF8);
			p->name = tmp;
			CFRelease(s);
		}
		if(authoredit)
		{
			CFStringRef s = HIViewCopyText(authoredit);
			err = CFStringGetCString(s,tmp,sizeof(char)*256,kCFStringEncodingUTF8);
			p->author = tmp;
			CFRelease(s);
		}
		if(commentedit)
		{
			CFStringRef s = HIViewCopyText(commentedit);
			err = CFStringGetCString(s,tmp,sizeof(char)*256,kCFStringEncodingUTF8);
			p->comments = tmp;
			CFRelease(s);
		}
		if(categorycombo)
		{
			CFStringRef s = HIViewCopyText(categorycombo);
			err = CFStringGetCString(s,tmp,sizeof(char)*256,kCFStringEncodingUTF8);
			p->category = tmp;
			CFRelease(s);
		}
	}
	CFRelease(nameedit);
	CFRelease(authoredit);
	CFRelease(commentedit);
	CFRelease(categorycombo);
	
	HideWindow(NibWindow);
	DisposeWindow(NibWindow);
	   
    return result;		
}



pascal OSStatus myWindowCloseHandler(  EventHandlerCallRef inRef,
									   EventRef inEvent,
									   void* userData)
{
    ControlRef control;
    UInt32 cmd;
	//get control hit by event
    GetEventParameter(inEvent,kEventParamDirectObject,typeControlRef,NULL,sizeof(ControlRef),NULL,&control);
    WindowRef window=GetControlOwner(control);
	GetControlCommandID(control,&cmd);
	
	switch(cmd){
		case kHICommandCancel:
        case kHICommandOK: 
			UInt32 result = (cmd == kHICommandOK)?1:0;
			SetWindowProperty(window,'VmbA','Rslt',sizeof(UInt32),&result);
			QuitApplicationEventLoop();
			break;
    }
	
    return noErr;
}

#elif MAC_COCOA

bool spawn_patchstore_dialog(patchdata *p, vector<patchlist_category> *patch_category, int startcategory)
{
// TODO
   return false;
}

#elif WIN32

#include "stdafx.h"
#ifndef _WINDEF_
#include "windef.h"
#endif
#include "resource.h"

#include "dialog_patch_store.h"
#include <string>
using namespace std;

bool spawn_patchstore_dialog(patchdata *p, vector<patchlist_category> *patch_category, int startcategory)
{
	CPatchStoreDialog psd;		
	
	psd.name = p->name;
	psd.category = p->category;
	psd.comment = p->comments;
	psd.author = p->author;
	psd.patch_category = patch_category;
	psd.startcategory = startcategory;

	psd.DoModal(::GetActiveWindow(), NULL);	
	if (psd.updated)
	{
		p->name = psd.name;
		p->category = psd.category;
		p->comments = psd.comment;
		p->author = psd.author;
		return true;
	}
	return false;
}
#else
#error Unknown platform
#endif