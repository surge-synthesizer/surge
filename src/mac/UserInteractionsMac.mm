#include "UserInteractions.h"
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <ApplicationServices/ApplicationServices.h>
#include <AppKit/AppKit.h>

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
