 

 


 

 

#include "includes.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include <netinet/in.h>

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_PATHS_H
# include <paths.h>
#endif
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "xmalloc.h"
#include "sshkey.h"
#include "hostfile.h"
#include "ssh.h"
#include "loginrec.h"
#include "log.h"
#include "atomicio.h"
#include "packet.h"
#include "canohost.h"
#include "auth.h"
#include "sshbuf.h"
#include "ssherr.h"
#include "misc.h"

#ifdef HAVE_UTIL_H
# include <util.h>
#endif

 

#if HAVE_UTMP_H
void set_utmp_time(struct logininfo *li, struct utmp *ut);
void construct_utmp(struct logininfo *li, struct utmp *ut);
#endif

#ifdef HAVE_UTMPX_H
void set_utmpx_time(struct logininfo *li, struct utmpx *ut);
void construct_utmpx(struct logininfo *li, struct utmpx *ut);
#endif

int utmp_write_entry(struct logininfo *li);
int utmpx_write_entry(struct logininfo *li);
int wtmp_write_entry(struct logininfo *li);
int wtmpx_write_entry(struct logininfo *li);
int lastlog_write_entry(struct logininfo *li);
int syslogin_write_entry(struct logininfo *li);

int getlast_entry(struct logininfo *li);
int lastlog_get_entry(struct logininfo *li);
int utmpx_get_entry(struct logininfo *li);
int wtmp_get_entry(struct logininfo *li);
int wtmpx_get_entry(struct logininfo *li);

extern struct sshbuf *loginmsg;

 
#define MIN_SIZEOF(s1,s2) (sizeof(s1) < sizeof(s2) ? sizeof(s1) : sizeof(s2))

 

 
int
login_login(struct logininfo *li)
{
	li->type = LTYPE_LOGIN;
	return (login_write(li));
}


 
int
login_logout(struct logininfo *li)
{
	li->type = LTYPE_LOGOUT;
	return (login_write(li));
}

 
unsigned int
login_get_lastlog_time(const uid_t uid)
{
	struct logininfo li;

	if (login_get_lastlog(&li, uid))
		return (li.tv_sec);
	else
		return (0);
}

 
struct logininfo *
login_get_lastlog(struct logininfo *li, const uid_t uid)
{
	struct passwd *pw;

	memset(li, '\0', sizeof(*li));
	li->uid = uid;

	 
	pw = getpwuid(uid);
	if (pw == NULL)
		fatal("%s: Cannot find account for uid %ld", __func__,
		    (long)uid);

	if (strlcpy(li->username, pw->pw_name, sizeof(li->username)) >=
	    sizeof(li->username)) {
		error("%s: username too long (%lu > max %lu)", __func__,
		    (unsigned long)strlen(pw->pw_name),
		    (unsigned long)sizeof(li->username) - 1);
		return NULL;
	}

	if (getlast_entry(li))
		return (li);
	else
		return (NULL);
}

 
struct
logininfo *login_alloc_entry(pid_t pid, const char *username,
    const char *hostname, const char *line)
{
	struct logininfo *newli;

	newli = xmalloc(sizeof(*newli));
	login_init_entry(newli, pid, username, hostname, line);
	return (newli);
}


 
void
login_free_entry(struct logininfo *li)
{
	free(li);
}


 
int
login_init_entry(struct logininfo *li, pid_t pid, const char *username,
    const char *hostname, const char *line)
{
	struct passwd *pw;

	memset(li, 0, sizeof(*li));

	li->pid = pid;

	 
	if (line)
		line_fullname(li->line, line, sizeof(li->line));

	if (username) {
		strlcpy(li->username, username, sizeof(li->username));
		pw = getpwnam(li->username);
		if (pw == NULL) {
			fatal("%s: Cannot find user \"%s\"", __func__,
			    li->username);
		}
		li->uid = pw->pw_uid;
	}

	if (hostname)
		strlcpy(li->hostname, hostname, sizeof(li->hostname));

	return (1);
}

 
void
login_set_current_time(struct logininfo *li)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	li->tv_sec = tv.tv_sec;
	li->tv_usec = tv.tv_usec;
}

 
void
login_set_addr(struct logininfo *li, const struct sockaddr *sa,
    const unsigned int sa_size)
{
	unsigned int bufsize = sa_size;

	 
	if (sizeof(li->hostaddr) < sa_size)
		bufsize = sizeof(li->hostaddr);

	memcpy(&li->hostaddr.sa, sa, bufsize);
}


 
int
login_write(struct logininfo *li)
{
#ifndef HAVE_CYGWIN
	if (geteuid() != 0) {
		logit("Attempt to write login records by non-root user (aborting)");
		return (1);
	}
#endif

	 
	login_set_current_time(li);
#ifdef USE_LOGIN
	syslogin_write_entry(li);
#endif
#ifdef USE_LASTLOG
	if (li->type == LTYPE_LOGIN)
		lastlog_write_entry(li);
#endif
#ifdef USE_UTMP
	utmp_write_entry(li);
#endif
#ifdef USE_WTMP
	wtmp_write_entry(li);
#endif
#ifdef USE_UTMPX
	utmpx_write_entry(li);
#endif
#ifdef USE_WTMPX
	wtmpx_write_entry(li);
#endif
#ifdef CUSTOM_SYS_AUTH_RECORD_LOGIN
	if (li->type == LTYPE_LOGIN &&
	    !sys_auth_record_login(li->username,li->hostname,li->line,
	    loginmsg))
		logit("Writing login record failed for %s", li->username);
#endif
#ifdef SSH_AUDIT_EVENTS
	if (li->type == LTYPE_LOGIN)
		audit_session_open(li);
	else if (li->type == LTYPE_LOGOUT)
		audit_session_close(li);
#endif
	return (0);
}

#ifdef LOGIN_NEEDS_UTMPX
int
login_utmp_only(struct logininfo *li)
{
	li->type = LTYPE_LOGIN;
	login_set_current_time(li);
# ifdef USE_UTMP
	utmp_write_entry(li);
# endif
# ifdef USE_WTMP
	wtmp_write_entry(li);
# endif
# ifdef USE_UTMPX
	utmpx_write_entry(li);
# endif
# ifdef USE_WTMPX
	wtmpx_write_entry(li);
# endif
	return (0);
}
#endif

 

 
int
getlast_entry(struct logininfo *li)
{
#ifdef USE_LASTLOG
	return(lastlog_get_entry(li));
#else  
#if defined(USE_UTMPX) && defined(HAVE_SETUTXDB) && \
    defined(UTXDB_LASTLOGIN) && defined(HAVE_GETUTXUSER)
	return (utmpx_get_entry(li));
#endif

#if defined(DISABLE_LASTLOG)
	 
	return (0);
# elif defined(USE_WTMP) && \
    (defined(HAVE_TIME_IN_UTMP) || defined(HAVE_TV_IN_UTMP))
	 
	return (wtmp_get_entry(li));
# elif defined(USE_WTMPX) && \
    (defined(HAVE_TIME_IN_UTMPX) || defined(HAVE_TV_IN_UTMPX))
	 
	return (wtmpx_get_entry(li));
# else
	 
	return (0);
# endif  
#endif  
}



 


 
char *
line_fullname(char *dst, const char *src, u_int dstsize)
{
	memset(dst, '\0', dstsize);
	if ((strncmp(src, "/dev/", 5) == 0) || (dstsize < (strlen(src) + 5)))
		strlcpy(dst, src, dstsize);
	else {
		strlcpy(dst, "/dev/", dstsize);
		strlcat(dst, src, dstsize);
	}
	return (dst);
}

 
char *
line_stripname(char *dst, const char *src, int dstsize)
{
	memset(dst, '\0', dstsize);
	if (strncmp(src, "/dev/", 5) == 0)
		strlcpy(dst, src + 5, dstsize);
	else
		strlcpy(dst, src, dstsize);
	return (dst);
}

 
char *
line_abbrevname(char *dst, const char *src, int dstsize)
{
	size_t len;

	memset(dst, '\0', dstsize);

	 
	if (strncmp(src, "/dev/", 5) == 0)
		src += 5;

#ifdef WITH_ABBREV_NO_TTY
	if (strncmp(src, "tty", 3) == 0)
		src += 3;
#endif

	len = strlen(src);

	if (len > 0) {
		if (((int)len - dstsize) > 0)
			src +=  ((int)len - dstsize);

		 
		strncpy(dst, src, (size_t)dstsize);
	}

	return (dst);
}

 

#if defined(USE_UTMP) || defined (USE_WTMP) || defined (USE_LOGIN)

 
void
set_utmp_time(struct logininfo *li, struct utmp *ut)
{
# if defined(HAVE_TV_IN_UTMP)
	ut->ut_tv.tv_sec = li->tv_sec;
	ut->ut_tv.tv_usec = li->tv_usec;
# elif defined(HAVE_TIME_IN_UTMP)
	ut->ut_time = li->tv_sec;
# endif
}

void
construct_utmp(struct logininfo *li,
		    struct utmp *ut)
{
# ifdef HAVE_ADDR_V6_IN_UTMP
	struct sockaddr_in6 *sa6;
# endif

	memset(ut, '\0', sizeof(*ut));

	 

# ifdef HAVE_ID_IN_UTMP
	line_abbrevname(ut->ut_id, li->line, sizeof(ut->ut_id));
# endif

# ifdef HAVE_TYPE_IN_UTMP
	 
	switch (li->type) {
	case LTYPE_LOGIN:
		ut->ut_type = USER_PROCESS;
		break;
	case LTYPE_LOGOUT:
		ut->ut_type = DEAD_PROCESS;
		break;
	}
# endif
	set_utmp_time(li, ut);

	line_stripname(ut->ut_line, li->line, sizeof(ut->ut_line));

# ifdef HAVE_PID_IN_UTMP
	ut->ut_pid = li->pid;
# endif

	 
	if (li->type == LTYPE_LOGOUT)
		return;

	 

	 
	strncpy(ut->ut_name, li->username,
	    MIN_SIZEOF(ut->ut_name, li->username));
# ifdef HAVE_HOST_IN_UTMP
	strncpy(ut->ut_host, li->hostname,
	    MIN_SIZEOF(ut->ut_host, li->hostname));
# endif
# ifdef HAVE_ADDR_IN_UTMP
	 
	if (li->hostaddr.sa.sa_family == AF_INET)
		ut->ut_addr = li->hostaddr.sa_in.sin_addr.s_addr;
# endif
# ifdef HAVE_ADDR_V6_IN_UTMP
	 
	if (li->hostaddr.sa.sa_family == AF_INET6) {
		sa6 = ((struct sockaddr_in6 *)&li->hostaddr.sa);
		memcpy(ut->ut_addr_v6, sa6->sin6_addr.s6_addr, 16);
		if (IN6_IS_ADDR_V4MAPPED(&sa6->sin6_addr)) {
			ut->ut_addr_v6[0] = ut->ut_addr_v6[3];
			ut->ut_addr_v6[1] = 0;
			ut->ut_addr_v6[2] = 0;
			ut->ut_addr_v6[3] = 0;
		}
	}
# endif
}
#endif  

 

#if defined(USE_UTMPX) || defined (USE_WTMPX)
 
void
set_utmpx_time(struct logininfo *li, struct utmpx *utx)
{
# if defined(HAVE_TV_IN_UTMPX)
	utx->ut_tv.tv_sec = li->tv_sec;
	utx->ut_tv.tv_usec = li->tv_usec;
# elif defined(HAVE_TIME_IN_UTMPX)
	utx->ut_time = li->tv_sec;
# endif
}

void
construct_utmpx(struct logininfo *li, struct utmpx *utx)
{
# ifdef HAVE_ADDR_V6_IN_UTMP
	struct sockaddr_in6 *sa6;
#  endif
	memset(utx, '\0', sizeof(*utx));

# ifdef HAVE_ID_IN_UTMPX
	line_abbrevname(utx->ut_id, li->line, sizeof(utx->ut_id));
# endif

	 
	switch (li->type) {
	case LTYPE_LOGIN:
		utx->ut_type = USER_PROCESS;
		break;
	case LTYPE_LOGOUT:
		utx->ut_type = DEAD_PROCESS;
		break;
	}
	line_stripname(utx->ut_line, li->line, sizeof(utx->ut_line));
	set_utmpx_time(li, utx);
	utx->ut_pid = li->pid;

	 
	strncpy(utx->ut_user, li->username,
	    MIN_SIZEOF(utx->ut_user, li->username));

	if (li->type == LTYPE_LOGOUT)
		return;

	 

# ifdef HAVE_HOST_IN_UTMPX
	strncpy(utx->ut_host, li->hostname,
	    MIN_SIZEOF(utx->ut_host, li->hostname));
# endif
# ifdef HAVE_SS_IN_UTMPX
	utx->ut_ss = li->hostaddr.sa_storage;
# endif
# ifdef HAVE_ADDR_IN_UTMPX
	 
	if (li->hostaddr.sa.sa_family == AF_INET)
		utx->ut_addr = li->hostaddr.sa_in.sin_addr.s_addr;
# endif
# ifdef HAVE_ADDR_V6_IN_UTMP
	 
	if (li->hostaddr.sa.sa_family == AF_INET6) {
		sa6 = ((struct sockaddr_in6 *)&li->hostaddr.sa);
		memcpy(utx->ut_addr_v6, sa6->sin6_addr.s6_addr, 16);
		if (IN6_IS_ADDR_V4MAPPED(&sa6->sin6_addr)) {
			utx->ut_addr_v6[0] = utx->ut_addr_v6[3];
			utx->ut_addr_v6[1] = 0;
			utx->ut_addr_v6[2] = 0;
			utx->ut_addr_v6[3] = 0;
		}
	}
# endif
# ifdef HAVE_SYSLEN_IN_UTMPX
	 
	utx->ut_syslen = MINIMUM(strlen(li->hostname), sizeof(utx->ut_host));
# endif
}
#endif  

 

 
#ifdef USE_UTMP

 
# if !defined(DISABLE_PUTUTLINE) && defined(HAVE_SETUTENT) && \
	defined(HAVE_PUTUTLINE)
#  define UTMP_USE_LIBRARY
# endif


 
# ifdef UTMP_USE_LIBRARY
static int
utmp_write_library(struct logininfo *li, struct utmp *ut)
{
	setutent();
	pututline(ut);
#  ifdef HAVE_ENDUTENT
	endutent();
#  endif
	return (1);
}
# else  

 
static int
utmp_write_direct(struct logininfo *li, struct utmp *ut)
{
	struct utmp old_ut;
	register int fd;
	int tty;

	 

#if defined(HAVE_GETTTYENT)
	struct ttyent *ty;

	tty=0;
	setttyent();
	while (NULL != (ty = getttyent())) {
		tty++;
		if (!strncmp(ty->ty_name, ut->ut_line, sizeof(ut->ut_line)))
			break;
	}
	endttyent();

	if (NULL == ty) {
		logit("%s: tty not found", __func__);
		return (0);
	}
#else  

	tty = ttyslot();  

#endif  

	if (tty > 0 && (fd = open(UTMP_FILE, O_RDWR|O_CREAT, 0644)) >= 0) {
		off_t pos, ret;

		pos = (off_t)tty * sizeof(struct utmp);
		if ((ret = lseek(fd, pos, SEEK_SET)) == -1) {
			logit("%s: lseek: %s", __func__, strerror(errno));
			close(fd);
			return (0);
		}
		if (ret != pos) {
			logit("%s: Couldn't seek to tty %d slot in %s",
			    __func__, tty, UTMP_FILE);
			close(fd);
			return (0);
		}
		 
		if (atomicio(read, fd, &old_ut, sizeof(old_ut)) == sizeof(old_ut) &&
		    (ut->ut_host[0] == '\0') && (old_ut.ut_host[0] != '\0') &&
		    (strncmp(old_ut.ut_line, ut->ut_line, sizeof(ut->ut_line)) == 0) &&
		    (strncmp(old_ut.ut_name, ut->ut_name, sizeof(ut->ut_name)) == 0))
			memcpy(ut->ut_host, old_ut.ut_host, sizeof(ut->ut_host));

		if ((ret = lseek(fd, pos, SEEK_SET)) == -1) {
			logit("%s: lseek: %s", __func__, strerror(errno));
			close(fd);
			return (0);
		}
		if (ret != pos) {
			logit("%s: Couldn't seek to tty %d slot in %s",
			    __func__, tty, UTMP_FILE);
			close(fd);
			return (0);
		}
		if (atomicio(vwrite, fd, ut, sizeof(*ut)) != sizeof(*ut)) {
			logit("%s: error writing %s: %s", __func__,
			    UTMP_FILE, strerror(errno));
			close(fd);
			return (0);
		}

		close(fd);
		return (1);
	} else {
		return (0);
	}
}
# endif  

static int
utmp_perform_login(struct logininfo *li)
{
	struct utmp ut;

	construct_utmp(li, &ut);
# ifdef UTMP_USE_LIBRARY
	if (!utmp_write_library(li, &ut)) {
		logit("%s: utmp_write_library() failed", __func__);
		return (0);
	}
# else
	if (!utmp_write_direct(li, &ut)) {
		logit("%s: utmp_write_direct() failed", __func__);
		return (0);
	}
# endif
	return (1);
}


static int
utmp_perform_logout(struct logininfo *li)
{
	struct utmp ut;

	construct_utmp(li, &ut);
# ifdef UTMP_USE_LIBRARY
	if (!utmp_write_library(li, &ut)) {
		logit("%s: utmp_write_library() failed", __func__);
		return (0);
	}
# else
	if (!utmp_write_direct(li, &ut)) {
		logit("%s: utmp_write_direct() failed", __func__);
		return (0);
	}
# endif
	return (1);
}


int
utmp_write_entry(struct logininfo *li)
{
	switch(li->type) {
	case LTYPE_LOGIN:
		return (utmp_perform_login(li));

	case LTYPE_LOGOUT:
		return (utmp_perform_logout(li));

	default:
		logit("%s: invalid type field", __func__);
		return (0);
	}
}
#endif  


 

 
#ifdef USE_UTMPX

 
# if !defined(DISABLE_PUTUTXLINE) && defined(HAVE_SETUTXENT) && \
	defined(HAVE_PUTUTXLINE)
#  define UTMPX_USE_LIBRARY
# endif


 
# ifdef UTMPX_USE_LIBRARY
static int
utmpx_write_library(struct logininfo *li, struct utmpx *utx)
{
	setutxent();
	pututxline(utx);

#  ifdef HAVE_ENDUTXENT
	endutxent();
#  endif
	return (1);
}

# else  

 
static int
utmpx_write_direct(struct logininfo *li, struct utmpx *utx)
{
	logit("%s: not implemented!", __func__);
	return (0);
}
# endif  

static int
utmpx_perform_login(struct logininfo *li)
{
	struct utmpx utx;

	construct_utmpx(li, &utx);
# ifdef UTMPX_USE_LIBRARY
	if (!utmpx_write_library(li, &utx)) {
		logit("%s: utmp_write_library() failed", __func__);
		return (0);
	}
# else
	if (!utmpx_write_direct(li, &ut)) {
		logit("%s: utmp_write_direct() failed", __func__);
		return (0);
	}
# endif
	return (1);
}


static int
utmpx_perform_logout(struct logininfo *li)
{
	struct utmpx utx;

	construct_utmpx(li, &utx);
# ifdef HAVE_ID_IN_UTMPX
	line_abbrevname(utx.ut_id, li->line, sizeof(utx.ut_id));
# endif
# ifdef HAVE_TYPE_IN_UTMPX
	utx.ut_type = DEAD_PROCESS;
# endif

# ifdef UTMPX_USE_LIBRARY
	utmpx_write_library(li, &utx);
# else
	utmpx_write_direct(li, &utx);
# endif
	return (1);
}

int
utmpx_write_entry(struct logininfo *li)
{
	switch(li->type) {
	case LTYPE_LOGIN:
		return (utmpx_perform_login(li));
	case LTYPE_LOGOUT:
		return (utmpx_perform_logout(li));
	default:
		logit("%s: invalid type field", __func__);
		return (0);
	}
}
#endif  


 

#ifdef USE_WTMP

 
static int
wtmp_write(struct logininfo *li, struct utmp *ut)
{
	struct stat buf;
	int fd, ret = 1;

	if ((fd = open(WTMP_FILE, O_WRONLY|O_APPEND, 0)) < 0) {
		logit("%s: problem writing %s: %s", __func__,
		    WTMP_FILE, strerror(errno));
		return (0);
	}
	if (fstat(fd, &buf) == 0)
		if (atomicio(vwrite, fd, ut, sizeof(*ut)) != sizeof(*ut)) {
			ftruncate(fd, buf.st_size);
			logit("%s: problem writing %s: %s", __func__,
			    WTMP_FILE, strerror(errno));
			ret = 0;
		}
	close(fd);
	return (ret);
}

static int
wtmp_perform_login(struct logininfo *li)
{
	struct utmp ut;

	construct_utmp(li, &ut);
	return (wtmp_write(li, &ut));
}


static int
wtmp_perform_logout(struct logininfo *li)
{
	struct utmp ut;

	construct_utmp(li, &ut);
	return (wtmp_write(li, &ut));
}


int
wtmp_write_entry(struct logininfo *li)
{
	switch(li->type) {
	case LTYPE_LOGIN:
		return (wtmp_perform_login(li));
	case LTYPE_LOGOUT:
		return (wtmp_perform_logout(li));
	default:
		logit("%s: invalid type field", __func__);
		return (0);
	}
}


 

 
static int
wtmp_islogin(struct logininfo *li, struct utmp *ut)
{
	if (strncmp(li->username, ut->ut_name,
	    MIN_SIZEOF(li->username, ut->ut_name)) == 0) {
# ifdef HAVE_TYPE_IN_UTMP
		if (ut->ut_type & USER_PROCESS)
			return (1);
# else
		return (1);
# endif
	}
	return (0);
}

int
wtmp_get_entry(struct logininfo *li)
{
	struct stat st;
	struct utmp ut;
	int fd, found = 0;

	 
	li->tv_sec = li->tv_usec = 0;

	if ((fd = open(WTMP_FILE, O_RDONLY)) < 0) {
		logit("%s: problem opening %s: %s", __func__,
		    WTMP_FILE, strerror(errno));
		return (0);
	}
	if (fstat(fd, &st) != 0) {
		logit("%s: couldn't stat %s: %s", __func__,
		    WTMP_FILE, strerror(errno));
		close(fd);
		return (0);
	}

	 
	if (lseek(fd, -(off_t)sizeof(struct utmp), SEEK_END) == -1) {
		 
		close(fd);
		return (0);
	}

	while (!found) {
		if (atomicio(read, fd, &ut, sizeof(ut)) != sizeof(ut)) {
			logit("%s: read of %s failed: %s", __func__,
			    WTMP_FILE, strerror(errno));
			close (fd);
			return (0);
		}
		if (wtmp_islogin(li, &ut) ) {
			found = 1;
			 
# ifdef HAVE_TIME_IN_UTMP
			li->tv_sec = ut.ut_time;
# else
#  if HAVE_TV_IN_UTMP
			li->tv_sec = ut.ut_tv.tv_sec;
#  endif
# endif
			line_fullname(li->line, ut.ut_line,
			    MIN_SIZEOF(li->line, ut.ut_line));
# ifdef HAVE_HOST_IN_UTMP
			strlcpy(li->hostname, ut.ut_host,
			    MIN_SIZEOF(li->hostname, ut.ut_host));
# endif
			continue;
		}
		 
		if (lseek(fd, -(off_t)(2 * sizeof(struct utmp)), SEEK_CUR) == -1) {
			 
			close(fd);
			return (0);
		}
	}

	 
	close(fd);
	return (1);
}
# endif  


 

#ifdef USE_WTMPX
 
static int
wtmpx_write(struct logininfo *li, struct utmpx *utx)
{
#ifndef HAVE_UPDWTMPX
	struct stat buf;
	int fd, ret = 1;

	if ((fd = open(WTMPX_FILE, O_WRONLY|O_APPEND, 0)) < 0) {
		logit("%s: problem opening %s: %s", __func__,
		    WTMPX_FILE, strerror(errno));
		return (0);
	}

	if (fstat(fd, &buf) == 0)
		if (atomicio(vwrite, fd, utx, sizeof(*utx)) != sizeof(*utx)) {
			ftruncate(fd, buf.st_size);
			logit("%s: problem writing %s: %s", __func__,
			    WTMPX_FILE, strerror(errno));
			ret = 0;
		}
	close(fd);

	return (ret);
#else
	updwtmpx(WTMPX_FILE, utx);
	return (1);
#endif
}


static int
wtmpx_perform_login(struct logininfo *li)
{
	struct utmpx utx;

	construct_utmpx(li, &utx);
	return (wtmpx_write(li, &utx));
}


static int
wtmpx_perform_logout(struct logininfo *li)
{
	struct utmpx utx;

	construct_utmpx(li, &utx);
	return (wtmpx_write(li, &utx));
}


int
wtmpx_write_entry(struct logininfo *li)
{
	switch(li->type) {
	case LTYPE_LOGIN:
		return (wtmpx_perform_login(li));
	case LTYPE_LOGOUT:
		return (wtmpx_perform_logout(li));
	default:
		logit("%s: invalid type field", __func__);
		return (0);
	}
}

 

 
static int
wtmpx_islogin(struct logininfo *li, struct utmpx *utx)
{
	if (strncmp(li->username, utx->ut_user,
	    MIN_SIZEOF(li->username, utx->ut_user)) == 0 ) {
# ifdef HAVE_TYPE_IN_UTMPX
		if (utx->ut_type == USER_PROCESS)
			return (1);
# else
		return (1);
# endif
	}
	return (0);
}


int
wtmpx_get_entry(struct logininfo *li)
{
	struct stat st;
	struct utmpx utx;
	int fd, found=0;

	 
	li->tv_sec = li->tv_usec = 0;

	if ((fd = open(WTMPX_FILE, O_RDONLY)) < 0) {
		logit("%s: problem opening %s: %s", __func__,
		    WTMPX_FILE, strerror(errno));
		return (0);
	}
	if (fstat(fd, &st) != 0) {
		logit("%s: couldn't stat %s: %s", __func__,
		    WTMPX_FILE, strerror(errno));
		close(fd);
		return (0);
	}

	 
	if (lseek(fd, -(off_t)sizeof(struct utmpx), SEEK_END) == -1 ) {
		 
		close(fd);
		return (0);
	}

	while (!found) {
		if (atomicio(read, fd, &utx, sizeof(utx)) != sizeof(utx)) {
			logit("%s: read of %s failed: %s", __func__,
			    WTMPX_FILE, strerror(errno));
			close (fd);
			return (0);
		}
		 
		if (wtmpx_islogin(li, &utx)) {
			found = 1;
# if defined(HAVE_TV_IN_UTMPX)
			li->tv_sec = utx.ut_tv.tv_sec;
# elif defined(HAVE_TIME_IN_UTMPX)
			li->tv_sec = utx.ut_time;
# endif
			line_fullname(li->line, utx.ut_line, sizeof(li->line));
# if defined(HAVE_HOST_IN_UTMPX)
			strlcpy(li->hostname, utx.ut_host,
			    MIN_SIZEOF(li->hostname, utx.ut_host));
# endif
			continue;
		}
		if (lseek(fd, -(off_t)(2 * sizeof(struct utmpx)), SEEK_CUR) == -1) {
			close(fd);
			return (0);
		}
	}

	close(fd);
	return (1);
}
#endif  

 

#ifdef USE_LOGIN
static int
syslogin_perform_login(struct logininfo *li)
{
	struct utmp *ut;

	ut = xmalloc(sizeof(*ut));
	construct_utmp(li, ut);
	login(ut);
	free(ut);

	return (1);
}

static int
syslogin_perform_logout(struct logininfo *li)
{
# ifdef HAVE_LOGOUT
	char line[UT_LINESIZE];

	(void)line_stripname(line, li->line, sizeof(line));

	if (!logout(line))
		logit("%s: logout() returned an error", __func__);
#  ifdef HAVE_LOGWTMP
	else
		logwtmp(line, "", "");
#  endif
	 
# endif
	return (1);
}

int
syslogin_write_entry(struct logininfo *li)
{
	switch (li->type) {
	case LTYPE_LOGIN:
		return (syslogin_perform_login(li));
	case LTYPE_LOGOUT:
		return (syslogin_perform_logout(li));
	default:
		logit("%s: Invalid type field", __func__);
		return (0);
	}
}
#endif  

 

 

#ifdef USE_LASTLOG

#if !defined(LASTLOG_WRITE_PUTUTXLINE) || !defined(HAVE_GETLASTLOGXBYNAME)
 
static int
lastlog_openseek(struct logininfo *li, int *fd, int filemode)
{
	off_t offset;
	char lastlog_file[1024];
	struct stat st;

	if (stat(LASTLOG_FILE, &st) != 0) {
		logit("%s: Couldn't stat %s: %s", __func__,
		    LASTLOG_FILE, strerror(errno));
		return (0);
	}
	if (S_ISDIR(st.st_mode)) {
		snprintf(lastlog_file, sizeof(lastlog_file), "%s/%s",
		    LASTLOG_FILE, li->username);
	} else if (S_ISREG(st.st_mode)) {
		strlcpy(lastlog_file, LASTLOG_FILE, sizeof(lastlog_file));
	} else {
		logit("%s: %.100s is not a file or directory!", __func__,
		    LASTLOG_FILE);
		return (0);
	}

	*fd = open(lastlog_file, filemode, 0600);
	if (*fd < 0) {
		debug("%s: Couldn't open %s: %s", __func__,
		    lastlog_file, strerror(errno));
		return (0);
	}

	if (S_ISREG(st.st_mode)) {
		 
		offset = (off_t) ((u_long)li->uid * sizeof(struct lastlog));

		if (lseek(*fd, offset, SEEK_SET) != offset) {
			logit("%s: %s->lseek(): %s", __func__,
			    lastlog_file, strerror(errno));
			close(*fd);
			return (0);
		}
	}

	return (1);
}
#endif  

#ifdef LASTLOG_WRITE_PUTUTXLINE
int
lastlog_write_entry(struct logininfo *li)
{
	switch(li->type) {
	case LTYPE_LOGIN:
		return 1;  
	default:
		logit("lastlog_write_entry: Invalid type field");
		return 0;
	}
}
#else  
int
lastlog_write_entry(struct logininfo *li)
{
	struct lastlog last;
	int fd;

	switch(li->type) {
	case LTYPE_LOGIN:
		 
		memset(&last, '\0', sizeof(last));
		line_stripname(last.ll_line, li->line, sizeof(last.ll_line));
		strlcpy(last.ll_host, li->hostname,
		    MIN_SIZEOF(last.ll_host, li->hostname));
		last.ll_time = li->tv_sec;
	
		if (!lastlog_openseek(li, &fd, O_RDWR|O_CREAT))
			return (0);
	
		 
		if (atomicio(vwrite, fd, &last, sizeof(last)) != sizeof(last)) {
			close(fd);
			logit("%s: Error writing to %s: %s", __func__,
			    LASTLOG_FILE, strerror(errno));
			return (0);
		}
	
		close(fd);
		return (1);
	default:
		logit("%s: Invalid type field", __func__);
		return (0);
	}
}
#endif  

#ifdef HAVE_GETLASTLOGXBYNAME
int
lastlog_get_entry(struct logininfo *li)
{
	struct lastlogx l, *ll;

	if ((ll = getlastlogxbyname(li->username, &l)) == NULL) {
		memset(&l, '\0', sizeof(l));
		ll = &l;
	}
	line_fullname(li->line, ll->ll_line, sizeof(li->line));
	strlcpy(li->hostname, ll->ll_host,
		MIN_SIZEOF(li->hostname, ll->ll_host));
	li->tv_sec = ll->ll_tv.tv_sec;
	li->tv_usec = ll->ll_tv.tv_usec;
	return (1);
}
#else  
int
lastlog_get_entry(struct logininfo *li)
{
	struct lastlog last;
	int fd, ret;

	if (!lastlog_openseek(li, &fd, O_RDONLY))
		return (0);

	ret = atomicio(read, fd, &last, sizeof(last));
	close(fd);

	switch (ret) {
	case 0:
		memset(&last, '\0', sizeof(last));
		 
	case sizeof(last):
		line_fullname(li->line, last.ll_line, sizeof(li->line));
		strlcpy(li->hostname, last.ll_host,
		    MIN_SIZEOF(li->hostname, last.ll_host));
		li->tv_sec = last.ll_time;
		return (1);
	case -1:
		error("%s: Error reading from %s: %s", __func__,
		    LASTLOG_FILE, strerror(errno));
		return (0);
	default:
		error("%s: Error reading from %s: Expecting %d, got %d",
		    __func__, LASTLOG_FILE, (int)sizeof(last), ret);
		return (0);
	}

	 
	return (0);
}
#endif  
#endif  

#if defined(USE_UTMPX) && defined(HAVE_SETUTXDB) && \
    defined(UTXDB_LASTLOGIN) && defined(HAVE_GETUTXUSER)
int
utmpx_get_entry(struct logininfo *li)
{
	struct utmpx *utx;

	if (setutxdb(UTXDB_LASTLOGIN, NULL) != 0)
		return (0);
	utx = getutxuser(li->username);
	if (utx == NULL) {
		endutxent();
		return (0);
	}

	line_fullname(li->line, utx->ut_line,
	    MIN_SIZEOF(li->line, utx->ut_line));
	strlcpy(li->hostname, utx->ut_host,
	    MIN_SIZEOF(li->hostname, utx->ut_host));
	li->tv_sec = utx->ut_tv.tv_sec;
	li->tv_usec = utx->ut_tv.tv_usec;
	endutxent();
	return (1);
}
#endif  

#ifdef USE_BTMP
   

void
record_failed_login(struct ssh *ssh, const char *username, const char *hostname,
    const char *ttyn)
{
	int fd;
	struct utmp ut;
	struct sockaddr_storage from;
	socklen_t fromlen = sizeof(from);
	struct sockaddr_in *a4;
	struct sockaddr_in6 *a6;
	time_t t;
	struct stat fst;

	if (geteuid() != 0)
		return;
	if ((fd = open(_PATH_BTMP, O_WRONLY | O_APPEND)) < 0) {
		debug("Unable to open the btmp file %s: %s", _PATH_BTMP,
		    strerror(errno));
		return;
	}
	if (fstat(fd, &fst) < 0) {
		logit("%s: fstat of %s failed: %s", __func__, _PATH_BTMP,
		    strerror(errno));
		goto out;
	}
	if((fst.st_mode & (S_IXGRP | S_IRWXO)) || (fst.st_uid != 0)){
		logit("Excess permission or bad ownership on file %s",
		    _PATH_BTMP);
		goto out;
	}

	memset(&ut, 0, sizeof(ut));
	 
	strncpy(ut.ut_user, username, sizeof(ut.ut_user));
	strlcpy(ut.ut_line, "ssh:notty", sizeof(ut.ut_line));

	time(&t);
	ut.ut_time = t;      
	ut.ut_type = LOGIN_PROCESS;
	ut.ut_pid = getpid();

	 
	strncpy(ut.ut_host, hostname, sizeof(ut.ut_host));

	if (ssh_packet_connection_is_on_socket(ssh) &&
	    getpeername(ssh_packet_get_connection_in(ssh),
	    (struct sockaddr *)&from, &fromlen) == 0) {
		ipv64_normalise_mapped(&from, &fromlen);
		if (from.ss_family == AF_INET) {
			a4 = (struct sockaddr_in *)&from;
			memcpy(&ut.ut_addr, &(a4->sin_addr),
			    MIN_SIZEOF(ut.ut_addr, a4->sin_addr));
		}
#ifdef HAVE_ADDR_V6_IN_UTMP
		if (from.ss_family == AF_INET6) {
			a6 = (struct sockaddr_in6 *)&from;
			memcpy(&ut.ut_addr_v6, &(a6->sin6_addr),
			    MIN_SIZEOF(ut.ut_addr_v6, a6->sin6_addr));
		}
#endif
	}

	if (atomicio(vwrite, fd, &ut, sizeof(ut)) != sizeof(ut))
		error("Failed to write to %s: %s", _PATH_BTMP,
		    strerror(errno));

out:
	close(fd);
}
#endif	 
