 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: hardscroll.c,v 1.54 2020/02/02 23:34:34 tom Exp $")

#if defined(SCROLLDEBUG) || defined(HASHDEBUG)

# undef screen_lines
# define screen_lines(sp) MAXLINES
NCURSES_EXPORT_VAR (int)
  oldnums[MAXLINES];
# define OLDNUM(sp,n)	oldnums[n]
# define _tracef	printf
# undef TR
# define TR(n, a)	if (_nc_tracing & (n)) { _tracef a ; putchar('\n'); }

extern				NCURSES_EXPORT_VAR(unsigned) _nc_tracing;

#else  

 
NCURSES_EXPORT_VAR (int *)
  _nc_oldnums = 0;		 

# if USE_HASHMAP
#  define oldnums(sp)   (sp)->_oldnum_list
#  define OLDNUM(sp,n)	oldnums(sp)[n]
# else  
#  define OLDNUM(sp,n)	NewScreen(sp)->_line[n].oldindex
# endif	 

#define OLDNUM_SIZE(sp) (sp)->_oldnum_size

#endif  

NCURSES_EXPORT(void)
NCURSES_SP_NAME(_nc_scroll_optimize) (NCURSES_SP_DCL0)
 
{
    int i;
    int start, end, shift;

    TR(TRACE_ICALLS, (T_CALLED("_nc_scroll_optimize(%p)"), (void *) SP_PARM));

#if !defined(SCROLLDEBUG) && !defined(HASHDEBUG)
#if USE_HASHMAP
     
    assert(OLDNUM_SIZE(SP_PARM) >= 0);
    assert(screen_lines(SP_PARM) > 0);
    if ((oldnums(SP_PARM) == 0)
	|| (OLDNUM_SIZE(SP_PARM) < screen_lines(SP_PARM))) {
	int need_lines = ((OLDNUM_SIZE(SP_PARM) < screen_lines(SP_PARM))
			  ? screen_lines(SP_PARM)
			  : OLDNUM_SIZE(SP_PARM));
	int *new_oldnums = typeRealloc(int,
				       (size_t) need_lines,
				       oldnums(SP_PARM));
	if (!new_oldnums)
	    return;
	oldnums(SP_PARM) = new_oldnums;
	OLDNUM_SIZE(SP_PARM) = need_lines;
    }
     
    NCURSES_SP_NAME(_nc_hash_map) (NCURSES_SP_ARG);
#endif
#endif  

#ifdef TRACE
    if (USE_TRACEF(TRACE_UPDATE | TRACE_MOVE)) {
	NCURSES_SP_NAME(_nc_linedump) (NCURSES_SP_ARG);
	_nc_unlock_global(tracef);
    }
#endif  

     
    for (i = 0; i < screen_lines(SP_PARM);) {
	while (i < screen_lines(SP_PARM)
	       && (OLDNUM(SP_PARM, i) == _NEWINDEX || OLDNUM(SP_PARM, i) <= i))
	    i++;
	if (i >= screen_lines(SP_PARM))
	    break;

	shift = OLDNUM(SP_PARM, i) - i;		 
	start = i;

	i++;
	while (i < screen_lines(SP_PARM)
	       && OLDNUM(SP_PARM, i) != _NEWINDEX
	       && OLDNUM(SP_PARM, i) - i == shift)
	    i++;
	end = i - 1 + shift;

	TR(TRACE_UPDATE | TRACE_MOVE, ("scroll [%d, %d] by %d", start, end, shift));
#if !defined(SCROLLDEBUG) && !defined(HASHDEBUG)
	if (NCURSES_SP_NAME(_nc_scrolln) (NCURSES_SP_ARGx
					  shift,
					  start,
					  end,
					  screen_lines(SP_PARM) - 1) == ERR) {
	    TR(TRACE_UPDATE | TRACE_MOVE, ("unable to scroll"));
	    continue;
	}
#endif  
    }

     
    for (i = screen_lines(SP_PARM) - 1; i >= 0;) {
	while (i >= 0
	       && (OLDNUM(SP_PARM, i) == _NEWINDEX
		   || OLDNUM(SP_PARM, i) >= i)) {
	    i--;
	}
	if (i < 0)
	    break;

	shift = OLDNUM(SP_PARM, i) - i;		 
	end = i;

	i--;
	while (i >= 0
	       && OLDNUM(SP_PARM, i) != _NEWINDEX
	       && OLDNUM(SP_PARM, i) - i == shift) {
	    i--;
	}
	start = i + 1 - (-shift);

	TR(TRACE_UPDATE | TRACE_MOVE, ("scroll [%d, %d] by %d", start, end, shift));
#if !defined(SCROLLDEBUG) && !defined(HASHDEBUG)
	if (NCURSES_SP_NAME(_nc_scrolln) (NCURSES_SP_ARGx
					  shift,
					  start,
					  end,
					  screen_lines(SP_PARM) - 1) == ERR) {
	    TR(TRACE_UPDATE | TRACE_MOVE, ("unable to scroll"));
	    continue;
	}
#endif  
    }
    TR(TRACE_ICALLS, (T_RETURN("")));
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(void)
_nc_scroll_optimize(void)
{
    NCURSES_SP_NAME(_nc_scroll_optimize) (CURRENT_SCREEN);
}
#endif

#if defined(TRACE) || defined(SCROLLDEBUG) || defined(HASHDEBUG)
NCURSES_EXPORT(void)
NCURSES_SP_NAME(_nc_linedump) (NCURSES_SP_DCL0)
 
{
    char *buf = 0;
    size_t want = ((size_t) screen_lines(SP_PARM) + 1) * 4;
    (void) SP_PARM;

    if ((buf = typeMalloc(char, want)) != 0) {
	int n;

	*buf = '\0';
	for (n = 0; n < screen_lines(SP_PARM); n++)
	    _nc_SPRINTF(buf + strlen(buf),
			_nc_SLIMIT(want - strlen(buf))
			" %02d", OLDNUM(SP_PARM, n));
	TR(TRACE_UPDATE | TRACE_MOVE, ("virt %s", buf));
	free(buf);
    }
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(void)
_nc_linedump(void)
{
    NCURSES_SP_NAME(_nc_linedump) (CURRENT_SCREEN);
}
#endif

#endif  

#ifdef SCROLLDEBUG

int
main(int argc GCC_UNUSED, char *argv[]GCC_UNUSED)
{
    char line[BUFSIZ], *st;

#ifdef TRACE
    _nc_tracing = TRACE_MOVE;
#endif
    for (;;) {
	int n;

	for (n = 0; n < screen_lines(sp); n++)
	    oldnums[n] = _NEWINDEX;

	 
	if (fgets(line, sizeof(line), stdin) == (char *) NULL)
	    exit(EXIT_SUCCESS);

	 
	n = 0;
	if (line[0] == '#') {
	    (void) fputs(line, stderr);
	    continue;
	}
	st = strtok(line, " ");
	do {
	    oldnums[n++] = atoi(st);
	} while
	    ((st = strtok((char *) NULL, " ")) != 0);

	 
	(void) fputs("Initial input:\n", stderr);
	_nc_linedump();

	_nc_scroll_optimize();
    }
}

#endif  

 
