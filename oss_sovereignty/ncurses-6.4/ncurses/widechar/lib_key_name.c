 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_key_name.c,v 1.5 2020/02/02 23:34:34 tom Exp $")

#define MyData _nc_globals.key_name

NCURSES_EXPORT(NCURSES_CONST char *)
key_name(wchar_t c)
{
    cchar_t my_cchar;
    wchar_t *my_wchars;
    size_t len;

    memset(&my_cchar, 0, sizeof(my_cchar));
    my_cchar.chars[0] = c;
    my_cchar.chars[1] = L'\0';

    my_wchars = wunctrl(&my_cchar);
    len = wcstombs(MyData, my_wchars, sizeof(MyData) - 1);
    if (isEILSEQ(len) || (len == 0)) {
	return 0;
    }

    MyData[len] = '\0';
    return MyData;
}
