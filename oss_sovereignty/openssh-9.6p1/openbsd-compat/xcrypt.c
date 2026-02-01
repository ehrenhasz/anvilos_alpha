 

#include "includes.h"

#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

# if defined(HAVE_CRYPT_H) && !defined(HAVE_SECUREWARE)
#  include <crypt.h>
# endif

# ifdef __hpux
#  include <hpsecurity.h>
#  include <prot.h>
# endif

# ifdef HAVE_SECUREWARE
#  include <sys/security.h>
#  include <sys/audit.h>
#  include <prot.h>
# endif

# if defined(HAVE_SHADOW_H) && !defined(DISABLE_SHADOW)
#  include <shadow.h>
# endif

# if defined(HAVE_GETPWANAM) && !defined(DISABLE_SHADOW)
#  include <sys/label.h>
#  include <sys/audit.h>
#  include <pwdadj.h>
# endif

# if defined(WITH_OPENSSL) && !defined(HAVE_CRYPT) && defined(HAVE_DES_CRYPT)
#  include <openssl/des.h>
#  define crypt DES_crypt
# endif

#define MINIMUM(a, b)	(((a) < (b)) ? (a) : (b))

 
static const char *
pick_salt(void)
{
	struct passwd *pw;
	char *passwd, *p;
	size_t typelen;
	static char salt[32];

	if (salt[0] != '\0')
		return salt;
	strlcpy(salt, "xx", sizeof(salt));
	setpwent();
	while ((pw = getpwent()) != NULL) {
		if ((passwd = shadow_pw(pw)) == NULL)
			continue;
		if (passwd[0] == '$' && (p = strrchr(passwd+1, '$')) != NULL) {
			typelen = p - passwd + 1;
			strlcpy(salt, passwd, MINIMUM(typelen, sizeof(salt)));
			explicit_bzero(passwd, strlen(passwd));
			goto out;
		}
	}
 out:
	endpwent();
	return salt;
}

char *
xcrypt(const char *password, const char *salt)
{
	char *crypted;

	 
	if (salt == NULL)
		salt = pick_salt();

#if defined(__hpux) && !defined(HAVE_SECUREWARE)
	if (iscomsec())
		crypted = bigcrypt(password, salt);
	else
		crypted = crypt(password, salt);
# elif defined(HAVE_SECUREWARE)
	crypted = bigcrypt(password, salt);
# else
	crypted = crypt(password, salt);
#endif

	return crypted;
}

 

char *
shadow_pw(struct passwd *pw)
{
	char *pw_password = pw->pw_passwd;

# if defined(HAVE_SHADOW_H) && !defined(DISABLE_SHADOW)
	struct spwd *spw = getspnam(pw->pw_name);

	if (spw != NULL)
		pw_password = spw->sp_pwdp;
# endif

#ifdef USE_LIBIAF
	return(get_iaf_password(pw));
#endif

# if defined(HAVE_GETPWANAM) && !defined(DISABLE_SHADOW)
	struct passwd_adjunct *spw;
	if (issecure() && (spw = getpwanam(pw->pw_name)) != NULL)
		pw_password = spw->pwa_passwd;
# elif defined(HAVE_SECUREWARE)
	struct pr_passwd *spw = getprpwnam(pw->pw_name);

	if (spw != NULL)
		pw_password = spw->ufld.fd_encrypt;
# endif

	return pw_password;
}
