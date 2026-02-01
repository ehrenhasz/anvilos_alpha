 
# define lock_lock(lock) ((void) dfa)
# define lock_unlock(lock) ((void) 0)
#endif

 
#if !defined _LIBC && ! (defined isblank || (HAVE_ISBLANK && HAVE_DECL_ISBLANK))
# define isblank(ch) ((ch) == ' ' || (ch) == '\t')
#endif

 
#ifndef _LIBC
# undef isascii
# define isascii(c) (((c) & ~0x7f) == 0)
#endif

#ifdef _LIBC
# ifndef _RE_DEFINE_LOCALE_FUNCTIONS
#  define _RE_DEFINE_LOCALE_FUNCTIONS 1
#   include <locale/localeinfo.h>
#   include <locale/coll-lookup.h>
# endif
#endif

 
#if (HAVE_LIBINTL_H && ENABLE_NLS) || defined _LIBC
# include <libintl.h>
# ifdef _LIBC
#  undef gettext
#  define gettext(msgid) \
  __dcgettext (_libc_intl_domainname, msgid, LC_MESSAGES)
# endif
#else
# undef gettext
# define gettext(msgid) (msgid)
#endif

#ifndef gettext_noop
 
# define gettext_noop(String) String
#endif

 
#define ASCII_CHARS 0x80

 
#define SBC_MAX (UCHAR_MAX + 1)

#define COLL_ELEM_LEN_MAX 8

 
#define NEWLINE_CHAR '\n'
#define WIDE_NEWLINE_CHAR L'\n'

 
#ifndef _LIBC
# undef __wctype
# undef __iswalnum
# undef __iswctype
# undef __towlower
# undef __towupper
# define __wctype wctype
# define __iswalnum iswalnum
# define __iswctype iswctype
# define __towlower towlower
# define __towupper towupper
# define __btowc btowc
# define __mbrtowc mbrtowc
# define __wcrtomb wcrtomb
# define __regfree regfree
#endif  

 

#ifndef ULONG_WIDTH
# define ULONG_WIDTH REGEX_UINTEGER_WIDTH (ULONG_MAX)
 
# define REGEX_UINTEGER_WIDTH(max) REGEX_COB128 (max)
# define REGEX_COB128(n) (REGEX_COB64 ((n) >> 31 >> 31 >> 2) + REGEX_COB64 (n))
# define REGEX_COB64(n) (REGEX_COB32 ((n) >> 31 >> 1) + REGEX_COB32 (n))
# define REGEX_COB32(n) (REGEX_COB16 ((n) >> 16) + REGEX_COB16 (n))
# define REGEX_COB16(n) (REGEX_COB8 ((n) >> 8) + REGEX_COB8 (n))
# define REGEX_COB8(n) (REGEX_COB4 ((n) >> 4) + REGEX_COB4 (n))
# define REGEX_COB4(n) (!!((n) & 8) + !!((n) & 4) + !!((n) & 2) + ((n) & 1))
# if ULONG_MAX / 2 + 1 != 1ul << (ULONG_WIDTH - 1)
#  error "ULONG_MAX out of range"
# endif
#endif

 
typedef regoff_t Idx;
#ifdef _REGEX_LARGE_OFFSETS
# define IDX_MAX SSIZE_MAX
#else
# define IDX_MAX INT_MAX
#endif

 
typedef __re_size_t re_hashval_t;

 
typedef unsigned long int bitset_word_t;
 
#define BITSET_WORD_MAX ULONG_MAX
 
#define BITSET_WORD_BITS ULONG_WIDTH

 
#define BITSET_WORDS ((SBC_MAX + BITSET_WORD_BITS - 1) / BITSET_WORD_BITS)

typedef bitset_word_t bitset_t[BITSET_WORDS];
typedef bitset_word_t *re_bitset_ptr_t;
typedef const bitset_word_t *re_const_bitset_ptr_t;

#define PREV_WORD_CONSTRAINT 0x0001
#define PREV_NOTWORD_CONSTRAINT 0x0002
#define NEXT_WORD_CONSTRAINT 0x0004
#define NEXT_NOTWORD_CONSTRAINT 0x0008
#define PREV_NEWLINE_CONSTRAINT 0x0010
#define NEXT_NEWLINE_CONSTRAINT 0x0020
#define PREV_BEGBUF_CONSTRAINT 0x0040
#define NEXT_ENDBUF_CONSTRAINT 0x0080
#define WORD_DELIM_CONSTRAINT 0x0100
#define NOT_WORD_DELIM_CONSTRAINT 0x0200

typedef enum
{
  INSIDE_WORD = PREV_WORD_CONSTRAINT | NEXT_WORD_CONSTRAINT,
  WORD_FIRST = PREV_NOTWORD_CONSTRAINT | NEXT_WORD_CONSTRAINT,
  WORD_LAST = PREV_WORD_CONSTRAINT | NEXT_NOTWORD_CONSTRAINT,
  INSIDE_NOTWORD = PREV_NOTWORD_CONSTRAINT | NEXT_NOTWORD_CONSTRAINT,
  LINE_FIRST = PREV_NEWLINE_CONSTRAINT,
  LINE_LAST = NEXT_NEWLINE_CONSTRAINT,
  BUF_FIRST = PREV_BEGBUF_CONSTRAINT,
  BUF_LAST = NEXT_ENDBUF_CONSTRAINT,
  WORD_DELIM = WORD_DELIM_CONSTRAINT,
  NOT_WORD_DELIM = NOT_WORD_DELIM_CONSTRAINT
} re_context_type;

typedef struct
{
  Idx alloc;
  Idx nelem;
  Idx *elems;
} re_node_set;

typedef enum
{
  NON_TYPE = 0,

   
  CHARACTER = 1,
  END_OF_RE = 2,
  SIMPLE_BRACKET = 3,
  OP_BACK_REF = 4,
  OP_PERIOD = 5,
  COMPLEX_BRACKET = 6,
  OP_UTF8_PERIOD = 7,

   
#define EPSILON_BIT 8
  OP_OPEN_SUBEXP = EPSILON_BIT | 0,
  OP_CLOSE_SUBEXP = EPSILON_BIT | 1,
  OP_ALT = EPSILON_BIT | 2,
  OP_DUP_ASTERISK = EPSILON_BIT | 3,
  ANCHOR = EPSILON_BIT | 4,

   
  CONCAT = 16,
  SUBEXP = 17,

   
  OP_DUP_PLUS = 18,
  OP_DUP_QUESTION,
  OP_OPEN_BRACKET,
  OP_CLOSE_BRACKET,
  OP_CHARSET_RANGE,
  OP_OPEN_DUP_NUM,
  OP_CLOSE_DUP_NUM,
  OP_NON_MATCH_LIST,
  OP_OPEN_COLL_ELEM,
  OP_CLOSE_COLL_ELEM,
  OP_OPEN_EQUIV_CLASS,
  OP_CLOSE_EQUIV_CLASS,
  OP_OPEN_CHAR_CLASS,
  OP_CLOSE_CHAR_CLASS,
  OP_WORD,
  OP_NOTWORD,
  OP_SPACE,
  OP_NOTSPACE,
  BACK_SLASH

} re_token_type_t;

typedef struct
{
   
  wchar_t *mbchars;

#ifdef _LIBC
   
  int32_t *coll_syms;
#endif

#ifdef _LIBC
   
  int32_t *equiv_classes;
#endif

   
#ifdef _LIBC
  uint32_t *range_starts;
  uint32_t *range_ends;
#else
  wchar_t *range_starts;
  wchar_t *range_ends;
#endif

   
  wctype_t *char_classes;

   
  unsigned int non_match : 1;

   
  Idx nmbchars;

   
  Idx ncoll_syms;

   
  Idx nequiv_classes;

   
  Idx nranges;

   
  Idx nchar_classes;
} re_charset_t;

typedef struct
{
  union
  {
    unsigned char c;		 
    re_bitset_ptr_t sbcset;	 
    re_charset_t *mbcset;	 
    Idx idx;			 
    re_context_type ctx_type;	 
  } opr;
#if (__GNUC__ >= 2 || defined __clang__) && !defined __STRICT_ANSI__
  re_token_type_t type : 8;
#else
  re_token_type_t type;
#endif
  unsigned int constraint : 10;	 
  unsigned int duplicated : 1;
  unsigned int opt_subexp : 1;
  unsigned int accept_mb : 1;
   
  unsigned int mb_partial : 1;
  unsigned int word_char : 1;
} re_token_t;

#define IS_EPSILON_NODE(type) ((type) & EPSILON_BIT)

struct re_string_t
{
   
  const unsigned char *raw_mbs;
   
  unsigned char *mbs;
   
  wint_t *wcs;
  Idx *offsets;
  mbstate_t cur_state;
   
  Idx raw_mbs_idx;
   
  Idx valid_len;
   
  Idx valid_raw_len;
   
  Idx bufs_len;
   
  Idx cur_idx;
   
  Idx raw_len;
   
  Idx len;
   
  Idx raw_stop;
   
  Idx stop;

   
  unsigned int tip_context;
   
  RE_TRANSLATE_TYPE trans;
   
  re_const_bitset_ptr_t word_char;
   
  unsigned char icase;
  unsigned char is_utf8;
  unsigned char map_notascii;
  unsigned char mbs_allocated;
  unsigned char offsets_needed;
  unsigned char newline_anchor;
  unsigned char word_ops_used;
  int mb_cur_max;
};
typedef struct re_string_t re_string_t;


struct re_dfa_t;
typedef struct re_dfa_t re_dfa_t;

#ifndef _LIBC
# define IS_IN(libc) false
#endif

#define re_string_peek_byte(pstr, offset) \
  ((pstr)->mbs[(pstr)->cur_idx + offset])
#define re_string_fetch_byte(pstr) \
  ((pstr)->mbs[(pstr)->cur_idx++])
#define re_string_first_byte(pstr, idx) \
  ((idx) == (pstr)->valid_len || (pstr)->wcs[idx] != WEOF)
#define re_string_is_single_byte_char(pstr, idx) \
  ((pstr)->wcs[idx] != WEOF && ((pstr)->valid_len == (idx) + 1 \
				|| (pstr)->wcs[(idx) + 1] != WEOF))
#define re_string_eoi(pstr) ((pstr)->stop <= (pstr)->cur_idx)
#define re_string_cur_idx(pstr) ((pstr)->cur_idx)
#define re_string_get_buffer(pstr) ((pstr)->mbs)
#define re_string_length(pstr) ((pstr)->len)
#define re_string_byte_at(pstr,idx) ((pstr)->mbs[idx])
#define re_string_skip_bytes(pstr,idx) ((pstr)->cur_idx += (idx))
#define re_string_set_index(pstr,idx) ((pstr)->cur_idx = (idx))

#ifdef _LIBC
# define MALLOC_0_IS_NONNULL 1
#elif !defined MALLOC_0_IS_NONNULL
# define MALLOC_0_IS_NONNULL 0
#endif

#ifndef MAX
# define MAX(a,b) ((a) < (b) ? (b) : (a))
#endif
#ifndef MIN
# define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#define re_malloc(t,n) ((t *) malloc ((n) * sizeof (t)))
#define re_realloc(p,t,n) ((t *) realloc (p, (n) * sizeof (t)))
#define re_free(p) free (p)

struct bin_tree_t
{
  struct bin_tree_t *parent;
  struct bin_tree_t *left;
  struct bin_tree_t *right;
  struct bin_tree_t *first;
  struct bin_tree_t *next;

  re_token_t token;

   
  Idx node_idx;
};
typedef struct bin_tree_t bin_tree_t;

#define BIN_TREE_STORAGE_SIZE \
  ((1024 - sizeof (void *)) / sizeof (bin_tree_t))

struct bin_tree_storage_t
{
  struct bin_tree_storage_t *next;
  bin_tree_t data[BIN_TREE_STORAGE_SIZE];
};
typedef struct bin_tree_storage_t bin_tree_storage_t;

#define CONTEXT_WORD 1
#define CONTEXT_NEWLINE (CONTEXT_WORD << 1)
#define CONTEXT_BEGBUF (CONTEXT_NEWLINE << 1)
#define CONTEXT_ENDBUF (CONTEXT_BEGBUF << 1)

#define IS_WORD_CONTEXT(c) ((c) & CONTEXT_WORD)
#define IS_NEWLINE_CONTEXT(c) ((c) & CONTEXT_NEWLINE)
#define IS_BEGBUF_CONTEXT(c) ((c) & CONTEXT_BEGBUF)
#define IS_ENDBUF_CONTEXT(c) ((c) & CONTEXT_ENDBUF)
#define IS_ORDINARY_CONTEXT(c) ((c) == 0)

#define IS_WORD_CHAR(ch) (isalnum (ch) || (ch) == '_')
#define IS_NEWLINE(ch) ((ch) == NEWLINE_CHAR)
#define IS_WIDE_WORD_CHAR(ch) (__iswalnum (ch) || (ch) == L'_')
#define IS_WIDE_NEWLINE(ch) ((ch) == WIDE_NEWLINE_CHAR)

#define NOT_SATISFY_PREV_CONSTRAINT(constraint,context) \
 ((((constraint) & PREV_WORD_CONSTRAINT) && !IS_WORD_CONTEXT (context)) \
  || ((constraint & PREV_NOTWORD_CONSTRAINT) && IS_WORD_CONTEXT (context)) \
  || ((constraint & PREV_NEWLINE_CONSTRAINT) && !IS_NEWLINE_CONTEXT (context))\
  || ((constraint & PREV_BEGBUF_CONSTRAINT) && !IS_BEGBUF_CONTEXT (context)))

#define NOT_SATISFY_NEXT_CONSTRAINT(constraint,context) \
 ((((constraint) & NEXT_WORD_CONSTRAINT) && !IS_WORD_CONTEXT (context)) \
  || (((constraint) & NEXT_NOTWORD_CONSTRAINT) && IS_WORD_CONTEXT (context)) \
  || (((constraint) & NEXT_NEWLINE_CONSTRAINT) && !IS_NEWLINE_CONTEXT (context)) \
  || (((constraint) & NEXT_ENDBUF_CONSTRAINT) && !IS_ENDBUF_CONTEXT (context)))

struct re_dfastate_t
{
  re_hashval_t hash;
  re_node_set nodes;
  re_node_set non_eps_nodes;
  re_node_set inveclosure;
  re_node_set *entrance_nodes;
  struct re_dfastate_t **trtable, **word_trtable;
  unsigned int context : 4;
  unsigned int halt : 1;
   
  unsigned int accept_mb : 1;
   
  unsigned int has_backref : 1;
  unsigned int has_constraint : 1;
};
typedef struct re_dfastate_t re_dfastate_t;

struct re_state_table_entry
{
  Idx num;
  Idx alloc;
  re_dfastate_t **array;
};

 

typedef struct
{
  Idx next_idx;
  Idx alloc;
  re_dfastate_t **array;
} state_array_t;

 

typedef struct
{
  Idx node;
  Idx str_idx;  
  state_array_t path;
} re_sub_match_last_t;

 

typedef struct
{
  Idx str_idx;
  Idx node;
  state_array_t *path;
  Idx alasts;  
  Idx nlasts;  
  re_sub_match_last_t **lasts;
} re_sub_match_top_t;

struct re_backref_cache_entry
{
  Idx node;
  Idx str_idx;
  Idx subexp_from;
  Idx subexp_to;
  bitset_word_t eps_reachable_subexps_map;
  char more;
};

typedef struct
{
   
  re_string_t input;
  const re_dfa_t *const dfa;
   
  int eflags;
   
  Idx match_last;
  Idx last_node;
   
  re_dfastate_t **state_log;
  Idx state_log_top;
   
  Idx nbkref_ents;
  Idx abkref_ents;
  struct re_backref_cache_entry *bkref_ents;
  int max_mb_elem_len;
  Idx nsub_tops;
  Idx asub_tops;
  re_sub_match_top_t **sub_tops;
} re_match_context_t;

typedef struct
{
  re_dfastate_t **sifted_states;
  re_dfastate_t **limited_states;
  Idx last_node;
  Idx last_str_idx;
  re_node_set limits;
} re_sift_context_t;

struct re_fail_stack_ent_t
{
  Idx idx;
  Idx node;
  regmatch_t *regs;
  re_node_set eps_via_nodes;
};

struct re_fail_stack_t
{
  Idx num;
  Idx alloc;
  struct re_fail_stack_ent_t *stack;
};

struct re_dfa_t
{
  re_token_t *nodes;
  size_t nodes_alloc;
  size_t nodes_len;
  Idx *nexts;
  Idx *org_indices;
  re_node_set *edests;
  re_node_set *eclosures;
  re_node_set *inveclosures;
  struct re_state_table_entry *state_table;
  re_dfastate_t *init_state;
  re_dfastate_t *init_state_word;
  re_dfastate_t *init_state_nl;
  re_dfastate_t *init_state_begbuf;
  bin_tree_t *str_tree;
  bin_tree_storage_t *str_tree_storage;
  re_bitset_ptr_t sb_char;
  int str_tree_storage_idx;

   
  re_hashval_t state_hash_mask;
  Idx init_node;
  Idx nbackref;  

   
  bitset_word_t used_bkref_map;
  bitset_word_t completed_bkref_map;

  unsigned int has_plural_match : 1;
   
  unsigned int has_mb_node : 1;
  unsigned int is_utf8 : 1;
  unsigned int map_notascii : 1;
  unsigned int word_ops_used : 1;
  int mb_cur_max;
  bitset_t word_char;
  reg_syntax_t syntax;
  Idx *subexp_map;
#ifdef DEBUG
  char* re_str;
#endif
  lock_define (lock)
};

#define re_node_set_init_empty(set) memset (set, '\0', sizeof (re_node_set))
#define re_node_set_remove(set,id) \
  (re_node_set_remove_at (set, re_node_set_contains (set, id) - 1))
#define re_node_set_empty(p) ((p)->nelem = 0)
#define re_node_set_free(set) re_free ((set)->elems)


typedef enum
{
  SB_CHAR,
  MB_CHAR,
  EQUIV_CLASS,
  COLL_SYM,
  CHAR_CLASS
} bracket_elem_type;

typedef struct
{
  bracket_elem_type type;
  union
  {
    unsigned char ch;
    unsigned char *name;
    wchar_t wch;
  } opr;
} bracket_elem_t;


 

static inline void
bitset_set (bitset_t set, Idx i)
{
  set[i / BITSET_WORD_BITS] |= (bitset_word_t) 1 << i % BITSET_WORD_BITS;
}

static inline void
bitset_clear (bitset_t set, Idx i)
{
  set[i / BITSET_WORD_BITS] &= ~ ((bitset_word_t) 1 << i % BITSET_WORD_BITS);
}

static inline bool
bitset_contain (const bitset_t set, Idx i)
{
  return (set[i / BITSET_WORD_BITS] >> i % BITSET_WORD_BITS) & 1;
}

static inline void
bitset_empty (bitset_t set)
{
  memset (set, '\0', sizeof (bitset_t));
}

static inline void
bitset_set_all (bitset_t set)
{
  memset (set, -1, sizeof (bitset_word_t) * (SBC_MAX / BITSET_WORD_BITS));
  if (SBC_MAX % BITSET_WORD_BITS != 0)
    set[BITSET_WORDS - 1] =
      ((bitset_word_t) 1 << SBC_MAX % BITSET_WORD_BITS) - 1;
}

static inline void
bitset_copy (bitset_t dest, const bitset_t src)
{
  memcpy (dest, src, sizeof (bitset_t));
}

static inline void
bitset_not (bitset_t set)
{
  int bitset_i;
  for (bitset_i = 0; bitset_i < SBC_MAX / BITSET_WORD_BITS; ++bitset_i)
    set[bitset_i] = ~set[bitset_i];
  if (SBC_MAX % BITSET_WORD_BITS != 0)
    set[BITSET_WORDS - 1] =
      ((((bitset_word_t) 1 << SBC_MAX % BITSET_WORD_BITS) - 1)
       & ~set[BITSET_WORDS - 1]);
}

static inline void
bitset_merge (bitset_t dest, const bitset_t src)
{
  int bitset_i;
  for (bitset_i = 0; bitset_i < BITSET_WORDS; ++bitset_i)
    dest[bitset_i] |= src[bitset_i];
}

static inline void
bitset_mask (bitset_t dest, const bitset_t src)
{
  int bitset_i;
  for (bitset_i = 0; bitset_i < BITSET_WORDS; ++bitset_i)
    dest[bitset_i] &= src[bitset_i];
}

 
static int
__attribute__ ((pure, unused))
re_string_char_size_at (const re_string_t *pstr, Idx idx)
{
  int byte_idx;
  if (pstr->mb_cur_max == 1)
    return 1;
  for (byte_idx = 1; idx + byte_idx < pstr->valid_len; ++byte_idx)
    if (pstr->wcs[idx + byte_idx] != WEOF)
      break;
  return byte_idx;
}

static wint_t
__attribute__ ((pure, unused))
re_string_wchar_at (const re_string_t *pstr, Idx idx)
{
  if (pstr->mb_cur_max == 1)
    return (wint_t) pstr->mbs[idx];
  return (wint_t) pstr->wcs[idx];
}

#ifdef _LIBC
# include <locale/weight.h>
#endif

static int
__attribute__ ((pure, unused))
re_string_elem_size_at (const re_string_t *pstr, Idx idx)
{
#ifdef _LIBC
  const unsigned char *p, *extra;
  const int32_t *table, *indirect;
  uint_fast32_t nrules = _NL_CURRENT_WORD (LC_COLLATE, _NL_COLLATE_NRULES);

  if (nrules != 0)
    {
      table = (const int32_t *) _NL_CURRENT (LC_COLLATE, _NL_COLLATE_TABLEMB);
      extra = (const unsigned char *)
	_NL_CURRENT (LC_COLLATE, _NL_COLLATE_EXTRAMB);
      indirect = (const int32_t *) _NL_CURRENT (LC_COLLATE,
						_NL_COLLATE_INDIRECTMB);
      p = pstr->mbs + idx;
      findidx (table, indirect, extra, &p, pstr->len - idx);
      return p - pstr->mbs - idx;
    }
#endif  

  return 1;
}

#ifdef _LIBC
# if __glibc_has_attribute (__fallthrough__)
#  define FALLTHROUGH __attribute__ ((__fallthrough__))
# else
#  define FALLTHROUGH ((void) 0)
# endif
#else
# include "attribute.h"
#endif

#endif  
