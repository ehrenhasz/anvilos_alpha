 

 

#include <curses.priv.h>

MODULE_ID("$Id: version.c,v 1.7 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(const char *)
curses_version(void)
{
    T((T_CALLED("curses_version()")));
    returnCPtr("ncurses " NCURSES_VERSION_STRING);
}
