#include <Cocoa/Cocoa.h>
#include "cocoa_utils.h"

double CocoaUtils::getDoubleClickInterval()
{
   return [NSEvent doubleClickInterval];
}