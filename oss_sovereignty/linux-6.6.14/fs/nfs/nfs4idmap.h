#ifndef NFS_IDMAP_H
#define NFS_IDMAP_H
#include <linux/uidgid.h>
#include <uapi/linux/nfs_idmap.h>
struct nfs_client;
struct nfs_server;
struct nfs_fattr;
struct nfs4_string;
int nfs_idmap_init(void);
void nfs_idmap_quit(void);
int nfs_idmap_new(struct nfs_client *);
void nfs_idmap_delete(struct nfs_client *);
void nfs_fattr_init_names(struct nfs_fattr *fattr,
		struct nfs4_string *owner_name,
		struct nfs4_string *group_name);
void nfs_fattr_free_names(struct nfs_fattr *);
void nfs_fattr_map_and_free_names(struct nfs_server *, struct nfs_fattr *);
int nfs_map_name_to_uid(const struct nfs_server *, const char *, size_t, kuid_t *);
int nfs_map_group_to_gid(const struct nfs_server *, const char *, size_t, kgid_t *);
int nfs_map_uid_to_name(const struct nfs_server *, kuid_t, char *, size_t);
int nfs_map_gid_to_group(const struct nfs_server *, kgid_t, char *, size_t);
int nfs_map_string_to_numeric(const char *name, size_t namelen, __u32 *res);
extern unsigned int nfs_idmap_cache_timeout;
#endif  
