 

#ifdef WINVER
#  undef WINVER
#endif
#define WINVER 0x0501

#include <curses.priv.h>

#include <windows.h>

MODULE_ID("$Id: gettimeofday.c,v 1.6 2020/07/11 21:03:53 tom Exp $")

#define JAN1970 116444736000000000LL	 

NCURSES_EXPORT(int)
gettimeofday(struct timeval *tv, void *tz GCC_UNUSED)
{
    union {
	FILETIME ft;
	long long since1601;	 
    } data;

    GetSystemTimeAsFileTime(&data.ft);
    tv->tv_usec = (long) ((data.since1601 / 10LL) % 1000000LL);
    tv->tv_sec = (long) ((data.since1601 - JAN1970) / 10000000LL);
    return (0);
}
