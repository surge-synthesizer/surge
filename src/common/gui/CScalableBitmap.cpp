#include "CScalableBitmap.h"
#if MAC
#include <CoreFoundation/CoreFoundation.h>
#endif

// Remember this is user zoom * display zoom. See comment in CScalableBitmap.h
int  CScalableBitmap::currentPhysicalZoomFactor = 100;
void CScalableBitmap::setPhysicalZoomFactor(int zoomFactor)
{
    currentPhysicalZoomFactor = zoomFactor;
}

CScalableBitmap::CScalableBitmap(CResourceDescription desc) : CBitmap(desc)
{
    int id = 0;
    if(desc.type == CResourceDescription::kIntegerType)
        id = (int32_t)desc.u.id;

    /*
    ** Remember, Scales are the percentage scale in units of percents. So 100 is 1x.
    ** This integerification allows us to hash on the scale values and still support
    ** things like a 1.25 bitmap set.
    */
    
    scales = {{ 100, 150, 200, 300, 400 }}; // This is the collection of sizes we currently ask skins to export.
    scaleFilePostfixes[ 100 ] = "";
    scaleFilePostfixes[ 150 ] = "@15x";
    scaleFilePostfixes[ 200 ] = "@2x";
    scaleFilePostfixes[ 300 ] = "@3x";
    scaleFilePostfixes[ 400 ] = "@4x";

    for(auto sc : scales)
    {
        auto postfix = scaleFilePostfixes[sc];

        char filename [1024];
        snprintf (filename, 1024, "scalable/bmp%05d%s.png", id, postfix.c_str());
            
        CBitmap *tmp = new CBitmap(CResourceDescription( filename ));

        if(tmp->getWidth() > 0)
        {
            scaledBitmaps[sc] = tmp;
        }
        else
        {
            scaledBitmaps[sc] = NULL;
        }
        
    }
    lastSeenZoom = currentPhysicalZoomFactor;
    additionalZoom = 100;
}


void CScalableBitmap::draw (CDrawContext* context, const CRect& rect, const CPoint& offset, float alpha )
{
    if (lastSeenZoom != currentPhysicalZoomFactor)
    {
        int ns = -1;
        for (auto s : scales)
        {
            if (s >= currentPhysicalZoomFactor && ns < 0)
                ns = s;
        }
        if (ns<0)
        {
            ns = scales.back();
        }
        bestFitScaleGroup = ns;
        lastSeenZoom = currentPhysicalZoomFactor;
    }

    // Check if my bitmaps are there and if so use them
    if (scaledBitmaps[ bestFitScaleGroup ] != NULL)
    {
        // Seems like you would do this backwards; but the TF shrinks and the invtf regrows for positioning
        // but it is easier to calculate the grow one since it is at our scale
        CGraphicsTransform invtf = CGraphicsTransform().scale( bestFitScaleGroup / 100.0, bestFitScaleGroup / 100.0 );
        CGraphicsTransform tf = invtf.inverse().scale( additionalZoom / 100.0, additionalZoom / 100.0 );
        
        CDrawContext::Transform tr(*context, tf);

        // Have to de-const these to do the transform alas
        CRect ncrect = rect;
        CRect nr = invtf.transform(ncrect);

        CPoint ncoff = offset;
        CPoint no = invtf.transform(ncoff);
        
        scaledBitmaps[ bestFitScaleGroup ]->draw(context, nr, no, alpha);
    }
    else
    {
        // and if not, fall back to the default bitmap but still apply the additional zoom
        CGraphicsTransform tf = CGraphicsTransform().scale( additionalZoom / 100.0, additionalZoom / 100.0 );
        CDrawContext::Transform tr(*context, tf);
        CBitmap::draw(context, rect, offset, alpha);
    }
}

