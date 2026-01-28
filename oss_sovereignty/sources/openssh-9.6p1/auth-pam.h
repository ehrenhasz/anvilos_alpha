

#include "includes.h"
#ifdef USE_PAM

struct ssh;

void start_pam(struct ssh *);
void finish_pam(void);
u_int do_pam_account(void);
void do_pam_session(struct ssh *);
void do_pam_setcred(int );
void do_pam_chauthtok(void);
int do_pam_putenv(char *, char *);
char ** fetch_pam_environment(void);
char ** fetch_pam_child_environment(void);
void free_pam_environment(char **);
void sshpam_thread_cleanup(void);
void sshpam_cleanup(void);
int sshpam_auth_passwd(Authctxt *, const char *);
int sshpam_get_maxtries_reached(void);
void sshpam_set_maxtries_reached(int);
int is_pam_session_open(void);

#endif 
