 

 

#include <curses.priv.h>

#include <ctype.h>
#include <tic.h>

MODULE_ID("$Id: comp_expand.c,v 1.34 2021/09/04 10:29:15 tom Exp $")

#if 0
#define DEBUG_THIS(p) DEBUG(9, p)
#else
#define DEBUG_THIS(p)		 
#endif

static int
trailing_spaces(const char *src)
{
    while (*src == ' ')
	src++;
    return *src == 0;
}

 
#define REALPRINT(s) (UChar(*(s)) < 127 && isprint(UChar(*(s))))

#define P_LIMIT(p)   (length - (size_t)(p))

NCURSES_EXPORT(char *)
_nc_tic_expand(const char *srcp, bool tic_format, int numbers)
{
    static char *buffer;
    static size_t length;

    int bufp;
    const char *str = VALID_STRING(srcp) ? srcp : "\0\0";
    size_t need = (2 + strlen(str)) * 4;
    int ch;
    int octals = 0;
    struct {
	int ch;
	int offset;
    } fixups[MAX_TC_FIXUPS];

    if (srcp == 0) {
#if NO_LEAKS
	if (buffer != 0) {
	    FreeAndNull(buffer);
	    length = 0;
	}
#endif
	return 0;
    }
    if (buffer == 0 || need > length) {
	if ((buffer = typeRealloc(char, length = need, buffer)) == 0)
	      return 0;
    }

    DEBUG_THIS(("_nc_tic_expand %s:%s:%s",
		tic_format ? "ti" : "tc",
		numbers ? "#" : "",
		_nc_visbuf(srcp)));
    bufp = 0;
    while ((ch = UChar(*str)) != 0) {
	if (ch == '%' && REALPRINT(str + 1)) {
	    buffer[bufp++] = *str++;
	     
	    switch (numbers) {
	    case -1:
		if (str[0] == S_QUOTE
		    && str[1] != '\\'
		    && REALPRINT(str + 1)
		    && str[2] == S_QUOTE) {
		    _nc_SPRINTF(buffer + bufp, _nc_SLIMIT(P_LIMIT(bufp))
				"{%d}", str[1]);
		    bufp += (int) strlen(buffer + bufp);
		    str += 2;
		} else {
		    buffer[bufp++] = *str;
		}
		break;
		 
	    case 1:
		if (str[0] == L_BRACE
		    && isdigit(UChar(str[1]))) {
		    char *dst = 0;
		    long value = strtol(str + 1, &dst, 0);
		    if (dst != 0
			&& *dst == R_BRACE
			&& value < 127
			&& value != '\\'	 
			&& isprint((int) value)) {
			ch = (int) value;
			buffer[bufp++] = S_QUOTE;
			if (ch == '\\'
			    || ch == S_QUOTE)
			    buffer[bufp++] = '\\';
			buffer[bufp++] = (char) ch;
			buffer[bufp++] = S_QUOTE;
			str = dst;
		    } else {
			buffer[bufp++] = *str;
		    }
		} else {
		    buffer[bufp++] = *str;
		}
		break;
	    default:
		if (*str == ',')	 
		    buffer[bufp++] = '\\';
		buffer[bufp++] = *str;
		break;
	    }
	} else if (ch == 128) {
	    buffer[bufp++] = '\\';
	    buffer[bufp++] = '0';
	} else if (ch == '\033') {
	    buffer[bufp++] = '\\';
	    buffer[bufp++] = 'E';
	} else if (ch == '\\' && tic_format && (str == srcp || str[-1] != '^')) {
	    buffer[bufp++] = '\\';
	    buffer[bufp++] = '\\';
	} else if (ch == ' ' && tic_format && (str == srcp ||
					       trailing_spaces(str))) {
	    buffer[bufp++] = '\\';
	    buffer[bufp++] = 's';
	} else if ((ch == ',' || ch == '^') && tic_format) {
	    buffer[bufp++] = '\\';
	    buffer[bufp++] = (char) ch;
	} else if (REALPRINT(str)
		   && (ch != ','
		       && !(ch == ':' && !tic_format)
		       && !(ch == '!' && !tic_format)
		       && ch != '^'))
	    buffer[bufp++] = (char) ch;
	else if (ch == '\r') {
	    buffer[bufp++] = '\\';
	    buffer[bufp++] = 'r';
	} else if (ch == '\n') {
	    buffer[bufp++] = '\\';
	    buffer[bufp++] = 'n';
	}
#define UnCtl(c) ((c) + '@')
	else if (UChar(ch) < 32
		 && isdigit(UChar(str[1]))) {
	    _nc_SPRINTF(&buffer[bufp], _nc_SLIMIT(P_LIMIT(bufp))
			"^%c", UnCtl(ch));
	    bufp += 2;
	} else {
	    _nc_SPRINTF(&buffer[bufp], _nc_SLIMIT(P_LIMIT(bufp))
			"\\%03o", ch);
	    if ((octals < MAX_TC_FIXUPS) &&
		((tic_format && (ch == 127)) || ch < 32)) {
		fixups[octals].ch = UChar(ch);
		fixups[octals].offset = bufp;
		++octals;
	    }
	    bufp += 4;
	}

	str++;
    }

    buffer[bufp] = '\0';

     
    if (octals != 0 && (!tic_format || (bufp - (4 * octals)) < MIN_TC_FIXUPS)) {
	while (--octals >= 0) {
	    char *p = buffer + fixups[octals].offset;
	    *p++ = '^';
	    *p++ = (char) ((fixups[octals].ch == 127)
			   ? '?'
			   : (fixups[octals].ch + (int) '@'));
	    while ((p[0] = p[2]) != 0) {
		++p;
	    }
	}
    }
    DEBUG_THIS(("... %s", _nc_visbuf(buffer)));
    return (buffer);
}
