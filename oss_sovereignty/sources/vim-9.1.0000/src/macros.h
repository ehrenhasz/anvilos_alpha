




#define LT_POS(a, b) (((a).lnum != (b).lnum) \
		   ? (a).lnum < (b).lnum \
		   : (a).col != (b).col \
		       ? (a).col < (b).col \
		       : (a).coladd < (b).coladd)
#define LT_POSP(a, b) (((a)->lnum != (b)->lnum) \
		   ? (a)->lnum < (b)->lnum \
		   : (a)->col != (b)->col \
		       ? (a)->col < (b)->col \
		       : (a)->coladd < (b)->coladd)
#define EQUAL_POS(a, b) (((a).lnum == (b).lnum) && ((a).col == (b).col) && ((a).coladd == (b).coladd))
#define CLEAR_POS(a) do {(a)->lnum = 0; (a)->col = 0; (a)->coladd = 0;} while (0)
#define EMPTY_POS(a) ((a).lnum == 0 && (a).col == 0 && (a).coladd == 0)

#define LTOREQ_POS(a, b) (LT_POS(a, b) || EQUAL_POS(a, b))


#define VIM_ISWHITE(x)		((x) == ' ' || (x) == '\t')
#define IS_WHITE_OR_NUL(x)	((x) == ' ' || (x) == '\t' || (x) == NUL)
#define IS_WHITE_NL_OR_NUL(x)	((x) == ' ' || (x) == '\t' || (x) == '\n' || (x) == NUL)


#define LINEEMPTY(p) (*ml_get(p) == NUL)


#define BUFEMPTY() (curbuf->b_ml.ml_line_count == 1 && *ml_get((linenr_T)1) == NUL)


#ifdef MSWIN
#  define TOUPPER_LOC(c)	toupper_tab[(c) & 255]
#  define TOLOWER_LOC(c)	tolower_tab[(c) & 255]
#else
# ifdef BROKEN_TOUPPER
#  define TOUPPER_LOC(c)	(islower(c) ? toupper(c) : (c))
#  define TOLOWER_LOC(c)	(isupper(c) ? tolower(c) : (c))
# else
#  define TOUPPER_LOC		toupper
#  define TOLOWER_LOC		tolower
# endif
#endif


#define TOUPPER_ASC(c)	(((c) < 'a' || (c) > 'z') ? (c) : (c) - ('a' - 'A'))
#define TOLOWER_ASC(c)	(((c) < 'A' || (c) > 'Z') ? (c) : (c) + ('a' - 'A'))


#define MB_ISLOWER(c)	vim_islower(c)
#define MB_ISUPPER(c)	vim_isupper(c)
#define MB_TOLOWER(c)	vim_tolower(c)
#define MB_TOUPPER(c)	vim_toupper(c)
#define MB_CASEFOLD(c)	(enc_utf8 ? utf_fold(c) : MB_TOLOWER(c))




#define VIM_ISDIGIT(c) ((unsigned)(c) - '0' < 10)



#define ASCII_ISLOWER(c) ((unsigned)(c) - 'a' < 26)
#define ASCII_ISUPPER(c) ((unsigned)(c) - 'A' < 26)
#define ASCII_ISALPHA(c) (ASCII_ISUPPER(c) || ASCII_ISLOWER(c))
#define ASCII_ISALNUM(c) (ASCII_ISALPHA(c) || VIM_ISDIGIT(c))


#define EMPTY_IF_NULL(x) ((x) ? (x) : (char_u *)"")

#ifdef FEAT_LANGMAP

# define LANGMAP_ADJUST(c, condition) \
    do { \
	if (*p_langmap \
		&& (condition) \
		&& (p_lrm || (!p_lrm && KeyTyped)) \
		&& !KeyStuffed \
		&& (c) >= 0) \
	{ \
	    if ((c) < 256) \
		c = langmap_mapchar[c]; \
	    else \
		c = langmap_adjust_mb(c); \
	} \
    } while (0)
#else
# define LANGMAP_ADJUST(c, condition) 
#endif


#define VIM_ISBREAK(c) ((c) < 256 && breakat_flags[(char_u)(c)])


#ifdef VMS
# define mch_access(n, p)	access(vms_fixfilename(n), (p))
				
# define mch_fopen(n, p)	fopen(vms_fixfilename(n), (p))
# define mch_fstat(n, p)	fstat((n), (p))
# undef HAVE_LSTAT		
# define mch_stat(n, p)		stat(vms_fixfilename(n), (p))
#else
# ifndef MSWIN
#   define mch_access(n, p)	access((n), (p))
# endif



# ifdef MSWIN
#  define mch_fstat(n, p)	_fstat64((n), (p))
# else
#  define mch_fstat(n, p)	fstat((n), (p))
# endif

# ifdef MSWIN	
#  define mch_stat(n, p)	vim_stat((n), (p))
# else
#  ifdef STAT_IGNORES_SLASH
#   define mch_stat(n, p)	vim_stat((n), (p))
#  else
#   define mch_stat(n, p)	stat((n), (p))
#  endif
# endif
#endif

#ifdef HAVE_LSTAT
# define mch_lstat(n, p)	lstat((n), (p))
#else
# define mch_lstat(n, p)	mch_stat((n), (p))
#endif

#ifdef VMS

# define mch_open(n, m, p)	open(vms_fixfilename(n), (m), (p))
#endif


#if defined(UNIX) || defined(VMS)  
# define mch_open_rw(n, f)	mch_open((n), (f), (mode_t)0600)
#else
# if defined(MSWIN)  
#  define mch_open_rw(n, f)	mch_open((n), (f), S_IREAD | S_IWRITE)
# else
#  define mch_open_rw(n, f)	mch_open((n), (f), 0)
# endif
#endif

#ifdef STARTUPTIME
# define TIME_MSG(s) do { if (time_fd != NULL) time_msg(s, NULL); } while (0)
#else
# define TIME_MSG(s) do {  } while (0)
#endif

#define REPLACE_NORMAL(s) (((s) & REPLACE_FLAG) && !((s) & VREPLACE_FLAG))

#ifdef FEAT_ARABIC
# define ARABIC_CHAR(ch)	    (((ch) & 0xFF00) == 0x0600)
# define UTF_COMPOSINGLIKE(p1, p2)  utf_composinglike((p1), (p2))
#else
# define UTF_COMPOSINGLIKE(p1, p2)  utf_iscomposing(utf_ptr2char(p2))
#endif

#ifdef FEAT_RIGHTLEFT
    
# define CURSOR_BAR_RIGHT (curwin->w_p_rl && (!(State & MODE_CMDLINE) || cmdmsg_rl))
#endif



#define MB_PTR_ADV(p)	    p += (*mb_ptr2len)(p)

#define MB_CPTR_ADV(p)	    p += enc_utf8 ? utf_ptr2len(p) : (*mb_ptr2len)(p)

#define MB_PTR_BACK(s, p)  p -= has_mbyte ? ((*mb_head_off)(s, (p) - 1) + 1) : 1

#define MB_CPTR2LEN(p)	    (enc_utf8 ? utf_ptr2len(p) : (*mb_ptr2len)(p))

#define MB_COPY_CHAR(f, t) do { if (has_mbyte) mb_copy_char(&(f), &(t)); else *(t)++ = *(f)++; } while (0)
#define MB_CHARLEN(p)	    (has_mbyte ? mb_charlen(p) : (int)STRLEN(p))
#define MB_CHAR2LEN(c)	    (has_mbyte ? mb_char2len(c) : 1)
#define PTR2CHAR(p)	    (has_mbyte ? mb_ptr2char(p) : (int)*(p))
#define MB_CHAR2BYTES(c, b) do { if (has_mbyte) (b) += (*mb_char2bytes)((c), (b)); else *(b)++ = (c); } while (0)

#ifdef FEAT_AUTOCHDIR
# define DO_AUTOCHDIR do { if (p_acd) do_autochdir(); } while (0)
#else
# define DO_AUTOCHDIR do {  } while (0)
#endif

#define RESET_BINDING(wp)  do { (wp)->w_p_scb = FALSE; (wp)->w_p_crb = FALSE; \
			    } while (0)

#ifdef FEAT_DIFF
# define PLINES_NOFILL(x) plines_nofill(x)
# define PLINES_WIN_NOFILL(w, l, h) plines_win_nofill((w), (l), (h))
#else
# define PLINES_NOFILL(x) plines(x)
# define PLINES_WIN_NOFILL(w, l, h) plines_win((w), (l), (h))
#endif

#if defined(FEAT_JOB_CHANNEL) || defined(FEAT_CLIENTSERVER)
# define MESSAGE_QUEUE
#endif

#include <float.h>
#if defined(HAVE_MATH_H)
  
# include <math.h>
#endif

#if defined(FEAT_EVAL)
# ifdef USING_FLOAT_STUFF
#  ifdef MSWIN
#   ifndef isnan
#    define isnan(x) _isnan(x)
     static __inline int isinf(double x)
	{ return !_finite(x) && !_isnan(x); }
#   endif
#  else
#   ifndef HAVE_ISNAN
     static inline int isnan(double x)
	{ return x != x; }
#   endif
#   ifndef HAVE_ISINF
     static inline int isinf(double x)
	{ return !isnan(x) && isnan(x - x); }
#   endif
#  endif
#  if !defined(INFINITY)
#   if defined(DBL_MAX)
#    ifdef VMS
#     define INFINITY DBL_MAX
#    else
#     define INFINITY (DBL_MAX+DBL_MAX)
#    endif
#   else
#    define INFINITY (1.0 / 0.0)
#   endif
#  endif
#  if !defined(NAN)
#   define NAN (INFINITY-INFINITY)
#  endif
#  if !defined(DBL_EPSILON)
#   define DBL_EPSILON 2.2204460492503131e-16
#  endif
# endif
#endif

#ifdef FEAT_EVAL
# define FUNCARG(fp, j)	((char_u **)(fp->uf_args.ga_data))[j]
#endif


#define DI2HIKEY(di) ((di)->di_key)
#define HIKEY2DI(p)  ((dictitem_T *)((p) - offsetof(dictitem_T, di_key)))
#define HI2DI(hi)     HIKEY2DI((hi)->hi_key)


#ifdef FEAT_GUI
# define mch_enable_flush()	gui_enable_flush()
# define mch_disable_flush()	gui_disable_flush()
#else
# define mch_enable_flush()
# define mch_disable_flush()
#endif


#define VIM_CLEAR(p) \
    do { \
	if ((p) != NULL) \
	{ \
	    vim_free(p); \
	    (p) = NULL; \
	} \
    } while (0)


#define IS_USER_CMDIDX(idx) ((int)(idx) < 0)


#ifdef FEAT_PROP_POPUP
# define WIN_IS_POPUP(wp) ((wp)->w_popup_flags != 0)
# define ERROR_IF_POPUP_WINDOW error_if_popup_window(FALSE)
# define ERROR_IF_ANY_POPUP_WINDOW error_if_popup_window(TRUE)
#else
# define WIN_IS_POPUP(wp) 0
# define ERROR_IF_POPUP_WINDOW 0
# define ERROR_IF_ANY_POPUP_WINDOW 0
#endif
#if defined(FEAT_PROP_POPUP) && defined(FEAT_TERMINAL)
# define ERROR_IF_TERM_POPUP_WINDOW error_if_term_popup_window()
#else
# define ERROR_IF_TERM_POPUP_WINDOW 0
#endif


#ifdef ABORT_ON_INTERNAL_ERROR
# define ESTACK_CHECK_DECLARATION int estack_len_before
# define ESTACK_CHECK_SETUP do { estack_len_before = exestack.ga_len; } while (0)
# define ESTACK_CHECK_NOW \
    do { \
	if (estack_len_before != exestack.ga_len) \
	    siemsg("Exestack length expected: %d, actual: %d", estack_len_before, exestack.ga_len); \
    } while (0)
# define CHECK_CURBUF \
    do { \
	if (curwin != NULL && curwin->w_buffer != curbuf) \
	    iemsg("curbuf != curwin->w_buffer"); \
    } while (0)
#else
# define ESTACK_CHECK_DECLARATION do {  } while (0)
# define ESTACK_CHECK_SETUP do {  } while (0)
# define ESTACK_CHECK_NOW do {  } while (0)
# define CHECK_CURBUF do {  } while (0)
#endif


#define CHECK_LIST_MATERIALIZE(l) \
    do { \
	if ((l)->lv_first == &range_list_item) \
	    range_list_materialize(l); \
    } while (0)


#define GA_GROW_FAILS(gap, n) unlikely((((gap)->ga_maxlen - (gap)->ga_len < (n)) ? ga_grow_inner((gap), (n)) : OK) == FAIL)

#define GA_GROW_OK(gap, n) likely((((gap)->ga_maxlen - (gap)->ga_len < (n)) ? ga_grow_inner((gap), (n)) : OK) == OK)

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif


#define ARRAY_LENGTH(a) (sizeof(a) / sizeof((a)[0]))

#ifdef FEAT_MENU
#define FOR_ALL_MENUS(m) \
    for ((m) = root_menu; (m) != NULL; (m) = (m)->next)
#define FOR_ALL_CHILD_MENUS(p, c) \
    for ((c) = (p)->children; (c) != NULL; (c) = (c)->next)
#endif

#define FOR_ALL_WINDOWS(wp) \
    for ((wp) = firstwin; (wp) != NULL; (wp) = (wp)->w_next)
#define FOR_ALL_FRAMES(frp, first_frame) \
    for ((frp) = first_frame; (frp) != NULL; (frp) = (frp)->fr_next)
#define FOR_ALL_TABPAGES(tp) \
    for ((tp) = first_tabpage; (tp) != NULL; (tp) = (tp)->tp_next)
#define FOR_ALL_WINDOWS_IN_TAB(tp, wp) \
    for ((wp) = ((tp) == NULL || (tp) == curtab) \
	    ? firstwin : (tp)->tp_firstwin; (wp); (wp) = (wp)->w_next)

#define FOR_ALL_TAB_WINDOWS(tp, wp) \
    for ((tp) = first_tabpage; (tp) != NULL; (tp) = (tp)->tp_next) \
	for ((wp) = ((tp) == curtab) \
		? firstwin : (tp)->tp_firstwin; (wp); (wp) = (wp)->w_next)

#define FOR_ALL_POPUPWINS(wp) \
    for ((wp) = first_popupwin; (wp) != NULL; (wp) = (wp)->w_next)
#define FOR_ALL_POPUPWINS_IN_TAB(tp, wp) \
    for ((wp) = (tp)->tp_first_popupwin; (wp) != NULL; (wp) = (wp)->w_next)

#define FOR_ALL_BUFFERS(buf) \
    for ((buf) = firstbuf; (buf) != NULL; (buf) = (buf)->b_next)

#define FOR_ALL_BUF_WININFO(buf, wip) \
    for ((wip) = (buf)->b_wininfo; (wip) != NULL; (wip) = (wip)->wi_next)


#define FOR_ALL_SIGNS_IN_BUF(buf, sign) \
    for ((sign) = (buf)->b_signlist; (sign) != NULL; (sign) = (sign)->se_next)

#ifdef FEAT_SPELL
#define FOR_ALL_SPELL_LANGS(slang) \
    for ((slang) = first_lang; (slang) != NULL; (slang) = (slang)->sl_next)
#endif


#define FOR_ALL_LIST_ITEMS(l, li) \
    for ((li) = (l) == NULL ? NULL : (l)->lv_first; (li) != NULL; (li) = (li)->li_next)


#define FOR_ALL_HASHTAB_ITEMS(ht, hi, todo) \
    for ((hi) = (ht)->ht_array; (todo) > 0; ++(hi))
