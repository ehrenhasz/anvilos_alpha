 

#include "includes.h"

#if defined(HAVE_LIBIAF)  &&  !defined(HAVE_SECUREWARE)
#include <sys/types.h>
#ifdef HAVE_CRYPT_H
# include <crypt.h>
#endif
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "xmalloc.h"
#include "packet.h"
#include "auth-options.h"
#include "log.h"
#include "misc.h"	 
#include "servconf.h"
#include "hostfile.h"
#include "auth.h"
#include "ssh.h"
#include "ssh_api.h"

int nischeck(char *);

int
sys_auth_passwd(struct ssh *ssh, const char *password)
{
	Authctxt *authctxt = ssh->authctxt;
	struct passwd *pw = authctxt->pw;
	char *salt;
	int result;

	 
	char *pw_password = authctxt->valid ? shadow_pw(pw) : pw->pw_passwd;

	if (pw_password == NULL)
		return 0;

	 
	if (strcmp(pw_password, "") == 0 && strcmp(password, "") == 0)
		return (1);

	 
	salt = (pw_password[0] && pw_password[1]) ? pw_password : "xx";

	 
#ifdef UNIXWARE_LONG_PASSWORDS
	if (!nischeck(pw->pw_name)) {
		result = ((strcmp(bigcrypt(password, salt), pw_password) == 0)
		||  (strcmp(osr5bigcrypt(password, salt), pw_password) == 0));
	}
	else
#endif  
		result = (strcmp(xcrypt(password, salt), pw_password) == 0);

#ifdef USE_LIBIAF
	if (authctxt->valid)
		free(pw_password);
#endif
	return(result);
}

#ifdef UNIXWARE_LONG_PASSWORDS
int
nischeck(char *namep)
{
	char password_file[] = "/etc/passwd";
	FILE *fd;
	struct passwd *ent = NULL;

	if ((fd = fopen (password_file, "r")) == NULL) {
		 
		return(0);
	}

	 
	while (ent = fgetpwent(fd)) {
		if (strcmp (ent->pw_name, namep) == 0) {
			 
			fclose (fd);
			return(0);
		}
	}

	fclose (fd);
	return (1);
}

#endif  

 

#ifdef USE_LIBIAF
char *
get_iaf_password(struct passwd *pw)
{
	char *pw_password = NULL;

	uinfo_t uinfo;
	if (!ia_openinfo(pw->pw_name,&uinfo)) {
		ia_get_logpwd(uinfo, &pw_password);
		if (pw_password == NULL)
			fatal("ia_get_logpwd: Unable to get the shadow passwd");
		ia_closeinfo(uinfo);
		return pw_password;
	}
	else
		fatal("ia_openinfo: Unable to open the shadow passwd file");
}
#endif  
#endif  

