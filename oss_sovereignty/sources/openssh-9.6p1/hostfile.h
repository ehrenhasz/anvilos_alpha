


#ifndef HOSTFILE_H
#define HOSTFILE_H

typedef enum {
	HOST_OK, HOST_NEW, HOST_CHANGED, HOST_REVOKED, HOST_FOUND
}       HostStatus;

typedef enum {
	MRK_ERROR, MRK_NONE, MRK_REVOKE, MRK_CA
}	HostkeyMarker;

struct hostkey_entry {
	char *host;
	char *file;
	u_long line;
	struct sshkey *key;
	HostkeyMarker marker;
	u_int note; 
};
struct hostkeys {
	struct hostkey_entry *entries;
	u_int num_entries;
};

struct hostkeys *init_hostkeys(void);
void	 load_hostkeys(struct hostkeys *, const char *,
    const char *, u_int);
void	 load_hostkeys_file(struct hostkeys *, const char *,
    const char *, FILE *, u_int note);
void	 free_hostkeys(struct hostkeys *);

HostStatus check_key_in_hostkeys(struct hostkeys *, struct sshkey *,
    const struct hostkey_entry **);
int	 lookup_key_in_hostkeys_by_type(struct hostkeys *, int, int,
    const struct hostkey_entry **);
int	 lookup_marker_in_hostkeys(struct hostkeys *, int);

int	 hostfile_read_key(char **, u_int *, struct sshkey *);
int	 add_host_to_hostfile(const char *, const char *,
    const struct sshkey *, int);

int	 hostfile_replace_entries(const char *filename,
    const char *host, const char *ip, struct sshkey **keys, size_t nkeys,
    int store_hash, int quiet, int hash_alg);

#define HASH_MAGIC	"|1|"
#define HASH_DELIM	'|'

#define CA_MARKER	"@cert-authority"
#define REVOKE_MARKER	"@revoked"

char	*host_hash(const char *, const char *, u_int);


#define HKF_WANT_MATCH		(1)	
#define HKF_WANT_PARSE_KEY	(1<<1)	

#define HKF_STATUS_OK		0	
#define HKF_STATUS_INVALID	1	
#define HKF_STATUS_COMMENT	2	
#define HKF_STATUS_MATCHED	3	

#define HKF_MATCH_HOST		(1)	
#define HKF_MATCH_IP		(1<<1)	
#define HKF_MATCH_HOST_HASHED	(1<<2)	
#define HKF_MATCH_IP_HASHED	(1<<3)	



struct hostkey_foreach_line {
	const char *path; 
	u_long linenum;	
	u_int status;	
	u_int match;	
	char *line;	
	int marker;	
	const char *hosts; 
	const char *rawkey; 
	int keytype;	
	struct sshkey *key; 
	const char *comment; 
	u_int note;	
};


typedef int hostkeys_foreach_fn(struct hostkey_foreach_line *l, void *ctx);


int hostkeys_foreach(const char *path,
    hostkeys_foreach_fn *callback, void *ctx,
    const char *host, const char *ip, u_int options, u_int note);
int hostkeys_foreach_file(const char *path, FILE *f,
    hostkeys_foreach_fn *callback, void *ctx,
    const char *host, const char *ip, u_int options, u_int note);

void hostfile_create_user_ssh_dir(const char *, int);

#endif
