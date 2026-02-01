 

 

#include <curses.priv.h>

#include <ctype.h>

#include <tic.h>

MODULE_ID("$Id: trim_sgr0.c,v 1.21 2021/06/17 21:20:30 tom Exp $")

#undef CUR
#define CUR tp->

#define CSI       233
#define ESC       033		 
#define L_BRACK   '['

static char *
set_attribute_9(TERMTYPE2 *tp, int flag)
{
    const char *value;
    char *result;

    value = TIPARM_9(set_attributes, 0, 0, 0, 0, 0, 0, 0, 0, flag);
    if (PRESENT(value))
	result = strdup(value);
    else
	result = 0;
    return result;
}

static int
is_csi(const char *s)
{
    int result = 0;
    if (s != 0) {
	if (UChar(s[0]) == CSI)
	    result = 1;
	else if (s[0] == ESC && s[1] == L_BRACK)
	    result = 2;
    }
    return result;
}

static char *
skip_zero(char *s)
{
    if (s[0] == '0') {
	if (s[1] == ';')
	    s += 2;
	else if (isalpha(UChar(s[1])))
	    s += 1;
    }
    return s;
}

static const char *
skip_delay(const char *s)
{
    if (s[0] == '$' && s[1] == '<') {
	s += 2;
	while (isdigit(UChar(*s)) || *s == '/')
	    ++s;
	if (*s == '>')
	    ++s;
    }
    return s;
}

 
static bool
rewrite_sgr(char *s, char *attr)
{
    if (s != 0) {
	if (PRESENT(attr)) {
	    size_t len_s = strlen(s);
	    size_t len_a = strlen(attr);

	    if (len_s > len_a && !strncmp(attr, s, len_a)) {
		unsigned n;
		TR(TRACE_DATABASE, ("rewrite:\n\t%s", s));
		for (n = 0; n < len_s - len_a; ++n) {
		    s[n] = s[n + len_a];
		}
		_nc_STRCPY(s + n, attr, strlen(s) + 1);
		TR(TRACE_DATABASE, ("to:\n\t%s", s));
	    }
	}
	return TRUE;
    }
    return FALSE;		 
}

static bool
similar_sgr(char *a, char *b)
{
    bool result = FALSE;
    if (a != 0 && b != 0) {
	int csi_a = is_csi(a);
	int csi_b = is_csi(b);
	size_t len_a;
	size_t len_b;

	TR(TRACE_DATABASE, ("similar_sgr:\n\t%s\n\t%s",
			    _nc_visbuf2(1, a),
			    _nc_visbuf2(2, b)));
	if (csi_a != 0 && csi_b != 0 && csi_a == csi_b) {
	    a += csi_a;
	    b += csi_b;
	    if (*a != *b) {
		a = skip_zero(a);
		b = skip_zero(b);
	    }
	}
	len_a = strlen(a);
	len_b = strlen(b);
	if (len_a && len_b) {
	    if (len_a > len_b)
		result = (strncmp(a, b, len_b) == 0);
	    else
		result = (strncmp(a, b, len_a) == 0);
	}
	TR(TRACE_DATABASE, ("...similar_sgr: %d\n\t%s\n\t%s", result,
			    _nc_visbuf2(1, a),
			    _nc_visbuf2(2, b)));
    }
    return result;
}

static unsigned
chop_out(char *string, unsigned i, unsigned j)
{
    TR(TRACE_DATABASE, ("chop_out %d..%d from %s", i, j, _nc_visbuf(string)));
    while (string[j] != '\0') {
	string[i++] = string[j++];
    }
    string[i] = '\0';
    return i;
}

 
static unsigned
compare_part(const char *part, const char *full)
{
    const char *next_part;
    const char *next_full;
    unsigned used_full = 0;
    unsigned used_delay = 0;

    while (*part != 0) {
	if (*part != *full) {
	    used_full = 0;
	    break;
	}

	 
	if (used_delay != 0) {
	    used_full += used_delay;
	    used_delay = 0;
	}
	if (*part == '$' && *full == '$') {
	    next_part = skip_delay(part);
	    next_full = skip_delay(full);
	    if (next_part != part && next_full != full) {
		used_delay += (unsigned) (next_full - full);
		full = next_full;
		part = next_part;
		continue;
	    }
	}
	++used_full;
	++part;
	++full;
    }
    return used_full;
}

 
NCURSES_EXPORT(char *)
_nc_trim_sgr0(TERMTYPE2 *tp)
{
    char *result = exit_attribute_mode;

    T((T_CALLED("_nc_trim_sgr0()")));

    if (PRESENT(exit_attribute_mode)
	&& PRESENT(set_attributes)) {
	bool found = FALSE;
	char *on = set_attribute_9(tp, 1);
	char *off = set_attribute_9(tp, 0);
	char *end = strdup(exit_attribute_mode);
	char *tmp;
	size_t i, j, k;

	TR(TRACE_DATABASE, ("checking if we can trim sgr0 based on sgr"));
	TR(TRACE_DATABASE, ("sgr0       %s", _nc_visbuf(end)));
	TR(TRACE_DATABASE, ("sgr(9:off) %s", _nc_visbuf(off)));
	TR(TRACE_DATABASE, ("sgr(9:on)  %s", _nc_visbuf(on)));

	if (!rewrite_sgr(on, enter_alt_charset_mode)
	    || !rewrite_sgr(off, exit_alt_charset_mode)
	    || !rewrite_sgr(end, exit_alt_charset_mode)) {
	    FreeIfNeeded(off);
	} else if (similar_sgr(off, end)
		   && !similar_sgr(off, on)) {
	    TR(TRACE_DATABASE, ("adjusting sgr(9:off) : %s", _nc_visbuf(off)));
	    result = off;
	     
	    if (PRESENT(exit_alt_charset_mode)) {
		TR(TRACE_DATABASE, ("scan for rmacs %s", _nc_visbuf(exit_alt_charset_mode)));
		j = strlen(off);
		k = strlen(exit_alt_charset_mode);
		if (j > k) {
		    for (i = 0; i <= (j - k); ++i) {
			unsigned k2 = compare_part(exit_alt_charset_mode,
						   off + i);
			if (k2 != 0) {
			    found = TRUE;
			    chop_out(off, (unsigned) i, (unsigned) (i + k2));
			    break;
			}
		    }
		}
	    }
	     
	    if (!found) {
		if ((i = (size_t) is_csi(off)) != 0
		    && off[strlen(off) - 1] == 'm') {
		    TR(TRACE_DATABASE, ("looking for SGR 10 in %s",
					_nc_visbuf(off)));
		    tmp = skip_zero(off + i);
		    if (tmp[0] == '1'
			&& skip_zero(tmp + 1) != tmp + 1) {
			i = (size_t) (tmp - off);
			if (off[i - 1] == ';')
			    i--;
			j = (size_t) (skip_zero(tmp + 1) - off);
			(void) chop_out(off, (unsigned) i, (unsigned) j);
			found = TRUE;
		    }
		}
	    }
	    if (!found
		&& (tmp = strstr(end, off)) != 0
		&& strcmp(end, off) != 0) {
		i = (size_t) (tmp - end);
		j = strlen(off);
		tmp = strdup(end);
		chop_out(tmp, (unsigned) i, (unsigned) j);
		free(off);
		result = tmp;
	    }
	    TR(TRACE_DATABASE, ("...adjusted sgr0 : %s", _nc_visbuf(result)));
	    if (!strcmp(result, exit_attribute_mode)) {
		TR(TRACE_DATABASE, ("...same result, discard"));
		free(result);
		result = exit_attribute_mode;
	    }
	} else {
	     
	    free(off);
	}
	FreeIfNeeded(end);
	FreeIfNeeded(on);
    } else {
	 
    }

    returnPtr(result);
}
