 

 

#define __INTERNAL_CAPS_VISIBLE
#include <curses.priv.h>

#include <termcap.h>
#include <tic.h>
#include <ctype.h>

#ifndef CUR
#define CUR SP_TERMTYPE
#endif

MODULE_ID("$Id: lib_termcap.c,v 1.88 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT_VAR(char *) UP = 0;
NCURSES_EXPORT_VAR(char *) BC = 0;

#define MyCache  _nc_globals.tgetent_cache
#define CacheInx _nc_globals.tgetent_index
#define CacheSeq _nc_globals.tgetent_sequence

#define FIX_SGR0 MyCache[CacheInx].fix_sgr0
#define LAST_TRM MyCache[CacheInx].last_term
#define LAST_BUF MyCache[CacheInx].last_bufp
#define LAST_USE MyCache[CacheInx].last_used
#define LAST_SEQ MyCache[CacheInx].sequence

 
#define ValidCap(cap) (((cap)[0] != '\0') && ((cap)[1] != '\0'))
#define SameCap(a,b)  (((a)[0] == (b)[0]) && ((a)[1] == (b)[1]))
#define ValidExt(ext) (ValidCap(ext) && (ext)[2] == '\0')

 

NCURSES_EXPORT(int)
NCURSES_SP_NAME(tgetent) (NCURSES_SP_DCLx char *bufp, const char *name)
{
    int rc = ERR;
    int n;
    bool found_cache = FALSE;
#ifdef USE_TERM_DRIVER
    TERMINAL *termp = 0;
#endif

    START_TRACE();
    T((T_CALLED("tgetent()")));

    TINFO_SETUP_TERM(&termp, name, STDOUT_FILENO, &rc, TRUE);

#ifdef USE_TERM_DRIVER
    if (termp == 0 ||
	!((TERMINAL_CONTROL_BLOCK *) termp)->drv->isTerminfo)
	returnCode(rc);
#endif

     
    for (n = 0; n < TGETENT_MAX; ++n) {
	bool same_result = (MyCache[n].last_used && MyCache[n].last_bufp == bufp);
	if (same_result) {
	    CacheInx = n;
	    if (FIX_SGR0 != 0) {
		FreeAndNull(FIX_SGR0);
	    }
	     
	    if (LAST_TRM != 0 && LAST_TRM != TerminalOf(SP_PARM)) {
		TERMINAL *trm = LAST_TRM;
		NCURSES_SP_NAME(del_curterm) (NCURSES_SP_ARGx LAST_TRM);
		for (CacheInx = 0; CacheInx < TGETENT_MAX; ++CacheInx)
		    if (LAST_TRM == trm)
			LAST_TRM = 0;
		CacheInx = n;
	    }
	    found_cache = TRUE;
	    break;
	}
    }
    if (!found_cache) {
	int best = 0;

	for (CacheInx = 0; CacheInx < TGETENT_MAX; ++CacheInx) {
	    if (LAST_SEQ < MyCache[best].sequence) {
		best = CacheInx;
	    }
	}
	CacheInx = best;
    }
    if (rc == 1) {
	LAST_TRM = TerminalOf(SP_PARM);
	LAST_SEQ = ++CacheSeq;
    } else {
	LAST_TRM = 0;
    }

    PC = 0;
    UP = 0;
    BC = 0;
    FIX_SGR0 = 0;		 

    if (rc == 1) {

	if (cursor_left)
	    if ((backspaces_with_bs = (char) !strcmp(cursor_left, "\b")) == 0)
		backspace_if_not_bs = cursor_left;

	 
	if (pad_char != NULL)
	    PC = pad_char[0];
	if (cursor_up != NULL)
	    UP = cursor_up;
	if (backspace_if_not_bs != NULL)
	    BC = backspace_if_not_bs;

	if ((FIX_SGR0 = _nc_trim_sgr0(&TerminalType(TerminalOf(SP_PARM))))
	    != 0) {
	    if (!strcmp(FIX_SGR0, exit_attribute_mode)) {
		if (FIX_SGR0 != exit_attribute_mode) {
		    free(FIX_SGR0);
		}
		FIX_SGR0 = 0;
	    }
	}
	LAST_BUF = bufp;
	LAST_USE = TRUE;

	SetNoPadding(SP_PARM);
	(void) NCURSES_SP_NAME(baudrate) (NCURSES_SP_ARG);	 

 
#include <capdefaults.c>
 

    }
    returnCode(rc);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
tgetent(char *bufp, const char *name)
{
    return NCURSES_SP_NAME(tgetent) (CURRENT_SCREEN, bufp, name);
}
#endif

#if 0
static bool
same_tcname(const char *a, const char *b)
{
    bool code = SameCap(a, b);
    fprintf(stderr, "compare(%s,%s) %s\n", a, b, code ? "same" : "diff");
    return code;
}

#else
#define same_tcname(a,b) SameCap(a,b)
#endif

 

NCURSES_EXPORT(int)
NCURSES_SP_NAME(tgetflag) (NCURSES_SP_DCLx const char *id)
{
    int result = 0;		 

    T((T_CALLED("tgetflag(%p, %s)"), (void *) SP_PARM, id));
    if (HasTInfoTerminal(SP_PARM) && ValidCap(id)) {
	TERMTYPE2 *tp = &TerminalType(TerminalOf(SP_PARM));
	struct name_table_entry const *entry_ptr;
	int j = -1;

	entry_ptr = _nc_find_type_entry(id, BOOLEAN, TRUE);
	if (entry_ptr != 0) {
	    j = entry_ptr->nte_index;
	}
#if NCURSES_XNAMES
	else {
	    int i;
	    for_each_ext_boolean(i, tp) {
		const char *capname = ExtBoolname(tp, i, boolcodes);
		if (same_tcname(id, capname) && ValidExt(capname)) {
		    j = i;
		    break;
		}
	    }
	}
#endif
	if (j >= 0) {
	     
	    result = tp->Booleans[j];
	}
    }
    returnCode(result);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
tgetflag(const char *id)
{
    return NCURSES_SP_NAME(tgetflag) (CURRENT_SCREEN, id);
}
#endif

 

NCURSES_EXPORT(int)
NCURSES_SP_NAME(tgetnum) (NCURSES_SP_DCLx const char *id)
{
    int result = ABSENT_NUMERIC;

    T((T_CALLED("tgetnum(%p, %s)"), (void *) SP_PARM, id));
    if (HasTInfoTerminal(SP_PARM) && ValidCap(id)) {
	TERMTYPE2 *tp = &TerminalType(TerminalOf(SP_PARM));
	struct name_table_entry const *entry_ptr;
	int j = -1;

	entry_ptr = _nc_find_type_entry(id, NUMBER, TRUE);
	if (entry_ptr != 0) {
	    j = entry_ptr->nte_index;
	}
#if NCURSES_XNAMES
	else {
	    int i;
	    for_each_ext_number(i, tp) {
		const char *capname = ExtNumname(tp, i, numcodes);
		if (same_tcname(id, capname) && ValidExt(capname)) {
		    j = i;
		    break;
		}
	    }
	}
#endif
	if (j >= 0) {
	    if (VALID_NUMERIC(tp->Numbers[j]))
		result = tp->Numbers[j];
	}
    }
    returnCode(result);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
tgetnum(const char *id)
{
    return NCURSES_SP_NAME(tgetnum) (CURRENT_SCREEN, id);
}
#endif

 

NCURSES_EXPORT(char *)
NCURSES_SP_NAME(tgetstr) (NCURSES_SP_DCLx const char *id, char **area)
{
    char *result = NULL;

    T((T_CALLED("tgetstr(%s,%p)"), id, (void *) area));
    if (HasTInfoTerminal(SP_PARM) && ValidCap(id)) {
	TERMTYPE2 *tp = &TerminalType(TerminalOf(SP_PARM));
	struct name_table_entry const *entry_ptr;
	int j = -1;

	entry_ptr = _nc_find_type_entry(id, STRING, TRUE);
	if (entry_ptr != 0) {
	    j = entry_ptr->nte_index;
	}
#if NCURSES_XNAMES
	else {
	    int i;
	    for_each_ext_string(i, tp) {
		const char *capname = ExtStrname(tp, i, strcodes);
		if (same_tcname(id, capname) && ValidExt(capname)) {
		    j = i;
		    break;
		}
	    }
	}
#endif
	if (j >= 0) {
	    result = tp->Strings[j];
	    TR(TRACE_DATABASE, ("found match %d: %s", j, _nc_visbuf(result)));
	     
	    if (VALID_STRING(result)) {
		if (result == exit_attribute_mode
		    && FIX_SGR0 != 0) {
		    result = FIX_SGR0;
		    TR(TRACE_DATABASE, ("altered to : %s", _nc_visbuf(result)));
		}
		if (area != 0
		    && *area != 0) {
		    _nc_STRCPY(*area, result, 1024);
		    result = *area;
		    *area += strlen(*area) + 1;
		}
	    }
	}
    }
    returnPtr(result);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(char *)
tgetstr(const char *id, char **area)
{
    return NCURSES_SP_NAME(tgetstr) (CURRENT_SCREEN, id, area);
}
#endif

#if NO_LEAKS
#undef CacheInx
#define CacheInx num
NCURSES_EXPORT(void)
_nc_tgetent_leak(TERMINAL *termp)
{
    if (termp != 0) {
	int num;
	for (CacheInx = 0; CacheInx < TGETENT_MAX; ++CacheInx) {
	    if (LAST_TRM == termp) {
		FreeAndNull(FIX_SGR0);
		if (LAST_TRM != 0) {
		    LAST_TRM = 0;
		}
		break;
	    }
	}
    }
}

NCURSES_EXPORT(void)
_nc_tgetent_leaks(void)
{
    int num;
    for (CacheInx = 0; CacheInx < TGETENT_MAX; ++CacheInx) {
	if (LAST_TRM != 0) {
	    del_curterm(LAST_TRM);
	    _nc_tgetent_leak(LAST_TRM);
	}
    }
}
#endif
