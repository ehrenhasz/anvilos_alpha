 

 

 

#include <curses.priv.h>

#if HAVE_SYS_TERMIO_H
#include <sys/termio.h>		 
#endif

MODULE_ID("$Id: lib_initscr.c,v 1.48 2020/09/07 14:26:48 tom Exp $")

NCURSES_EXPORT(WINDOW *)
initscr(void)
{
    WINDOW *result;

    START_TRACE();
    T((T_CALLED("initscr()")));

    _nc_init_pthreads();
    _nc_lock_global(curses);

     
    if (!_nc_globals.init_screen) {
	const char *env;
	char *name;

	_nc_globals.init_screen = TRUE;

	env = getenv("TERM");
	(void) VALID_TERM_ENV(env, "unknown");

	if ((name = strdup(env)) == NULL) {
	    fprintf(stderr, "Error opening allocating $TERM.\n");
	    ExitProgram(EXIT_FAILURE);
	}
#ifdef __CYGWIN__
	 
	if (NC_ISATTY(fileno(stdout))) {
	    FILE *fp = fopen("/dev/tty", "w");
	    if (fp != 0 && NC_ISATTY(fileno(fp))) {
		fclose(stdout);
		dup2(fileno(fp), STDOUT_FILENO);
		stdout = fdopen(STDOUT_FILENO, "w");
	    }
	}
#endif
	if (newterm(name, stdout, stdin) == 0) {
	    fprintf(stderr, "Error opening terminal: %s.\n", name);
	    ExitProgram(EXIT_FAILURE);
	}

	 
#if NCURSES_SP_FUNCS
	NCURSES_SP_NAME(def_prog_mode) (CURRENT_SCREEN);
#else
	def_prog_mode();
#endif
	free(name);
    }
    result = stdscr;
    _nc_unlock_global(curses);

    returnWin(result);
}
