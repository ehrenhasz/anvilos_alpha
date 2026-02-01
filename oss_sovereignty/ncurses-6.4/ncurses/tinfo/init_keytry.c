 

#include <curses.priv.h>
#include <tic.h>		 

MODULE_ID("$Id: init_keytry.c,v 1.19 2020/02/02 23:34:34 tom Exp $")

 

 
#undef CUR
#define CUR SP_TERMTYPE

#if	BROKEN_LINKER
#undef	_nc_tinfo_fkeys
#endif

 
#include <init_keytry.h>
 

#if	BROKEN_LINKER
const struct tinfo_fkeys *
_nc_tinfo_fkeysf(void)
{
    return _nc_tinfo_fkeys;
}
#endif

NCURSES_EXPORT(void)
_nc_init_keytry(SCREEN *sp)
{
     

    if (sp != 0) {
	unsigned n;

	for (n = 0; _nc_tinfo_fkeys[n].code; n++) {
	    if (_nc_tinfo_fkeys[n].offset < STRCOUNT) {
		(void) _nc_add_to_try(&(sp->_keytry),
				      CUR Strings[_nc_tinfo_fkeys[n].offset],
				      _nc_tinfo_fkeys[n].code);
	    }
	}
#if NCURSES_XNAMES
	 
	{
	    TERMTYPE *tp = &(sp->_term->type);
	    for (n = STRCOUNT; n < NUM_STRINGS(tp); ++n) {
		const char *name = ExtStrname(tp, (int) n, strnames);
		char *value = tp->Strings[n];
		if (name != 0
		    && *name == 'k'
		    && value != 0
		    && NCURSES_SP_NAME(key_defined) (NCURSES_SP_ARGx
						     value) == 0) {
		    (void) _nc_add_to_try(&(sp->_keytry),
					  value,
					  n - STRCOUNT + KEY_MAX);
		}
	    }
	}
#endif
#ifdef TRACE
	_nc_trace_tries(sp->_keytry);
#endif
    }
}
