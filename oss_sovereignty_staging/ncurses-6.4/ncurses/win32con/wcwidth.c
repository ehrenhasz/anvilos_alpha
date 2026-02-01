 
#include <curses.priv.h>

MODULE_ID("$Id: wcwidth.c,v 1.4 2020/07/11 21:02:10 tom Exp $")

#if USE_WIDEC_SUPPORT
#define mk_wcwidth(ucs)          _nc_wcwidth(ucs)
#define mk_wcswidth(pwcs, n)     _nc_wcswidth(pwcs, n)
#define mk_wcwidth_cjk(ucs)      _nc_wcwidth_cjk(ucs)
#define mk_wcswidth_cjk(pwcs, n) _nc_wcswidth_cjk(pwcs, n)

NCURSES_EXPORT(int) mk_wcwidth(wchar_t);
NCURSES_EXPORT(int) mk_wcswidth(const wchar_t *, size_t);
NCURSES_EXPORT(int) mk_wcwidth_cjk(wchar_t);
NCURSES_EXPORT(int) mk_wcswidth_cjk(const wchar_t *, size_t);

#include <wcwidth.h>
#else
void _nc_empty_wcwidth(void);
void
_nc_empty_wcwidth(void)
{
}
#endif
