
 

#include <linux/sunrpc/svc_xprt.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/sunrpc/addr.h>
#include <linux/highmem.h>
#include <linux/log2.h>
#include <linux/hash.h>
#include <net/checksum.h>

#include "nfsd.h"
#include "cache.h"
#include "trace.h"

 
#define TARGET_BUCKET_SIZE	64

struct nfsd_drc_bucket {
	struct rb_root rb_head;
	struct list_head lru_head;
	spinlock_t cache_lock;
};

static struct kmem_cache	*drc_slab;

static int	nfsd_cache_append(struct svc_rqst *rqstp, struct kvec *vec);
static unsigned long nfsd_reply_cache_count(struct shrinker *shrink,
					    struct shrink_control *sc);
static unsigned long nfsd_reply_cache_scan(struct shrinker *shrink,
					   struct shrink_control *sc);

 
static unsigned int
nfsd_cache_size_limit(void)
{
	unsigned int limit;
	unsigned long low_pages = totalram_pages() - totalhigh_pages();

	limit = (16 * int_sqrt(low_pages)) << (PAGE_SHIFT-10);
	return min_t(unsigned int, limit, 256*1024);
}

 
static unsigned int
nfsd_hashsize(unsigned int limit)
{
	return roundup_pow_of_two(limit / TARGET_BUCKET_SIZE);
}

static struct nfsd_cacherep *
nfsd_cacherep_alloc(struct svc_rqst *rqstp, __wsum csum,
		    struct nfsd_net *nn)
{
	struct nfsd_cacherep *rp;

	rp = kmem_cache_alloc(drc_slab, GFP_KERNEL);
	if (rp) {
		rp->c_state = RC_UNUSED;
		rp->c_type = RC_NOCACHE;
		RB_CLEAR_NODE(&rp->c_node);
		INIT_LIST_HEAD(&rp->c_lru);

		memset(&rp->c_key, 0, sizeof(rp->c_key));
		rp->c_key.k_xid = rqstp->rq_xid;
		rp->c_key.k_proc = rqstp->rq_proc;
		rpc_copy_addr((struct sockaddr *)&rp->c_key.k_addr, svc_addr(rqstp));
		rpc_set_port((struct sockaddr *)&rp->c_key.k_addr, rpc_get_port(svc_addr(rqstp)));
		rp->c_key.k_prot = rqstp->rq_prot;
		rp->c_key.k_vers = rqstp->rq_vers;
		rp->c_key.k_len = rqstp->rq_arg.len;
		rp->c_key.k_csum = csum;
	}
	return rp;
}

static void nfsd_cacherep_free(struct nfsd_cacherep *rp)
{
	if (rp->c_type == RC_REPLBUFF)
		kfree(rp->c_replvec.iov_base);
	kmem_cache_free(drc_slab, rp);
}

static unsigned long
nfsd_cacherep_dispose(struct list_head *dispose)
{
	struct nfsd_cacherep *rp;
	unsigned long freed = 0;

	while (!list_empty(dispose)) {
		rp = list_first_entry(dispose, struct nfsd_cacherep, c_lru);
		list_del(&rp->c_lru);
		nfsd_cacherep_free(rp);
		freed++;
	}
	return freed;
}

static void
nfsd_cacherep_unlink_locked(struct nfsd_net *nn, struct nfsd_drc_bucket *b,
			    struct nfsd_cacherep *rp)
{
	if (rp->c_type == RC_REPLBUFF && rp->c_replvec.iov_base)
		nfsd_stats_drc_mem_usage_sub(nn, rp->c_replvec.iov_len);
	if (rp->c_state != RC_UNUSED) {
		rb_erase(&rp->c_node, &b->rb_head);
		list_del(&rp->c_lru);
		atomic_dec(&nn->num_drc_entries);
		nfsd_stats_drc_mem_usage_sub(nn, sizeof(*rp));
	}
}

static void
nfsd_reply_cache_free_locked(struct nfsd_drc_bucket *b, struct nfsd_cacherep *rp,
				struct nfsd_net *nn)
{
	nfsd_cacherep_unlink_locked(nn, b, rp);
	nfsd_cacherep_free(rp);
}

static void
nfsd_reply_cache_free(struct nfsd_drc_bucket *b, struct nfsd_cacherep *rp,
			struct nfsd_net *nn)
{
	spin_lock(&b->cache_lock);
	nfsd_cacherep_unlink_locked(nn, b, rp);
	spin_unlock(&b->cache_lock);
	nfsd_cacherep_free(rp);
}

int nfsd_drc_slab_create(void)
{
	drc_slab = kmem_cache_create("nfsd_drc",
				sizeof(struct nfsd_cacherep), 0, 0, NULL);
	return drc_slab ? 0: -ENOMEM;
}

void nfsd_drc_slab_free(void)
{
	kmem_cache_destroy(drc_slab);
}

 
int nfsd_net_reply_cache_init(struct nfsd_net *nn)
{
	return nfsd_percpu_counters_init(nn->counter, NFSD_NET_COUNTERS_NUM);
}

 
void nfsd_net_reply_cache_destroy(struct nfsd_net *nn)
{
	nfsd_percpu_counters_destroy(nn->counter, NFSD_NET_COUNTERS_NUM);
}

int nfsd_reply_cache_init(struct nfsd_net *nn)
{
	unsigned int hashsize;
	unsigned int i;
	int status = 0;

	nn->max_drc_entries = nfsd_cache_size_limit();
	atomic_set(&nn->num_drc_entries, 0);
	hashsize = nfsd_hashsize(nn->max_drc_entries);
	nn->maskbits = ilog2(hashsize);

	nn->nfsd_reply_cache_shrinker.scan_objects = nfsd_reply_cache_scan;
	nn->nfsd_reply_cache_shrinker.count_objects = nfsd_reply_cache_count;
	nn->nfsd_reply_cache_shrinker.seeks = 1;
	status = register_shrinker(&nn->nfsd_reply_cache_shrinker,
				   "nfsd-reply:%s", nn->nfsd_name);
	if (status)
		return status;

	nn->drc_hashtbl = kvzalloc(array_size(hashsize,
				sizeof(*nn->drc_hashtbl)), GFP_KERNEL);
	if (!nn->drc_hashtbl)
		goto out_shrinker;

	for (i = 0; i < hashsize; i++) {
		INIT_LIST_HEAD(&nn->drc_hashtbl[i].lru_head);
		spin_lock_init(&nn->drc_hashtbl[i].cache_lock);
	}
	nn->drc_hashsize = hashsize;

	return 0;
out_shrinker:
	unregister_shrinker(&nn->nfsd_reply_cache_shrinker);
	printk(KERN_ERR "nfsd: failed to allocate reply cache\n");
	return -ENOMEM;
}

void nfsd_reply_cache_shutdown(struct nfsd_net *nn)
{
	struct nfsd_cacherep *rp;
	unsigned int i;

	unregister_shrinker(&nn->nfsd_reply_cache_shrinker);

	for (i = 0; i < nn->drc_hashsize; i++) {
		struct list_head *head = &nn->drc_hashtbl[i].lru_head;
		while (!list_empty(head)) {
			rp = list_first_entry(head, struct nfsd_cacherep, c_lru);
			nfsd_reply_cache_free_locked(&nn->drc_hashtbl[i],
									rp, nn);
		}
	}

	kvfree(nn->drc_hashtbl);
	nn->drc_hashtbl = NULL;
	nn->drc_hashsize = 0;

}

 
static void
lru_put_end(struct nfsd_drc_bucket *b, struct nfsd_cacherep *rp)
{
	rp->c_timestamp = jiffies;
	list_move_tail(&rp->c_lru, &b->lru_head);
}

static noinline struct nfsd_drc_bucket *
nfsd_cache_bucket_find(__be32 xid, struct nfsd_net *nn)
{
	unsigned int hash = hash_32((__force u32)xid, nn->maskbits);

	return &nn->drc_hashtbl[hash];
}

 
static void
nfsd_prune_bucket_locked(struct nfsd_net *nn, struct nfsd_drc_bucket *b,
			 unsigned int max, struct list_head *dispose)
{
	unsigned long expiry = jiffies - RC_EXPIRE;
	struct nfsd_cacherep *rp, *tmp;
	unsigned int freed = 0;

	lockdep_assert_held(&b->cache_lock);

	 
	list_for_each_entry_safe(rp, tmp, &b->lru_head, c_lru) {
		 
		if (rp->c_state == RC_INPROG)
			continue;

		if (atomic_read(&nn->num_drc_entries) <= nn->max_drc_entries &&
		    time_before(expiry, rp->c_timestamp))
			break;

		nfsd_cacherep_unlink_locked(nn, b, rp);
		list_add(&rp->c_lru, dispose);

		if (max && ++freed > max)
			break;
	}
}

 
static unsigned long
nfsd_reply_cache_count(struct shrinker *shrink, struct shrink_control *sc)
{
	struct nfsd_net *nn = container_of(shrink,
				struct nfsd_net, nfsd_reply_cache_shrinker);

	return atomic_read(&nn->num_drc_entries);
}

 
static unsigned long
nfsd_reply_cache_scan(struct shrinker *shrink, struct shrink_control *sc)
{
	struct nfsd_net *nn = container_of(shrink,
				struct nfsd_net, nfsd_reply_cache_shrinker);
	unsigned long freed = 0;
	LIST_HEAD(dispose);
	unsigned int i;

	for (i = 0; i < nn->drc_hashsize; i++) {
		struct nfsd_drc_bucket *b = &nn->drc_hashtbl[i];

		if (list_empty(&b->lru_head))
			continue;

		spin_lock(&b->cache_lock);
		nfsd_prune_bucket_locked(nn, b, 0, &dispose);
		spin_unlock(&b->cache_lock);

		freed += nfsd_cacherep_dispose(&dispose);
		if (freed > sc->nr_to_scan)
			break;
	}

	trace_nfsd_drc_gc(nn, freed);
	return freed;
}

 
static __wsum nfsd_cache_csum(struct xdr_buf *buf, unsigned int start,
			      unsigned int remaining)
{
	unsigned int base, len;
	struct xdr_buf subbuf;
	__wsum csum = 0;
	void *p;
	int idx;

	if (remaining > RC_CSUMLEN)
		remaining = RC_CSUMLEN;
	if (xdr_buf_subsegment(buf, &subbuf, start, remaining))
		return csum;

	 
	if (subbuf.head[0].iov_len) {
		len = min_t(unsigned int, subbuf.head[0].iov_len, remaining);
		csum = csum_partial(subbuf.head[0].iov_base, len, csum);
		remaining -= len;
	}

	 
	idx = subbuf.page_base / PAGE_SIZE;
	base = subbuf.page_base & ~PAGE_MASK;
	while (remaining) {
		p = page_address(subbuf.pages[idx]) + base;
		len = min_t(unsigned int, PAGE_SIZE - base, remaining);
		csum = csum_partial(p, len, csum);
		remaining -= len;
		base = 0;
		++idx;
	}
	return csum;
}

static int
nfsd_cache_key_cmp(const struct nfsd_cacherep *key,
		   const struct nfsd_cacherep *rp, struct nfsd_net *nn)
{
	if (key->c_key.k_xid == rp->c_key.k_xid &&
	    key->c_key.k_csum != rp->c_key.k_csum) {
		nfsd_stats_payload_misses_inc(nn);
		trace_nfsd_drc_mismatch(nn, key, rp);
	}

	return memcmp(&key->c_key, &rp->c_key, sizeof(key->c_key));
}

 
static struct nfsd_cacherep *
nfsd_cache_insert(struct nfsd_drc_bucket *b, struct nfsd_cacherep *key,
			struct nfsd_net *nn)
{
	struct nfsd_cacherep	*rp, *ret = key;
	struct rb_node		**p = &b->rb_head.rb_node,
				*parent = NULL;
	unsigned int		entries = 0;
	int cmp;

	while (*p != NULL) {
		++entries;
		parent = *p;
		rp = rb_entry(parent, struct nfsd_cacherep, c_node);

		cmp = nfsd_cache_key_cmp(key, rp, nn);
		if (cmp < 0)
			p = &parent->rb_left;
		else if (cmp > 0)
			p = &parent->rb_right;
		else {
			ret = rp;
			goto out;
		}
	}
	rb_link_node(&key->c_node, parent, p);
	rb_insert_color(&key->c_node, &b->rb_head);
out:
	 
	if (entries > nn->longest_chain) {
		nn->longest_chain = entries;
		nn->longest_chain_cachesize = atomic_read(&nn->num_drc_entries);
	} else if (entries == nn->longest_chain) {
		 
		nn->longest_chain_cachesize = min_t(unsigned int,
				nn->longest_chain_cachesize,
				atomic_read(&nn->num_drc_entries));
	}

	lru_put_end(b, ret);
	return ret;
}

 
int nfsd_cache_lookup(struct svc_rqst *rqstp, unsigned int start,
		      unsigned int len, struct nfsd_cacherep **cacherep)
{
	struct nfsd_net		*nn;
	struct nfsd_cacherep	*rp, *found;
	__wsum			csum;
	struct nfsd_drc_bucket	*b;
	int type = rqstp->rq_cachetype;
	unsigned long freed;
	LIST_HEAD(dispose);
	int rtn = RC_DOIT;

	if (type == RC_NOCACHE) {
		nfsd_stats_rc_nocache_inc();
		goto out;
	}

	csum = nfsd_cache_csum(&rqstp->rq_arg, start, len);

	 
	nn = net_generic(SVC_NET(rqstp), nfsd_net_id);
	rp = nfsd_cacherep_alloc(rqstp, csum, nn);
	if (!rp)
		goto out;

	b = nfsd_cache_bucket_find(rqstp->rq_xid, nn);
	spin_lock(&b->cache_lock);
	found = nfsd_cache_insert(b, rp, nn);
	if (found != rp)
		goto found_entry;
	*cacherep = rp;
	rp->c_state = RC_INPROG;
	nfsd_prune_bucket_locked(nn, b, 3, &dispose);
	spin_unlock(&b->cache_lock);

	freed = nfsd_cacherep_dispose(&dispose);
	trace_nfsd_drc_gc(nn, freed);

	nfsd_stats_rc_misses_inc();
	atomic_inc(&nn->num_drc_entries);
	nfsd_stats_drc_mem_usage_add(nn, sizeof(*rp));
	goto out;

found_entry:
	 
	nfsd_reply_cache_free_locked(NULL, rp, nn);
	nfsd_stats_rc_hits_inc();
	rtn = RC_DROPIT;
	rp = found;

	 
	if (rp->c_state == RC_INPROG)
		goto out_trace;

	 
	rtn = RC_DOIT;
	if (!test_bit(RQ_SECURE, &rqstp->rq_flags) && rp->c_secure)
		goto out_trace;

	 
	switch (rp->c_type) {
	case RC_NOCACHE:
		break;
	case RC_REPLSTAT:
		xdr_stream_encode_be32(&rqstp->rq_res_stream, rp->c_replstat);
		rtn = RC_REPLY;
		break;
	case RC_REPLBUFF:
		if (!nfsd_cache_append(rqstp, &rp->c_replvec))
			goto out_unlock;  
		rtn = RC_REPLY;
		break;
	default:
		WARN_ONCE(1, "nfsd: bad repcache type %d\n", rp->c_type);
	}

out_trace:
	trace_nfsd_drc_found(nn, rqstp, rtn);
out_unlock:
	spin_unlock(&b->cache_lock);
out:
	return rtn;
}

 
void nfsd_cache_update(struct svc_rqst *rqstp, struct nfsd_cacherep *rp,
		       int cachetype, __be32 *statp)
{
	struct nfsd_net *nn = net_generic(SVC_NET(rqstp), nfsd_net_id);
	struct kvec	*resv = &rqstp->rq_res.head[0], *cachv;
	struct nfsd_drc_bucket *b;
	int		len;
	size_t		bufsize = 0;

	if (!rp)
		return;

	b = nfsd_cache_bucket_find(rp->c_key.k_xid, nn);

	len = resv->iov_len - ((char*)statp - (char*)resv->iov_base);
	len >>= 2;

	 
	if (!statp || len > (256 >> 2)) {
		nfsd_reply_cache_free(b, rp, nn);
		return;
	}

	switch (cachetype) {
	case RC_REPLSTAT:
		if (len != 1)
			printk("nfsd: RC_REPLSTAT/reply len %d!\n",len);
		rp->c_replstat = *statp;
		break;
	case RC_REPLBUFF:
		cachv = &rp->c_replvec;
		bufsize = len << 2;
		cachv->iov_base = kmalloc(bufsize, GFP_KERNEL);
		if (!cachv->iov_base) {
			nfsd_reply_cache_free(b, rp, nn);
			return;
		}
		cachv->iov_len = bufsize;
		memcpy(cachv->iov_base, statp, bufsize);
		break;
	case RC_NOCACHE:
		nfsd_reply_cache_free(b, rp, nn);
		return;
	}
	spin_lock(&b->cache_lock);
	nfsd_stats_drc_mem_usage_add(nn, bufsize);
	lru_put_end(b, rp);
	rp->c_secure = test_bit(RQ_SECURE, &rqstp->rq_flags);
	rp->c_type = cachetype;
	rp->c_state = RC_DONE;
	spin_unlock(&b->cache_lock);
	return;
}

static int
nfsd_cache_append(struct svc_rqst *rqstp, struct kvec *data)
{
	__be32 *p;

	p = xdr_reserve_space(&rqstp->rq_res_stream, data->iov_len);
	if (unlikely(!p))
		return false;
	memcpy(p, data->iov_base, data->iov_len);
	xdr_commit_encode(&rqstp->rq_res_stream);
	return true;
}

 
int nfsd_reply_cache_stats_show(struct seq_file *m, void *v)
{
	struct nfsd_net *nn = net_generic(file_inode(m->file)->i_sb->s_fs_info,
					  nfsd_net_id);

	seq_printf(m, "max entries:           %u\n", nn->max_drc_entries);
	seq_printf(m, "num entries:           %u\n",
		   atomic_read(&nn->num_drc_entries));
	seq_printf(m, "hash buckets:          %u\n", 1 << nn->maskbits);
	seq_printf(m, "mem usage:             %lld\n",
		   percpu_counter_sum_positive(&nn->counter[NFSD_NET_DRC_MEM_USAGE]));
	seq_printf(m, "cache hits:            %lld\n",
		   percpu_counter_sum_positive(&nfsdstats.counter[NFSD_STATS_RC_HITS]));
	seq_printf(m, "cache misses:          %lld\n",
		   percpu_counter_sum_positive(&nfsdstats.counter[NFSD_STATS_RC_MISSES]));
	seq_printf(m, "not cached:            %lld\n",
		   percpu_counter_sum_positive(&nfsdstats.counter[NFSD_STATS_RC_NOCACHE]));
	seq_printf(m, "payload misses:        %lld\n",
		   percpu_counter_sum_positive(&nn->counter[NFSD_NET_PAYLOAD_MISSES]));
	seq_printf(m, "longest chain len:     %u\n", nn->longest_chain);
	seq_printf(m, "cachesize at longest:  %u\n", nn->longest_chain_cachesize);
	return 0;
}
