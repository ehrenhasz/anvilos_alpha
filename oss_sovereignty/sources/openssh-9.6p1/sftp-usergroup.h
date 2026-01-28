




void get_remote_user_groups_from_glob(struct sftp_conn *conn, glob_t *g);
void get_remote_user_groups_from_dirents(struct sftp_conn *conn, SFTP_DIRENT **d);


const char *ruser_name(uid_t uid);
const char *rgroup_name(uid_t gid);
