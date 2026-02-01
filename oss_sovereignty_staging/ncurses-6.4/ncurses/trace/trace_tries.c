 

 
 

#include <curses.priv.h>

MODULE_ID("$Id: trace_tries.c,v 1.18 2020/02/02 23:34:34 tom Exp $")

#ifdef TRACE
#define my_buffer _nc_globals.tracetry_buf
#define my_length _nc_globals.tracetry_used

static void
recur_tries(TRIES * tree, unsigned level)
{
    if (level > my_length) {
	my_length = (level + 1) * 4;
	my_buffer = (unsigned char *) _nc_doalloc(my_buffer, my_length);
    }

    if (my_buffer != 0) {
	while (tree != 0) {
	    if ((my_buffer[level] = tree->ch) == 0)
		my_buffer[level] = 128;
	    my_buffer[level + 1] = 0;
	    if (tree->value != 0) {
		_tracef("%5d: %s (%s)", tree->value,
			_nc_visbuf((char *) my_buffer), keyname(tree->value));
	    }
	    if (tree->child)
		recur_tries(tree->child, level + 1);
	    tree = tree->sibling;
	}
    }
}

NCURSES_EXPORT(void)
_nc_trace_tries(TRIES * tree)
{
    if ((my_buffer = typeMalloc(unsigned char, my_length = 80)) != 0) {
	_tracef("BEGIN tries %p", (void *) tree);
	recur_tries(tree, 0);
	_tracef(". . . tries %p", (void *) tree);
	free(my_buffer);
    }
}

#else
EMPTY_MODULE(_nc_empty_trace_tries)
#endif
