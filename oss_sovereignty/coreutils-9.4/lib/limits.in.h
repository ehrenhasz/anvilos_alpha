 

#@INCLUDE_NEXT@ @NEXT_LIMITS_H@

#else
 

#ifndef _@GUARD_PREFIX@_LIMITS_H

# define _GL_ALREADY_INCLUDING_LIMITS_H

 
# @INCLUDE_NEXT@ @NEXT_LIMITS_H@

# undef _GL_ALREADY_INCLUDING_LIMITS_H

#ifndef _@GUARD_PREFIX@_LIMITS_H
#define _@GUARD_PREFIX@_LIMITS_H

#ifndef LLONG_MIN
# if defined LONG_LONG_MIN  
#  define LLONG_MIN LONG_LONG_MIN
# elif defined LONGLONG_MIN  
#  define LLONG_MIN LONGLONG_MIN
# elif defined __GNUC__
#  define LLONG_MIN (- __LONG_LONG_MAX__ - 1LL)
# endif
#endif
#ifndef LLONG_MAX
# if defined LONG_LONG_MAX  
#  define LLONG_MAX LONG_LONG_MAX
# elif defined LONGLONG_MAX  
#  define LLONG_MAX LONGLONG_MAX
# elif defined __GNUC__
#  define LLONG_MAX __LONG_LONG_MAX__
# endif
#endif
#ifndef ULLONG_MAX
# if defined ULONG_LONG_MAX  
#  define ULLONG_MAX ULONG_LONG_MAX
# elif defined ULONGLONG_MAX  
#  define ULLONG_MAX ULONGLONG_MAX
# elif defined __GNUC__
#  define ULLONG_MAX (__LONG_LONG_MAX__ * 2ULL + 1ULL)
# endif
#endif

 
#define _GL_INTEGER_WIDTH(min, max) (((min) < 0) + _GL_COB128 (max))
#define _GL_COB128(n) (_GL_COB64 ((n) >> 31 >> 31 >> 2) + _GL_COB64 (n))
#define _GL_COB64(n) (_GL_COB32 ((n) >> 31 >> 1) + _GL_COB32 (n))
#define _GL_COB32(n) (_GL_COB16 ((n) >> 16) + _GL_COB16 (n))
#define _GL_COB16(n) (_GL_COB8 ((n) >> 8) + _GL_COB8 (n))
#define _GL_COB8(n) (_GL_COB4 ((n) >> 4) + _GL_COB4 (n))
#define _GL_COB4(n) (!!((n) & 8) + !!((n) & 4) + !!((n) & 2) + !!((n) & 1))

#ifndef WORD_BIT
 
# define WORD_BIT 32
#endif
#ifndef LONG_BIT
 
# if LONG_MAX == INT_MAX
#  define LONG_BIT 32
# else
#  define LONG_BIT 64
# endif
#endif

 
#ifndef MB_LEN_MAX
# define MB_LEN_MAX 16
#endif

 

#if (! defined ULLONG_WIDTH                                             \
     && (defined _GNU_SOURCE || defined __STDC_WANT_IEC_60559_BFP_EXT__ \
         || (defined __STDC_VERSION__ && 201710 < __STDC_VERSION__)))
# define CHAR_WIDTH _GL_INTEGER_WIDTH (CHAR_MIN, CHAR_MAX)
# define SCHAR_WIDTH _GL_INTEGER_WIDTH (SCHAR_MIN, SCHAR_MAX)
# define UCHAR_WIDTH _GL_INTEGER_WIDTH (0, UCHAR_MAX)
# define SHRT_WIDTH _GL_INTEGER_WIDTH (SHRT_MIN, SHRT_MAX)
# define USHRT_WIDTH _GL_INTEGER_WIDTH (0, USHRT_MAX)
# define INT_WIDTH _GL_INTEGER_WIDTH (INT_MIN, INT_MAX)
# define UINT_WIDTH _GL_INTEGER_WIDTH (0, UINT_MAX)
# define LONG_WIDTH _GL_INTEGER_WIDTH (LONG_MIN, LONG_MAX)
# define ULONG_WIDTH _GL_INTEGER_WIDTH (0, ULONG_MAX)
# define LLONG_WIDTH _GL_INTEGER_WIDTH (LLONG_MIN, LLONG_MAX)
# define ULLONG_WIDTH _GL_INTEGER_WIDTH (0, ULLONG_MAX)
#endif

 

#if (defined _GNU_SOURCE \
     || (defined __STDC_VERSION__ && 201710 < __STDC_VERSION__))
# if ! defined BOOL_WIDTH
#  define BOOL_WIDTH 1
#  define BOOL_MAX 1
# elif ! defined BOOL_MAX
#  define BOOL_MAX ((((1U << (BOOL_WIDTH - 1)) - 1) << 1) + 1)
# endif
#endif

 

 
#ifndef SSIZE_MAX
# ifdef _WIN64
#  define SSIZE_MAX LLONG_MAX
# else
#  define SSIZE_MAX LONG_MAX
# endif
#endif

#endif  
#endif  
#endif
