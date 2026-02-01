 

 

#include <curses.priv.h>

#ifndef CUR
#define CUR SP_TERMTYPE
#endif

MODULE_ID("$Id: lib_print.c,v 1.30 2021/04/18 14:58:57 tom Exp $")

NCURSES_EXPORT(int)
NCURSES_SP_NAME(mcprint) (NCURSES_SP_DCLx char *data, int len)
 
{
    int result;
    char *mybuf = NULL, *switchon;
    size_t onsize, offsize;
    size_t need;

    errno = 0;
    if (!HasTInfoTerminal(SP_PARM)
	|| len <= 0
	|| (!prtr_non && (!prtr_on || !prtr_off))) {
	errno = ENODEV;
	return (ERR);
    }

    if (prtr_non) {
	switchon = TIPARM_1(prtr_non, len);
	onsize = strlen(switchon);
	offsize = 0;
    } else {
	switchon = prtr_on;
	onsize = strlen(prtr_on);
	offsize = strlen(prtr_off);
    }

    need = onsize + (size_t) len + offsize;

    if (switchon == 0
	|| (mybuf = typeMalloc(char, need + 1)) == 0) {
	free(mybuf);
	errno = ENOMEM;
	return (ERR);
    }

    _nc_STRCPY(mybuf, switchon, need);
    memcpy(mybuf + onsize, data, (size_t) len);
    if (offsize)
	_nc_STRCPY(mybuf + onsize + len, prtr_off, need);

     
    result = (int) write(TerminalOf(SP_PARM)->Filedes, mybuf, need);

     
#ifndef _NC_WINDOWS
    (void) sleep(0);
#endif
    free(mybuf);
    return (result);
}

#if NCURSES_SP_FUNCS && !defined(USE_TERM_DRIVER)
NCURSES_EXPORT(int)
mcprint(char *data, int len)
{
    return NCURSES_SP_NAME(mcprint) (CURRENT_SCREEN, data, len);
}
#endif
