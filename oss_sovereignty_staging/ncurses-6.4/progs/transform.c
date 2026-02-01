 

 
#include <progs.priv.h>
#include <string.h>

#include <transform.h>

MODULE_ID("$Id: transform.c,v 1.4 2020/02/02 23:34:34 tom Exp $")

#ifdef SUFFIX_IGNORED
static void
trim_suffix(const char *a, size_t *len)
{
    const char ignore[] = SUFFIX_IGNORED;

    if (sizeof(ignore) != 0) {
	bool trim = FALSE;
	size_t need = (sizeof(ignore) - 1);

	if (*len > need) {
	    size_t first = *len - need;
	    size_t n;
	    trim = TRUE;
	    for (n = first; n < *len; ++n) {
		if (tolower(UChar(a[n])) != tolower(UChar(ignore[n - first]))) {
		    trim = FALSE;
		    break;
		}
	    }
	    if (trim) {
		*len -= need;
	    }
	}
    }
}
#else
#define trim_suffix(a, len)	 
#endif

bool
same_program(const char *a, const char *b)
{
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);

    trim_suffix(a, &len_a);
    trim_suffix(b, &len_b);

    return (len_a == len_b) && (strncmp(a, b, len_a) == 0);
}
