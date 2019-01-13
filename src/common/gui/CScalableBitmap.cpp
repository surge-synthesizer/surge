#include "CScalableBitmap.h"
#if MAC
#include <CoreFoundation/CoreFoundation.h>
#endif

CScalableBitmap::CScalableBitmap(CResourceDescription desc) : CBitmap(desc)
{
    int id = 0;
    if(desc.type == CResourceDescription::kIntegerType)
        id = (int32_t)desc.u.id;

    /*
    ** At this stage this scales array may seem like overkill since it only has two
    ** values. But to impelemnt scalability we will end up with multiple sizes, including
    ** 1.5x maybe 1.25x and so on. So build the architecture now to support that even
    ** though we only populate with two types. And since our sizes will include a 1.25x
    ** hash on integer percentages (so 100 -> 1x), not on floats.
    */

    scales = {{ 100, 200 }};
    scaleFilePostfixes[ 100 ] = "";
    scaleFilePostfixes[ 200 ] = "@2x";

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
}

void CScalableBitmap::draw (CDrawContext* context, const CRect& rect, const CPoint& offset, float alpha )
{
    /*
    ** For now this is a fixed 'retina' style draw of using 2x bitmaps at 0.5 scale if they exist
    */
    if (scaledBitmaps[ 200 ] != NULL)
    {
        CGraphicsTransform tf = CGraphicsTransform().scale( 0.5, 0.5 );
        CGraphicsTransform invtf = tf.inverse();
        
        CDrawContext::Transform tr(*context, tf);

        // Have to de-const these to do the transform alas
        CRect ncrect = rect;
        CRect nr = invtf.transform(ncrect);

        CPoint ncoff = offset;
        CPoint no = invtf.transform(ncoff);
        
        scaledBitmaps[ 200 ]->draw(context, nr, no, alpha);
    }
    else
    {
        CBitmap::draw(context, rect, offset, alpha);
    }
}
