 

 

 

#include <curses.priv.h>

#ifndef CUR
#define CUR SP_TERMTYPE
#endif

#include <tic.h>

MODULE_ID("$Id: lib_newterm.c,v 1.104 2022/07/09 18:58:58 tom Exp $")

#ifdef USE_TERM_DRIVER
#define NumLabels      InfoOf(SP_PARM).numlabels
#else
#define NumLabels      num_labels
#endif

#ifndef ONLCR			 
#define ONLCR 0
#endif

 
static NCURSES_INLINE int
_nc_initscr(NCURSES_SP_DCL0)
{
    int result = ERR;
    TERMINAL *term = TerminalOf(SP_PARM);

     
     
    T((T_CALLED("_nc_initscr(%p) ->term %p"), (void *) SP_PARM, (void *) term));
    if (NCURSES_SP_NAME(cbreak) (NCURSES_SP_ARG) == OK) {
	TTY buf;

	buf = term->Nttyb;
#ifdef TERMIOS
	buf.c_lflag &= (unsigned) ~(ECHO | ECHONL);
	buf.c_iflag &= (unsigned) ~(ICRNL | INLCR | IGNCR);
	buf.c_oflag &= (unsigned) ~(ONLCR);
#elif HAVE_SGTTY_H
	buf.sg_flags &= ~(ECHO | CRMOD);
#elif defined(EXP_WIN32_DRIVER)
	buf.dwFlagIn = CONMODE_IN_DEFAULT;
	buf.dwFlagOut = CONMODE_OUT_DEFAULT | VT_FLAG_OUT;
	if (WINCONSOLE.isTermInfoConsole) {
	    buf.dwFlagIn |= VT_FLAG_IN;
	}
#else
	memset(&buf, 0, sizeof(buf));
#endif
	result = NCURSES_SP_NAME(_nc_set_tty_mode) (NCURSES_SP_ARGx &buf);
	if (result == OK)
	    term->Nttyb = buf;
    }
    returnCode(result);
}

 
NCURSES_EXPORT(void)
NCURSES_SP_NAME(filter) (NCURSES_SP_DCL0)
{
    START_TRACE();
    T((T_CALLED("filter(%p)"), (void *) SP_PARM));
#if NCURSES_SP_FUNCS
    if (IsPreScreen(SP_PARM)) {
	SP_PARM->_filtered = TRUE;
    }
#else
    _nc_prescreen.filter_mode = TRUE;
#endif
    returnVoid;
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(void)
filter(void)
{
    START_TRACE();
    T((T_CALLED("filter()")));
    _nc_prescreen.filter_mode = TRUE;
    returnVoid;
}
#endif

#if NCURSES_EXT_FUNCS
 
NCURSES_EXPORT(void)
NCURSES_SP_NAME(nofilter) (NCURSES_SP_DCL0)
{
    START_TRACE();
    T((T_CALLED("nofilter(%p)"), (void *) SP_PARM));
#if NCURSES_SP_FUNCS
    if (IsPreScreen(SP_PARM)) {
	SP_PARM->_filtered = FALSE;
    }
#else
    _nc_prescreen.filter_mode = FALSE;
#endif
    returnVoid;
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(void)
nofilter(void)
{
    START_TRACE();
    T((T_CALLED("nofilter()")));
    _nc_prescreen.filter_mode = FALSE;
    returnVoid;
}
#endif
#endif  

NCURSES_EXPORT(SCREEN *)
NCURSES_SP_NAME(newterm) (NCURSES_SP_DCLx
			  const char *name,
			  FILE *ofp,
			  FILE *ifp)
{
    int errret;
    SCREEN *result = 0;
    SCREEN *current;
    TERMINAL *its_term;
    FILE *_ofp = ofp ? ofp : stdout;
    FILE *_ifp = ifp ? ifp : stdin;
    TERMINAL *new_term = 0;

    START_TRACE();
    T((T_CALLED("newterm(%p, \"%s\", %p,%p)"),
       (void *) SP_PARM,
       (name ? name : ""),
       (void *) ofp,
       (void *) ifp));

#if NCURSES_SP_FUNCS
    assert(SP_PARM != 0);
    if (SP_PARM == 0)
	returnSP(SP_PARM);
#endif

    _nc_init_pthreads();
    _nc_lock_global(curses);

    current = CURRENT_SCREEN;
    its_term = (current ? current->_term : 0);

#if defined(EXP_WIN32_DRIVER)
    _setmode(fileno(_ifp), _O_BINARY);
    _setmode(fileno(_ofp), _O_BINARY);
#endif

    INIT_TERM_DRIVER();
     
    if (
	   TINFO_SETUP_TERM(&new_term, name,
			    fileno(_ofp), &errret, FALSE) != ERR) {
	int slk_format;
	int filter_mode;

	_nc_set_screen(0);
#ifdef USE_TERM_DRIVER
	assert(new_term != 0);
#endif

#if NCURSES_SP_FUNCS
	slk_format = SP_PARM->slk_format;
	filter_mode = SP_PARM->_filtered;
#else
	slk_format = _nc_globals.slk_format;
	filter_mode = _nc_prescreen.filter_mode;
#endif

	 
	if (NCURSES_SP_NAME(_nc_setupscreen) (
#if NCURSES_SP_FUNCS
						 &SP_PARM,
#endif
						 *(ptrLines(SP_PARM)),
						 *(ptrCols(SP_PARM)),
						 _ofp,
						 filter_mode,
						 slk_format) == ERR) {
	    _nc_set_screen(current);
	    result = 0;
	} else {
	    int value;
	    int cols;

#ifdef USE_TERM_DRIVER
	    TERMINAL_CONTROL_BLOCK *TCB;
#elif !NCURSES_SP_FUNCS
	    _nc_set_screen(CURRENT_SCREEN);
#endif
	    assert(SP_PARM != 0);
	    cols = *(ptrCols(SP_PARM));
#ifdef USE_TERM_DRIVER
	    _nc_set_screen(SP_PARM);
	    TCB = (TERMINAL_CONTROL_BLOCK *) new_term;
	    TCB->csp = SP_PARM;
#endif
	     
	    if (current)
		current->_term = its_term;

#ifdef USE_TERM_DRIVER
	    SP_PARM->_term = new_term;
#else
	    new_term = SP_PARM->_term;
#endif

	     
	    if ((value = _nc_getenv_num("ESCDELAY")) >= 0) {
#if NCURSES_EXT_FUNCS
		NCURSES_SP_NAME(set_escdelay) (NCURSES_SP_ARGx value);
#else
		ESCDELAY = value;
#endif
	    }

	     
	    if (slk_format && NumLabels > 0 && SLK_STDFMT(slk_format))
		_nc_slk_initialize(StdScreen(SP_PARM), cols);

	    SP_PARM->_ifd = fileno(_ifp);
	    NCURSES_SP_NAME(typeahead) (NCURSES_SP_ARGx fileno(_ifp));
#ifdef TERMIOS
	    SP_PARM->_use_meta = ((new_term->Ottyb.c_cflag & CSIZE) == CS8 &&
				  !(new_term->Ottyb.c_iflag & ISTRIP)) ||
		USE_KLIBC_KBD;
#else
	    SP_PARM->_use_meta = FALSE;
#endif
	    SP_PARM->_endwin = ewInitial;
#ifndef USE_TERM_DRIVER
	     
	    SP_PARM->_scrolling = ((scroll_forward && scroll_reverse) ||
				   ((parm_rindex ||
				     parm_insert_line ||
				     insert_line) &&
				    (parm_index ||
				     parm_delete_line ||
				     delete_line)));
#endif

	    NCURSES_SP_NAME(baudrate) (NCURSES_SP_ARG);		 

	    SP_PARM->_keytry = 0;

	     
#ifdef USE_TERM_DRIVER
	    TCBOf(SP_PARM)->drv->td_scinit(SP_PARM);
#else  
	     
#define SGR0_TEST(mode) (mode != 0) && (exit_attribute_mode == 0 || strcmp(mode, exit_attribute_mode))
	    SP_PARM->_use_rmso = SGR0_TEST(exit_standout_mode);
	    SP_PARM->_use_rmul = SGR0_TEST(exit_underline_mode);
#if USE_ITALIC
	    SP_PARM->_use_ritm = SGR0_TEST(exit_italics_mode);
#endif

	     
	    _nc_mvcur_init();

	     
	    _nc_screen_init();
#endif  

	     
	    _nc_initscr(NCURSES_SP_ARG);

	    _nc_signal_handler(TRUE);
	    result = SP_PARM;
	}
    }
    _nc_unlock_global(curses);
    returnSP(result);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(SCREEN *)
newterm(const char *name, FILE *ofp, FILE *ifp)
{
    SCREEN *rc;

    _nc_init_pthreads();
    _nc_lock_global(prescreen);
    START_TRACE();
    rc = NCURSES_SP_NAME(newterm) (CURRENT_SCREEN_PRE, name, ofp, ifp);
    _nc_forget_prescr();
    _nc_unlock_global(prescreen);

    return rc;
}
#endif
