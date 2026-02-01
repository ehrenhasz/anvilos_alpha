 
 

#include "includes.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#include "sshlogin.h"
#include "ssherr.h"
#include "loginrec.h"
#include "log.h"
#include "sshbuf.h"
#include "misc.h"
#include "servconf.h"

extern struct sshbuf *loginmsg;
extern ServerOptions options;

 
time_t
get_last_login_time(uid_t uid, const char *logname,
    char *buf, size_t bufsize)
{
	struct logininfo li;

	login_get_lastlog(&li, uid);
	strlcpy(buf, li.hostname, bufsize);
	return (time_t)li.tv_sec;
}

 
static void
store_lastlog_message(const char *user, uid_t uid)
{
#ifndef NO_SSH_LASTLOG
# ifndef CUSTOM_SYS_AUTH_GET_LASTLOGIN_MSG
	char hostname[HOST_NAME_MAX+1] = "";
	time_t last_login_time;
# endif
	char *time_string;
	int r;

	if (!options.print_lastlog)
		return;

# ifdef CUSTOM_SYS_AUTH_GET_LASTLOGIN_MSG
	time_string = sys_auth_get_lastlogin_msg(user, uid);
	if (time_string != NULL) {
		if ((r = sshbuf_put(loginmsg,
		    time_string, strlen(time_string))) != 0)
			fatal("%s: buffer error: %s", __func__, ssh_err(r));
		free(time_string);
	}
# else
	last_login_time = get_last_login_time(uid, user, hostname,
	    sizeof(hostname));

	if (last_login_time != 0) {
		time_string = ctime(&last_login_time);
		time_string[strcspn(time_string, "\n")] = '\0';
		if (strcmp(hostname, "") == 0)
			r = sshbuf_putf(loginmsg, "Last login: %s\r\n",
			    time_string);
		else
			r = sshbuf_putf(loginmsg, "Last login: %s from %s\r\n",
			    time_string, hostname);
		if (r != 0)
			fatal_fr(r, "sshbuf_putf");
	}
# endif  
#endif  
}

 
void
record_login(pid_t pid, const char *tty, const char *user, uid_t uid,
    const char *host, struct sockaddr *addr, socklen_t addrlen)
{
	struct logininfo *li;

	 
	store_lastlog_message(user, uid);

	li = login_alloc_entry(pid, user, host, tty);
	login_set_addr(li, addr, addrlen);
	login_login(li);
	login_free_entry(li);
}

#ifdef LOGIN_NEEDS_UTMPX
void
record_utmp_only(pid_t pid, const char *ttyname, const char *user,
		 const char *host, struct sockaddr *addr, socklen_t addrlen)
{
	struct logininfo *li;

	li = login_alloc_entry(pid, user, host, ttyname);
	login_set_addr(li, addr, addrlen);
	login_utmp_only(li);
	login_free_entry(li);
}
#endif

 
void
record_logout(pid_t pid, const char *tty, const char *user)
{
	struct logininfo *li;

	li = login_alloc_entry(pid, user, NULL, tty);
	login_logout(li);
	login_free_entry(li);
}
