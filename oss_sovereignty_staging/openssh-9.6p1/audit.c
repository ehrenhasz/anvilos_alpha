 

#include "includes.h"

#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#ifdef SSH_AUDIT_EVENTS

#include "audit.h"
#include "log.h"
#include "hostfile.h"
#include "auth.h"

 
extern Authctxt *the_authctxt;

 
ssh_audit_event_t
audit_classify_auth(const char *method)
{
	if (strcmp(method, "none") == 0)
		return SSH_AUTH_FAIL_NONE;
	else if (strcmp(method, "password") == 0)
		return SSH_AUTH_FAIL_PASSWD;
	else if (strcmp(method, "publickey") == 0 ||
	    strcmp(method, "rsa") == 0)
		return SSH_AUTH_FAIL_PUBKEY;
	else if (strncmp(method, "keyboard-interactive", 20) == 0 ||
	    strcmp(method, "challenge-response") == 0)
		return SSH_AUTH_FAIL_KBDINT;
	else if (strcmp(method, "hostbased") == 0 ||
	    strcmp(method, "rhosts-rsa") == 0)
		return SSH_AUTH_FAIL_HOSTBASED;
	else if (strcmp(method, "gssapi-with-mic") == 0)
		return SSH_AUTH_FAIL_GSSAPI;
	else
		return SSH_AUDIT_UNKNOWN;
}

 
const char *
audit_username(void)
{
	static const char unknownuser[] = "(unknown user)";
	static const char invaliduser[] = "(invalid user)";

	if (the_authctxt == NULL || the_authctxt->user == NULL)
		return (unknownuser);
	if (!the_authctxt->valid)
		return (invaliduser);
	return (the_authctxt->user);
}

const char *
audit_event_lookup(ssh_audit_event_t ev)
{
	int i;
	static struct event_lookup_struct {
		ssh_audit_event_t event;
		const char *name;
	} event_lookup[] = {
		{SSH_LOGIN_EXCEED_MAXTRIES,	"LOGIN_EXCEED_MAXTRIES"},
		{SSH_LOGIN_ROOT_DENIED,		"LOGIN_ROOT_DENIED"},
		{SSH_AUTH_SUCCESS,		"AUTH_SUCCESS"},
		{SSH_AUTH_FAIL_NONE,		"AUTH_FAIL_NONE"},
		{SSH_AUTH_FAIL_PASSWD,		"AUTH_FAIL_PASSWD"},
		{SSH_AUTH_FAIL_KBDINT,		"AUTH_FAIL_KBDINT"},
		{SSH_AUTH_FAIL_PUBKEY,		"AUTH_FAIL_PUBKEY"},
		{SSH_AUTH_FAIL_HOSTBASED,	"AUTH_FAIL_HOSTBASED"},
		{SSH_AUTH_FAIL_GSSAPI,		"AUTH_FAIL_GSSAPI"},
		{SSH_INVALID_USER,		"INVALID_USER"},
		{SSH_NOLOGIN,			"NOLOGIN"},
		{SSH_CONNECTION_CLOSE,		"CONNECTION_CLOSE"},
		{SSH_CONNECTION_ABANDON,	"CONNECTION_ABANDON"},
		{SSH_AUDIT_UNKNOWN,		"AUDIT_UNKNOWN"}
	};

	for (i = 0; event_lookup[i].event != SSH_AUDIT_UNKNOWN; i++)
		if (event_lookup[i].event == ev)
			break;
	return(event_lookup[i].name);
}

# ifndef CUSTOM_SSH_AUDIT_EVENTS
 

 
void
audit_connection_from(const char *host, int port)
{
	debug("audit connection from %s port %d euid %d", host, port,
	    (int)geteuid());
}

 
void
audit_event(struct ssh *ssh, ssh_audit_event_t event)
{
	debug("audit event euid %d user %s event %d (%s)", geteuid(),
	    audit_username(), event, audit_event_lookup(event));
}

 
void
audit_session_open(struct logininfo *li)
{
	const char *t = li->line ? li->line : "(no tty)";

	debug("audit session open euid %d user %s tty name %s", geteuid(),
	    audit_username(), t);
}

 
void
audit_session_close(struct logininfo *li)
{
	const char *t = li->line ? li->line : "(no tty)";

	debug("audit session close euid %d user %s tty name %s", geteuid(),
	    audit_username(), t);
}

 
void
audit_run_command(const char *command)
{
	debug("audit run command euid %d user %s command '%.200s'", geteuid(),
	    audit_username(), command);
}
# endif   
#endif  
