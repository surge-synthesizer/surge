/*
** CScalableBitmap is an implmentation of VSGTUI::CBitmap which can 
** load bitmaps at multiple resolutions and draw them scaled accordingly
*/

#include <vstgui/vstgui.h>

#include <vector>
#include <map>

class CScalableBitmap : public VSTGUI::CBitmap
{
public:
    CScalableBitmap( CResourceDescription d );

    std::vector< int > scales;  // 100, 150, 200, 300 etc... - int percentages
    std::map< int, VSTGUI::CBitmap * > scaledBitmaps;
    std::map< int, std::string > scaleFilePostfixes;

    virtual void draw (CDrawContext* context, const CRect& rect, const CPoint& offset, float alpha);
};
