 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: obsolete.c,v 1.6 2020/02/02 23:34:34 tom Exp $")

 
NCURSES_EXPORT(void)
NCURSES_SP_NAME(_nc_set_buffer) (NCURSES_SP_DCLx FILE *ofp, int buffered)
{
#if NCURSES_SP_FUNCS
    (void) SP_PARM;
#endif
    (void) ofp;
    (void) buffered;
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(void)
_nc_set_buffer(FILE *ofp, int buffered)
{
    NCURSES_SP_NAME(_nc_set_buffer) (CURRENT_SCREEN, ofp, buffered);
}
#endif

#if !HAVE_STRDUP
NCURSES_EXPORT(char *)
_nc_strdup(const char *s)
{
    char *result = 0;
    if (s != 0) {
	size_t need = strlen(s);
	result = malloc(need + 1);
	if (result != 0) {
	    _nc_STRCPY(result, s, need);
	}
    }
    return result;
}
#endif

#if USE_MY_MEMMOVE
#define DST ((char *)s1)
#define SRC ((const char *)s2)
NCURSES_EXPORT(void *)
_nc_memmove(void *s1, const void *s2, size_t n)
{
    if (n != 0) {
	if ((DST + n > SRC) && (SRC + n > DST)) {
	    static char *bfr;
	    static size_t length;
	    register size_t j;
	    if (length < n) {
		length = (n * 3) / 2;
		bfr = typeRealloc(char, length, bfr);
	    }
	    for (j = 0; j < n; j++)
		bfr[j] = SRC[j];
	    s2 = bfr;
	}
	while (n-- != 0)
	    DST[n] = SRC[n];
    }
    return s1;
}
#endif  

#ifdef EXP_XTERM_1005
NCURSES_EXPORT(int)
_nc_conv_to_utf8(unsigned char *target, unsigned source, unsigned limit)
{
#define CH(n) UChar((source) >> ((n) * 8))
    int rc = 0;

    if (source <= 0x0000007f)
	rc = 1;
    else if (source <= 0x000007ff)
	rc = 2;
    else if (source <= 0x0000ffff)
	rc = 3;
    else if (source <= 0x001fffff)
	rc = 4;
    else if (source <= 0x03ffffff)
	rc = 5;
    else			 
	rc = 6;

    if ((unsigned) rc > limit) {	 
	rc = 0;
    }

    if (target != 0) {
	switch (rc) {
	case 1:
	    target[0] = CH(0);
	    break;

	case 2:
	    target[1] = UChar(0x80 | (CH(0) & 0x3f));
	    target[0] = UChar(0xc0 | (CH(0) >> 6) | ((CH(1) & 0x07) << 2));
	    break;

	case 3:
	    target[2] = UChar(0x80 | (CH(0) & 0x3f));
	    target[1] = UChar(0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	    target[0] = UChar(0xe0 | ((int) (CH(1) & 0xf0) >> 4));
	    break;

	case 4:
	    target[3] = UChar(0x80 | (CH(0) & 0x3f));
	    target[2] = UChar(0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	    target[1] = UChar(0x80 |
			      ((int) (CH(1) & 0xf0) >> 4) |
			      ((int) (CH(2) & 0x03) << 4));
	    target[0] = UChar(0xf0 | ((int) (CH(2) & 0x1f) >> 2));
	    break;

	case 5:
	    target[4] = UChar(0x80 | (CH(0) & 0x3f));
	    target[3] = UChar(0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	    target[2] = UChar(0x80 |
			      ((int) (CH(1) & 0xf0) >> 4) |
			      ((int) (CH(2) & 0x03) << 4));
	    target[1] = UChar(0x80 | (CH(2) >> 2));
	    target[0] = UChar(0xf8 | (CH(3) & 0x03));
	    break;

	case 6:
	    target[5] = UChar(0x80 | (CH(0) & 0x3f));
	    target[4] = UChar(0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	    target[3] = UChar(0x80 | (CH(1) >> 4) | ((CH(2) & 0x03) << 4));
	    target[2] = UChar(0x80 | (CH(2) >> 2));
	    target[1] = UChar(0x80 | (CH(3) & 0x3f));
	    target[0] = UChar(0xfc | ((int) (CH(3) & 0x40) >> 6));
	    break;
	}
    }

    return rc;			 
#undef CH
}

NCURSES_EXPORT(int)
_nc_conv_to_utf32(unsigned *target, const char *source, unsigned limit)
{
#define CH(n) UChar((*target) >> ((n) * 8))
    int rc = 0;
    int j;
    unsigned mask = 0;

     
    if ((*source & 0x80) == 0) {
	rc = 1;
	mask = (unsigned) *source;
    } else if ((*source & 0xe0) == 0xc0) {
	rc = 2;
	mask = (unsigned) (*source & 0x1f);
    } else if ((*source & 0xf0) == 0xe0) {
	rc = 3;
	mask = (unsigned) (*source & 0x0f);
    } else if ((*source & 0xf8) == 0xf0) {
	rc = 4;
	mask = (unsigned) (*source & 0x07);
    } else if ((*source & 0xfc) == 0xf8) {
	rc = 5;
	mask = (unsigned) (*source & 0x03);
    } else if ((*source & 0xfe) == 0xfc) {
	rc = 6;
	mask = (unsigned) (*source & 0x01);
    }

    if ((unsigned) rc > limit) {	 
	rc = 0;
    }

     
    if (rc > 1) {
	for (j = 1; j < rc; j++) {
	    if ((source[j] & 0xc0) != 0x80)
		break;
	}
	if (j != rc) {
	    rc = 0;
	}
    }

    if (target != 0) {
	int shift = 0;
	*target = 0;
	for (j = 1; j < rc; j++) {
	    *target |= (unsigned) (source[rc - j] & 0x3f) << shift;
	    shift += 6;
	}
	*target |= mask << shift;
    }
    return rc;
#undef CH
}
#endif  
