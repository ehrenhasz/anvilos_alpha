 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: doalloc.c,v 1.14 2021/04/24 23:43:39 tom Exp $")

void *
_nc_doalloc(void *oldp, size_t amount)
{
    void *newp;

    if (oldp != NULL) {
	if (amount == 0) {
	    free(oldp);
	    newp = NULL;
	} else if ((newp = realloc(oldp, amount)) == 0) {
	    free(oldp);
	    errno = ENOMEM;	 
	}
    } else {
	newp = malloc(amount);
    }
    return newp;
}
