#include "UserInteractions.h"
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <ApplicationServices/ApplicationServices.h>

namespace Surge
{

namespace UserInteractions
{

void promptError(const std::string &message, const std::string &title,
                 SurgeGUIEditor *guiEditor)
{
    CFStringRef cfT = CFStringCreateWithCString(kCFAllocatorDefault, title.c_str(), kCFStringEncodingUTF8);
    CFStringRef cfM = CFStringCreateWithCString(kCFAllocatorDefault, message.c_str(), kCFStringEncodingUTF8);

    SInt32 nRes = 0;
    CFUserNotificationRef pDlg = NULL;
    const void* keys[] = {kCFUserNotificationAlertHeaderKey, kCFUserNotificationAlertMessageKey};
    const void* vals[] = {cfT, cfM};

    CFDictionaryRef dict =
        CFDictionaryCreate(0, keys, vals, sizeof(keys) / sizeof(*keys),
                           &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    pDlg = CFUserNotificationCreate(kCFAllocatorDefault, 0, kCFUserNotificationStopAlertLevel,
                                    &nRes, dict);

    CFRelease(cfT);
    CFRelease(cfM);
}

void promptError(const Surge::Error &error, SurgeGUIEditor *guiEditor)
{
    promptError(error.getMessage(), error.getTitle());
}

MessageResult promptOKCancel(const std::string &message, const std::string &title,
                             SurgeGUIEditor *guiEditor)
{
    CFStringRef cfT = CFStringCreateWithCString(kCFAllocatorDefault, title.c_str(), kCFStringEncodingUTF8);
    CFStringRef cfM = CFStringCreateWithCString(kCFAllocatorDefault, message.c_str(), kCFStringEncodingUTF8);

    CFOptionFlags responseFlags;
    CFUserNotificationDisplayAlert(0, kCFUserNotificationPlainAlertLevel, 0, 0, 0,
                                   cfT,
                                   cfM,
                                   CFSTR("OK"), CFSTR("Cancel"), 0, &responseFlags);

    CFRelease( cfT );
    CFRelease( cfM );

    if ((responseFlags & 0x3) != kCFUserNotificationDefaultResponse)
        return UserInteractions::CANCEL;
    return UserInteractions::OK;
}

void openURL(const std::string &url_str)
{
    CFURLRef url = CFURLCreateWithBytes (
        NULL,                        // allocator
        (UInt8*)url_str.c_str(),     // URLBytes
        url_str.length(),            // length
        kCFStringEncodingASCII,      // encoding
        NULL                         // baseURL
        );
    LSOpenCFURLRef(url,0);
    CFRelease(url);
}

void openFolderInFileBrowser(const std::string& folder)
{
   std::string url = "file://" + folder;
   UserInteractions::openURL(url);
}

};

};
