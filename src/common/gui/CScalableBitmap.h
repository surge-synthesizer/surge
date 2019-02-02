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
    CScalableBitmap(VSTGUI::CResourceDescription d);

    virtual void draw (VSTGUI::CDrawContext* context, const VSTGUI::CRect& rect, const VSTGUI::CPoint& offset, float alpha);

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

    /*
    ** VST2 has an error where the background doesn't zoom with the frame. Everything else
    ** does though. So we need to hand scale the background and only the background image
    ** when we draw it. We do that, simply, by having this class have an extra scale factor
    ** which we apply to the BG Bitmap in Vst2PluginInstance::handleZoom.
    */
    void setExtraScaleFactor (int a) { extraScaleFactor = a; }

private:
    std::vector< int > scales;  // 100, 150, 200, 300 etc... - int percentages
    std::map< int, VSTGUI::CBitmap * > scaledBitmaps;
    int lastSeenZoom, bestFitScaleGroup;
    int extraScaleFactor;

    static int currentPhysicalZoomFactor;
};
