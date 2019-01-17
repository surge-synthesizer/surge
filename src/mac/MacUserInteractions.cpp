#include "UserInteractions.h"
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>

namespace Surge
{
    namespace UserInteractions
    {
        void promptError(const Surge::Error &e)
        {
            promptError(e.getMessage(), e.getTitle());
        }

        void promptError(const std::string &message,
                         const std::string &title)
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

        UserInteractions::MessageResult promptOKCancel(const std::string &message,
                                                       const std::string &title)
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

        void openURL(const std::string &url)
        {
        }
    };
};

