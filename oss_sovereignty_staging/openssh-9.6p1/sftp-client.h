 

 

 

#ifndef _SFTP_CLIENT_H
#define _SFTP_CLIENT_H

#ifdef USE_SYSTEM_GLOB
# include <glob.h>
#else
# include "openbsd-compat/glob.h"
#endif

typedef struct SFTP_DIRENT SFTP_DIRENT;

struct SFTP_DIRENT {
	char *filename;
	char *longname;
	Attrib a;
};

 
struct sftp_statvfs {
	u_int64_t f_bsize;
	u_int64_t f_frsize;
	u_int64_t f_blocks;
	u_int64_t f_bfree;
	u_int64_t f_bavail;
	u_int64_t f_files;
	u_int64_t f_ffree;
	u_int64_t f_favail;
	u_int64_t f_fsid;
	u_int64_t f_flag;
	u_int64_t f_namemax;
};

 
struct sftp_limits {
	u_int64_t packet_length;
	u_int64_t read_length;
	u_int64_t write_length;
	u_int64_t open_handles;
};

 
#define SFTP_QUIET		0	 
#define SFTP_PRINT		1	 
#define SFTP_PROGRESS_ONLY	2	 

 
struct sftp_conn *sftp_init(int, int, u_int, u_int, u_int64_t);

u_int sftp_proto_version(struct sftp_conn *);

 
int sftp_get_limits(struct sftp_conn *, struct sftp_limits *);

 
int sftp_close(struct sftp_conn *, const u_char *, u_int);

 
int sftp_readdir(struct sftp_conn *, const char *, SFTP_DIRENT ***);

 
void sftp_free_dirents(SFTP_DIRENT **);

 
int sftp_rm(struct sftp_conn *, const char *);

 
int sftp_mkdir(struct sftp_conn *, const char *, Attrib *, int);

 
int sftp_rmdir(struct sftp_conn *, const char *);

 
int sftp_stat(struct sftp_conn *, const char *, int, Attrib *);

 
int sftp_lstat(struct sftp_conn *, const char *, int, Attrib *);

 
int sftp_setstat(struct sftp_conn *, const char *, Attrib *);

 
int sftp_fsetstat(struct sftp_conn *, const u_char *, u_int, Attrib *);

 
int sftp_lsetstat(struct sftp_conn *conn, const char *path, Attrib *a);

 
char *sftp_realpath(struct sftp_conn *, const char *);

 
char *sftp_expand_path(struct sftp_conn *, const char *);

 
int sftp_can_expand_path(struct sftp_conn *);

 
int sftp_statvfs(struct sftp_conn *, const char *, struct sftp_statvfs *, int);

 
int sftp_rename(struct sftp_conn *, const char *, const char *, int);

 
int sftp_copy(struct sftp_conn *, const char *, const char *);

 
int sftp_hardlink(struct sftp_conn *, const char *, const char *);

 
int sftp_symlink(struct sftp_conn *, const char *, const char *);

 
int sftp_fsync(struct sftp_conn *conn, u_char *, u_int);

 
int sftp_download(struct sftp_conn *, const char *, const char *, Attrib *,
    int, int, int, int);

 
int sftp_download_dir(struct sftp_conn *, const char *, const char *, Attrib *,
    int, int, int, int, int, int);

 
int sftp_upload(struct sftp_conn *, const char *, const char *,
    int, int, int, int);

 
int sftp_upload_dir(struct sftp_conn *, const char *, const char *,
    int, int, int, int, int, int);

 
int sftp_crossload(struct sftp_conn *from, struct sftp_conn *to,
    const char *from_path, const char *to_path,
    Attrib *a, int preserve_flag);

 
int sftp_crossload_dir(struct sftp_conn *from, struct sftp_conn *to,
    const char *from_path, const char *to_path,
    Attrib *dirattrib, int preserve_flag, int print_flag,
    int follow_link_flag);

 
int sftp_can_get_users_groups_by_id(struct sftp_conn *conn);
int sftp_get_users_groups_by_id(struct sftp_conn *conn,
    const u_int *uids, u_int nuids,
    const u_int *gids, u_int ngids,
    char ***usernamesp, char ***groupnamesp);

 
char *sftp_path_append(const char *, const char *);

 
char *sftp_make_absolute(char *, const char *);

 
int sftp_remote_is_dir(struct sftp_conn *conn, const char *path);

 
int sftp_globpath_is_dir(const char *pathname);

#endif
