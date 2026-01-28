

#ifdef _AIX

#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif

struct ssh;
struct sshbuf;


int usrinfo(int, char *, int);
#if defined(HAVE_DECL_SETAUTHDB) && (HAVE_DECL_SETAUTHDB == 0)
int setauthdb(const char *, char *);
#endif

#if defined(HAVE_DECL_AUTHENTICATE) && (HAVE_DECL_AUTHENTICATE == 0)
int authenticate(char *, char *, int *, char **);
#endif
#if defined(HAVE_DECL_LOGINFAILED) && (HAVE_DECL_LOGINFAILED == 0)
int loginfailed(char *, char *, char *);
#endif
#if defined(HAVE_DECL_LOGINRESTRICTIONS) && (HAVE_DECL_LOGINRESTRICTIONS == 0)
int loginrestrictions(char *, int, char *, char **);
#endif
#if defined(HAVE_DECL_LOGINSUCCESS) && (HAVE_DECL_LOGINSUCCESS == 0)
int loginsuccess(char *, char *, char *, char **);
#endif
#if defined(HAVE_DECL_PASSWDEXPIRED) && (HAVE_DECL_PASSWDEXPIRED == 0)
int passwdexpired(char *, char **);
#endif


#ifdef r_type
# undef r_type
#endif


#if !defined(HAVE_NANOSLEEP) && defined(HAVE_NSLEEP)
# define nanosleep(a,b) nsleep(a,b)
#endif


#ifdef HAVE_SYS_TIMERS_H
# include <sys/timers.h>
#endif


#ifdef HAVE_USERSEC_H
# include <usersec.h>
#endif


#ifdef HAVE_SETAUTHDB
# define REGISTRY_SIZE	16
#endif

void aix_usrinfo(struct passwd *);

#ifdef WITH_AIXAUTHENTICATE
# define CUSTOM_SYS_AUTH_PASSWD 1
# define CUSTOM_SYS_AUTH_ALLOWED_USER 1
int sys_auth_allowed_user(struct passwd *, struct sshbuf *);
# define CUSTOM_SYS_AUTH_RECORD_LOGIN 1
int sys_auth_record_login(const char *, const char *, const char *,
    struct sshbuf *);
# define CUSTOM_SYS_AUTH_GET_LASTLOGIN_MSG
char *sys_auth_get_lastlogin_msg(const char *, uid_t);
# define CUSTOM_FAILED_LOGIN 1
# if defined(S_AUTHDOMAIN)  && defined (S_AUTHNAME)
# define USE_AIX_KRB_NAME
char *aix_krb5_get_principal_name(const char *);
# endif
#endif

void aix_setauthdb(const char *);
void aix_restoreauthdb(void);
void aix_remove_embedded_newlines(char *);

#if defined(AIX_GETNAMEINFO_HACK) && !defined(BROKEN_GETADDRINFO)
# ifdef getnameinfo
#  undef getnameinfo
# endif
int sshaix_getnameinfo(const struct sockaddr *, size_t, char *, size_t,
    char *, size_t, int);
# define getnameinfo(a,b,c,d,e,f,g) (sshaix_getnameinfo(a,b,c,d,e,f,g))
#endif


#if !defined(HAVE_GETGROUPLIST) && defined(HAVE_GETGRSET)
# define HAVE_GETGROUPLIST
# define USE_GETGRSET
int getgrouplist(const char *, gid_t, gid_t *, int *);
#endif

#endif 
