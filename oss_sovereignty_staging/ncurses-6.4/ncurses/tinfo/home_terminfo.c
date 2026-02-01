 

 

 

#include <curses.priv.h>
#include <tic.h>

MODULE_ID("$Id: home_terminfo.c,v 1.17 2020/02/02 23:34:34 tom Exp $")

 

#define MyBuffer _nc_globals.home_terminfo

NCURSES_EXPORT(char *)
_nc_home_terminfo(void)
{
    char *result = 0;
#if USE_HOME_TERMINFO
    if (use_terminfo_vars()) {

	if (MyBuffer == 0) {
	    char *home;

	    if ((home = getenv("HOME")) != 0) {
		size_t want = (strlen(home) + sizeof(PRIVATE_INFO));
		TYPE_MALLOC(char, want, MyBuffer);
		_nc_SPRINTF(MyBuffer, _nc_SLIMIT(want) PRIVATE_INFO, home);
	    }
	}
	result = MyBuffer;
    }
#endif
    return result;
}
