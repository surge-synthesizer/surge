#include "DisplayInfo.h"
#include "UserInteractions.h"

namespace Surge
{
namespace GUI
{

using namespace VSTGUI;
    
float getDisplayBackingScaleFactor(CFrame *)
{
    return 1.0;
}
    
CRect getScreenDimensions(CFrame *)
{
    return CRect(CPoint(0,0), CPoint(0,0));
}

}
}
