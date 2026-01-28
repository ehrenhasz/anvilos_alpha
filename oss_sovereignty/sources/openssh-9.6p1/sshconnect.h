



typedef struct Sensitive Sensitive;
struct Sensitive {
	struct sshkey	**keys;
	int		nkeys;
};

struct ssh_conn_info {
	char *conn_hash_hex;
	char *shorthost;
	char *uidstr;
	char *keyalias;
	char *thishost;
	char *host_arg;
	char *portstr;
	char *remhost;
	char *remuser;
	char *homedir;
	char *locuser;
	char *jmphost;
};

struct addrinfo;
struct ssh;
struct hostkeys;
struct ssh_conn_info;


#define DEFAULT_CLIENT_PERCENT_EXPAND_ARGS(conn_info) \
	"C", conn_info->conn_hash_hex, \
	"L", conn_info->shorthost, \
	"i", conn_info->uidstr, \
	"k", conn_info->keyalias, \
	"l", conn_info->thishost, \
	"n", conn_info->host_arg, \
	"p", conn_info->portstr, \
	"d", conn_info->homedir, \
	"h", conn_info->remhost, \
	"r", conn_info->remuser, \
	"u", conn_info->locuser, \
	"j", conn_info->jmphost

int	 ssh_connect(struct ssh *, const char *, const char *,
	    struct addrinfo *, struct sockaddr_storage *, u_short,
	    int, int *, int);
void	 ssh_kill_proxy_command(void);

void	 ssh_login(struct ssh *, Sensitive *, const char *,
    struct sockaddr *, u_short, struct passwd *, int,
    const struct ssh_conn_info *);

int	 verify_host_key(char *, struct sockaddr *, struct sshkey *,
    const struct ssh_conn_info *);

void	 get_hostfile_hostname_ipaddr(char *, struct sockaddr *, u_short,
    char **, char **);

void	 ssh_kex2(struct ssh *ssh, char *, struct sockaddr *, u_short,
    const struct ssh_conn_info *);

void	 ssh_userauth2(struct ssh *ssh, const char *, const char *,
    char *, Sensitive *);

int	 ssh_local_cmd(const char *);

void	 maybe_add_key_to_agent(const char *, struct sshkey *,
    const char *, const char *);

void	 load_hostkeys_command(struct hostkeys *, const char *,
    const char *, const struct ssh_conn_info *,
    const struct sshkey *, const char *);
