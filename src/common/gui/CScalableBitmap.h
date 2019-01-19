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

    virtual void draw (CDrawContext* context, const CRect& rect, const CPoint& offset, float alpha);

    /*
    ** The 'zoom factor' is set statically across all bitmaps since it is a
    ** function of the current editor which contains them.
    **
    ** If in the future we have a non 1:1 editor -> instance relationship we
    ** may need to reconsider the management of this factor.
    **
    ** This zoom factor is the product of the logical (user) zoom factor
    ** and any zoom the display may apply, which is checked in SurgeGUIEditor::getDisplayBackingScale
    ** and applied *before* this call. See SurgeGUIEditor::setZoomFactor.
    */
    static void setPhysicalZoomFactor (int zoomFactor); // See comment on zoom factor units in SurgeGUIEditor.h

private:
    std::vector< int > scales;  // 100, 150, 200, 300 etc... - int percentages
    std::map< int, VSTGUI::CBitmap * > scaledBitmaps;
    std::map< int, std::string > scaleFilePostfixes;
    int lastSeenZoom, bestFitScaleGroup;

    static int currentPhysicalZoomFactor;
};
