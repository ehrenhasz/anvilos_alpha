 

 

#ifndef SERVCONF_H
#define SERVCONF_H

#include <openbsd-compat/sys-queue.h>

#define MAX_PORTS		256	 

 
#define	PERMIT_NOT_SET		-1
#define	PERMIT_NO		0
#define	PERMIT_FORCED_ONLY	1
#define	PERMIT_NO_PASSWD	2
#define	PERMIT_YES		3

 
#define PRIVSEP_OFF		0
#define PRIVSEP_ON		1
#define PRIVSEP_NOSANDBOX	2

 
#define PERMITOPEN_ANY		0
#define PERMITOPEN_NONE		-2

 
#define IGNORE_RHOSTS_NO	0
#define IGNORE_RHOSTS_YES	1
#define IGNORE_RHOSTS_SHOSTS	2

#define DEFAULT_AUTH_FAIL_MAX	6	 
#define DEFAULT_SESSIONS_MAX	10	 

 
#define INTERNAL_SFTP_NAME	"internal-sftp"

 
#define PUBKEYAUTH_TOUCH_REQUIRED	(1)
#define PUBKEYAUTH_VERIFY_REQUIRED	(1<<1)

struct ssh;
struct fwd_perm_list;

 
struct queued_listenaddr {
	char *addr;
	int port;  
	char *rdomain;
};

 
struct listenaddr {
	char *rdomain;
	struct addrinfo *addrs;
};

typedef struct {
	u_int	num_ports;
	u_int	ports_from_cmdline;
	int	ports[MAX_PORTS];	 
	struct queued_listenaddr *queued_listen_addrs;
	u_int	num_queued_listens;
	struct listenaddr *listen_addrs;
	u_int	num_listen_addrs;
	int	address_family;		 

	char	*routing_domain;	 

	char   **host_key_files;	 
	int	*host_key_file_userprovided;  
	u_int	num_host_key_files;      
	char   **host_cert_files;	 
	u_int	num_host_cert_files;	 

	char   *host_key_agent;		 
	char   *pid_file;		 
	char   *moduli_file;		 
	int     login_grace_time;	 
	int     permit_root_login;	 
	int     ignore_rhosts;	 
	int     ignore_user_known_hosts;	 
	int     print_motd;	 
	int	print_lastlog;	 
	int     x11_forwarding;	 
	int     x11_display_offset;	 
	int     x11_use_localhost;	 
	char   *xauth_location;	 
	int	permit_tty;	 
	int	permit_user_rc;	 
	int     strict_modes;	 
	int     tcp_keep_alive;	 
	int	ip_qos_interactive;	 
	int	ip_qos_bulk;		 
	char   *ciphers;	 
	char   *macs;		 
	char   *kex_algorithms;	 
	struct ForwardOptions fwd_opts;	 
	SyslogFacility log_facility;	 
	LogLevel log_level;	 
	u_int	num_log_verbose;	 
	char	**log_verbose;
	int     hostbased_authentication;	 
	int     hostbased_uses_name_from_packet_only;  
	char   *hostbased_accepted_algos;  
	char   *hostkeyalgorithms;	 
	char   *ca_sign_algorithms;	 
	int     pubkey_authentication;	 
	char   *pubkey_accepted_algos;	 
	int	pubkey_auth_options;	 
	int     kerberos_authentication;	 
	int     kerberos_or_local_passwd;	 
	int     kerberos_ticket_cleanup;	 
	int     kerberos_get_afs_token;		 
	int     gss_authentication;	 
	int     gss_cleanup_creds;	 
	int     gss_strict_acceptor;	 
	int     password_authentication;	 
	int     kbd_interactive_authentication;	 
	int     permit_empty_passwd;	 
	int     permit_user_env;	 
	char   *permit_user_env_allowlist;  
	int     compression;	 
	int	allow_tcp_forwarding;  
	int	allow_streamlocal_forwarding;  
	int	allow_agent_forwarding;
	int	disable_forwarding;
	u_int num_allow_users;
	char   **allow_users;
	u_int num_deny_users;
	char   **deny_users;
	u_int num_allow_groups;
	char   **allow_groups;
	u_int num_deny_groups;
	char   **deny_groups;

	u_int num_subsystems;
	char   **subsystem_name;
	char   **subsystem_command;
	char   **subsystem_args;

	u_int num_accept_env;
	char   **accept_env;
	u_int num_setenv;
	char   **setenv;

	int	max_startups_begin;
	int	max_startups_rate;
	int	max_startups;
	int	per_source_max_startups;
	int	per_source_masklen_ipv4;
	int	per_source_masklen_ipv6;
	int	max_authtries;
	int	max_sessions;
	char   *banner;			 
	int	use_dns;
	int	client_alive_interval;	 
	int	client_alive_count_max;	 

	u_int	num_authkeys_files;	 
	char   **authorized_keys_files;

	char   *adm_forced_command;

	int	use_pam;		 

	int	permit_tun;

	char   **permitted_opens;	 
	u_int   num_permitted_opens;
	char   **permitted_listens;  
	u_int   num_permitted_listens;

	char   *chroot_directory;
	char   *revoked_keys_file;
	char   *trusted_user_ca_keys;
	char   *authorized_keys_command;
	char   *authorized_keys_command_user;
	char   *authorized_principals_file;
	char   *authorized_principals_command;
	char   *authorized_principals_command_user;

	int64_t rekey_limit;
	int	rekey_interval;

	char   *version_addendum;	 

	u_int	num_auth_methods;
	char   **auth_methods;

	int	fingerprint_hash;
	int	expose_userauth_info;
	u_int64_t timing_secret;
	char   *sk_provider;
	int	required_rsa_size;	 

	char	**channel_timeouts;	 
	u_int	num_channel_timeouts;

	int	unused_connection_timeout;
}       ServerOptions;

 
struct connection_info {
	const char *user;
	const char *host;	 
	const char *address;	 
	const char *laddress;	 
	int lport;		 
	const char *rdomain;	 
	int test;		 
};

 
struct include_item {
	char *selector;
	char *filename;
	struct sshbuf *contents;
	TAILQ_ENTRY(include_item) entry;
};
TAILQ_HEAD(include_list, include_item);


 
#define COPY_MATCH_STRING_OPTS() do { \
		M_CP_STROPT(banner); \
		M_CP_STROPT(trusted_user_ca_keys); \
		M_CP_STROPT(revoked_keys_file); \
		M_CP_STROPT(authorized_keys_command); \
		M_CP_STROPT(authorized_keys_command_user); \
		M_CP_STROPT(authorized_principals_file); \
		M_CP_STROPT(authorized_principals_command); \
		M_CP_STROPT(authorized_principals_command_user); \
		M_CP_STROPT(hostbased_accepted_algos); \
		M_CP_STROPT(pubkey_accepted_algos); \
		M_CP_STROPT(ca_sign_algorithms); \
		M_CP_STROPT(routing_domain); \
		M_CP_STROPT(permit_user_env_allowlist); \
		M_CP_STRARRAYOPT(authorized_keys_files, num_authkeys_files); \
		M_CP_STRARRAYOPT(allow_users, num_allow_users); \
		M_CP_STRARRAYOPT(deny_users, num_deny_users); \
		M_CP_STRARRAYOPT(allow_groups, num_allow_groups); \
		M_CP_STRARRAYOPT(deny_groups, num_deny_groups); \
		M_CP_STRARRAYOPT(accept_env, num_accept_env); \
		M_CP_STRARRAYOPT(setenv, num_setenv); \
		M_CP_STRARRAYOPT(auth_methods, num_auth_methods); \
		M_CP_STRARRAYOPT(permitted_opens, num_permitted_opens); \
		M_CP_STRARRAYOPT(permitted_listens, num_permitted_listens); \
		M_CP_STRARRAYOPT(channel_timeouts, num_channel_timeouts); \
		M_CP_STRARRAYOPT(log_verbose, num_log_verbose); \
		M_CP_STRARRAYOPT(subsystem_name, num_subsystems); \
		M_CP_STRARRAYOPT(subsystem_command, num_subsystems); \
		M_CP_STRARRAYOPT(subsystem_args, num_subsystems); \
	} while (0)

struct connection_info *get_connection_info(struct ssh *, int, int);
void	 initialize_server_options(ServerOptions *);
void	 fill_default_server_options(ServerOptions *);
int	 process_server_config_line(ServerOptions *, char *, const char *, int,
	    int *, struct connection_info *, struct include_list *includes);
void	 process_permitopen(struct ssh *ssh, ServerOptions *options);
void	 process_channel_timeouts(struct ssh *ssh, ServerOptions *);
void	 load_server_config(const char *, struct sshbuf *);
void	 parse_server_config(ServerOptions *, const char *, struct sshbuf *,
	    struct include_list *includes, struct connection_info *, int);
void	 parse_server_match_config(ServerOptions *,
	    struct include_list *includes, struct connection_info *);
int	 parse_server_match_testspec(struct connection_info *, char *);
int	 server_match_spec_complete(struct connection_info *);
void	 servconf_merge_subsystems(ServerOptions *, ServerOptions *);
void	 copy_set_server_options(ServerOptions *, ServerOptions *, int);
void	 dump_config(ServerOptions *);
char	*derelativise_path(const char *);
void	 servconf_add_hostkey(const char *, const int,
	    ServerOptions *, const char *path, int);
void	 servconf_add_hostcert(const char *, const int,
	    ServerOptions *, const char *path);

#endif				 
