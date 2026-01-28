




#ifndef _SIMX_H
#define _SIMX_H

#ifdef FOR_MSW

#include "windows.h"			




#if __STDC__ || defined(__cplusplus) || defined(c_plusplus)
 
#define FUNC(f, t, p) extern t f p
#define LFUNC(f, t, p) static t f p
#else 
#define FUNC(f, t, p) extern t f()
#define LFUNC(f, t, p) static t f()
#endif


FUNC(boundCheckingMalloc, void *, (long s));
FUNC(boundCheckingCalloc, void *, (long num, long s));
FUNC(boundCheckingRealloc, void *, (void *p, long s));



typedef HDC Display;			
typedef void *Screen;			
typedef void *Visual;			

typedef void *Colormap;			

typedef COLORREF Pixel;

#define PIXEL_ALREADY_TYPEDEFED		

typedef struct {
    Pixel pixel;
    BYTE red, green, blue;
}      XColor;

typedef struct {
    HBITMAP bitmap;
    unsigned int width;
    unsigned int height;
    unsigned int depth;
}      XImage;

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif



    FUNC(XDefaultVisual, Visual *, (Display *display, Screen *screen));
    FUNC(XDefaultScreen, Screen *, (Display *d));
    FUNC(XDefaultColormap, Colormap *, (Display *display, Screen *screen));
    FUNC(XDefaultDepth, int, (Display *d, Screen *s));


    FUNC(XParseColor, int, (Display *, Colormap *, char *, XColor *));
    FUNC(XAllocColor, int, (Display *, Colormap *, XColor *));
    FUNC(XQueryColors, void, (Display *display, Colormap *colormap,
			      XColor *xcolors, int ncolors));
    FUNC(XFreeColors, int, (Display *d, Colormap cmap,
			    unsigned long pixels[],
			    int npixels, unsigned long planes));

    FUNC(XCreateImage, XImage *, (Display *, Visual *, int depth, int format,
				  int x, int y, int width, int height,
				  int pad, int foo));


    FUNC(XDestroyImage, void  , (XImage *));

    FUNC(XImageFree, void, (XImage *));
#if defined(__cplusplus) || defined(c_plusplus)
} 
#endif 

#define ZPixmap 1			
#define XYBitmap 1			

#ifndef True
#define True 1
#define False 0
#endif
#ifndef Bool
typedef BOOL Bool;		
#endif

#undef LFUNC
#undef FUNC

#endif 

#endif 
