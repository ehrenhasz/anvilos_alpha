


#ifndef _LINUX_XSK_QUEUE_H
#define _LINUX_XSK_QUEUE_H

#include <linux/types.h>
#include <linux/if_xdp.h>
#include <net/xdp_sock.h>
#include <net/xsk_buff_pool.h>

#include "xsk.h"

struct xdp_ring {
	u32 producer ____cacheline_aligned_in_smp;
	
	u32 pad1 ____cacheline_aligned_in_smp;
	u32 consumer ____cacheline_aligned_in_smp;
	u32 pad2 ____cacheline_aligned_in_smp;
	u32 flags;
	u32 pad3 ____cacheline_aligned_in_smp;
};


struct xdp_rxtx_ring {
	struct xdp_ring ptrs;
	struct xdp_desc desc[] ____cacheline_aligned_in_smp;
};


struct xdp_umem_ring {
	struct xdp_ring ptrs;
	u64 desc[] ____cacheline_aligned_in_smp;
};

struct xsk_queue {
	u32 ring_mask;
	u32 nentries;
	u32 cached_prod;
	u32 cached_cons;
	struct xdp_ring *ring;
	u64 invalid_descs;
	u64 queue_empty_descs;
	size_t ring_vmalloc_size;
};

struct parsed_desc {
	u32 mb;
	u32 valid;
};







static inline void __xskq_cons_read_addr_unchecked(struct xsk_queue *q, u32 cached_cons, u64 *addr)
{
	struct xdp_umem_ring *ring = (struct xdp_umem_ring *)q->ring;
	u32 idx = cached_cons & q->ring_mask;

	*addr = ring->desc[idx];
}

static inline bool xskq_cons_read_addr_unchecked(struct xsk_queue *q, u64 *addr)
{
	if (q->cached_cons != q->cached_prod) {
		__xskq_cons_read_addr_unchecked(q, q->cached_cons, addr);
		return true;
	}

	return false;
}

static inline bool xp_unused_options_set(u32 options)
{
	return options & ~XDP_PKT_CONTD;
}

static inline bool xp_aligned_validate_desc(struct xsk_buff_pool *pool,
					    struct xdp_desc *desc)
{
	u64 offset = desc->addr & (pool->chunk_size - 1);

	if (!desc->len)
		return false;

	if (offset + desc->len > pool->chunk_size)
		return false;

	if (desc->addr >= pool->addrs_cnt)
		return false;

	if (xp_unused_options_set(desc->options))
		return false;
	return true;
}

static inline bool xp_unaligned_validate_desc(struct xsk_buff_pool *pool,
					      struct xdp_desc *desc)
{
	u64 addr = xp_unaligned_add_offset_to_addr(desc->addr);

	if (!desc->len)
		return false;

	if (desc->len > pool->chunk_size)
		return false;

	if (addr >= pool->addrs_cnt || addr + desc->len > pool->addrs_cnt ||
	    xp_desc_crosses_non_contig_pg(pool, addr, desc->len))
		return false;

	if (xp_unused_options_set(desc->options))
		return false;
	return true;
}

static inline bool xp_validate_desc(struct xsk_buff_pool *pool,
				    struct xdp_desc *desc)
{
	return pool->unaligned ? xp_unaligned_validate_desc(pool, desc) :
		xp_aligned_validate_desc(pool, desc);
}

static inline bool xskq_has_descs(struct xsk_queue *q)
{
	return q->cached_cons != q->cached_prod;
}

static inline bool xskq_cons_is_valid_desc(struct xsk_queue *q,
					   struct xdp_desc *d,
					   struct xsk_buff_pool *pool)
{
	if (!xp_validate_desc(pool, d)) {
		q->invalid_descs++;
		return false;
	}
	return true;
}

static inline bool xskq_cons_read_desc(struct xsk_queue *q,
				       struct xdp_desc *desc,
				       struct xsk_buff_pool *pool)
{
	if (q->cached_cons != q->cached_prod) {
		struct xdp_rxtx_ring *ring = (struct xdp_rxtx_ring *)q->ring;
		u32 idx = q->cached_cons & q->ring_mask;

		*desc = ring->desc[idx];
		return xskq_cons_is_valid_desc(q, desc, pool);
	}

	q->queue_empty_descs++;
	return false;
}

static inline void xskq_cons_release_n(struct xsk_queue *q, u32 cnt)
{
	q->cached_cons += cnt;
}

static inline void parse_desc(struct xsk_queue *q, struct xsk_buff_pool *pool,
			      struct xdp_desc *desc, struct parsed_desc *parsed)
{
	parsed->valid = xskq_cons_is_valid_desc(q, desc, pool);
	parsed->mb = xp_mb_desc(desc);
}

static inline
u32 xskq_cons_read_desc_batch(struct xsk_queue *q, struct xsk_buff_pool *pool,
			      u32 max)
{
	u32 cached_cons = q->cached_cons, nb_entries = 0;
	struct xdp_desc *descs = pool->tx_descs;
	u32 total_descs = 0, nr_frags = 0;

	
	while (cached_cons != q->cached_prod && nb_entries < max) {
		struct xdp_rxtx_ring *ring = (struct xdp_rxtx_ring *)q->ring;
		u32 idx = cached_cons & q->ring_mask;
		struct parsed_desc parsed;

		descs[nb_entries] = ring->desc[idx];
		cached_cons++;
		parse_desc(q, pool, &descs[nb_entries], &parsed);
		if (unlikely(!parsed.valid))
			break;

		if (likely(!parsed.mb)) {
			total_descs += (nr_frags + 1);
			nr_frags = 0;
		} else {
			nr_frags++;
			if (nr_frags == pool->netdev->xdp_zc_max_segs) {
				nr_frags = 0;
				break;
			}
		}
		nb_entries++;
	}

	cached_cons -= nr_frags;
	
	xskq_cons_release_n(q, cached_cons - q->cached_cons);
	return total_descs;
}



static inline void __xskq_cons_release(struct xsk_queue *q)
{
	smp_store_release(&q->ring->consumer, q->cached_cons); 
}

static inline void __xskq_cons_peek(struct xsk_queue *q)
{
	
	q->cached_prod = smp_load_acquire(&q->ring->producer);  
}

static inline void xskq_cons_get_entries(struct xsk_queue *q)
{
	__xskq_cons_release(q);
	__xskq_cons_peek(q);
}

static inline u32 xskq_cons_nb_entries(struct xsk_queue *q, u32 max)
{
	u32 entries = q->cached_prod - q->cached_cons;

	if (entries >= max)
		return max;

	__xskq_cons_peek(q);
	entries = q->cached_prod - q->cached_cons;

	return entries >= max ? max : entries;
}

static inline bool xskq_cons_has_entries(struct xsk_queue *q, u32 cnt)
{
	return xskq_cons_nb_entries(q, cnt) >= cnt;
}

static inline bool xskq_cons_peek_addr_unchecked(struct xsk_queue *q, u64 *addr)
{
	if (q->cached_prod == q->cached_cons)
		xskq_cons_get_entries(q);
	return xskq_cons_read_addr_unchecked(q, addr);
}

static inline bool xskq_cons_peek_desc(struct xsk_queue *q,
				       struct xdp_desc *desc,
				       struct xsk_buff_pool *pool)
{
	if (q->cached_prod == q->cached_cons)
		xskq_cons_get_entries(q);
	return xskq_cons_read_desc(q, desc, pool);
}


static inline void xskq_cons_release(struct xsk_queue *q)
{
	q->cached_cons++;
}

static inline void xskq_cons_cancel_n(struct xsk_queue *q, u32 cnt)
{
	q->cached_cons -= cnt;
}

static inline u32 xskq_cons_present_entries(struct xsk_queue *q)
{
	
	return READ_ONCE(q->ring->producer) - READ_ONCE(q->ring->consumer);
}



static inline u32 xskq_prod_nb_free(struct xsk_queue *q, u32 max)
{
	u32 free_entries = q->nentries - (q->cached_prod - q->cached_cons);

	if (free_entries >= max)
		return max;

	
	q->cached_cons = READ_ONCE(q->ring->consumer);
	free_entries = q->nentries - (q->cached_prod - q->cached_cons);

	return free_entries >= max ? max : free_entries;
}

static inline bool xskq_prod_is_full(struct xsk_queue *q)
{
	return xskq_prod_nb_free(q, 1) ? false : true;
}

static inline void xskq_prod_cancel_n(struct xsk_queue *q, u32 cnt)
{
	q->cached_prod -= cnt;
}

static inline int xskq_prod_reserve(struct xsk_queue *q)
{
	if (xskq_prod_is_full(q))
		return -ENOSPC;

	
	q->cached_prod++;
	return 0;
}

static inline int xskq_prod_reserve_addr(struct xsk_queue *q, u64 addr)
{
	struct xdp_umem_ring *ring = (struct xdp_umem_ring *)q->ring;

	if (xskq_prod_is_full(q))
		return -ENOSPC;

	
	ring->desc[q->cached_prod++ & q->ring_mask] = addr;
	return 0;
}

static inline void xskq_prod_write_addr_batch(struct xsk_queue *q, struct xdp_desc *descs,
					      u32 nb_entries)
{
	struct xdp_umem_ring *ring = (struct xdp_umem_ring *)q->ring;
	u32 i, cached_prod;

	
	cached_prod = q->cached_prod;
	for (i = 0; i < nb_entries; i++)
		ring->desc[cached_prod++ & q->ring_mask] = descs[i].addr;
	q->cached_prod = cached_prod;
}

static inline int xskq_prod_reserve_desc(struct xsk_queue *q,
					 u64 addr, u32 len, u32 flags)
{
	struct xdp_rxtx_ring *ring = (struct xdp_rxtx_ring *)q->ring;
	u32 idx;

	if (xskq_prod_is_full(q))
		return -ENOBUFS;

	
	idx = q->cached_prod++ & q->ring_mask;
	ring->desc[idx].addr = addr;
	ring->desc[idx].len = len;
	ring->desc[idx].options = flags;

	return 0;
}

static inline void __xskq_prod_submit(struct xsk_queue *q, u32 idx)
{
	smp_store_release(&q->ring->producer, idx); 
}

static inline void xskq_prod_submit(struct xsk_queue *q)
{
	__xskq_prod_submit(q, q->cached_prod);
}

static inline void xskq_prod_submit_n(struct xsk_queue *q, u32 nb_entries)
{
	__xskq_prod_submit(q, q->ring->producer + nb_entries);
}

static inline bool xskq_prod_is_empty(struct xsk_queue *q)
{
	
	return READ_ONCE(q->ring->consumer) == READ_ONCE(q->ring->producer);
}



static inline u64 xskq_nb_invalid_descs(struct xsk_queue *q)
{
	return q ? q->invalid_descs : 0;
}

static inline u64 xskq_nb_queue_empty_descs(struct xsk_queue *q)
{
	return q ? q->queue_empty_descs : 0;
}

struct xsk_queue *xskq_create(u32 nentries, bool umem_queue);
void xskq_destroy(struct xsk_queue *q_ops);

#endif 
