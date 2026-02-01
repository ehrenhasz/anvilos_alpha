 

#ifndef _BSD_CYGWIN_UTIL_H
#define _BSD_CYGWIN_UTIL_H

#ifdef HAVE_CYGWIN

#undef ERROR

 
typedef void *HANDLE;
#define INVALID_HANDLE_VALUE ((HANDLE) -1)
#define DNLEN 16
#define UNLEN 256

 
extern HANDLE cygwin_logon_user (const struct passwd *, const char *);
extern void cygwin_set_impersonation_token (const HANDLE);

#include <sys/cygwin.h>
#include <io.h>

#define CYGWIN_SSH_PRIVSEP_USER (cygwin_ssh_privsep_user())
const char *cygwin_ssh_privsep_user();

int binary_open(const char *, int , ...);
int check_ntsec(const char *);
char **fetch_windows_environment(void);
void free_windows_environment(char **);
int cygwin_ug_match_pattern_list(const char *, const char *);

#ifndef NO_BINARY_OPEN
#define open binary_open
#endif

#endif  

#endif  
