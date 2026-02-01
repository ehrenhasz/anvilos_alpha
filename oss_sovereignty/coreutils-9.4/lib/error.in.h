 

 
#if @HAVE_ERROR_H@
# @INCLUDE_NEXT@ @NEXT_ERROR_H@
#endif

#ifndef _@GUARD_PREFIX@_ERROR_H
#define _@GUARD_PREFIX@_ERROR_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#include <stddef.h>

 
#include <stdio.h>

 

#if GNULIB_VFPRINTF_POSIX
# define _GL_ATTRIBUTE_SPEC_PRINTF_ERROR _GL_ATTRIBUTE_SPEC_PRINTF_STANDARD
#else
# define _GL_ATTRIBUTE_SPEC_PRINTF_ERROR _GL_ATTRIBUTE_SPEC_PRINTF_SYSTEM
#endif

 
#ifdef __GNUC__
 
# define __gl_error_call1(function, status, ...) \
    ((function) (status, __VA_ARGS__), \
     (status) != 0 ? unreachable () : (void) 0)
 
# define __gl_error_call(function, status, ...) \
    (__builtin_constant_p (status) \
     ? __gl_error_call1 (function, status, __VA_ARGS__) \
     : ({ \
         int const __errstatus = status; \
         __gl_error_call1 (function, __errstatus, __VA_ARGS__); \
       }))
#else
# define __gl_error_call(function, status, ...) \
    (function) (status, __VA_ARGS__)
#endif

#ifdef __cplusplus
extern "C" {
#endif

 
#if @REPLACE_ERROR@
# if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#  undef error
#  define error rpl_error
# endif
_GL_FUNCDECL_RPL (error, void,
                  (int __status, int __errnum, const char *__format, ...)
                  _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_ERROR, 3, 4)));
_GL_CXXALIAS_RPL (error, void,
                  (int __status, int __errnum, const char *__format, ...));
# ifndef _GL_NO_INLINE_ERROR
#  undef error
#  define error(status, ...) \
     __gl_error_call (rpl_error, status, __VA_ARGS__)
# endif
#else
# if ! @HAVE_ERROR@
_GL_FUNCDECL_SYS (error, void,
                  (int __status, int __errnum, const char *__format, ...)
                  _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_ERROR, 3, 4)));
# endif
_GL_CXXALIAS_SYS (error, void,
                  (int __status, int __errnum, const char *__format, ...));
# ifndef _GL_NO_INLINE_ERROR
#  ifdef error
 
#   if _GL_GNUC_PREREQ (4, 7)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wattributes"
_GL_ATTRIBUTE_MAYBE_UNUSED
static void
_GL_ATTRIBUTE_ALWAYS_INLINE
_GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_ERROR, 3, 4))
_gl_inline_error (int __status, int __errnum, const char *__format, ...)
{
  return error (__status, __errnum, __format, __builtin_va_arg_pack ());
}
#    pragma GCC diagnostic pop
#    undef error
#    define error(status, ...) \
       __gl_error_call (_gl_inline_error, status, __VA_ARGS__)
#   endif
#  else
#   define error(status, ...) \
      __gl_error_call (error, status, __VA_ARGS__)
#  endif
# endif
#endif
#if __GLIBC__ >= 2
_GL_CXXALIASWARN (error);
#endif

 
#if @REPLACE_ERROR_AT_LINE@
# if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#  undef error_at_line
#  define error_at_line rpl_error_at_line
# endif
_GL_FUNCDECL_RPL (error_at_line, void,
                  (int __status, int __errnum, const char *__filename,
                   unsigned int __lineno, const char *__format, ...)
                  _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_ERROR, 5, 6)));
_GL_CXXALIAS_RPL (error_at_line, void,
                  (int __status, int __errnum, const char *__filename,
                   unsigned int __lineno, const char *__format, ...));
# ifndef _GL_NO_INLINE_ERROR
#  undef error_at_line
#  define error_at_line(status, ...) \
     __gl_error_call (rpl_error_at_line, status, __VA_ARGS__)
# endif
#else
# if ! @HAVE_ERROR_AT_LINE@
_GL_FUNCDECL_SYS (error_at_line, void,
                  (int __status, int __errnum, const char *__filename,
                   unsigned int __lineno, const char *__format, ...)
                  _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_ERROR, 5, 6)));
# endif
_GL_CXXALIAS_SYS (error_at_line, void,
                  (int __status, int __errnum, const char *__filename,
                   unsigned int __lineno, const char *__format, ...));
# ifndef _GL_NO_INLINE_ERROR
#  ifdef error_at_line
 
#   if _GL_GNUC_PREREQ (4, 7)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wattributes"
_GL_ATTRIBUTE_MAYBE_UNUSED
static void
_GL_ATTRIBUTE_ALWAYS_INLINE
_GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_ERROR, 5, 6))
_gl_inline_error_at_line (int __status, int __errnum, const char *__filename,
                          unsigned int __lineno, const char *__format, ...)
{
  return error_at_line (__status, __errnum, __filename, __lineno, __format,
                        __builtin_va_arg_pack ());
}
#    pragma GCC diagnostic pop
#    undef error_at_line
#    define error_at_line(status, ...) \
       __gl_error_call (_gl_inline_error_at_line, status, __VA_ARGS__)
#   endif
#  else
#   define error_at_line(status, ...) \
      __gl_error_call (error_at_line, status, __VA_ARGS__)
#  endif
# endif
#endif
_GL_CXXALIASWARN (error_at_line);

 
extern void (*error_print_progname) (void);

 
extern unsigned int error_message_count;

 
extern int error_one_per_line;

#ifdef __cplusplus
}
#endif

#endif  
#endif  
