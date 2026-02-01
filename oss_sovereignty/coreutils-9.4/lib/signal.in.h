 

# @INCLUDE_NEXT@ @NEXT_SIGNAL_H@

#else
 

#ifndef _@GUARD_PREFIX@_SIGNAL_H

#define _GL_ALREADY_INCLUDING_SIGNAL_H

 
#include <sys/types.h>

 
#@INCLUDE_NEXT@ @NEXT_SIGNAL_H@

#undef _GL_ALREADY_INCLUDING_SIGNAL_H

#ifndef _@GUARD_PREFIX@_SIGNAL_H
#define _@GUARD_PREFIX@_SIGNAL_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#if (@GNULIB_PTHREAD_SIGMASK@ || defined GNULIB_POSIXCHECK) \
    && defined __OpenBSD__
# include <sys/param.h>
#endif

 
#if (@GNULIB_PTHREAD_SIGMASK@ || defined GNULIB_POSIXCHECK) \
    && ((defined __APPLE__ && defined __MACH__) \
        || (defined __FreeBSD__ && __FreeBSD__ < 8) \
        || (defined __OpenBSD__ && OpenBSD < 201205) \
        || defined __osf__ || defined __sun || defined __ANDROID__ \
        || defined __KLIBC__) \
    && ! defined __GLIBC__
# include <pthread.h>
#endif

 

 

 

 
#if ! @HAVE_TYPE_VOLATILE_SIG_ATOMIC_T@
# if !GNULIB_defined_sig_atomic_t
typedef int rpl_sig_atomic_t;
#  undef sig_atomic_t
#  define sig_atomic_t rpl_sig_atomic_t
#  define GNULIB_defined_sig_atomic_t 1
# endif
#endif

 
#if !@HAVE_SIGSET_T@
# if !GNULIB_defined_sigset_t
typedef unsigned int sigset_t;
#  define GNULIB_defined_sigset_t 1
# endif
#endif

 
#if !@HAVE_SIGHANDLER_T@
# ifdef __cplusplus
extern "C" {
# endif
# if !GNULIB_defined_sighandler_t
typedef void (*sighandler_t) (int);
#  define GNULIB_defined_sighandler_t 1
# endif
# ifdef __cplusplus
}
# endif
#endif


#if @GNULIB_SIGNAL_H_SIGPIPE@
# ifndef SIGPIPE
 
#  define SIGPIPE 13
#  define GNULIB_defined_SIGPIPE 1
 
# endif
#endif


 
#ifndef NSIG
# if defined __TANDEM
#  define NSIG 32
# endif
#endif


#if @GNULIB_PTHREAD_SIGMASK@
# if @REPLACE_PTHREAD_SIGMASK@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef pthread_sigmask
#   define pthread_sigmask rpl_pthread_sigmask
#  endif
_GL_FUNCDECL_RPL (pthread_sigmask, int,
                  (int how,
                   const sigset_t *restrict new_mask,
                   sigset_t *restrict old_mask));
_GL_CXXALIAS_RPL (pthread_sigmask, int,
                  (int how,
                   const sigset_t *restrict new_mask,
                   sigset_t *restrict old_mask));
# else
#  if !(@HAVE_PTHREAD_SIGMASK@ || defined pthread_sigmask)
_GL_FUNCDECL_SYS (pthread_sigmask, int,
                  (int how,
                   const sigset_t *restrict new_mask,
                   sigset_t *restrict old_mask));
#  endif
_GL_CXXALIAS_SYS (pthread_sigmask, int,
                  (int how,
                   const sigset_t *restrict new_mask,
                   sigset_t *restrict old_mask));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (pthread_sigmask);
# endif
#elif defined GNULIB_POSIXCHECK
# undef pthread_sigmask
# if HAVE_RAW_DECL_PTHREAD_SIGMASK
_GL_WARN_ON_USE (pthread_sigmask, "pthread_sigmask is not portable - "
                 "use gnulib module pthread_sigmask for portability");
# endif
#endif


#if @GNULIB_RAISE@
# if @REPLACE_RAISE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef raise
#   define raise rpl_raise
#  endif
_GL_FUNCDECL_RPL (raise, int, (int sig));
_GL_CXXALIAS_RPL (raise, int, (int sig));
# else
#  if !@HAVE_RAISE@
_GL_FUNCDECL_SYS (raise, int, (int sig));
#  endif
_GL_CXXALIAS_SYS (raise, int, (int sig));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (raise);
# endif
#elif defined GNULIB_POSIXCHECK
# undef raise
 
_GL_WARN_ON_USE (raise, "raise can crash on native Windows - "
                 "use gnulib module raise for portability");
#endif


#if @GNULIB_SIGPROCMASK@
# if !@HAVE_POSIX_SIGNALBLOCKING@

#  ifndef GNULIB_defined_signal_blocking
#   define GNULIB_defined_signal_blocking 1
#  endif

 
#  ifndef NSIG
#   define NSIG 32
#  endif

 
#  if !GNULIB_defined_verify_NSIG_constraint
typedef int verify_NSIG_constraint[NSIG <= 32 ? 1 : -1];
#   define GNULIB_defined_verify_NSIG_constraint 1
#  endif

# endif

 
#if (defined _GL_EXTERN_INLINE_IN_USE && defined __APPLE__ \
     && (defined __i386__ || defined __x86_64__))
# undef sigaddset
# undef sigdelset
# undef sigemptyset
# undef sigfillset
# undef sigismember
#endif

 
# if @HAVE_POSIX_SIGNALBLOCKING@
 
#  if defined __cplusplus && defined GNULIB_NAMESPACE
#   undef sigismember
#  endif
# else
_GL_FUNCDECL_SYS (sigismember, int, (const sigset_t *set, int sig)
                                    _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (sigismember, int, (const sigset_t *set, int sig));
_GL_CXXALIASWARN (sigismember);

 
# if @HAVE_POSIX_SIGNALBLOCKING@
 
#  if defined __cplusplus && defined GNULIB_NAMESPACE
#   undef sigemptyset
#  endif
# else
_GL_FUNCDECL_SYS (sigemptyset, int, (sigset_t *set) _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (sigemptyset, int, (sigset_t *set));
_GL_CXXALIASWARN (sigemptyset);

 
# if @HAVE_POSIX_SIGNALBLOCKING@
 
#  if defined __cplusplus && defined GNULIB_NAMESPACE
#   undef sigaddset
#  endif
# else
_GL_FUNCDECL_SYS (sigaddset, int, (sigset_t *set, int sig)
                                  _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (sigaddset, int, (sigset_t *set, int sig));
_GL_CXXALIASWARN (sigaddset);

 
# if @HAVE_POSIX_SIGNALBLOCKING@
 
#  if defined __cplusplus && defined GNULIB_NAMESPACE
#   undef sigdelset
#  endif
# else
_GL_FUNCDECL_SYS (sigdelset, int, (sigset_t *set, int sig)
                                  _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (sigdelset, int, (sigset_t *set, int sig));
_GL_CXXALIASWARN (sigdelset);

 
# if @HAVE_POSIX_SIGNALBLOCKING@
 
#  if defined __cplusplus && defined GNULIB_NAMESPACE
#   undef sigfillset
#  endif
# else
_GL_FUNCDECL_SYS (sigfillset, int, (sigset_t *set) _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (sigfillset, int, (sigset_t *set));
_GL_CXXALIASWARN (sigfillset);

 
# if !@HAVE_POSIX_SIGNALBLOCKING@
_GL_FUNCDECL_SYS (sigpending, int, (sigset_t *set) _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (sigpending, int, (sigset_t *set));
_GL_CXXALIASWARN (sigpending);

 
# if !@HAVE_POSIX_SIGNALBLOCKING@
#  define SIG_BLOCK   0   
#  define SIG_SETMASK 1   
#  define SIG_UNBLOCK 2   
_GL_FUNCDECL_SYS (sigprocmask, int,
                  (int operation,
                   const sigset_t *restrict set,
                   sigset_t *restrict old_set));
# endif
_GL_CXXALIAS_SYS (sigprocmask, int,
                  (int operation,
                   const sigset_t *restrict set,
                   sigset_t *restrict old_set));
_GL_CXXALIASWARN (sigprocmask);

 
# ifdef __cplusplus
extern "C" {
# endif
# if !GNULIB_defined_function_taking_int_returning_void_t
typedef void (*_gl_function_taking_int_returning_void_t) (int);
#  define GNULIB_defined_function_taking_int_returning_void_t 1
# endif
# ifdef __cplusplus
}
# endif
# if !@HAVE_POSIX_SIGNALBLOCKING@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define signal rpl_signal
#  endif
_GL_FUNCDECL_RPL (signal, _gl_function_taking_int_returning_void_t,
                  (int sig, _gl_function_taking_int_returning_void_t func));
_GL_CXXALIAS_RPL (signal, _gl_function_taking_int_returning_void_t,
                  (int sig, _gl_function_taking_int_returning_void_t func));
# else
 
#  if defined __OpenBSD__
_GL_FUNCDECL_SYS (signal, _gl_function_taking_int_returning_void_t,
                  (int sig, _gl_function_taking_int_returning_void_t func));
#  endif
_GL_CXXALIAS_SYS (signal, _gl_function_taking_int_returning_void_t,
                  (int sig, _gl_function_taking_int_returning_void_t func));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (signal);
# endif

# if !@HAVE_POSIX_SIGNALBLOCKING@ && GNULIB_defined_SIGPIPE
 
_GL_EXTERN_C int _gl_raise_SIGPIPE (void);
# endif

#elif defined GNULIB_POSIXCHECK
# undef sigaddset
# if HAVE_RAW_DECL_SIGADDSET
_GL_WARN_ON_USE (sigaddset, "sigaddset is unportable - "
                 "use the gnulib module sigprocmask for portability");
# endif
# undef sigdelset
# if HAVE_RAW_DECL_SIGDELSET
_GL_WARN_ON_USE (sigdelset, "sigdelset is unportable - "
                 "use the gnulib module sigprocmask for portability");
# endif
# undef sigemptyset
# if HAVE_RAW_DECL_SIGEMPTYSET
_GL_WARN_ON_USE (sigemptyset, "sigemptyset is unportable - "
                 "use the gnulib module sigprocmask for portability");
# endif
# undef sigfillset
# if HAVE_RAW_DECL_SIGFILLSET
_GL_WARN_ON_USE (sigfillset, "sigfillset is unportable - "
                 "use the gnulib module sigprocmask for portability");
# endif
# undef sigismember
# if HAVE_RAW_DECL_SIGISMEMBER
_GL_WARN_ON_USE (sigismember, "sigismember is unportable - "
                 "use the gnulib module sigprocmask for portability");
# endif
# undef sigpending
# if HAVE_RAW_DECL_SIGPENDING
_GL_WARN_ON_USE (sigpending, "sigpending is unportable - "
                 "use the gnulib module sigprocmask for portability");
# endif
# undef sigprocmask
# if HAVE_RAW_DECL_SIGPROCMASK
_GL_WARN_ON_USE (sigprocmask, "sigprocmask is unportable - "
                 "use the gnulib module sigprocmask for portability");
# endif
#endif  


#if @GNULIB_SIGACTION@
# if !@HAVE_SIGACTION@

#  if !@HAVE_SIGINFO_T@

#   if !GNULIB_defined_siginfo_types

 
union sigval
{
  int sival_int;
  void *sival_ptr;
};

 
struct siginfo_t
{
  int si_signo;
  int si_code;
  int si_errno;
  pid_t si_pid;
  uid_t si_uid;
  void *si_addr;
  int si_status;
  long si_band;
  union sigval si_value;
};
typedef struct siginfo_t siginfo_t;

#    define GNULIB_defined_siginfo_types 1
#   endif

#  endif  

 

#  if !GNULIB_defined_struct_sigaction

struct sigaction
{
  union
  {
    void (*_sa_handler) (int);
     
    void (*_sa_sigaction) (int, siginfo_t *, void *);
  } _sa_func;
  sigset_t sa_mask;
   
  int sa_flags;
};
#   define sa_handler _sa_func._sa_handler
#   define sa_sigaction _sa_func._sa_sigaction
 
#   define SA_RESETHAND 1
#   define SA_NODEFER 2
#   define SA_RESTART 4

#   define GNULIB_defined_struct_sigaction 1
#  endif

_GL_FUNCDECL_SYS (sigaction, int, (int, const struct sigaction *restrict,
                                   struct sigaction *restrict));

# elif !@HAVE_STRUCT_SIGACTION_SA_SIGACTION@

#  define sa_sigaction sa_handler

# endif  

_GL_CXXALIAS_SYS (sigaction, int, (int, const struct sigaction *restrict,
                                   struct sigaction *restrict));
_GL_CXXALIASWARN (sigaction);

#elif defined GNULIB_POSIXCHECK
# undef sigaction
# if HAVE_RAW_DECL_SIGACTION
_GL_WARN_ON_USE (sigaction, "sigaction is unportable - "
                 "use the gnulib module sigaction for portability");
# endif
#endif

 
#ifndef SA_NODEFER
# define SA_NODEFER 0
#endif


#endif  
#endif  
#endif
