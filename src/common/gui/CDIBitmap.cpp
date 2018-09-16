#include "cdibitmap.h"
#include "dsputils.h"


/*HBITMAP CreateDIBSection(
  HDC hdc,                 // handle to DC
  CONST BITMAPINFO *pbmi,  // bitmap data
  UINT iUsage,             // data type indicator
  VOID **ppvBits,          // bit values
  HANDLE hSection,         // handle to file mapping object
  DWORD dwOffset           // offset to bitmap bit values
);
*/
#if MAC
void ImageDataReleaseFunc(void *info, void *data, size_t size)
{
    if (data != NULL)
    {
        free(data);
    }
}
#endif

CDIBitmap::CDIBitmap (long width, long height)
:  _width(width)
, _height(height)
{
   _bitmap = new CBitmap(width, height);
}

void CDIBitmap::draw (CDrawContext *context, CRect &rect, const CPoint &offset)
{
   //context->clearRect(rect);
   _bitmap->draw(context, rect, offset);
}

CDIBitmap::~CDIBitmap ()
{
}

unsigned int RGB2BGR(unsigned int x)
{
#if MAC && !PPC
	//return ((x&0xFF00) << 16) | (x&0xFF0000) | ((x&0xFF000000) >> 16) | (x&0xFF);
	return ((x&0xFF) << 16) | (x&0xFF00) | ((x&0xFF0000) >> 16) | (x&0xFF000000);
#else
	return x;
#endif
}

void CDIBitmap::clear(unsigned int color)
{
   CRect r;
   r.left = 0;
   r.top = 0;
   r.right = _width;
   r.bottom = _height;
   fillRect(r, color);
}

void CDIBitmap::clear(CColor color)
{
   clear(ccol_to_int(color));
}

void CDIBitmap::fillRect(CRect r, unsigned int color)
{
   long xs = limit_range((int)r.left, 0, _width-1);
	long xe = limit_range((int)r.right, 0, _width);
	long ys = limit_range((int)r.top, 0, _height-1);
	long ye = limit_range((int)r.bottom, 0, _height);
	
   for (int y = ys; y < ye; y++)
   {
      for (int x = xs; x < xe; x++)
      {
         setPixel(x, y, color);
      }
   }
}

void CDIBitmap::setPixel(int x, int y, unsigned int color)
{
   if (_bitmapAccess->setPosition(x, y))
   {
      _bitmapAccess->setValue(color);
   }
}

void CDIBitmap::begin()
{
   _bitmapAccess = CBitmapPixelAccess::create(_bitmap);
}

void CDIBitmap::commit()
{
   _bitmapAccess = nullptr;
}

unsigned int CDIBitmap::ccol_to_int(CColor color)
{
	rgbpixel c;	

	c.ch.r = color.red;
	c.ch.g = color.green;
	c.ch.b = color.blue;
	c.ch.a = color.alpha;

	return c.rgba;
}

CColor CDIBitmap::int_to_ccol(unsigned int col)
{
	rgbpixel c;	
	c.rgba = col;

   CColor color;
	color.red = c.ch.r;
	color.green = c.ch.g;
	color.blue = c.ch.b;
	color.alpha = 255; //c.ch.a;

	return color;
}