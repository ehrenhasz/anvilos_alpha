 

#include "includes.h"
#if defined(USE_LINUX_AUDIT)
#include <libaudit.h>
#include <unistd.h>
#include <string.h>

#include "log.h"
#include "audit.h"
#include "canohost.h"
#include "packet.h"

const char *audit_username(void);

int
linux_audit_record_event(int uid, const char *username, const char *hostname,
    const char *ip, const char *ttyn, int success)
{
	int audit_fd, rc, saved_errno;

	if ((audit_fd = audit_open()) < 0) {
		if (errno == EINVAL || errno == EPROTONOSUPPORT ||
		    errno == EAFNOSUPPORT)
			return 1;  
		else
			return 0;  
	}
	rc = audit_log_acct_message(audit_fd, AUDIT_USER_LOGIN,
	    NULL, "login", username ? username : "(unknown)",
	    username == NULL ? uid : -1, hostname, ip, ttyn, success);
	saved_errno = errno;
	close(audit_fd);

	 
	if ((rc == -EPERM) && (geteuid() != 0))
		rc = 0;
	errno = saved_errno;

	return rc >= 0;
}

 

void
audit_connection_from(const char *host, int port)
{
	 
}

void
audit_run_command(const char *command)
{
	 
}

void
audit_session_open(struct logininfo *li)
{
	if (linux_audit_record_event(li->uid, NULL, li->hostname, NULL,
	    li->line, 1) == 0)
		fatal("linux_audit_write_entry failed: %s", strerror(errno));
}

void
audit_session_close(struct logininfo *li)
{
	 
}

void
audit_event(struct ssh *ssh, ssh_audit_event_t event)
{
	switch(event) {
	case SSH_AUTH_SUCCESS:
	case SSH_CONNECTION_CLOSE:
	case SSH_NOLOGIN:
	case SSH_LOGIN_EXCEED_MAXTRIES:
	case SSH_LOGIN_ROOT_DENIED:
		break;
	case SSH_AUTH_FAIL_NONE:
	case SSH_AUTH_FAIL_PASSWD:
	case SSH_AUTH_FAIL_KBDINT:
	case SSH_AUTH_FAIL_PUBKEY:
	case SSH_AUTH_FAIL_HOSTBASED:
	case SSH_AUTH_FAIL_GSSAPI:
	case SSH_INVALID_USER:
		linux_audit_record_event(-1, audit_username(), NULL,
		    ssh_remote_ipaddr(ssh), "sshd", 0);
		break;
	default:
		debug("%s: unhandled event %d", __func__, event);
		break;
	}
}
#endif  
