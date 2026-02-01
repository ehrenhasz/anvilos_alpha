 

#@INCLUDE_NEXT@ @NEXT_STDIO_H@

#else
 

#ifndef _@GUARD_PREFIX@_STDIO_H

 
#if (defined __APPLE__ && defined __MACH__) && !defined _POSIX_C_SOURCE
# define _POSIX_C_SOURCE 200809L
# define _GL_DEFINED__POSIX_C_SOURCE
#endif

#define _GL_ALREADY_INCLUDING_STDIO_H

 
#@INCLUDE_NEXT@ @NEXT_STDIO_H@

#undef _GL_ALREADY_INCLUDING_STDIO_H

#ifdef _GL_DEFINED__POSIX_C_SOURCE
# undef _GL_DEFINED__POSIX_C_SOURCE
# undef _POSIX_C_SOURCE
#endif

#ifndef _@GUARD_PREFIX@_STDIO_H
#define _@GUARD_PREFIX@_STDIO_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#include <stdarg.h>

#include <stddef.h>

 
#include <sys/types.h>

 
 
#if (@GNULIB_RENAMEAT@ || defined GNULIB_POSIXCHECK) && (defined __sun || defined __NetBSD__) \
    && ! defined __GLIBC__
# include <unistd.h>
#endif

 
 
#if (@GNULIB_RENAMEAT@ || defined GNULIB_POSIXCHECK) && defined __ANDROID__ \
    && ! defined __GLIBC__
# include <sys/stat.h>
#endif

 
 
#if (@GNULIB_PERROR@ || defined GNULIB_POSIXCHECK) \
    && (defined _WIN32 && ! defined __CYGWIN__) \
    && ! defined __GLIBC__
# include <stdlib.h>
#endif

 
 
 
#if (@GNULIB_REMOVE@ || @GNULIB_RENAME@ || defined GNULIB_POSIXCHECK) \
    && (defined _WIN32 && ! defined __CYGWIN__) \
    && ! defined __GLIBC__
# include <io.h>
#endif


 
#ifndef _GL_ATTRIBUTE_DEALLOC
# if __GNUC__ >= 11
#  define _GL_ATTRIBUTE_DEALLOC(f, i) __attribute__ ((__malloc__ (f, i)))
# else
#  define _GL_ATTRIBUTE_DEALLOC(f, i)
# endif
#endif

 
#ifndef _GL_ATTRIBUTE_FORMAT
# if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7) || defined __clang__
#  define _GL_ATTRIBUTE_FORMAT(spec) __attribute__ ((__format__ spec))
# else
#  define _GL_ATTRIBUTE_FORMAT(spec)  
# endif
#endif

 
#ifndef _GL_ATTRIBUTE_MALLOC
# if __GNUC__ >= 3 || defined __clang__
#  define _GL_ATTRIBUTE_MALLOC __attribute__ ((__malloc__))
# else
#  define _GL_ATTRIBUTE_MALLOC
# endif
#endif

 
 
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)
# define _GL_ATTRIBUTE_SPEC_PRINTF_STANDARD __gnu_printf__
#else
# define _GL_ATTRIBUTE_SPEC_PRINTF_STANDARD __printf__
#endif

 
 
#if GNULIB_PRINTF_ATTRIBUTE_FLAVOR_GNU
# define _GL_ATTRIBUTE_SPEC_PRINTF_SYSTEM _GL_ATTRIBUTE_SPEC_PRINTF_STANDARD
#else
# define _GL_ATTRIBUTE_SPEC_PRINTF_SYSTEM __printf__
#endif

 
#define _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(formatstring_parameter, first_argument) \
  _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_STANDARD, formatstring_parameter, first_argument))

 
#define _GL_ATTRIBUTE_FORMAT_PRINTF_SYSTEM(formatstring_parameter, first_argument) \
  _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_SYSTEM, formatstring_parameter, first_argument))

 
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)
# define _GL_ATTRIBUTE_FORMAT_SCANF(formatstring_parameter, first_argument) \
   _GL_ATTRIBUTE_FORMAT ((__gnu_scanf__, formatstring_parameter, first_argument))
#else
# define _GL_ATTRIBUTE_FORMAT_SCANF(formatstring_parameter, first_argument) \
   _GL_ATTRIBUTE_FORMAT ((__scanf__, formatstring_parameter, first_argument))
#endif

 
#define _GL_ATTRIBUTE_FORMAT_SCANF_SYSTEM(formatstring_parameter, first_argument) \
  _GL_ATTRIBUTE_FORMAT ((__scanf__, formatstring_parameter, first_argument))

 

 

 

 
#define _GL_STDIO_STRINGIZE(token) #token
#define _GL_STDIO_MACROEXPAND_AND_STRINGIZE(token) _GL_STDIO_STRINGIZE(token)

 
#if (defined _GL_EXTERN_INLINE_IN_USE && defined __APPLE__ \
     && defined __GNUC__ && defined __STDC__)
# undef putc_unlocked
#endif


 
#ifndef _PRINTF_NAN_LEN_MAX
# if defined __FreeBSD__ || defined __DragonFly__ \
     || defined __NetBSD__ \
     || (defined __APPLE__ && defined __MACH__)
 
#  define _PRINTF_NAN_LEN_MAX 3
# elif (__GLIBC__ >= 2) || MUSL_LIBC || defined __OpenBSD__ || defined __sun || defined __CYGWIN__
 
#  define _PRINTF_NAN_LEN_MAX 4
# elif defined _AIX
 
#  define _PRINTF_NAN_LEN_MAX 5
# elif defined _WIN32 && !defined __CYGWIN__
 
#  define _PRINTF_NAN_LEN_MAX 10
# elif defined __sgi
 
#  define _PRINTF_NAN_LEN_MAX 14
# else
 
#  define _PRINTF_NAN_LEN_MAX 32
# endif
#endif


#if @GNULIB_DPRINTF@
# if @REPLACE_DPRINTF@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define dprintf rpl_dprintf
#  endif
_GL_FUNCDECL_RPL (dprintf, int, (int fd, const char *restrict format, ...)
                                _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (2, 3)
                                _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (dprintf, int, (int fd, const char *restrict format, ...));
# else
#  if !@HAVE_DPRINTF@
_GL_FUNCDECL_SYS (dprintf, int, (int fd, const char *restrict format, ...)
                                _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (2, 3)
                                _GL_ARG_NONNULL ((2)));
#  endif
_GL_CXXALIAS_SYS (dprintf, int, (int fd, const char *restrict format, ...));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (dprintf);
# endif
#elif defined GNULIB_POSIXCHECK
# undef dprintf
# if HAVE_RAW_DECL_DPRINTF
_GL_WARN_ON_USE (dprintf, "dprintf is unportable - "
                 "use gnulib module dprintf for portability");
# endif
#endif

#if @GNULIB_FCLOSE@
 
# if @REPLACE_FCLOSE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define fclose rpl_fclose
#  endif
_GL_FUNCDECL_RPL (fclose, int, (FILE *stream) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (fclose, int, (FILE *stream));
# else
_GL_CXXALIAS_SYS (fclose, int, (FILE *stream));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (fclose);
# endif
#elif defined GNULIB_POSIXCHECK
# undef fclose
 
_GL_WARN_ON_USE (fclose, "fclose is not always POSIX compliant - "
                 "use gnulib module fclose for portable POSIX compliance");
#endif

#if @GNULIB_MDA_FCLOSEALL@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fcloseall
#   define fcloseall _fcloseall
#  endif
_GL_CXXALIAS_MDA (fcloseall, int, (void));
# else
#  if @HAVE_DECL_FCLOSEALL@
#   if defined __FreeBSD__ || defined __DragonFly__
_GL_CXXALIAS_SYS (fcloseall, void, (void));
#   else
_GL_CXXALIAS_SYS (fcloseall, int, (void));
#   endif
#  endif
# endif
# if (defined _WIN32 && !defined __CYGWIN__) || @HAVE_DECL_FCLOSEALL@
_GL_CXXALIASWARN (fcloseall);
# endif
#endif

#if @GNULIB_FDOPEN@
# if @REPLACE_FDOPEN@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fdopen
#   define fdopen rpl_fdopen
#  endif
_GL_FUNCDECL_RPL (fdopen, FILE *,
                  (int fd, const char *mode)
                  _GL_ARG_NONNULL ((2)) _GL_ATTRIBUTE_DEALLOC (fclose, 1)
                  _GL_ATTRIBUTE_MALLOC);
_GL_CXXALIAS_RPL (fdopen, FILE *, (int fd, const char *mode));
# elif defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fdopen
#   define fdopen _fdopen
#  endif
_GL_CXXALIAS_MDA (fdopen, FILE *, (int fd, const char *mode));
# else
#  if __GNUC__ >= 11
 
_GL_FUNCDECL_SYS (fdopen, FILE *,
                  (int fd, const char *mode)
                  _GL_ARG_NONNULL ((2)) _GL_ATTRIBUTE_DEALLOC (fclose, 1)
                  _GL_ATTRIBUTE_MALLOC);
#  endif
_GL_CXXALIAS_SYS (fdopen, FILE *, (int fd, const char *mode));
# endif
_GL_CXXALIASWARN (fdopen);
#else
# if @GNULIB_FCLOSE@ && __GNUC__ >= 11 && !defined fdopen
 
_GL_FUNCDECL_SYS (fdopen, FILE *,
                  (int fd, const char *mode)
                  _GL_ARG_NONNULL ((2)) _GL_ATTRIBUTE_DEALLOC (fclose, 1)
                  _GL_ATTRIBUTE_MALLOC);
# endif
# if defined GNULIB_POSIXCHECK
#  undef fdopen
 
_GL_WARN_ON_USE (fdopen, "fdopen on native Windows platforms is not POSIX compliant - "
                 "use gnulib module fdopen for portability");
# elif @GNULIB_MDA_FDOPEN@
 
#  if defined _WIN32 && !defined __CYGWIN__
#   if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#    undef fdopen
#    define fdopen _fdopen
#   endif
_GL_CXXALIAS_MDA (fdopen, FILE *, (int fd, const char *mode));
#  else
_GL_CXXALIAS_SYS (fdopen, FILE *, (int fd, const char *mode));
#  endif
_GL_CXXALIASWARN (fdopen);
# endif
#endif

#if @GNULIB_FFLUSH@
 
# if @REPLACE_FFLUSH@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define fflush rpl_fflush
#  endif
_GL_FUNCDECL_RPL (fflush, int, (FILE *gl_stream));
_GL_CXXALIAS_RPL (fflush, int, (FILE *gl_stream));
# else
_GL_CXXALIAS_SYS (fflush, int, (FILE *gl_stream));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (fflush);
# endif
#elif defined GNULIB_POSIXCHECK
# undef fflush
 
_GL_WARN_ON_USE (fflush, "fflush is not always POSIX compliant - "
                 "use gnulib module fflush for portable POSIX compliance");
#endif

#if @GNULIB_FGETC@
# if @REPLACE_STDIO_READ_FUNCS@ && @GNULIB_STDIO_H_NONBLOCKING@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fgetc
#   define fgetc rpl_fgetc
#  endif
_GL_FUNCDECL_RPL (fgetc, int, (FILE *stream) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (fgetc, int, (FILE *stream));
# else
_GL_CXXALIAS_SYS (fgetc, int, (FILE *stream));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (fgetc);
# endif
#endif

#if @GNULIB_FGETS@
# if @REPLACE_STDIO_READ_FUNCS@ && @GNULIB_STDIO_H_NONBLOCKING@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fgets
#   define fgets rpl_fgets
#  endif
_GL_FUNCDECL_RPL (fgets, char *,
                  (char *restrict s, int n, FILE *restrict stream)
                  _GL_ARG_NONNULL ((1, 3)));
_GL_CXXALIAS_RPL (fgets, char *,
                  (char *restrict s, int n, FILE *restrict stream));
# else
_GL_CXXALIAS_SYS (fgets, char *,
                  (char *restrict s, int n, FILE *restrict stream));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (fgets);
# endif
#endif

#if @GNULIB_MDA_FILENO@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fileno
#   define fileno _fileno
#  endif
_GL_CXXALIAS_MDA (fileno, int, (FILE *restrict stream));
# else
_GL_CXXALIAS_SYS (fileno, int, (FILE *restrict stream));
# endif
_GL_CXXALIASWARN (fileno);
#endif

#if @GNULIB_FOPEN@
# if (@GNULIB_FOPEN@ && @REPLACE_FOPEN@) \
     || (@GNULIB_FOPEN_GNU@ && @REPLACE_FOPEN_FOR_FOPEN_GNU@)
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fopen
#   define fopen rpl_fopen
#  endif
_GL_FUNCDECL_RPL (fopen, FILE *,
                  (const char *restrict filename, const char *restrict mode)
                  _GL_ARG_NONNULL ((1, 2)) _GL_ATTRIBUTE_DEALLOC (fclose, 1)
                  _GL_ATTRIBUTE_MALLOC);
_GL_CXXALIAS_RPL (fopen, FILE *,
                  (const char *restrict filename, const char *restrict mode));
# else
#  if __GNUC__ >= 11
 
_GL_FUNCDECL_SYS (fopen, FILE *,
                  (const char *restrict filename, const char *restrict mode)
                  _GL_ARG_NONNULL ((1, 2)) _GL_ATTRIBUTE_DEALLOC (fclose, 1));
#  endif
_GL_CXXALIAS_SYS (fopen, FILE *,
                  (const char *restrict filename, const char *restrict mode));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (fopen);
# endif
#else
# if @GNULIB_FCLOSE@ && __GNUC__ >= 11 && !defined fopen
 
_GL_FUNCDECL_SYS (fopen, FILE *,
                  (const char *restrict filename, const char *restrict mode)
                  _GL_ARG_NONNULL ((1, 2)) _GL_ATTRIBUTE_DEALLOC (fclose, 1));
# endif
# if defined GNULIB_POSIXCHECK
#  undef fopen
 
_GL_WARN_ON_USE (fopen, "fopen on native Windows platforms is not POSIX compliant - "
                 "use gnulib module fopen for portability");
# endif
#endif

#if @GNULIB_FPRINTF_POSIX@ || @GNULIB_FPRINTF@
# if (@GNULIB_FPRINTF_POSIX@ && @REPLACE_FPRINTF@) \
     || (@GNULIB_FPRINTF@ && @REPLACE_STDIO_WRITE_FUNCS@ && (@GNULIB_STDIO_H_NONBLOCKING@ || @GNULIB_STDIO_H_SIGPIPE@))
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define fprintf rpl_fprintf
#  endif
#  define GNULIB_overrides_fprintf 1
#  if @GNULIB_FPRINTF_POSIX@ || @GNULIB_VFPRINTF_POSIX@
_GL_FUNCDECL_RPL (fprintf, int,
                  (FILE *restrict fp, const char *restrict format, ...)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (2, 3)
                  _GL_ARG_NONNULL ((1, 2)));
#  else
_GL_FUNCDECL_RPL (fprintf, int,
                  (FILE *restrict fp, const char *restrict format, ...)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_SYSTEM (2, 3)
                  _GL_ARG_NONNULL ((1, 2)));
#  endif
_GL_CXXALIAS_RPL (fprintf, int,
                  (FILE *restrict fp, const char *restrict format, ...));
# else
_GL_CXXALIAS_SYS (fprintf, int,
                  (FILE *restrict fp, const char *restrict format, ...));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (fprintf);
# endif
#endif
#if !@GNULIB_FPRINTF_POSIX@ && defined GNULIB_POSIXCHECK
# if !GNULIB_overrides_fprintf
#  undef fprintf
# endif
 
_GL_WARN_ON_USE (fprintf, "fprintf is not always POSIX compliant - "
                 "use gnulib module fprintf-posix for portable "
                 "POSIX compliance");
#endif

#if @GNULIB_FPURGE@
 
# if @REPLACE_FPURGE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define fpurge rpl_fpurge
#  endif
_GL_FUNCDECL_RPL (fpurge, int, (FILE *gl_stream) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (fpurge, int, (FILE *gl_stream));
# else
#  if !@HAVE_DECL_FPURGE@
_GL_FUNCDECL_SYS (fpurge, int, (FILE *gl_stream) _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (fpurge, int, (FILE *gl_stream));
# endif
_GL_CXXALIASWARN (fpurge);
#elif defined GNULIB_POSIXCHECK
# undef fpurge
# if HAVE_RAW_DECL_FPURGE
_GL_WARN_ON_USE (fpurge, "fpurge is not always present - "
                 "use gnulib module fpurge for portability");
# endif
#endif

#if @GNULIB_FPUTC@
# if @REPLACE_STDIO_WRITE_FUNCS@ && (@GNULIB_STDIO_H_NONBLOCKING@ || @GNULIB_STDIO_H_SIGPIPE@)
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fputc
#   define fputc rpl_fputc
#  endif
_GL_FUNCDECL_RPL (fputc, int, (int c, FILE *stream) _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (fputc, int, (int c, FILE *stream));
# else
_GL_CXXALIAS_SYS (fputc, int, (int c, FILE *stream));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (fputc);
# endif
#endif

#if @GNULIB_FPUTS@
# if @REPLACE_STDIO_WRITE_FUNCS@ && (@GNULIB_STDIO_H_NONBLOCKING@ || @GNULIB_STDIO_H_SIGPIPE@)
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fputs
#   define fputs rpl_fputs
#  endif
_GL_FUNCDECL_RPL (fputs, int,
                  (const char *restrict string, FILE *restrict stream)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (fputs, int,
                  (const char *restrict string, FILE *restrict stream));
# else
_GL_CXXALIAS_SYS (fputs, int,
                  (const char *restrict string, FILE *restrict stream));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (fputs);
# endif
#endif

#if @GNULIB_FREAD@
# if @REPLACE_STDIO_READ_FUNCS@ && @GNULIB_STDIO_H_NONBLOCKING@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fread
#   define fread rpl_fread
#  endif
_GL_FUNCDECL_RPL (fread, size_t,
                  (void *restrict ptr, size_t s, size_t n,
                   FILE *restrict stream)
                  _GL_ARG_NONNULL ((4)));
_GL_CXXALIAS_RPL (fread, size_t,
                  (void *restrict ptr, size_t s, size_t n,
                   FILE *restrict stream));
# else
_GL_CXXALIAS_SYS (fread, size_t,
                  (void *restrict ptr, size_t s, size_t n,
                   FILE *restrict stream));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (fread);
# endif
#endif

#if @GNULIB_FREOPEN@
# if @REPLACE_FREOPEN@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef freopen
#   define freopen rpl_freopen
#  endif
_GL_FUNCDECL_RPL (freopen, FILE *,
                  (const char *restrict filename, const char *restrict mode,
                   FILE *restrict stream)
                  _GL_ARG_NONNULL ((2, 3)));
_GL_CXXALIAS_RPL (freopen, FILE *,
                  (const char *restrict filename, const char *restrict mode,
                   FILE *restrict stream));
# else
_GL_CXXALIAS_SYS (freopen, FILE *,
                  (const char *restrict filename, const char *restrict mode,
                   FILE *restrict stream));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (freopen);
# endif
#elif defined GNULIB_POSIXCHECK
# undef freopen
 
_GL_WARN_ON_USE (freopen,
                 "freopen on native Windows platforms is not POSIX compliant - "
                 "use gnulib module freopen for portability");
#endif

#if @GNULIB_FSCANF@
# if @REPLACE_STDIO_READ_FUNCS@ && @GNULIB_STDIO_H_NONBLOCKING@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fscanf
#   define fscanf rpl_fscanf
#  endif
_GL_FUNCDECL_RPL (fscanf, int,
                  (FILE *restrict stream, const char *restrict format, ...)
                  _GL_ATTRIBUTE_FORMAT_SCANF_SYSTEM (2, 3)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (fscanf, int,
                  (FILE *restrict stream, const char *restrict format, ...));
# else
_GL_CXXALIAS_SYS (fscanf, int,
                  (FILE *restrict stream, const char *restrict format, ...));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (fscanf);
# endif
#endif


 

#if @GNULIB_FSEEK@
# if defined GNULIB_POSIXCHECK && !defined _GL_NO_LARGE_FILES
#  define _GL_FSEEK_WARN  
#  undef fseek
# endif
# if @REPLACE_FSEEK@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fseek
#   define fseek rpl_fseek
#  endif
_GL_FUNCDECL_RPL (fseek, int, (FILE *fp, long offset, int whence)
                              _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (fseek, int, (FILE *fp, long offset, int whence));
# else
_GL_CXXALIAS_SYS (fseek, int, (FILE *fp, long offset, int whence));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (fseek);
# endif
#endif

#if @GNULIB_FSEEKO@
# if !@GNULIB_FSEEK@ && !defined _GL_NO_LARGE_FILES
#  define _GL_FSEEK_WARN  
#  undef fseek
# endif
# if @REPLACE_FSEEKO@
 
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fseeko
#   define fseeko rpl_fseeko
#  endif
_GL_FUNCDECL_RPL (fseeko, int, (FILE *fp, off_t offset, int whence)
                               _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (fseeko, int, (FILE *fp, off_t offset, int whence));
# else
#  if ! @HAVE_DECL_FSEEKO@
_GL_FUNCDECL_SYS (fseeko, int, (FILE *fp, off_t offset, int whence)
                               _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (fseeko, int, (FILE *fp, off_t offset, int whence));
# endif
_GL_CXXALIASWARN (fseeko);
#elif defined GNULIB_POSIXCHECK
# define _GL_FSEEK_WARN  
# undef fseek
# undef fseeko
# if HAVE_RAW_DECL_FSEEKO
_GL_WARN_ON_USE (fseeko, "fseeko is unportable - "
                 "use gnulib module fseeko for portability");
# endif
#endif

#ifdef _GL_FSEEK_WARN
# undef _GL_FSEEK_WARN
 
_GL_WARN_ON_USE (fseek, "fseek cannot handle files larger than 4 GB "
                 "on 32-bit platforms - "
                 "use fseeko function for handling of large files");
#endif


 

#if @GNULIB_FTELL@
# if defined GNULIB_POSIXCHECK && !defined _GL_NO_LARGE_FILES
#  define _GL_FTELL_WARN  
#  undef ftell
# endif
# if @REPLACE_FTELL@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef ftell
#   define ftell rpl_ftell
#  endif
_GL_FUNCDECL_RPL (ftell, long, (FILE *fp) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (ftell, long, (FILE *fp));
# else
_GL_CXXALIAS_SYS (ftell, long, (FILE *fp));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (ftell);
# endif
#endif

#if @GNULIB_FTELLO@
# if !@GNULIB_FTELL@ && !defined _GL_NO_LARGE_FILES
#  define _GL_FTELL_WARN  
#  undef ftell
# endif
# if @REPLACE_FTELLO@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef ftello
#   define ftello rpl_ftello
#  endif
_GL_FUNCDECL_RPL (ftello, off_t, (FILE *fp) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (ftello, off_t, (FILE *fp));
# else
#  if ! @HAVE_DECL_FTELLO@
_GL_FUNCDECL_SYS (ftello, off_t, (FILE *fp) _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (ftello, off_t, (FILE *fp));
# endif
_GL_CXXALIASWARN (ftello);
#elif defined GNULIB_POSIXCHECK
# define _GL_FTELL_WARN  
# undef ftell
# undef ftello
# if HAVE_RAW_DECL_FTELLO
_GL_WARN_ON_USE (ftello, "ftello is unportable - "
                 "use gnulib module ftello for portability");
# endif
#endif

#ifdef _GL_FTELL_WARN
# undef _GL_FTELL_WARN
 
_GL_WARN_ON_USE (ftell, "ftell cannot handle files larger than 4 GB "
                 "on 32-bit platforms - "
                 "use ftello function for handling of large files");
#endif


#if @GNULIB_FWRITE@
# if @REPLACE_STDIO_WRITE_FUNCS@ && (@GNULIB_STDIO_H_NONBLOCKING@ || @GNULIB_STDIO_H_SIGPIPE@)
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fwrite
#   define fwrite rpl_fwrite
#  endif
_GL_FUNCDECL_RPL (fwrite, size_t,
                  (const void *restrict ptr, size_t s, size_t n,
                   FILE *restrict stream)
                  _GL_ARG_NONNULL ((1, 4)));
_GL_CXXALIAS_RPL (fwrite, size_t,
                  (const void *restrict ptr, size_t s, size_t n,
                   FILE *restrict stream));
# else
_GL_CXXALIAS_SYS (fwrite, size_t,
                  (const void *restrict ptr, size_t s, size_t n,
                   FILE *restrict stream));

 
#  if (0 < __USE_FORTIFY_LEVEL                                          \
       && __GLIBC__ == 2 && 4 <= __GLIBC_MINOR__ && __GLIBC_MINOR__ <= 15 \
       && 3 < __GNUC__ + (4 <= __GNUC_MINOR__)                          \
       && !defined __cplusplus)
#   undef fwrite
#   undef fwrite_unlocked
extern size_t __REDIRECT (rpl_fwrite,
                          (const void *__restrict, size_t, size_t,
                           FILE *__restrict),
                          fwrite);
extern size_t __REDIRECT (rpl_fwrite_unlocked,
                          (const void *__restrict, size_t, size_t,
                           FILE *__restrict),
                          fwrite_unlocked);
#   define fwrite rpl_fwrite
#   define fwrite_unlocked rpl_fwrite_unlocked
#  endif
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (fwrite);
# endif
#endif

#if @GNULIB_GETC@
# if @REPLACE_STDIO_READ_FUNCS@ && @GNULIB_STDIO_H_NONBLOCKING@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef getc
#   define getc rpl_fgetc
#  endif
_GL_FUNCDECL_RPL (fgetc, int, (FILE *stream) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL_1 (getc, rpl_fgetc, int, (FILE *stream));
# else
_GL_CXXALIAS_SYS (getc, int, (FILE *stream));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (getc);
# endif
#endif

#if @GNULIB_GETCHAR@
# if @REPLACE_STDIO_READ_FUNCS@ && @GNULIB_STDIO_H_NONBLOCKING@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef getchar
#   define getchar rpl_getchar
#  endif
_GL_FUNCDECL_RPL (getchar, int, (void));
_GL_CXXALIAS_RPL (getchar, int, (void));
# else
_GL_CXXALIAS_SYS (getchar, int, (void));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (getchar);
# endif
#endif

#if @GNULIB_GETDELIM@
 
# if @REPLACE_GETDELIM@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef getdelim
#   define getdelim rpl_getdelim
#  endif
_GL_FUNCDECL_RPL (getdelim, ssize_t,
                  (char **restrict lineptr, size_t *restrict linesize,
                   int delimiter,
                   FILE *restrict stream)
                  _GL_ARG_NONNULL ((1, 2, 4)));
_GL_CXXALIAS_RPL (getdelim, ssize_t,
                  (char **restrict lineptr, size_t *restrict linesize,
                   int delimiter,
                   FILE *restrict stream));
# else
#  if !@HAVE_DECL_GETDELIM@
_GL_FUNCDECL_SYS (getdelim, ssize_t,
                  (char **restrict lineptr, size_t *restrict linesize,
                   int delimiter,
                   FILE *restrict stream)
                  _GL_ARG_NONNULL ((1, 2, 4)));
#  endif
_GL_CXXALIAS_SYS (getdelim, ssize_t,
                  (char **restrict lineptr, size_t *restrict linesize,
                   int delimiter,
                   FILE *restrict stream));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (getdelim);
# endif
#elif defined GNULIB_POSIXCHECK
# undef getdelim
# if HAVE_RAW_DECL_GETDELIM
_GL_WARN_ON_USE (getdelim, "getdelim is unportable - "
                 "use gnulib module getdelim for portability");
# endif
#endif

#if @GNULIB_GETLINE@
 
# if @REPLACE_GETLINE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef getline
#   define getline rpl_getline
#  endif
_GL_FUNCDECL_RPL (getline, ssize_t,
                  (char **restrict lineptr, size_t *restrict linesize,
                   FILE *restrict stream)
                  _GL_ARG_NONNULL ((1, 2, 3)));
_GL_CXXALIAS_RPL (getline, ssize_t,
                  (char **restrict lineptr, size_t *restrict linesize,
                   FILE *restrict stream));
# else
#  if !@HAVE_DECL_GETLINE@
_GL_FUNCDECL_SYS (getline, ssize_t,
                  (char **restrict lineptr, size_t *restrict linesize,
                   FILE *restrict stream)
                  _GL_ARG_NONNULL ((1, 2, 3)));
#  endif
_GL_CXXALIAS_SYS (getline, ssize_t,
                  (char **restrict lineptr, size_t *restrict linesize,
                   FILE *restrict stream));
# endif
# if __GLIBC__ >= 2 && @HAVE_DECL_GETLINE@
_GL_CXXALIASWARN (getline);
# endif
#elif defined GNULIB_POSIXCHECK
# undef getline
# if HAVE_RAW_DECL_GETLINE
_GL_WARN_ON_USE (getline, "getline is unportable - "
                 "use gnulib module getline for portability");
# endif
#endif

 
#undef gets
#if HAVE_RAW_DECL_GETS && !defined __cplusplus
_GL_WARN_ON_USE (gets, "gets is a security hole - use fgets instead");
#endif

#if @GNULIB_MDA_GETW@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef getw
#   define getw _getw
#  endif
_GL_CXXALIAS_MDA (getw, int, (FILE *restrict stream));
# else
#  if @HAVE_DECL_GETW@
#   if defined __APPLE__ && defined __MACH__
 
_GL_FUNCDECL_SYS (getw, int, (FILE *restrict stream));
#   endif
_GL_CXXALIAS_SYS (getw, int, (FILE *restrict stream));
#  endif
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (getw);
# endif
#endif

#if @GNULIB_OBSTACK_PRINTF@ || @GNULIB_OBSTACK_PRINTF_POSIX@
struct obstack;
 
# if @REPLACE_OBSTACK_PRINTF@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define obstack_printf rpl_obstack_printf
#  endif
_GL_FUNCDECL_RPL (obstack_printf, int,
                  (struct obstack *obs, const char *format, ...)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (2, 3)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (obstack_printf, int,
                  (struct obstack *obs, const char *format, ...));
# else
#  if !@HAVE_DECL_OBSTACK_PRINTF@
_GL_FUNCDECL_SYS (obstack_printf, int,
                  (struct obstack *obs, const char *format, ...)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (2, 3)
                  _GL_ARG_NONNULL ((1, 2)));
#  endif
_GL_CXXALIAS_SYS (obstack_printf, int,
                  (struct obstack *obs, const char *format, ...));
# endif
_GL_CXXALIASWARN (obstack_printf);
# if @REPLACE_OBSTACK_PRINTF@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define obstack_vprintf rpl_obstack_vprintf
#  endif
_GL_FUNCDECL_RPL (obstack_vprintf, int,
                  (struct obstack *obs, const char *format, va_list args)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (2, 0)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (obstack_vprintf, int,
                  (struct obstack *obs, const char *format, va_list args));
# else
#  if !@HAVE_DECL_OBSTACK_PRINTF@
_GL_FUNCDECL_SYS (obstack_vprintf, int,
                  (struct obstack *obs, const char *format, va_list args)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (2, 0)
                  _GL_ARG_NONNULL ((1, 2)));
#  endif
_GL_CXXALIAS_SYS (obstack_vprintf, int,
                  (struct obstack *obs, const char *format, va_list args));
# endif
_GL_CXXALIASWARN (obstack_vprintf);
#endif

#if @GNULIB_PCLOSE@
# if !@HAVE_PCLOSE@
_GL_FUNCDECL_SYS (pclose, int, (FILE *stream) _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (pclose, int, (FILE *stream));
_GL_CXXALIASWARN (pclose);
#elif defined GNULIB_POSIXCHECK
# undef pclose
# if HAVE_RAW_DECL_PCLOSE
_GL_WARN_ON_USE (pclose, "pclose is unportable - "
                 "use gnulib module pclose for more portability");
# endif
#endif

#if @GNULIB_PERROR@
 
# if @REPLACE_PERROR@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define perror rpl_perror
#  endif
_GL_FUNCDECL_RPL (perror, void, (const char *string));
_GL_CXXALIAS_RPL (perror, void, (const char *string));
# else
_GL_CXXALIAS_SYS (perror, void, (const char *string));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (perror);
# endif
#elif defined GNULIB_POSIXCHECK
# undef perror
 
_GL_WARN_ON_USE (perror, "perror is not always POSIX compliant - "
                 "use gnulib module perror for portability");
#endif

#if @GNULIB_POPEN@
# if @REPLACE_POPEN@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef popen
#   define popen rpl_popen
#  endif
_GL_FUNCDECL_RPL (popen, FILE *,
                  (const char *cmd, const char *mode)
                  _GL_ARG_NONNULL ((1, 2)) _GL_ATTRIBUTE_DEALLOC (pclose, 1)
                  _GL_ATTRIBUTE_MALLOC);
_GL_CXXALIAS_RPL (popen, FILE *, (const char *cmd, const char *mode));
# else
#  if !@HAVE_POPEN@ || __GNUC__ >= 11
_GL_FUNCDECL_SYS (popen, FILE *,
                  (const char *cmd, const char *mode)
                  _GL_ARG_NONNULL ((1, 2)) _GL_ATTRIBUTE_DEALLOC (pclose, 1)
                  _GL_ATTRIBUTE_MALLOC);
#  endif
_GL_CXXALIAS_SYS (popen, FILE *, (const char *cmd, const char *mode));
# endif
_GL_CXXALIASWARN (popen);
#else
# if @GNULIB_PCLOSE@ && __GNUC__ >= 11 && !defined popen
 
_GL_FUNCDECL_SYS (popen, FILE *,
                  (const char *cmd, const char *mode)
                  _GL_ARG_NONNULL ((1, 2)) _GL_ATTRIBUTE_DEALLOC (pclose, 1)
                  _GL_ATTRIBUTE_MALLOC);
# endif
# if defined GNULIB_POSIXCHECK
#  undef popen
#  if HAVE_RAW_DECL_POPEN
_GL_WARN_ON_USE (popen, "popen is buggy on some platforms - "
                 "use gnulib module popen or pipe for more portability");
#  endif
# endif
#endif

#if @GNULIB_PRINTF_POSIX@ || @GNULIB_PRINTF@
# if (@GNULIB_PRINTF_POSIX@ && @REPLACE_PRINTF@) \
     || (@GNULIB_PRINTF@ && @REPLACE_STDIO_WRITE_FUNCS@ && (@GNULIB_STDIO_H_NONBLOCKING@ || @GNULIB_STDIO_H_SIGPIPE@))
#  if defined __GNUC__ || defined __clang__
#   if !(defined __cplusplus && defined GNULIB_NAMESPACE)
 
#    define printf __printf__
#   endif
#   if @GNULIB_PRINTF_POSIX@ || @GNULIB_VFPRINTF_POSIX@
_GL_FUNCDECL_RPL_1 (__printf__, int,
                    (const char *restrict format, ...)
                    __asm__ (@ASM_SYMBOL_PREFIX@
                             _GL_STDIO_MACROEXPAND_AND_STRINGIZE(rpl_printf))
                    _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (1, 2)
                    _GL_ARG_NONNULL ((1)));
#   else
_GL_FUNCDECL_RPL_1 (__printf__, int,
                    (const char *restrict format, ...)
                    __asm__ (@ASM_SYMBOL_PREFIX@
                             _GL_STDIO_MACROEXPAND_AND_STRINGIZE(rpl_printf))
                    _GL_ATTRIBUTE_FORMAT_PRINTF_SYSTEM (1, 2)
                    _GL_ARG_NONNULL ((1)));
#   endif
_GL_CXXALIAS_RPL_1 (printf, __printf__, int, (const char *format, ...));
#  else
#   if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#    define printf rpl_printf
#   endif
_GL_FUNCDECL_RPL (printf, int,
                  (const char *restrict format, ...)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (1, 2)
                  _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (printf, int, (const char *restrict format, ...));
#  endif
#  define GNULIB_overrides_printf 1
# else
_GL_CXXALIAS_SYS (printf, int, (const char *restrict format, ...));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (printf);
# endif
#endif
#if !@GNULIB_PRINTF_POSIX@ && defined GNULIB_POSIXCHECK
# if !GNULIB_overrides_printf
#  undef printf
# endif
 
_GL_WARN_ON_USE (printf, "printf is not always POSIX compliant - "
                 "use gnulib module printf-posix for portable "
                 "POSIX compliance");
#endif

#if @GNULIB_PUTC@
# if @REPLACE_STDIO_WRITE_FUNCS@ && (@GNULIB_STDIO_H_NONBLOCKING@ || @GNULIB_STDIO_H_SIGPIPE@)
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef putc
#   define putc rpl_fputc
#  endif
_GL_FUNCDECL_RPL (fputc, int, (int c, FILE *stream) _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL_1 (putc, rpl_fputc, int, (int c, FILE *stream));
# else
_GL_CXXALIAS_SYS (putc, int, (int c, FILE *stream));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (putc);
# endif
#endif

#if @GNULIB_PUTCHAR@
# if @REPLACE_STDIO_WRITE_FUNCS@ && (@GNULIB_STDIO_H_NONBLOCKING@ || @GNULIB_STDIO_H_SIGPIPE@)
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef putchar
#   define putchar rpl_putchar
#  endif
_GL_FUNCDECL_RPL (putchar, int, (int c));
_GL_CXXALIAS_RPL (putchar, int, (int c));
# else
_GL_CXXALIAS_SYS (putchar, int, (int c));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (putchar);
# endif
#endif

#if @GNULIB_PUTS@
# if @REPLACE_STDIO_WRITE_FUNCS@ && (@GNULIB_STDIO_H_NONBLOCKING@ || @GNULIB_STDIO_H_SIGPIPE@)
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef puts
#   define puts rpl_puts
#  endif
_GL_FUNCDECL_RPL (puts, int, (const char *string) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (puts, int, (const char *string));
# else
_GL_CXXALIAS_SYS (puts, int, (const char *string));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (puts);
# endif
#endif

#if @GNULIB_MDA_PUTW@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef putw
#   define putw _putw
#  endif
_GL_CXXALIAS_MDA (putw, int, (int w, FILE *restrict stream));
# else
#  if @HAVE_DECL_PUTW@
#   if defined __APPLE__ && defined __MACH__
 
_GL_FUNCDECL_SYS (putw, int, (int w, FILE *restrict stream));
#   endif
_GL_CXXALIAS_SYS (putw, int, (int w, FILE *restrict stream));
#  endif
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (putw);
# endif
#endif

#if @GNULIB_REMOVE@
# if @REPLACE_REMOVE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef remove
#   define remove rpl_remove
#  endif
_GL_FUNCDECL_RPL (remove, int, (const char *name) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (remove, int, (const char *name));
# else
_GL_CXXALIAS_SYS (remove, int, (const char *name));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (remove);
# endif
#elif defined GNULIB_POSIXCHECK
# undef remove
 
_GL_WARN_ON_USE (remove, "remove cannot handle directories on some platforms - "
                 "use gnulib module remove for more portability");
#endif

#if @GNULIB_RENAME@
# if @REPLACE_RENAME@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef rename
#   define rename rpl_rename
#  endif
_GL_FUNCDECL_RPL (rename, int,
                  (const char *old_filename, const char *new_filename)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (rename, int,
                  (const char *old_filename, const char *new_filename));
# else
_GL_CXXALIAS_SYS (rename, int,
                  (const char *old_filename, const char *new_filename));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (rename);
# endif
#elif defined GNULIB_POSIXCHECK
# undef rename
 
_GL_WARN_ON_USE (rename, "rename is buggy on some platforms - "
                 "use gnulib module rename for more portability");
#endif

#if @GNULIB_RENAMEAT@
# if @REPLACE_RENAMEAT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef renameat
#   define renameat rpl_renameat
#  endif
_GL_FUNCDECL_RPL (renameat, int,
                  (int fd1, char const *file1, int fd2, char const *file2)
                  _GL_ARG_NONNULL ((2, 4)));
_GL_CXXALIAS_RPL (renameat, int,
                  (int fd1, char const *file1, int fd2, char const *file2));
# else
#  if !@HAVE_RENAMEAT@
_GL_FUNCDECL_SYS (renameat, int,
                  (int fd1, char const *file1, int fd2, char const *file2)
                  _GL_ARG_NONNULL ((2, 4)));
#  endif
_GL_CXXALIAS_SYS (renameat, int,
                  (int fd1, char const *file1, int fd2, char const *file2));
# endif
_GL_CXXALIASWARN (renameat);
#elif defined GNULIB_POSIXCHECK
# undef renameat
# if HAVE_RAW_DECL_RENAMEAT
_GL_WARN_ON_USE (renameat, "renameat is not portable - "
                 "use gnulib module renameat for portability");
# endif
#endif

#if @GNULIB_SCANF@
# if @REPLACE_STDIO_READ_FUNCS@ && @GNULIB_STDIO_H_NONBLOCKING@
#  if defined __GNUC__ || defined __clang__
#   if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#    undef scanf
 
#    define scanf __scanf__
#   endif
_GL_FUNCDECL_RPL_1 (__scanf__, int,
                    (const char *restrict format, ...)
                    __asm__ (@ASM_SYMBOL_PREFIX@
                             _GL_STDIO_MACROEXPAND_AND_STRINGIZE(rpl_scanf))
                    _GL_ATTRIBUTE_FORMAT_SCANF_SYSTEM (1, 2)
                    _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL_1 (scanf, __scanf__, int, (const char *restrict format, ...));
#  else
#   if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#    undef scanf
#    define scanf rpl_scanf
#   endif
_GL_FUNCDECL_RPL (scanf, int, (const char *restrict format, ...)
                              _GL_ATTRIBUTE_FORMAT_SCANF_SYSTEM (1, 2)
                              _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (scanf, int, (const char *restrict format, ...));
#  endif
# else
_GL_CXXALIAS_SYS (scanf, int, (const char *restrict format, ...));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (scanf);
# endif
#endif

#if @GNULIB_SNPRINTF@
# if @REPLACE_SNPRINTF@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define snprintf rpl_snprintf
#  endif
#  define GNULIB_overrides_snprintf 1
_GL_FUNCDECL_RPL (snprintf, int,
                  (char *restrict str, size_t size,
                   const char *restrict format, ...)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (3, 4)
                  _GL_ARG_NONNULL ((3)));
_GL_CXXALIAS_RPL (snprintf, int,
                  (char *restrict str, size_t size,
                   const char *restrict format, ...));
# else
#  if !@HAVE_DECL_SNPRINTF@
_GL_FUNCDECL_SYS (snprintf, int,
                  (char *restrict str, size_t size,
                   const char *restrict format, ...)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (3, 4)
                  _GL_ARG_NONNULL ((3)));
#  endif
_GL_CXXALIAS_SYS (snprintf, int,
                  (char *restrict str, size_t size,
                   const char *restrict format, ...));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (snprintf);
# endif
#elif defined GNULIB_POSIXCHECK
# undef snprintf
# if HAVE_RAW_DECL_SNPRINTF
_GL_WARN_ON_USE (snprintf, "snprintf is unportable - "
                 "use gnulib module snprintf for portability");
# endif
#endif

 

#if @GNULIB_SPRINTF_POSIX@
# if @REPLACE_SPRINTF@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define sprintf rpl_sprintf
#  endif
#  define GNULIB_overrides_sprintf 1
_GL_FUNCDECL_RPL (sprintf, int,
                  (char *restrict str, const char *restrict format, ...)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (2, 3)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (sprintf, int,
                  (char *restrict str, const char *restrict format, ...));
# else
_GL_CXXALIAS_SYS (sprintf, int,
                  (char *restrict str, const char *restrict format, ...));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (sprintf);
# endif
#elif defined GNULIB_POSIXCHECK
# undef sprintf
 
_GL_WARN_ON_USE (sprintf, "sprintf is not always POSIX compliant - "
                 "use gnulib module sprintf-posix for portable "
                 "POSIX compliance");
#endif

#if @GNULIB_MDA_TEMPNAM@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef tempnam
#   define tempnam _tempnam
#  endif
_GL_CXXALIAS_MDA (tempnam, char *, (const char *dir, const char *prefix));
# else
_GL_CXXALIAS_SYS (tempnam, char *, (const char *dir, const char *prefix));
# endif
_GL_CXXALIASWARN (tempnam);
#endif

#if @GNULIB_TMPFILE@
# if @REPLACE_TMPFILE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define tmpfile rpl_tmpfile
#  endif
_GL_FUNCDECL_RPL (tmpfile, FILE *, (void)
                                   _GL_ATTRIBUTE_DEALLOC (fclose, 1)
                                   _GL_ATTRIBUTE_MALLOC);
_GL_CXXALIAS_RPL (tmpfile, FILE *, (void));
# else
#  if __GNUC__ >= 11
 
_GL_FUNCDECL_SYS (tmpfile, FILE *, (void)
                                   _GL_ATTRIBUTE_DEALLOC (fclose, 1)
                                   _GL_ATTRIBUTE_MALLOC);
#  endif
_GL_CXXALIAS_SYS (tmpfile, FILE *, (void));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (tmpfile);
# endif
#else
# if @GNULIB_FCLOSE@ && __GNUC__ >= 11 && !defined tmpfile
 
_GL_FUNCDECL_SYS (tmpfile, FILE *, (void)
                                   _GL_ATTRIBUTE_DEALLOC (fclose, 1)
                                   _GL_ATTRIBUTE_MALLOC);
# endif
# if defined GNULIB_POSIXCHECK
#  undef tmpfile
#  if HAVE_RAW_DECL_TMPFILE
_GL_WARN_ON_USE (tmpfile, "tmpfile is not usable on mingw - "
                 "use gnulib module tmpfile for portability");
#  endif
# endif
#endif

#if @GNULIB_VASPRINTF@
 
# if @REPLACE_VASPRINTF@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define asprintf rpl_asprintf
#  endif
#  define GNULIB_overrides_asprintf
_GL_FUNCDECL_RPL (asprintf, int,
                  (char **result, const char *format, ...)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (2, 3)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (asprintf, int,
                  (char **result, const char *format, ...));
# else
#  if !@HAVE_VASPRINTF@
_GL_FUNCDECL_SYS (asprintf, int,
                  (char **result, const char *format, ...)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (2, 3)
                  _GL_ARG_NONNULL ((1, 2)));
#  endif
_GL_CXXALIAS_SYS (asprintf, int,
                  (char **result, const char *format, ...));
# endif
_GL_CXXALIASWARN (asprintf);
# if @REPLACE_VASPRINTF@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define vasprintf rpl_vasprintf
#  endif
#  define GNULIB_overrides_vasprintf 1
_GL_FUNCDECL_RPL (vasprintf, int,
                  (char **result, const char *format, va_list args)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (2, 0)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (vasprintf, int,
                  (char **result, const char *format, va_list args));
# else
#  if !@HAVE_VASPRINTF@
_GL_FUNCDECL_SYS (vasprintf, int,
                  (char **result, const char *format, va_list args)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (2, 0)
                  _GL_ARG_NONNULL ((1, 2)));
#  endif
_GL_CXXALIAS_SYS (vasprintf, int,
                  (char **result, const char *format, va_list args));
# endif
_GL_CXXALIASWARN (vasprintf);
#endif

#if @GNULIB_VDPRINTF@
# if @REPLACE_VDPRINTF@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define vdprintf rpl_vdprintf
#  endif
_GL_FUNCDECL_RPL (vdprintf, int,
                  (int fd, const char *restrict format, va_list args)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (2, 0)
                  _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (vdprintf, int,
                  (int fd, const char *restrict format, va_list args));
# else
#  if !@HAVE_VDPRINTF@
_GL_FUNCDECL_SYS (vdprintf, int,
                  (int fd, const char *restrict format, va_list args)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (2, 0)
                  _GL_ARG_NONNULL ((2)));
#  endif
 
_GL_CXXALIAS_SYS_CAST (vdprintf, int,
                       (int fd, const char *restrict format, va_list args));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (vdprintf);
# endif
#elif defined GNULIB_POSIXCHECK
# undef vdprintf
# if HAVE_RAW_DECL_VDPRINTF
_GL_WARN_ON_USE (vdprintf, "vdprintf is unportable - "
                 "use gnulib module vdprintf for portability");
# endif
#endif

#if @GNULIB_VFPRINTF_POSIX@ || @GNULIB_VFPRINTF@
# if (@GNULIB_VFPRINTF_POSIX@ && @REPLACE_VFPRINTF@) \
     || (@GNULIB_VFPRINTF@ && @REPLACE_STDIO_WRITE_FUNCS@ && (@GNULIB_STDIO_H_NONBLOCKING@ || @GNULIB_STDIO_H_SIGPIPE@))
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define vfprintf rpl_vfprintf
#  endif
#  define GNULIB_overrides_vfprintf 1
#  if @GNULIB_VFPRINTF_POSIX@
_GL_FUNCDECL_RPL (vfprintf, int,
                  (FILE *restrict fp,
                   const char *restrict format, va_list args)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (2, 0)
                  _GL_ARG_NONNULL ((1, 2)));
#  else
_GL_FUNCDECL_RPL (vfprintf, int,
                  (FILE *restrict fp,
                   const char *restrict format, va_list args)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_SYSTEM (2, 0)
                  _GL_ARG_NONNULL ((1, 2)));
#  endif
_GL_CXXALIAS_RPL (vfprintf, int,
                  (FILE *restrict fp,
                   const char *restrict format, va_list args));
# else
 
_GL_CXXALIAS_SYS_CAST (vfprintf, int,
                       (FILE *restrict fp,
                        const char *restrict format, va_list args));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (vfprintf);
# endif
#endif
#if !@GNULIB_VFPRINTF_POSIX@ && defined GNULIB_POSIXCHECK
# if !GNULIB_overrides_vfprintf
#  undef vfprintf
# endif
 
_GL_WARN_ON_USE (vfprintf, "vfprintf is not always POSIX compliant - "
                 "use gnulib module vfprintf-posix for portable "
                      "POSIX compliance");
#endif

#if @GNULIB_VFSCANF@
# if @REPLACE_STDIO_READ_FUNCS@ && @GNULIB_STDIO_H_NONBLOCKING@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef vfscanf
#   define vfscanf rpl_vfscanf
#  endif
_GL_FUNCDECL_RPL (vfscanf, int,
                  (FILE *restrict stream,
                   const char *restrict format, va_list args)
                  _GL_ATTRIBUTE_FORMAT_SCANF_SYSTEM (2, 0)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (vfscanf, int,
                  (FILE *restrict stream,
                   const char *restrict format, va_list args));
# else
_GL_CXXALIAS_SYS (vfscanf, int,
                  (FILE *restrict stream,
                   const char *restrict format, va_list args));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (vfscanf);
# endif
#endif

#if @GNULIB_VPRINTF_POSIX@ || @GNULIB_VPRINTF@
# if (@GNULIB_VPRINTF_POSIX@ && @REPLACE_VPRINTF@) \
     || (@GNULIB_VPRINTF@ && @REPLACE_STDIO_WRITE_FUNCS@ && (@GNULIB_STDIO_H_NONBLOCKING@ || @GNULIB_STDIO_H_SIGPIPE@))
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define vprintf rpl_vprintf
#  endif
#  define GNULIB_overrides_vprintf 1
#  if @GNULIB_VPRINTF_POSIX@ || @GNULIB_VFPRINTF_POSIX@
_GL_FUNCDECL_RPL (vprintf, int, (const char *restrict format, va_list args)
                                _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (1, 0)
                                _GL_ARG_NONNULL ((1)));
#  else
_GL_FUNCDECL_RPL (vprintf, int, (const char *restrict format, va_list args)
                                _GL_ATTRIBUTE_FORMAT_PRINTF_SYSTEM (1, 0)
                                _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_RPL (vprintf, int, (const char *restrict format, va_list args));
# else
 
_GL_CXXALIAS_SYS_CAST (vprintf, int,
                       (const char *restrict format, va_list args));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (vprintf);
# endif
#endif
#if !@GNULIB_VPRINTF_POSIX@ && defined GNULIB_POSIXCHECK
# if !GNULIB_overrides_vprintf
#  undef vprintf
# endif
 
_GL_WARN_ON_USE (vprintf, "vprintf is not always POSIX compliant - "
                 "use gnulib module vprintf-posix for portable "
                 "POSIX compliance");
#endif

#if @GNULIB_VSCANF@
# if @REPLACE_STDIO_READ_FUNCS@ && @GNULIB_STDIO_H_NONBLOCKING@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef vscanf
#   define vscanf rpl_vscanf
#  endif
_GL_FUNCDECL_RPL (vscanf, int, (const char *restrict format, va_list args)
                               _GL_ATTRIBUTE_FORMAT_SCANF_SYSTEM (1, 0)
                               _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (vscanf, int, (const char *restrict format, va_list args));
# else
_GL_CXXALIAS_SYS (vscanf, int, (const char *restrict format, va_list args));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (vscanf);
# endif
#endif

#if @GNULIB_VSNPRINTF@
# if @REPLACE_VSNPRINTF@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define vsnprintf rpl_vsnprintf
#  endif
#  define GNULIB_overrides_vsnprintf 1
_GL_FUNCDECL_RPL (vsnprintf, int,
                  (char *restrict str, size_t size,
                   const char *restrict format, va_list args)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (3, 0)
                  _GL_ARG_NONNULL ((3)));
_GL_CXXALIAS_RPL (vsnprintf, int,
                  (char *restrict str, size_t size,
                   const char *restrict format, va_list args));
# else
#  if !@HAVE_DECL_VSNPRINTF@
_GL_FUNCDECL_SYS (vsnprintf, int,
                  (char *restrict str, size_t size,
                   const char *restrict format, va_list args)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (3, 0)
                  _GL_ARG_NONNULL ((3)));
#  endif
_GL_CXXALIAS_SYS (vsnprintf, int,
                  (char *restrict str, size_t size,
                   const char *restrict format, va_list args));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (vsnprintf);
# endif
#elif defined GNULIB_POSIXCHECK
# undef vsnprintf
# if HAVE_RAW_DECL_VSNPRINTF
_GL_WARN_ON_USE (vsnprintf, "vsnprintf is unportable - "
                 "use gnulib module vsnprintf for portability");
# endif
#endif

#if @GNULIB_VSPRINTF_POSIX@
# if @REPLACE_VSPRINTF@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define vsprintf rpl_vsprintf
#  endif
#  define GNULIB_overrides_vsprintf 1
_GL_FUNCDECL_RPL (vsprintf, int,
                  (char *restrict str,
                   const char *restrict format, va_list args)
                  _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD (2, 0)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (vsprintf, int,
                  (char *restrict str,
                   const char *restrict format, va_list args));
# else
 
_GL_CXXALIAS_SYS_CAST (vsprintf, int,
                       (char *restrict str,
                        const char *restrict format, va_list args));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (vsprintf);
# endif
#elif defined GNULIB_POSIXCHECK
# undef vsprintf
 
_GL_WARN_ON_USE (vsprintf, "vsprintf is not always POSIX compliant - "
                 "use gnulib module vsprintf-posix for portable "
                      "POSIX compliance");
#endif

#endif  
#endif  
#endif
