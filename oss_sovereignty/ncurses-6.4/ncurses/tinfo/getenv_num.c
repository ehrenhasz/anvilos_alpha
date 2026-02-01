 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: getenv_num.c,v 1.8 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
_nc_getenv_num(const char *name)
{
    char *dst = 0;
    char *src = getenv(name);
    long value;

    if ((src == 0)
	|| (value = strtol(src, &dst, 0)) < 0
	|| (dst == src)
	|| (*dst != '\0')
	|| (int) value < value)
	value = -1;

    return (int) value;
}

NCURSES_EXPORT(void)
_nc_setenv_num(const char *name, int value)
{
    if (name != 0 && value >= 0) {
	char buffer[128];
#if HAVE_SETENV
	_nc_SPRINTF(buffer, _nc_SLIMIT(sizeof(buffer)) "%d", value);
	setenv(name, buffer, 1);
#elif HAVE_PUTENV
	char *s;
	_nc_SPRINTF(buffer, _nc_SLIMIT(sizeof(buffer)) "%s=%d", name, value);
	if ((s = strdup(buffer)) != 0)
	    putenv(s);
#else
#error expected setenv/putenv functions
#endif
    }
}
