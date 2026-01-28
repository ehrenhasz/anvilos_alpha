



#ifndef AUTH_H
#define AUTH_H

#include <signal.h>
#include <stdio.h>

#ifdef HAVE_LOGIN_CAP
#include <login_cap.h>
#endif
#ifdef BSD_AUTH
#include <bsd_auth.h>
#endif
#ifdef KRB5
#include <krb5.h>
#endif

struct passwd;
struct ssh;
struct sshbuf;
struct sshkey;
struct sshkey_cert;
struct sshauthopt;

typedef struct Authctxt Authctxt;
typedef struct Authmethod Authmethod;
typedef struct KbdintDevice KbdintDevice;

struct Authctxt {
	sig_atomic_t	 success;
	int		 authenticated;	
	int		 postponed;	
	int		 valid;		
	int		 attempt;
	int		 failures;
	int		 server_caused_failure;
	int		 force_pwchange;
	char		*user;		
	char		*service;
	struct passwd	*pw;		
	char		*style;

	
	char		**auth_methods;	
	u_int		 num_auth_methods;

	
	void		*methoddata;
	void		*kbdintctxt;
#ifdef BSD_AUTH
	auth_session_t	*as;
#endif
#ifdef KRB5
	krb5_context	 krb5_ctx;
	krb5_ccache	 krb5_fwd_ccache;
	krb5_principal	 krb5_user;
	char		*krb5_ticket_file;
	char		*krb5_ccname;
#endif
	struct sshbuf	*loginmsg;

	
	struct sshkey	**prev_keys;
	u_int		 nprev_keys;

	
	struct sshkey	*auth_method_key;
	char		*auth_method_info;

	
	struct sshbuf	*session_info;	
};



struct Authmethod {
	char	*name;
	char	*synonym;
	int	(*userauth)(struct ssh *, const char *);
	int	*enabled;
};


struct KbdintDevice
{
	const char *name;
	void*	(*init_ctx)(Authctxt*);
	int	(*query)(void *ctx, char **name, char **infotxt,
		    u_int *numprompts, char ***prompts, u_int **echo_on);
	int	(*respond)(void *ctx, u_int numresp, char **responses);
	void	(*free_ctx)(void *ctx);
};

int
auth_rhosts2(struct passwd *, const char *, const char *, const char *);

int      auth_password(struct ssh *, const char *);

int	 hostbased_key_allowed(struct ssh *, struct passwd *,
	    const char *, char *, struct sshkey *);
int	 user_key_allowed(struct ssh *ssh, struct passwd *, struct sshkey *,
    int, struct sshauthopt **);
int	 auth2_key_already_used(Authctxt *, const struct sshkey *);


void	 auth2_authctxt_reset_info(Authctxt *);
void	 auth2_record_key(Authctxt *, int, const struct sshkey *);
void	 auth2_record_info(Authctxt *authctxt, const char *, ...)
	    __attribute__((__format__ (printf, 2, 3)))
	    __attribute__((__nonnull__ (2)));
void	 auth2_update_session_info(Authctxt *, const char *, const char *);

#ifdef KRB5
int	auth_krb5(Authctxt *authctxt, krb5_data *auth, char **client, krb5_data *);
int	auth_krb5_tgt(Authctxt *authctxt, krb5_data *tgt);
int	auth_krb5_password(Authctxt *authctxt, const char *password);
void	krb5_cleanup_proc(Authctxt *authctxt);
#endif 

#if defined(USE_SHADOW) && defined(HAS_SHADOW_EXPIRE)
#include <shadow.h>
int auth_shadow_acctexpired(struct spwd *);
int auth_shadow_pwexpired(Authctxt *);
#endif

#include "auth-pam.h"
#include "audit.h"
void remove_kbdint_device(const char *);

void	do_authentication2(struct ssh *);

void	auth_log(struct ssh *, int, int, const char *, const char *);
void	auth_maxtries_exceeded(struct ssh *) __attribute__((noreturn));
void	userauth_finish(struct ssh *, int, const char *, const char *);
int	auth_root_allowed(struct ssh *, const char *);

char	*auth2_read_banner(void);
int	 auth2_methods_valid(const char *, int);
int	 auth2_update_methods_lists(Authctxt *, const char *, const char *);
int	 auth2_setup_methods_lists(Authctxt *);
int	 auth2_method_allowed(Authctxt *, const char *, const char *);

void	privsep_challenge_enable(void);

int	auth2_challenge(struct ssh *, char *);
void	auth2_challenge_stop(struct ssh *);
int	bsdauth_query(void *, char **, char **, u_int *, char ***, u_int **);
int	bsdauth_respond(void *, u_int, char **);

int	allowed_user(struct ssh *, struct passwd *);
struct passwd * getpwnamallow(struct ssh *, const char *user);

char	*expand_authorized_keys(const char *, struct passwd *pw);
char	*authorized_principals_file(struct passwd *);

int	 auth_key_is_revoked(struct sshkey *);

const char	*auth_get_canonical_hostname(struct ssh *, int);

HostStatus
check_key_in_hostfiles(struct passwd *, struct sshkey *, const char *,
    const char *, const char *);


struct sshkey	*get_hostkey_by_index(int);
struct sshkey	*get_hostkey_public_by_index(int, struct ssh *);
struct sshkey	*get_hostkey_public_by_type(int, int, struct ssh *);
struct sshkey	*get_hostkey_private_by_type(int, int, struct ssh *);
int	 get_hostkey_index(struct sshkey *, int, struct ssh *);
int	 sshd_hostkey_sign(struct ssh *, struct sshkey *, struct sshkey *,
    u_char **, size_t *, const u_char *, size_t, const char *);


const struct sshauthopt *auth_options(struct ssh *);
int	 auth_activate_options(struct ssh *, struct sshauthopt *);
void	 auth_restrict_session(struct ssh *);
void	 auth_log_authopts(const char *, const struct sshauthopt *, int);


void	 auth_debug_add(const char *fmt,...)
    __attribute__((format(printf, 1, 2)));
void	 auth_debug_send(struct ssh *);
void	 auth_debug_reset(void);

struct passwd *fakepw(void);


int	 auth_authorise_keyopts(struct passwd *, struct sshauthopt *, int,
    const char *, const char *, const char *);
int	 auth_check_principals_line(char *, const struct sshkey_cert *,
    const char *, struct sshauthopt **);
int	 auth_process_principals(FILE *, const char *,
    const struct sshkey_cert *, struct sshauthopt **);
int	 auth_check_authkey_line(struct passwd *, struct sshkey *,
    char *, const char *, const char *, const char *, struct sshauthopt **);
int	 auth_check_authkeys_file(struct passwd *, FILE *, char *,
    struct sshkey *, const char *, const char *, struct sshauthopt **);
FILE	*auth_openkeyfile(const char *, struct passwd *, int);
FILE	*auth_openprincipals(const char *, struct passwd *, int);

int	 sys_auth_passwd(struct ssh *, const char *);

#if defined(KRB5) && !defined(HEIMDAL)
krb5_error_code ssh_krb5_cc_gen(krb5_context, krb5_ccache *);
#endif

#endif 
