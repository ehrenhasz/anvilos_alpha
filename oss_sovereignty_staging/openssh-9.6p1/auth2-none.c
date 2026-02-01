 
 

#include "includes.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>

#include "atomicio.h"
#include "xmalloc.h"
#include "sshkey.h"
#include "hostfile.h"
#include "auth.h"
#include "packet.h"
#include "log.h"
#include "misc.h"
#include "servconf.h"
#include "ssh2.h"
#include "ssherr.h"
#ifdef GSSAPI
#include "ssh-gss.h"
#endif
#include "monitor_wrap.h"

 
extern ServerOptions options;

 
static int none_enabled = 1;

static int
userauth_none(struct ssh *ssh, const char *method)
{
	int r;

	none_enabled = 0;
	if ((r = sshpkt_get_end(ssh)) != 0)
		fatal_fr(r, "parse packet");
	if (options.permit_empty_passwd && options.password_authentication)
		return (PRIVSEP(auth_password(ssh, "")));
	return (0);
}

Authmethod method_none = {
	"none",
	NULL,
	userauth_none,
	&none_enabled
};
