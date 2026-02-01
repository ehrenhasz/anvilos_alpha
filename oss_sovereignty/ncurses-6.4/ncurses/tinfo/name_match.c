 

 

#include <curses.priv.h>
#include <tic.h>

MODULE_ID("$Id: name_match.c,v 1.25 2020/02/02 23:34:34 tom Exp $")

#define FirstName _nc_globals.first_name

#if NCURSES_USE_TERMCAP && NCURSES_XNAMES
static const char *
skip_index(const char *name)
{
    if ((_nc_syntax == SYN_TERMCAP) && _nc_user_definable) {
	const char *bar = strchr(name, '|');
	if (bar != 0 && (bar - name) == 2)
	    name = bar + 1;
    }
    return name;
}
#endif

 
NCURSES_EXPORT(char *)
_nc_first_name(const char *const sp)
{
#if NO_LEAKS
    if (sp == 0) {
	if (FirstName != 0) {
	    FreeAndNull(FirstName);
	}
    } else
#endif
    {
	if (FirstName == 0)
	    FirstName = typeMalloc(char, MAX_NAME_SIZE + 1);

	if (FirstName != 0) {
	    unsigned n;
	    const char *src = sp;
#if NCURSES_USE_TERMCAP && NCURSES_XNAMES
	    src = skip_index(sp);
#endif
	    for (n = 0; n < MAX_NAME_SIZE; n++) {
		if ((FirstName[n] = src[n]) == '\0'
		    || (FirstName[n] == '|'))
		    break;
	    }
	    FirstName[n] = '\0';
	}
    }
    return (FirstName);
}

 
NCURSES_EXPORT(int)
_nc_name_match(const char *const namelst, const char *const name, const char *const delim)
{
    const char *s;

    if ((s = namelst) != 0) {
	while (*s != '\0') {
	    const char *d, *t;
	    int code, found;

	    for (d = name; *d != '\0'; d++) {
		if (*s != *d)
		    break;
		s++;
	    }
	    found = FALSE;
	    for (code = TRUE; *s != '\0'; code = FALSE, s++) {
		for (t = delim; *t != '\0'; t++) {
		    if (*s == *t) {
			found = TRUE;
			break;
		    }
		}
		if (found)
		    break;
	    }
	    if (code && *d == '\0')
		return code;
	    if (*s++ == 0)
		break;
	}
    }
    return FALSE;
}
