 

 

 

#define __INTERNAL_CAPS_VISIBLE
#include <curses.priv.h>

#include <ctype.h>
#include <tic.h>

MODULE_ID("$Id: parse_entry.c,v 1.107 2022/05/08 00:11:44 tom Exp $")

#ifdef LINT
static short const parametrized[] =
{0};
#else
#include <parametrized.h>
#endif

static void postprocess_termcap(TERMTYPE2 *, bool);
static void postprocess_terminfo(TERMTYPE2 *);
static struct name_table_entry const *lookup_fullname(const char *name);

#if NCURSES_XNAMES

static struct name_table_entry const *
_nc_extend_names(ENTRY * entryp, const char *name, int token_type)
{
    static struct name_table_entry temp;
    TERMTYPE2 *tp = &(entryp->tterm);
    unsigned offset = 0;
    unsigned actual;
    unsigned tindex;
    unsigned first, last, n;
    bool found;

    switch (token_type) {
    case BOOLEAN:
	first = 0;
	last = tp->ext_Booleans;
	offset = tp->ext_Booleans;
	tindex = tp->num_Booleans;
	break;
    case NUMBER:
	first = tp->ext_Booleans;
	last = tp->ext_Numbers + first;
	offset = (unsigned) (tp->ext_Booleans + tp->ext_Numbers);
	tindex = tp->num_Numbers;
	break;
    case STRING:
	first = (unsigned) (tp->ext_Booleans + tp->ext_Numbers);
	last = tp->ext_Strings + first;
	offset = (unsigned) (tp->ext_Booleans + tp->ext_Numbers + tp->ext_Strings);
	tindex = tp->num_Strings;
	break;
    case CANCEL:
	actual = NUM_EXT_NAMES(tp);
	for (n = 0; n < actual; n++) {
	    if (!strcmp(name, tp->ext_Names[n])) {
		if (n > (unsigned) (tp->ext_Booleans + tp->ext_Numbers)) {
		    token_type = STRING;
		} else if (n > tp->ext_Booleans) {
		    token_type = NUMBER;
		} else {
		    token_type = BOOLEAN;
		}
		return _nc_extend_names(entryp, name, token_type);
	    }
	}
	 
	return _nc_extend_names(entryp, name, STRING);
    default:
	return 0;
    }

     
    for (n = first, found = FALSE; n < last; n++) {
	int cmp = strcmp(tp->ext_Names[n], name);
	if (cmp == 0)
	    found = TRUE;
	if (cmp >= 0) {
	    offset = n;
	    tindex = n - first;
	    switch (token_type) {
	    case BOOLEAN:
		tindex += BOOLCOUNT;
		break;
	    case NUMBER:
		tindex += NUMCOUNT;
		break;
	    case STRING:
		tindex += STRCOUNT;
		break;
	    }
	    break;
	}
    }

#define for_each_value(max) \
	for (last = (unsigned) (max - 1); last > tindex; last--)

    if (!found) {
	switch (token_type) {
	case BOOLEAN:
	    tp->ext_Booleans++;
	    tp->num_Booleans++;
	    TYPE_REALLOC(NCURSES_SBOOL, tp->num_Booleans, tp->Booleans);
	    for_each_value(tp->num_Booleans)
		tp->Booleans[last] = tp->Booleans[last - 1];
	    break;
	case NUMBER:
	    tp->ext_Numbers++;
	    tp->num_Numbers++;
	    TYPE_REALLOC(NCURSES_INT2, tp->num_Numbers, tp->Numbers);
	    for_each_value(tp->num_Numbers)
		tp->Numbers[last] = tp->Numbers[last - 1];
	    break;
	case STRING:
	    tp->ext_Strings++;
	    tp->num_Strings++;
	    TYPE_REALLOC(char *, tp->num_Strings, tp->Strings);
	    for_each_value(tp->num_Strings)
		tp->Strings[last] = tp->Strings[last - 1];
	    break;
	}
	actual = NUM_EXT_NAMES(tp);
	TYPE_REALLOC(char *, actual, tp->ext_Names);
	while (--actual > offset)
	    tp->ext_Names[actual] = tp->ext_Names[actual - 1];
	tp->ext_Names[offset] = _nc_save_str(name);
    }

    temp.nte_name = tp->ext_Names[offset];
    temp.nte_type = token_type;
    temp.nte_index = (short) tindex;
    temp.nte_link = -1;

    return &temp;
}

static const char *
usertype2s(int mask)
{
    const char *result = "unknown";
    if (mask & (1 << BOOLEAN)) {
	result = "boolean";
    } else if (mask & (1 << NUMBER)) {
	result = "number";
    } else if (mask & (1 << STRING)) {
	result = "string";
    }
    return result;
}

static bool
expected_type(const char *name, int token_type, bool silent)
{
    struct user_table_entry const *entry = _nc_find_user_entry(name);
    bool result = TRUE;
    if ((entry != 0) && (token_type != CANCEL)) {
	int have_type = (1 << token_type);
	if (!(entry->ute_type & have_type)) {
	    if (!silent)
		_nc_warning("expected %s-type for %s, have %s",
			    usertype2s(entry->ute_type),
			    name,
			    usertype2s(have_type));
	    result = FALSE;
	}
    }
    return result;
}
#endif  

 
static bool
valid_entryname(const char *name)
{
    bool result = TRUE;
    bool first = TRUE;
    int ch;
    while ((ch = UChar(*name++)) != '\0') {
	if (ch <= ' ' || ch > '~' || strchr("/\\|=,:", ch) != NULL) {
	    result = FALSE;
	    break;
	}
	if (!first && strchr("#@", ch) != NULL) {
	    result = FALSE;
	    break;
	}
	first = FALSE;
    }
    return result;
}

 

#define BAD_TC_USAGE if (!bad_tc_usage) \
 	{ bad_tc_usage = TRUE; \
	 _nc_warning("Legacy termcap allows only a trailing tc= clause"); }

#define MAX_NUMBER MAX_OF_TYPE(NCURSES_INT2)

NCURSES_EXPORT(int)
_nc_parse_entry(ENTRY * entryp, int literal, bool silent)
{
    int token_type;
    struct name_table_entry const *entry_ptr;
    char *ptr, *base;
    const char *name;
    bool bad_tc_usage = FALSE;

    TR(TRACE_DATABASE,
       (T_CALLED("_nc_parse_entry(entry=%p, literal=%d, silent=%d)"),
	(void *) entryp, literal, silent));

    token_type = _nc_get_token(silent);

    if (token_type == EOF)
	returnDB(EOF);
    if (token_type != NAMES)
	_nc_err_abort("Entry does not start with terminal names in column one");

    _nc_init_entry(entryp);

    entryp->cstart = _nc_comment_start;
    entryp->cend = _nc_comment_end;
    entryp->startline = _nc_start_line;
    DEBUG(2, ("Comment range is %ld to %ld", entryp->cstart, entryp->cend));

     
#define ok_TC2(s) (isgraph(UChar(s)) && (s) != '|')
    ptr = _nc_curr_token.tk_name;
    if (_nc_syntax == SYN_TERMCAP
#if NCURSES_XNAMES
	&& !_nc_user_definable
#endif
	) {
	if (ok_TC2(ptr[0]) && ok_TC2(ptr[1]) && (ptr[2] == '|')) {
	    ptr += 3;
	    _nc_curr_token.tk_name[2] = '\0';
	}
    }

    entryp->tterm.str_table = entryp->tterm.term_names = _nc_save_str(ptr);

    if (entryp->tterm.str_table == 0)
	returnDB(ERR);

    DEBUG(2, ("Starting '%s'", ptr));

     
    name = _nc_first_name(entryp->tterm.term_names);
    if (!valid_entryname(name)) {
	_nc_warning("invalid entry name \"%s\"", name);
	name = "invalid";
    }
    _nc_set_type(name);

     
    for (base = entryp->tterm.term_names; (ptr = strchr(base, '|')) != 0;
	 base = ptr + 1) {
	if (ptr - base > MAX_ALIAS) {
	    _nc_warning("%s `%.*s' may be too long",
			(base == entryp->tterm.term_names)
			? "primary name"
			: "alias",
			(int) (ptr - base), base);
	}
    }

    entryp->nuses = 0;

    for (token_type = _nc_get_token(silent);
	 token_type != EOF && token_type != NAMES;
	 token_type = _nc_get_token(silent)) {
	bool is_use = (strcmp(_nc_curr_token.tk_name, "use") == 0);
	bool is_tc = !is_use && (strcmp(_nc_curr_token.tk_name, "tc") == 0);
	if (is_use || is_tc) {
	    if (!VALID_STRING(_nc_curr_token.tk_valstring)
		|| _nc_curr_token.tk_valstring[0] == '\0') {
		_nc_warning("missing name for use-clause");
		continue;
	    } else if (!valid_entryname(_nc_curr_token.tk_valstring)) {
		_nc_warning("invalid name for use-clause \"%s\"",
			    _nc_curr_token.tk_valstring);
		continue;
	    } else if (entryp->nuses >= MAX_USES) {
		_nc_warning("too many use-clauses, ignored \"%s\"",
			    _nc_curr_token.tk_valstring);
		continue;
	    }
	    entryp->uses[entryp->nuses].name = _nc_save_str(_nc_curr_token.tk_valstring);
	    entryp->uses[entryp->nuses].line = _nc_curr_line;
	    entryp->nuses++;
	    if (entryp->nuses > 1 && is_tc) {
		BAD_TC_USAGE
	    }
	} else {
	     
	    entry_ptr = _nc_find_entry(_nc_curr_token.tk_name,
				       _nc_get_hash_table(_nc_syntax));

	     
	    if (entry_ptr == NOTFOUND) {
		const struct alias *ap;

		if (_nc_syntax == SYN_TERMCAP) {
		    if (entryp->nuses != 0) {
			BAD_TC_USAGE
		    }
		    for (ap = _nc_get_alias_table(TRUE); ap->from; ap++)
			if (strcmp(ap->from, _nc_curr_token.tk_name) == 0) {
			    if (ap->to == (char *) 0) {
				_nc_warning("%s (%s termcap extension) ignored",
					    ap->from, ap->source);
				goto nexttok;
			    }

			    entry_ptr = _nc_find_entry(ap->to,
						       _nc_get_hash_table(TRUE));
			    if (entry_ptr && !silent)
				_nc_warning("%s (%s termcap extension) aliased to %s",
					    ap->from, ap->source, ap->to);
			    break;
			}
		} else {	 
		    for (ap = _nc_get_alias_table(FALSE); ap->from; ap++)
			if (strcmp(ap->from, _nc_curr_token.tk_name) == 0) {
			    if (ap->to == (char *) 0) {
				_nc_warning("%s (%s terminfo extension) ignored",
					    ap->from, ap->source);
				goto nexttok;
			    }

			    entry_ptr = _nc_find_entry(ap->to,
						       _nc_get_hash_table(FALSE));
			    if (entry_ptr && !silent)
				_nc_warning("%s (%s terminfo extension) aliased to %s",
					    ap->from, ap->source, ap->to);
			    break;
			}

		    if (entry_ptr == NOTFOUND) {
			entry_ptr = lookup_fullname(_nc_curr_token.tk_name);
		    }
		}
	    }
#if NCURSES_XNAMES
	     
	    if (entry_ptr == NOTFOUND
		&& _nc_user_definable) {
		if (expected_type(_nc_curr_token.tk_name, token_type, silent)) {
		    if ((entry_ptr = _nc_extend_names(entryp,
						      _nc_curr_token.tk_name,
						      token_type)) != 0) {
			if (_nc_tracing >= DEBUG_LEVEL(1)) {
			    _nc_warning("extended capability '%s'",
					_nc_curr_token.tk_name);
			}
		    }
		} else {
		     
		    continue;
		}
	    }
#endif  

	     
	    if (entry_ptr == NOTFOUND) {
		if (!silent)
		    _nc_warning("unknown capability '%s'",
				_nc_curr_token.tk_name);
		continue;
	    }

	     
	    if (token_type == CANCEL) {
		 
		if (!strcmp("ma", _nc_curr_token.tk_name)) {
		    entry_ptr = _nc_find_type_entry("ma", NUMBER,
						    _nc_syntax != 0);
		    assert(entry_ptr != 0);
		}
	    } else if (entry_ptr->nte_type != token_type) {
		 

		if (token_type == NUMBER
		    && !strcmp("ma", _nc_curr_token.tk_name)) {
		     
		    entry_ptr = _nc_find_type_entry("ma", NUMBER,
						    _nc_syntax != 0);
		    assert(entry_ptr != 0);

		} else if (token_type == STRING
			   && !strcmp("MT", _nc_curr_token.tk_name)) {
		     
		    entry_ptr = _nc_find_type_entry("MT", STRING,
						    _nc_syntax != 0);
		    assert(entry_ptr != 0);

		} else if (token_type == BOOLEAN
			   && entry_ptr->nte_type == STRING) {
		     
		    token_type = STRING;
		} else {
		     
		    if (!silent) {
			const char *type_name;
			switch (entry_ptr->nte_type) {
			case BOOLEAN:
			    type_name = "boolean";
			    break;
			case STRING:
			    type_name = "string";
			    break;
			case NUMBER:
			    type_name = "numeric";
			    break;
			default:
			    type_name = "unknown";
			    break;
			}
			_nc_warning("wrong type used for %s capability '%s'",
				    type_name, _nc_curr_token.tk_name);
		    }
		    continue;
		}
	    }

	     
	    switch (token_type) {
	    case CANCEL:
		switch (entry_ptr->nte_type) {
		case BOOLEAN:
		    entryp->tterm.Booleans[entry_ptr->nte_index] = CANCELLED_BOOLEAN;
		    break;

		case NUMBER:
		    entryp->tterm.Numbers[entry_ptr->nte_index] = CANCELLED_NUMERIC;
		    break;

		case STRING:
		    entryp->tterm.Strings[entry_ptr->nte_index] = CANCELLED_STRING;
		    break;
		}
		break;

	    case BOOLEAN:
		entryp->tterm.Booleans[entry_ptr->nte_index] = TRUE;
		break;

	    case NUMBER:
#if !NCURSES_EXT_NUMBERS
		if (_nc_curr_token.tk_valnumber > MAX_NUMBER) {
		    entryp->tterm.Numbers[entry_ptr->nte_index] = MAX_NUMBER;
		} else
#endif
		{
		    entryp->tterm.Numbers[entry_ptr->nte_index] =
			(NCURSES_INT2) _nc_curr_token.tk_valnumber;
		}
		break;

	    case STRING:
		ptr = _nc_curr_token.tk_valstring;
		if (_nc_syntax == SYN_TERMCAP) {
		    int n = entry_ptr->nte_index;
		    ptr = _nc_captoinfo(_nc_curr_token.tk_name,
					ptr,
					(n < (int) SIZEOF(parametrized))
					? parametrized[n]
					: 0);
		}
		entryp->tterm.Strings[entry_ptr->nte_index] = _nc_save_str(ptr);
		break;

	    default:
		if (!silent)
		    _nc_warning("unknown token type");
		_nc_panic_mode((char) ((_nc_syntax == SYN_TERMCAP) ? ':' : ','));
		continue;
	    }
	}			 
      nexttok:
	continue;		 
    }				 

    _nc_push_token(token_type);
    _nc_set_type(_nc_first_name(entryp->tterm.term_names));

     
    if (!literal) {
	if (_nc_syntax == SYN_TERMCAP) {
	    bool has_base_entry = FALSE;

	     
	    if (strchr(entryp->tterm.term_names, '+')) {
		has_base_entry = TRUE;
	    } else {
		unsigned i;
		 
		for (i = 0; i < entryp->nuses; i++) {
		    if (entryp->uses[i].name != 0
			&& !strchr(entryp->uses[i].name, '+'))
			has_base_entry = TRUE;
		}
	    }

	    postprocess_termcap(&entryp->tterm, has_base_entry);
	} else
	    postprocess_terminfo(&entryp->tterm);
    }
    _nc_wrap_entry(entryp, FALSE);

    returnDB(OK);
}

NCURSES_EXPORT(int)
_nc_capcmp(const char *s, const char *t)
 
{
    bool ok_s = VALID_STRING(s);
    bool ok_t = VALID_STRING(t);

    if (ok_s && ok_t) {
	for (;;) {
	    if (s[0] == '$' && s[1] == '<') {
		for (s += 2;; s++) {
		    if (!(isdigit(UChar(*s))
			  || *s == '.'
			  || *s == '*'
			  || *s == '/'
			  || *s == '>')) {
			break;
		    }
		}
	    }

	    if (t[0] == '$' && t[1] == '<') {
		for (t += 2;; t++) {
		    if (!(isdigit(UChar(*t))
			  || *t == '.'
			  || *t == '*'
			  || *t == '/'
			  || *t == '>')) {
			break;
		    }
		}
	    }

	     

	    if (*s == '\0' && *t == '\0')
		return (0);

	    if (*s != *t)
		return (*t - *s);

	     
	    s++, t++;
	}
    } else if (ok_s || ok_t) {
	return 1;
    }
    return 0;
}

static void
append_acs0(string_desc * dst, int code, char *src, size_t off)
{
    if (src != 0 && off < strlen(src)) {
	char temp[3];
	temp[0] = (char) code;
	temp[1] = src[off];
	temp[2] = 0;
	_nc_safe_strcat(dst, temp);
    }
}

static void
append_acs(string_desc * dst, int code, char *src)
{
    if (VALID_STRING(src) && strlen(src) == 1) {
	append_acs0(dst, code, src, 0);
    }
}

 
#define DATA(from, to) { { from }, { to } }
typedef struct {
    const char from[3];
    const char to[6];
} assoc;
static assoc const ko_xlate[] =
{
    DATA("al", "kil1"),		 
    DATA("bt", "kcbt"),		 
    DATA("cd", "ked"),		 
    DATA("ce", "kel"),		 
    DATA("cl", "kclr"),		 
    DATA("ct", "tbc"),		 
    DATA("dc", "kdch1"),	 
    DATA("dl", "kdl1"),		 
    DATA("do", "kcud1"),	 
    DATA("ei", "krmir"),	 
    DATA("ho", "khome"),	 
    DATA("ic", "kich1"),	 
    DATA("im", "kIC"),		 
    DATA("le", "kcub1"),	 
    DATA("nd", "kcuf1"),	 
    DATA("nl", "kent"),		 
    DATA("st", "khts"),		 
    DATA("ta", ""),
    DATA("up", "kcuu1"),	 
};

 

static const char C_CR[] = "\r";
static const char C_LF[] = "\n";
static const char C_BS[] = "\b";
static const char C_HT[] = "\t";

 

#undef CUR
#define CUR tp->

static void
postprocess_termcap(TERMTYPE2 *tp, bool has_base)
{
    char buf[MAX_LINE * 2 + 2];
    string_desc result;

    TR(TRACE_DATABASE,
       (T_CALLED("postprocess_termcap(tp=%p, has_base=%d)"),
	(void *) tp, has_base));

     

     
    if (!has_base) {
	if (WANTED(init_3string) && PRESENT(termcap_init2))
	    init_3string = _nc_save_str(termcap_init2);

	if (WANTED(reset_2string) && PRESENT(termcap_reset))
	    reset_2string = _nc_save_str(termcap_reset);

	if (WANTED(carriage_return)) {
	    if (carriage_return_delay > 0) {
		_nc_SPRINTF(buf, _nc_SLIMIT(sizeof(buf))
			    "%s$<%d>", C_CR, carriage_return_delay);
		carriage_return = _nc_save_str(buf);
	    } else
		carriage_return = _nc_save_str(C_CR);
	}
	if (WANTED(cursor_left)) {
	    if (backspace_delay > 0) {
		_nc_SPRINTF(buf, _nc_SLIMIT(sizeof(buf))
			    "%s$<%d>", C_BS, backspace_delay);
		cursor_left = _nc_save_str(buf);
	    } else if (backspaces_with_bs == 1)
		cursor_left = _nc_save_str(C_BS);
	    else if (PRESENT(backspace_if_not_bs))
		cursor_left = backspace_if_not_bs;
	}
	 
	if (WANTED(cursor_down)) {
	    if (PRESENT(linefeed_if_not_lf))
		cursor_down = linefeed_if_not_lf;
	    else if (linefeed_is_newline != 1) {
		if (new_line_delay > 0) {
		    _nc_SPRINTF(buf, _nc_SLIMIT(sizeof(buf))
				"%s$<%d>", C_LF, new_line_delay);
		    cursor_down = _nc_save_str(buf);
		} else
		    cursor_down = _nc_save_str(C_LF);
	    }
	}
	if (WANTED(scroll_forward) && crt_no_scrolling != 1) {
	    if (PRESENT(linefeed_if_not_lf))
		cursor_down = linefeed_if_not_lf;
	    else if (linefeed_is_newline != 1) {
		if (new_line_delay > 0) {
		    _nc_SPRINTF(buf, _nc_SLIMIT(sizeof(buf))
				"%s$<%d>", C_LF, new_line_delay);
		    scroll_forward = _nc_save_str(buf);
		} else
		    scroll_forward = _nc_save_str(C_LF);
	    }
	}
	if (WANTED(newline)) {
	    if (linefeed_is_newline == 1) {
		if (new_line_delay > 0) {
		    _nc_SPRINTF(buf, _nc_SLIMIT(sizeof(buf))
				"%s$<%d>", C_LF, new_line_delay);
		    newline = _nc_save_str(buf);
		} else
		    newline = _nc_save_str(C_LF);
	    } else if (PRESENT(carriage_return) && PRESENT(scroll_forward)) {
		_nc_str_init(&result, buf, sizeof(buf));
		if (_nc_safe_strcat(&result, carriage_return)
		    && _nc_safe_strcat(&result, scroll_forward))
		    newline = _nc_save_str(buf);
	    } else if (PRESENT(carriage_return) && PRESENT(cursor_down)) {
		_nc_str_init(&result, buf, sizeof(buf));
		if (_nc_safe_strcat(&result, carriage_return)
		    && _nc_safe_strcat(&result, cursor_down))
		    newline = _nc_save_str(buf);
	    }
	}
    }

     

    if (!has_base) {
	 
	if (return_does_clr_eol == 1 || no_correctly_working_cr == 1)
	    carriage_return = ABSENT_STRING;

	 
	if (WANTED(tab)) {
	    if (horizontal_tab_delay > 0) {
		_nc_SPRINTF(buf, _nc_SLIMIT(sizeof(buf))
			    "%s$<%d>", C_HT, horizontal_tab_delay);
		tab = _nc_save_str(buf);
	    } else
		tab = _nc_save_str(C_HT);
	}
	if (init_tabs == ABSENT_NUMERIC && has_hardware_tabs == TRUE)
	    init_tabs = 8;

	 
	if (WANTED(bell))
	    bell = _nc_save_str("\007");
    }

     
    if (has_hardware_tabs == TRUE) {
	if (init_tabs != 8 && init_tabs != ABSENT_NUMERIC)
	    _nc_warning("hardware tabs with a width other than 8: %d", init_tabs);
	else {
	    if (PRESENT(tab) && _nc_capcmp(tab, C_HT))
		_nc_warning("hardware tabs with a non-^I tab string %s",
			    _nc_visbuf(tab));
	    else {
		if (WANTED(tab))
		    tab = _nc_save_str(C_HT);
		init_tabs = 8;
	    }
	}
    }
     
    if (PRESENT(other_non_function_keys)) {
	char *base;
	char *bp, *cp, *dp;
	struct name_table_entry const *from_ptr;
	struct name_table_entry const *to_ptr;
	char buf2[MAX_TERMINFO_LENGTH];
	bool foundim;

	 
	dp = strchr(other_non_function_keys, 'i');
	foundim = (dp != 0) && (dp[1] == 'm');

	 
	for (base = other_non_function_keys;
	     (cp = strchr(base, ',')) != 0;
	     base = cp + 1) {
	    size_t len = (unsigned) (cp - base);
	    size_t n;
	    assoc const *ap = 0;

	    for (n = 0; n < SIZEOF(ko_xlate); ++n) {
		if (len == strlen(ko_xlate[n].from)
		    && strncmp(ko_xlate[n].from, base, len) == 0) {
		    ap = ko_xlate + n;
		    break;
		}
	    }
	    if (ap == 0) {
		_nc_warning("unknown capability `%.*s' in ko string",
			    (int) len, base);
		continue;
	    } else if (ap->to[0] == '\0')	 
		continue;

	     

	    from_ptr = _nc_find_entry(ap->from, _nc_get_hash_table(TRUE));
	    to_ptr = _nc_find_entry(ap->to, _nc_get_hash_table(FALSE));

	    if (!from_ptr || !to_ptr)	 
		_nc_err_abort("ko translation table is invalid, I give up");

	    if (WANTED(tp->Strings[from_ptr->nte_index])) {
		_nc_warning("no value for ko capability %s", ap->from);
		continue;
	    }

	    if (tp->Strings[to_ptr->nte_index]) {
		const char *s = tp->Strings[from_ptr->nte_index];
		const char *t = tp->Strings[to_ptr->nte_index];
		 
		if (VALID_STRING(s) && VALID_STRING(t) && strcmp(s, t) != 0)
		    _nc_warning("%s (%s) already has an explicit value %s, ignoring ko",
				ap->to, ap->from, t);
		continue;
	    }

	     
	    bp = tp->Strings[from_ptr->nte_index];
	    if (VALID_STRING(bp)) {
		for (dp = buf2; *bp; bp++) {
		    if (bp[0] == '$' && bp[1] == '<') {
			while (*bp && *bp != '>') {
			    ++bp;
			}
		    } else
			*dp++ = *bp;
		}
		*dp = '\0';

		tp->Strings[to_ptr->nte_index] = _nc_save_str(buf2);
	    } else {
		tp->Strings[to_ptr->nte_index] = bp;
	    }
	}

	 
	if (foundim && WANTED(key_ic) && PRESENT(key_sic)) {
	    key_ic = key_sic;
	    key_sic = ABSENT_STRING;
	}
    }

    if (!has_base) {
	if (!hard_copy) {
	    if (WANTED(key_backspace))
		key_backspace = _nc_save_str(C_BS);
	    if (WANTED(key_left))
		key_left = _nc_save_str(C_BS);
	    if (WANTED(key_down))
		key_down = _nc_save_str(C_LF);
	}
    }

     
    if (PRESENT(acs_ulcorner) ||
	PRESENT(acs_llcorner) ||
	PRESENT(acs_urcorner) ||
	PRESENT(acs_lrcorner) ||
	PRESENT(acs_ltee) ||
	PRESENT(acs_rtee) ||
	PRESENT(acs_btee) ||
	PRESENT(acs_ttee) ||
	PRESENT(acs_hline) ||
	PRESENT(acs_vline) ||
	PRESENT(acs_plus)) {
	char buf2[MAX_TERMCAP_LENGTH];

	_nc_str_init(&result, buf2, sizeof(buf2));
	_nc_safe_strcat(&result, acs_chars);

	append_acs(&result, 'j', acs_lrcorner);
	append_acs(&result, 'k', acs_urcorner);
	append_acs(&result, 'l', acs_ulcorner);
	append_acs(&result, 'm', acs_llcorner);
	append_acs(&result, 'n', acs_plus);
	append_acs(&result, 'q', acs_hline);
	append_acs(&result, 't', acs_ltee);
	append_acs(&result, 'u', acs_rtee);
	append_acs(&result, 'v', acs_btee);
	append_acs(&result, 'w', acs_ttee);
	append_acs(&result, 'x', acs_vline);

	if (buf2[0]) {
	    acs_chars = _nc_save_str(buf2);
	    _nc_warning("acsc string synthesized from XENIX capabilities");
	}
    } else if (acs_chars == ABSENT_STRING
	       && PRESENT(enter_alt_charset_mode)
	       && PRESENT(exit_alt_charset_mode)) {
	acs_chars = _nc_save_str(VT_ACSC);
    }
    returnVoidDB;
}

static void
postprocess_terminfo(TERMTYPE2 *tp)
{
    TR(TRACE_DATABASE,
       (T_CALLED("postprocess_terminfo(tp=%p)"),
	(void *) tp));

     

     
    if (PRESENT(box_chars_1)) {
	char buf2[MAX_TERMCAP_LENGTH];
	string_desc result;

	_nc_str_init(&result, buf2, sizeof(buf2));
	_nc_safe_strcat(&result, acs_chars);

	append_acs0(&result, 'l', box_chars_1, 0);	 
	append_acs0(&result, 'q', box_chars_1, 1);	 
	append_acs0(&result, 'k', box_chars_1, 2);	 
	append_acs0(&result, 'x', box_chars_1, 3);	 
	append_acs0(&result, 'j', box_chars_1, 4);	 
	append_acs0(&result, 'm', box_chars_1, 5);	 
	append_acs0(&result, 'w', box_chars_1, 6);	 
	append_acs0(&result, 'u', box_chars_1, 7);	 
	append_acs0(&result, 'v', box_chars_1, 8);	 
	append_acs0(&result, 't', box_chars_1, 9);	 
	append_acs0(&result, 'n', box_chars_1, 10);	 

	if (buf2[0]) {
	    acs_chars = _nc_save_str(buf2);
	    _nc_warning("acsc string synthesized from AIX capabilities");
	    box_chars_1 = ABSENT_STRING;
	}
    }
     
    returnVoidDB;
}

 
static struct name_table_entry const *
lookup_fullname(const char *find)
{
    int state = -1;

    for (;;) {
	int count = 0;
	NCURSES_CONST char *const *names;

	switch (++state) {
	case BOOLEAN:
	    names = boolfnames;
	    break;
	case STRING:
	    names = strfnames;
	    break;
	case NUMBER:
	    names = numfnames;
	    break;
	default:
	    return NOTFOUND;
	}

	for (count = 0; names[count] != 0; count++) {
	    if (!strcmp(names[count], find)) {
		struct name_table_entry const *entry_ptr = _nc_get_table(FALSE);
		while (entry_ptr->nte_type != state
		       || entry_ptr->nte_index != count)
		    entry_ptr++;
		return entry_ptr;
	    }
	}
    }
}

 
