 
 

#ifndef __NFSD_NETNS_H__
#define __NFSD_NETNS_H__

#include <net/net_namespace.h>
#include <net/netns/generic.h>
#include <linux/filelock.h>
#include <linux/percpu_counter.h>
#include <linux/siphash.h>

 
#define CLIENT_HASH_BITS                 4
#define CLIENT_HASH_SIZE                (1 << CLIENT_HASH_BITS)
#define CLIENT_HASH_MASK                (CLIENT_HASH_SIZE - 1)

#define SESSION_HASH_SIZE	512

struct cld_net;
struct nfsd4_client_tracking_ops;

enum {
	 
	NFSD_NET_PAYLOAD_MISSES,
	 
	NFSD_NET_DRC_MEM_USAGE,
	NFSD_NET_COUNTERS_NUM
};

 
struct nfsd_net {
	struct cld_net *cld_net;

	struct cache_detail *svc_expkey_cache;
	struct cache_detail *svc_export_cache;

	struct cache_detail *idtoname_cache;
	struct cache_detail *nametoid_cache;

	struct lock_manager nfsd4_manager;
	bool grace_ended;
	time64_t boot_time;

	struct dentry *nfsd_client_dir;

	 
	struct list_head *reclaim_str_hashtbl;
	int reclaim_str_hashtbl_size;
	struct list_head *conf_id_hashtbl;
	struct rb_root conf_name_tree;
	struct list_head *unconf_id_hashtbl;
	struct rb_root unconf_name_tree;
	struct list_head *sessionid_hashtbl;
	 
	struct list_head client_lru;
	struct list_head close_lru;
	struct list_head del_recall_lru;

	 
	struct list_head blocked_locks_lru;

	struct delayed_work laundromat_work;

	 
	spinlock_t client_lock;

	 
	spinlock_t blocked_locks_lock;

	struct file *rec_file;
	bool in_grace;
	const struct nfsd4_client_tracking_ops *client_tracking_ops;

	time64_t nfsd4_lease;
	time64_t nfsd4_grace;
	bool somebody_reclaimed;

	bool track_reclaim_completes;
	atomic_t nr_reclaim_complete;

	bool nfsd_net_up;
	bool lockd_up;

	seqlock_t writeverf_lock;
	unsigned char writeverf[8];

	 
	unsigned int max_connections;

	u32 clientid_base;
	u32 clientid_counter;
	u32 clverifier_counter;

	struct svc_serv *nfsd_serv;
	 
	int keep_active;

	 
	u32		s2s_cp_cl_id;
	struct idr	s2s_cp_stateids;
	spinlock_t	s2s_cp_lock;

	 
	bool *nfsd_versions;
	bool *nfsd4_minorversions;

	 
	struct nfsd_drc_bucket   *drc_hashtbl;

	 
	unsigned int             max_drc_entries;

	 
	unsigned int             maskbits;
	unsigned int             drc_hashsize;

	 

	 
	atomic_t                 num_drc_entries;

	 
	struct percpu_counter    counter[NFSD_NET_COUNTERS_NUM];

	 
	unsigned int             longest_chain;

	 
	unsigned int             longest_chain_cachesize;

	struct shrinker		nfsd_reply_cache_shrinker;

	 
	spinlock_t              nfsd_ssc_lock;
	struct list_head        nfsd_ssc_mount_list;
	wait_queue_head_t       nfsd_ssc_waitq;

	 
	char			nfsd_name[UNX_MAXNODENAME+1];

	struct nfsd_fcache_disposal *fcache_disposal;

	siphash_key_t		siphash_key;

	atomic_t		nfs4_client_count;
	int			nfs4_max_clients;

	atomic_t		nfsd_courtesy_clients;
	struct shrinker		nfsd_client_shrinker;
	struct work_struct	nfsd_shrinker_work;
};

 
#define nfsd_netns_ready(nn) ((nn)->sessionid_hashtbl)

extern void nfsd_netns_free_versions(struct nfsd_net *nn);

extern unsigned int nfsd_net_id;

void nfsd_copy_write_verifier(__be32 verf[2], struct nfsd_net *nn);
void nfsd_reset_write_verifier(struct nfsd_net *nn);
#endif  
