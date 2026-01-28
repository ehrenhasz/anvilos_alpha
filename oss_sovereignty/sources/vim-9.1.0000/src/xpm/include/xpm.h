







#ifndef XPM_h
#define XPM_h


#define XpmFormat 3
#define XpmVersion 4
#define XpmRevision 11
#define XpmIncludeVersion ((XpmFormat * 100 + XpmVersion) * 100 + XpmRevision)

#ifndef XPM_NUMBERS

#ifdef FOR_MSW
# define SYSV			
# include <malloc.h>
# include "simx.h"		
#define NEED_STRCASECMP		
#else 
# ifdef AMIGA
#  include "amigax.h"
# else 
#  include <X11/Xlib.h>
#  include <X11/Xutil.h>
# endif 
#endif 


#if ! defined(_XtIntrinsic_h) && ! defined(PIXEL_ALREADY_TYPEDEFED)
typedef unsigned long Pixel;	
# define PIXEL_ALREADY_TYPEDEFED
#endif


#ifndef NeedFunctionPrototypes
# if defined(__STDC__) || defined(__cplusplus) || defined(c_plusplus)
#  define NeedFunctionPrototypes 1
# else
#  define NeedFunctionPrototypes 0
# endif
#endif




#define XpmColorError    1
#define XpmSuccess       0
#define XpmOpenFailed   -1
#define XpmFileInvalid  -2
#define XpmNoMemory     -3
#define XpmColorFailed  -4

typedef struct {
    char *name;			
    char *value;		
    Pixel pixel;		
}      XpmColorSymbol;

typedef struct {
    char *name;			
    unsigned int nlines;	
    char **lines;		
}      XpmExtension;

typedef struct {
    char *string;		
    char *symbolic;		
    char *m_color;		
    char *g4_color;		
    char *g_color;		
    char *c_color;		
}      XpmColor;

typedef struct {
    unsigned int width;		
    unsigned int height;	
    unsigned int cpp;		
    unsigned int ncolors;	
    XpmColor *colorTable;	
    unsigned int *data;		
}      XpmImage;

typedef struct {
    unsigned long valuemask;	
    char *hints_cmt;		
    char *colors_cmt;		
    char *pixels_cmt;		
    unsigned int x_hotspot;	
    unsigned int y_hotspot;	
    unsigned int nextensions;	
    XpmExtension *extensions;	
}      XpmInfo;

typedef int (*XpmAllocColorFunc)(
#if NeedFunctionPrototypes
    Display*			,
    Colormap			,
    char*			,
    XColor*			,
    void*			
#endif
);

typedef int (*XpmFreeColorsFunc)(
#if NeedFunctionPrototypes
    Display*			,
    Colormap			,
    Pixel*			,
    int				,
    void*			
#endif
);

typedef struct {
    unsigned long valuemask;		

    Visual *visual;			
    Colormap colormap;			
    unsigned int depth;			
    unsigned int width;			
    unsigned int height;		
    unsigned int x_hotspot;		
    unsigned int y_hotspot;		
    unsigned int cpp;			
    Pixel *pixels;			
    unsigned int npixels;		
    XpmColorSymbol *colorsymbols;	
    unsigned int numsymbols;		
    char *rgb_fname;			
    unsigned int nextensions;		
    XpmExtension *extensions;		

    unsigned int ncolors;               
    XpmColor *colorTable;               

    char *hints_cmt;                    
    char *colors_cmt;                   
    char *pixels_cmt;                   

    unsigned int mask_pixel;            

    
    Bool exactColors;			
    unsigned int closeness;		
    unsigned int red_closeness;		
    unsigned int green_closeness;	
    unsigned int blue_closeness;	
    int color_key;			

    Pixel *alloc_pixels;		
    int nalloc_pixels;			

    Bool alloc_close_colors;    	
    int bitmap_format;			

    
    XpmAllocColorFunc alloc_color;	
    XpmFreeColorsFunc free_colors;	
    void *color_closure;		

}      XpmAttributes;


#define XpmVisual	   (1L<<0)
#define XpmColormap	   (1L<<1)
#define XpmDepth	   (1L<<2)
#define XpmSize		   (1L<<3)	
#define XpmHotspot	   (1L<<4)	
#define XpmCharsPerPixel   (1L<<5)
#define XpmColorSymbols	   (1L<<6)
#define XpmRgbFilename	   (1L<<7)

#define XpmInfos	   (1L<<8)
#define XpmReturnInfos	   XpmInfos

#define XpmReturnPixels	   (1L<<9)
#define XpmExtensions      (1L<<10)
#define XpmReturnExtensions XpmExtensions

#define XpmExactColors     (1L<<11)
#define XpmCloseness	   (1L<<12)
#define XpmRGBCloseness	   (1L<<13)
#define XpmColorKey	   (1L<<14)

#define XpmColorTable      (1L<<15)
#define XpmReturnColorTable XpmColorTable

#define XpmReturnAllocPixels (1L<<16)
#define XpmAllocCloseColors (1L<<17)
#define XpmBitmapFormat    (1L<<18)

#define XpmAllocColor      (1L<<19)
#define XpmFreeColors      (1L<<20)
#define XpmColorClosure    (1L<<21)



#define XpmComments        XpmInfos
#define XpmReturnComments  XpmComments


#ifndef FOR_MSW
#define XpmUndefPixel 0x80000000
#else

#define XpmUndefPixel 0x8000
#endif


#define XPM_MONO	2
#define XPM_GREY4	3
#define XPM_GRAY4	3
#define XPM_GREY 	4
#define XPM_GRAY 	4
#define XPM_COLOR	5



#if NeedFunctionPrototypes
#define FUNC(f, t, p) extern t f p
#define LFUNC(f, t, p) static t f p
#else
#define FUNC(f, t, p) extern t f()
#define LFUNC(f, t, p) static t f()
#endif




#ifdef __cplusplus
extern "C" {
#endif




#if !defined(FOR_MSW) && !defined(AMIGA)
    FUNC(XpmCreatePixmapFromData, int, (Display *display,
					Drawable d,
					char **data,
					Pixmap *pixmap_return,
					Pixmap *shapemask_return,
					XpmAttributes *attributes));

    FUNC(XpmCreateDataFromPixmap, int, (Display *display,
					char ***data_return,
					Pixmap pixmap,
					Pixmap shapemask,
					XpmAttributes *attributes));

    FUNC(XpmReadFileToPixmap, int, (Display *display,
				    Drawable d,
				    char *filename,
				    Pixmap *pixmap_return,
				    Pixmap *shapemask_return,
				    XpmAttributes *attributes));

    FUNC(XpmWriteFileFromPixmap, int, (Display *display,
				       char *filename,
				       Pixmap pixmap,
				       Pixmap shapemask,
				       XpmAttributes *attributes));
#endif

    FUNC(XpmCreateImageFromData, int, (Display *display,
				       char **data,
				       XImage **image_return,
				       XImage **shapemask_return,
				       XpmAttributes *attributes));

    FUNC(XpmCreateDataFromImage, int, (Display *display,
				       char ***data_return,
				       XImage *image,
				       XImage *shapeimage,
				       XpmAttributes *attributes));

    FUNC(XpmReadFileToImage, int, (Display *display,
				   char *filename,
				   XImage **image_return,
				   XImage **shapeimage_return,
				   XpmAttributes *attributes));

    FUNC(XpmWriteFileFromImage, int, (Display *display,
				      char *filename,
				      XImage *image,
				      XImage *shapeimage,
				      XpmAttributes *attributes));

    FUNC(XpmCreateImageFromBuffer, int, (Display *display,
					 char *buffer,
					 XImage **image_return,
					 XImage **shapemask_return,
					 XpmAttributes *attributes));
#if !defined(FOR_MSW) && !defined(AMIGA)
    FUNC(XpmCreatePixmapFromBuffer, int, (Display *display,
					  Drawable d,
					  char *buffer,
					  Pixmap *pixmap_return,
					  Pixmap *shapemask_return,
					  XpmAttributes *attributes));

    FUNC(XpmCreateBufferFromImage, int, (Display *display,
					 char **buffer_return,
					 XImage *image,
					 XImage *shapeimage,
					 XpmAttributes *attributes));

    FUNC(XpmCreateBufferFromPixmap, int, (Display *display,
					  char **buffer_return,
					  Pixmap pixmap,
					  Pixmap shapemask,
					  XpmAttributes *attributes));
#endif
    FUNC(XpmReadFileToBuffer, int, (char *filename, char **buffer_return));
    FUNC(XpmWriteFileFromBuffer, int, (char *filename, char *buffer));

    FUNC(XpmReadFileToData, int, (char *filename, char ***data_return));
    FUNC(XpmWriteFileFromData, int, (char *filename, char **data));

    FUNC(XpmAttributesSize, int, ());
    FUNC(XpmFreeAttributes, void, (XpmAttributes *attributes));
    FUNC(XpmFreeExtensions, void, (XpmExtension *extensions,
				   int nextensions));

    FUNC(XpmFreeXpmImage, void, (XpmImage *image));
    FUNC(XpmFreeXpmInfo, void, (XpmInfo *info));
    FUNC(XpmGetErrorString, char *, (int errcode));
    FUNC(XpmLibraryVersion, int, ());

    
    FUNC(XpmReadFileToXpmImage, int, (char *filename,
				      XpmImage *image,
				      XpmInfo *info));

    FUNC(XpmWriteFileFromXpmImage, int, (char *filename,
					 XpmImage *image,
					 XpmInfo *info));
#if !defined(FOR_MSW) && !defined(AMIGA)
    FUNC(XpmCreatePixmapFromXpmImage, int, (Display *display,
					    Drawable d,
					    XpmImage *image,
					    Pixmap *pixmap_return,
					    Pixmap *shapemask_return,
					    XpmAttributes *attributes));
#endif
    FUNC(XpmCreateImageFromXpmImage, int, (Display *display,
					   XpmImage *image,
					   XImage **image_return,
					   XImage **shapeimage_return,
					   XpmAttributes *attributes));

    FUNC(XpmCreateXpmImageFromImage, int, (Display *display,
					   XImage *image,
					   XImage *shapeimage,
					   XpmImage *xpmimage,
					   XpmAttributes *attributes));
#if !defined(FOR_MSW) && !defined(AMIGA)
    FUNC(XpmCreateXpmImageFromPixmap, int, (Display *display,
					    Pixmap pixmap,
					    Pixmap shapemask,
					    XpmImage *xpmimage,
					    XpmAttributes *attributes));
#endif
    FUNC(XpmCreateDataFromXpmImage, int, (char ***data_return,
					  XpmImage *image,
					  XpmInfo *info));

    FUNC(XpmCreateXpmImageFromData, int, (char **data,
					  XpmImage *image,
					  XpmInfo *info));

    FUNC(XpmCreateXpmImageFromBuffer, int, (char *buffer,
					    XpmImage *image,
					    XpmInfo *info));

    FUNC(XpmCreateBufferFromXpmImage, int, (char **buffer_return,
					    XpmImage *image,
					    XpmInfo *info));

    FUNC(XpmGetParseError, int, (char *filename,
				 int *linenum_return,
				 int *charnum_return));

    FUNC(XpmFree, void, (void *ptr));

#ifdef __cplusplus
} 
#endif





#define XpmPixmapColorError  XpmColorError
#define XpmPixmapSuccess     XpmSuccess
#define XpmPixmapOpenFailed  XpmOpenFailed
#define XpmPixmapFileInvalid XpmFileInvalid
#define XpmPixmapNoMemory    XpmNoMemory
#define XpmPixmapColorFailed XpmColorFailed

#define XpmReadPixmapFile(dpy, d, file, pix, mask, att) \
    XpmReadFileToPixmap(dpy, d, file, pix, mask, att)
#define XpmWritePixmapFile(dpy, file, pix, mask, att) \
    XpmWriteFileFromPixmap(dpy, file, pix, mask, att)


#define PixmapColorError  XpmColorError
#define PixmapSuccess     XpmSuccess
#define PixmapOpenFailed  XpmOpenFailed
#define PixmapFileInvalid XpmFileInvalid
#define PixmapNoMemory    XpmNoMemory
#define PixmapColorFailed XpmColorFailed

#define ColorSymbol XpmColorSymbol

#define XReadPixmapFile(dpy, d, file, pix, mask, att) \
    XpmReadFileToPixmap(dpy, d, file, pix, mask, att)
#define XWritePixmapFile(dpy, file, pix, mask, att) \
    XpmWriteFileFromPixmap(dpy, file, pix, mask, att)
#define XCreatePixmapFromData(dpy, d, data, pix, mask, att) \
    XpmCreatePixmapFromData(dpy, d, data, pix, mask, att)
#define XCreateDataFromPixmap(dpy, data, pix, mask, att) \
    XpmCreateDataFromPixmap(dpy, data, pix, mask, att)

#endif 
#endif
