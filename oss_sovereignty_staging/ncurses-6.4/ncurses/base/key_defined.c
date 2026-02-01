 

 

#include <curses.priv.h>

MODULE_ID("$Id: key_defined.c,v 1.10 2020/02/02 23:34:34 tom Exp $")

static int
find_definition(TRIES * tree, const char *str)
{
    TRIES *ptr;
    int result = OK;

    if (str != 0 && *str != '\0') {
	for (ptr = tree; ptr != 0; ptr = ptr->sibling) {
	    if (UChar(*str) == UChar(ptr->ch)) {
		if (str[1] == '\0' && ptr->child != 0) {
		    result = ERR;
		} else if ((result = find_definition(ptr->child, str + 1))
			   == OK) {
		    result = ptr->value;
		} else if (str[1] == '\0') {
		    result = ERR;
		}
	    }
	    if (result != OK)
		break;
	}
    }
    return (result);
}

 
NCURSES_EXPORT(int)
NCURSES_SP_NAME(key_defined) (NCURSES_SP_DCLx const char *str)
{
    int code = ERR;

    T((T_CALLED("key_defined(%p, %s)"), (void *) SP_PARM, _nc_visbuf(str)));
    if (SP_PARM != 0 && str != 0) {
	code = find_definition(SP_PARM->_keytry, str);
    }

    returnCode(code);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
key_defined(const char *str)
{
    return NCURSES_SP_NAME(key_defined) (CURRENT_SCREEN, str);
}
#endif
