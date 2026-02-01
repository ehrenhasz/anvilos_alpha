 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: tries.c,v 1.31 2020/02/02 23:34:34 tom Exp $")

 
NCURSES_EXPORT(char *)
_nc_expand_try(TRIES * tree, unsigned code, int *count, size_t len)
{
    TRIES *ptr = tree;
    char *result = 0;

    if (code != 0) {
	while (ptr != 0) {
	    if ((result = _nc_expand_try(ptr->child, code, count, len + 1))
		!= 0) {
		break;
	    }
	    if (ptr->value == code) {
		*count -= 1;
		if (*count == -1) {
		    result = typeCalloc(char, len + 2);
		    break;
		}
	    }
	    ptr = ptr->sibling;
	}
    }
    if (result != 0) {
	if (ptr != 0 && (result[len] = (char) ptr->ch) == 0)
	    *((unsigned char *) (result + len)) = 128;
#ifdef TRACE
	if (len == 0 && USE_TRACEF(TRACE_MAXIMUM)) {
	    _tracef("expand_key %s %s",
		    _nc_tracechar(CURRENT_SCREEN, (int) code),
		    _nc_visbuf(result));
	    _nc_unlock_global(tracef);
	}
#endif
    }
    return result;
}

 
NCURSES_EXPORT(int)
_nc_remove_key(TRIES ** tree, unsigned code)
{
    T((T_CALLED("_nc_remove_key(%p,%d)"), (void *) tree, code));

    if (code == 0)
	returnCode(FALSE);

    while (*tree != 0) {
	if (_nc_remove_key(&(*tree)->child, code)) {
	    returnCode(TRUE);
	}
	if ((*tree)->value == code) {
	    if ((*tree)->child) {
		 
		(*tree)->value = 0;
	    } else {
		TRIES *to_free = *tree;
		*tree = (*tree)->sibling;
		free(to_free);
	    }
	    returnCode(TRUE);
	}
	tree = &(*tree)->sibling;
    }
    returnCode(FALSE);
}

 
NCURSES_EXPORT(int)
_nc_remove_string(TRIES ** tree, const char *string)
{
    T((T_CALLED("_nc_remove_string(%p,%s)"), (void *) tree, _nc_visbuf(string)));

    if (string == 0 || *string == 0)
	returnCode(FALSE);

    while (*tree != 0) {
	if (UChar((*tree)->ch) == UChar(*string)) {
	    if (string[1] != 0)
		returnCode(_nc_remove_string(&(*tree)->child, string + 1));
	    if ((*tree)->child == 0) {
		TRIES *to_free = *tree;
		*tree = (*tree)->sibling;
		free(to_free);
		returnCode(TRUE);
	    } else {
		returnCode(FALSE);
	    }
	}
	tree = &(*tree)->sibling;
    }
    returnCode(FALSE);
}
