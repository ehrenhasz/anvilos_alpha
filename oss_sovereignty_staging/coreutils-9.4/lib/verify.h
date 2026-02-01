 

#ifndef _GL_VERIFY_H
#define _GL_VERIFY_H


 
#ifndef __cplusplus
# if (201112 <= __STDC_VERSION__ \
      || (!defined __STRICT_ANSI__ \
          && (4 < __GNUC__ + (6 <= __GNUC_MINOR__) || 5 <= __clang_major__)))
#  define _GL_HAVE__STATIC_ASSERT 1
# endif
# if (202311 <= __STDC_VERSION__ \
      || (!defined __STRICT_ANSI__ && 9 <= __GNUC__))
#  define _GL_HAVE__STATIC_ASSERT1 1
# endif
#endif

 
#ifndef _GL_HAVE__STATIC_ASSERT
# include <stddef.h>
# undef _Static_assert
#endif

 

 
#define _GL_CONCAT(x, y) _GL_CONCAT0 (x, y)
#define _GL_CONCAT0(x, y) x##y

 
#if defined __COUNTER__ && __COUNTER__ != __COUNTER__
# define _GL_COUNTER __COUNTER__
#else
# define _GL_COUNTER __LINE__
#endif

 
#define _GL_GENSYM(prefix) _GL_CONCAT (prefix, _GL_COUNTER)

 

#define _GL_VERIFY_TRUE(R, DIAGNOSTIC) \
   (!!sizeof (_GL_VERIFY_TYPE (R, DIAGNOSTIC)))

#ifdef __cplusplus
# if !GNULIB_defined_struct__gl_verify_type
template <int w>
  struct _gl_verify_type {
    unsigned int _gl_verify_error_if_negative: w;
  };
#  define GNULIB_defined_struct__gl_verify_type 1
# endif
# define _GL_VERIFY_TYPE(R, DIAGNOSTIC) \
    _gl_verify_type<(R) ? 1 : -1>
#elif defined _GL_HAVE__STATIC_ASSERT
# define _GL_VERIFY_TYPE(R, DIAGNOSTIC) \
    struct {                                   \
      _Static_assert (R, DIAGNOSTIC);          \
      int _gl_dummy;                          \
    }
#else
# define _GL_VERIFY_TYPE(R, DIAGNOSTIC) \
    struct { unsigned int _gl_verify_error_if_negative: (R) ? 1 : -1; }
#endif

 

#if 202311 <= __STDC_VERSION__ || 200410 <= __cpp_static_assert
# define _GL_VERIFY(R, DIAGNOSTIC, ...) static_assert (R, DIAGNOSTIC)
#elif defined _GL_HAVE__STATIC_ASSERT
# define _GL_VERIFY(R, DIAGNOSTIC, ...) _Static_assert (R, DIAGNOSTIC)
#else
# define _GL_VERIFY(R, DIAGNOSTIC, ...)                                \
    extern int (*_GL_GENSYM (_gl_verify_function) (void))	       \
      [_GL_VERIFY_TRUE (R, DIAGNOSTIC)]
# if 4 < __GNUC__ + (6 <= __GNUC_MINOR__)
#  pragma GCC diagnostic ignored "-Wnested-externs"
# endif
#endif

 
#ifdef _GL_STATIC_ASSERT_H
 
 
# if (defined __cplusplus && defined __clang__ \
      && (4 <= __clang_major__ + (8 <= __clang_minor__)))
#  if 5 <= __clang_major__
 
#   pragma clang diagnostic ignored "-Wc++17-extensions"
#  else
 
#   pragma clang diagnostic ignored "-Wc++1z-extensions"
#  endif
# elif !defined _GL_HAVE__STATIC_ASSERT1 && !defined _Static_assert
#  if !defined _MSC_VER || defined __clang__
#   define _Static_assert(...) \
      _GL_VERIFY (__VA_ARGS__, "static assertion failed", -)
#  else
#   if defined __cplusplus && _MSC_VER >= 1910
      
#    define _Static_assert static_assert
#   else
      
# if (!defined static_assert \
      && __STDC_VERSION__ < 202311 \
      && (!defined __cplusplus \
          || (__cpp_static_assert < 201411 \
              && __GNUG__ < 6 && __clang_major__ < 6 && _MSC_VER < 1910)))
#  if defined __cplusplus && _MSC_VER >= 1900 && !defined __clang__
 
#   define _GL_EXPAND(x) x
#   define _GL_SA1(a1) static_assert ((a1), "static assertion failed")
#   define _GL_SA2 static_assert
#   define _GL_SA3 static_assert
#   define _GL_SA_PICK(x1,x2,x3,x4,...) x4
#   define static_assert(...) _GL_EXPAND(_GL_SA_PICK(__VA_ARGS__,_GL_SA3,_GL_SA2,_GL_SA1)) (__VA_ARGS__)
 
#   define _ALLOW_KEYWORD_MACROS 1
#  else
#   define static_assert _Static_assert  
#  endif
# endif
#endif

 

#if defined __clang_major__ && __clang_major__ < 5
# define _GL_HAS_BUILTIN_TRAP 0
#elif 3 < __GNUC__ + (3 < __GNUC_MINOR__ + (4 <= __GNUC_PATCHLEVEL__))
# define _GL_HAS_BUILTIN_TRAP 1
#elif defined __has_builtin
# define _GL_HAS_BUILTIN_TRAP __has_builtin (__builtin_trap)
#else
# define _GL_HAS_BUILTIN_TRAP 0
#endif

#ifndef _GL_HAS_BUILTIN_UNREACHABLE
# if defined __clang_major__ && __clang_major__ < 5
#  define _GL_HAS_BUILTIN_UNREACHABLE 0
# elif 4 < __GNUC__ + (5 <= __GNUC_MINOR__)
#  define _GL_HAS_BUILTIN_UNREACHABLE 1
# elif defined __has_builtin
#  define _GL_HAS_BUILTIN_UNREACHABLE __has_builtin (__builtin_unreachable)
# else
#  define _GL_HAS_BUILTIN_UNREACHABLE 0
# endif
#endif

 

 

#define verify_expr(R, E) \
   (_GL_VERIFY_TRUE (R, "verify_expr (" #R ", " #E ")") ? (E) : (E))

 

#ifdef __PGI
 
# define verify(R) _GL_VERIFY (R, "verify (...)", -)
#else
# define verify(R) _GL_VERIFY (R, "verify (" #R ")", -)
#endif

 

#if _GL_HAS_BUILTIN_UNREACHABLE
# define assume(R) ((R) ? (void) 0 : __builtin_unreachable ())
#elif 1200 <= _MSC_VER
# define assume(R) __assume (R)
#elif 202311 <= __STDC_VERSION__
# include <stddef.h>
# define assume(R) ((R) ? (void) 0 : unreachable ())
#elif (defined GCC_LINT || defined lint) && _GL_HAS_BUILTIN_TRAP
   
# define assume(R) ((R) ? (void) 0 : __builtin_trap ())
#else
   
# define assume(R) ((R) ? (void) 0 :   (void) 0)
#endif

 

#endif
