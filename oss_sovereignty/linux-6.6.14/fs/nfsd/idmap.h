#ifndef LINUX_NFSD_IDMAP_H
#define LINUX_NFSD_IDMAP_H
#include <linux/in.h>
#include <linux/sunrpc/svc.h>
#include <linux/nfs_idmap.h>
#ifdef CONFIG_NFSD_V4
int nfsd_idmap_init(struct net *);
void nfsd_idmap_shutdown(struct net *);
#else
static inline int nfsd_idmap_init(struct net *net)
{
	return 0;
}
static inline void nfsd_idmap_shutdown(struct net *net)
{
}
#endif
__be32 nfsd_map_name_to_uid(struct svc_rqst *, const char *, size_t, kuid_t *);
__be32 nfsd_map_name_to_gid(struct svc_rqst *, const char *, size_t, kgid_t *);
__be32 nfsd4_encode_user(struct xdr_stream *, struct svc_rqst *, kuid_t);
__be32 nfsd4_encode_group(struct xdr_stream *, struct svc_rqst *, kgid_t);
#endif  
