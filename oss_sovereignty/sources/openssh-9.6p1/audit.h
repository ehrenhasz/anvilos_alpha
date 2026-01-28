

#ifndef _SSH_AUDIT_H
# define _SSH_AUDIT_H

#include "loginrec.h"

struct ssh;

enum ssh_audit_event_type {
	SSH_LOGIN_EXCEED_MAXTRIES,
	SSH_LOGIN_ROOT_DENIED,
	SSH_AUTH_SUCCESS,
	SSH_AUTH_FAIL_NONE,
	SSH_AUTH_FAIL_PASSWD,
	SSH_AUTH_FAIL_KBDINT,	
	SSH_AUTH_FAIL_PUBKEY,	
	SSH_AUTH_FAIL_HOSTBASED,	
	SSH_AUTH_FAIL_GSSAPI,
	SSH_INVALID_USER,
	SSH_NOLOGIN,		
	SSH_CONNECTION_CLOSE,	
	SSH_CONNECTION_ABANDON,	
	SSH_AUDIT_UNKNOWN
};
typedef enum ssh_audit_event_type ssh_audit_event_t;

void	audit_connection_from(const char *, int);
void	audit_event(struct ssh *, ssh_audit_event_t);
void	audit_session_open(struct logininfo *);
void	audit_session_close(struct logininfo *);
void	audit_run_command(const char *);
ssh_audit_event_t audit_classify_auth(const char *);

#endif 
