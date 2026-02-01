 

 

 

#include <curses.priv.h>

#include <ctype.h>
#include <tic.h>

MODULE_ID("$Id: comp_scan.c,v 1.119 2022/08/07 00:20:26 tom Exp $")

 
#define MAXCAPLEN	600

#define iswhite(ch)	(ch == ' '  ||  ch == '\t')

NCURSES_EXPORT_VAR (int) _nc_syntax = 0;          
NCURSES_EXPORT_VAR (int) _nc_strict_bsd = 1;   
NCURSES_EXPORT_VAR (long) _nc_curr_file_pos = 0;  
NCURSES_EXPORT_VAR (long) _nc_comment_start = 0;  
NCURSES_EXPORT_VAR (long) _nc_comment_end = 0;    
NCURSES_EXPORT_VAR (long) _nc_start_line = 0;     

NCURSES_EXPORT_VAR (struct token) _nc_curr_token =
{
    0, 0, 0
};

 

static bool first_column;	 
static bool had_newline;
static char separator;		 
static int pushtype;		 
static char *pushname;

#if NCURSES_EXT_FUNCS
NCURSES_EXPORT_VAR (bool) _nc_disable_period = FALSE;  
#endif

 

#define LEXBUFSIZ	1024

static char *bufptr;		 
static char *bufstart;		 
static FILE *yyin;		 

 

NCURSES_EXPORT(void)
_nc_reset_input(FILE *fp, char *buf)
{
    TR(TRACE_DATABASE,
       (T_CALLED("_nc_reset_input(fp=%p, buf=%p)"), (void *) fp, buf));

    pushtype = NO_PUSHBACK;
    if (pushname != 0)
	pushname[0] = '\0';
    yyin = fp;
    bufstart = bufptr = buf;
    _nc_curr_file_pos = 0L;
    if (fp != 0)
	_nc_curr_line = 0;
    _nc_curr_col = 0;

    returnVoidDB;
}

 
static int
last_char(int from_end)
{
    size_t len = strlen(bufptr);
    int result = 0;

    while (len--) {
	if (!isspace(UChar(bufptr[len]))) {
	    if (from_end <= (int) len)
		result = bufptr[(int) len - from_end];
	    break;
	}
    }
    return result;
}

 
static int
get_text(char *buffer, int length)
{
    int count = 0;
    int limit = length - 1;

    while (limit-- > 0) {
	int ch = fgetc(yyin);

	if (ch == '\0') {
	    _nc_err_abort("This is not a text-file");
	} else if (ch == EOF) {
	    break;
	}
	++count;
	*buffer++ = (char) ch;
	if (ch == '\n')
	    break;
    }
    *buffer = '\0';
    return count;
}

 

static int
next_char(void)
{
    static char *result;
    static size_t allocated;
    int the_char;

    if (!yyin) {
	if (result != 0) {
	    FreeAndNull(result);
	    FreeAndNull(pushname);
	    bufptr = 0;
	    bufstart = 0;
	    allocated = 0;
	}
	 
	if (bufptr == 0 || *bufptr == '\0')
	    return (EOF);
	if (*bufptr == '\n') {
	    _nc_curr_line++;
	    _nc_curr_col = 0;
	} else if (*bufptr == '\t') {
	    _nc_curr_col = (_nc_curr_col | 7);
	}
    } else if (!bufptr || !*bufptr) {
	 
	size_t len;

	do {
	    size_t used = 0;
	    bufstart = 0;
	    do {
		if (used + (LEXBUFSIZ / 4) >= allocated) {
		    allocated += (allocated + LEXBUFSIZ);
		    result = typeRealloc(char, allocated, result);
		    if (result == 0)
			return (EOF);
		    if (bufstart)
			bufstart = result;
		}
		if (used == 0)
		    _nc_curr_file_pos = ftell(yyin);

		if (get_text(result + used, (int) (allocated - used))) {
		    bufstart = result;
		    if (used == 0) {
			if (_nc_curr_line == 0
			    && IS_TIC_MAGIC(result)) {
			    _nc_err_abort("This is a compiled terminal description, not a source");
			}
			_nc_curr_line++;
			_nc_curr_col = 0;
		    }
		} else {
		    if (used != 0)
			_nc_STRCAT(result, "\n", allocated);
		}
		if ((bufptr = bufstart) != 0) {
		    used = strlen(bufptr);
		    if (used == 0)
			return (EOF);
		    while (iswhite(*bufptr)) {
			if (*bufptr == '\t') {
			    _nc_curr_col = (_nc_curr_col | 7) + 1;
			} else {
			    _nc_curr_col++;
			}
			bufptr++;
		    }

		     
		    if ((len = strlen(bufptr)) > 1) {
			if (bufptr[len - 1] == '\n'
			    && bufptr[len - 2] == '\r') {
			    len--;
			    bufptr[len - 1] = '\n';
			    bufptr[len] = '\0';
			}
		    }
		} else {
		    return (EOF);
		}
	    } while (bufptr[len - 1] != '\n');	 
	} while (result[0] == '#');	 
    } else if (*bufptr == '\t') {
	_nc_curr_col = (_nc_curr_col | 7);
    }

    first_column = (bufptr == bufstart);
    if (first_column)
	had_newline = FALSE;

    _nc_curr_col++;
    the_char = *bufptr++;
    return UChar(the_char);
}

static void
push_back(int c)
 
{
    if (bufptr == bufstart)
	_nc_syserr_abort("Can't backspace off beginning of line");
    *--bufptr = (char) c;
    _nc_curr_col--;
}

static long
stream_pos(void)
 
{
    return (yyin ? ftell(yyin) : (bufptr ? bufptr - bufstart : 0));
}

static bool
end_of_stream(void)
 
{
    return ((yyin
	     ? (feof(yyin) && (bufptr == NULL || *bufptr == '\0'))
	     : (bufptr && *bufptr == '\0'))
	    ? TRUE : FALSE);
}

 
static NCURSES_INLINE int
eat_escaped_newline(int ch)
{
    if (ch == '\\')
	while ((ch = next_char()) == '\n' || iswhite(ch))
	    continue;
    return ch;
}

#define TOK_BUF_SIZE MAX_ENTRY_SIZE

#define OkToAdd() \
	((tok_ptr - tok_buf) < (TOK_BUF_SIZE - 2))

#define AddCh(ch) \
	*tok_ptr++ = (char) ch; \
	*tok_ptr = '\0'

static char *tok_buf;

 

NCURSES_EXPORT(int)
_nc_get_token(bool silent)
{
    static const char terminfo_punct[] = "@%&*!#";

    char *after_name;		 
    char *after_list;		 
    char *numchk;
    char *tok_ptr;
    char *s;
    char numbuf[80];
    int ch, c0, c1;
    int dot_flag = FALSE;
    int type;
    long number;
    long token_start;
    unsigned found;
#ifdef TRACE
    int old_line;
    int old_col;
#endif

    DEBUG(3, (T_CALLED("_nc_get_token(silent=%d)"), silent));

    if (pushtype != NO_PUSHBACK) {
	int retval = pushtype;

	_nc_set_type(pushname != 0 ? pushname : "");
	DEBUG(3, ("pushed-back token: `%s', class %d",
		  _nc_curr_token.tk_name, pushtype));

	pushtype = NO_PUSHBACK;
	if (pushname != 0)
	    pushname[0] = '\0';

	 
	DEBUG(3, (T_RETURN("%d"), retval));
	return (retval);
    }

    if (end_of_stream()) {
	yyin = 0;
	(void) next_char();	 
	if (tok_buf != 0) {
	    if (_nc_curr_token.tk_name == tok_buf)
		_nc_curr_token.tk_name = 0;
	}
	DEBUG(3, (T_RETURN("%d"), EOF));
	return (EOF);
    }

  start_token:
    token_start = stream_pos();
    while ((ch = next_char()) == '\n' || iswhite(ch)) {
	if (ch == '\n')
	    had_newline = TRUE;
	continue;
    }

    ch = eat_escaped_newline(ch);
    _nc_curr_token.tk_valstring = 0;

#ifdef TRACE
    old_line = _nc_curr_line;
    old_col = _nc_curr_col;
#endif
    if (ch == EOF)
	type = EOF;
    else {
	 
	if (separator == ':' && ch == ':')
	    ch = next_char();

	if (ch == '.'
#if NCURSES_EXT_FUNCS
	    && !_nc_disable_period
#endif
	    ) {
	    dot_flag = TRUE;
	    DEBUG(8, ("dot-flag set"));

	    while ((ch = next_char()) == '.' || iswhite(ch))
		continue;
	}

	if (ch == EOF) {
	    type = EOF;
	    goto end_of_token;
	}

	 
	if (!isalnum(UChar(ch))
#if NCURSES_EXT_FUNCS
	    && !(ch == '.' && _nc_disable_period)
#endif
	    && ((strchr) (terminfo_punct, (char) ch) == 0)) {
	    if (!silent)
		_nc_warning("Illegal character (expected alphanumeric or %s) - '%s'",
			    terminfo_punct, unctrl(UChar(ch)));
	    _nc_panic_mode(separator);
	    goto start_token;
	}

	if (tok_buf == 0)
	    tok_buf = typeMalloc(char, TOK_BUF_SIZE);

#ifdef TRACE
	old_line = _nc_curr_line;
	old_col = _nc_curr_col;
#endif
	tok_ptr = tok_buf;
	AddCh(ch);

	if (first_column) {
	    _nc_comment_start = token_start;
	    _nc_comment_end = _nc_curr_file_pos;
	    _nc_start_line = _nc_curr_line;

	    _nc_syntax = ERR;
	    after_name = 0;
	    after_list = 0;
	    while ((ch = next_char()) != '\n') {
		if (ch == EOF) {
		    _nc_err_abort(MSG_NO_INPUTS);
		} else if (ch == '|') {
		    after_list = tok_ptr;
		    if (after_name == 0)
			after_name = tok_ptr;
		} else if (ch == ':' && last_char(0) != ',') {
		    _nc_syntax = SYN_TERMCAP;
		    separator = ':';
		    break;
		} else if (ch == ',') {
		    _nc_syntax = SYN_TERMINFO;
		    separator = ',';
		     
		    if (after_name == 0)
			break;
		     
		    c0 = last_char(0);
		    c1 = last_char(1);
		    if (c1 != ':' && c0 != '\\' && c0 != ':') {
			bool capability = FALSE;

			 
			for (s = bufptr; isspace(UChar(*s)); ++s) {
			    ;
			}
			if (islower(UChar(*s))) {
			    char *name = s;
			    while (isalnum(UChar(*s))) {
				++s;
			    }
			    if (*s == '#' || *s == '=' || *s == '@') {
				 
				capability = TRUE;
			    } else if (*s == ',') {
				c0 = *s;
				*s = '\0';
				 
				if (_nc_find_entry(name,
						   _nc_get_hash_table(FALSE))) {
				    capability = TRUE;
				}
				*s = (char) c0;
			    }
			}
			if (capability) {
			    break;
			}
		    }
		} else
		    ch = eat_escaped_newline(ch);

		if (OkToAdd()) {
		    AddCh(ch);
		} else {
		    break;
		}
	    }
	    *tok_ptr = '\0';
	    if (_nc_syntax == ERR) {
		 
		_nc_syntax = SYN_TERMCAP;
		separator = ':';
	    } else if (_nc_syntax == SYN_TERMINFO) {
		 
		for (--tok_ptr;
		     iswhite(*tok_ptr) || *tok_ptr == ',';
		     tok_ptr--)
		    continue;
		tok_ptr[1] = '\0';
	    }

	     
	    if (after_name != 0) {
		ch = *after_name;
		*after_name = '\0';
		_nc_set_type(tok_buf);
		*after_name = (char) ch;
	    }

	     
	    if (after_list != 0) {
		if (!silent) {
		    if (*after_list == '\0' || strchr("|", after_list[1]) != NULL) {
			_nc_warning("empty longname field");
		    } else if (strchr(after_list, ' ') == 0) {
			_nc_warning("older tic versions may treat the description field as an alias");
		    }
		}
	    } else {
		after_list = tok_buf + strlen(tok_buf);
		DEBUG(2, ("missing description"));
	    }

	     
	    for (s = tok_buf; s < after_list; ++s) {
		if (isspace(UChar(*s))) {
		    if (!silent)
			_nc_warning("whitespace in name or alias field");
		    break;
		} else if (*s == '/') {
		    if (!silent)
			_nc_warning("slashes aren't allowed in names or aliases");
		    break;
		} else if (strchr("$[]!*?", *s)) {
		    if (!silent)
			_nc_warning("dubious character `%c' in name or alias field", *s);
		    break;
		}
	    }

	    _nc_curr_token.tk_name = tok_buf;
	    type = NAMES;
	} else {
	    if (had_newline && _nc_syntax == SYN_TERMCAP) {
		_nc_warning("Missing backslash before newline");
		had_newline = FALSE;
	    }
	    while ((ch = next_char()) != EOF) {
		if (!isalnum(UChar(ch))) {
		    if (_nc_syntax == SYN_TERMINFO) {
			if (ch != '_')
			    break;
		    } else {	 
			if (ch != ';')
			    break;
		    }
		}
		if (OkToAdd()) {
		    AddCh(ch);
		} else {
		    ch = EOF;
		    break;
		}
	    }

	    *tok_ptr++ = '\0';	 
	    switch (ch) {
	    case ',':
	    case ':':
		if (ch != separator)
		    _nc_err_abort("Separator inconsistent with syntax");
		_nc_curr_token.tk_name = tok_buf;
		type = BOOLEAN;
		break;
	    case '@':
		if ((ch = next_char()) != separator && !silent)
		    _nc_warning("Missing separator after `%s', have %s",
				tok_buf, unctrl(UChar(ch)));
		_nc_curr_token.tk_name = tok_buf;
		type = CANCEL;
		break;

	    case '#':
		found = 0;
		while (isalnum(ch = next_char())) {
		    numbuf[found++] = (char) ch;
		    if (found >= sizeof(numbuf) - 1)
			break;
		}
		numbuf[found] = '\0';
		number = strtol(numbuf, &numchk, 0);
		if (!silent) {
		    if (numchk == numbuf)
			_nc_warning("no value given for `%s'", tok_buf);
		    if ((*numchk != '\0') || (ch != separator))
			_nc_warning("Missing separator for `%s'", tok_buf);
		    if (number < 0)
			_nc_warning("value of `%s' cannot be negative", tok_buf);
		    if (number > MAX_OF_TYPE(NCURSES_INT2)) {
			_nc_warning("limiting value of `%s' from %#lx to %#x",
				    tok_buf,
				    number, MAX_OF_TYPE(NCURSES_INT2));
			number = MAX_OF_TYPE(NCURSES_INT2);
		    }
		}
		_nc_curr_token.tk_name = tok_buf;
		_nc_curr_token.tk_valnumber = (int) number;
		type = NUMBER;
		break;

	    case '=':
		ch = _nc_trans_string(tok_ptr, tok_buf + TOK_BUF_SIZE);
		if (!silent && ch != separator)
		    _nc_warning("Missing separator");
		_nc_curr_token.tk_name = tok_buf;
		_nc_curr_token.tk_valstring = tok_ptr;
		type = STRING;
		break;

	    case EOF:
		type = EOF;
		break;
	    default:
		 
		type = UNDEF;
		if (!silent)
		    _nc_warning("Illegal character - '%s'", unctrl(UChar(ch)));
	    }
	}			 
    }				 

  end_of_token:

#ifdef TRACE
    if (dot_flag == TRUE)
	DEBUG(8, ("Commented out "));

    if (_nc_tracing >= DEBUG_LEVEL(8)) {
	_tracef("parsed %d.%d to %d.%d",
		old_line, old_col,
		_nc_curr_line, _nc_curr_col);
    }
    if (_nc_tracing >= DEBUG_LEVEL(7)) {
	switch (type) {
	case BOOLEAN:
	    _tracef("Token: Boolean; name='%s'",
		    _nc_curr_token.tk_name);
	    break;

	case NUMBER:
	    _tracef("Token: Number;  name='%s', value=%d",
		    _nc_curr_token.tk_name,
		    _nc_curr_token.tk_valnumber);
	    break;

	case STRING:
	    _tracef("Token: String;  name='%s', value=%s",
		    _nc_curr_token.tk_name,
		    _nc_visbuf(_nc_curr_token.tk_valstring));
	    break;

	case CANCEL:
	    _tracef("Token: Cancel; name='%s'",
		    _nc_curr_token.tk_name);
	    break;

	case NAMES:

	    _tracef("Token: Names; value='%s'",
		    _nc_curr_token.tk_name);
	    break;

	case EOF:
	    _tracef("Token: End of file");
	    break;

	default:
	    _nc_warning("Bad token type");
	}
    }
#endif

    if (dot_flag == TRUE)	 
	type = _nc_get_token(silent);

    DEBUG(3, ("token: `%s', class %d",
	      ((_nc_curr_token.tk_name != 0)
	       ? _nc_curr_token.tk_name
	       : "<null>"),
	      type));

    DEBUG(3, (T_RETURN("%d"), type));
    return (type);
}

 

NCURSES_EXPORT(int)
_nc_trans_string(char *ptr, char *last)
{
    int count = 0;
    int number = 0;
    int i, c;
    int last_ch = '\0';
    bool ignored = FALSE;
    bool long_warning = FALSE;

    while ((c = next_char()) != separator && c != EOF) {
	if (ptr >= (last - 1)) {
	    if (c != EOF) {
		while ((c = next_char()) != separator && c != EOF) {
		    ;
		}
	    }
	    break;
	}
	if ((_nc_syntax == SYN_TERMCAP) && c == '\n')
	    break;
	if (c == '^' && last_ch != '%') {
	    c = next_char();
	    if (c == EOF)
		_nc_err_abort(MSG_NO_INPUTS);

	    if (!(is7bits(c) && isprint(c))) {
		_nc_warning("Illegal ^ character - '%s'", unctrl(UChar(c)));
	    }
	    if (c == '?' && (_nc_syntax != SYN_TERMCAP)) {
		*(ptr++) = '\177';
	    } else {
		if ((c &= 037) == 0)
		    c = 128;
		*(ptr++) = (char) (c);
	    }
	} else if (c == '\\') {
	    bool strict_bsd = ((_nc_syntax == SYN_TERMCAP) && _nc_strict_bsd);

	    c = next_char();
	    if (c == EOF)
		_nc_err_abort(MSG_NO_INPUTS);

	    if (isoctal(c) || (strict_bsd && isdigit(c))) {
		number = c - '0';
		for (i = 0; i < 2; i++) {
		    c = next_char();
		    if (c == EOF)
			_nc_err_abort(MSG_NO_INPUTS);

		    if (!isoctal(c)) {
			if (isdigit(c)) {
			    if (!strict_bsd) {
				_nc_warning("Non-octal digit `%c' in \\ sequence", c);
				 
			    }
			} else {
			    push_back(c);
			    break;
			}
		    }

		    number = number * 8 + c - '0';
		}

		number = UChar(number);
		if (number == 0 && !strict_bsd)
		    number = 0200;
		*(ptr++) = (char) number;
	    } else {
		switch (c) {
		case 'E':
		    *(ptr++) = '\033';
		    break;

		case 'n':
		    *(ptr++) = '\n';
		    break;

		case 'r':
		    *(ptr++) = '\r';
		    break;

		case 'b':
		    *(ptr++) = '\010';
		    break;

		case 'f':
		    *(ptr++) = '\014';
		    break;

		case 't':
		    *(ptr++) = '\t';
		    break;

		case '\\':
		    *(ptr++) = '\\';
		    break;

		case '^':
		    *(ptr++) = '^';
		    break;

		case ',':
		    *(ptr++) = ',';
		    break;

		case '\n':
		    continue;

		default:
		    if ((_nc_syntax == SYN_TERMINFO) || !_nc_strict_bsd) {
			switch (c) {
			case 'a':
			    c = '\007';
			    break;
			case 'e':
			    c = '\033';
			    break;
			case 'l':
			    c = '\n';
			    break;
			case 's':
			    c = ' ';
			    break;
			case ':':
			    c = ':';
			    break;
			default:
			    _nc_warning("Illegal character '%s' in \\ sequence",
					unctrl(UChar(c)));
			    break;
			}
		    }
		     
		case '|':
		    *(ptr++) = (char) c;
		}		 
	    }			 
	}
	 
	else if (c == '\n' && (_nc_syntax == SYN_TERMINFO)) {
	     
	    ignored = TRUE;
	} else {
	    *(ptr++) = (char) c;
	}

	if (!ignored) {
	    if (_nc_curr_col <= 1) {
		push_back(c);
		c = '\n';
		break;
	    }
	    last_ch = c;
	    count++;
	}
	ignored = FALSE;

	if (count > MAXCAPLEN && !long_warning) {
	    _nc_warning("Very long string found.  Missing separator?");
	    long_warning = TRUE;
	}
    }				 

    *ptr = '\0';

    return (c);
}

 

NCURSES_EXPORT(void)
_nc_push_token(int tokclass)
{
     
    pushtype = tokclass;
    if (pushname == 0)
	pushname = typeMalloc(char, MAX_NAME_SIZE + 1);
    _nc_get_type(pushname);

    DEBUG(3, ("pushing token: `%s', class %d",
	      ((_nc_curr_token.tk_name != 0)
	       ? _nc_curr_token.tk_name
	       : "<null>"),
	      pushtype));
}

 
NCURSES_EXPORT(void)
_nc_panic_mode(char ch)
{
    for (;;) {
	int c = next_char();
	if (c == ch)
	    return;
	if (c == EOF)
	    return;
    }
}

#if NO_LEAKS
NCURSES_EXPORT(void)
_nc_comp_scan_leaks(void)
{
    if (pushname != 0) {
	FreeAndNull(pushname);
    }
    if (tok_buf != 0) {
	FreeAndNull(tok_buf);
    }
}
#endif
