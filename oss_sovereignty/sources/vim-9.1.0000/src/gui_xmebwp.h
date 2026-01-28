


#ifndef EnhancedBP_H
#define EnhancedBP_H

#include <Xm/PushBP.h>

#include "gui_xmebw.h"



typedef struct _XmEnhancedButtonClassPart
{
    Pixmap stipple_bitmap;
} XmEnhancedButtonClassPart;


typedef struct
{
    CoreClassPart core_class;
    XmPrimitiveClassPart primitive_class;
    XmLabelClassPart label_class;
    XmPushButtonClassPart pushbutton_class;
    XmEnhancedButtonClassPart enhancedbutton_class;
} XmEnhancedButtonClassRec;


extern XmEnhancedButtonClassRec xmEnhancedButtonClassRec;


typedef struct _XmEnhancedButtonPart
{
    
    String pixmap_data;
    String pixmap_file;
    Dimension spacing;
    int label_location;

    
    int pixmap_depth;
    Dimension pixmap_width;
    Dimension pixmap_height;
    Pixmap normal_pixmap;
    Pixmap armed_pixmap;
    Pixmap insensitive_pixmap;
    Pixmap highlight_pixmap;

    int doing_setvalues;
    int doing_destroy;
} XmEnhancedButtonPart;



typedef struct _XmEnhancedButtonRec
{
    CorePart core;
    XmPrimitivePart primitive;
    XmLabelPart label;
    XmPushButtonPart pushbutton;
    XmEnhancedButtonPart enhancedbutton;
} XmEnhancedButtonRec;

#endif
