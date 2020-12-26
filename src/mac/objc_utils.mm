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

#include "objc_utils.h"
#include <Foundation/Foundation.h>

namespace Surge
{
namespace ObjCUtil
{
void loadUrlToPath(
    const std::string& urlS,
    const fs::path& path,
    const std::function<void(const std::string& url)>& onDone,
    const std::function<void(const std::string& url, const std::string& msg)>& onError)
{
   NSString* urlns = [NSString stringWithCString:urlS.c_str()
                                        encoding:[NSString defaultCStringEncoding]];
   NSURL* url = [NSURL URLWithString:urlns];

   // use the error: nullable here
   // https://developer.apple.com/documentation/foundation/nsdata/1547238-datawithcontentsofurl?language=objc
   NSError* err = nil;
   NSData* data = [NSData dataWithContentsOfURL:url options:NSDataReadingUncached error:&err];
   if (err)
   {
      onError(urlS, [[err localizedDescription] UTF8String]);
      return;
   }

   NSString* pathns = [NSString stringWithCString:path_to_string(path).c_str()
                                         encoding:[NSString defaultCStringEncoding]];
   if (![data writeToFile:pathns atomically:NO])
   {
      onError(urlS, "Unable to write to file");
      return;
   }

   // And if we are all OK
   onDone(urlS);
}
}

}