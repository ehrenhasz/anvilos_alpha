 

 

 
#include <curses.priv.h>

#if HAVE_WCTYPE_H
#include <wctype.h>
#endif

MODULE_ID("$Id: lib_slk_wset.c,v 1.15 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
slk_wset(int i, const wchar_t *astr, int format)
{
    int result = ERR;
    const wchar_t *str;
    mbstate_t state;

    T((T_CALLED("slk_wset(%d, %s, %d)"), i, _nc_viswbuf(astr), format));

    if (astr != 0) {
	size_t arglen;

	init_mb(state);
	str = astr;

	if ((arglen = wcsrtombs(NULL, &str, (size_t) 0, &state)) != (size_t) -1) {
	    char *mystr;

	    if ((mystr = (char *) _nc_doalloc(0, arglen + 1)) != 0) {
		str = astr;
		if (wcsrtombs(mystr, &str, arglen, &state) != (size_t) -1) {
		     
		    mystr[arglen] = 0;
		    result = slk_set(i, mystr, format);
		}
		free(mystr);
	    }
	}
    }
    returnCode(result);
}
