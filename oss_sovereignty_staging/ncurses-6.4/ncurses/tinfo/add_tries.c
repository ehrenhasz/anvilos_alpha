 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: add_tries.c,v 1.12 2020/02/02 23:34:34 tom Exp $")

#define SET_TRY(dst,src) if ((dst->ch = *src++) == 128) dst->ch = '\0'
#define CMP_TRY(a,b) ((a)? (a == b) : (b == 128))

NCURSES_EXPORT(int)
_nc_add_to_try(TRIES ** tree, const char *str, unsigned code)
{
    TRIES *ptr, *savedptr;
    unsigned const char *txt = (unsigned const char *) str;

    T((T_CALLED("_nc_add_to_try(%p, %s, %u)"),
       (void *) *tree, _nc_visbuf(str), code));
    if (txt == 0 || *txt == '\0' || code == 0)
	returnCode(ERR);

    if ((*tree) != 0) {
	ptr = savedptr = (*tree);

	for (;;) {
	    unsigned char cmp = *txt;

	    while (!CMP_TRY(ptr->ch, cmp)
		   && ptr->sibling != 0)
		ptr = ptr->sibling;

	    if (CMP_TRY(ptr->ch, cmp)) {
		if (*(++txt) == '\0') {
		    ptr->value = (unsigned short) code;
		    returnCode(OK);
		}
		if (ptr->child != 0)
		    ptr = ptr->child;
		else
		    break;
	    } else {
		if ((ptr->sibling = typeCalloc(TRIES, 1)) == 0) {
		    returnCode(ERR);
		}

		savedptr = ptr = ptr->sibling;
		SET_TRY(ptr, txt);
		ptr->value = 0;

		break;
	    }
	}			 
    } else {			 
	savedptr = ptr = (*tree) = typeCalloc(TRIES, 1);

	if (ptr == 0) {
	    returnCode(ERR);
	}

	SET_TRY(ptr, txt);
	ptr->value = 0;
    }

     

    while (*txt) {
	ptr->child = typeCalloc(TRIES, 1);

	ptr = ptr->child;

	if (ptr == 0) {
	    while ((ptr = savedptr) != 0) {
		savedptr = ptr->child;
		free(ptr);
	    }
	    *tree = NULL;
	    returnCode(ERR);
	}

	SET_TRY(ptr, txt);
	ptr->value = 0;
    }

    ptr->value = (unsigned short) code;
    returnCode(OK);
}
