 

#ifndef _@GUARD_PREFIX@_STDINT_H

#if __GNUC__ >= 3
@PRAGMA_SYSTEM_HEADER@
#endif
@PRAGMA_COLUMNS@

 
#define _GL_JUST_INCLUDE_SYSTEM_INTTYPES_H

 
#if defined __ANDROID__ && defined _GL_INCLUDING_SYS_TYPES_H
# @INCLUDE_NEXT@ @NEXT_STDINT_H@
#else

 

#if @HAVE_STDINT_H@
# if defined __sgi && ! defined __c99
    
#  define __STDINT_H__
# endif

   
# ifdef __cplusplus
#  ifndef __STDC_CONSTANT_MACROS
#   define __STDC_CONSTANT_MACROS 1
#  endif
#  ifndef __STDC_LIMIT_MACROS
#   define __STDC_LIMIT_MACROS 1
#  endif
# endif

   
# @INCLUDE_NEXT@ @NEXT_STDINT_H@
#endif

#if ! defined _@GUARD_PREFIX@_STDINT_H && ! defined _GL_JUST_INCLUDE_SYSTEM_STDINT_H
#define _@GUARD_PREFIX@_STDINT_H

 
#include <limits.h>

 
#if @GNULIBHEADERS_OVERRIDE_WINT_T@
# undef WINT_MIN
# undef WINT_MAX
# define WINT_MIN 0x0U
# define WINT_MAX 0xffffffffU
#endif

#if ! @HAVE_C99_STDINT_H@

 
# if @HAVE_SYS_TYPES_H@ && ! defined _AIX
#  include <sys/types.h>
# endif

# if @HAVE_INTTYPES_H@
   
#  include <inttypes.h>
# elif @HAVE_SYS_INTTYPES_H@
   
#  include <sys/inttypes.h>
# endif

# if @HAVE_SYS_BITYPES_H@ && ! defined __BIT_TYPES_DEFINED__
   
#  include <sys/bitypes.h>
# endif

# undef _GL_JUST_INCLUDE_SYSTEM_INTTYPES_H

 

 
# define _STDINT_UNSIGNED_MIN(bits, zero) \
    (zero)
# define _STDINT_SIGNED_MIN(bits, zero) \
    (~ _STDINT_MAX (1, bits, zero))

# define _STDINT_MAX(signed, bits, zero) \
    (((((zero) + 1) << ((bits) ? (bits) - 1 - (signed) : 0)) - 1) * 2 + 1)

#if !GNULIB_defined_stdint_types

 

 

# undef int8_t
# undef uint8_t
typedef signed char gl_int8_t;
typedef unsigned char gl_uint8_t;
# define int8_t gl_int8_t
# define uint8_t gl_uint8_t

# undef int16_t
# undef uint16_t
typedef short int gl_int16_t;
typedef unsigned short int gl_uint16_t;
# define int16_t gl_int16_t
# define uint16_t gl_uint16_t

# undef int32_t
# undef uint32_t
typedef int gl_int32_t;
typedef unsigned int gl_uint32_t;
# define int32_t gl_int32_t
# define uint32_t gl_uint32_t

 

# ifdef INT64_MAX
#  define GL_INT64_T
# else
 
#  if LONG_MAX >> 31 >> 31 == 1
#   undef int64_t
typedef long int gl_int64_t;
#   define int64_t gl_int64_t
#   define GL_INT64_T
#  elif defined _MSC_VER
#   undef int64_t
typedef __int64 gl_int64_t;
#   define int64_t gl_int64_t
#   define GL_INT64_T
#  else
#   undef int64_t
typedef long long int gl_int64_t;
#   define int64_t gl_int64_t
#   define GL_INT64_T
#  endif
# endif

# ifdef UINT64_MAX
#  define GL_UINT64_T
# else
#  if ULONG_MAX >> 31 >> 31 >> 1 == 1
#   undef uint64_t
typedef unsigned long int gl_uint64_t;
#   define uint64_t gl_uint64_t
#   define GL_UINT64_T
#  elif defined _MSC_VER
#   undef uint64_t
typedef unsigned __int64 gl_uint64_t;
#   define uint64_t gl_uint64_t
#   define GL_UINT64_T
#  else
#   undef uint64_t
typedef unsigned long long int gl_uint64_t;
#   define uint64_t gl_uint64_t
#   define GL_UINT64_T
#  endif
# endif

 
# define _UINT8_T
# define _UINT32_T
# define _UINT64_T


 

 

# undef int_least8_t
# undef uint_least8_t
# undef int_least16_t
# undef uint_least16_t
# undef int_least32_t
# undef uint_least32_t
# undef int_least64_t
# undef uint_least64_t
# define int_least8_t int8_t
# define uint_least8_t uint8_t
# define int_least16_t int16_t
# define uint_least16_t uint16_t
# define int_least32_t int32_t
# define uint_least32_t uint32_t
# ifdef GL_INT64_T
#  define int_least64_t int64_t
# endif
# ifdef GL_UINT64_T
#  define uint_least64_t uint64_t
# endif

 

 

 

# undef int_fast8_t
# undef uint_fast8_t
# undef int_fast16_t
# undef uint_fast16_t
# undef int_fast32_t
# undef uint_fast32_t
# undef int_fast64_t
# undef uint_fast64_t
typedef signed char gl_int_fast8_t;
typedef unsigned char gl_uint_fast8_t;

# ifdef __sun
 
typedef int gl_int_fast32_t;
typedef unsigned int gl_uint_fast32_t;
# else
typedef long int gl_int_fast32_t;
typedef unsigned long int gl_uint_fast32_t;
# endif
typedef gl_int_fast32_t gl_int_fast16_t;
typedef gl_uint_fast32_t gl_uint_fast16_t;

# define int_fast8_t gl_int_fast8_t
# define uint_fast8_t gl_uint_fast8_t
# define int_fast16_t gl_int_fast16_t
# define uint_fast16_t gl_uint_fast16_t
# define int_fast32_t gl_int_fast32_t
# define uint_fast32_t gl_uint_fast32_t
# ifdef GL_INT64_T
#  define int_fast64_t int64_t
# endif
# ifdef GL_UINT64_T
#  define uint_fast64_t uint64_t
# endif

 

 
# if !((defined __KLIBC__ && defined _INTPTR_T_DECLARED) \
       || defined __MINGW32__)
#  undef intptr_t
#  undef uintptr_t
#  ifdef _WIN64
typedef long long int gl_intptr_t;
typedef unsigned long long int gl_uintptr_t;
#  else
typedef long int gl_intptr_t;
typedef unsigned long int gl_uintptr_t;
#  endif
#  define intptr_t gl_intptr_t
#  define uintptr_t gl_uintptr_t
# endif

 

 

 

# ifndef INTMAX_MAX
#  undef INTMAX_C
#  undef intmax_t
#  if LONG_MAX >> 30 == 1
typedef long long int gl_intmax_t;
#   define intmax_t gl_intmax_t
#  elif defined GL_INT64_T
#   define intmax_t int64_t
#  else
typedef long int gl_intmax_t;
#   define intmax_t gl_intmax_t
#  endif
# endif

# ifndef UINTMAX_MAX
#  undef UINTMAX_C
#  undef uintmax_t
#  if ULONG_MAX >> 31 == 1
typedef unsigned long long int gl_uintmax_t;
#   define uintmax_t gl_uintmax_t
#  elif defined GL_UINT64_T
#   define uintmax_t uint64_t
#  else
typedef unsigned long int gl_uintmax_t;
#   define uintmax_t gl_uintmax_t
#  endif
# endif

 
typedef int _verify_intmax_size[sizeof (intmax_t) == sizeof (uintmax_t)
                                ? 1 : -1];

# define GNULIB_defined_stdint_types 1
# endif  

 

 

 

# undef INT8_MIN
# undef INT8_MAX
# undef UINT8_MAX
# define INT8_MIN  (~ INT8_MAX)
# define INT8_MAX  127
# define UINT8_MAX  255

# undef INT16_MIN
# undef INT16_MAX
# undef UINT16_MAX
# define INT16_MIN  (~ INT16_MAX)
# define INT16_MAX  32767
# define UINT16_MAX  65535

# undef INT32_MIN
# undef INT32_MAX
# undef UINT32_MAX
# define INT32_MIN  (~ INT32_MAX)
# define INT32_MAX  2147483647
# define UINT32_MAX  4294967295U

# if defined GL_INT64_T && ! defined INT64_MAX
 
#  define INT64_MIN  (- INTMAX_C (1) << 63)
#  define INT64_MAX  INTMAX_C (9223372036854775807)
# endif

# if defined GL_UINT64_T && ! defined UINT64_MAX
#  define UINT64_MAX  UINTMAX_C (18446744073709551615)
# endif

 

 

# undef INT_LEAST8_MIN
# undef INT_LEAST8_MAX
# undef UINT_LEAST8_MAX
# define INT_LEAST8_MIN  INT8_MIN
# define INT_LEAST8_MAX  INT8_MAX
# define UINT_LEAST8_MAX  UINT8_MAX

# undef INT_LEAST16_MIN
# undef INT_LEAST16_MAX
# undef UINT_LEAST16_MAX
# define INT_LEAST16_MIN  INT16_MIN
# define INT_LEAST16_MAX  INT16_MAX
# define UINT_LEAST16_MAX  UINT16_MAX

# undef INT_LEAST32_MIN
# undef INT_LEAST32_MAX
# undef UINT_LEAST32_MAX
# define INT_LEAST32_MIN  INT32_MIN
# define INT_LEAST32_MAX  INT32_MAX
# define UINT_LEAST32_MAX  UINT32_MAX

# undef INT_LEAST64_MIN
# undef INT_LEAST64_MAX
# ifdef GL_INT64_T
#  define INT_LEAST64_MIN  INT64_MIN
#  define INT_LEAST64_MAX  INT64_MAX
# endif

# undef UINT_LEAST64_MAX
# ifdef GL_UINT64_T
#  define UINT_LEAST64_MAX  UINT64_MAX
# endif

 

 

# undef INT_FAST8_MIN
# undef INT_FAST8_MAX
# undef UINT_FAST8_MAX
# define INT_FAST8_MIN  SCHAR_MIN
# define INT_FAST8_MAX  SCHAR_MAX
# define UINT_FAST8_MAX  UCHAR_MAX

# undef INT_FAST16_MIN
# undef INT_FAST16_MAX
# undef UINT_FAST16_MAX
# define INT_FAST16_MIN  INT_FAST32_MIN
# define INT_FAST16_MAX  INT_FAST32_MAX
# define UINT_FAST16_MAX  UINT_FAST32_MAX

# undef INT_FAST32_MIN
# undef INT_FAST32_MAX
# undef UINT_FAST32_MAX
# ifdef __sun
#  define INT_FAST32_MIN  INT_MIN
#  define INT_FAST32_MAX  INT_MAX
#  define UINT_FAST32_MAX  UINT_MAX
# else
#  define INT_FAST32_MIN  LONG_MIN
#  define INT_FAST32_MAX  LONG_MAX
#  define UINT_FAST32_MAX  ULONG_MAX
# endif

# undef INT_FAST64_MIN
# undef INT_FAST64_MAX
# ifdef GL_INT64_T
#  define INT_FAST64_MIN  INT64_MIN
#  define INT_FAST64_MAX  INT64_MAX
# endif

# undef UINT_FAST64_MAX
# ifdef GL_UINT64_T
#  define UINT_FAST64_MAX  UINT64_MAX
# endif

 

# undef INTPTR_MIN
# undef INTPTR_MAX
# undef UINTPTR_MAX
# ifdef _WIN64
#  define INTPTR_MIN  LLONG_MIN
#  define INTPTR_MAX  LLONG_MAX
#  define UINTPTR_MAX  ULLONG_MAX
# else
#  define INTPTR_MIN  LONG_MIN
#  define INTPTR_MAX  LONG_MAX
#  define UINTPTR_MAX  ULONG_MAX
# endif

 

# ifndef INTMAX_MAX
#  undef INTMAX_MIN
#  ifdef INT64_MAX
#   define INTMAX_MIN  INT64_MIN
#   define INTMAX_MAX  INT64_MAX
#  else
#   define INTMAX_MIN  INT32_MIN
#   define INTMAX_MAX  INT32_MAX
#  endif
# endif

# ifndef UINTMAX_MAX
#  ifdef UINT64_MAX
#   define UINTMAX_MAX  UINT64_MAX
#  else
#   define UINTMAX_MAX  UINT32_MAX
#  endif
# endif

 

 
# undef PTRDIFF_MIN
# undef PTRDIFF_MAX
# if @APPLE_UNIVERSAL_BUILD@
#  ifdef _LP64
#   define PTRDIFF_MIN  _STDINT_SIGNED_MIN (64, 0l)
#   define PTRDIFF_MAX  _STDINT_MAX (1, 64, 0l)
#  else
#   define PTRDIFF_MIN  _STDINT_SIGNED_MIN (32, 0)
#   define PTRDIFF_MAX  _STDINT_MAX (1, 32, 0)
#  endif
# else
#  define PTRDIFF_MIN  \
    _STDINT_SIGNED_MIN (@BITSIZEOF_PTRDIFF_T@, 0@PTRDIFF_T_SUFFIX@)
#  define PTRDIFF_MAX  \
    _STDINT_MAX (1, @BITSIZEOF_PTRDIFF_T@, 0@PTRDIFF_T_SUFFIX@)
# endif

 
# undef SIG_ATOMIC_MIN
# undef SIG_ATOMIC_MAX
# if @HAVE_SIGNED_SIG_ATOMIC_T@
#  define SIG_ATOMIC_MIN  \
    _STDINT_SIGNED_MIN (@BITSIZEOF_SIG_ATOMIC_T@, 0@SIG_ATOMIC_T_SUFFIX@)
# else
#  define SIG_ATOMIC_MIN  \
    _STDINT_UNSIGNED_MIN (@BITSIZEOF_SIG_ATOMIC_T@, 0@SIG_ATOMIC_T_SUFFIX@)
# endif
# define SIG_ATOMIC_MAX  \
   _STDINT_MAX (@HAVE_SIGNED_SIG_ATOMIC_T@, @BITSIZEOF_SIG_ATOMIC_T@, \
                0@SIG_ATOMIC_T_SUFFIX@)


 
# undef SIZE_MAX
# if @APPLE_UNIVERSAL_BUILD@
#  ifdef _LP64
#   define SIZE_MAX  _STDINT_MAX (0, 64, 0ul)
#  else
#   define SIZE_MAX  _STDINT_MAX (0, 32, 0ul)
#  endif
# else
#  define SIZE_MAX  _STDINT_MAX (0, @BITSIZEOF_SIZE_T@, 0@SIZE_T_SUFFIX@)
# endif

 
 
# if @HAVE_WCHAR_H@ && ! (defined WCHAR_MIN && defined WCHAR_MAX)
#  define _GL_JUST_INCLUDE_SYSTEM_WCHAR_H
#  include <wchar.h>
#  undef _GL_JUST_INCLUDE_SYSTEM_WCHAR_H
# endif
# undef WCHAR_MIN
# undef WCHAR_MAX
# if @HAVE_SIGNED_WCHAR_T@
#  define WCHAR_MIN  \
    _STDINT_SIGNED_MIN (@BITSIZEOF_WCHAR_T@, 0@WCHAR_T_SUFFIX@)
# else
#  define WCHAR_MIN  \
    _STDINT_UNSIGNED_MIN (@BITSIZEOF_WCHAR_T@, 0@WCHAR_T_SUFFIX@)
# endif
# define WCHAR_MAX  \
   _STDINT_MAX (@HAVE_SIGNED_WCHAR_T@, @BITSIZEOF_WCHAR_T@, 0@WCHAR_T_SUFFIX@)

 
 
# if !@GNULIBHEADERS_OVERRIDE_WINT_T@
#  undef WINT_MIN
#  undef WINT_MAX
#  if @HAVE_SIGNED_WINT_T@
#   define WINT_MIN  \
     _STDINT_SIGNED_MIN (@BITSIZEOF_WINT_T@, 0@WINT_T_SUFFIX@)
#  else
#   define WINT_MIN  \
     _STDINT_UNSIGNED_MIN (@BITSIZEOF_WINT_T@, 0@WINT_T_SUFFIX@)
#  endif
#  define WINT_MAX  \
    _STDINT_MAX (@HAVE_SIGNED_WINT_T@, @BITSIZEOF_WINT_T@, 0@WINT_T_SUFFIX@)
# endif

 

 
 

 

# undef INT8_C
# undef UINT8_C
# define INT8_C(x) x
# define UINT8_C(x) x

# undef INT16_C
# undef UINT16_C
# define INT16_C(x) x
# define UINT16_C(x) x

# undef INT32_C
# undef UINT32_C
# define INT32_C(x) x
# define UINT32_C(x) x ## U

# undef INT64_C
# undef UINT64_C
# if LONG_MAX >> 31 >> 31 == 1
#  define INT64_C(x) x##L
# elif defined _MSC_VER
#  define INT64_C(x) x##i64
# else
#  define INT64_C(x) x##LL
# endif
# if ULONG_MAX >> 31 >> 31 >> 1 == 1
#  define UINT64_C(x) x##UL
# elif defined _MSC_VER
#  define UINT64_C(x) x##ui64
# else
#  define UINT64_C(x) x##ULL
# endif

 

# ifndef INTMAX_C
#  if LONG_MAX >> 30 == 1
#   define INTMAX_C(x)   x##LL
#  elif defined GL_INT64_T
#   define INTMAX_C(x)   INT64_C(x)
#  else
#   define INTMAX_C(x)   x##L
#  endif
# endif

# ifndef UINTMAX_C
#  if ULONG_MAX >> 31 == 1
#   define UINTMAX_C(x)  x##ULL
#  elif defined GL_UINT64_T
#   define UINTMAX_C(x)  UINT64_C(x)
#  else
#   define UINTMAX_C(x)  x##UL
#  endif
# endif

#endif  

 

#if (!defined UINTMAX_WIDTH \
     && (defined _GNU_SOURCE || defined __STDC_WANT_IEC_60559_BFP_EXT__))
# ifdef INT8_MAX
#  define INT8_WIDTH _GL_INTEGER_WIDTH (INT8_MIN, INT8_MAX)
# endif
# ifdef UINT8_MAX
#  define UINT8_WIDTH _GL_INTEGER_WIDTH (0, UINT8_MAX)
# endif
# ifdef INT16_MAX
#  define INT16_WIDTH _GL_INTEGER_WIDTH (INT16_MIN, INT16_MAX)
# endif
# ifdef UINT16_MAX
#  define UINT16_WIDTH _GL_INTEGER_WIDTH (0, UINT16_MAX)
# endif
# ifdef INT32_MAX
#  define INT32_WIDTH _GL_INTEGER_WIDTH (INT32_MIN, INT32_MAX)
# endif
# ifdef UINT32_MAX
#  define UINT32_WIDTH _GL_INTEGER_WIDTH (0, UINT32_MAX)
# endif
# ifdef INT64_MAX
#  define INT64_WIDTH _GL_INTEGER_WIDTH (INT64_MIN, INT64_MAX)
# endif
# ifdef UINT64_MAX
#  define UINT64_WIDTH _GL_INTEGER_WIDTH (0, UINT64_MAX)
# endif
# define INT_LEAST8_WIDTH _GL_INTEGER_WIDTH (INT_LEAST8_MIN, INT_LEAST8_MAX)
# define UINT_LEAST8_WIDTH _GL_INTEGER_WIDTH (0, UINT_LEAST8_MAX)
# define INT_LEAST16_WIDTH _GL_INTEGER_WIDTH (INT_LEAST16_MIN, INT_LEAST16_MAX)
# define UINT_LEAST16_WIDTH _GL_INTEGER_WIDTH (0, UINT_LEAST16_MAX)
# define INT_LEAST32_WIDTH _GL_INTEGER_WIDTH (INT_LEAST32_MIN, INT_LEAST32_MAX)
# define UINT_LEAST32_WIDTH _GL_INTEGER_WIDTH (0, UINT_LEAST32_MAX)
# define INT_LEAST64_WIDTH _GL_INTEGER_WIDTH (INT_LEAST64_MIN, INT_LEAST64_MAX)
# define UINT_LEAST64_WIDTH _GL_INTEGER_WIDTH (0, UINT_LEAST64_MAX)
# define INT_FAST8_WIDTH _GL_INTEGER_WIDTH (INT_FAST8_MIN, INT_FAST8_MAX)
# define UINT_FAST8_WIDTH _GL_INTEGER_WIDTH (0, UINT_FAST8_MAX)
# define INT_FAST16_WIDTH _GL_INTEGER_WIDTH (INT_FAST16_MIN, INT_FAST16_MAX)
# define UINT_FAST16_WIDTH _GL_INTEGER_WIDTH (0, UINT_FAST16_MAX)
# define INT_FAST32_WIDTH _GL_INTEGER_WIDTH (INT_FAST32_MIN, INT_FAST32_MAX)
# define UINT_FAST32_WIDTH _GL_INTEGER_WIDTH (0, UINT_FAST32_MAX)
# define INT_FAST64_WIDTH _GL_INTEGER_WIDTH (INT_FAST64_MIN, INT_FAST64_MAX)
# define UINT_FAST64_WIDTH _GL_INTEGER_WIDTH (0, UINT_FAST64_MAX)
# define INTPTR_WIDTH _GL_INTEGER_WIDTH (INTPTR_MIN, INTPTR_MAX)
# define UINTPTR_WIDTH _GL_INTEGER_WIDTH (0, UINTPTR_MAX)
# define INTMAX_WIDTH _GL_INTEGER_WIDTH (INTMAX_MIN, INTMAX_MAX)
# define UINTMAX_WIDTH _GL_INTEGER_WIDTH (0, UINTMAX_MAX)
# define PTRDIFF_WIDTH _GL_INTEGER_WIDTH (PTRDIFF_MIN, PTRDIFF_MAX)
# define SIZE_WIDTH _GL_INTEGER_WIDTH (0, SIZE_MAX)
# define WCHAR_WIDTH _GL_INTEGER_WIDTH (WCHAR_MIN, WCHAR_MAX)
# ifdef WINT_MAX
#  define WINT_WIDTH _GL_INTEGER_WIDTH (WINT_MIN, WINT_MAX)
# endif
# ifdef SIG_ATOMIC_MAX
#  define SIG_ATOMIC_WIDTH _GL_INTEGER_WIDTH (SIG_ATOMIC_MIN, SIG_ATOMIC_MAX)
# endif
#endif  

#endif  
#endif  
#endif  
