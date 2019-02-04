#include "DisplayInfo.h"
#include "UserInteractions.h"

namespace Surge
{
namespace GUI
{

using namespace VSTGUI;
    
float getDisplayBackingScaleFactor(CFrame *)
{
    Surge::UserInteractions::promptError("getDisplayBackingScaleFactor not implemented yet on linux.",
                                         "Software Error");
    return 1.0;
}
    
CRect getScreenDimensions(CFrame *)
{
    Surge::UserInteractions::promptError("getScreenDimensions not implemented yet on linux. Returning 1400x1050",
                                         "Software Error");
    return CRect(CPoint(0,0), CPoint(1400,1050)); // a typical mid-range 15" laptop
}

}
}
