 

 

 

#include <curses.priv.h>

#include <tic.h>

MODULE_ID("$Id: alloc_ttype.c,v 1.46 2022/09/17 21:44:35 tom Exp $")

#if NCURSES_XNAMES
 
static int
merge_names(char **dst, char **a, int na, char **b, int nb)
{
    int n = 0;
    while (na > 0 && nb > 0) {
	int cmp = strcmp(*a, *b);
	if (cmp < 0) {
	    dst[n++] = *a++;
	    na--;
	} else if (cmp > 0) {
	    dst[n++] = *b++;
	    nb--;
	} else {
	    dst[n++] = *a;
	    a++, b++;
	    na--, nb--;
	}
    }
    while (na-- > 0) {
	dst[n++] = *a++;
    }
    while (nb-- > 0) {
	dst[n++] = *b++;
    }
    DEBUG(4, ("merge_names -> %d", n));
    return n;
}

static bool
find_name(char **table, int item, int length, const char *name)
{
    int n;
    int result = -1;

    for (n = item; n < length; ++n) {
	if (!strcmp(table[n], name)) {
	    DEBUG(4, ("found name '%s' @%d", name, n));
	    result = n;
	    break;
	}
    }
    if (result < 0) {
	DEBUG(4, ("did not find name '%s'", name));
    }
    return (result >= 0);
}

#define EXTEND_NUM(num, ext) \
	DEBUG(4, ("extending " #num " from %d to %d", \
	 to->num, (unsigned short) (to->num + (ext - to->ext)))); \
	to->num = (unsigned short) (to->num + (ext - to->ext))

static void
realign_data(TERMTYPE2 *to, char **ext_Names,
	     int ext_Booleans,
	     int ext_Numbers,
	     int ext_Strings)
{
    int n, m, base;
    int to_Booleans = to->ext_Booleans;
    int to_Numbers = to->ext_Numbers;
    int to_Strings = to->ext_Strings;
    int to1, to2, from;

    DEBUG(4, ("realign_data %d/%d/%d vs %d/%d/%d",
	      ext_Booleans,
	      ext_Numbers,
	      ext_Strings,
	      to->ext_Booleans,
	      to->ext_Numbers,
	      to->ext_Strings));

    if (to->ext_Booleans != ext_Booleans) {
	to1 = 0;
	to2 = to_Booleans + to1;
	from = 0;
	EXTEND_NUM(num_Booleans, ext_Booleans);
	TYPE_REALLOC(NCURSES_SBOOL, to->num_Booleans, to->Booleans);
	for (n = to->ext_Booleans - 1,
	     m = ext_Booleans - 1,
	     base = to->num_Booleans - (m + 1); m >= 0; m--) {
	    if (find_name(to->ext_Names, to1, to2, ext_Names[m + from])) {
		to->Booleans[base + m] = to->Booleans[base + n--];
	    } else {
		to->Booleans[base + m] = FALSE;
	    }
	}
	to->ext_Booleans = UShort(ext_Booleans);
    }

    if (to->ext_Numbers != ext_Numbers) {
	to1 = to_Booleans;
	to2 = to_Numbers + to1;
	from = ext_Booleans;
	EXTEND_NUM(num_Numbers, ext_Numbers);
	TYPE_REALLOC(NCURSES_INT2, to->num_Numbers, to->Numbers);
	for (n = to->ext_Numbers - 1,
	     m = ext_Numbers - 1,
	     base = to->num_Numbers - (m + 1); m >= 0; m--) {
	    if (find_name(to->ext_Names, to1, to2, ext_Names[m + from])) {
		to->Numbers[base + m] = to->Numbers[base + n--];
	    } else {
		to->Numbers[base + m] = ABSENT_NUMERIC;
	    }
	}
	to->ext_Numbers = UShort(ext_Numbers);
    }

    if (to->ext_Strings != ext_Strings) {
	to1 = to_Booleans + to_Numbers;
	to2 = to_Strings + to1;
	from = ext_Booleans + ext_Numbers;
	EXTEND_NUM(num_Strings, ext_Strings);
	TYPE_REALLOC(char *, to->num_Strings, to->Strings);
	for (n = to->ext_Strings - 1,
	     m = ext_Strings - 1,
	     base = to->num_Strings - (m + 1); m >= 0; m--) {
	    if (find_name(to->ext_Names, to1, to2, ext_Names[m + from])) {
		to->Strings[base + m] = to->Strings[base + n--];
	    } else {
		to->Strings[base + m] = ABSENT_STRING;
	    }
	}
	to->ext_Strings = UShort(ext_Strings);
    }
}

 
static unsigned
_nc_first_ext_name(TERMTYPE2 *tp, int token_type)
{
    unsigned first;

    switch (token_type) {
    case BOOLEAN:
	first = 0;
	break;
    case NUMBER:
	first = tp->ext_Booleans;
	break;
    case STRING:
	first = (unsigned) (tp->ext_Booleans + tp->ext_Numbers);
	break;
    default:
	first = 0;
	break;
    }
    return first;
}

 
static unsigned
_nc_last_ext_name(TERMTYPE2 *tp, int token_type)
{
    unsigned last;

    switch (token_type) {
    case BOOLEAN:
	last = tp->ext_Booleans;
	break;
    case NUMBER:
	last = (unsigned) (tp->ext_Booleans + tp->ext_Numbers);
	break;
    default:
    case STRING:
	last = NUM_EXT_NAMES(tp);
	break;
    }
    return last;
}

 
static int
_nc_find_ext_name(TERMTYPE2 *tp, char *name, int token_type)
{
    unsigned j;
    unsigned first = _nc_first_ext_name(tp, token_type);
    unsigned last = _nc_last_ext_name(tp, token_type);

    for (j = first; j < last; j++) {
	if (!strcmp(name, tp->ext_Names[j])) {
	    return (int) j;
	}
    }
    return -1;
}

 
static int
_nc_ext_data_index(TERMTYPE2 *tp, int n, int token_type)
{
    switch (token_type) {
    case BOOLEAN:
	n += (tp->num_Booleans - tp->ext_Booleans);
	break;
    case NUMBER:
	n += (tp->num_Numbers - tp->ext_Numbers)
	    - (tp->ext_Booleans);
	break;
    default:
    case STRING:
	n += (tp->num_Strings - tp->ext_Strings)
	    - (tp->ext_Booleans + tp->ext_Numbers);
    }
    return n;
}

 
static bool
_nc_del_ext_name(TERMTYPE2 *tp, char *name, int token_type)
{
    int first;

    if ((first = _nc_find_ext_name(tp, name, token_type)) >= 0) {
	int j;
	int last = (int) NUM_EXT_NAMES(tp) - 1;

	for (j = first; j < last; j++) {
	    tp->ext_Names[j] = tp->ext_Names[j + 1];
	}
	first = _nc_ext_data_index(tp, first, token_type);
	switch (token_type) {
	case BOOLEAN:
	    last = tp->num_Booleans - 1;
	    for (j = first; j < last; j++)
		tp->Booleans[j] = tp->Booleans[j + 1];
	    tp->ext_Booleans--;
	    tp->num_Booleans--;
	    break;
	case NUMBER:
	    last = tp->num_Numbers - 1;
	    for (j = first; j < last; j++)
		tp->Numbers[j] = tp->Numbers[j + 1];
	    tp->ext_Numbers--;
	    tp->num_Numbers--;
	    break;
	case STRING:
	    last = tp->num_Strings - 1;
	    for (j = first; j < last; j++)
		tp->Strings[j] = tp->Strings[j + 1];
	    tp->ext_Strings--;
	    tp->num_Strings--;
	    break;
	}
	return TRUE;
    }
    return FALSE;
}

 
static int
_nc_ins_ext_name(TERMTYPE2 *tp, char *name, int token_type)
{
    unsigned first = _nc_first_ext_name(tp, token_type);
    unsigned last = _nc_last_ext_name(tp, token_type);
    unsigned total = NUM_EXT_NAMES(tp) + 1;
    unsigned j, k;

    for (j = first; j < last; j++) {
	int cmp = strcmp(name, tp->ext_Names[j]);
	if (cmp == 0)
	     
	    return _nc_ext_data_index(tp, (int) j, token_type);
	if (cmp < 0) {
	    break;
	}
    }

    TYPE_REALLOC(char *, total, tp->ext_Names);
    for (k = total - 1; k > j; k--)
	tp->ext_Names[k] = tp->ext_Names[k - 1];
    tp->ext_Names[j] = name;
    j = (unsigned) _nc_ext_data_index(tp, (int) j, token_type);

    switch (token_type) {
    case BOOLEAN:
	tp->ext_Booleans++;
	tp->num_Booleans++;
	TYPE_REALLOC(NCURSES_SBOOL, tp->num_Booleans, tp->Booleans);
	for (k = (unsigned) (tp->num_Booleans - 1); k > j; k--)
	    tp->Booleans[k] = tp->Booleans[k - 1];
	break;
    case NUMBER:
	tp->ext_Numbers++;
	tp->num_Numbers++;
	TYPE_REALLOC(NCURSES_INT2, tp->num_Numbers, tp->Numbers);
	for (k = (unsigned) (tp->num_Numbers - 1); k > j; k--)
	    tp->Numbers[k] = tp->Numbers[k - 1];
	break;
    case STRING:
	tp->ext_Strings++;
	tp->num_Strings++;
	TYPE_REALLOC(char *, tp->num_Strings, tp->Strings);
	for (k = (unsigned) (tp->num_Strings - 1); k > j; k--)
	    tp->Strings[k] = tp->Strings[k - 1];
	break;
    }
    return (int) j;
}

 
static void
adjust_cancels(TERMTYPE2 *to, TERMTYPE2 *from)
{
    int first = to->ext_Booleans + to->ext_Numbers;
    int last = first + to->ext_Strings;
    int j, k;

    DEBUG(3, (T_CALLED("adjust_cancels(%s), from(%s)"),
	      to ? NonNull(to->term_names) : "?",
	      from ? NonNull(from->term_names) : "?"));
    for (j = first; j < last;) {
	char *name = to->ext_Names[j];
	int j_str = to->num_Strings - first - to->ext_Strings;

	if (to->Strings[j + j_str] == CANCELLED_STRING) {
	    if (_nc_find_ext_name(from, to->ext_Names[j], BOOLEAN) >= 0) {
		if (_nc_del_ext_name(to, name, STRING)
		    || _nc_del_ext_name(to, name, NUMBER)) {
		    k = _nc_ins_ext_name(to, name, BOOLEAN);
		    to->Booleans[k] = FALSE;
		} else {
		    j++;
		}
	    } else if (_nc_find_ext_name(from, to->ext_Names[j], NUMBER) >= 0) {
		if (_nc_del_ext_name(to, name, STRING)
		    || _nc_del_ext_name(to, name, BOOLEAN)) {
		    k = _nc_ins_ext_name(to, name, NUMBER);
		    to->Numbers[k] = CANCELLED_NUMERIC;
		} else {
		    j++;
		}
	    } else if (_nc_find_ext_name(from, to->ext_Names[j], STRING) >= 0) {
		if (_nc_del_ext_name(to, name, NUMBER)
		    || _nc_del_ext_name(to, name, BOOLEAN)) {
		    k = _nc_ins_ext_name(to, name, STRING);
		    to->Strings[k] = CANCELLED_STRING;
		} else {
		    j++;
		}
	    } else {
		j++;
	    }
	} else {
	    j++;
	}
    }
    DEBUG(3, (T_RETURN("")));
}

NCURSES_EXPORT(void)
_nc_align_termtype(TERMTYPE2 *to, TERMTYPE2 *from)
{
    int na;
    int nb;
    char **ext_Names;

    na = to ? ((int) NUM_EXT_NAMES(to)) : 0;
    nb = from ? ((int) NUM_EXT_NAMES(from)) : 0;

    DEBUG(2, (T_CALLED("_nc_align_termtype to(%d:%s), from(%d:%s)"),
	      na, to ? NonNull(to->term_names) : "?",
	      nb, from ? NonNull(from->term_names) : "?"));

    if (to != NULL && from != NULL && (na != 0 || nb != 0)) {
	int ext_Booleans, ext_Numbers, ext_Strings;
	bool used_ext_Names = FALSE;

	if ((na == nb)		 
	    &&(to->ext_Booleans == from->ext_Booleans)
	    && (to->ext_Numbers == from->ext_Numbers)
	    && (to->ext_Strings == from->ext_Strings)) {
	    int n;
	    bool same;

	    for (n = 0, same = TRUE; n < na; n++) {
		if (strcmp(to->ext_Names[n], from->ext_Names[n])) {
		    same = FALSE;
		    break;
		}
	    }
	    if (same) {
		DEBUG(2, (T_RETURN("")));
		return;
	    }
	}
	 
	TYPE_MALLOC(char *, (size_t)(na + nb), ext_Names);

	if (to->ext_Strings && (from->ext_Booleans + from->ext_Numbers))
	    adjust_cancels(to, from);

	if (from->ext_Strings && (to->ext_Booleans + to->ext_Numbers))
	    adjust_cancels(from, to);

	ext_Booleans = merge_names(ext_Names,
				   to->ext_Names,
				   to->ext_Booleans,
				   from->ext_Names,
				   from->ext_Booleans);
	ext_Numbers = merge_names(ext_Names + ext_Booleans,
				  to->ext_Names
				  + to->ext_Booleans,
				  to->ext_Numbers,
				  from->ext_Names
				  + from->ext_Booleans,
				  from->ext_Numbers);
	ext_Strings = merge_names(ext_Names + ext_Numbers + ext_Booleans,
				  to->ext_Names
				  + to->ext_Booleans
				  + to->ext_Numbers,
				  to->ext_Strings,
				  from->ext_Names
				  + from->ext_Booleans
				  + from->ext_Numbers,
				  from->ext_Strings);
	 
	if (na != (ext_Booleans + ext_Numbers + ext_Strings)) {
	    realign_data(to, ext_Names, ext_Booleans, ext_Numbers, ext_Strings);
	    FreeIfNeeded(to->ext_Names);
	    to->ext_Names = ext_Names;
	    DEBUG(2, ("realigned %d extended names for '%s' (to)",
		      NUM_EXT_NAMES(to), to->term_names));
	    used_ext_Names = TRUE;
	}
	if (nb != (ext_Booleans + ext_Numbers + ext_Strings)) {
	    nb = (ext_Booleans + ext_Numbers + ext_Strings);
	    realign_data(from, ext_Names, ext_Booleans, ext_Numbers, ext_Strings);
	    TYPE_REALLOC(char *, (size_t) nb, from->ext_Names);
	    memcpy(from->ext_Names, ext_Names, sizeof(char *) * (size_t) nb);
	    DEBUG(2, ("realigned %d extended names for '%s' (from)",
		      NUM_EXT_NAMES(from), from->term_names));
	}
	if (!used_ext_Names)
	    free(ext_Names);
    }
    DEBUG(2, (T_RETURN("")));
}
#endif

#define srcINT 1
#define dstINT 2

 
static void
copy_termtype(TERMTYPE2 *dst, const TERMTYPE2 *src, int mode)
{
    unsigned i;
    int pass;
    char *new_table;
#if NCURSES_EXT_NUMBERS
    short *oldptr = 0;
    int *newptr = 0;
#endif

    DEBUG(2, (T_CALLED("copy_termtype(dst=%p, src=%p, mode=%d)"), (void *)
	      dst, (const void *) src, mode));
    *dst = *src;		 

    TYPE_MALLOC(NCURSES_SBOOL, NUM_BOOLEANS(dst), dst->Booleans);
    TYPE_MALLOC(char *, NUM_STRINGS(dst), dst->Strings);

    memcpy(dst->Booleans,
	   src->Booleans,
	   NUM_BOOLEANS(dst) * sizeof(dst->Booleans[0]));
    memcpy(dst->Strings,
	   src->Strings,
	   NUM_STRINGS(dst) * sizeof(dst->Strings[0]));

    new_table = NULL;
    for (pass = 0; pass < 2; ++pass) {
	size_t str_size = 0;
	if (src->term_names != NULL) {
	    if (pass) {
		dst->term_names = new_table + str_size;
		strcpy(dst->term_names + str_size, src->term_names);
	    }
	    str_size += strlen(src->term_names) + 1;
	}
	for_each_string(i, src) {
	    if (VALID_STRING(src->Strings[i])) {
		if (pass) {
		    strcpy(new_table + str_size, src->Strings[i]);
		    dst->Strings[i] = new_table + str_size;
		}
		str_size += strlen(src->Strings[i]) + 1;
	    }
	}
	if (pass) {
	    dst->str_table = new_table;
	} else {
	    ++str_size;
	    if ((new_table = malloc(str_size)) == NULL)
		_nc_err_abort(MSG_NO_MEMORY);
	}
    }

#if NCURSES_EXT_NUMBERS
    if ((mode & dstINT) == 0) {
	DEBUG(2, ("...convert int ->short"));
	TYPE_MALLOC(short, NUM_NUMBERS(dst), oldptr);
	((TERMTYPE *) dst)->Numbers = oldptr;
    } else {
	DEBUG(2, ("...copy without changing size"));
	TYPE_MALLOC(int, NUM_NUMBERS(dst), newptr);
	dst->Numbers = newptr;
    }
    if ((mode == srcINT) && (oldptr != 0)) {
	DEBUG(2, ("...copy int ->short"));
	for (i = 0; i < NUM_NUMBERS(dst); ++i) {
	    if (src->Numbers[i] > MAX_OF_TYPE(short)) {
		oldptr[i] = MAX_OF_TYPE(short);
	    } else {
		oldptr[i] = (short) src->Numbers[i];
	    }
	}
    } else if ((mode == dstINT) && (newptr != 0)) {
	DEBUG(2, ("...copy short ->int"));
	for (i = 0; i < NUM_NUMBERS(dst); ++i) {
	    newptr[i] = ((const short *) (src->Numbers))[i];
	}
    } else {
	DEBUG(2, ("...copy %s without change",
		  (mode & dstINT)
		  ? "int"
		  : "short"));
	memcpy(dst->Numbers,
	       src->Numbers,
	       NUM_NUMBERS(dst) * ((mode & dstINT)
				   ? sizeof(int)
				   : sizeof(short)));
    }
#else
    (void) mode;
    TYPE_MALLOC(short, NUM_NUMBERS(dst), dst->Numbers);
    memcpy(dst->Numbers,
	   src->Numbers,
	   NUM_NUMBERS(dst) * sizeof(dst->Numbers[0]));
#endif

#if NCURSES_XNAMES
    if ((i = NUM_EXT_NAMES(src)) != 0) {
	TYPE_MALLOC(char *, i, dst->ext_Names);
	memcpy(dst->ext_Names, src->ext_Names, i * sizeof(char *));

	new_table = NULL;
	for (pass = 0; pass < 2; ++pass) {
	    size_t str_size = 0;
	    char *raw_data = src->ext_str_table;
	    if (raw_data != NULL) {
		for (i = 0; i < src->ext_Strings; ++i) {
		    size_t skip = strlen(raw_data) + 1;
		    if (skip != 1) {
			if (pass) {
			    strcpy(new_table + str_size, raw_data);
			}
			str_size += skip;
			raw_data += skip;
		    }
		}
	    }
	    for (i = 0; i < NUM_EXT_NAMES(dst); ++i) {
		if (VALID_STRING(src->ext_Names[i])) {
		    if (pass) {
			strcpy(new_table + str_size, src->ext_Names[i]);
			dst->ext_Names[i] = new_table + str_size;
		    }
		    str_size += strlen(src->ext_Names[i]) + 1;
		}
	    }
	    if (pass) {
		dst->ext_str_table = new_table;
	    } else {
		++str_size;
		if ((new_table = calloc(str_size, 1)) == NULL)
		    _nc_err_abort(MSG_NO_MEMORY);
	    }
	}
    } else {
	dst->ext_Names = 0;
    }
#endif
    DEBUG(2, (T_RETURN("")));
}

 
NCURSES_EXPORT(void)
_nc_copy_termtype(TERMTYPE *dst, const TERMTYPE *src)
{
    DEBUG(2, (T_CALLED("_nc_copy_termtype(dst=%p, src=%p)"), (void *) dst,
	      (const void *) src));
    copy_termtype((TERMTYPE2 *) dst, (const TERMTYPE2 *) src, 0);
    DEBUG(2, (T_RETURN("")));
}

#if NCURSES_EXT_NUMBERS
NCURSES_EXPORT(void)
_nc_copy_termtype2(TERMTYPE2 *dst, const TERMTYPE2 *src)
{
    DEBUG(2, (T_CALLED("_nc_copy_termtype2(dst=%p, src=%p)"), (void *) dst,
	      (const void *) src));
    copy_termtype(dst, src, srcINT | dstINT);
    DEBUG(2, (T_RETURN("")));
}

 
NCURSES_EXPORT(void)
_nc_export_termtype2(TERMTYPE *dst, const TERMTYPE2 *src)
{
    DEBUG(2, (T_CALLED("_nc_export_termtype2(dst=%p, src=%p)"), (void *)
	      dst, (const void *) src));
    copy_termtype((TERMTYPE2 *) dst, src, srcINT);
    DEBUG(2, (T_RETURN("")));
}
#endif  
