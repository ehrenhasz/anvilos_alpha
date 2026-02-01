 

 

 

#define USE_LIBTINFO
#include <clear_cmd.h>

MODULE_ID("$Id: clear_cmd.c,v 1.5 2020/02/02 23:34:34 tom Exp $")

static int
putch(int c)
{
    return putchar(c);
}

int
clear_cmd(bool legacy)
{
    int retval = tputs(clear_screen, lines > 0 ? lines : 1, putch);
    if (!legacy) {
	 
	char *E3 = tigetstr("E3");
	if (E3)
	    (void) tputs(E3, lines > 0 ? lines : 1, putch);
    }
    return retval;
}
