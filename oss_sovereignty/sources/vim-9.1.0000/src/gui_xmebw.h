


#ifndef EnhancedB_H
#define EnhancedB_H



#ifndef XmNshift
# define XmNshift		"shift"
#endif
#ifndef XmCShift
# define XmCShift		"Shift"
#endif

#ifndef XmNlabelLocation
# define XmNlabelLocation	"labelLocation"
#endif
#ifndef XmCLocation
# define XmCLocation		"Location"
#endif

#ifndef XmNpixmapData
# define XmNpixmapData		"pixmapData"
#endif

#ifndef XmNpixmapFile
# define XmNpixmapFile		"pixmapFile"
#endif


#ifdef HAVE_XM_JOINSIDET_H
# include <Xm/JoinSideT.h>
#else
# define XmLEFT	    1
# define XmRIGHT    2
# define XmTOP	    3
# define XmBOTTOM   4
#endif

#define XmIsEnhancedButton(w) XtIsSubclass(w, xmEnhancedButtonWidgetClass)


extern Widget XgCreateEPushButtonWidget(Widget, char *, ArgList, Cardinal);

extern WidgetClass xmEnhancedButtonWidgetClass;
typedef struct _XmEnhancedButtonClassRec *XmEnhancedButtonWidgetClass;
typedef struct _XmEnhancedButtonRec *XmEnhancedButtonWidget;

#endif
