 

 

#ifndef AUTH_OPTIONS_H
#define AUTH_OPTIONS_H

struct passwd;
struct sshkey;

 
#define SSH_AUTHOPT_PERMIT_MAX	4096

 
#define SSH_AUTHOPT_ENV_MAX	1024

 
struct sshauthopt {
	 
	int permit_port_forwarding_flag;
	int permit_agent_forwarding_flag;
	int permit_x11_forwarding_flag;
	int permit_pty_flag;
	int permit_user_rc;

	 
	int restricted;

	 
	uint64_t valid_before;

	 
	int cert_authority;
	char *cert_principals;

	int force_tun_device;
	char *force_command;

	 
	size_t nenv;
	char **env;

	 
	size_t npermitopen;
	char **permitopen;

	 
	size_t npermitlisten;
	char **permitlisten;

	 
	char *required_from_host_cert;
	char *required_from_host_keys;

	 
	int no_require_user_presence;
	 
	int require_verify;
};

struct sshauthopt *sshauthopt_new(void);
struct sshauthopt *sshauthopt_new_with_keys_defaults(void);
void sshauthopt_free(struct sshauthopt *opts);
struct sshauthopt *sshauthopt_copy(const struct sshauthopt *orig);
int sshauthopt_serialise(const struct sshauthopt *opts, struct sshbuf *m, int);
int sshauthopt_deserialise(struct sshbuf *m, struct sshauthopt **opts);

 
struct sshauthopt *sshauthopt_parse(const char *s, const char **errstr);

 
struct sshauthopt *sshauthopt_from_cert(struct sshkey *k);

 
struct sshauthopt *sshauthopt_merge(const struct sshauthopt *primary,
    const struct sshauthopt *additional, const char **errstrp);

#endif
