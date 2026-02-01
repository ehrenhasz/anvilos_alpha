 
 

#include "includes.h"

#include <errno.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>

#include <grp.h>

#include "log.h"
#include "uidswap.h"
#include "xmalloc.h"

 

#if defined(_POSIX_SAVED_IDS) && !defined(BROKEN_SAVED_UIDS)
 
#define SAVED_IDS_WORK_WITH_SETEUID
 
static uid_t	saved_euid = 0;
static gid_t	saved_egid = 0;
#endif

 
static int	privileged = 0;
static int	temporarily_use_uid_effective = 0;
static uid_t	user_groups_uid;
static gid_t	*saved_egroups = NULL, *user_groups = NULL;
static int	saved_egroupslen = -1, user_groupslen = -1;

 
void
temporarily_use_uid(struct passwd *pw)
{
	 
#ifdef SAVED_IDS_WORK_WITH_SETEUID
	saved_euid = geteuid();
	saved_egid = getegid();
	debug("temporarily_use_uid: %u/%u (e=%u/%u)",
	    (u_int)pw->pw_uid, (u_int)pw->pw_gid,
	    (u_int)saved_euid, (u_int)saved_egid);
#ifndef HAVE_CYGWIN
	if (saved_euid != 0) {
		privileged = 0;
		return;
	}
#endif
#else
	if (geteuid() != 0) {
		privileged = 0;
		return;
	}
#endif  

	privileged = 1;
	temporarily_use_uid_effective = 1;

	saved_egroupslen = getgroups(0, NULL);
	if (saved_egroupslen == -1)
		fatal("getgroups: %.100s", strerror(errno));
	if (saved_egroupslen > 0) {
		saved_egroups = xreallocarray(saved_egroups,
		    saved_egroupslen, sizeof(gid_t));
		if (getgroups(saved_egroupslen, saved_egroups) == -1)
			fatal("getgroups: %.100s", strerror(errno));
	} else {  
		free(saved_egroups);
		saved_egroups = NULL;
	}

	 
	if (user_groupslen == -1 || user_groups_uid != pw->pw_uid) {
		if (initgroups(pw->pw_name, pw->pw_gid) == -1)
			fatal("initgroups: %s: %.100s", pw->pw_name,
			    strerror(errno));

		user_groupslen = getgroups(0, NULL);
		if (user_groupslen == -1)
			fatal("getgroups: %.100s", strerror(errno));
		if (user_groupslen > 0) {
			user_groups = xreallocarray(user_groups,
			    user_groupslen, sizeof(gid_t));
			if (getgroups(user_groupslen, user_groups) == -1)
				fatal("getgroups: %.100s", strerror(errno));
		} else {  
			free(user_groups);
			user_groups = NULL;
		}
		user_groups_uid = pw->pw_uid;
	}
	 
	if (setgroups(user_groupslen, user_groups) == -1)
		fatal("setgroups: %.100s", strerror(errno));
#ifndef SAVED_IDS_WORK_WITH_SETEUID
	 
	if (setgid(getegid()) == -1)
		debug("setgid %u: %.100s", (u_int) getegid(), strerror(errno));
	 
	if (setuid(geteuid()) == -1)
		debug("setuid %u: %.100s", (u_int) geteuid(), strerror(errno));
#endif  
	if (setegid(pw->pw_gid) == -1)
		fatal("setegid %u: %.100s", (u_int)pw->pw_gid,
		    strerror(errno));
	if (seteuid(pw->pw_uid) == -1)
		fatal("seteuid %u: %.100s", (u_int)pw->pw_uid,
		    strerror(errno));
}

 
void
restore_uid(void)
{
	 
	if (!privileged) {
		debug("restore_uid: (unprivileged)");
		return;
	}
	if (!temporarily_use_uid_effective)
		fatal("restore_uid: temporarily_use_uid not effective");

#ifdef SAVED_IDS_WORK_WITH_SETEUID
	debug("restore_uid: %u/%u", (u_int)saved_euid, (u_int)saved_egid);
	 
	if (seteuid(saved_euid) == -1)
		fatal("seteuid %u: %.100s", (u_int)saved_euid, strerror(errno));
	if (setegid(saved_egid) == -1)
		fatal("setegid %u: %.100s", (u_int)saved_egid, strerror(errno));
#else  
	 
	if (setuid(getuid()) == -1)
		fatal("%s: setuid failed: %s", __func__, strerror(errno));
	if (setgid(getgid()) == -1)
		fatal("%s: setgid failed: %s", __func__, strerror(errno));
#endif  

	if (setgroups(saved_egroupslen, saved_egroups) == -1)
		fatal("setgroups: %.100s", strerror(errno));
	temporarily_use_uid_effective = 0;
}

 
void
permanently_set_uid(struct passwd *pw)
{
#ifndef NO_UID_RESTORATION_TEST
	uid_t old_uid = getuid();
	gid_t old_gid = getgid();
#endif

	if (pw == NULL)
		fatal("permanently_set_uid: no user given");
	if (temporarily_use_uid_effective)
		fatal("permanently_set_uid: temporarily_use_uid effective");
	debug("permanently_set_uid: %u/%u", (u_int)pw->pw_uid,
	    (u_int)pw->pw_gid);

	if (setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) == -1)
		fatal("setresgid %u: %.100s", (u_int)pw->pw_gid, strerror(errno));

#ifdef __APPLE__
	 
	if (initgroups(pw->pw_name, pw->pw_gid) == -1)
		fatal("initgroups %.100s %u: %.100s",
		    pw->pw_name, (u_int)pw->pw_gid, strerror(errno));
#endif

	if (setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) == -1)
		fatal("setresuid %u: %.100s", (u_int)pw->pw_uid, strerror(errno));

#ifndef NO_UID_RESTORATION_TEST
	 
	if (old_gid != pw->pw_gid && pw->pw_uid != 0 &&
	    (setgid(old_gid) != -1 || setegid(old_gid) != -1))
		fatal("%s: was able to restore old [e]gid", __func__);
#endif

	 
	if (getgid() != pw->pw_gid || getegid() != pw->pw_gid) {
		fatal("%s: egid incorrect gid:%u egid:%u (should be %u)",
		    __func__, (u_int)getgid(), (u_int)getegid(),
		    (u_int)pw->pw_gid);
	}

#ifndef NO_UID_RESTORATION_TEST
	 
	if (old_uid != pw->pw_uid &&
	    (setuid(old_uid) != -1 || seteuid(old_uid) != -1))
		fatal("%s: was able to restore old [e]uid", __func__);
#endif

	 
	if (getuid() != pw->pw_uid || geteuid() != pw->pw_uid) {
		fatal("%s: euid incorrect uid:%u euid:%u (should be %u)",
		    __func__, (u_int)getuid(), (u_int)geteuid(),
		    (u_int)pw->pw_uid);
	}
}
