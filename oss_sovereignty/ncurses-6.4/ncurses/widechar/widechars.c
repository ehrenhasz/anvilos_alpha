 

#include <curses.priv.h>

#if USE_WIDEC_SUPPORT

MODULE_ID("$Id: widechars.c,v 1.9 2020/08/29 16:22:03 juergen Exp $")

#if (defined(_NC_WINDOWS)) && !defined(_NC_MSC)
 

int
_nc_mbtowc(wchar_t *pwc, const char *s, size_t n)
{
    int result;
    int count;
    int try;

    if (s != 0 && n != 0) {
	 
	count = 0;
	for (try = 1; try <= (int) n; ++try) {
	    count = MultiByteToWideChar(CP_UTF8,
					MB_ERR_INVALID_CHARS,
					s,
					try,
					pwc,
					0);
	    TR(TRACE_BITS, ("...try %d:%d", try, count));
	    if (count > 0) {
		break;
	    }
	}
	if (count < 1 || count > 2) {
	    result = -1;
	} else {
	    wchar_t actual[2];
	    memset(&actual, 0, sizeof(actual));
	    count = MultiByteToWideChar(CP_UTF8,
					MB_ERR_INVALID_CHARS,
					s,
					try,
					actual,
					2);
	    TR(TRACE_BITS, ("\twin32 ->%#x, %#x", actual[0], actual[1]));
	    *pwc = actual[0];
	    if (actual[1] != 0)
		result = -1;
	    else
		result = try;
	}
    } else {
	result = 0;
    }

    return result;
}

int
_nc_mblen(const char *s, size_t n)
{
    int result = -1;
    int count;
    wchar_t temp;

    if (s != 0 && n != 0) {
	count = _nc_mbtowc(&temp, s, n);
	if (count == 1) {
	    int check = WideCharToMultiByte(CP_UTF8,
					    0,
					    &temp,
					    1,
					    NULL,
					    0,	 
					    NULL,
					    NULL);
	    TR(TRACE_BITS, ("\tcheck ->%d\n", check));
	    if (check > 0 && (size_t) check <= n) {
		result = check;
	    }
	}
    } else {
	result = 0;
    }

    return result;
}

int __MINGW_NOTHROW
_nc_wctomb(char *s, wchar_t wc)
{
    int result;
    int check;

    check = WideCharToMultiByte(CP_UTF8,
				0,
				&wc,
				1,
				NULL,
				0,	 
				NULL,
				NULL);
    if (check > 0) {
	result = WideCharToMultiByte(CP_UTF8,
				     0,
				     &wc,
				     1,
				     s,
				     check + 1,
				     NULL,
				     NULL);
    } else {
	result = -1;
    }
    return result;
}

#endif  

#endif  
