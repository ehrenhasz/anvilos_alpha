 

 

#include <curses.priv.h>

MODULE_ID("$Id: charable.c,v 1.8 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(bool) _nc_is_charable(wchar_t ch)
{
    bool result;
#if HAVE_WCTOB
    result = (wctob((wint_t) ch) == (int) ch);
#else
    result = (_nc_to_char(ch) >= 0);
#endif
    return result;
}

NCURSES_EXPORT(int) _nc_to_char(wint_t ch)
{
    int result;
#if HAVE_WCTOB
    result = wctob(ch);
#elif HAVE_WCTOMB
    char temp[MB_LEN_MAX];
    result = wctomb(temp, ch);
    if (strlen(temp) == 1)
	result = UChar(temp[0]);
    else
	result = -1;
#else
#error expected either wctob/wctomb
#endif
    return result;
}

NCURSES_EXPORT(wint_t) _nc_to_widechar(int ch)
{
    wint_t result;
#if HAVE_BTOWC
    result = btowc(ch);
#elif HAVE_MBTOWC
    wchar_t convert;
    char temp[2];
    temp[0] = ch;
    temp[1] = '\0';
    if (mbtowc(&convert, temp, 1) >= 0)
	result = convert;
    else
	result = WEOF;
#else
#error expected either btowc/mbtowc
#endif
    return result;
}
