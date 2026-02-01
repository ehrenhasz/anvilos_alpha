 
 
#ifndef NFSD_EXPORT_H
#define NFSD_EXPORT_H

#include <linux/sunrpc/cache.h>
#include <linux/percpu_counter.h>
#include <uapi/linux/nfsd/export.h>
#include <linux/nfs4.h>

struct knfsd_fh;
struct svc_fh;
struct svc_rqst;

 

#define MAX_FS_LOCATIONS	128

struct nfsd4_fs_location {
	char *hosts;  
	char *path;   
};

struct nfsd4_fs_locations {
	uint32_t locations_count;
	struct nfsd4_fs_location *locations;
 
	int migrated;
};

 
#define MAX_SECINFO_LIST	8
#define EX_UUID_LEN		16

struct exp_flavor_info {
	u32	pseudoflavor;
	u32	flags;
};

 
enum {
	EXP_STATS_FH_STALE,
	EXP_STATS_IO_READ,
	EXP_STATS_IO_WRITE,
	EXP_STATS_COUNTERS_NUM
};

struct export_stats {
	time64_t		start_time;
	struct percpu_counter	counter[EXP_STATS_COUNTERS_NUM];
};

struct svc_export {
	struct cache_head	h;
	struct auth_domain *	ex_client;
	int			ex_flags;
	struct path		ex_path;
	kuid_t			ex_anon_uid;
	kgid_t			ex_anon_gid;
	int			ex_fsid;
	unsigned char *		ex_uuid;  
	struct nfsd4_fs_locations ex_fslocs;
	uint32_t		ex_nflavors;
	struct exp_flavor_info	ex_flavors[MAX_SECINFO_LIST];
	u32			ex_layout_types;
	struct nfsd4_deviceid_map *ex_devid_map;
	struct cache_detail	*cd;
	struct rcu_head		ex_rcu;
	struct export_stats	ex_stats;
	unsigned long		ex_xprtsec_modes;
};

 
struct svc_expkey {
	struct cache_head	h;

	struct auth_domain *	ek_client;
	int			ek_fsidtype;
	u32			ek_fsid[6];

	struct path		ek_path;
	struct rcu_head		ek_rcu;
};

#define EX_ISSYNC(exp)		(!((exp)->ex_flags & NFSEXP_ASYNC))
#define EX_NOHIDE(exp)		((exp)->ex_flags & NFSEXP_NOHIDE)
#define EX_WGATHER(exp)		((exp)->ex_flags & NFSEXP_GATHERED_WRITES)

int nfsexp_flags(struct svc_rqst *rqstp, struct svc_export *exp);
__be32 check_nfsd_access(struct svc_export *exp, struct svc_rqst *rqstp);

 
int			nfsd_export_init(struct net *);
void			nfsd_export_shutdown(struct net *);
void			nfsd_export_flush(struct net *);
struct svc_export *	rqst_exp_get_by_name(struct svc_rqst *,
					     struct path *);
struct svc_export *	rqst_exp_parent(struct svc_rqst *,
					struct path *);
struct svc_export *	rqst_find_fsidzero_export(struct svc_rqst *);
int			exp_rootfh(struct net *, struct auth_domain *,
					char *path, struct knfsd_fh *, int maxsize);
__be32			exp_pseudoroot(struct svc_rqst *, struct svc_fh *);

static inline void exp_put(struct svc_export *exp)
{
	cache_put(&exp->h, exp->cd);
}

static inline struct svc_export *exp_get(struct svc_export *exp)
{
	cache_get(&exp->h);
	return exp;
}
struct svc_export * rqst_exp_find(struct svc_rqst *, int, u32 *);

#endif  
