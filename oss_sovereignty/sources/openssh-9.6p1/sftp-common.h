




#define SFTP_MAX_MSG_LENGTH	(256 * 1024)

struct sshbuf;
typedef struct Attrib Attrib;


struct Attrib {
	u_int32_t	flags;
	u_int64_t	size;
	u_int32_t	uid;
	u_int32_t	gid;
	u_int32_t	perm;
	u_int32_t	atime;
	u_int32_t	mtime;
};

void	 attrib_clear(Attrib *);
void	 stat_to_attrib(const struct stat *, Attrib *);
void	 attrib_to_stat(const Attrib *, struct stat *);
int	 decode_attrib(struct sshbuf *, Attrib *);
int	 encode_attrib(struct sshbuf *, const Attrib *);
char	*ls_file(const char *, const struct stat *, int, int,
    const char *, const char *);

const char *fx2txt(int);
