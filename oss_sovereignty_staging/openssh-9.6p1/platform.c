 

#include "includes.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "misc.h"
#include "servconf.h"
#include "sshkey.h"
#include "hostfile.h"
#include "auth.h"
#include "auth-pam.h"
#include "platform.h"

#include "openbsd-compat/openbsd-compat.h"

extern int use_privsep;
extern ServerOptions options;

void
platform_pre_listen(void)
{
#ifdef LINUX_OOM_ADJUST
	 
	oom_adjust_setup();
#endif
}

void
platform_pre_fork(void)
{
#ifdef USE_SOLARIS_PROCESS_CONTRACTS
	solaris_contract_pre_fork();
#endif
}

void
platform_pre_restart(void)
{
#ifdef LINUX_OOM_ADJUST
	oom_adjust_restore();
#endif
}

void
platform_post_fork_parent(pid_t child_pid)
{
#ifdef USE_SOLARIS_PROCESS_CONTRACTS
	solaris_contract_post_fork_parent(child_pid);
#endif
}

void
platform_post_fork_child(void)
{
#ifdef USE_SOLARIS_PROCESS_CONTRACTS
	solaris_contract_post_fork_child();
#endif
#ifdef LINUX_OOM_ADJUST
	oom_adjust_restore();
#endif
}

 
int
platform_privileged_uidswap(void)
{
#ifdef HAVE_CYGWIN
	 
	return 1;
#else
	return (getuid() == 0 || geteuid() == 0);
#endif
}

 
void
platform_setusercontext(struct passwd *pw)
{
#ifdef WITH_SELINUX
	 
	(void)ssh_selinux_enabled();
#endif

#ifdef USE_SOLARIS_PROJECTS
	 
	if (!options.use_pam && (getuid() == 0 || geteuid() == 0))
		solaris_set_default_project(pw);
#endif

#if defined(HAVE_LOGIN_CAP) && defined (__bsdi__)
	if (getuid() == 0 || geteuid() == 0)
		setpgid(0, 0);
# endif

#if defined(HAVE_LOGIN_CAP) && defined(USE_PAM)
	 
	if (getuid() == 0 || geteuid() == 0) {
		if (options.use_pam) {
			do_pam_setcred(use_privsep);
		}
	}
# endif  

#if !defined(HAVE_LOGIN_CAP) && defined(HAVE_GETLUID) && defined(HAVE_SETLUID)
	if (getuid() == 0 || geteuid() == 0) {
		 
		if (getluid() == -1 && setluid(pw->pw_uid) == -1)
			error("setluid: %s", strerror(errno));
	}
#endif
}

 
void
platform_setusercontext_post_groups(struct passwd *pw)
{
#if !defined(HAVE_LOGIN_CAP) && defined(USE_PAM)
	 
	if (options.use_pam) {
		do_pam_setcred(use_privsep);
	}
#endif  

#if !defined(HAVE_LOGIN_CAP) && (defined(WITH_IRIX_PROJECT) || \
    defined(WITH_IRIX_JOBS) || defined(WITH_IRIX_ARRAY))
	irix_setusercontext(pw);
#endif  

#ifdef _AIX
	aix_usrinfo(pw);
#endif  

#ifdef HAVE_SETPCRED
	 
	{
		char **creds = NULL, *chroot_creds[] =
		    { "REAL_USER=root", NULL };

		if (options.chroot_directory != NULL &&
		    strcasecmp(options.chroot_directory, "none") != 0)
			creds = chroot_creds;

		if (setpcred(pw->pw_name, creds) == -1)
			fatal("Failed to set process credentials");
	}
#endif  
#ifdef WITH_SELINUX
	ssh_selinux_setup_exec_context(pw->pw_name);
#endif
}

char *
platform_krb5_get_principal_name(const char *pw_name)
{
#ifdef USE_AIX_KRB_NAME
	return aix_krb5_get_principal_name(pw_name);
#else
	return NULL;
#endif
}

 
int
platform_locked_account(struct passwd *pw)
{
	int locked = 0;
	char *passwd = pw->pw_passwd;
#ifdef USE_SHADOW
	struct spwd *spw = NULL;
#ifdef USE_LIBIAF
	char *iaf_passwd = NULL;
#endif

	spw = getspnam(pw->pw_name);
#ifdef HAS_SHADOW_EXPIRE
	if (spw != NULL && auth_shadow_acctexpired(spw))
		return 1;
#endif  

	if (spw != NULL)
#ifdef USE_LIBIAF
		iaf_passwd = passwd = get_iaf_password(pw);
#else
		passwd = spw->sp_pwdp;
#endif  
#endif

	 
	if (passwd && *passwd) {
#ifdef LOCKED_PASSWD_STRING
		if (strcmp(passwd, LOCKED_PASSWD_STRING) == 0)
			locked = 1;
#endif
#ifdef LOCKED_PASSWD_PREFIX
		if (strncmp(passwd, LOCKED_PASSWD_PREFIX,
		    strlen(LOCKED_PASSWD_PREFIX)) == 0)
			locked = 1;
#endif
#ifdef LOCKED_PASSWD_SUBSTR
		if (strstr(passwd, LOCKED_PASSWD_SUBSTR))
			locked = 1;
#endif
	}
#ifdef USE_LIBIAF
	if (iaf_passwd != NULL)
		freezero(iaf_passwd, strlen(iaf_passwd));
#endif  

	return locked;
}
