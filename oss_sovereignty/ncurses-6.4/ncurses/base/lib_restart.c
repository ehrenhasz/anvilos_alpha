 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_restart.c,v 1.17 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
NCURSES_SP_NAME(restartterm) (NCURSES_SP_DCLx
			      NCURSES_CONST char *termp,
			      int filenum,
			      int *errret)
{
    int result;
#ifdef USE_TERM_DRIVER
    TERMINAL *new_term = 0;
#endif

    START_TRACE();
    T((T_CALLED("restartterm(%p,%s,%d,%p)"),
       (void *) SP_PARM,
       termp,
       filenum,
       (void *) errret));

    if (TINFO_SETUP_TERM(&new_term, termp, filenum, errret, FALSE) != OK) {
	result = ERR;
    } else if (SP_PARM != 0) {
	int saveecho = SP_PARM->_echo;
	int savecbreak = SP_PARM->_cbreak;
	int saveraw = SP_PARM->_raw;
	int savenl = SP_PARM->_nl;

#ifdef USE_TERM_DRIVER
	SP_PARM->_term = new_term;
#endif
	if (saveecho) {
	    NCURSES_SP_NAME(echo) (NCURSES_SP_ARG);
	} else {
	    NCURSES_SP_NAME(noecho) (NCURSES_SP_ARG);
	}

	if (savecbreak) {
	    NCURSES_SP_NAME(cbreak) (NCURSES_SP_ARG);
	    NCURSES_SP_NAME(noraw) (NCURSES_SP_ARG);
	} else if (saveraw) {
	    NCURSES_SP_NAME(nocbreak) (NCURSES_SP_ARG);
	    NCURSES_SP_NAME(raw) (NCURSES_SP_ARG);
	} else {
	    NCURSES_SP_NAME(nocbreak) (NCURSES_SP_ARG);
	    NCURSES_SP_NAME(noraw) (NCURSES_SP_ARG);
	}
	if (savenl) {
	    NCURSES_SP_NAME(nl) (NCURSES_SP_ARG);
	} else {
	    NCURSES_SP_NAME(nonl) (NCURSES_SP_ARG);
	}

	NCURSES_SP_NAME(reset_prog_mode) (NCURSES_SP_ARG);

#if USE_SIZECHANGE
	_nc_update_screensize(SP_PARM);
#endif

	result = OK;
    } else {
	result = ERR;
    }
    returnCode(result);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
restartterm(NCURSES_CONST char *termp, int filenum, int *errret)
{
    START_TRACE();
    return NCURSES_SP_NAME(restartterm) (CURRENT_SCREEN, termp, filenum, errret);
}
#endif
