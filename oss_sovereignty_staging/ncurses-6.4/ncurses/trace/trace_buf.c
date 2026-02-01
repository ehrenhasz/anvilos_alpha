 

 
 

#include <curses.priv.h>

MODULE_ID("$Id: trace_buf.c,v 1.21 2020/02/02 23:34:34 tom Exp $")

#ifdef TRACE

#define MyList _nc_globals.tracebuf_ptr
#define MySize _nc_globals.tracebuf_used

static char *
_nc_trace_alloc(int bufnum, size_t want)
{
    char *result = 0;

    if (bufnum >= 0) {
	if ((size_t) (bufnum + 1) > MySize) {
	    size_t need = (size_t) (bufnum + 1) * 2;
	    if ((MyList = typeRealloc(TRACEBUF, need, MyList)) != 0) {
		while (need > MySize)
		    MyList[MySize++].text = 0;
	    }
	}

	if (MyList != 0) {
	    if (MyList[bufnum].text == 0
		|| want > MyList[bufnum].size) {
		MyList[bufnum].text = typeRealloc(char, want, MyList[bufnum].text);
		if (MyList[bufnum].text != 0)
		    MyList[bufnum].size = want;
	    }
	    result = MyList[bufnum].text;
	}
    }
#if NO_LEAKS
    else {
	if (MySize) {
	    if (MyList) {
		while (MySize--) {
		    if (MyList[MySize].text != 0) {
			free(MyList[MySize].text);
		    }
		}
		free(MyList);
		MyList = 0;
	    }
	    MySize = 0;
	}
    }
#endif
    return result;
}

 
NCURSES_EXPORT(char *)
_nc_trace_buf(int bufnum, size_t want)
{
    char *result = _nc_trace_alloc(bufnum, want);
    if (result != 0)
	*result = '\0';
    return result;
}

 
NCURSES_EXPORT(char *)
_nc_trace_bufcat(int bufnum, const char *value)
{
    char *buffer = _nc_trace_alloc(bufnum, (size_t) 0);
    if (buffer != 0) {
	size_t have = strlen(buffer);
	size_t need = strlen(value) + have;

	buffer = _nc_trace_alloc(bufnum, 1 + need);
	if (buffer != 0)
	    _nc_STRCPY(buffer + have, value, need);

    }
    return buffer;
}
#else
EMPTY_MODULE(_nc_empty_trace_buf)
#endif  
