 
 

#include "includes.h"

#include <sys/types.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "xmalloc.h"
#include "packet.h"
#include "hostfile.h"
#include "auth.h"
#include "log.h"
#include "misc.h"
#include "servconf.h"
#include "ssherr.h"

 
extern ServerOptions options;

static int
userauth_kbdint(struct ssh *ssh, const char *method)
{
	int r, authenticated = 0;
	char *lang, *devs;

	if ((r = sshpkt_get_cstring(ssh, &lang, NULL)) != 0 ||
	    (r = sshpkt_get_cstring(ssh, &devs, NULL)) != 0 ||
	    (r = sshpkt_get_end(ssh)) != 0)
		fatal_fr(r, "parse packet");

	debug("keyboard-interactive devs %s", devs);

	if (options.kbd_interactive_authentication)
		authenticated = auth2_challenge(ssh, devs);

	free(devs);
	free(lang);
	return authenticated;
}

Authmethod method_kbdint = {
	"keyboard-interactive",
	NULL,
	userauth_kbdint,
	&options.kbd_interactive_authentication
};
