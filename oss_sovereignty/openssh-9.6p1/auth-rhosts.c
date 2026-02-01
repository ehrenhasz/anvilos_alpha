 
 

#include "includes.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_NETGROUP_H
# include <netgroup.h>
#endif
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#include "packet.h"
#include "uidswap.h"
#include "pathnames.h"
#include "log.h"
#include "misc.h"
#include "xmalloc.h"
#include "sshbuf.h"
#include "sshkey.h"
#include "servconf.h"
#include "canohost.h"
#include "hostfile.h"
#include "auth.h"

 
extern ServerOptions options;
extern int use_privsep;

 

static int
check_rhosts_file(const char *filename, const char *hostname,
		  const char *ipaddr, const char *client_user,
		  const char *server_user)
{
	FILE *f;
#define RBUFLN 1024
	char buf[RBUFLN]; 
	int fd;
	struct stat st;

	 
	if ((fd = open(filename, O_RDONLY|O_NONBLOCK)) == -1)
		return 0;
	if (fstat(fd, &st) == -1) {
		close(fd);
		return 0;
	}
	if (!S_ISREG(st.st_mode)) {
		logit("User %s hosts file %s is not a regular file",
		    server_user, filename);
		close(fd);
		return 0;
	}
	unset_nonblock(fd);
	if ((f = fdopen(fd, "r")) == NULL) {
		close(fd);
		return 0;
	}
	while (fgets(buf, sizeof(buf), f)) {
		 
		char hostbuf[RBUFLN], userbuf[RBUFLN], dummy[RBUFLN];
		char *host, *user, *cp;
		int negated;

		for (cp = buf; *cp == ' ' || *cp == '\t'; cp++)
			;
		if (*cp == '#' || *cp == '\n' || !*cp)
			continue;

		 
		if (strncmp(cp, "NO_PLUS", 7) == 0)
			continue;

		 
		switch (sscanf(buf, "%1023s %1023s %1023s", hostbuf, userbuf,
		    dummy)) {
		case 0:
			auth_debug_add("Found empty line in %.100s.", filename);
			continue;
		case 1:
			 
			strlcpy(userbuf, server_user, sizeof(userbuf));
			break;
		case 2:
			 
			break;
		case 3:
			auth_debug_add("Found garbage in %.100s.", filename);
			continue;
		default:
			 
			continue;
		}

		host = hostbuf;
		user = userbuf;
		negated = 0;

		 
		if (host[0] == '-') {
			negated = 1;
			host++;
		} else if (host[0] == '+')
			host++;

		if (user[0] == '-') {
			negated = 1;
			user++;
		} else if (user[0] == '+')
			user++;

		 
		if (!host[0] || !user[0]) {
			 
			auth_debug_add("Ignoring wild host/user names "
			    "in %.100s.", filename);
			continue;
		}
		 
		if (host[0] == '@') {
			if (!innetgr(host + 1, hostname, NULL, NULL) &&
			    !innetgr(host + 1, ipaddr, NULL, NULL))
				continue;
		} else if (strcasecmp(host, hostname) &&
		    strcmp(host, ipaddr) != 0)
			continue;	 

		 
		if (user[0] == '@') {
			if (!innetgr(user + 1, NULL, client_user, NULL))
				continue;
		} else if (strcmp(user, client_user) != 0)
			continue;	 

		 
		fclose(f);

		 
		if (negated) {
			auth_debug_add("Matched negative entry in %.100s.",
			    filename);
			return 0;
		}
		 
		return 1;
	}

	 
	fclose(f);
	return 0;
}

 
int
auth_rhosts2(struct passwd *pw, const char *client_user, const char *hostname,
    const char *ipaddr)
{
	char *path = NULL;
	struct stat st;
	static const char * const rhosts_files[] = {".shosts", ".rhosts", NULL};
	u_int rhosts_file_index;
	int r;

	debug2_f("clientuser %s hostname %s ipaddr %s",
	    client_user, hostname, ipaddr);

	 
	temporarily_use_uid(pw);
	 
	for (rhosts_file_index = 0; rhosts_files[rhosts_file_index];
	    rhosts_file_index++) {
		 
		xasprintf(&path, "%s/%s",
		    pw->pw_dir, rhosts_files[rhosts_file_index]);
		r = stat(path, &st);
		free(path);
		if (r >= 0)
			break;
	}
	 
	restore_uid();

	 
	if (!rhosts_files[rhosts_file_index] &&
	    stat(_PATH_RHOSTS_EQUIV, &st) == -1 &&
	    stat(_PATH_SSH_HOSTS_EQUIV, &st) == -1) {
		debug3_f("no hosts access files exist");
		return 0;
	}

	 
	if (pw->pw_uid == 0)
		debug3_f("root user, ignoring system hosts files");
	else {
		if (check_rhosts_file(_PATH_RHOSTS_EQUIV, hostname, ipaddr,
		    client_user, pw->pw_name)) {
			auth_debug_add("Accepted for %.100s [%.100s] by "
			    "/etc/hosts.equiv.", hostname, ipaddr);
			return 1;
		}
		if (check_rhosts_file(_PATH_SSH_HOSTS_EQUIV, hostname, ipaddr,
		    client_user, pw->pw_name)) {
			auth_debug_add("Accepted for %.100s [%.100s] by "
			    "%.100s.", hostname, ipaddr, _PATH_SSH_HOSTS_EQUIV);
			return 1;
		}
	}

	 
	if (stat(pw->pw_dir, &st) == -1) {
		logit("Rhosts authentication refused for %.100s: "
		    "no home directory %.200s", pw->pw_name, pw->pw_dir);
		auth_debug_add("Rhosts authentication refused for %.100s: "
		    "no home directory %.200s", pw->pw_name, pw->pw_dir);
		return 0;
	}
	if (options.strict_modes &&
	    ((st.st_uid != 0 && st.st_uid != pw->pw_uid) ||
	    (st.st_mode & 022) != 0)) {
		logit("Rhosts authentication refused for %.100s: "
		    "bad ownership or modes for home directory.", pw->pw_name);
		auth_debug_add("Rhosts authentication refused for %.100s: "
		    "bad ownership or modes for home directory.", pw->pw_name);
		return 0;
	}
	 
	temporarily_use_uid(pw);

	 
	for (rhosts_file_index = 0; rhosts_files[rhosts_file_index];
	    rhosts_file_index++) {
		 
		xasprintf(&path, "%s/%s",
		    pw->pw_dir, rhosts_files[rhosts_file_index]);
		if (stat(path, &st) == -1) {
			debug3_f("stat %s: %s", path, strerror(errno));
			free(path);
			continue;
		}

		 
		if (options.strict_modes &&
		    ((st.st_uid != 0 && st.st_uid != pw->pw_uid) ||
		    (st.st_mode & 022) != 0)) {
			logit("Rhosts authentication refused for %.100s: "
			    "bad modes for %.200s", pw->pw_name, path);
			auth_debug_add("Bad file modes for %.200s", path);
			free(path);
			continue;
		}
		 
		if (options.ignore_rhosts == IGNORE_RHOSTS_YES ||
		    (options.ignore_rhosts == IGNORE_RHOSTS_SHOSTS &&
		    strcmp(rhosts_files[rhosts_file_index], ".shosts") != 0)) {
			auth_debug_add("Server has been configured to "
			    "ignore %.100s.", rhosts_files[rhosts_file_index]);
			free(path);
			continue;
		}
		 
		if (check_rhosts_file(path, hostname, ipaddr,
		    client_user, pw->pw_name)) {
			auth_debug_add("Accepted by %.100s.",
			    rhosts_files[rhosts_file_index]);
			 
			restore_uid();
			auth_debug_add("Accepted host %s ip %s client_user "
			    "%s server_user %s", hostname, ipaddr,
			    client_user, pw->pw_name);
			free(path);
			return 1;
		}
		free(path);
	}

	 
	restore_uid();
	return 0;
}
