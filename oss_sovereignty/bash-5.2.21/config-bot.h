 
 

 

 
 
 

#if !defined (HAVE_VPRINTF) && defined (HAVE_DOPRNT)
#  define USE_VFPRINTF_EMULATION
#  define HAVE_VPRINTF
#endif

#if defined (HAVE_SYS_RESOURCE_H) && defined (HAVE_GETRLIMIT)
#  define HAVE_RESOURCE
#endif

#if !defined (GETPGRP_VOID)
#  define HAVE_BSD_PGRP
#endif

 
#if defined (HAVE_STDARG_H)
#  define PREFER_STDARG
#  define USE_VARARGS
#else
#  if defined (HAVE_VARARGS_H)
#    define PREFER_VARARGS
#    define USE_VARARGS
#  endif
#endif

#if defined (HAVE_SYS_SOCKET_H) && defined (HAVE_GETPEERNAME) && defined (HAVE_NETINET_IN_H)
#  define HAVE_NETWORK
#endif

#if defined (HAVE_REGEX_H) && defined (HAVE_REGCOMP) && defined (HAVE_REGEXEC)
#  define HAVE_POSIX_REGEXP
#endif

 
#if HAVE_DECL_SYS_SIGLIST && !defined (SYS_SIGLIST_DECLARED)
#  define SYS_SIGLIST_DECLARED
#endif

 
 
 

 
#if !defined (HAVE_TERMIOS_H) || !defined (HAVE_TCGETATTR) || defined (ultrix)
#  define TERMIOS_MISSING
#endif

 
#if defined (HAVE_GETCWD) && defined (GETCWD_BROKEN) && !defined (SOLARIS)
#  undef HAVE_GETCWD
#endif

#if !defined (HAVE_DEV_FD) && defined (NAMED_PIPES_MISSING)
#  undef PROCESS_SUBSTITUTION
#endif

#if defined (JOB_CONTROL_MISSING)
#  undef JOB_CONTROL
#endif

#if defined (STRCOLL_BROKEN)
#  undef HAVE_STRCOLL
#endif

#if !defined (HAVE_POSIX_REGEXP)
#  undef COND_REGEXP
#endif

#if !HAVE_MKSTEMP
#  undef USE_MKSTEMP
#endif

#if !HAVE_MKDTEMP
#  undef USE_MKDTEMP
#endif

 
#if defined (RESTRICTED_SHELL)
#  define RESTRICTED_SHELL_NAME "rbash"
#endif

 
 
 

 
#if defined (BANG_HISTORY) && !defined (HISTORY)
#  define HISTORY
#endif  

#if defined (READLINE) && !defined (HISTORY)
#  define HISTORY
#endif

#if defined (PROGRAMMABLE_COMPLETION) && !defined (READLINE)
#  undef PROGRAMMABLE_COMPLETION
#endif

#if !defined (V9_ECHO)
#  undef DEFAULT_ECHO_TO_XPG
#endif

#if !defined (PROMPT_STRING_DECODE)
#  undef PPROMPT
#  define PPROMPT "$ "
#endif

#if !defined (HAVE_SYSLOG) || !defined (HAVE_SYSLOG_H)
#  undef SYSLOG_HISTORY
#endif

 
 
 

 
 
#if defined (HAVE_WCTYPE_H) && defined (HAVE_WCHAR_H) && defined (HAVE_LOCALE_H)
#  include <wchar.h>
#  include <wctype.h>
#  if defined (HAVE_ISWCTYPE) && \
      defined (HAVE_ISWLOWER) && \
      defined (HAVE_ISWUPPER) && \
      defined (HAVE_MBSRTOWCS) && \
      defined (HAVE_MBRTOWC) && \
      defined (HAVE_MBRLEN) && \
      defined (HAVE_TOWLOWER) && \
      defined (HAVE_TOWUPPER) && \
      defined (HAVE_WCHAR_T) && \
      defined (HAVE_WCTYPE_T) && \
      defined (HAVE_WINT_T) && \
      defined (HAVE_WCWIDTH) && \
      defined (HAVE_WCTYPE)
      
#    define HANDLE_MULTIBYTE      1
#  endif
#endif

 
#if defined (NO_MULTIBYTE_SUPPORT)
#  undef HANDLE_MULTIBYTE
#endif

 
#if HANDLE_MULTIBYTE && !defined (HAVE_MBSTATE_T)
#  define wcsrtombs(dest, src, len, ps) (wcsrtombs) (dest, src, len, 0)
#  define mbsrtowcs(dest, src, len, ps) (mbsrtowcs) (dest, src, len, 0)
#  define wcrtomb(s, wc, ps) (wcrtomb) (s, wc, 0)
#  define mbrtowc(pwc, s, n, ps) (mbrtowc) (pwc, s, n, 0)
#  define mbrlen(s, n, ps) (mbrlen) (s, n, 0)
#  define mbstate_t int
#endif

 
#ifdef HANDLE_MULTIBYTE
#  include <limits.h>
#  if defined(MB_LEN_MAX) && (MB_LEN_MAX < 16)
#    undef MB_LEN_MAX
#  endif
#  if !defined (MB_LEN_MAX)
#    define MB_LEN_MAX 16
#  endif
#endif

 
 
 

 
 
 
 
 

 
 
