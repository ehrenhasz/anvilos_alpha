 
 

#ifndef NFSCACHE_H
#define NFSCACHE_H

#include <linux/sunrpc/svc.h>
#include "netns.h"

 
struct nfsd_cacherep {
	struct {
		 
		__be32			k_xid;
		__wsum			k_csum;
		u32			k_proc;
		u32			k_prot;
		u32			k_vers;
		unsigned int		k_len;
		struct sockaddr_in6	k_addr;
	} c_key;

	struct rb_node		c_node;
	struct list_head	c_lru;
	unsigned char		c_state,	 
				c_type,		 
				c_secure : 1;	 
	unsigned long		c_timestamp;
	union {
		struct kvec	u_vec;
		__be32		u_status;
	}			c_u;
};

#define c_replvec		c_u.u_vec
#define c_replstat		c_u.u_status

 
enum {
	RC_UNUSED,
	RC_INPROG,
	RC_DONE
};

 
enum {
	RC_DROPIT,
	RC_REPLY,
	RC_DOIT
};

 
enum {
	RC_NOCACHE,
	RC_REPLSTAT,
	RC_REPLBUFF,
};

 
#define RC_EXPIRE		(120 * HZ)

 
#define RC_CSUMLEN		(256U)

int	nfsd_drc_slab_create(void);
void	nfsd_drc_slab_free(void);
int	nfsd_net_reply_cache_init(struct nfsd_net *nn);
void	nfsd_net_reply_cache_destroy(struct nfsd_net *nn);
int	nfsd_reply_cache_init(struct nfsd_net *);
void	nfsd_reply_cache_shutdown(struct nfsd_net *);
int	nfsd_cache_lookup(struct svc_rqst *rqstp, unsigned int start,
			  unsigned int len, struct nfsd_cacherep **cacherep);
void	nfsd_cache_update(struct svc_rqst *rqstp, struct nfsd_cacherep *rp,
			  int cachetype, __be32 *statp);
int	nfsd_reply_cache_stats_show(struct seq_file *m, void *v);

#endif  
