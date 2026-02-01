 

 

#if __GNUC__ >= 3
@PRAGMA_SYSTEM_HEADER@
#endif
@PRAGMA_COLUMNS@

#if defined __need_wchar_t || defined __need_size_t  \
  || defined __need_ptrdiff_t || defined __need_NULL \
  || defined __need_wint_t
 

# if !(defined _@GUARD_PREFIX@_STDDEF_H && defined _@GUARD_PREFIX@_STDDEF_WINT_T)
#  ifdef __need_wint_t
#   define _@GUARD_PREFIX@_STDDEF_WINT_T
#  endif
#  @INCLUDE_NEXT@ @NEXT_STDDEF_H@
    
#  undef __need_wchar_t
#  undef __need_size_t
#  undef __need_ptrdiff_t
#  undef __need_NULL
#  undef __need_wint_t
# endif

#else
 

# ifndef _@GUARD_PREFIX@_STDDEF_H

 
#  if defined _AIX && defined __LP64__ && !@HAVE_MAX_ALIGN_T@
#   if !GNULIB_defined_max_align_t
#    ifdef _MAX_ALIGN_T
 
typedef long rpl_max_align_t;
#     define max_align_t rpl_max_align_t
#    else
 
typedef long max_align_t;
#     define _MAX_ALIGN_T
#    endif
#    define __CLANG_MAX_ALIGN_T_DEFINED
#    define GNULIB_defined_max_align_t 1
#   endif
#  endif

 

#  @INCLUDE_NEXT@ @NEXT_STDDEF_H@

 
#  if (@REPLACE_NULL@ \
       && (!defined _@GUARD_PREFIX@_STDDEF_H || defined _@GUARD_PREFIX@_STDDEF_WINT_T))
#   undef NULL
#   ifdef __cplusplus
    
#    if __GNUG__ >= 3
     
#     define NULL __null
#    else
#     define NULL 0L
#    endif
#   else
#    define NULL ((void *) 0)
#   endif
#  endif

#  ifndef _@GUARD_PREFIX@_STDDEF_H
#   define _@GUARD_PREFIX@_STDDEF_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#if !@HAVE_WCHAR_T@
# define wchar_t int
#endif

 
#if (defined _MSC_VER || (defined __KLIBC__ && !defined __LIBCN__)) \
    && defined __cplusplus
# include <cstddef>
#else
# if ! (@HAVE_MAX_ALIGN_T@ || (defined _GCC_MAX_ALIGN_T && !defined __clang__))
#  if !GNULIB_defined_max_align_t
 
#   if defined __GNUC__ || (__clang_major__ >= 4)
#    define _GL_STDDEF_ALIGNAS(type) \
       __attribute__ ((__aligned__ (__alignof__ (type))))
#   else
#    define _GL_STDDEF_ALIGNAS(type)  
#   endif
typedef union
{
  char *__p _GL_STDDEF_ALIGNAS (char *);
  double __d _GL_STDDEF_ALIGNAS (double);
  long double __ld _GL_STDDEF_ALIGNAS (long double);
  long int __i _GL_STDDEF_ALIGNAS (long int);
} rpl_max_align_t;
#   define max_align_t rpl_max_align_t
#   define __CLANG_MAX_ALIGN_T_DEFINED
#   define GNULIB_defined_max_align_t 1
#  endif
# endif
#endif

 
#ifndef unreachable

 
# ifndef _GL_HAS_BUILTIN_UNREACHABLE
#  if defined __clang_major__ && __clang_major__ < 5
#   define _GL_HAS_BUILTIN_UNREACHABLE 0
#  elif 4 < __GNUC__ + (5 <= __GNUC_MINOR__)
#   define _GL_HAS_BUILTIN_UNREACHABLE 1
#  elif defined __has_builtin
#   define _GL_HAS_BUILTIN_UNREACHABLE __has_builtin (__builtin_unreachable)
#  else
#   define _GL_HAS_BUILTIN_UNREACHABLE 0
#  endif
# endif

# if _GL_HAS_BUILTIN_UNREACHABLE
#  define unreachable() __builtin_unreachable ()
# elif 1200 <= _MSC_VER
#  define unreachable() __assume (0)
# else
 
extern
#  if defined __cplusplus
"C"
#  endif
_Noreturn
void abort (void)
#  if defined __cplusplus && (__GLIBC__ >= 2)
throw ()
#  endif
;
#  define unreachable() abort ()
# endif

#endif

#  endif  
# endif  
#endif  
