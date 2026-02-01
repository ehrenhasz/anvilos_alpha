 

 

#if !defined (_SIGLIST_H_)
#define _SIGLIST_H_

#if !defined (SYS_SIGLIST_DECLARED) && !defined (HAVE_STRSIGNAL)

#if defined (HAVE_UNDER_SYS_SIGLIST) && !defined (HAVE_SYS_SIGLIST) && !defined (sys_siglist)
#  define sys_siglist _sys_siglist
#endif  

#if !defined (sys_siglist)
extern char *sys_siglist[];
#endif  

#endif  

#if !defined (strsignal) && !defined (HAVE_STRSIGNAL)
#  define strsignal(sig) (char *)sys_siglist[sig]
#endif  

#if !defined (strsignal) && !HAVE_DECL_STRSIGNAL
extern char *strsignal PARAMS((int));
#endif

#endif  
