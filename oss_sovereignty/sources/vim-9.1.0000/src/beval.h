

#if !defined(BEVAL__H) && (defined(FEAT_BEVAL) || defined(PROTO))
#define BEVAL__H

#ifdef FEAT_GUI_GTK
# ifdef USE_GTK3
#  include <gtk/gtk.h>
# else
#  include <gtk/gtkwidget.h>
# endif
#else
# if defined(FEAT_GUI_X11)
#  include <X11/Intrinsic.h>
# endif
#endif

typedef enum
{
    ShS_NEUTRAL,			
    ShS_PENDING,			
    ShS_UPDATE_PENDING,			
    ShS_SHOWING				
} BeState;

typedef struct BalloonEvalStruct
{
#ifdef FEAT_BEVAL_GUI
# ifdef FEAT_GUI_GTK
    GtkWidget		*target;	
    GtkWidget		*balloonShell;
    GtkWidget		*balloonLabel;
    unsigned int	timerID;	
    BeState		showState;	
    int			x;
    int			y;
    unsigned int	state;		
# else
#  if !defined(FEAT_GUI_MSWIN)
    Widget		target;		
    Widget		balloonShell;
    Widget		balloonLabel;
    XtIntervalId	timerID;	
    BeState		showState;	
    XtAppContext	appContext;	
    Position		x;
    Position		y;
    Position		x_root;
    Position		y_root;
    int			state;		
#  else
    HWND		target;
    HWND		balloon;
    int			x;
    int			y;
    BeState		showState;	
#  endif
# endif
# if !defined(FEAT_GUI_GTK) && !defined(FEAT_GUI_MSWIN)
    Dimension		screen_width;	
    Dimension		screen_height;	
# endif
    void		(*msgCB)(struct BalloonEvalStruct *, int);
    void		*clientData;	
#endif

    int			ts;		
#ifdef FEAT_VARTABS
    int			*vts;		
#endif
    char_u		*msg;		
#ifdef FEAT_GUI_MSWIN
    void		*tofree;
#endif
#ifdef FEAT_GUI_HAIKU
    int			x;
    int			y;
#endif
} BalloonEval;

#define EVAL_OFFSET_X 15 
#define EVAL_OFFSET_Y 10

#ifdef FEAT_BEVAL_GUI
# include "gui_beval.pro"
#endif

#endif 
