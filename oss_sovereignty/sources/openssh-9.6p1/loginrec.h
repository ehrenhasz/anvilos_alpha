#ifndef _HAVE_LOGINREC_H_
#define _HAVE_LOGINREC_H_





#include "includes.h"

struct ssh;





union login_netinfo {
	struct sockaddr sa;
	struct sockaddr_in sa_in;
	struct sockaddr_storage sa_storage;
};




#define LTYPE_LOGIN    7
#define LTYPE_LOGOUT   8


#define LINFO_PROGSIZE 64
#define LINFO_LINESIZE 64
#define LINFO_NAMESIZE 512
#define LINFO_HOSTSIZE 256

struct logininfo {
	char       progname[LINFO_PROGSIZE];     
	int        progname_null;
	short int  type;                         
	pid_t      pid;                          
	uid_t      uid;                          
	char       line[LINFO_LINESIZE];         
	char       username[LINFO_NAMESIZE];     
	char       hostname[LINFO_HOSTSIZE];     
	
	int        exit;                        
	int        termination;                 
	
	unsigned int tv_sec;
	unsigned int tv_usec;
	union login_netinfo hostaddr;       
}; 






struct logininfo *login_alloc_entry(pid_t pid, const char *username,
				    const char *hostname, const char *line);

void login_free_entry(struct logininfo *li);

int login_init_entry(struct logininfo *li, pid_t pid, const char *username,
    const char *hostname, const char *line);

void login_set_current_time(struct logininfo *li);


int login_login (struct logininfo *li);
int login_logout(struct logininfo *li);
#ifdef LOGIN_NEEDS_UTMPX
int login_utmp_only(struct logininfo *li);
#endif




int login_write (struct logininfo *li);
int login_log_entry(struct logininfo *li);


void login_set_addr(struct logininfo *li, const struct sockaddr *sa,
		    const unsigned int sa_size);



struct logininfo *login_get_lastlog(struct logininfo *li, const uid_t uid);

unsigned int login_get_lastlog_time(const uid_t uid);


char *line_fullname(char *dst, const char *src, u_int dstsize);
char *line_stripname(char *dst, const char *src, int dstsize);
char *line_abbrevname(char *dst, const char *src, int dstsize);

void record_failed_login(struct ssh *, const char *, const char *,
    const char *);

#endif 
