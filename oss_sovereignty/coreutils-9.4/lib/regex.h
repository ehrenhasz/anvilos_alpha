 
#ifdef __cplusplus
extern "C" {
#endif

 
#ifdef _GNU_SOURCE
# define __USE_GNU 1
#endif

#ifdef _REGEX_LARGE_OFFSETS

 

 
typedef size_t __re_size_t;

 
typedef size_t __re_long_size_t;

#else

 
typedef unsigned int __re_size_t;
typedef unsigned long int __re_long_size_t;

#endif

 
typedef long int s_reg_t;
typedef unsigned long int active_reg_t;

 
typedef unsigned long int reg_syntax_t;

#ifdef __USE_GNU
 
# define RE_BACKSLASH_ESCAPE_IN_LISTS ((unsigned long int) 1)

 
# define RE_BK_PLUS_QM (RE_BACKSLASH_ESCAPE_IN_LISTS << 1)

 
# define RE_CHAR_CLASSES (RE_BK_PLUS_QM << 1)

 
# define RE_CONTEXT_INDEP_ANCHORS (RE_CHAR_CLASSES << 1)

 
# define RE_CONTEXT_INDEP_OPS (RE_CONTEXT_INDEP_ANCHORS << 1)

 
# define RE_CONTEXT_INVALID_OPS (RE_CONTEXT_INDEP_OPS << 1)

 
# define RE_DOT_NEWLINE (RE_CONTEXT_INVALID_OPS << 1)

 
# define RE_DOT_NOT_NULL (RE_DOT_NEWLINE << 1)

 
# define RE_HAT_LISTS_NOT_NEWLINE (RE_DOT_NOT_NULL << 1)

 
# define RE_INTERVALS (RE_HAT_LISTS_NOT_NEWLINE << 1)

 
# define RE_LIMITED_OPS (RE_INTERVALS << 1)

 
# define RE_NEWLINE_ALT (RE_LIMITED_OPS << 1)

 
# define RE_NO_BK_BRACES (RE_NEWLINE_ALT << 1)

 
# define RE_NO_BK_PARENS (RE_NO_BK_BRACES << 1)

 
# define RE_NO_BK_REFS (RE_NO_BK_PARENS << 1)

 
# define RE_NO_BK_VBAR (RE_NO_BK_REFS << 1)

 
# define RE_NO_EMPTY_RANGES (RE_NO_BK_VBAR << 1)

 
# define RE_UNMATCHED_RIGHT_PAREN_ORD (RE_NO_EMPTY_RANGES << 1)

 
# define RE_NO_POSIX_BACKTRACKING (RE_UNMATCHED_RIGHT_PAREN_ORD << 1)

 
# define RE_NO_GNU_OPS (RE_NO_POSIX_BACKTRACKING << 1)

 
# define RE_DEBUG (RE_NO_GNU_OPS << 1)

 
# define RE_INVALID_INTERVAL_ORD (RE_DEBUG << 1)

 
# define RE_ICASE (RE_INVALID_INTERVAL_ORD << 1)

 
# define RE_CARET_ANCHORS_HERE (RE_ICASE << 1)

 
# define RE_CONTEXT_INVALID_DUP (RE_CARET_ANCHORS_HERE << 1)

 
# define RE_NO_SUB (RE_CONTEXT_INVALID_DUP << 1)
#endif

 
extern reg_syntax_t re_syntax_options;

#ifdef __USE_GNU
 
 
# define RE_SYNTAX_EMACS 0

# define RE_SYNTAX_AWK							\
  (RE_BACKSLASH_ESCAPE_IN_LISTS   | RE_DOT_NOT_NULL			\
   | RE_NO_BK_PARENS              | RE_NO_BK_REFS			\
   | RE_NO_BK_VBAR                | RE_NO_EMPTY_RANGES			\
   | RE_DOT_NEWLINE		  | RE_CONTEXT_INDEP_ANCHORS		\
   | RE_CHAR_CLASSES							\
   | RE_UNMATCHED_RIGHT_PAREN_ORD | RE_NO_GNU_OPS)

# define RE_SYNTAX_GNU_AWK						\
  ((RE_SYNTAX_POSIX_EXTENDED | RE_BACKSLASH_ESCAPE_IN_LISTS		\
    | RE_INVALID_INTERVAL_ORD)						\
   & ~(RE_DOT_NOT_NULL | RE_CONTEXT_INDEP_OPS				\
      | RE_CONTEXT_INVALID_OPS ))

# define RE_SYNTAX_POSIX_AWK						\
  (RE_SYNTAX_POSIX_EXTENDED | RE_BACKSLASH_ESCAPE_IN_LISTS		\
   | RE_INTERVALS	    | RE_NO_GNU_OPS				\
   | RE_INVALID_INTERVAL_ORD)

# define RE_SYNTAX_GREP							\
  ((RE_SYNTAX_POSIX_BASIC | RE_NEWLINE_ALT)				\
   & ~(RE_CONTEXT_INVALID_DUP | RE_DOT_NOT_NULL))

# define RE_SYNTAX_EGREP						\
  ((RE_SYNTAX_POSIX_EXTENDED | RE_INVALID_INTERVAL_ORD | RE_NEWLINE_ALT) \
   & ~(RE_CONTEXT_INVALID_OPS | RE_DOT_NOT_NULL))

 
# define RE_SYNTAX_POSIX_EGREP						\
  RE_SYNTAX_EGREP

 
# define RE_SYNTAX_ED RE_SYNTAX_POSIX_BASIC

# define RE_SYNTAX_SED RE_SYNTAX_POSIX_BASIC

 
# define _RE_SYNTAX_POSIX_COMMON					\
  (RE_CHAR_CLASSES | RE_DOT_NEWLINE      | RE_DOT_NOT_NULL		\
   | RE_INTERVALS  | RE_NO_EMPTY_RANGES)

# define RE_SYNTAX_POSIX_BASIC						\
  (_RE_SYNTAX_POSIX_COMMON | RE_BK_PLUS_QM | RE_CONTEXT_INVALID_DUP)

 
# define RE_SYNTAX_POSIX_MINIMAL_BASIC					\
  (_RE_SYNTAX_POSIX_COMMON | RE_LIMITED_OPS)

# define RE_SYNTAX_POSIX_EXTENDED					\
  (_RE_SYNTAX_POSIX_COMMON  | RE_CONTEXT_INDEP_ANCHORS			\
   | RE_CONTEXT_INDEP_OPS   | RE_NO_BK_BRACES				\
   | RE_NO_BK_PARENS        | RE_NO_BK_VBAR				\
   | RE_CONTEXT_INVALID_OPS | RE_UNMATCHED_RIGHT_PAREN_ORD)

 
# define RE_SYNTAX_POSIX_MINIMAL_EXTENDED				\
  (_RE_SYNTAX_POSIX_COMMON  | RE_CONTEXT_INDEP_ANCHORS			\
   | RE_CONTEXT_INVALID_OPS | RE_NO_BK_BRACES				\
   | RE_NO_BK_PARENS        | RE_NO_BK_REFS				\
   | RE_NO_BK_VBAR	    | RE_UNMATCHED_RIGHT_PAREN_ORD)
 

 
# ifdef _REGEX_INCLUDE_LIMITS_H
#  include <limits.h>
# endif
# ifdef RE_DUP_MAX
#  undef RE_DUP_MAX
# endif

 
# define RE_DUP_MAX (0x7fff)
#endif


 

 
#define REG_EXTENDED 1

 
#define REG_ICASE (1 << 1)

 
#define REG_NEWLINE (1 << 2)

 
#define REG_NOSUB (1 << 3)


 

 
#define REG_NOTBOL 1

 
#define REG_NOTEOL (1 << 1)

 
#define REG_STARTEND (1 << 2)


 

typedef enum
{
  _REG_ENOSYS = -1,	 
  _REG_NOERROR = 0,	 
  _REG_NOMATCH,		 

   
  _REG_BADPAT,		 
  _REG_ECOLLATE,	 
  _REG_ECTYPE,		 
  _REG_EESCAPE,		 
  _REG_ESUBREG,		 
  _REG_EBRACK,		 
  _REG_EPAREN,		 
  _REG_EBRACE,		 
  _REG_BADBR,		 
  _REG_ERANGE,		 
  _REG_ESPACE,		 
  _REG_BADRPT,		 

   
  _REG_EEND,		 
  _REG_ESIZE,		 
  _REG_ERPAREN		 
} reg_errcode_t;

#if defined _XOPEN_SOURCE || defined __USE_XOPEN2K
# define REG_ENOSYS	_REG_ENOSYS
#endif
#define REG_NOERROR	_REG_NOERROR
#define REG_NOMATCH	_REG_NOMATCH
#define REG_BADPAT	_REG_BADPAT
#define REG_ECOLLATE	_REG_ECOLLATE
#define REG_ECTYPE	_REG_ECTYPE
#define REG_EESCAPE	_REG_EESCAPE
#define REG_ESUBREG	_REG_ESUBREG
#define REG_EBRACK	_REG_EBRACK
#define REG_EPAREN	_REG_EPAREN
#define REG_EBRACE	_REG_EBRACE
#define REG_BADBR	_REG_BADBR
#define REG_ERANGE	_REG_ERANGE
#define REG_ESPACE	_REG_ESPACE
#define REG_BADRPT	_REG_BADRPT
#define REG_EEND	_REG_EEND
#define REG_ESIZE	_REG_ESIZE
#define REG_ERPAREN	_REG_ERPAREN

 

#ifndef RE_TRANSLATE_TYPE
# define __RE_TRANSLATE_TYPE unsigned char *
# ifdef __USE_GNU
#  define RE_TRANSLATE_TYPE __RE_TRANSLATE_TYPE
# endif
#endif

#ifdef __USE_GNU
# define __REPB_PREFIX(name) name
#else
# define __REPB_PREFIX(name) __##name
#endif

struct re_pattern_buffer
{
   
  struct re_dfa_t *__REPB_PREFIX(buffer);

   
  __re_long_size_t __REPB_PREFIX(allocated);

   
  __re_long_size_t __REPB_PREFIX(used);

   
  reg_syntax_t __REPB_PREFIX(syntax);

   
  char *__REPB_PREFIX(fastmap);

   
  __RE_TRANSLATE_TYPE __REPB_PREFIX(translate);

   
  size_t re_nsub;

   
  unsigned __REPB_PREFIX(can_be_null) : 1;

   
#ifdef __USE_GNU
# define REGS_UNALLOCATED 0
# define REGS_REALLOCATE 1
# define REGS_FIXED 2
#endif
  unsigned __REPB_PREFIX(regs_allocated) : 2;

   
  unsigned __REPB_PREFIX(fastmap_accurate) : 1;

   
  unsigned __REPB_PREFIX(no_sub) : 1;

   
  unsigned __REPB_PREFIX(not_bol) : 1;

   
  unsigned __REPB_PREFIX(not_eol) : 1;

   
  unsigned __REPB_PREFIX(newline_anchor) : 1;
};

typedef struct re_pattern_buffer regex_t;

 
#ifdef _REGEX_LARGE_OFFSETS
 
typedef ssize_t regoff_t;
#else
 
typedef int regoff_t;
#endif


#ifdef __USE_GNU
 
struct re_registers
{
  __re_size_t num_regs;
  regoff_t *start;
  regoff_t *end;
};


 
# ifndef RE_NREGS
#  define RE_NREGS 30
# endif
#endif


 
typedef struct
{
  regoff_t rm_so;   
  regoff_t rm_eo;   
} regmatch_t;

 

#ifndef _REGEX_NELTS
# if (defined __STDC_VERSION__ && 199901L <= __STDC_VERSION__ \
	&& !defined __STDC_NO_VLA__)
#  define _REGEX_NELTS(n) n
# else
#  define _REGEX_NELTS(n)
# endif
#endif

#if defined __GNUC__ && 4 < __GNUC__ + (6 <= __GNUC_MINOR__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wvla"
#endif

#ifndef _Attr_access_
# ifdef __attr_access
#  define _Attr_access_(arg) __attr_access (arg)
# elif defined __GNUC__ && 10 <= __GNUC__
#  define _Attr_access_(x) __attribute__ ((__access__ x))
# else
#  define _Attr_access_(x)
# endif
#endif

#ifdef __USE_GNU
 
extern reg_syntax_t re_set_syntax (reg_syntax_t __syntax);

 
extern const char *re_compile_pattern (const char *__pattern, size_t __length,
				       struct re_pattern_buffer *__buffer)
    _Attr_access_ ((__read_only__, 1, 2));


 
extern int re_compile_fastmap (struct re_pattern_buffer *__buffer);


 
extern regoff_t re_search (struct re_pattern_buffer *__buffer,
			   const char *__String, regoff_t __length,
			   regoff_t __start, regoff_t __range,
			   struct re_registers *__regs)
    _Attr_access_ ((__read_only__, 2, 3));


 
extern regoff_t re_search_2 (struct re_pattern_buffer *__buffer,
			     const char *__string1, regoff_t __length1,
			     const char *__string2, regoff_t __length2,
			     regoff_t __start, regoff_t __range,
			     struct re_registers *__regs,
			     regoff_t __stop)
    _Attr_access_ ((__read_only__, 2, 3))
    _Attr_access_ ((__read_only__, 4, 5));


 
extern regoff_t re_match (struct re_pattern_buffer *__buffer,
			  const char *__String, regoff_t __length,
			  regoff_t __start, struct re_registers *__regs)
    _Attr_access_ ((__read_only__, 2, 3));


 
extern regoff_t re_match_2 (struct re_pattern_buffer *__buffer,
			    const char *__string1, regoff_t __length1,
			    const char *__string2, regoff_t __length2,
			    regoff_t __start, struct re_registers *__regs,
			    regoff_t __stop)
    _Attr_access_ ((__read_only__, 2, 3))
    _Attr_access_ ((__read_only__, 4, 5));


 
extern void re_set_registers (struct re_pattern_buffer *__buffer,
			      struct re_registers *__regs,
			      __re_size_t __num_regs,
			      regoff_t *__starts, regoff_t *__ends);
#endif	 

#if defined _REGEX_RE_COMP || (defined _LIBC && defined __USE_MISC)
 
extern char *re_comp (const char *);
extern int re_exec (const char *);
#endif

 
#ifndef _Restrict_
# if defined __restrict \
     || 2 < __GNUC__ + (95 <= __GNUC_MINOR__) \
     || __clang_major__ >= 3
#  define _Restrict_ __restrict
# elif 199901L <= __STDC_VERSION__ || defined restrict
#  define _Restrict_ restrict
# else
#  define _Restrict_
# endif
#endif
 
#ifndef _Restrict_arr_
# ifdef __restrict_arr
#  define _Restrict_arr_ __restrict_arr
# elif ((199901L <= __STDC_VERSION__ \
         || 3 < __GNUC__ + (1 <= __GNUC_MINOR__) \
         || __clang_major__ >= 3) \
        && !defined __cplusplus)
#  define _Restrict_arr_ _Restrict_
# else
#  define _Restrict_arr_
# endif
#endif

 
extern int regcomp (regex_t *_Restrict_ __preg,
		    const char *_Restrict_ __pattern,
		    int __cflags);

extern int regexec (const regex_t *_Restrict_ __preg,
		    const char *_Restrict_ __String, size_t __nmatch,
		    regmatch_t __pmatch[_Restrict_arr_
					_REGEX_NELTS (__nmatch)],
		    int __eflags);

extern size_t regerror (int __errcode, const regex_t *_Restrict_ __preg,
			char *_Restrict_ __errbuf, size_t __errbuf_size)
    _Attr_access_ ((__write_only__, 3, 4));

extern void regfree (regex_t *__preg);

#if defined __GNUC__ && 4 < __GNUC__ + (6 <= __GNUC_MINOR__)
# pragma GCC diagnostic pop
#endif

#ifdef __cplusplus
}
#endif	 

#endif  
