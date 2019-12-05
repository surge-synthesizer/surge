#include "UserInteractions.h"
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <ApplicationServices/ApplicationServices.h>
#include <AppKit/AppKit.h>

#include <iostream>
#include <sstream>
#include <unistd.h>

namespace Surge
{

namespace UserInteractions
{

void promptError(const std::string& message, const std::string& title, SurgeGUIEditor* guiEditor)
{
   CFStringRef cfT =
       CFStringCreateWithCString(kCFAllocatorDefault, title.c_str(), kCFStringEncodingUTF8);
   CFStringRef cfM =
       CFStringCreateWithCString(kCFAllocatorDefault, message.c_str(), kCFStringEncodingUTF8);

   SInt32 nRes = 0;
   CFUserNotificationRef pDlg = NULL;
   const void* keys[] = {kCFUserNotificationAlertHeaderKey, kCFUserNotificationAlertMessageKey};
   const void* vals[] = {cfT, cfM};

   CFDictionaryRef dict =
       CFDictionaryCreate(0, keys, vals, sizeof(keys) / sizeof(*keys),
                          &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

   pDlg = CFUserNotificationCreate(kCFAllocatorDefault, 0, kCFUserNotificationStopAlertLevel, &nRes,
                                   dict);

   CFRelease(cfT);
   CFRelease(cfM);
}

void promptError(const Surge::Error& error, SurgeGUIEditor* guiEditor)
{
   promptError(error.getMessage(), error.getTitle());
}

void promptInfo(const std::string& message, const std::string& title, SurgeGUIEditor* guiEditor)
{
   CFStringRef cfT =
       CFStringCreateWithCString(kCFAllocatorDefault, title.c_str(), kCFStringEncodingUTF8);
   CFStringRef cfM =
       CFStringCreateWithCString(kCFAllocatorDefault, message.c_str(), kCFStringEncodingUTF8);

   SInt32 nRes = 0;
   CFUserNotificationRef pDlg = NULL;
   const void* keys[] = {kCFUserNotificationAlertHeaderKey, kCFUserNotificationAlertMessageKey};
   const void* vals[] = {cfT, cfM};

   CFDictionaryRef dict =
       CFDictionaryCreate(0, keys, vals, sizeof(keys) / sizeof(*keys),
                          &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

   pDlg = CFUserNotificationCreate(kCFAllocatorDefault, 0, kCFUserNotificationNoteAlertLevel, &nRes,
                                   dict);

   CFRelease(cfT);
   CFRelease(cfM);
}


MessageResult
promptOKCancel(const std::string& message, const std::string& title, SurgeGUIEditor* guiEditor)
{
   CFStringRef cfT =
       CFStringCreateWithCString(kCFAllocatorDefault, title.c_str(), kCFStringEncodingUTF8);
   CFStringRef cfM =
       CFStringCreateWithCString(kCFAllocatorDefault, message.c_str(), kCFStringEncodingUTF8);

   CFOptionFlags responseFlags;
   CFUserNotificationDisplayAlert(0, kCFUserNotificationPlainAlertLevel, 0, 0, 0, cfT, cfM,
                                  CFSTR("OK"), CFSTR("Cancel"), 0, &responseFlags);

   CFRelease(cfT);
   CFRelease(cfM);

   if ((responseFlags & 0x3) != kCFUserNotificationDefaultResponse)
      return UserInteractions::CANCEL;
   return UserInteractions::OK;
}

void openURL(const std::string& url_str)
{
   CFURLRef url = CFURLCreateWithBytes(NULL,                    // allocator
                                       (UInt8*)url_str.c_str(), // URLBytes
                                       url_str.length(),        // length
                                       kCFStringEncodingASCII,  // encoding
                                       NULL                     // baseURL
   );
   LSOpenCFURLRef(url, 0);
   CFRelease(url);
}

void showHTML( const std::string &html )
{
    // Why does mktemp crash on macos I wonder?
    std::ostringstream fns;
    fns << "/var/tmp/surge-tuning." << rand() << ".html";

    FILE *f = fopen(fns.str().c_str(), "w" );
    if( f )
    {
        fprintf( f, "%s", html.c_str());
        fclose(f);
        std::string url = std::string("file://") + fns.str();
        openURL(url);
    }
}

void openFolderInFileBrowser(const std::string& folder)
{
   std::string url = "file://" + folder;
   UserInteractions::openURL(url);
}

void promptFileOpenDialog(const std::string& initialDirectory,
                          const std::string& filterSuffix,
                          std::function<void(std::string)> callbackOnOpen,
                          bool canSelectDirectories,
                          bool canCreateDirectories,
                          SurgeGUIEditor* guiEditor)
{
   // FIXME TODO - support the filterSuffix and initialDirectory
   NSOpenPanel* panel = [NSOpenPanel openPanel];
   [panel setCanChooseDirectories:canSelectDirectories];
   [panel setCanCreateDirectories:canCreateDirectories];
   [panel setDirectoryURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:(initialDirectory.c_str())]]];

   // Logic and others have wierd window ordering which can mean this somethimes pops behind.
   // See #1001.
   [panel makeKeyAndOrderFront:panel];
   [panel setLevel:CGShieldingWindowLevel()];
   
   [panel beginWithCompletionHandler:^(NSInteger result) {
     if (result == NSFileHandlingPanelOKButton)
     {
        NSURL* theDoc = [[panel URLs] objectAtIndex:0];
        NSString* path = [theDoc path];
        std::string pstring([path UTF8String]);
        callbackOnOpen(pstring);
     }
   }];
}

};

};
