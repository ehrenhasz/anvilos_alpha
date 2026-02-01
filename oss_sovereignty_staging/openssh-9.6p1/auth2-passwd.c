 
 

#include "includes.h"

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "packet.h"
#include "ssherr.h"
#include "log.h"
#include "sshkey.h"
#include "hostfile.h"
#include "auth.h"
#ifdef GSSAPI
#include "ssh-gss.h"
#endif
#include "monitor_wrap.h"
#include "misc.h"
#include "servconf.h"

 
extern ServerOptions options;

static int
userauth_passwd(struct ssh *ssh, const char *method)
{
	char *password = NULL;
	int authenticated = 0, r;
	u_char change;
	size_t len = 0;

	if ((r = sshpkt_get_u8(ssh, &change)) != 0 ||
	    (r = sshpkt_get_cstring(ssh, &password, &len)) != 0 ||
	    (change && (r = sshpkt_get_cstring(ssh, NULL, NULL)) != 0) ||
	    (r = sshpkt_get_end(ssh)) != 0) {
		freezero(password, len);
		fatal_fr(r, "parse packet");
	}

	if (change)
		logit("password change not supported");
	else if (PRIVSEP(auth_password(ssh, password)) == 1)
		authenticated = 1;
	freezero(password, len);
	return authenticated;
}

Authmethod method_passwd = {
	"password",
	NULL,
	userauth_passwd,
	&options.password_authentication
};
