 

 

 

#include <curses.priv.h>
#include <tic.h>

MODULE_ID("$Id: strings.c,v 1.10 2020/02/02 23:34:34 tom Exp $")

 

#if !HAVE_STRSTR
NCURSES_EXPORT(char *)
_nc_strstr(const char *haystack, const char *needle)
{
    size_t len1 = strlen(haystack);
    size_t len2 = strlen(needle);
    char *result = 0;

    while ((len1 != 0) && (len1-- >= len2)) {
	if (!strncmp(haystack, needle, len2)) {
	    result = (char *) haystack;
	    break;
	}
	haystack++;
    }
    return result;
}
#endif

 
NCURSES_EXPORT(string_desc *)
_nc_str_init(string_desc * dst, char *src, size_t len)
{
    if (dst != 0) {
	dst->s_head = src;
	dst->s_tail = src;
	dst->s_size = len - 1;
	dst->s_init = dst->s_size;
	if (src != 0)
	    *src = 0;
    }
    return dst;
}

 
NCURSES_EXPORT(string_desc *)
_nc_str_null(string_desc * dst, size_t len)
{
    return _nc_str_init(dst, 0, len);
}

 
NCURSES_EXPORT(string_desc *)
_nc_str_copy(string_desc * dst, string_desc * src)
{
    *dst = *src;
    return dst;
}

 
NCURSES_EXPORT(bool)
_nc_safe_strcat(string_desc * dst, const char *src)
{
    if (PRESENT(src)) {
	size_t len = strlen(src);

	if (len < dst->s_size) {
	    if (dst->s_tail != 0) {
		_nc_STRCPY(dst->s_tail, src, dst->s_size);
		dst->s_tail += len;
	    }
	    dst->s_size -= len;
	    return TRUE;
	}
    }
    return FALSE;
}

 
NCURSES_EXPORT(bool)
_nc_safe_strcpy(string_desc * dst, const char *src)
{
    if (PRESENT(src)) {
	size_t len = strlen(src);

	if (len < dst->s_size) {
	    if (dst->s_head != 0) {
		_nc_STRCPY(dst->s_head, src, dst->s_size);
		dst->s_tail = dst->s_head + len;
	    }
	    dst->s_size = dst->s_init - len;
	    return TRUE;
	}
    }
    return FALSE;
}
