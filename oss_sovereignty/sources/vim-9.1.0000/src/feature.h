







#if defined(FEAT_SMALL)
# undef FEAT_SMALL
# if !defined(FEAT_TINY)
#  define FEAT_TINY
# endif
#endif


#if defined(FEAT_BIG)
# undef FEAT_BIG
# if !defined(FEAT_NORMAL)
#  define FEAT_NORMAL
# endif
#endif


#if !defined(FEAT_TINY) && !defined(FEAT_NORMAL) && !defined(FEAT_HUGE)



#endif


#if !defined(FEAT_TINY) && !defined(FEAT_NORMAL) && !defined(FEAT_HUGE)
# if defined(UNIX) || defined(MSWIN) || defined(MACOS_X)
#  define FEAT_HUGE
# else
#  define FEAT_NORMAL
# endif
#endif


#ifdef FEAT_HUGE
# define FEAT_NORMAL
#endif
#ifdef FEAT_NORMAL
# define FEAT_TINY
#endif






#define MAX_MSG_HIST_LEN 200


#ifdef FEAT_NORMAL
# define FEAT_FOLDING
#endif


#ifdef FEAT_NORMAL
# define FEAT_DIGRAPHS
#endif


#ifdef FEAT_HUGE
# define FEAT_LANGMAP
#endif


#ifdef FEAT_HUGE
# define FEAT_KEYMAP
#endif

#ifdef FEAT_NORMAL
# define VIM_BACKTICK		
#endif


#ifdef FEAT_NORMAL
# define FEAT_LINEBREAK
#endif


#ifdef FEAT_NORMAL
# define FEAT_SEARCH_EXTRA
#endif


#ifdef FEAT_NORMAL
# define FEAT_QUICKFIX
#endif


#ifdef FEAT_NORMAL
# define FEAT_FIND_ID
#endif


#if defined(FEAT_HUGE) && !defined(DISABLE_RIGHTLEFT)
# define FEAT_RIGHTLEFT
#endif


#if defined(FEAT_HUGE) && !defined(DISABLE_ARABIC)
# define FEAT_ARABIC
#endif
#ifdef FEAT_ARABIC
# ifndef FEAT_RIGHTLEFT
#   define FEAT_RIGHTLEFT
# endif
#endif


#ifdef FEAT_HUGE
# define FEAT_EMACS_TAGS
#endif


#if defined(UNIX) && defined(FEAT_HUGE) && !defined(FEAT_CSCOPE) && !defined(MACOS_X)
# define FEAT_CSCOPE
#endif


#ifdef FEAT_NORMAL
# define FEAT_EVAL
#endif

#ifdef FEAT_EVAL
# define HAVE_SANDBOX
#endif


#if defined(FEAT_HUGE) \
	&& defined(FEAT_EVAL) \
	&& ((defined(HAVE_GETTIMEOFDAY) && defined(HAVE_SYS_TIME_H)) \
		|| defined(MSWIN))
# define FEAT_PROFILE
#endif


#if defined(FEAT_NORMAL) \
	&& defined(FEAT_EVAL) \
	&& ((defined(HAVE_GETTIMEOFDAY) && defined(HAVE_SYS_TIME_H) \
		&& (!defined(MACOS_X) || defined(HAVE_DISPATCH_DISPATCH_H))) \
	    || defined(MSWIN))
# define FEAT_RELTIME
#endif


#if defined(FEAT_RELTIME) && (defined(UNIX) || defined(MSWIN) || defined(VMS))
# define FEAT_TIMERS
#endif


#if defined(FEAT_EVAL)
# define FEAT_COMPL_FUNC
#endif


#if defined(FEAT_NORMAL) && (defined(MSWIN) || defined(FEAT_EVAL)) \
	&& !defined(AMIGA)
# define FEAT_PRINTER
#endif
#if defined(FEAT_PRINTER) && ((defined(MSWIN) && defined(MSWINPS)) \
	|| (!defined(MSWIN) && defined(FEAT_EVAL)))
# define FEAT_POSTSCRIPT
#endif


#if defined(FEAT_NORMAL) && !defined(FEAT_DIFF)
# define FEAT_DIFF
#endif


#ifdef FEAT_NORMAL
# define FEAT_STL_OPT
#endif


#ifdef FEAT_NORMAL
# define FEAT_BYTEOFF
#endif


#ifdef FEAT_NORMAL
# define FEAT_VIMINFO


#endif


#if defined(FEAT_NORMAL) || defined(PROTO)
# define FEAT_SYN_HL
#endif


#if defined(FEAT_NORMAL) && defined(FEAT_SYN_HL)
# define FEAT_CONCEAL
#endif


#if (defined(FEAT_NORMAL) || defined(PROTO))
# define FEAT_SPELL
#endif


#if defined(FEAT_NORMAL) && !defined(FEAT_CRYPT) || defined(PROTO)
# define FEAT_CRYPT
#endif


#if defined(HAVE_SODIUM) && defined(FEAT_CRYPT)
# define FEAT_SODIUM
#endif


#if defined(FEAT_EVAL)
# define FEAT_SESSION
#endif


#ifdef FEAT_NORMAL
# define FEAT_MULTI_LANG
#endif
#if defined(HAVE_GETTEXT) && defined(FEAT_MULTI_LANG) \
	&& (defined(HAVE_LOCALE_H) || defined(X_LOCALE))
# define FEAT_GETTEXT
#endif


#if defined(FEAT_GUI_MSWIN) && !defined(FEAT_MBYTE_IME)

#endif

#if defined(FEAT_HUGE) && defined(FEAT_GUI_HAIKU) && !defined(FEAT_MBYTE_IME)
# define FEAT_MBYTE_IME
#endif


#if (defined(HAVE_ICONV_H) && defined(HAVE_ICONV)) || defined(DYNAMIC_ICONV)
# define USE_ICONV
#endif


#ifndef FEAT_XIM

#endif

#if defined(FEAT_XIM) && defined(FEAT_GUI_GTK)
# define USE_XIM 1		
#endif

#if defined(FEAT_XIM)

					
#endif


#ifndef FEAT_XFONTSET
# if defined(HAVE_X11) && !defined(FEAT_GUI_GTK)
#  define FEAT_XFONTSET
# else

# endif
#endif



#if defined(HAVE_DLOPEN) && defined(HAVE_DLSYM)
# define USE_DLOPEN
#endif
#if defined(FEAT_EVAL) && (defined(MSWIN) || ((defined(UNIX) || defined(VMS)) \
	&& (defined(USE_DLOPEN) || defined(HAVE_SHL_LOAD))))
# define FEAT_LIBCALL
#endif


#ifdef FEAT_NORMAL
# define FEAT_MENU
# ifdef FEAT_GUI_MSWIN
#  define FEAT_TEAROFF
# endif
#endif


#if defined(FEAT_MENU) && !defined(ALWAYS_USE_GUI)
# define FEAT_TERM_POPUP_MENU
#endif


#if !defined(FEAT_SOUND) && defined(HAVE_CANBERRA)
# define FEAT_SOUND
#endif
#if defined(FEAT_SOUND) && defined(HAVE_CANBERRA)
# define FEAT_SOUND_CANBERRA
#endif


#if (defined(HAVE_XM_XPMP_H) && defined(FEAT_GUI_MOTIF)) \
		|| defined(HAVE_X11_XPM_H)
# define HAVE_XPM 1
#endif


#if defined(FEAT_NORMAL) && defined(FEAT_MENU) \
	&& (defined(FEAT_GUI_GTK) \
		|| defined(FEAT_GUI_MSWIN) \
		|| (defined(FEAT_GUI_MOTIF) && defined(HAVE_XPM)) \
		|| defined(FEAT_GUI_PHOTON) \
		|| defined(FEAT_GUI_HAIKU))

# define FEAT_TOOLBAR
#endif


#if defined(FEAT_TOOLBAR) && !defined(FEAT_MENU)
# define FEAT_MENU
#endif


#if defined(FEAT_GUI_GTK) && defined(USE_GTK3)
# define FEAT_GUI_DARKTHEME
#endif


#if defined(FEAT_NORMAL) \
    && (defined(FEAT_GUI_GTK) \
	|| (defined(FEAT_GUI_MOTIF) && defined(HAVE_XM_NOTEBOOK_H)) \
	|| defined(FEAT_GUI_HAIKU) \
	|| defined(FEAT_GUI_MSWIN))
# define FEAT_GUI_TABLINE
#endif


#if defined(FEAT_NORMAL)
# define FEAT_BROWSE_CMD
# if defined(FEAT_GUI_MSWIN) || defined(FEAT_GUI_MOTIF) \
	|| defined(FEAT_GUI_GTK) || defined(FEAT_GUI_HAIKU) || defined(FEAT_GUI_PHOTON)
#  define FEAT_BROWSE
# endif
#endif


#if defined(FEAT_GUI_MSWIN) && !defined(VIMDLL)
# define ALWAYS_USE_GUI
#endif


#ifdef FEAT_NORMAL
# if (defined(FEAT_GUI_MOTIF) && defined(HAVE_X11_XPM_H)) \
	|| defined(FEAT_GUI_GTK) \
	|| defined(FEAT_GUI_PHOTON) \
	|| defined(FEAT_GUI_HAIKU) \
	|| defined(FEAT_GUI_MSWIN)
#  define FEAT_CON_DIALOG
#  define FEAT_GUI_DIALOG
# else
#  define FEAT_CON_DIALOG
# endif
#endif
#if !defined(FEAT_GUI_DIALOG) && (defined(FEAT_GUI_MOTIF) \
	|| defined(FEAT_GUI_GTK) \
	|| defined(FEAT_GUI_MSWIN))

# define FEAT_GUI_DIALOG
#endif
#if defined(FEAT_GUI_DIALOG) && \
	(defined(FEAT_GUI_MOTIF) \
	 || defined(FEAT_GUI_GTK) || defined(FEAT_GUI_MSWIN) \
	 || defined(FEAT_GUI_PHOTON) \
	 || defined(FEAT_GUI_HAIKU))
# define FEAT_GUI_TEXTDIALOG
# ifndef ALWAYS_USE_GUI
#  define FEAT_CON_DIALOG
# endif
#endif


#if (defined(FEAT_NORMAL) && defined(FEAT_SYN_HL)) && !defined(ALWAYS_USE_GUI)
# define FEAT_TERMGUICOLORS
#endif


#ifdef FEAT_HUGE
# define FEAT_VARTABS
#endif




#ifndef VMS
# define FEAT_WRITEBACKUP
#endif








#if defined(FEAT_NORMAL) \
	&& ((defined(HAVE_GETTIMEOFDAY) && defined(HAVE_SYS_TIME_H)) \
		|| defined(MSWIN))
# define STARTUPTIME 1
#endif














#define SESSION_FILE	"Session.vim"































#ifndef FILETYPE_FILE
# define FILETYPE_FILE		"filetype.vim"
#endif
#ifndef FTPLUGIN_FILE
# define FTPLUGIN_FILE		"ftplugin.vim"
#endif
#ifndef INDENT_FILE
# define INDENT_FILE		"indent.vim"
#endif
#ifndef FTOFF_FILE
# define FTOFF_FILE		"ftoff.vim"
#endif
#ifndef FTPLUGOF_FILE
# define FTPLUGOF_FILE		"ftplugof.vim"
#endif
#ifndef INDOFF_FILE
# define INDOFF_FILE		"indoff.vim"
#endif





#ifndef SYS_OPTWIN_FILE
# define SYS_OPTWIN_FILE	"$VIMRUNTIME/optwin.vim"
#endif





#ifndef RUNTIME_DIRNAME
# define RUNTIME_DIRNAME "runtime"
#endif
















#if defined(FEAT_NORMAL) || defined(FEAT_GUI_MOTIF)
# define WANT_X11
#endif


#if defined(HAVE_X11) && defined(WANT_X11) && defined(HAVE_X11_SM_SMLIB_H)
# define USE_XSMP
#endif
#if defined(USE_XSMP_INTERACT) && !defined(USE_XSMP)
# undef USE_XSMP_INTERACT
#endif



#if defined(UNIX) || defined(VMS)
# define FEAT_MOUSE_XTERM
# ifdef FEAT_NORMAL
#  define FEAT_MOUSE_NET
#  define FEAT_MOUSE_DEC
#  define FEAT_MOUSE_URXVT
# endif
#endif
#if defined(MSWIN)
# define DOS_MOUSE
#endif
#if defined(__QNX__)
# define FEAT_MOUSE_PTERM
#endif


#if defined(FEAT_NORMAL) && defined(HAVE_GPM)
# define FEAT_MOUSE_GPM

# ifndef DYNAMIC_GPM

# endif
#endif

#if defined(FEAT_NORMAL) && defined(HAVE_SYSMOUSE)
# define FEAT_SYSMOUSE
#endif


#if defined(FEAT_MOUSE_URXVT) && !defined(FEAT_MOUSE_XTERM)
# define FEAT_MOUSE_XTERM
#endif



#ifdef FEAT_CYGWIN_WIN32_CLIPBOARD
# define FEAT_CLIPBOARD
#endif

#ifdef FEAT_GUI
# ifndef FEAT_CLIPBOARD
#  define FEAT_CLIPBOARD
# endif
#endif

#if defined(FEAT_NORMAL) \
	&& (defined(UNIX) || defined(VMS)) \
	&& defined(WANT_X11) && defined(HAVE_X11)
# define FEAT_XCLIPBOARD
# ifndef FEAT_CLIPBOARD
#  define FEAT_CLIPBOARD
# endif
#endif


#if defined(FEAT_CLIPBOARD) && defined(FEAT_GUI_GTK)
# define FEAT_DND
#endif

#if defined(FEAT_GUI_MSWIN)
# define MSWIN_FIND_REPLACE	
# define MSWIN_FR_BUFSIZE 256
#endif

#if defined(FEAT_GUI_GTK) || defined(FEAT_GUI_MOTIF) \
	|| defined(MSWIN_FIND_REPLACE)
# define FIND_REPLACE_DIALOG 1
#endif


#if (defined(MSWIN) || defined(FEAT_XCLIPBOARD)) && defined(FEAT_EVAL)
# define FEAT_CLIENTSERVER
#endif


#if defined(FEAT_CLIENTSERVER) && !defined(FEAT_AUTOSERVERNAME)
# ifdef MSWIN
    
#  define FEAT_AUTOSERVERNAME
# else
    

# endif
#endif


#if defined(HAVE_TGETENT)
# define FEAT_TERMRESPONSE
#endif


#ifdef FEAT_NORMAL

# if defined(MSWIN) && (!defined(FEAT_GUI_MSWIN) || defined(VIMDLL))
#  define MCH_CURSOR_SHAPE
# endif
# if defined(FEAT_GUI_MSWIN) || defined(FEAT_GUI_MOTIF) \
	|| defined(FEAT_GUI_GTK) \
	|| defined(FEAT_GUI_PHOTON)
#  define FEAT_MOUSESHAPE
# endif
#endif



#if defined(FEAT_GUI) \
	    || defined(MCH_CURSOR_SHAPE) \
	    || defined(FEAT_MOUSESHAPE) \
	    || defined(FEAT_CONCEAL) \
	    || (defined(UNIX) && defined(FEAT_NORMAL))
# define CURSOR_SHAPE
#endif

#if defined(FEAT_MZSCHEME) && (defined(FEAT_GUI_MSWIN) || defined(FEAT_GUI_GTK)    \
	|| defined(FEAT_GUI_MOTIF))
# define MZSCHEME_GUI_THREADS
#endif


#if defined(AMIGA) && !defined(NO_ARP) && !defined(__amigaos4__) \
	&& !defined(__MORPHOS__) && !defined(__AROS__)
# define FEAT_ARP
#endif








#if !defined(FEAT_EVAL) && defined(FEAT_NETBEANS_INTG)
# undef FEAT_NETBEANS_INTG
#endif


#if !defined(FEAT_EVAL) && defined(FEAT_JOB_CHANNEL)
# undef FEAT_JOB_CHANNEL
#endif


#if defined(FEAT_TERMINAL) && !defined(FEAT_JOB_CHANNEL)
# undef FEAT_TERMINAL
#endif
#if defined(FEAT_TERMINAL) && !defined(CURSOR_SHAPE)
# define CURSOR_SHAPE
#endif
#if defined(FEAT_TERMINAL) && !defined(FEAT_SYN_HL)

# define FEAT_SYN_HL
#endif


#if defined(FEAT_TERMINAL)
# define FEAT_AUTOSHELLDIR
#endif

#if defined(FEAT_EVAL) && defined(FEAT_SYN_HL)
# define FEAT_PROP_POPUP
#endif


#if defined(FEAT_PROP_POPUP) && defined(FEAT_TIMERS)
# define HAS_MESSAGE_WINDOW
#endif

#if defined(FEAT_SYN_HL) && defined(FEAT_RELTIME)

# define SYN_TIME_LIMIT 1
#endif



#if defined(FEAT_NORMAL) || defined(FEAT_NETBEANS_INTG) || defined(FEAT_PROP_POPUP)
# define FEAT_SIGNS
# if (defined(FEAT_GUI_MOTIF) && defined(HAVE_X11_XPM_H)) \
	|| defined(FEAT_GUI_GTK) \
	|| (defined(MSWIN) && defined(FEAT_GUI))
#  define FEAT_SIGN_ICONS
# endif
#endif


#if (defined(FEAT_GUI_MOTIF) \
	|| defined(FEAT_GUI_GTK) || defined(FEAT_GUI_MSWIN)) \
	&& (   ((defined(FEAT_TOOLBAR) || defined(FEAT_GUI_TABLINE)) \
		&& !defined(FEAT_GUI_GTK) && !defined(FEAT_GUI_MSWIN)) \
	    || defined(FEAT_NETBEANS_INTG) || defined(FEAT_EVAL))
# define FEAT_BEVAL_GUI
# if !defined(FEAT_XFONTSET) && !defined(FEAT_GUI_GTK) \
	&& !defined(FEAT_GUI_MSWIN)
#  define FEAT_XFONTSET
# endif
#endif

#if defined(FEAT_BEVAL_GUI) && defined(FEAT_GUI_MOTIF)
# define FEAT_BEVAL_TIP		
#endif


#if defined(FEAT_HUGE) && defined(FEAT_TIMERS) && \
	(defined(UNIX) || defined(VMS) || \
	 (defined(MSWIN) && (!defined(FEAT_GUI_MSWIN) || defined(VIMDLL))))
# define FEAT_BEVAL_TERM
#endif

#if defined(FEAT_BEVAL_GUI) || defined(FEAT_BEVAL_TERM)
# define FEAT_BEVAL
#endif


#if defined(FEAT_GUI_MOTIF)
# define FEAT_GUI_X11
#endif

#if defined(FEAT_NETBEANS_INTG)

# if !defined(FEAT_MENU)
#  define FEAT_MENU
# endif
#endif


#if defined(FEAT_NETBEANS_INTG) || defined(FEAT_NORMAL)
# define FEAT_AUTOCHDIR
#endif


#ifdef FEAT_NORMAL
# define FEAT_PERSISTENT_UNDO
#endif


#if (defined(UNIX) && !defined(USE_SYSTEM)) \
	    || (defined(MSWIN) && defined(FEAT_GUI_MSWIN))
# define FEAT_FILTERPIPE
#endif


#if (!defined(FEAT_GUI) || defined(VIMDLL)) && defined(MSWIN)
# define FEAT_VTP
#endif

#if defined(DYNAMIC_PERL) \
	|| defined(DYNAMIC_PYTHON) || defined(DYNAMIC_PYTHON3) \
	|| defined(DYNAMIC_RUBY) \
	|| defined(DYNAMIC_TCL) \
	|| defined(DYNAMIC_ICONV) \
	|| defined(DYNAMIC_GETTEXT) \
	|| defined(DYNAMIC_MZSCHEME) \
	|| defined(DYNAMIC_LUA) \
	|| defined(FEAT_TERMINAL)
# define USING_LOAD_LIBRARY
#endif



#if defined(FEAT_NORMAL) && defined(HAVE_XATTR) && !defined(MACOS_X)
# define FEAT_XATTR
#endif
