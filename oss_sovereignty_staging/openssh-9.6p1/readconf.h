 

 

#ifndef READCONF_H
#define READCONF_H

 

#define SSH_MAX_HOSTS_FILES	32
#define MAX_CANON_DOMAINS	32
#define PATH_MAX_SUN		(sizeof((struct sockaddr_un *)0)->sun_path)

struct allowed_cname {
	char *source_list;
	char *target_list;
};

typedef struct {
	char   *host_arg;	 
	int     forward_agent;	 
	char   *forward_agent_sock_path;  
	int     forward_x11;	 
	int     forward_x11_timeout;	 
	int     forward_x11_trusted;	 
	int     exit_on_forward_failure;	 
	char   *xauth_location;	 
	struct ForwardOptions fwd_opts;	 
	int     pubkey_authentication;	 
	int     hostbased_authentication;	 
	int     gss_authentication;	 
	int     gss_deleg_creds;	 
	int     password_authentication;	 
	int     kbd_interactive_authentication;  
	char	*kbd_interactive_devices;  
	int     batch_mode;	 
	int     check_host_ip;	 
	int     strict_host_key_checking;	 
	int     compression;	 
	int     tcp_keep_alive;	 
	int	ip_qos_interactive;	 
	int	ip_qos_bulk;		 
	SyslogFacility log_facility;	 
	LogLevel log_level;	 
	u_int	num_log_verbose;	 
	char   **log_verbose;
	int     port;		 
	int     address_family;
	int     connection_attempts;	 
	int     connection_timeout;	 
	int     number_of_password_prompts;	 
	char   *ciphers;	 
	char   *macs;		 
	char   *hostkeyalgorithms;	 
	char   *kex_algorithms;	 
	char   *ca_sign_algorithms;	 
	char   *hostname;	 
	char   *tag;		 
	char   *host_key_alias;	 
	char   *proxy_command;	 
	char   *user;		 
	int     escape_char;	 

	u_int	num_system_hostfiles;	 
	char   *system_hostfiles[SSH_MAX_HOSTS_FILES];
	u_int	num_user_hostfiles;	 
	char   *user_hostfiles[SSH_MAX_HOSTS_FILES];
	char   *preferred_authentications;
	char   *bind_address;	 
	char   *bind_interface;	 
	char   *pkcs11_provider;  
	char   *sk_provider;  
	int	verify_host_key_dns;	 

	int     num_identity_files;	 
	char   *identity_files[SSH_MAX_IDENTITY_FILES];
	int    identity_file_userprovided[SSH_MAX_IDENTITY_FILES];
	struct sshkey *identity_keys[SSH_MAX_IDENTITY_FILES];

	int	num_certificate_files;  
	char	*certificate_files[SSH_MAX_CERTIFICATE_FILES];
	int	certificate_file_userprovided[SSH_MAX_CERTIFICATE_FILES];
	struct sshkey *certificates[SSH_MAX_CERTIFICATE_FILES];

	int	add_keys_to_agent;
	int	add_keys_to_agent_lifespan;
	char   *identity_agent;		 

	 
	int     num_local_forwards;
	struct Forward *local_forwards;

	 
	int     num_remote_forwards;
	struct Forward *remote_forwards;
	int	clear_forwardings;

	 
	char  **permitted_remote_opens;
	u_int	num_permitted_remote_opens;

	 
	char   *stdio_forward_host;
	int	stdio_forward_port;

	int	enable_ssh_keysign;
	int64_t rekey_limit;
	int	rekey_interval;
	int	no_host_authentication_for_localhost;
	int	identities_only;
	int	server_alive_interval;
	int	server_alive_count_max;

	u_int	num_send_env;
	char	**send_env;
	u_int	num_setenv;
	char	**setenv;

	char	*control_path;
	int	control_master;
	int     control_persist;  
	int     control_persist_timeout;  

	int	hash_known_hosts;

	int	tun_open;	 
	int     tun_local;	 
	int     tun_remote;	 

	char	*local_command;
	int	permit_local_command;
	char	*remote_command;
	int	visual_host_key;

	int	request_tty;
	int	session_type;
	int	stdin_null;
	int	fork_after_authentication;

	int	proxy_use_fdpass;

	int	num_canonical_domains;
	char	*canonical_domains[MAX_CANON_DOMAINS];
	int	canonicalize_hostname;
	int	canonicalize_max_dots;
	int	canonicalize_fallback_local;
	int	num_permitted_cnames;
	struct allowed_cname permitted_cnames[MAX_CANON_DOMAINS];

	char	*revoked_host_keys;

	int	 fingerprint_hash;

	int	 update_hostkeys;  

	char   *hostbased_accepted_algos;
	char   *pubkey_accepted_algos;

	char   *jump_user;
	char   *jump_host;
	int	jump_port;
	char   *jump_extra;

	char   *known_hosts_command;

	int	required_rsa_size;	 
	int	enable_escape_commandline;	 
	int	obscure_keystroke_timing_interval;

	char	**channel_timeouts;	 
	u_int	num_channel_timeouts;

	char	*ignored_unknown;  
}       Options;

#define SSH_PUBKEY_AUTH_NO	0x00
#define SSH_PUBKEY_AUTH_UNBOUND	0x01
#define SSH_PUBKEY_AUTH_HBOUND	0x02
#define SSH_PUBKEY_AUTH_ALL	0x03

#define SSH_CANONICALISE_NO	0
#define SSH_CANONICALISE_YES	1
#define SSH_CANONICALISE_ALWAYS	2

#define SSHCTL_MASTER_NO	0
#define SSHCTL_MASTER_YES	1
#define SSHCTL_MASTER_AUTO	2
#define SSHCTL_MASTER_ASK	3
#define SSHCTL_MASTER_AUTO_ASK	4

#define REQUEST_TTY_AUTO	0
#define REQUEST_TTY_NO		1
#define REQUEST_TTY_YES		2
#define REQUEST_TTY_FORCE	3

#define SESSION_TYPE_NONE	0
#define SESSION_TYPE_SUBSYSTEM	1
#define SESSION_TYPE_DEFAULT	2

#define SSHCONF_CHECKPERM	1   
#define SSHCONF_USERCONF	2   
#define SSHCONF_FINAL		4   
#define SSHCONF_NEVERMATCH	8   

#define SSH_UPDATE_HOSTKEYS_NO	0
#define SSH_UPDATE_HOSTKEYS_YES	1
#define SSH_UPDATE_HOSTKEYS_ASK	2

#define SSH_STRICT_HOSTKEY_OFF	0
#define SSH_STRICT_HOSTKEY_NEW	1
#define SSH_STRICT_HOSTKEY_YES	2
#define SSH_STRICT_HOSTKEY_ASK	3

 
#define SSH_KEYSTROKE_DEFAULT_INTERVAL_MS	20
#define SSH_KEYSTROKE_CHAFF_MIN_MS		1024
#define SSH_KEYSTROKE_CHAFF_RNG_MS		2048

const char *kex_default_pk_alg(void);
char	*ssh_connection_hash(const char *thishost, const char *host,
    const char *portstr, const char *user, const char *jump_host);
void     initialize_options(Options *);
int      fill_default_options(Options *);
void	 fill_default_options_for_canonicalization(Options *);
void	 free_options(Options *o);
int	 process_config_line(Options *, struct passwd *, const char *,
    const char *, char *, const char *, int, int *, int);
int	 read_config_file(const char *, struct passwd *, const char *,
    const char *, Options *, int, int *);
int	 parse_forward(struct Forward *, const char *, int, int);
int	 parse_jump(const char *, Options *, int);
int	 parse_ssh_uri(const char *, char **, char **, int *);
int	 default_ssh_port(void);
int	 option_clear_or_none(const char *);
int	 config_has_permitted_cnames(Options *);
void	 dump_client_config(Options *o, const char *host);

void	 add_local_forward(Options *, const struct Forward *);
void	 add_remote_forward(Options *, const struct Forward *);
void	 add_identity_file(Options *, const char *, const char *, int);
void	 add_certificate_file(Options *, const char *, int);

#endif				 
