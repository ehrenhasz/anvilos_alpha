 

 
 

#include <curses.priv.h>

MODULE_ID("$Id: trace_xnames.c,v 1.8 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(void)
_nc_trace_xnames(TERMTYPE *tp GCC_UNUSED)
{
#ifdef TRACE
#if NCURSES_XNAMES
    int limit = tp->ext_Booleans + tp->ext_Numbers + tp->ext_Strings;

    if (limit) {
	int n;
	int begin_num = tp->ext_Booleans;
	int begin_str = tp->ext_Booleans + tp->ext_Numbers;

	_tracef("extended names (%s) %d = %d+%d+%d of %d+%d+%d",
		tp->term_names,
		limit,
		tp->ext_Booleans, tp->ext_Numbers, tp->ext_Strings,
		tp->num_Booleans, tp->num_Numbers, tp->num_Strings);

	for (n = 0; n < limit; n++) {
	    int m;

	    if ((m = n - begin_str) >= 0) {
		_tracef("[%d] %s = %s", n,
			tp->ext_Names[n],
			_nc_visbuf(tp->Strings[tp->num_Strings + m - tp->ext_Strings]));
	    } else if ((m = n - begin_num) >= 0) {
		_tracef("[%d] %s = %d (num)", n,
			tp->ext_Names[n],
			tp->Numbers[tp->num_Numbers + m - tp->ext_Numbers]);
	    } else {
		_tracef("[%d] %s = %d (bool)", n,
			tp->ext_Names[n],
			tp->Booleans[tp->num_Booleans + n - tp->ext_Booleans]);
	    }
	}
    }
#endif
#endif
}
