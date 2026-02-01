 

 

 

#define entry _ncu_entry
#define ENTRY _ncu_ENTRY

#include <curses.priv.h>

#undef entry
#undef ENTRY

#if HAVE_TSEARCH
#include <search.h>
#endif

#include <ctype.h>
#include <tic.h>

MODULE_ID("$Id: lib_tparm.c,v 1.137 2021/11/20 23:29:15 tom Exp $")

 

NCURSES_EXPORT_VAR(int) _nc_tparm_err = 0;

#define TPS(var) tps->var
#define popcount _nc_popcount	 

#define get_tparm_state(term) \
	    (term != NULL \
	      ? &(term->tparm_state) \
	      : &(_nc_prescreen.tparm_state))

#define isUPPER(c) ((c) >= 'A' && (c) <= 'Z')
#define isLOWER(c) ((c) >= 'a' && (c) <= 'z')
#define tc_BUMP()  if (level < 0 && number < 2) number++

typedef struct {
    const char *format;		 
    int tparm_type;		 
    int num_actual;
    int num_parsed;
    int num_popped;
    TPARM_ARG param[NUM_PARM];
    char *p_is_s[NUM_PARM];
} TPARM_DATA;

#if HAVE_TSEARCH
#define MyCache _nc_globals.cached_tparm
#define MyCount _nc_globals.count_tparm
static int which_tparm;
static TPARM_DATA **delete_tparm;
#endif  

static char dummy[] = "";	 

#if HAVE_TSEARCH
static int
cmp_format(const void *p, const void *q)
{
    const char *a = *(char *const *) p;
    const char *b = *(char *const *) q;
    return strcmp(a, b);
}
#endif

#if HAVE_TSEARCH
static void
visit_nodes(const void *nodep, VISIT which, int depth)
{
    (void) depth;
    if (which == preorder || which == leaf) {
	delete_tparm[which_tparm] = *(TPARM_DATA **) nodep;
	which_tparm++;
    }
}
#endif

NCURSES_EXPORT(void)
_nc_free_tparm(TERMINAL *termp)
{
    TPARM_STATE *tps = get_tparm_state(termp);
#if HAVE_TSEARCH
    if (MyCount != 0) {
	delete_tparm = typeCalloc(TPARM_DATA *, MyCount);
	which_tparm = 0;
	twalk(MyCache, visit_nodes);
	for (which_tparm = 0; which_tparm < MyCount; ++which_tparm) {
	    TPARM_DATA *ptr = delete_tparm[which_tparm];
	    if (ptr != NULL) {
		tdelete(ptr, &MyCache, cmp_format);
		free((char *) ptr->format);
		free(ptr);
	    }
	}
	which_tparm = 0;
	twalk(MyCache, visit_nodes);
	FreeAndNull(delete_tparm);
	MyCount = 0;
	which_tparm = 0;
    }
#endif
    FreeAndNull(TPS(out_buff));
    TPS(out_size) = 0;
    TPS(out_used) = 0;

    FreeAndNull(TPS(fmt_buff));
    TPS(fmt_size) = 0;
}

static int
tparm_error(TPARM_STATE *tps, const char *message)
{
    (void) tps;
    (void) message;
    DEBUG(2, ("%s: %s", message, _nc_visbuf(TPS(tparam_base))));
    return ++_nc_tparm_err;
}

#define get_space(tps, need) \
{ \
    size_t need2get = need + TPS(out_used); \
    if (need2get > TPS(out_size)) { \
	TPS(out_size) = need2get * 2; \
	TYPE_REALLOC(char, TPS(out_size), TPS(out_buff)); \
    } \
}

#if NCURSES_EXPANDED
static NCURSES_INLINE void
  (get_space) (TPARM_STATE *tps, size_t need) {
    get_space(tps, need);
}

#undef get_space
#endif

#define save_text(tps, fmt, s, len) \
{ \
    size_t s_len = (size_t) len + strlen(s) + strlen(fmt); \
    get_space(tps, s_len + 1); \
    _nc_SPRINTF(TPS(out_buff) + TPS(out_used), \
		_nc_SLIMIT(TPS(out_size) - TPS(out_used)) \
		fmt, s); \
    TPS(out_used) += strlen(TPS(out_buff) + TPS(out_used)); \
}

#if NCURSES_EXPANDED
static NCURSES_INLINE void
  (save_text) (TPARM_STATE *tps, const char *fmt, const char *s, int len) {
    save_text(tps, fmt, s, len);
}

#undef save_text
#endif

#define save_number(tps, fmt, number, len) \
{ \
    size_t s_len = (size_t) len + 30 + strlen(fmt); \
    get_space(tps, s_len + 1); \
    _nc_SPRINTF(TPS(out_buff) + TPS(out_used), \
		_nc_SLIMIT(TPS(out_size) - TPS(out_used)) \
		fmt, number); \
    TPS(out_used) += strlen(TPS(out_buff) + TPS(out_used)); \
}

#if NCURSES_EXPANDED
static NCURSES_INLINE void
  (save_number) (TPARM_STATE *tps, const char *fmt, int number, int len) {
    save_number(tps, fmt, number, len);
}

#undef save_number
#endif

#define save_char(tps, c) \
{ \
    get_space(tps, (size_t) 1); \
    TPS(out_buff)[TPS(out_used)++] = (char) ((c == 0) ? 0200 : c); \
}

#if NCURSES_EXPANDED
static NCURSES_INLINE void
  (save_char) (TPARM_STATE *tps, int c) {
    save_char(tps, c);
}

#undef save_char
#endif

#define npush(tps, x) \
{ \
    if (TPS(stack_ptr) < STACKSIZE) { \
	TPS(stack)[TPS(stack_ptr)].num_type = TRUE; \
	TPS(stack)[TPS(stack_ptr)].data.num = x; \
	TPS(stack_ptr)++; \
    } else { \
	(void) tparm_error(tps, "npush: stack overflow"); \
    } \
}

#if NCURSES_EXPANDED
static NCURSES_INLINE void
  (npush) (TPARM_STATE *tps, int x) {
    npush(tps, x);
}

#undef npush
#endif

#define spush(tps, x) \
{ \
    if (TPS(stack_ptr) < STACKSIZE) { \
	TPS(stack)[TPS(stack_ptr)].num_type = FALSE; \
	TPS(stack)[TPS(stack_ptr)].data.str = x; \
	TPS(stack_ptr)++; \
    } else { \
	(void) tparm_error(tps, "spush: stack overflow"); \
    } \
}

#if NCURSES_EXPANDED
static NCURSES_INLINE void
  (spush) (TPARM_STATE *tps, char *x) {
    spush(tps, x);
}

#undef spush
#endif

#define npop(tps) \
    ((TPS(stack_ptr)-- > 0) \
     ? ((TPS(stack)[TPS(stack_ptr)].num_type) \
	 ? TPS(stack)[TPS(stack_ptr)].data.num \
	 : 0) \
     : (tparm_error(tps, "npop: stack underflow"), \
        TPS(stack_ptr) = 0))

#if NCURSES_EXPANDED
static NCURSES_INLINE int
  (npop) (TPARM_STATE *tps) {
    return npop(tps);
}
#undef npop
#endif

#define spop(tps) \
    ((TPS(stack_ptr)-- > 0) \
     ? ((!TPS(stack)[TPS(stack_ptr)].num_type \
        && TPS(stack)[TPS(stack_ptr)].data.str != 0) \
         ? TPS(stack)[TPS(stack_ptr)].data.str \
         : dummy) \
     : (tparm_error(tps, "spop: stack underflow"), \
        dummy))

#if NCURSES_EXPANDED
static NCURSES_INLINE char *
  (spop) (TPARM_STATE *tps) {
    return spop(tps);
}
#undef spop
#endif

static NCURSES_INLINE const char *
parse_format(const char *s, char *format, int *len)
{
    *len = 0;
    if (format != 0) {
	bool done = FALSE;
	bool allowminus = FALSE;
	bool dot = FALSE;
	bool err = FALSE;
	char *fmt = format;
	int my_width = 0;
	int my_prec = 0;
	int value = 0;

	*len = 0;
	*format++ = '%';
	while (*s != '\0' && !done) {
	    switch (*s) {
	    case 'c':		 
	    case 'd':		 
	    case 'o':		 
	    case 'x':		 
	    case 'X':		 
	    case 's':
#ifdef EXP_XTERM_1005
	    case 'u':
#endif
		*format++ = *s;
		done = TRUE;
		break;
	    case '.':
		*format++ = *s++;
		if (dot) {
		    err = TRUE;
		} else {	 
		    dot = TRUE;
		    my_width = value;
		}
		value = 0;
		break;
	    case '#':
		*format++ = *s++;
		break;
	    case ' ':
		*format++ = *s++;
		break;
	    case ':':
		s++;
		allowminus = TRUE;
		break;
	    case '-':
		if (allowminus) {
		    *format++ = *s++;
		} else {
		    done = TRUE;
		}
		break;
	    default:
		if (isdigit(UChar(*s))) {
		    value = (value * 10) + (*s - '0');
		    if (value > 10000)
			err = TRUE;
		    *format++ = *s++;
		} else {
		    done = TRUE;
		}
	    }
	}

	 
	if (err) {
	    my_width = my_prec = value = 0;
	    format = fmt;
	    *format++ = '%';
	    *format++ = *s;
	}

	 
	if (dot)
	    my_prec = value;
	else
	    my_width = value;

	*format = '\0';
	 
	*len = (my_width > my_prec) ? my_width : my_prec;
    }
    return s;
}

 
NCURSES_EXPORT(int)
_nc_tparm_analyze(TERMINAL *term, const char *string, char **p_is_s, int *popcount)
{
    TPARM_STATE *tps = get_tparm_state(term);
    size_t len2;
    int i;
    int lastpop = -1;
    int len;
    int number = 0;
    int level = -1;
    const char *cp = string;

    if (cp == 0)
	return 0;

    if ((len2 = strlen(cp)) + 2 > TPS(fmt_size)) {
	TPS(fmt_size) += len2 + 2;
	TPS(fmt_buff) = typeRealloc(char, TPS(fmt_size), TPS(fmt_buff));
	if (TPS(fmt_buff) == 0)
	    return 0;
    }

    memset(p_is_s, 0, sizeof(p_is_s[0]) * NUM_PARM);
    *popcount = 0;

    while ((cp - string) < (int) len2) {
	if (*cp == '%') {
	    cp++;
	    cp = parse_format(cp, TPS(fmt_buff), &len);
	    switch (*cp) {
	    default:
		break;

	    case 'd':		 
	    case 'o':		 
	    case 'x':		 
	    case 'X':		 
	    case 'c':		 
#ifdef EXP_XTERM_1005
	    case 'u':
#endif
		if (lastpop <= 0) {
		    tc_BUMP();
		}
		level -= 1;
		lastpop = -1;
		break;

	    case 'l':
	    case 's':
		if (lastpop > 0) {
		    level -= 1;
		    p_is_s[lastpop - 1] = dummy;
		}
		tc_BUMP();
		break;

	    case 'p':
		cp++;
		i = (UChar(*cp) - '0');
		if (i >= 0 && i <= NUM_PARM) {
		    ++level;
		    lastpop = i;
		    if (lastpop > *popcount)
			*popcount = lastpop;
		}
		break;

	    case 'P':
		++cp;
		break;

	    case 'g':
		++level;
		cp++;
		break;

	    case S_QUOTE:
		++level;
		cp += 2;
		lastpop = -1;
		break;

	    case L_BRACE:
		++level;
		cp++;
		while (isdigit(UChar(*cp))) {
		    cp++;
		}
		break;

	    case '+':
	    case '-':
	    case '*':
	    case '/':
	    case 'm':
	    case 'A':
	    case 'O':
	    case '&':
	    case '|':
	    case '^':
	    case '=':
	    case '<':
	    case '>':
		tc_BUMP();
		level -= 1;	 
		lastpop = -1;
		break;

	    case '!':
	    case '~':
		tc_BUMP();
		lastpop = -1;
		break;

	    case 'i':
		 
		break;
	    }
	}
	if (*cp != '\0')
	    cp++;
    }

    if (number > NUM_PARM)
	number = NUM_PARM;
    return number;
}

 
static int
tparm_setup(TERMINAL *term, const char *string, TPARM_DATA *result)
{
    TPARM_STATE *tps = get_tparm_state(term);
    int rc = OK;

    TPS(out_used) = 0;
    memset(result, 0, sizeof(*result));

    if (string == NULL) {
	TR(TRACE_CALLS, ("%s: format is null", TPS(tname)));
	rc = ERR;
    } else {
#if HAVE_TSEARCH
	TPARM_DATA *fs;
	void *ft;

	result->format = string;
	if ((ft = tfind(result, &MyCache, cmp_format)) != 0) {
	    size_t len2;
	    fs = *(TPARM_DATA **) ft;
	    *result = *fs;
	    if ((len2 = strlen(string)) + 2 > TPS(fmt_size)) {
		TPS(fmt_size) += len2 + 2;
		TPS(fmt_buff) = typeRealloc(char, TPS(fmt_size), TPS(fmt_buff));
		if (TPS(fmt_buff) == 0)
		    return ERR;
	    }
	} else
#endif
	{
	     
	    result->num_parsed = _nc_tparm_analyze(term, string,
						   result->p_is_s,
						   &(result->num_popped));
	    if (TPS(fmt_buff) == 0) {
		TR(TRACE_CALLS, ("%s: error in analysis", TPS(tname)));
		rc = ERR;
	    } else {
		int n;

		if (result->num_parsed > NUM_PARM)
		    result->num_parsed = NUM_PARM;
		if (result->num_popped > NUM_PARM)
		    result->num_popped = NUM_PARM;
		result->num_actual = max(result->num_popped, result->num_parsed);

		for (n = 0; n < result->num_actual; ++n) {
		    if (result->p_is_s[n])
			result->tparm_type |= (1 << n);
		}
#if HAVE_TSEARCH
		if ((fs = typeCalloc(TPARM_DATA, 1)) != 0) {
		    *fs = *result;
		    if ((fs->format = strdup(string)) != 0) {
			if (tsearch(fs, &MyCache, cmp_format) != 0) {
			    ++MyCount;
			} else {
			    free(fs);
			    rc = ERR;
			}
		    } else {
			free(fs);
			rc = ERR;
		    }
		} else {
		    rc = ERR;
		}
#endif
	    }
	}
    }

    return rc;
}

 
static void
tparm_copy_valist(TPARM_DATA *data, int use_TPARM_ARG, va_list ap)
{
    int i;

    for (i = 0; i < data->num_actual; i++) {
	if (data->p_is_s[i] != 0) {
	    char *value = va_arg(ap, char *);
	    if (value == 0)
		value = dummy;
	    data->p_is_s[i] = value;
	    data->param[i] = 0;
	} else if (use_TPARM_ARG) {
	    data->param[i] = va_arg(ap, TPARM_ARG);
	} else {
	    data->param[i] = (TPARM_ARG) va_arg(ap, int);
	}
    }
}

 
static bool
tparm_tc_compat(TPARM_STATE *tps, TPARM_DATA *data)
{
    bool termcap_hack = FALSE;

    TPS(stack_ptr) = 0;

    if (data->num_popped == 0) {
	int i;

	termcap_hack = TRUE;
	for (i = data->num_parsed - 1; i >= 0; i--) {
	    if (data->p_is_s[i]) {
		spush(tps, data->p_is_s[i]);
	    } else {
		npush(tps, (int) data->param[i]);
	    }
	}
    }
    return termcap_hack;
}

#ifdef TRACE
static void
tparm_trace_call(TPARM_STATE *tps, const char *string, TPARM_DATA *data)
{
    if (USE_TRACEF(TRACE_CALLS)) {
	int i;
	for (i = 0; i < data->num_actual; i++) {
	    if (data->p_is_s[i] != 0) {
		save_text(tps, ", %s", _nc_visbuf(data->p_is_s[i]), 0);
	    } else if ((long) data->param[i] > MAX_OF_TYPE(NCURSES_INT2) ||
		       (long) data->param[i] < 0) {
		_tracef("BUG: problem with tparm parameter #%d of %d",
			i + 1, data->num_actual);
		break;
	    } else {
		save_number(tps, ", %d", (int) data->param[i], 0);
	    }
	}
	_tracef(T_CALLED("%s(%s%s)"), TPS(tname), _nc_visbuf(string), TPS(out_buff));
	TPS(out_used) = 0;
	_nc_unlock_global(tracef);
    }
}

#else
#define tparm_trace_call(tps, string, data)	 
#endif  

#define init_vars(name) \
	if (!name##_used) { \
	    name##_used = TRUE; \
	    memset(name##_vars, 0, sizeof(name##_vars)); \
	}

static NCURSES_INLINE char *
tparam_internal(TPARM_STATE *tps, const char *string, TPARM_DATA *data)
{
    int number;
    int len;
    int level;
    int x, y;
    int i;
    const char *s;
    const char *cp = string;
    size_t len2 = strlen(cp);
    bool incremented_two = FALSE;
    bool termcap_hack = tparm_tc_compat(tps, data);
     
    bool dynamic_used = FALSE;
    int dynamic_vars[NUM_VARS];

    tparm_trace_call(tps, string, data);

    while ((cp - string) < (int) len2) {
	if (*cp != '%') {
	    save_char(tps, UChar(*cp));
	} else {
	    TPS(tparam_base) = cp++;
	    cp = parse_format(cp, TPS(fmt_buff), &len);
	    switch (*cp) {
	    default:
		break;
	    case '%':
		save_char(tps, '%');
		break;

	    case 'd':		 
	    case 'o':		 
	    case 'x':		 
	    case 'X':		 
		x = npop(tps);
		save_number(tps, TPS(fmt_buff), x, len);
		break;

	    case 'c':		 
		x = npop(tps);
		save_char(tps, x);
		break;

#ifdef EXP_XTERM_1005
	    case 'u':
		{
		    unsigned char target[10];
		    unsigned source = (unsigned) npop(tps);
		    int rc = _nc_conv_to_utf8(target, source, (unsigned)
					      sizeof(target));
		    int n;
		    for (n = 0; n < rc; ++n) {
			save_char(tps, target[n]);
		    }
		}
		break;
#endif
	    case 'l':
		s = spop(tps);
		npush(tps, (int) strlen(s));
		break;

	    case 's':
		s = spop(tps);
		save_text(tps, TPS(fmt_buff), s, len);
		break;

	    case 'p':
		cp++;
		i = (UChar(*cp) - '1');
		if (i >= 0 && i < NUM_PARM) {
		    if (data->p_is_s[i]) {
			spush(tps, data->p_is_s[i]);
		    } else {
			npush(tps, (int) data->param[i]);
		    }
		}
		break;

	    case 'P':
		cp++;
		if (isUPPER(*cp)) {
		    i = (UChar(*cp) - 'A');
		    TPS(static_vars)[i] = npop(tps);
		} else if (isLOWER(*cp)) {
		    i = (UChar(*cp) - 'a');
		    init_vars(dynamic);
		    dynamic_vars[i] = npop(tps);
		}
		break;

	    case 'g':
		cp++;
		if (isUPPER(*cp)) {
		    i = (UChar(*cp) - 'A');
		    npush(tps, TPS(static_vars)[i]);
		} else if (isLOWER(*cp)) {
		    i = (UChar(*cp) - 'a');
		    init_vars(dynamic);
		    npush(tps, dynamic_vars[i]);
		}
		break;

	    case S_QUOTE:
		cp++;
		npush(tps, UChar(*cp));
		cp++;
		break;

	    case L_BRACE:
		number = 0;
		cp++;
		while (isdigit(UChar(*cp))) {
		    number = (number * 10) + (UChar(*cp) - '0');
		    cp++;
		}
		npush(tps, number);
		break;

	    case '+':
		y = npop(tps);
		x = npop(tps);
		npush(tps, x + y);
		break;

	    case '-':
		y = npop(tps);
		x = npop(tps);
		npush(tps, x - y);
		break;

	    case '*':
		y = npop(tps);
		x = npop(tps);
		npush(tps, x * y);
		break;

	    case '/':
		y = npop(tps);
		x = npop(tps);
		npush(tps, y ? (x / y) : 0);
		break;

	    case 'm':
		y = npop(tps);
		x = npop(tps);
		npush(tps, y ? (x % y) : 0);
		break;

	    case 'A':
		y = npop(tps);
		x = npop(tps);
		npush(tps, y && x);
		break;

	    case 'O':
		y = npop(tps);
		x = npop(tps);
		npush(tps, y || x);
		break;

	    case '&':
		y = npop(tps);
		x = npop(tps);
		npush(tps, x & y);
		break;

	    case '|':
		y = npop(tps);
		x = npop(tps);
		npush(tps, x | y);
		break;

	    case '^':
		y = npop(tps);
		x = npop(tps);
		npush(tps, x ^ y);
		break;

	    case '=':
		y = npop(tps);
		x = npop(tps);
		npush(tps, x == y);
		break;

	    case '<':
		y = npop(tps);
		x = npop(tps);
		npush(tps, x < y);
		break;

	    case '>':
		y = npop(tps);
		x = npop(tps);
		npush(tps, x > y);
		break;

	    case '!':
		x = npop(tps);
		npush(tps, !x);
		break;

	    case '~':
		x = npop(tps);
		npush(tps, ~x);
		break;

	    case 'i':
		 
		if (!incremented_two) {
		    incremented_two = TRUE;
		    if (data->p_is_s[0] == 0) {
			data->param[0]++;
			if (termcap_hack)
			    TPS(stack)[0].data.num = (int) data->param[0];
		    }
		    if (data->p_is_s[1] == 0) {
			data->param[1]++;
			if (termcap_hack)
			    TPS(stack)[1].data.num = (int) data->param[1];
		    }
		}
		break;

	    case '?':
		break;

	    case 't':
		x = npop(tps);
		if (!x) {
		     
		    cp++;
		    level = 0;
		    while (*cp) {
			if (*cp == '%') {
			    cp++;
			    if (*cp == '?')
				level++;
			    else if (*cp == ';') {
				if (level > 0)
				    level--;
				else
				    break;
			    } else if (*cp == 'e' && level == 0)
				break;
			}

			if (*cp)
			    cp++;
		    }
		}
		break;

	    case 'e':
		 
		cp++;
		level = 0;
		while (*cp) {
		    if (*cp == '%') {
			cp++;
			if (*cp == '?')
			    level++;
			else if (*cp == ';') {
			    if (level > 0)
				level--;
			    else
				break;
			}
		    }

		    if (*cp)
			cp++;
		}
		break;

	    case ';':
		break;

	    }			 
	}			 

	if (*cp == '\0')
	    break;

	cp++;
    }				 

    get_space(tps, (size_t) 1);
    TPS(out_buff)[TPS(out_used)] = '\0';

    if (TPS(stack_ptr) && !_nc_tparm_err) {
	DEBUG(2, ("tparm: stack has %d item%s on return",
		  TPS(stack_ptr),
		  TPS(stack_ptr) == 1 ? "" : "s"));
	_nc_tparm_err++;
    }

    T((T_RETURN("%s"), _nc_visbuf(TPS(out_buff))));
    return (TPS(out_buff));
}

#if NCURSES_TPARM_VARARGS

NCURSES_EXPORT(char *)
tparm(const char *string, ...)
{
    TPARM_STATE *tps = get_tparm_state(cur_term);
    TPARM_DATA myData;
    char *result = NULL;

    _nc_tparm_err = 0;
#ifdef TRACE
    tps->tname = "tparm";
#endif  

    if (tparm_setup(cur_term, string, &myData) == OK) {
	va_list ap;

	va_start(ap, string);
	tparm_copy_valist(&myData, TRUE, ap);
	va_end(ap);

	result = tparam_internal(tps, string, &myData);
    }
    return result;
}

#else  

NCURSES_EXPORT(char *)
tparm(const char *string,
      TPARM_ARG a1,
      TPARM_ARG a2,
      TPARM_ARG a3,
      TPARM_ARG a4,
      TPARM_ARG a5,
      TPARM_ARG a6,
      TPARM_ARG a7,
      TPARM_ARG a8,
      TPARM_ARG a9)
{
    TPARM_STATE *tps = get_tparm_state(cur_term);
    TPARM_DATA myData;
    char *result = NULL;

    _nc_tparm_err = 0;
#ifdef TRACE
    tps->tname = "tparm";
#endif  

    if (tparm_setup(cur_term, string, &myData) == OK) {

	myData.param[0] = a1;
	myData.param[1] = a2;
	myData.param[2] = a3;
	myData.param[3] = a4;
	myData.param[4] = a5;
	myData.param[5] = a6;
	myData.param[6] = a7;
	myData.param[7] = a8;
	myData.param[8] = a9;

	result = tparam_internal(tps, string, &myData);
    }
    return result;
}

#endif  

NCURSES_EXPORT(char *)
tiparm(const char *string, ...)
{
    TPARM_STATE *tps = get_tparm_state(cur_term);
    TPARM_DATA myData;
    char *result = NULL;

    _nc_tparm_err = 0;
#ifdef TRACE
    tps->tname = "tiparm";
#endif  

    if (tparm_setup(cur_term, string, &myData) == OK) {
	va_list ap;

	va_start(ap, string);
	tparm_copy_valist(&myData, FALSE, ap);
	va_end(ap);

	result = tparam_internal(tps, string, &myData);
    }
    return result;
}

 
NCURSES_EXPORT(char *)
_nc_tiparm(int expected, const char *string, ...)
{
    TPARM_STATE *tps = get_tparm_state(cur_term);
    TPARM_DATA myData;
    char *result = NULL;

    _nc_tparm_err = 0;
#ifdef TRACE
    tps->tname = "_nc_tiparm";
#endif  

    if (tparm_setup(cur_term, string, &myData) == OK
	&& myData.num_actual <= expected
	&& myData.tparm_type == 0) {
	va_list ap;

	va_start(ap, string);
	tparm_copy_valist(&myData, FALSE, ap);
	va_end(ap);

	result = tparam_internal(tps, string, &myData);
    }
    return result;
}

 
NCURSES_EXPORT(void)
_nc_reset_tparm(TERMINAL *term)
{
    TPARM_STATE *tps = get_tparm_state(term);
    memset(TPS(static_vars), 0, sizeof(TPS(static_vars)));
}
