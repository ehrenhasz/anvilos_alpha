#ifndef _FS_NFSD_FILECACHE_H
#define _FS_NFSD_FILECACHE_H

#include <linux/fsnotify_backend.h>

 
struct nfsd_file_mark {
	struct fsnotify_mark	nfm_mark;
	refcount_t		nfm_ref;
};

 
struct nfsd_file {
	struct rhlist_head	nf_rlist;
	void			*nf_inode;
	struct file		*nf_file;
	const struct cred	*nf_cred;
	struct net		*nf_net;
#define NFSD_FILE_HASHED	(0)
#define NFSD_FILE_PENDING	(1)
#define NFSD_FILE_REFERENCED	(2)
#define NFSD_FILE_GC		(3)
	unsigned long		nf_flags;
	refcount_t		nf_ref;
	unsigned char		nf_may;

	struct nfsd_file_mark	*nf_mark;
	struct list_head	nf_lru;
	struct rcu_head		nf_rcu;
	ktime_t			nf_birthtime;
};

int nfsd_file_cache_init(void);
void nfsd_file_cache_purge(struct net *);
void nfsd_file_cache_shutdown(void);
int nfsd_file_cache_start_net(struct net *net);
void nfsd_file_cache_shutdown_net(struct net *net);
void nfsd_file_put(struct nfsd_file *nf);
struct nfsd_file *nfsd_file_get(struct nfsd_file *nf);
void nfsd_file_close_inode_sync(struct inode *inode);
bool nfsd_file_is_cached(struct inode *inode);
__be32 nfsd_file_acquire_gc(struct svc_rqst *rqstp, struct svc_fh *fhp,
		  unsigned int may_flags, struct nfsd_file **nfp);
__be32 nfsd_file_acquire(struct svc_rqst *rqstp, struct svc_fh *fhp,
		  unsigned int may_flags, struct nfsd_file **nfp);
__be32 nfsd_file_acquire_opened(struct svc_rqst *rqstp, struct svc_fh *fhp,
		  unsigned int may_flags, struct file *file,
		  struct nfsd_file **nfp);
int nfsd_file_cache_stats_show(struct seq_file *m, void *v);
#endif  
