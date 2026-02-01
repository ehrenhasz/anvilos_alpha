 

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/dma-mapping.h>
#include <linux/jiffies.h>
#include <linux/prefetch.h>
#include <linux/export.h>
#include <net/xfrm.h>
#include <net/ipv6.h>
#include <net/tcp.h>
#include <net/busy_poll.h>
#ifdef CONFIG_CHELSIO_T4_FCOE
#include <scsi/fc/fc_fcoe.h>
#endif  
#include "cxgb4.h"
#include "t4_regs.h"
#include "t4_values.h"
#include "t4_msg.h"
#include "t4fw_api.h"
#include "cxgb4_ptp.h"
#include "cxgb4_uld.h"
#include "cxgb4_tc_mqprio.h"
#include "sched.h"

 
#if PAGE_SHIFT >= 16
# define FL_PG_ORDER 0
#else
# define FL_PG_ORDER (16 - PAGE_SHIFT)
#endif

 
#define RX_COPY_THRES    256
#define RX_PULL_LEN      128

 
#define RX_PKT_SKB_LEN   512

 
#define MAX_TX_RECLAIM 32

 
#define MAX_RX_REFILL 16U

 
#define RX_QCHECK_PERIOD (HZ / 2)

 
#define TX_QCHECK_PERIOD (HZ / 2)

 
#define MAX_TIMER_TX_RECLAIM 100

 
#define NOMEM_TMR_IDX (SGE_NTIMERS - 1)

 
#define TXQ_STOP_THRES (SGE_MAX_WR_LEN / sizeof(struct tx_desc))

 
#define MAX_IMM_TX_PKT_LEN 256

 
#define MAX_CTRL_WR_LEN SGE_MAX_WR_LEN

struct rx_sw_desc {                 
	struct page *page;
	dma_addr_t dma_addr;
};

 
#define FL_MTU_SMALL 1500
#define FL_MTU_LARGE 9000

static inline unsigned int fl_mtu_bufsize(struct adapter *adapter,
					  unsigned int mtu)
{
	struct sge *s = &adapter->sge;

	return ALIGN(s->pktshift + ETH_HLEN + VLAN_HLEN + mtu, s->fl_align);
}

#define FL_MTU_SMALL_BUFSIZE(adapter) fl_mtu_bufsize(adapter, FL_MTU_SMALL)
#define FL_MTU_LARGE_BUFSIZE(adapter) fl_mtu_bufsize(adapter, FL_MTU_LARGE)

 
enum {
	RX_BUF_FLAGS     = 0x1f,    
	RX_BUF_SIZE      = 0x0f,    
	RX_UNMAPPED_BUF  = 0x10,    

	 
	RX_SMALL_PG_BUF  = 0x0,    
	RX_LARGE_PG_BUF  = 0x1,    

	RX_SMALL_MTU_BUF = 0x2,    
	RX_LARGE_MTU_BUF = 0x3,    
};

static int timer_pkt_quota[] = {1, 1, 2, 3, 4, 5};
#define MIN_NAPI_WORK  1

static inline dma_addr_t get_buf_addr(const struct rx_sw_desc *d)
{
	return d->dma_addr & ~(dma_addr_t)RX_BUF_FLAGS;
}

static inline bool is_buf_mapped(const struct rx_sw_desc *d)
{
	return !(d->dma_addr & RX_UNMAPPED_BUF);
}

 
static inline unsigned int txq_avail(const struct sge_txq *q)
{
	return q->size - 1 - q->in_use;
}

 
static inline unsigned int fl_cap(const struct sge_fl *fl)
{
	return fl->size - 8;    
}

 
static inline bool fl_starving(const struct adapter *adapter,
			       const struct sge_fl *fl)
{
	const struct sge *s = &adapter->sge;

	return fl->avail - fl->pend_cred <= s->fl_starve_thres;
}

int cxgb4_map_skb(struct device *dev, const struct sk_buff *skb,
		  dma_addr_t *addr)
{
	const skb_frag_t *fp, *end;
	const struct skb_shared_info *si;

	*addr = dma_map_single(dev, skb->data, skb_headlen(skb), DMA_TO_DEVICE);
	if (dma_mapping_error(dev, *addr))
		goto out_err;

	si = skb_shinfo(skb);
	end = &si->frags[si->nr_frags];

	for (fp = si->frags; fp < end; fp++) {
		*++addr = skb_frag_dma_map(dev, fp, 0, skb_frag_size(fp),
					   DMA_TO_DEVICE);
		if (dma_mapping_error(dev, *addr))
			goto unwind;
	}
	return 0;

unwind:
	while (fp-- > si->frags)
		dma_unmap_page(dev, *--addr, skb_frag_size(fp), DMA_TO_DEVICE);

	dma_unmap_single(dev, addr[-1], skb_headlen(skb), DMA_TO_DEVICE);
out_err:
	return -ENOMEM;
}
EXPORT_SYMBOL(cxgb4_map_skb);

static void unmap_skb(struct device *dev, const struct sk_buff *skb,
		      const dma_addr_t *addr)
{
	const skb_frag_t *fp, *end;
	const struct skb_shared_info *si;

	dma_unmap_single(dev, *addr++, skb_headlen(skb), DMA_TO_DEVICE);

	si = skb_shinfo(skb);
	end = &si->frags[si->nr_frags];
	for (fp = si->frags; fp < end; fp++)
		dma_unmap_page(dev, *addr++, skb_frag_size(fp), DMA_TO_DEVICE);
}

#ifdef CONFIG_NEED_DMA_MAP_STATE
 
static void deferred_unmap_destructor(struct sk_buff *skb)
{
	unmap_skb(skb->dev->dev.parent, skb, (dma_addr_t *)skb->head);
}
#endif

 
void free_tx_desc(struct adapter *adap, struct sge_txq *q,
		  unsigned int n, bool unmap)
{
	unsigned int cidx = q->cidx;
	struct tx_sw_desc *d;

	d = &q->sdesc[cidx];
	while (n--) {
		if (d->skb) {                        
			if (unmap && d->addr[0]) {
				unmap_skb(adap->pdev_dev, d->skb, d->addr);
				memset(d->addr, 0, sizeof(d->addr));
			}
			dev_consume_skb_any(d->skb);
			d->skb = NULL;
		}
		++d;
		if (++cidx == q->size) {
			cidx = 0;
			d = q->sdesc;
		}
	}
	q->cidx = cidx;
}

 
static inline int reclaimable(const struct sge_txq *q)
{
	int hw_cidx = ntohs(READ_ONCE(q->stat->cidx));
	hw_cidx -= q->cidx;
	return hw_cidx < 0 ? hw_cidx + q->size : hw_cidx;
}

 
static inline int reclaim_completed_tx(struct adapter *adap, struct sge_txq *q,
				       int maxreclaim, bool unmap)
{
	int reclaim = reclaimable(q);

	if (reclaim) {
		 
		if (maxreclaim < 0)
			maxreclaim = MAX_TX_RECLAIM;
		if (reclaim > maxreclaim)
			reclaim = maxreclaim;

		free_tx_desc(adap, q, reclaim, unmap);
		q->in_use -= reclaim;
	}

	return reclaim;
}

 
void cxgb4_reclaim_completed_tx(struct adapter *adap, struct sge_txq *q,
				bool unmap)
{
	(void)reclaim_completed_tx(adap, q, -1, unmap);
}
EXPORT_SYMBOL(cxgb4_reclaim_completed_tx);

static inline int get_buf_size(struct adapter *adapter,
			       const struct rx_sw_desc *d)
{
	struct sge *s = &adapter->sge;
	unsigned int rx_buf_size_idx = d->dma_addr & RX_BUF_SIZE;
	int buf_size;

	switch (rx_buf_size_idx) {
	case RX_SMALL_PG_BUF:
		buf_size = PAGE_SIZE;
		break;

	case RX_LARGE_PG_BUF:
		buf_size = PAGE_SIZE << s->fl_pg_order;
		break;

	case RX_SMALL_MTU_BUF:
		buf_size = FL_MTU_SMALL_BUFSIZE(adapter);
		break;

	case RX_LARGE_MTU_BUF:
		buf_size = FL_MTU_LARGE_BUFSIZE(adapter);
		break;

	default:
		BUG();
	}

	return buf_size;
}

 
static void free_rx_bufs(struct adapter *adap, struct sge_fl *q, int n)
{
	while (n--) {
		struct rx_sw_desc *d = &q->sdesc[q->cidx];

		if (is_buf_mapped(d))
			dma_unmap_page(adap->pdev_dev, get_buf_addr(d),
				       get_buf_size(adap, d),
				       DMA_FROM_DEVICE);
		put_page(d->page);
		d->page = NULL;
		if (++q->cidx == q->size)
			q->cidx = 0;
		q->avail--;
	}
}

 
static void unmap_rx_buf(struct adapter *adap, struct sge_fl *q)
{
	struct rx_sw_desc *d = &q->sdesc[q->cidx];

	if (is_buf_mapped(d))
		dma_unmap_page(adap->pdev_dev, get_buf_addr(d),
			       get_buf_size(adap, d), DMA_FROM_DEVICE);
	d->page = NULL;
	if (++q->cidx == q->size)
		q->cidx = 0;
	q->avail--;
}

static inline void ring_fl_db(struct adapter *adap, struct sge_fl *q)
{
	if (q->pend_cred >= 8) {
		u32 val = adap->params.arch.sge_fl_db;

		if (is_t4(adap->params.chip))
			val |= PIDX_V(q->pend_cred / 8);
		else
			val |= PIDX_T5_V(q->pend_cred / 8);

		 
		wmb();

		 
		if (unlikely(q->bar2_addr == NULL)) {
			t4_write_reg(adap, MYPF_REG(SGE_PF_KDOORBELL_A),
				     val | QID_V(q->cntxt_id));
		} else {
			writel(val | QID_V(q->bar2_qid),
			       q->bar2_addr + SGE_UDB_KDOORBELL);

			 
			wmb();
		}
		q->pend_cred &= 7;
	}
}

static inline void set_rx_sw_desc(struct rx_sw_desc *sd, struct page *pg,
				  dma_addr_t mapping)
{
	sd->page = pg;
	sd->dma_addr = mapping;       
}

 
static unsigned int refill_fl(struct adapter *adap, struct sge_fl *q, int n,
			      gfp_t gfp)
{
	struct sge *s = &adap->sge;
	struct page *pg;
	dma_addr_t mapping;
	unsigned int cred = q->avail;
	__be64 *d = &q->desc[q->pidx];
	struct rx_sw_desc *sd = &q->sdesc[q->pidx];
	int node;

#ifdef CONFIG_DEBUG_FS
	if (test_bit(q->cntxt_id - adap->sge.egr_start, adap->sge.blocked_fl))
		goto out;
#endif

	gfp |= __GFP_NOWARN;
	node = dev_to_node(adap->pdev_dev);

	if (s->fl_pg_order == 0)
		goto alloc_small_pages;

	 
	while (n) {
		pg = alloc_pages_node(node, gfp | __GFP_COMP, s->fl_pg_order);
		if (unlikely(!pg)) {
			q->large_alloc_failed++;
			break;        
		}

		mapping = dma_map_page(adap->pdev_dev, pg, 0,
				       PAGE_SIZE << s->fl_pg_order,
				       DMA_FROM_DEVICE);
		if (unlikely(dma_mapping_error(adap->pdev_dev, mapping))) {
			__free_pages(pg, s->fl_pg_order);
			q->mapping_err++;
			goto out;    
		}
		mapping |= RX_LARGE_PG_BUF;
		*d++ = cpu_to_be64(mapping);

		set_rx_sw_desc(sd, pg, mapping);
		sd++;

		q->avail++;
		if (++q->pidx == q->size) {
			q->pidx = 0;
			sd = q->sdesc;
			d = q->desc;
		}
		n--;
	}

alloc_small_pages:
	while (n--) {
		pg = alloc_pages_node(node, gfp, 0);
		if (unlikely(!pg)) {
			q->alloc_failed++;
			break;
		}

		mapping = dma_map_page(adap->pdev_dev, pg, 0, PAGE_SIZE,
				       DMA_FROM_DEVICE);
		if (unlikely(dma_mapping_error(adap->pdev_dev, mapping))) {
			put_page(pg);
			q->mapping_err++;
			goto out;
		}
		*d++ = cpu_to_be64(mapping);

		set_rx_sw_desc(sd, pg, mapping);
		sd++;

		q->avail++;
		if (++q->pidx == q->size) {
			q->pidx = 0;
			sd = q->sdesc;
			d = q->desc;
		}
	}

out:	cred = q->avail - cred;
	q->pend_cred += cred;
	ring_fl_db(adap, q);

	if (unlikely(fl_starving(adap, q))) {
		smp_wmb();
		q->low++;
		set_bit(q->cntxt_id - adap->sge.egr_start,
			adap->sge.starving_fl);
	}

	return cred;
}

static inline void __refill_fl(struct adapter *adap, struct sge_fl *fl)
{
	refill_fl(adap, fl, min(MAX_RX_REFILL, fl_cap(fl) - fl->avail),
		  GFP_ATOMIC);
}

 
static void *alloc_ring(struct device *dev, size_t nelem, size_t elem_size,
			size_t sw_size, dma_addr_t *phys, void *metadata,
			size_t stat_size, int node)
{
	size_t len = nelem * elem_size + stat_size;
	void *s = NULL;
	void *p = dma_alloc_coherent(dev, len, phys, GFP_KERNEL);

	if (!p)
		return NULL;
	if (sw_size) {
		s = kcalloc_node(sw_size, nelem, GFP_KERNEL, node);

		if (!s) {
			dma_free_coherent(dev, len, p, *phys);
			return NULL;
		}
	}
	if (metadata)
		*(void **)metadata = s;
	return p;
}

 
static inline unsigned int sgl_len(unsigned int n)
{
	 
	n--;
	return (3 * n) / 2 + (n & 1) + 2;
}

 
static inline unsigned int flits_to_desc(unsigned int n)
{
	BUG_ON(n > SGE_MAX_WR_LEN / 8);
	return DIV_ROUND_UP(n, 8);
}

 
static inline int is_eth_imm(const struct sk_buff *skb, unsigned int chip_ver)
{
	int hdrlen = 0;

	if (skb->encapsulation && skb_shinfo(skb)->gso_size &&
	    chip_ver > CHELSIO_T5) {
		hdrlen = sizeof(struct cpl_tx_tnl_lso);
		hdrlen += sizeof(struct cpl_tx_pkt_core);
	} else if (skb_shinfo(skb)->gso_type & SKB_GSO_UDP_L4) {
		return 0;
	} else {
		hdrlen = skb_shinfo(skb)->gso_size ?
			 sizeof(struct cpl_tx_pkt_lso_core) : 0;
		hdrlen += sizeof(struct cpl_tx_pkt);
	}
	if (skb->len <= MAX_IMM_TX_PKT_LEN - hdrlen)
		return hdrlen;
	return 0;
}

 
static inline unsigned int calc_tx_flits(const struct sk_buff *skb,
					 unsigned int chip_ver)
{
	unsigned int flits;
	int hdrlen = is_eth_imm(skb, chip_ver);

	 

	if (hdrlen)
		return DIV_ROUND_UP(skb->len + hdrlen, sizeof(__be64));

	 
	flits = sgl_len(skb_shinfo(skb)->nr_frags + 1);
	if (skb_shinfo(skb)->gso_size) {
		if (skb->encapsulation && chip_ver > CHELSIO_T5) {
			hdrlen = sizeof(struct fw_eth_tx_pkt_wr) +
				 sizeof(struct cpl_tx_tnl_lso);
		} else if (skb_shinfo(skb)->gso_type & SKB_GSO_UDP_L4) {
			u32 pkt_hdrlen;

			pkt_hdrlen = eth_get_headlen(skb->dev, skb->data,
						     skb_headlen(skb));
			hdrlen = sizeof(struct fw_eth_tx_eo_wr) +
				 round_up(pkt_hdrlen, 16);
		} else {
			hdrlen = sizeof(struct fw_eth_tx_pkt_wr) +
				 sizeof(struct cpl_tx_pkt_lso_core);
		}

		hdrlen += sizeof(struct cpl_tx_pkt_core);
		flits += (hdrlen / sizeof(__be64));
	} else {
		flits += (sizeof(struct fw_eth_tx_pkt_wr) +
			  sizeof(struct cpl_tx_pkt_core)) / sizeof(__be64);
	}
	return flits;
}

 
static inline unsigned int calc_tx_descs(const struct sk_buff *skb,
					 unsigned int chip_ver)
{
	return flits_to_desc(calc_tx_flits(skb, chip_ver));
}

 
void cxgb4_write_sgl(const struct sk_buff *skb, struct sge_txq *q,
		     struct ulptx_sgl *sgl, u64 *end, unsigned int start,
		     const dma_addr_t *addr)
{
	unsigned int i, len;
	struct ulptx_sge_pair *to;
	const struct skb_shared_info *si = skb_shinfo(skb);
	unsigned int nfrags = si->nr_frags;
	struct ulptx_sge_pair buf[MAX_SKB_FRAGS / 2 + 1];

	len = skb_headlen(skb) - start;
	if (likely(len)) {
		sgl->len0 = htonl(len);
		sgl->addr0 = cpu_to_be64(addr[0] + start);
		nfrags++;
	} else {
		sgl->len0 = htonl(skb_frag_size(&si->frags[0]));
		sgl->addr0 = cpu_to_be64(addr[1]);
	}

	sgl->cmd_nsge = htonl(ULPTX_CMD_V(ULP_TX_SC_DSGL) |
			      ULPTX_NSGE_V(nfrags));
	if (likely(--nfrags == 0))
		return;
	 
	to = (u8 *)end > (u8 *)q->stat ? buf : sgl->sge;

	for (i = (nfrags != si->nr_frags); nfrags >= 2; nfrags -= 2, to++) {
		to->len[0] = cpu_to_be32(skb_frag_size(&si->frags[i]));
		to->len[1] = cpu_to_be32(skb_frag_size(&si->frags[++i]));
		to->addr[0] = cpu_to_be64(addr[i]);
		to->addr[1] = cpu_to_be64(addr[++i]);
	}
	if (nfrags) {
		to->len[0] = cpu_to_be32(skb_frag_size(&si->frags[i]));
		to->len[1] = cpu_to_be32(0);
		to->addr[0] = cpu_to_be64(addr[i + 1]);
	}
	if (unlikely((u8 *)end > (u8 *)q->stat)) {
		unsigned int part0 = (u8 *)q->stat - (u8 *)sgl->sge, part1;

		if (likely(part0))
			memcpy(sgl->sge, buf, part0);
		part1 = (u8 *)end - (u8 *)q->stat;
		memcpy(q->desc, (u8 *)buf + part0, part1);
		end = (void *)q->desc + part1;
	}
	if ((uintptr_t)end & 8)            
		*end = 0;
}
EXPORT_SYMBOL(cxgb4_write_sgl);

 
void cxgb4_write_partial_sgl(const struct sk_buff *skb, struct sge_txq *q,
			     struct ulptx_sgl *sgl, u64 *end,
			     const dma_addr_t *addr, u32 start, u32 len)
{
	struct ulptx_sge_pair buf[MAX_SKB_FRAGS / 2 + 1] = {0}, *to;
	u32 frag_size, skb_linear_data_len = skb_headlen(skb);
	struct skb_shared_info *si = skb_shinfo(skb);
	u8 i = 0, frag_idx = 0, nfrags = 0;
	skb_frag_t *frag;

	 
	if (unlikely(start < skb_linear_data_len)) {
		frag_size = min(len, skb_linear_data_len - start);
		sgl->len0 = htonl(frag_size);
		sgl->addr0 = cpu_to_be64(addr[0] + start);
		len -= frag_size;
		nfrags++;
	} else {
		start -= skb_linear_data_len;
		frag = &si->frags[frag_idx];
		frag_size = skb_frag_size(frag);
		 
		while (start >= frag_size) {
			start -= frag_size;
			frag_idx++;
			frag = &si->frags[frag_idx];
			frag_size = skb_frag_size(frag);
		}

		frag_size = min(len, skb_frag_size(frag) - start);
		sgl->len0 = cpu_to_be32(frag_size);
		sgl->addr0 = cpu_to_be64(addr[frag_idx + 1] + start);
		len -= frag_size;
		nfrags++;
		frag_idx++;
	}

	 
	if (!len)
		goto done;

	 
	to = (u8 *)end > (u8 *)q->stat ? buf : sgl->sge;

	 
	while (len) {
		frag_size = min(len, skb_frag_size(&si->frags[frag_idx]));
		to->len[i & 1] = cpu_to_be32(frag_size);
		to->addr[i & 1] = cpu_to_be64(addr[frag_idx + 1]);
		if (i && (i & 1))
			to++;
		nfrags++;
		frag_idx++;
		i++;
		len -= frag_size;
	}

	 
	if (i & 1)
		to->len[1] = cpu_to_be32(0);

	 
	if (unlikely((u8 *)end > (u8 *)q->stat)) {
		u32 part0 = (u8 *)q->stat - (u8 *)sgl->sge, part1;

		if (likely(part0))
			memcpy(sgl->sge, buf, part0);
		part1 = (u8 *)end - (u8 *)q->stat;
		memcpy(q->desc, (u8 *)buf + part0, part1);
		end = (void *)q->desc + part1;
	}

	 
	if ((uintptr_t)end & 8)
		*end = 0;
done:
	sgl->cmd_nsge = htonl(ULPTX_CMD_V(ULP_TX_SC_DSGL) |
			ULPTX_NSGE_V(nfrags));
}
EXPORT_SYMBOL(cxgb4_write_partial_sgl);

 
static void cxgb_pio_copy(u64 __iomem *dst, u64 *src)
{
	int count = 8;

	while (count) {
		writeq(*src, dst);
		src++;
		dst++;
		count--;
	}
}

 
inline void cxgb4_ring_tx_db(struct adapter *adap, struct sge_txq *q, int n)
{
	 
	wmb();

	 
	if (unlikely(q->bar2_addr == NULL)) {
		u32 val = PIDX_V(n);
		unsigned long flags;

		 
		spin_lock_irqsave(&q->db_lock, flags);
		if (!q->db_disabled)
			t4_write_reg(adap, MYPF_REG(SGE_PF_KDOORBELL_A),
				     QID_V(q->cntxt_id) | val);
		else
			q->db_pidx_inc += n;
		q->db_pidx = q->pidx;
		spin_unlock_irqrestore(&q->db_lock, flags);
	} else {
		u32 val = PIDX_T5_V(n);

		 
		WARN_ON(val & DBPRIO_F);

		 
		if (n == 1 && q->bar2_qid == 0) {
			int index = (q->pidx
				     ? (q->pidx - 1)
				     : (q->size - 1));
			u64 *wr = (u64 *)&q->desc[index];

			cxgb_pio_copy((u64 __iomem *)
				      (q->bar2_addr + SGE_UDB_WCDOORBELL),
				      wr);
		} else {
			writel(val | QID_V(q->bar2_qid),
			       q->bar2_addr + SGE_UDB_KDOORBELL);
		}

		 
		wmb();
	}
}
EXPORT_SYMBOL(cxgb4_ring_tx_db);

 
void cxgb4_inline_tx_skb(const struct sk_buff *skb,
			 const struct sge_txq *q, void *pos)
{
	int left = (void *)q->stat - pos;
	u64 *p;

	if (likely(skb->len <= left)) {
		if (likely(!skb->data_len))
			skb_copy_from_linear_data(skb, pos, skb->len);
		else
			skb_copy_bits(skb, 0, pos, skb->len);
		pos += skb->len;
	} else {
		skb_copy_bits(skb, 0, pos, left);
		skb_copy_bits(skb, left, q->desc, skb->len - left);
		pos = (void *)q->desc + (skb->len - left);
	}

	 
	p = PTR_ALIGN(pos, 8);
	if ((uintptr_t)p & 8)
		*p = 0;
}
EXPORT_SYMBOL(cxgb4_inline_tx_skb);

static void *inline_tx_skb_header(const struct sk_buff *skb,
				  const struct sge_txq *q,  void *pos,
				  int length)
{
	u64 *p;
	int left = (void *)q->stat - pos;

	if (likely(length <= left)) {
		memcpy(pos, skb->data, length);
		pos += length;
	} else {
		memcpy(pos, skb->data, left);
		memcpy(q->desc, skb->data + left, length - left);
		pos = (void *)q->desc + (length - left);
	}
	 
	p = PTR_ALIGN(pos, 8);
	if ((uintptr_t)p & 8) {
		*p = 0;
		return p + 1;
	}
	return p;
}

 
static u64 hwcsum(enum chip_type chip, const struct sk_buff *skb)
{
	int csum_type;
	bool inner_hdr_csum = false;
	u16 proto, ver;

	if (skb->encapsulation &&
	    (CHELSIO_CHIP_VERSION(chip) > CHELSIO_T5))
		inner_hdr_csum = true;

	if (inner_hdr_csum) {
		ver = inner_ip_hdr(skb)->version;
		proto = (ver == 4) ? inner_ip_hdr(skb)->protocol :
			inner_ipv6_hdr(skb)->nexthdr;
	} else {
		ver = ip_hdr(skb)->version;
		proto = (ver == 4) ? ip_hdr(skb)->protocol :
			ipv6_hdr(skb)->nexthdr;
	}

	if (ver == 4) {
		if (proto == IPPROTO_TCP)
			csum_type = TX_CSUM_TCPIP;
		else if (proto == IPPROTO_UDP)
			csum_type = TX_CSUM_UDPIP;
		else {
nocsum:			 
			return TXPKT_L4CSUM_DIS_F;
		}
	} else {
		 
		if (proto == IPPROTO_TCP)
			csum_type = TX_CSUM_TCPIP6;
		else if (proto == IPPROTO_UDP)
			csum_type = TX_CSUM_UDPIP6;
		else
			goto nocsum;
	}

	if (likely(csum_type >= TX_CSUM_TCPIP)) {
		int eth_hdr_len, l4_len;
		u64 hdr_len;

		if (inner_hdr_csum) {
			 
			l4_len = skb_inner_network_header_len(skb);
			eth_hdr_len = skb_inner_network_offset(skb) - ETH_HLEN;
		} else {
			l4_len = skb_network_header_len(skb);
			eth_hdr_len = skb_network_offset(skb) - ETH_HLEN;
		}
		hdr_len = TXPKT_IPHDR_LEN_V(l4_len);

		if (CHELSIO_CHIP_VERSION(chip) <= CHELSIO_T5)
			hdr_len |= TXPKT_ETHHDR_LEN_V(eth_hdr_len);
		else
			hdr_len |= T6_TXPKT_ETHHDR_LEN_V(eth_hdr_len);
		return TXPKT_CSUM_TYPE_V(csum_type) | hdr_len;
	} else {
		int start = skb_transport_offset(skb);

		return TXPKT_CSUM_TYPE_V(csum_type) |
			TXPKT_CSUM_START_V(start) |
			TXPKT_CSUM_LOC_V(start + skb->csum_offset);
	}
}

static void eth_txq_stop(struct sge_eth_txq *q)
{
	netif_tx_stop_queue(q->txq);
	q->q.stops++;
}

static inline void txq_advance(struct sge_txq *q, unsigned int n)
{
	q->in_use += n;
	q->pidx += n;
	if (q->pidx >= q->size)
		q->pidx -= q->size;
}

#ifdef CONFIG_CHELSIO_T4_FCOE
static inline int
cxgb_fcoe_offload(struct sk_buff *skb, struct adapter *adap,
		  const struct port_info *pi, u64 *cntrl)
{
	const struct cxgb_fcoe *fcoe = &pi->fcoe;

	if (!(fcoe->flags & CXGB_FCOE_ENABLED))
		return 0;

	if (skb->protocol != htons(ETH_P_FCOE))
		return 0;

	skb_reset_mac_header(skb);
	skb->mac_len = sizeof(struct ethhdr);

	skb_set_network_header(skb, skb->mac_len);
	skb_set_transport_header(skb, skb->mac_len + sizeof(struct fcoe_hdr));

	if (!cxgb_fcoe_sof_eof_supported(adap, skb))
		return -ENOTSUPP;

	 
	*cntrl = TXPKT_CSUM_TYPE_V(TX_CSUM_FCOE) |
		     TXPKT_L4CSUM_DIS_F | TXPKT_IPCSUM_DIS_F |
		     TXPKT_CSUM_START_V(CXGB_FCOE_TXPKT_CSUM_START) |
		     TXPKT_CSUM_END_V(CXGB_FCOE_TXPKT_CSUM_END) |
		     TXPKT_CSUM_LOC_V(CXGB_FCOE_TXPKT_CSUM_END);
	return 0;
}
#endif  

 
enum cpl_tx_tnl_lso_type cxgb_encap_offload_supported(struct sk_buff *skb)
{
	u8 l4_hdr = 0;
	enum cpl_tx_tnl_lso_type tnl_type = TX_TNL_TYPE_OPAQUE;
	struct port_info *pi = netdev_priv(skb->dev);
	struct adapter *adapter = pi->adapter;

	if (skb->inner_protocol_type != ENCAP_TYPE_ETHER ||
	    skb->inner_protocol != htons(ETH_P_TEB))
		return tnl_type;

	switch (vlan_get_protocol(skb)) {
	case htons(ETH_P_IP):
		l4_hdr = ip_hdr(skb)->protocol;
		break;
	case htons(ETH_P_IPV6):
		l4_hdr = ipv6_hdr(skb)->nexthdr;
		break;
	default:
		return tnl_type;
	}

	switch (l4_hdr) {
	case IPPROTO_UDP:
		if (adapter->vxlan_port == udp_hdr(skb)->dest)
			tnl_type = TX_TNL_TYPE_VXLAN;
		else if (adapter->geneve_port == udp_hdr(skb)->dest)
			tnl_type = TX_TNL_TYPE_GENEVE;
		break;
	default:
		return tnl_type;
	}

	return tnl_type;
}

static inline void t6_fill_tnl_lso(struct sk_buff *skb,
				   struct cpl_tx_tnl_lso *tnl_lso,
				   enum cpl_tx_tnl_lso_type tnl_type)
{
	u32 val;
	int in_eth_xtra_len;
	int l3hdr_len = skb_network_header_len(skb);
	int eth_xtra_len = skb_network_offset(skb) - ETH_HLEN;
	const struct skb_shared_info *ssi = skb_shinfo(skb);
	bool v6 = (ip_hdr(skb)->version == 6);

	val = CPL_TX_TNL_LSO_OPCODE_V(CPL_TX_TNL_LSO) |
	      CPL_TX_TNL_LSO_FIRST_F |
	      CPL_TX_TNL_LSO_LAST_F |
	      (v6 ? CPL_TX_TNL_LSO_IPV6OUT_F : 0) |
	      CPL_TX_TNL_LSO_ETHHDRLENOUT_V(eth_xtra_len / 4) |
	      CPL_TX_TNL_LSO_IPHDRLENOUT_V(l3hdr_len / 4) |
	      (v6 ? 0 : CPL_TX_TNL_LSO_IPHDRCHKOUT_F) |
	      CPL_TX_TNL_LSO_IPLENSETOUT_F |
	      (v6 ? 0 : CPL_TX_TNL_LSO_IPIDINCOUT_F);
	tnl_lso->op_to_IpIdSplitOut = htonl(val);

	tnl_lso->IpIdOffsetOut = 0;

	 
	val = skb_inner_mac_header(skb) - skb_mac_header(skb);
	in_eth_xtra_len = skb_inner_network_header(skb) -
			  skb_inner_mac_header(skb) - ETH_HLEN;

	switch (tnl_type) {
	case TX_TNL_TYPE_VXLAN:
	case TX_TNL_TYPE_GENEVE:
		tnl_lso->UdpLenSetOut_to_TnlHdrLen =
			htons(CPL_TX_TNL_LSO_UDPCHKCLROUT_F |
			CPL_TX_TNL_LSO_UDPLENSETOUT_F);
		break;
	default:
		tnl_lso->UdpLenSetOut_to_TnlHdrLen = 0;
		break;
	}

	tnl_lso->UdpLenSetOut_to_TnlHdrLen |=
		 htons(CPL_TX_TNL_LSO_TNLHDRLEN_V(val) |
		       CPL_TX_TNL_LSO_TNLTYPE_V(tnl_type));

	tnl_lso->r1 = 0;

	val = CPL_TX_TNL_LSO_ETHHDRLEN_V(in_eth_xtra_len / 4) |
	      CPL_TX_TNL_LSO_IPV6_V(inner_ip_hdr(skb)->version == 6) |
	      CPL_TX_TNL_LSO_IPHDRLEN_V(skb_inner_network_header_len(skb) / 4) |
	      CPL_TX_TNL_LSO_TCPHDRLEN_V(inner_tcp_hdrlen(skb) / 4);
	tnl_lso->Flow_to_TcpHdrLen = htonl(val);

	tnl_lso->IpIdOffset = htons(0);

	tnl_lso->IpIdSplit_to_Mss = htons(CPL_TX_TNL_LSO_MSS_V(ssi->gso_size));
	tnl_lso->TCPSeqOffset = htonl(0);
	tnl_lso->EthLenOffset_Size = htonl(CPL_TX_TNL_LSO_SIZE_V(skb->len));
}

static inline void *write_tso_wr(struct adapter *adap, struct sk_buff *skb,
				 struct cpl_tx_pkt_lso_core *lso)
{
	int eth_xtra_len = skb_network_offset(skb) - ETH_HLEN;
	int l3hdr_len = skb_network_header_len(skb);
	const struct skb_shared_info *ssi;
	bool ipv6 = false;

	ssi = skb_shinfo(skb);
	if (ssi->gso_type & SKB_GSO_TCPV6)
		ipv6 = true;

	lso->lso_ctrl = htonl(LSO_OPCODE_V(CPL_TX_PKT_LSO) |
			      LSO_FIRST_SLICE_F | LSO_LAST_SLICE_F |
			      LSO_IPV6_V(ipv6) |
			      LSO_ETHHDR_LEN_V(eth_xtra_len / 4) |
			      LSO_IPHDR_LEN_V(l3hdr_len / 4) |
			      LSO_TCPHDR_LEN_V(tcp_hdr(skb)->doff));
	lso->ipid_ofst = htons(0);
	lso->mss = htons(ssi->gso_size);
	lso->seqno_offset = htonl(0);
	if (is_t4(adap->params.chip))
		lso->len = htonl(skb->len);
	else
		lso->len = htonl(LSO_T5_XFER_SIZE_V(skb->len));

	return (void *)(lso + 1);
}

 
int t4_sge_eth_txq_egress_update(struct adapter *adap, struct sge_eth_txq *eq,
				 int maxreclaim)
{
	unsigned int reclaimed, hw_cidx;
	struct sge_txq *q = &eq->q;
	int hw_in_use;

	if (!q->in_use || !__netif_tx_trylock(eq->txq))
		return 0;

	 
	reclaimed = reclaim_completed_tx(adap, &eq->q, maxreclaim, true);

	hw_cidx = ntohs(READ_ONCE(q->stat->cidx));
	hw_in_use = q->pidx - hw_cidx;
	if (hw_in_use < 0)
		hw_in_use += q->size;

	 
	if (netif_tx_queue_stopped(eq->txq) && hw_in_use < (q->size / 2)) {
		netif_tx_wake_queue(eq->txq);
		eq->q.restarts++;
	}

	__netif_tx_unlock(eq->txq);
	return reclaimed;
}

static inline int cxgb4_validate_skb(struct sk_buff *skb,
				     struct net_device *dev,
				     u32 min_pkt_len)
{
	u32 max_pkt_len;

	 
	if (unlikely(skb->len < min_pkt_len))
		return -EINVAL;

	 
	max_pkt_len = ETH_HLEN + dev->mtu;

	if (skb_vlan_tagged(skb))
		max_pkt_len += VLAN_HLEN;

	if (!skb_shinfo(skb)->gso_size && (unlikely(skb->len > max_pkt_len)))
		return -EINVAL;

	return 0;
}

static void *write_eo_udp_wr(struct sk_buff *skb, struct fw_eth_tx_eo_wr *wr,
			     u32 hdr_len)
{
	wr->u.udpseg.type = FW_ETH_TX_EO_TYPE_UDPSEG;
	wr->u.udpseg.ethlen = skb_network_offset(skb);
	wr->u.udpseg.iplen = cpu_to_be16(skb_network_header_len(skb));
	wr->u.udpseg.udplen = sizeof(struct udphdr);
	wr->u.udpseg.rtplen = 0;
	wr->u.udpseg.r4 = 0;
	if (skb_shinfo(skb)->gso_size)
		wr->u.udpseg.mss = cpu_to_be16(skb_shinfo(skb)->gso_size);
	else
		wr->u.udpseg.mss = cpu_to_be16(skb->len - hdr_len);
	wr->u.udpseg.schedpktsize = wr->u.udpseg.mss;
	wr->u.udpseg.plen = cpu_to_be32(skb->len - hdr_len);

	return (void *)(wr + 1);
}

 
static netdev_tx_t cxgb4_eth_xmit(struct sk_buff *skb, struct net_device *dev)
{
	enum cpl_tx_tnl_lso_type tnl_type = TX_TNL_TYPE_OPAQUE;
	bool ptp_enabled = is_ptp_enabled(skb, dev);
	unsigned int last_desc, flits, ndesc;
	u32 wr_mid, ctrl0, op, sgl_off = 0;
	const struct skb_shared_info *ssi;
	int len, qidx, credits, ret, left;
	struct tx_sw_desc *sgl_sdesc;
	struct fw_eth_tx_eo_wr *eowr;
	struct fw_eth_tx_pkt_wr *wr;
	struct cpl_tx_pkt_core *cpl;
	const struct port_info *pi;
	bool immediate = false;
	u64 cntrl, *end, *sgl;
	struct sge_eth_txq *q;
	unsigned int chip_ver;
	struct adapter *adap;

	ret = cxgb4_validate_skb(skb, dev, ETH_HLEN);
	if (ret)
		goto out_free;

	pi = netdev_priv(dev);
	adap = pi->adapter;
	ssi = skb_shinfo(skb);
#if IS_ENABLED(CONFIG_CHELSIO_IPSEC_INLINE)
	if (xfrm_offload(skb) && !ssi->gso_size)
		return adap->uld[CXGB4_ULD_IPSEC].tx_handler(skb, dev);
#endif  

#if IS_ENABLED(CONFIG_CHELSIO_TLS_DEVICE)
	if (tls_is_skb_tx_device_offloaded(skb) &&
	    (skb->len - skb_tcp_all_headers(skb)))
		return adap->uld[CXGB4_ULD_KTLS].tx_handler(skb, dev);
#endif  

	qidx = skb_get_queue_mapping(skb);
	if (ptp_enabled) {
		if (!(adap->ptp_tx_skb)) {
			skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
			adap->ptp_tx_skb = skb_get(skb);
		} else {
			goto out_free;
		}
		q = &adap->sge.ptptxq;
	} else {
		q = &adap->sge.ethtxq[qidx + pi->first_qset];
	}
	skb_tx_timestamp(skb);

	reclaim_completed_tx(adap, &q->q, -1, true);
	cntrl = TXPKT_L4CSUM_DIS_F | TXPKT_IPCSUM_DIS_F;

#ifdef CONFIG_CHELSIO_T4_FCOE
	ret = cxgb_fcoe_offload(skb, adap, pi, &cntrl);
	if (unlikely(ret == -EOPNOTSUPP))
		goto out_free;
#endif  

	chip_ver = CHELSIO_CHIP_VERSION(adap->params.chip);
	flits = calc_tx_flits(skb, chip_ver);
	ndesc = flits_to_desc(flits);
	credits = txq_avail(&q->q) - ndesc;

	if (unlikely(credits < 0)) {
		eth_txq_stop(q);
		dev_err(adap->pdev_dev,
			"%s: Tx ring %u full while queue awake!\n",
			dev->name, qidx);
		return NETDEV_TX_BUSY;
	}

	if (is_eth_imm(skb, chip_ver))
		immediate = true;

	if (skb->encapsulation && chip_ver > CHELSIO_T5)
		tnl_type = cxgb_encap_offload_supported(skb);

	last_desc = q->q.pidx + ndesc - 1;
	if (last_desc >= q->q.size)
		last_desc -= q->q.size;
	sgl_sdesc = &q->q.sdesc[last_desc];

	if (!immediate &&
	    unlikely(cxgb4_map_skb(adap->pdev_dev, skb, sgl_sdesc->addr) < 0)) {
		memset(sgl_sdesc->addr, 0, sizeof(sgl_sdesc->addr));
		q->mapping_err++;
		goto out_free;
	}

	wr_mid = FW_WR_LEN16_V(DIV_ROUND_UP(flits, 2));
	if (unlikely(credits < ETHTXQ_STOP_THRES)) {
		 
		eth_txq_stop(q);
		if (chip_ver > CHELSIO_T5)
			wr_mid |= FW_WR_EQUEQ_F | FW_WR_EQUIQ_F;
	}

	wr = (void *)&q->q.desc[q->q.pidx];
	eowr = (void *)&q->q.desc[q->q.pidx];
	wr->equiq_to_len16 = htonl(wr_mid);
	wr->r3 = cpu_to_be64(0);
	if (skb_shinfo(skb)->gso_type & SKB_GSO_UDP_L4)
		end = (u64 *)eowr + flits;
	else
		end = (u64 *)wr + flits;

	len = immediate ? skb->len : 0;
	len += sizeof(*cpl);
	if (ssi->gso_size && !(ssi->gso_type & SKB_GSO_UDP_L4)) {
		struct cpl_tx_pkt_lso_core *lso = (void *)(wr + 1);
		struct cpl_tx_tnl_lso *tnl_lso = (void *)(wr + 1);

		if (tnl_type)
			len += sizeof(*tnl_lso);
		else
			len += sizeof(*lso);

		wr->op_immdlen = htonl(FW_WR_OP_V(FW_ETH_TX_PKT_WR) |
				       FW_WR_IMMDLEN_V(len));
		if (tnl_type) {
			struct iphdr *iph = ip_hdr(skb);

			t6_fill_tnl_lso(skb, tnl_lso, tnl_type);
			cpl = (void *)(tnl_lso + 1);
			 
			if (iph->version == 4) {
				iph->check = 0;
				iph->tot_len = 0;
				iph->check = ~ip_fast_csum((u8 *)iph, iph->ihl);
			}
			if (skb->ip_summed == CHECKSUM_PARTIAL)
				cntrl = hwcsum(adap->params.chip, skb);
		} else {
			cpl = write_tso_wr(adap, skb, lso);
			cntrl = hwcsum(adap->params.chip, skb);
		}
		sgl = (u64 *)(cpl + 1);  
		q->tso++;
		q->tx_cso += ssi->gso_segs;
	} else if (ssi->gso_size) {
		u64 *start;
		u32 hdrlen;

		hdrlen = eth_get_headlen(dev, skb->data, skb_headlen(skb));
		len += hdrlen;
		wr->op_immdlen = cpu_to_be32(FW_WR_OP_V(FW_ETH_TX_EO_WR) |
					     FW_ETH_TX_EO_WR_IMMDLEN_V(len));
		cpl = write_eo_udp_wr(skb, eowr, hdrlen);
		cntrl = hwcsum(adap->params.chip, skb);

		start = (u64 *)(cpl + 1);
		sgl = (u64 *)inline_tx_skb_header(skb, &q->q, (void *)start,
						  hdrlen);
		if (unlikely(start > sgl)) {
			left = (u8 *)end - (u8 *)q->q.stat;
			end = (void *)q->q.desc + left;
		}
		sgl_off = hdrlen;
		q->uso++;
		q->tx_cso += ssi->gso_segs;
	} else {
		if (ptp_enabled)
			op = FW_PTP_TX_PKT_WR;
		else
			op = FW_ETH_TX_PKT_WR;
		wr->op_immdlen = htonl(FW_WR_OP_V(op) |
				       FW_WR_IMMDLEN_V(len));
		cpl = (void *)(wr + 1);
		sgl = (u64 *)(cpl + 1);
		if (skb->ip_summed == CHECKSUM_PARTIAL) {
			cntrl = hwcsum(adap->params.chip, skb) |
				TXPKT_IPCSUM_DIS_F;
			q->tx_cso++;
		}
	}

	if (unlikely((u8 *)sgl >= (u8 *)q->q.stat)) {
		 
		left = (u8 *)end - (u8 *)q->q.stat;
		end = (void *)q->q.desc + left;
		sgl = (void *)q->q.desc;
	}

	if (skb_vlan_tag_present(skb)) {
		q->vlan_ins++;
		cntrl |= TXPKT_VLAN_VLD_F | TXPKT_VLAN_V(skb_vlan_tag_get(skb));
#ifdef CONFIG_CHELSIO_T4_FCOE
		if (skb->protocol == htons(ETH_P_FCOE))
			cntrl |= TXPKT_VLAN_V(
				 ((skb->priority & 0x7) << VLAN_PRIO_SHIFT));
#endif  
	}

	ctrl0 = TXPKT_OPCODE_V(CPL_TX_PKT_XT) | TXPKT_INTF_V(pi->tx_chan) |
		TXPKT_PF_V(adap->pf);
	if (ptp_enabled)
		ctrl0 |= TXPKT_TSTAMP_F;
#ifdef CONFIG_CHELSIO_T4_DCB
	if (is_t4(adap->params.chip))
		ctrl0 |= TXPKT_OVLAN_IDX_V(q->dcb_prio);
	else
		ctrl0 |= TXPKT_T5_OVLAN_IDX_V(q->dcb_prio);
#endif
	cpl->ctrl0 = htonl(ctrl0);
	cpl->pack = htons(0);
	cpl->len = htons(skb->len);
	cpl->ctrl1 = cpu_to_be64(cntrl);

	if (immediate) {
		cxgb4_inline_tx_skb(skb, &q->q, sgl);
		dev_consume_skb_any(skb);
	} else {
		cxgb4_write_sgl(skb, &q->q, (void *)sgl, end, sgl_off,
				sgl_sdesc->addr);
		skb_orphan(skb);
		sgl_sdesc->skb = skb;
	}

	txq_advance(&q->q, ndesc);

	cxgb4_ring_tx_db(adap, &q->q, ndesc);
	return NETDEV_TX_OK;

out_free:
	dev_kfree_skb_any(skb);
	return NETDEV_TX_OK;
}

 
enum {
	 
	EQ_UNIT = SGE_EQ_IDXSIZE,
	FL_PER_EQ_UNIT = EQ_UNIT / sizeof(__be64),
	TXD_PER_EQ_UNIT = EQ_UNIT / sizeof(__be64),

	T4VF_ETHTXQ_MAX_HDR = (sizeof(struct fw_eth_tx_pkt_vm_wr) +
			       sizeof(struct cpl_tx_pkt_lso_core) +
			       sizeof(struct cpl_tx_pkt_core)) / sizeof(__be64),
};

 
static inline int t4vf_is_eth_imm(const struct sk_buff *skb)
{
	 
	return false;
}

 
static inline unsigned int t4vf_calc_tx_flits(const struct sk_buff *skb)
{
	unsigned int flits;

	 
	if (t4vf_is_eth_imm(skb))
		return DIV_ROUND_UP(skb->len + sizeof(struct cpl_tx_pkt),
				    sizeof(__be64));

	 
	flits = sgl_len(skb_shinfo(skb)->nr_frags + 1);
	if (skb_shinfo(skb)->gso_size)
		flits += (sizeof(struct fw_eth_tx_pkt_vm_wr) +
			  sizeof(struct cpl_tx_pkt_lso_core) +
			  sizeof(struct cpl_tx_pkt_core)) / sizeof(__be64);
	else
		flits += (sizeof(struct fw_eth_tx_pkt_vm_wr) +
			  sizeof(struct cpl_tx_pkt_core)) / sizeof(__be64);
	return flits;
}

 
static netdev_tx_t cxgb4_vf_eth_xmit(struct sk_buff *skb,
				     struct net_device *dev)
{
	unsigned int last_desc, flits, ndesc;
	const struct skb_shared_info *ssi;
	struct fw_eth_tx_pkt_vm_wr *wr;
	struct tx_sw_desc *sgl_sdesc;
	struct cpl_tx_pkt_core *cpl;
	const struct port_info *pi;
	struct sge_eth_txq *txq;
	struct adapter *adapter;
	int qidx, credits, ret;
	size_t fw_hdr_copy_len;
	unsigned int chip_ver;
	u64 cntrl, *end;
	u32 wr_mid;

	 
	BUILD_BUG_ON(sizeof(wr->firmware) !=
		     (sizeof(wr->ethmacdst) + sizeof(wr->ethmacsrc) +
		      sizeof(wr->ethtype) + sizeof(wr->vlantci)));
	fw_hdr_copy_len = sizeof(wr->firmware);
	ret = cxgb4_validate_skb(skb, dev, fw_hdr_copy_len);
	if (ret)
		goto out_free;

	 
	pi = netdev_priv(dev);
	adapter = pi->adapter;
	qidx = skb_get_queue_mapping(skb);
	WARN_ON(qidx >= pi->nqsets);
	txq = &adapter->sge.ethtxq[pi->first_qset + qidx];

	 
	reclaim_completed_tx(adapter, &txq->q, -1, true);

	 
	flits = t4vf_calc_tx_flits(skb);
	ndesc = flits_to_desc(flits);
	credits = txq_avail(&txq->q) - ndesc;

	if (unlikely(credits < 0)) {
		 
		eth_txq_stop(txq);
		dev_err(adapter->pdev_dev,
			"%s: TX ring %u full while queue awake!\n",
			dev->name, qidx);
		return NETDEV_TX_BUSY;
	}

	last_desc = txq->q.pidx + ndesc - 1;
	if (last_desc >= txq->q.size)
		last_desc -= txq->q.size;
	sgl_sdesc = &txq->q.sdesc[last_desc];

	if (!t4vf_is_eth_imm(skb) &&
	    unlikely(cxgb4_map_skb(adapter->pdev_dev, skb,
				   sgl_sdesc->addr) < 0)) {
		 
		memset(sgl_sdesc->addr, 0, sizeof(sgl_sdesc->addr));
		txq->mapping_err++;
		goto out_free;
	}

	chip_ver = CHELSIO_CHIP_VERSION(adapter->params.chip);
	wr_mid = FW_WR_LEN16_V(DIV_ROUND_UP(flits, 2));
	if (unlikely(credits < ETHTXQ_STOP_THRES)) {
		 
		eth_txq_stop(txq);
		if (chip_ver > CHELSIO_T5)
			wr_mid |= FW_WR_EQUEQ_F | FW_WR_EQUIQ_F;
	}

	 
	WARN_ON(DIV_ROUND_UP(T4VF_ETHTXQ_MAX_HDR, TXD_PER_EQ_UNIT) > 1);
	wr = (void *)&txq->q.desc[txq->q.pidx];
	wr->equiq_to_len16 = cpu_to_be32(wr_mid);
	wr->r3[0] = cpu_to_be32(0);
	wr->r3[1] = cpu_to_be32(0);
	skb_copy_from_linear_data(skb, &wr->firmware, fw_hdr_copy_len);
	end = (u64 *)wr + flits;

	 
	ssi = skb_shinfo(skb);
	if (ssi->gso_size) {
		struct cpl_tx_pkt_lso_core *lso = (void *)(wr + 1);
		bool v6 = (ssi->gso_type & SKB_GSO_TCPV6) != 0;
		int l3hdr_len = skb_network_header_len(skb);
		int eth_xtra_len = skb_network_offset(skb) - ETH_HLEN;

		wr->op_immdlen =
			cpu_to_be32(FW_WR_OP_V(FW_ETH_TX_PKT_VM_WR) |
				    FW_WR_IMMDLEN_V(sizeof(*lso) +
						    sizeof(*cpl)));
		  
		lso->lso_ctrl =
			cpu_to_be32(LSO_OPCODE_V(CPL_TX_PKT_LSO) |
				    LSO_FIRST_SLICE_F |
				    LSO_LAST_SLICE_F |
				    LSO_IPV6_V(v6) |
				    LSO_ETHHDR_LEN_V(eth_xtra_len / 4) |
				    LSO_IPHDR_LEN_V(l3hdr_len / 4) |
				    LSO_TCPHDR_LEN_V(tcp_hdr(skb)->doff));
		lso->ipid_ofst = cpu_to_be16(0);
		lso->mss = cpu_to_be16(ssi->gso_size);
		lso->seqno_offset = cpu_to_be32(0);
		if (is_t4(adapter->params.chip))
			lso->len = cpu_to_be32(skb->len);
		else
			lso->len = cpu_to_be32(LSO_T5_XFER_SIZE_V(skb->len));

		 
		cpl = (void *)(lso + 1);

		if (chip_ver <= CHELSIO_T5)
			cntrl = TXPKT_ETHHDR_LEN_V(eth_xtra_len);
		else
			cntrl = T6_TXPKT_ETHHDR_LEN_V(eth_xtra_len);

		cntrl |= TXPKT_CSUM_TYPE_V(v6 ?
					   TX_CSUM_TCPIP6 : TX_CSUM_TCPIP) |
			 TXPKT_IPHDR_LEN_V(l3hdr_len);
		txq->tso++;
		txq->tx_cso += ssi->gso_segs;
	} else {
		int len;

		len = (t4vf_is_eth_imm(skb)
		       ? skb->len + sizeof(*cpl)
		       : sizeof(*cpl));
		wr->op_immdlen =
			cpu_to_be32(FW_WR_OP_V(FW_ETH_TX_PKT_VM_WR) |
				    FW_WR_IMMDLEN_V(len));

		 
		cpl = (void *)(wr + 1);
		if (skb->ip_summed == CHECKSUM_PARTIAL) {
			cntrl = hwcsum(adapter->params.chip, skb) |
				TXPKT_IPCSUM_DIS_F;
			txq->tx_cso++;
		} else {
			cntrl = TXPKT_L4CSUM_DIS_F | TXPKT_IPCSUM_DIS_F;
		}
	}

	 
	if (skb_vlan_tag_present(skb)) {
		txq->vlan_ins++;
		cntrl |= TXPKT_VLAN_VLD_F | TXPKT_VLAN_V(skb_vlan_tag_get(skb));
	}

	  
	cpl->ctrl0 = cpu_to_be32(TXPKT_OPCODE_V(CPL_TX_PKT_XT) |
				 TXPKT_INTF_V(pi->port_id) |
				 TXPKT_PF_V(0));
	cpl->pack = cpu_to_be16(0);
	cpl->len = cpu_to_be16(skb->len);
	cpl->ctrl1 = cpu_to_be64(cntrl);

	 
	if (t4vf_is_eth_imm(skb)) {
		 
		cxgb4_inline_tx_skb(skb, &txq->q, cpl + 1);
		dev_consume_skb_any(skb);
	} else {
		 
		struct ulptx_sgl *sgl = (struct ulptx_sgl *)(cpl + 1);
		struct sge_txq *tq = &txq->q;

		 
		if (unlikely((void *)sgl == (void *)tq->stat)) {
			sgl = (void *)tq->desc;
			end = (void *)((void *)tq->desc +
				       ((void *)end - (void *)tq->stat));
		}

		cxgb4_write_sgl(skb, tq, sgl, end, 0, sgl_sdesc->addr);
		skb_orphan(skb);
		sgl_sdesc->skb = skb;
	}

	 
	txq_advance(&txq->q, ndesc);

	cxgb4_ring_tx_db(adapter, &txq->q, ndesc);
	return NETDEV_TX_OK;

out_free:
	 
	dev_kfree_skb_any(skb);
	return NETDEV_TX_OK;
}

 
static inline void reclaim_completed_tx_imm(struct sge_txq *q)
{
	int hw_cidx = ntohs(READ_ONCE(q->stat->cidx));
	int reclaim = hw_cidx - q->cidx;

	if (reclaim < 0)
		reclaim += q->size;

	q->in_use -= reclaim;
	q->cidx = hw_cidx;
}

static inline void eosw_txq_advance_index(u32 *idx, u32 n, u32 max)
{
	u32 val = *idx + n;

	if (val >= max)
		val -= max;

	*idx = val;
}

void cxgb4_eosw_txq_free_desc(struct adapter *adap,
			      struct sge_eosw_txq *eosw_txq, u32 ndesc)
{
	struct tx_sw_desc *d;

	d = &eosw_txq->desc[eosw_txq->last_cidx];
	while (ndesc--) {
		if (d->skb) {
			if (d->addr[0]) {
				unmap_skb(adap->pdev_dev, d->skb, d->addr);
				memset(d->addr, 0, sizeof(d->addr));
			}
			dev_consume_skb_any(d->skb);
			d->skb = NULL;
		}
		eosw_txq_advance_index(&eosw_txq->last_cidx, 1,
				       eosw_txq->ndesc);
		d = &eosw_txq->desc[eosw_txq->last_cidx];
	}
}

static inline void eosw_txq_advance(struct sge_eosw_txq *eosw_txq, u32 n)
{
	eosw_txq_advance_index(&eosw_txq->pidx, n, eosw_txq->ndesc);
	eosw_txq->inuse += n;
}

static inline int eosw_txq_enqueue(struct sge_eosw_txq *eosw_txq,
				   struct sk_buff *skb)
{
	if (eosw_txq->inuse == eosw_txq->ndesc)
		return -ENOMEM;

	eosw_txq->desc[eosw_txq->pidx].skb = skb;
	return 0;
}

static inline struct sk_buff *eosw_txq_peek(struct sge_eosw_txq *eosw_txq)
{
	return eosw_txq->desc[eosw_txq->last_pidx].skb;
}

static inline u8 ethofld_calc_tx_flits(struct adapter *adap,
				       struct sk_buff *skb, u32 hdr_len)
{
	u8 flits, nsgl = 0;
	u32 wrlen;

	wrlen = sizeof(struct fw_eth_tx_eo_wr) + sizeof(struct cpl_tx_pkt_core);
	if (skb_shinfo(skb)->gso_size &&
	    !(skb_shinfo(skb)->gso_type & SKB_GSO_UDP_L4))
		wrlen += sizeof(struct cpl_tx_pkt_lso_core);

	wrlen += roundup(hdr_len, 16);

	 
	flits = DIV_ROUND_UP(wrlen, 8);

	if (skb_shinfo(skb)->nr_frags > 0) {
		if (skb_headlen(skb) - hdr_len)
			nsgl = sgl_len(skb_shinfo(skb)->nr_frags + 1);
		else
			nsgl = sgl_len(skb_shinfo(skb)->nr_frags);
	} else if (skb->len - hdr_len) {
		nsgl = sgl_len(1);
	}

	return flits + nsgl;
}

static void *write_eo_wr(struct adapter *adap, struct sge_eosw_txq *eosw_txq,
			 struct sk_buff *skb, struct fw_eth_tx_eo_wr *wr,
			 u32 hdr_len, u32 wrlen)
{
	const struct skb_shared_info *ssi = skb_shinfo(skb);
	struct cpl_tx_pkt_core *cpl;
	u32 immd_len, wrlen16;
	bool compl = false;
	u8 ver, proto;

	ver = ip_hdr(skb)->version;
	proto = (ver == 6) ? ipv6_hdr(skb)->nexthdr : ip_hdr(skb)->protocol;

	wrlen16 = DIV_ROUND_UP(wrlen, 16);
	immd_len = sizeof(struct cpl_tx_pkt_core);
	if (skb_shinfo(skb)->gso_size &&
	    !(skb_shinfo(skb)->gso_type & SKB_GSO_UDP_L4))
		immd_len += sizeof(struct cpl_tx_pkt_lso_core);
	immd_len += hdr_len;

	if (!eosw_txq->ncompl ||
	    (eosw_txq->last_compl + wrlen16) >=
	    (adap->params.ofldq_wr_cred / 2)) {
		compl = true;
		eosw_txq->ncompl++;
		eosw_txq->last_compl = 0;
	}

	wr->op_immdlen = cpu_to_be32(FW_WR_OP_V(FW_ETH_TX_EO_WR) |
				     FW_ETH_TX_EO_WR_IMMDLEN_V(immd_len) |
				     FW_WR_COMPL_V(compl));
	wr->equiq_to_len16 = cpu_to_be32(FW_WR_LEN16_V(wrlen16) |
					 FW_WR_FLOWID_V(eosw_txq->hwtid));
	wr->r3 = 0;
	if (proto == IPPROTO_UDP) {
		cpl = write_eo_udp_wr(skb, wr, hdr_len);
	} else {
		wr->u.tcpseg.type = FW_ETH_TX_EO_TYPE_TCPSEG;
		wr->u.tcpseg.ethlen = skb_network_offset(skb);
		wr->u.tcpseg.iplen = cpu_to_be16(skb_network_header_len(skb));
		wr->u.tcpseg.tcplen = tcp_hdrlen(skb);
		wr->u.tcpseg.tsclk_tsoff = 0;
		wr->u.tcpseg.r4 = 0;
		wr->u.tcpseg.r5 = 0;
		wr->u.tcpseg.plen = cpu_to_be32(skb->len - hdr_len);

		if (ssi->gso_size) {
			struct cpl_tx_pkt_lso_core *lso = (void *)(wr + 1);

			wr->u.tcpseg.mss = cpu_to_be16(ssi->gso_size);
			cpl = write_tso_wr(adap, skb, lso);
		} else {
			wr->u.tcpseg.mss = cpu_to_be16(0xffff);
			cpl = (void *)(wr + 1);
		}
	}

	eosw_txq->cred -= wrlen16;
	eosw_txq->last_compl += wrlen16;
	return cpl;
}

static int ethofld_hard_xmit(struct net_device *dev,
			     struct sge_eosw_txq *eosw_txq)
{
	struct port_info *pi = netdev2pinfo(dev);
	struct adapter *adap = netdev2adap(dev);
	u32 wrlen, wrlen16, hdr_len, data_len;
	enum sge_eosw_state next_state;
	u64 cntrl, *start, *end, *sgl;
	struct sge_eohw_txq *eohw_txq;
	struct cpl_tx_pkt_core *cpl;
	struct fw_eth_tx_eo_wr *wr;
	bool skip_eotx_wr = false;
	struct tx_sw_desc *d;
	struct sk_buff *skb;
	int left, ret = 0;
	u8 flits, ndesc;

	eohw_txq = &adap->sge.eohw_txq[eosw_txq->hwqid];
	spin_lock(&eohw_txq->lock);
	reclaim_completed_tx_imm(&eohw_txq->q);

	d = &eosw_txq->desc[eosw_txq->last_pidx];
	skb = d->skb;
	skb_tx_timestamp(skb);

	wr = (struct fw_eth_tx_eo_wr *)&eohw_txq->q.desc[eohw_txq->q.pidx];
	if (unlikely(eosw_txq->state != CXGB4_EO_STATE_ACTIVE &&
		     eosw_txq->last_pidx == eosw_txq->flowc_idx)) {
		hdr_len = skb->len;
		data_len = 0;
		flits = DIV_ROUND_UP(hdr_len, 8);
		if (eosw_txq->state == CXGB4_EO_STATE_FLOWC_OPEN_SEND)
			next_state = CXGB4_EO_STATE_FLOWC_OPEN_REPLY;
		else
			next_state = CXGB4_EO_STATE_FLOWC_CLOSE_REPLY;
		skip_eotx_wr = true;
	} else {
		hdr_len = eth_get_headlen(dev, skb->data, skb_headlen(skb));
		data_len = skb->len - hdr_len;
		flits = ethofld_calc_tx_flits(adap, skb, hdr_len);
	}
	ndesc = flits_to_desc(flits);
	wrlen = flits * 8;
	wrlen16 = DIV_ROUND_UP(wrlen, 16);

	left = txq_avail(&eohw_txq->q) - ndesc;

	 
	if (unlikely(left < 0 || wrlen16 > eosw_txq->cred)) {
		ret = -ENOMEM;
		goto out_unlock;
	}

	if (unlikely(skip_eotx_wr)) {
		start = (u64 *)wr;
		eosw_txq->state = next_state;
		eosw_txq->cred -= wrlen16;
		eosw_txq->ncompl++;
		eosw_txq->last_compl = 0;
		goto write_wr_headers;
	}

	cpl = write_eo_wr(adap, eosw_txq, skb, wr, hdr_len, wrlen);
	cntrl = hwcsum(adap->params.chip, skb);
	if (skb_vlan_tag_present(skb))
		cntrl |= TXPKT_VLAN_VLD_F | TXPKT_VLAN_V(skb_vlan_tag_get(skb));

	cpl->ctrl0 = cpu_to_be32(TXPKT_OPCODE_V(CPL_TX_PKT_XT) |
				 TXPKT_INTF_V(pi->tx_chan) |
				 TXPKT_PF_V(adap->pf));
	cpl->pack = 0;
	cpl->len = cpu_to_be16(skb->len);
	cpl->ctrl1 = cpu_to_be64(cntrl);

	start = (u64 *)(cpl + 1);

write_wr_headers:
	sgl = (u64 *)inline_tx_skb_header(skb, &eohw_txq->q, (void *)start,
					  hdr_len);
	if (data_len) {
		ret = cxgb4_map_skb(adap->pdev_dev, skb, d->addr);
		if (unlikely(ret)) {
			memset(d->addr, 0, sizeof(d->addr));
			eohw_txq->mapping_err++;
			goto out_unlock;
		}

		end = (u64 *)wr + flits;
		if (unlikely(start > sgl)) {
			left = (u8 *)end - (u8 *)eohw_txq->q.stat;
			end = (void *)eohw_txq->q.desc + left;
		}

		if (unlikely((u8 *)sgl >= (u8 *)eohw_txq->q.stat)) {
			 
			left = (u8 *)end - (u8 *)eohw_txq->q.stat;

			end = (void *)eohw_txq->q.desc + left;
			sgl = (void *)eohw_txq->q.desc;
		}

		cxgb4_write_sgl(skb, &eohw_txq->q, (void *)sgl, end, hdr_len,
				d->addr);
	}

	if (skb_shinfo(skb)->gso_size) {
		if (skb_shinfo(skb)->gso_type & SKB_GSO_UDP_L4)
			eohw_txq->uso++;
		else
			eohw_txq->tso++;
		eohw_txq->tx_cso += skb_shinfo(skb)->gso_segs;
	} else if (skb->ip_summed == CHECKSUM_PARTIAL) {
		eohw_txq->tx_cso++;
	}

	if (skb_vlan_tag_present(skb))
		eohw_txq->vlan_ins++;

	txq_advance(&eohw_txq->q, ndesc);
	cxgb4_ring_tx_db(adap, &eohw_txq->q, ndesc);
	eosw_txq_advance_index(&eosw_txq->last_pidx, 1, eosw_txq->ndesc);

out_unlock:
	spin_unlock(&eohw_txq->lock);
	return ret;
}

static void ethofld_xmit(struct net_device *dev, struct sge_eosw_txq *eosw_txq)
{
	struct sk_buff *skb;
	int pktcount, ret;

	switch (eosw_txq->state) {
	case CXGB4_EO_STATE_ACTIVE:
	case CXGB4_EO_STATE_FLOWC_OPEN_SEND:
	case CXGB4_EO_STATE_FLOWC_CLOSE_SEND:
		pktcount = eosw_txq->pidx - eosw_txq->last_pidx;
		if (pktcount < 0)
			pktcount += eosw_txq->ndesc;
		break;
	case CXGB4_EO_STATE_FLOWC_OPEN_REPLY:
	case CXGB4_EO_STATE_FLOWC_CLOSE_REPLY:
	case CXGB4_EO_STATE_CLOSED:
	default:
		return;
	}

	while (pktcount--) {
		skb = eosw_txq_peek(eosw_txq);
		if (!skb) {
			eosw_txq_advance_index(&eosw_txq->last_pidx, 1,
					       eosw_txq->ndesc);
			continue;
		}

		ret = ethofld_hard_xmit(dev, eosw_txq);
		if (ret)
			break;
	}
}

static netdev_tx_t cxgb4_ethofld_xmit(struct sk_buff *skb,
				      struct net_device *dev)
{
	struct cxgb4_tc_port_mqprio *tc_port_mqprio;
	struct port_info *pi = netdev2pinfo(dev);
	struct adapter *adap = netdev2adap(dev);
	struct sge_eosw_txq *eosw_txq;
	u32 qid;
	int ret;

	ret = cxgb4_validate_skb(skb, dev, ETH_HLEN);
	if (ret)
		goto out_free;

	tc_port_mqprio = &adap->tc_mqprio->port_mqprio[pi->port_id];
	qid = skb_get_queue_mapping(skb) - pi->nqsets;
	eosw_txq = &tc_port_mqprio->eosw_txq[qid];
	spin_lock_bh(&eosw_txq->lock);
	if (eosw_txq->state != CXGB4_EO_STATE_ACTIVE)
		goto out_unlock;

	ret = eosw_txq_enqueue(eosw_txq, skb);
	if (ret)
		goto out_unlock;

	 
	skb_orphan(skb);

	eosw_txq_advance(eosw_txq, 1);
	ethofld_xmit(dev, eosw_txq);
	spin_unlock_bh(&eosw_txq->lock);
	return NETDEV_TX_OK;

out_unlock:
	spin_unlock_bh(&eosw_txq->lock);
out_free:
	dev_kfree_skb_any(skb);
	return NETDEV_TX_OK;
}

netdev_tx_t t4_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct port_info *pi = netdev_priv(dev);
	u16 qid = skb_get_queue_mapping(skb);

	if (unlikely(pi->eth_flags & PRIV_FLAG_PORT_TX_VM))
		return cxgb4_vf_eth_xmit(skb, dev);

	if (unlikely(qid >= pi->nqsets))
		return cxgb4_ethofld_xmit(skb, dev);

	if (is_ptp_enabled(skb, dev)) {
		struct adapter *adap = netdev2adap(dev);
		netdev_tx_t ret;

		spin_lock(&adap->ptp_lock);
		ret = cxgb4_eth_xmit(skb, dev);
		spin_unlock(&adap->ptp_lock);
		return ret;
	}

	return cxgb4_eth_xmit(skb, dev);
}

static void eosw_txq_flush_pending_skbs(struct sge_eosw_txq *eosw_txq)
{
	int pktcount = eosw_txq->pidx - eosw_txq->last_pidx;
	int pidx = eosw_txq->pidx;
	struct sk_buff *skb;

	if (!pktcount)
		return;

	if (pktcount < 0)
		pktcount += eosw_txq->ndesc;

	while (pktcount--) {
		pidx--;
		if (pidx < 0)
			pidx += eosw_txq->ndesc;

		skb = eosw_txq->desc[pidx].skb;
		if (skb) {
			dev_consume_skb_any(skb);
			eosw_txq->desc[pidx].skb = NULL;
			eosw_txq->inuse--;
		}
	}

	eosw_txq->pidx = eosw_txq->last_pidx + 1;
}

 
int cxgb4_ethofld_send_flowc(struct net_device *dev, u32 eotid, u32 tc)
{
	struct port_info *pi = netdev2pinfo(dev);
	struct adapter *adap = netdev2adap(dev);
	enum sge_eosw_state next_state;
	struct sge_eosw_txq *eosw_txq;
	u32 len, len16, nparams = 6;
	struct fw_flowc_wr *flowc;
	struct eotid_entry *entry;
	struct sge_ofld_rxq *rxq;
	struct sk_buff *skb;
	int ret = 0;

	len = struct_size(flowc, mnemval, nparams);
	len16 = DIV_ROUND_UP(len, 16);

	entry = cxgb4_lookup_eotid(&adap->tids, eotid);
	if (!entry)
		return -ENOMEM;

	eosw_txq = (struct sge_eosw_txq *)entry->data;
	if (!eosw_txq)
		return -ENOMEM;

	if (!(adap->flags & CXGB4_FW_OK)) {
		 
		complete(&eosw_txq->completion);
		return -EIO;
	}

	skb = alloc_skb(len, GFP_KERNEL);
	if (!skb)
		return -ENOMEM;

	spin_lock_bh(&eosw_txq->lock);
	if (tc != FW_SCHED_CLS_NONE) {
		if (eosw_txq->state != CXGB4_EO_STATE_CLOSED)
			goto out_free_skb;

		next_state = CXGB4_EO_STATE_FLOWC_OPEN_SEND;
	} else {
		if (eosw_txq->state != CXGB4_EO_STATE_ACTIVE)
			goto out_free_skb;

		next_state = CXGB4_EO_STATE_FLOWC_CLOSE_SEND;
	}

	flowc = __skb_put(skb, len);
	memset(flowc, 0, len);

	rxq = &adap->sge.eohw_rxq[eosw_txq->hwqid];
	flowc->flowid_len16 = cpu_to_be32(FW_WR_LEN16_V(len16) |
					  FW_WR_FLOWID_V(eosw_txq->hwtid));
	flowc->op_to_nparams = cpu_to_be32(FW_WR_OP_V(FW_FLOWC_WR) |
					   FW_FLOWC_WR_NPARAMS_V(nparams) |
					   FW_WR_COMPL_V(1));
	flowc->mnemval[0].mnemonic = FW_FLOWC_MNEM_PFNVFN;
	flowc->mnemval[0].val = cpu_to_be32(FW_PFVF_CMD_PFN_V(adap->pf));
	flowc->mnemval[1].mnemonic = FW_FLOWC_MNEM_CH;
	flowc->mnemval[1].val = cpu_to_be32(pi->tx_chan);
	flowc->mnemval[2].mnemonic = FW_FLOWC_MNEM_PORT;
	flowc->mnemval[2].val = cpu_to_be32(pi->tx_chan);
	flowc->mnemval[3].mnemonic = FW_FLOWC_MNEM_IQID;
	flowc->mnemval[3].val = cpu_to_be32(rxq->rspq.abs_id);
	flowc->mnemval[4].mnemonic = FW_FLOWC_MNEM_SCHEDCLASS;
	flowc->mnemval[4].val = cpu_to_be32(tc);
	flowc->mnemval[5].mnemonic = FW_FLOWC_MNEM_EOSTATE;
	flowc->mnemval[5].val = cpu_to_be32(tc == FW_SCHED_CLS_NONE ?
					    FW_FLOWC_MNEM_EOSTATE_CLOSING :
					    FW_FLOWC_MNEM_EOSTATE_ESTABLISHED);

	 
	if (tc == FW_SCHED_CLS_NONE)
		eosw_txq_flush_pending_skbs(eosw_txq);

	ret = eosw_txq_enqueue(eosw_txq, skb);
	if (ret)
		goto out_free_skb;

	eosw_txq->state = next_state;
	eosw_txq->flowc_idx = eosw_txq->pidx;
	eosw_txq_advance(eosw_txq, 1);
	ethofld_xmit(dev, eosw_txq);

	spin_unlock_bh(&eosw_txq->lock);
	return 0;

out_free_skb:
	dev_consume_skb_any(skb);
	spin_unlock_bh(&eosw_txq->lock);
	return ret;
}

 
static inline int is_imm(const struct sk_buff *skb)
{
	return skb->len <= MAX_CTRL_WR_LEN;
}

 
static void ctrlq_check_stop(struct sge_ctrl_txq *q, struct fw_wr_hdr *wr)
{
	reclaim_completed_tx_imm(&q->q);
	if (unlikely(txq_avail(&q->q) < TXQ_STOP_THRES)) {
		wr->lo |= htonl(FW_WR_EQUEQ_F | FW_WR_EQUIQ_F);
		q->q.stops++;
		q->full = 1;
	}
}

#define CXGB4_SELFTEST_LB_STR "CHELSIO_SELFTEST"

int cxgb4_selftest_lb_pkt(struct net_device *netdev)
{
	struct port_info *pi = netdev_priv(netdev);
	struct adapter *adap = pi->adapter;
	struct cxgb4_ethtool_lb_test *lb;
	int ret, i = 0, pkt_len, credits;
	struct fw_eth_tx_pkt_wr *wr;
	struct cpl_tx_pkt_core *cpl;
	u32 ctrl0, ndesc, flits;
	struct sge_eth_txq *q;
	u8 *sgl;

	pkt_len = ETH_HLEN + sizeof(CXGB4_SELFTEST_LB_STR);

	flits = DIV_ROUND_UP(pkt_len + sizeof(*cpl) + sizeof(*wr),
			     sizeof(__be64));
	ndesc = flits_to_desc(flits);

	lb = &pi->ethtool_lb;
	lb->loopback = 1;

	q = &adap->sge.ethtxq[pi->first_qset];
	__netif_tx_lock(q->txq, smp_processor_id());

	reclaim_completed_tx(adap, &q->q, -1, true);
	credits = txq_avail(&q->q) - ndesc;
	if (unlikely(credits < 0)) {
		__netif_tx_unlock(q->txq);
		return -ENOMEM;
	}

	wr = (void *)&q->q.desc[q->q.pidx];
	memset(wr, 0, sizeof(struct tx_desc));

	wr->op_immdlen = htonl(FW_WR_OP_V(FW_ETH_TX_PKT_WR) |
			       FW_WR_IMMDLEN_V(pkt_len +
			       sizeof(*cpl)));
	wr->equiq_to_len16 = htonl(FW_WR_LEN16_V(DIV_ROUND_UP(flits, 2)));
	wr->r3 = cpu_to_be64(0);

	cpl = (void *)(wr + 1);
	sgl = (u8 *)(cpl + 1);

	ctrl0 = TXPKT_OPCODE_V(CPL_TX_PKT_XT) | TXPKT_PF_V(adap->pf) |
		TXPKT_INTF_V(pi->tx_chan + 4);

	cpl->ctrl0 = htonl(ctrl0);
	cpl->pack = htons(0);
	cpl->len = htons(pkt_len);
	cpl->ctrl1 = cpu_to_be64(TXPKT_L4CSUM_DIS_F | TXPKT_IPCSUM_DIS_F);

	eth_broadcast_addr(sgl);
	i += ETH_ALEN;
	ether_addr_copy(&sgl[i], netdev->dev_addr);
	i += ETH_ALEN;

	snprintf(&sgl[i], sizeof(CXGB4_SELFTEST_LB_STR), "%s",
		 CXGB4_SELFTEST_LB_STR);

	init_completion(&lb->completion);
	txq_advance(&q->q, ndesc);
	cxgb4_ring_tx_db(adap, &q->q, ndesc);
	__netif_tx_unlock(q->txq);

	 
	ret = wait_for_completion_timeout(&lb->completion, 10 * HZ);
	if (!ret)
		ret = -ETIMEDOUT;
	else
		ret = lb->result;

	lb->loopback = 0;

	return ret;
}

 
static int ctrl_xmit(struct sge_ctrl_txq *q, struct sk_buff *skb)
{
	unsigned int ndesc;
	struct fw_wr_hdr *wr;

	if (unlikely(!is_imm(skb))) {
		WARN_ON(1);
		dev_kfree_skb(skb);
		return NET_XMIT_DROP;
	}

	ndesc = DIV_ROUND_UP(skb->len, sizeof(struct tx_desc));
	spin_lock(&q->sendq.lock);

	if (unlikely(q->full)) {
		skb->priority = ndesc;                   
		__skb_queue_tail(&q->sendq, skb);
		spin_unlock(&q->sendq.lock);
		return NET_XMIT_CN;
	}

	wr = (struct fw_wr_hdr *)&q->q.desc[q->q.pidx];
	cxgb4_inline_tx_skb(skb, &q->q, wr);

	txq_advance(&q->q, ndesc);
	if (unlikely(txq_avail(&q->q) < TXQ_STOP_THRES))
		ctrlq_check_stop(q, wr);

	cxgb4_ring_tx_db(q->adap, &q->q, ndesc);
	spin_unlock(&q->sendq.lock);

	kfree_skb(skb);
	return NET_XMIT_SUCCESS;
}

 
static void restart_ctrlq(struct tasklet_struct *t)
{
	struct sk_buff *skb;
	unsigned int written = 0;
	struct sge_ctrl_txq *q = from_tasklet(q, t, qresume_tsk);

	spin_lock(&q->sendq.lock);
	reclaim_completed_tx_imm(&q->q);
	BUG_ON(txq_avail(&q->q) < TXQ_STOP_THRES);   

	while ((skb = __skb_dequeue(&q->sendq)) != NULL) {
		struct fw_wr_hdr *wr;
		unsigned int ndesc = skb->priority;      

		written += ndesc;
		 
		wr = (struct fw_wr_hdr *)&q->q.desc[q->q.pidx];
		txq_advance(&q->q, ndesc);
		spin_unlock(&q->sendq.lock);

		cxgb4_inline_tx_skb(skb, &q->q, wr);
		kfree_skb(skb);

		if (unlikely(txq_avail(&q->q) < TXQ_STOP_THRES)) {
			unsigned long old = q->q.stops;

			ctrlq_check_stop(q, wr);
			if (q->q.stops != old) {           
				spin_lock(&q->sendq.lock);
				goto ringdb;
			}
		}
		if (written > 16) {
			cxgb4_ring_tx_db(q->adap, &q->q, written);
			written = 0;
		}
		spin_lock(&q->sendq.lock);
	}
	q->full = 0;
ringdb:
	if (written)
		cxgb4_ring_tx_db(q->adap, &q->q, written);
	spin_unlock(&q->sendq.lock);
}

 
int t4_mgmt_tx(struct adapter *adap, struct sk_buff *skb)
{
	int ret;

	local_bh_disable();
	ret = ctrl_xmit(&adap->sge.ctrlq[0], skb);
	local_bh_enable();
	return ret;
}

 
static inline int is_ofld_imm(const struct sk_buff *skb)
{
	struct work_request_hdr *req = (struct work_request_hdr *)skb->data;
	unsigned long opcode = FW_WR_OP_G(ntohl(req->wr_hi));

	if (unlikely(opcode == FW_ULPTX_WR))
		return skb->len <= MAX_IMM_ULPTX_WR_LEN;
	else if (opcode == FW_CRYPTO_LOOKASIDE_WR)
		return skb->len <= SGE_MAX_WR_LEN;
	else
		return skb->len <= MAX_IMM_OFLD_TX_DATA_WR_LEN;
}

 
static inline unsigned int calc_tx_flits_ofld(const struct sk_buff *skb)
{
	unsigned int flits, cnt;

	if (is_ofld_imm(skb))
		return DIV_ROUND_UP(skb->len, 8);

	flits = skb_transport_offset(skb) / 8U;    
	cnt = skb_shinfo(skb)->nr_frags;
	if (skb_tail_pointer(skb) != skb_transport_header(skb))
		cnt++;
	return flits + sgl_len(cnt);
}

 
static void txq_stop_maperr(struct sge_uld_txq *q)
{
	q->mapping_err++;
	q->q.stops++;
	set_bit(q->q.cntxt_id - q->adap->sge.egr_start,
		q->adap->sge.txq_maperr);
}

 
static void ofldtxq_stop(struct sge_uld_txq *q, struct fw_wr_hdr *wr)
{
	wr->lo |= htonl(FW_WR_EQUEQ_F | FW_WR_EQUIQ_F);
	q->q.stops++;
	q->full = 1;
}

 
static void service_ofldq(struct sge_uld_txq *q)
	__must_hold(&q->sendq.lock)
{
	u64 *pos, *before, *end;
	int credits;
	struct sk_buff *skb;
	struct sge_txq *txq;
	unsigned int left;
	unsigned int written = 0;
	unsigned int flits, ndesc;

	 
	if (q->service_ofldq_running)
		return;
	q->service_ofldq_running = true;

	while ((skb = skb_peek(&q->sendq)) != NULL && !q->full) {
		 
		spin_unlock(&q->sendq.lock);

		cxgb4_reclaim_completed_tx(q->adap, &q->q, false);

		flits = skb->priority;                 
		ndesc = flits_to_desc(flits);
		credits = txq_avail(&q->q) - ndesc;
		BUG_ON(credits < 0);
		if (unlikely(credits < TXQ_STOP_THRES))
			ofldtxq_stop(q, (struct fw_wr_hdr *)skb->data);

		pos = (u64 *)&q->q.desc[q->q.pidx];
		if (is_ofld_imm(skb))
			cxgb4_inline_tx_skb(skb, &q->q, pos);
		else if (cxgb4_map_skb(q->adap->pdev_dev, skb,
				       (dma_addr_t *)skb->head)) {
			txq_stop_maperr(q);
			spin_lock(&q->sendq.lock);
			break;
		} else {
			int last_desc, hdr_len = skb_transport_offset(skb);

			 
			before = (u64 *)pos;
			end = (u64 *)pos + flits;
			txq = &q->q;
			pos = (void *)inline_tx_skb_header(skb, &q->q,
							   (void *)pos,
							   hdr_len);
			if (before > (u64 *)pos) {
				left = (u8 *)end - (u8 *)txq->stat;
				end = (void *)txq->desc + left;
			}

			 
			if (pos == (u64 *)txq->stat) {
				left = (u8 *)end - (u8 *)txq->stat;
				end = (void *)txq->desc + left;
				pos = (void *)txq->desc;
			}

			cxgb4_write_sgl(skb, &q->q, (void *)pos,
					end, hdr_len,
					(dma_addr_t *)skb->head);
#ifdef CONFIG_NEED_DMA_MAP_STATE
			skb->dev = q->adap->port[0];
			skb->destructor = deferred_unmap_destructor;
#endif
			last_desc = q->q.pidx + ndesc - 1;
			if (last_desc >= q->q.size)
				last_desc -= q->q.size;
			q->q.sdesc[last_desc].skb = skb;
		}

		txq_advance(&q->q, ndesc);
		written += ndesc;
		if (unlikely(written > 32)) {
			cxgb4_ring_tx_db(q->adap, &q->q, written);
			written = 0;
		}

		 
		spin_lock(&q->sendq.lock);
		__skb_unlink(skb, &q->sendq);
		if (is_ofld_imm(skb))
			kfree_skb(skb);
	}
	if (likely(written))
		cxgb4_ring_tx_db(q->adap, &q->q, written);

	 
	q->service_ofldq_running = false;
}

 
static int ofld_xmit(struct sge_uld_txq *q, struct sk_buff *skb)
{
	skb->priority = calc_tx_flits_ofld(skb);        
	spin_lock(&q->sendq.lock);

	 
	__skb_queue_tail(&q->sendq, skb);
	if (q->sendq.qlen == 1)
		service_ofldq(q);

	spin_unlock(&q->sendq.lock);
	return NET_XMIT_SUCCESS;
}

 
static void restart_ofldq(struct tasklet_struct *t)
{
	struct sge_uld_txq *q = from_tasklet(q, t, qresume_tsk);

	spin_lock(&q->sendq.lock);
	q->full = 0;             
	service_ofldq(q);
	spin_unlock(&q->sendq.lock);
}

 
static inline unsigned int skb_txq(const struct sk_buff *skb)
{
	return skb->queue_mapping >> 1;
}

 
static inline unsigned int is_ctrl_pkt(const struct sk_buff *skb)
{
	return skb->queue_mapping & 1;
}

static inline int uld_send(struct adapter *adap, struct sk_buff *skb,
			   unsigned int tx_uld_type)
{
	struct sge_uld_txq_info *txq_info;
	struct sge_uld_txq *txq;
	unsigned int idx = skb_txq(skb);

	if (unlikely(is_ctrl_pkt(skb))) {
		 
		if (adap->tids.nsftids)
			idx = 0;
		return ctrl_xmit(&adap->sge.ctrlq[idx], skb);
	}

	txq_info = adap->sge.uld_txq_info[tx_uld_type];
	if (unlikely(!txq_info)) {
		WARN_ON(true);
		kfree_skb(skb);
		return NET_XMIT_DROP;
	}

	txq = &txq_info->uldtxq[idx];
	return ofld_xmit(txq, skb);
}

 
int t4_ofld_send(struct adapter *adap, struct sk_buff *skb)
{
	int ret;

	local_bh_disable();
	ret = uld_send(adap, skb, CXGB4_TX_OFLD);
	local_bh_enable();
	return ret;
}

 
int cxgb4_ofld_send(struct net_device *dev, struct sk_buff *skb)
{
	return t4_ofld_send(netdev2adap(dev), skb);
}
EXPORT_SYMBOL(cxgb4_ofld_send);

static void *inline_tx_header(const void *src,
			      const struct sge_txq *q,
			      void *pos, int length)
{
	int left = (void *)q->stat - pos;
	u64 *p;

	if (likely(length <= left)) {
		memcpy(pos, src, length);
		pos += length;
	} else {
		memcpy(pos, src, left);
		memcpy(q->desc, src + left, length - left);
		pos = (void *)q->desc + (length - left);
	}
	 
	p = PTR_ALIGN(pos, 8);
	if ((uintptr_t)p & 8) {
		*p = 0;
		return p + 1;
	}
	return p;
}

 
static int ofld_xmit_direct(struct sge_uld_txq *q, const void *src,
			    unsigned int len)
{
	unsigned int ndesc;
	int credits;
	u64 *pos;

	 
	if (len > MAX_IMM_OFLD_TX_DATA_WR_LEN) {
		WARN_ON(1);
		return NET_XMIT_DROP;
	}

	 
	if (!spin_trylock(&q->sendq.lock))
		return NET_XMIT_DROP;

	if (q->full || !skb_queue_empty(&q->sendq) ||
	    q->service_ofldq_running) {
		spin_unlock(&q->sendq.lock);
		return NET_XMIT_DROP;
	}
	ndesc = flits_to_desc(DIV_ROUND_UP(len, 8));
	credits = txq_avail(&q->q) - ndesc;
	pos = (u64 *)&q->q.desc[q->q.pidx];

	 
	inline_tx_header(src, &q->q, pos, len);
	if (unlikely(credits < TXQ_STOP_THRES))
		ofldtxq_stop(q, (struct fw_wr_hdr *)pos);
	txq_advance(&q->q, ndesc);
	cxgb4_ring_tx_db(q->adap, &q->q, ndesc);

	spin_unlock(&q->sendq.lock);
	return NET_XMIT_SUCCESS;
}

int cxgb4_immdata_send(struct net_device *dev, unsigned int idx,
		       const void *src, unsigned int len)
{
	struct sge_uld_txq_info *txq_info;
	struct sge_uld_txq *txq;
	struct adapter *adap;
	int ret;

	adap = netdev2adap(dev);

	local_bh_disable();
	txq_info = adap->sge.uld_txq_info[CXGB4_TX_OFLD];
	if (unlikely(!txq_info)) {
		WARN_ON(true);
		local_bh_enable();
		return NET_XMIT_DROP;
	}
	txq = &txq_info->uldtxq[idx];

	ret = ofld_xmit_direct(txq, src, len);
	local_bh_enable();
	return net_xmit_eval(ret);
}
EXPORT_SYMBOL(cxgb4_immdata_send);

 
static int t4_crypto_send(struct adapter *adap, struct sk_buff *skb)
{
	int ret;

	local_bh_disable();
	ret = uld_send(adap, skb, CXGB4_TX_CRYPTO);
	local_bh_enable();
	return ret;
}

 
int cxgb4_crypto_send(struct net_device *dev, struct sk_buff *skb)
{
	return t4_crypto_send(netdev2adap(dev), skb);
}
EXPORT_SYMBOL(cxgb4_crypto_send);

static inline void copy_frags(struct sk_buff *skb,
			      const struct pkt_gl *gl, unsigned int offset)
{
	int i;

	 
	__skb_fill_page_desc(skb, 0, gl->frags[0].page,
			     gl->frags[0].offset + offset,
			     gl->frags[0].size - offset);
	skb_shinfo(skb)->nr_frags = gl->nfrags;
	for (i = 1; i < gl->nfrags; i++)
		__skb_fill_page_desc(skb, i, gl->frags[i].page,
				     gl->frags[i].offset,
				     gl->frags[i].size);

	 
	get_page(gl->frags[gl->nfrags - 1].page);
}

 
struct sk_buff *cxgb4_pktgl_to_skb(const struct pkt_gl *gl,
				   unsigned int skb_len, unsigned int pull_len)
{
	struct sk_buff *skb;

	 
	if (gl->tot_len <= RX_COPY_THRES) {
		skb = dev_alloc_skb(gl->tot_len);
		if (unlikely(!skb))
			goto out;
		__skb_put(skb, gl->tot_len);
		skb_copy_to_linear_data(skb, gl->va, gl->tot_len);
	} else {
		skb = dev_alloc_skb(skb_len);
		if (unlikely(!skb))
			goto out;
		__skb_put(skb, pull_len);
		skb_copy_to_linear_data(skb, gl->va, pull_len);

		copy_frags(skb, gl, pull_len);
		skb->len = gl->tot_len;
		skb->data_len = skb->len - pull_len;
		skb->truesize += skb->data_len;
	}
out:	return skb;
}
EXPORT_SYMBOL(cxgb4_pktgl_to_skb);

 
static void t4_pktgl_free(const struct pkt_gl *gl)
{
	int n;
	const struct page_frag *p;

	for (p = gl->frags, n = gl->nfrags - 1; n--; p++)
		put_page(p->page);
}

 
static noinline int handle_trace_pkt(struct adapter *adap,
				     const struct pkt_gl *gl)
{
	struct sk_buff *skb;

	skb = cxgb4_pktgl_to_skb(gl, RX_PULL_LEN, RX_PULL_LEN);
	if (unlikely(!skb)) {
		t4_pktgl_free(gl);
		return 0;
	}

	if (is_t4(adap->params.chip))
		__skb_pull(skb, sizeof(struct cpl_trace_pkt));
	else
		__skb_pull(skb, sizeof(struct cpl_t5_trace_pkt));

	skb_reset_mac_header(skb);
	skb->protocol = htons(0xffff);
	skb->dev = adap->port[0];
	netif_receive_skb(skb);
	return 0;
}

 
static void cxgb4_sgetim_to_hwtstamp(struct adapter *adap,
				     struct skb_shared_hwtstamps *hwtstamps,
				     u64 sgetstamp)
{
	u64 ns;
	u64 tmp = (sgetstamp * 1000 * 1000 + adap->params.vpd.cclk / 2);

	ns = div_u64(tmp, adap->params.vpd.cclk);

	memset(hwtstamps, 0, sizeof(*hwtstamps));
	hwtstamps->hwtstamp = ns_to_ktime(ns);
}

static void do_gro(struct sge_eth_rxq *rxq, const struct pkt_gl *gl,
		   const struct cpl_rx_pkt *pkt, unsigned long tnl_hdr_len)
{
	struct adapter *adapter = rxq->rspq.adap;
	struct sge *s = &adapter->sge;
	struct port_info *pi;
	int ret;
	struct sk_buff *skb;

	skb = napi_get_frags(&rxq->rspq.napi);
	if (unlikely(!skb)) {
		t4_pktgl_free(gl);
		rxq->stats.rx_drops++;
		return;
	}

	copy_frags(skb, gl, s->pktshift);
	if (tnl_hdr_len)
		skb->csum_level = 1;
	skb->len = gl->tot_len - s->pktshift;
	skb->data_len = skb->len;
	skb->truesize += skb->data_len;
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	skb_record_rx_queue(skb, rxq->rspq.idx);
	pi = netdev_priv(skb->dev);
	if (pi->rxtstamp)
		cxgb4_sgetim_to_hwtstamp(adapter, skb_hwtstamps(skb),
					 gl->sgetstamp);
	if (rxq->rspq.netdev->features & NETIF_F_RXHASH)
		skb_set_hash(skb, (__force u32)pkt->rsshdr.hash_val,
			     PKT_HASH_TYPE_L3);

	if (unlikely(pkt->vlan_ex)) {
		__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), ntohs(pkt->vlan));
		rxq->stats.vlan_ex++;
	}
	ret = napi_gro_frags(&rxq->rspq.napi);
	if (ret == GRO_HELD)
		rxq->stats.lro_pkts++;
	else if (ret == GRO_MERGED || ret == GRO_MERGED_FREE)
		rxq->stats.lro_merged++;
	rxq->stats.pkts++;
	rxq->stats.rx_cso++;
}

enum {
	RX_NON_PTP_PKT = 0,
	RX_PTP_PKT_SUC = 1,
	RX_PTP_PKT_ERR = 2
};

 
static noinline int t4_systim_to_hwstamp(struct adapter *adapter,
					 struct sk_buff *skb)
{
	struct skb_shared_hwtstamps *hwtstamps;
	struct cpl_rx_mps_pkt *cpl = NULL;
	unsigned char *data;
	int offset;

	cpl = (struct cpl_rx_mps_pkt *)skb->data;
	if (!(CPL_RX_MPS_PKT_TYPE_G(ntohl(cpl->op_to_r1_hi)) &
	     X_CPL_RX_MPS_PKT_TYPE_PTP))
		return RX_PTP_PKT_ERR;

	data = skb->data + sizeof(*cpl);
	skb_pull(skb, 2 * sizeof(u64) + sizeof(struct cpl_rx_mps_pkt));
	offset = ETH_HLEN + IPV4_HLEN(skb->data) + UDP_HLEN;
	if (skb->len < offset + OFF_PTP_SEQUENCE_ID + sizeof(short))
		return RX_PTP_PKT_ERR;

	hwtstamps = skb_hwtstamps(skb);
	memset(hwtstamps, 0, sizeof(*hwtstamps));
	hwtstamps->hwtstamp = ns_to_ktime(get_unaligned_be64(data));

	return RX_PTP_PKT_SUC;
}

 
static int t4_rx_hststamp(struct adapter *adapter, const __be64 *rsp,
			  struct sge_eth_rxq *rxq, struct sk_buff *skb)
{
	int ret;

	if (unlikely((*(u8 *)rsp == CPL_RX_MPS_PKT) &&
		     !is_t4(adapter->params.chip))) {
		ret = t4_systim_to_hwstamp(adapter, skb);
		if (ret == RX_PTP_PKT_ERR) {
			kfree_skb(skb);
			rxq->stats.rx_drops++;
		}
		return ret;
	}
	return RX_NON_PTP_PKT;
}

 
static int t4_tx_hststamp(struct adapter *adapter, struct sk_buff *skb,
			  struct net_device *dev)
{
	struct port_info *pi = netdev_priv(dev);

	if (!is_t4(adapter->params.chip) && adapter->ptp_tx_skb) {
		cxgb4_ptp_read_hwstamp(adapter, pi);
		kfree_skb(skb);
		return 0;
	}
	return 1;
}

 
static void t4_tx_completion_handler(struct sge_rspq *rspq,
				     const __be64 *rsp,
				     const struct pkt_gl *gl)
{
	u8 opcode = ((const struct rss_header *)rsp)->opcode;
	struct port_info *pi = netdev_priv(rspq->netdev);
	struct adapter *adapter = rspq->adap;
	struct sge *s = &adapter->sge;
	struct sge_eth_txq *txq;

	 
	rsp++;

	 
	if (unlikely(opcode == CPL_FW4_MSG &&
		     ((const struct cpl_fw4_msg *)rsp)->type ==
							FW_TYPE_RSSCPL)) {
		rsp++;
		opcode = ((const struct rss_header *)rsp)->opcode;
		rsp++;
	}

	if (unlikely(opcode != CPL_SGE_EGR_UPDATE)) {
		pr_info("%s: unexpected FW4/CPL %#x on Rx queue\n",
			__func__, opcode);
		return;
	}

	txq = &s->ethtxq[pi->first_qset + rspq->idx];

	 
	if (CHELSIO_CHIP_VERSION(adapter->params.chip) <= CHELSIO_T5) {
		struct cpl_sge_egr_update *egr;

		egr = (struct cpl_sge_egr_update *)rsp;
		WRITE_ONCE(txq->q.stat->cidx, egr->cidx);
	}

	t4_sge_eth_txq_egress_update(adapter, txq, -1);
}

static int cxgb4_validate_lb_pkt(struct port_info *pi, const struct pkt_gl *si)
{
	struct adapter *adap = pi->adapter;
	struct cxgb4_ethtool_lb_test *lb;
	struct sge *s = &adap->sge;
	struct net_device *netdev;
	u8 *data;
	int i;

	netdev = adap->port[pi->port_id];
	lb = &pi->ethtool_lb;
	data = si->va + s->pktshift;

	i = ETH_ALEN;
	if (!ether_addr_equal(data + i, netdev->dev_addr))
		return -1;

	i += ETH_ALEN;
	if (strcmp(&data[i], CXGB4_SELFTEST_LB_STR))
		lb->result = -EIO;

	complete(&lb->completion);
	return 0;
}

 
int t4_ethrx_handler(struct sge_rspq *q, const __be64 *rsp,
		     const struct pkt_gl *si)
{
	bool csum_ok;
	struct sk_buff *skb;
	const struct cpl_rx_pkt *pkt;
	struct sge_eth_rxq *rxq = container_of(q, struct sge_eth_rxq, rspq);
	struct adapter *adapter = q->adap;
	struct sge *s = &q->adap->sge;
	int cpl_trace_pkt = is_t4(q->adap->params.chip) ?
			    CPL_TRACE_PKT : CPL_TRACE_PKT_T5;
	u16 err_vec, tnl_hdr_len = 0;
	struct port_info *pi;
	int ret = 0;

	pi = netdev_priv(q->netdev);
	 
	if (unlikely((*(u8 *)rsp == CPL_FW4_MSG) ||
		     (*(u8 *)rsp == CPL_SGE_EGR_UPDATE))) {
		t4_tx_completion_handler(q, rsp, si);
		return 0;
	}

	if (unlikely(*(u8 *)rsp == cpl_trace_pkt))
		return handle_trace_pkt(q->adap, si);

	pkt = (const struct cpl_rx_pkt *)rsp;
	 
	if (q->adap->params.tp.rx_pkt_encap) {
		err_vec = T6_COMPR_RXERR_VEC_G(be16_to_cpu(pkt->err_vec));
		tnl_hdr_len = T6_RX_TNLHDR_LEN_G(ntohs(pkt->err_vec));
	} else {
		err_vec = be16_to_cpu(pkt->err_vec);
	}

	csum_ok = pkt->csum_calc && !err_vec &&
		  (q->netdev->features & NETIF_F_RXCSUM);

	if (err_vec)
		rxq->stats.bad_rx_pkts++;

	if (unlikely(pi->ethtool_lb.loopback && pkt->iff >= NCHAN)) {
		ret = cxgb4_validate_lb_pkt(pi, si);
		if (!ret)
			return 0;
	}

	if (((pkt->l2info & htonl(RXF_TCP_F)) ||
	     tnl_hdr_len) &&
	    (q->netdev->features & NETIF_F_GRO) && csum_ok && !pkt->ip_frag) {
		do_gro(rxq, si, pkt, tnl_hdr_len);
		return 0;
	}

	skb = cxgb4_pktgl_to_skb(si, RX_PKT_SKB_LEN, RX_PULL_LEN);
	if (unlikely(!skb)) {
		t4_pktgl_free(si);
		rxq->stats.rx_drops++;
		return 0;
	}

	 
	if (unlikely(pi->ptp_enable)) {
		ret = t4_rx_hststamp(adapter, rsp, rxq, skb);
		if (ret == RX_PTP_PKT_ERR)
			return 0;
	}
	if (likely(!ret))
		__skb_pull(skb, s->pktshift);  

	 
	if (unlikely(pi->ptp_enable && !ret &&
		     (pkt->l2info & htonl(RXF_UDP_F)) &&
		     cxgb4_ptp_is_ptp_rx(skb))) {
		if (!t4_tx_hststamp(adapter, skb, q->netdev))
			return 0;
	}

	skb->protocol = eth_type_trans(skb, q->netdev);
	skb_record_rx_queue(skb, q->idx);
	if (skb->dev->features & NETIF_F_RXHASH)
		skb_set_hash(skb, (__force u32)pkt->rsshdr.hash_val,
			     PKT_HASH_TYPE_L3);

	rxq->stats.pkts++;

	if (pi->rxtstamp)
		cxgb4_sgetim_to_hwtstamp(q->adap, skb_hwtstamps(skb),
					 si->sgetstamp);
	if (csum_ok && (pkt->l2info & htonl(RXF_UDP_F | RXF_TCP_F))) {
		if (!pkt->ip_frag) {
			skb->ip_summed = CHECKSUM_UNNECESSARY;
			rxq->stats.rx_cso++;
		} else if (pkt->l2info & htonl(RXF_IP_F)) {
			__sum16 c = (__force __sum16)pkt->csum;
			skb->csum = csum_unfold(c);

			if (tnl_hdr_len) {
				skb->ip_summed = CHECKSUM_UNNECESSARY;
				skb->csum_level = 1;
			} else {
				skb->ip_summed = CHECKSUM_COMPLETE;
			}
			rxq->stats.rx_cso++;
		}
	} else {
		skb_checksum_none_assert(skb);
#ifdef CONFIG_CHELSIO_T4_FCOE
#define CPL_RX_PKT_FLAGS (RXF_PSH_F | RXF_SYN_F | RXF_UDP_F | \
			  RXF_TCP_F | RXF_IP_F | RXF_IP6_F | RXF_LRO_F)

		if (!(pkt->l2info & cpu_to_be32(CPL_RX_PKT_FLAGS))) {
			if ((pkt->l2info & cpu_to_be32(RXF_FCOE_F)) &&
			    (pi->fcoe.flags & CXGB_FCOE_ENABLED)) {
				if (q->adap->params.tp.rx_pkt_encap)
					csum_ok = err_vec &
						  T6_COMPR_RXERR_SUM_F;
				else
					csum_ok = err_vec & RXERR_CSUM_F;
				if (!csum_ok)
					skb->ip_summed = CHECKSUM_UNNECESSARY;
			}
		}

#undef CPL_RX_PKT_FLAGS
#endif  
	}

	if (unlikely(pkt->vlan_ex)) {
		__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), ntohs(pkt->vlan));
		rxq->stats.vlan_ex++;
	}
	skb_mark_napi_id(skb, &q->napi);
	netif_receive_skb(skb);
	return 0;
}

 
static void restore_rx_bufs(const struct pkt_gl *si, struct sge_fl *q,
			    int frags)
{
	struct rx_sw_desc *d;

	while (frags--) {
		if (q->cidx == 0)
			q->cidx = q->size - 1;
		else
			q->cidx--;
		d = &q->sdesc[q->cidx];
		d->page = si->frags[frags].page;
		d->dma_addr |= RX_UNMAPPED_BUF;
		q->avail++;
	}
}

 
static inline bool is_new_response(const struct rsp_ctrl *r,
				   const struct sge_rspq *q)
{
	return (r->type_gen >> RSPD_GEN_S) == q->gen;
}

 
static inline void rspq_next(struct sge_rspq *q)
{
	q->cur_desc = (void *)q->cur_desc + q->iqe_len;
	if (unlikely(++q->cidx == q->size)) {
		q->cidx = 0;
		q->gen ^= 1;
		q->cur_desc = q->desc;
	}
}

 
static int process_responses(struct sge_rspq *q, int budget)
{
	int ret, rsp_type;
	int budget_left = budget;
	const struct rsp_ctrl *rc;
	struct sge_eth_rxq *rxq = container_of(q, struct sge_eth_rxq, rspq);
	struct adapter *adapter = q->adap;
	struct sge *s = &adapter->sge;

	while (likely(budget_left)) {
		rc = (void *)q->cur_desc + (q->iqe_len - sizeof(*rc));
		if (!is_new_response(rc, q)) {
			if (q->flush_handler)
				q->flush_handler(q);
			break;
		}

		dma_rmb();
		rsp_type = RSPD_TYPE_G(rc->type_gen);
		if (likely(rsp_type == RSPD_TYPE_FLBUF_X)) {
			struct page_frag *fp;
			struct pkt_gl si;
			const struct rx_sw_desc *rsd;
			u32 len = ntohl(rc->pldbuflen_qid), bufsz, frags;

			if (len & RSPD_NEWBUF_F) {
				if (likely(q->offset > 0)) {
					free_rx_bufs(q->adap, &rxq->fl, 1);
					q->offset = 0;
				}
				len = RSPD_LEN_G(len);
			}
			si.tot_len = len;

			 
			for (frags = 0, fp = si.frags; ; frags++, fp++) {
				rsd = &rxq->fl.sdesc[rxq->fl.cidx];
				bufsz = get_buf_size(adapter, rsd);
				fp->page = rsd->page;
				fp->offset = q->offset;
				fp->size = min(bufsz, len);
				len -= fp->size;
				if (!len)
					break;
				unmap_rx_buf(q->adap, &rxq->fl);
			}

			si.sgetstamp = SGE_TIMESTAMP_G(
					be64_to_cpu(rc->last_flit));
			 
			dma_sync_single_for_cpu(q->adap->pdev_dev,
						get_buf_addr(rsd),
						fp->size, DMA_FROM_DEVICE);

			si.va = page_address(si.frags[0].page) +
				si.frags[0].offset;
			prefetch(si.va);

			si.nfrags = frags + 1;
			ret = q->handler(q, q->cur_desc, &si);
			if (likely(ret == 0))
				q->offset += ALIGN(fp->size, s->fl_align);
			else
				restore_rx_bufs(&si, &rxq->fl, frags);
		} else if (likely(rsp_type == RSPD_TYPE_CPL_X)) {
			ret = q->handler(q, q->cur_desc, NULL);
		} else {
			ret = q->handler(q, (const __be64 *)rc, CXGB4_MSG_AN);
		}

		if (unlikely(ret)) {
			 
			q->next_intr_params = QINTR_TIMER_IDX_V(NOMEM_TMR_IDX);
			break;
		}

		rspq_next(q);
		budget_left--;
	}

	if (q->offset >= 0 && fl_cap(&rxq->fl) - rxq->fl.avail >= 16)
		__refill_fl(q->adap, &rxq->fl);
	return budget - budget_left;
}

 
static int napi_rx_handler(struct napi_struct *napi, int budget)
{
	unsigned int params;
	struct sge_rspq *q = container_of(napi, struct sge_rspq, napi);
	int work_done;
	u32 val;

	work_done = process_responses(q, budget);
	if (likely(work_done < budget)) {
		int timer_index;

		napi_complete_done(napi, work_done);
		timer_index = QINTR_TIMER_IDX_G(q->next_intr_params);

		if (q->adaptive_rx) {
			if (work_done > max(timer_pkt_quota[timer_index],
					    MIN_NAPI_WORK))
				timer_index = (timer_index + 1);
			else
				timer_index = timer_index - 1;

			timer_index = clamp(timer_index, 0, SGE_TIMERREGS - 1);
			q->next_intr_params =
					QINTR_TIMER_IDX_V(timer_index) |
					QINTR_CNT_EN_V(0);
			params = q->next_intr_params;
		} else {
			params = q->next_intr_params;
			q->next_intr_params = q->intr_params;
		}
	} else
		params = QINTR_TIMER_IDX_V(7);

	val = CIDXINC_V(work_done) | SEINTARM_V(params);

	 
	if (unlikely(q->bar2_addr == NULL)) {
		t4_write_reg(q->adap, MYPF_REG(SGE_PF_GTS_A),
			     val | INGRESSQID_V((u32)q->cntxt_id));
	} else {
		writel(val | INGRESSQID_V(q->bar2_qid),
		       q->bar2_addr + SGE_UDB_GTS);
		wmb();
	}
	return work_done;
}

void cxgb4_ethofld_restart(struct tasklet_struct *t)
{
	struct sge_eosw_txq *eosw_txq = from_tasklet(eosw_txq, t,
						     qresume_tsk);
	int pktcount;

	spin_lock(&eosw_txq->lock);
	pktcount = eosw_txq->cidx - eosw_txq->last_cidx;
	if (pktcount < 0)
		pktcount += eosw_txq->ndesc;

	if (pktcount) {
		cxgb4_eosw_txq_free_desc(netdev2adap(eosw_txq->netdev),
					 eosw_txq, pktcount);
		eosw_txq->inuse -= pktcount;
	}

	 
	ethofld_xmit(eosw_txq->netdev, eosw_txq);
	spin_unlock(&eosw_txq->lock);
}

 
int cxgb4_ethofld_rx_handler(struct sge_rspq *q, const __be64 *rsp,
			     const struct pkt_gl *si)
{
	u8 opcode = ((const struct rss_header *)rsp)->opcode;

	 
	rsp++;

	if (opcode == CPL_FW4_ACK) {
		const struct cpl_fw4_ack *cpl;
		struct sge_eosw_txq *eosw_txq;
		struct eotid_entry *entry;
		struct sk_buff *skb;
		u32 hdr_len, eotid;
		u8 flits, wrlen16;
		int credits;

		cpl = (const struct cpl_fw4_ack *)rsp;
		eotid = CPL_FW4_ACK_FLOWID_G(ntohl(OPCODE_TID(cpl))) -
			q->adap->tids.eotid_base;
		entry = cxgb4_lookup_eotid(&q->adap->tids, eotid);
		if (!entry)
			goto out_done;

		eosw_txq = (struct sge_eosw_txq *)entry->data;
		if (!eosw_txq)
			goto out_done;

		spin_lock(&eosw_txq->lock);
		credits = cpl->credits;
		while (credits > 0) {
			skb = eosw_txq->desc[eosw_txq->cidx].skb;
			if (!skb)
				break;

			if (unlikely((eosw_txq->state ==
				      CXGB4_EO_STATE_FLOWC_OPEN_REPLY ||
				      eosw_txq->state ==
				      CXGB4_EO_STATE_FLOWC_CLOSE_REPLY) &&
				     eosw_txq->cidx == eosw_txq->flowc_idx)) {
				flits = DIV_ROUND_UP(skb->len, 8);
				if (eosw_txq->state ==
				    CXGB4_EO_STATE_FLOWC_OPEN_REPLY)
					eosw_txq->state = CXGB4_EO_STATE_ACTIVE;
				else
					eosw_txq->state = CXGB4_EO_STATE_CLOSED;
				complete(&eosw_txq->completion);
			} else {
				hdr_len = eth_get_headlen(eosw_txq->netdev,
							  skb->data,
							  skb_headlen(skb));
				flits = ethofld_calc_tx_flits(q->adap, skb,
							      hdr_len);
			}
			eosw_txq_advance_index(&eosw_txq->cidx, 1,
					       eosw_txq->ndesc);
			wrlen16 = DIV_ROUND_UP(flits * 8, 16);
			credits -= wrlen16;
		}

		eosw_txq->cred += cpl->credits;
		eosw_txq->ncompl--;

		spin_unlock(&eosw_txq->lock);

		 
		tasklet_schedule(&eosw_txq->qresume_tsk);
	}

out_done:
	return 0;
}

 
irqreturn_t t4_sge_intr_msix(int irq, void *cookie)
{
	struct sge_rspq *q = cookie;

	napi_schedule(&q->napi);
	return IRQ_HANDLED;
}

 
static unsigned int process_intrq(struct adapter *adap)
{
	unsigned int credits;
	const struct rsp_ctrl *rc;
	struct sge_rspq *q = &adap->sge.intrq;
	u32 val;

	spin_lock(&adap->sge.intrq_lock);
	for (credits = 0; ; credits++) {
		rc = (void *)q->cur_desc + (q->iqe_len - sizeof(*rc));
		if (!is_new_response(rc, q))
			break;

		dma_rmb();
		if (RSPD_TYPE_G(rc->type_gen) == RSPD_TYPE_INTR_X) {
			unsigned int qid = ntohl(rc->pldbuflen_qid);

			qid -= adap->sge.ingr_start;
			napi_schedule(&adap->sge.ingr_map[qid]->napi);
		}

		rspq_next(q);
	}

	val =  CIDXINC_V(credits) | SEINTARM_V(q->intr_params);

	 
	if (unlikely(q->bar2_addr == NULL)) {
		t4_write_reg(adap, MYPF_REG(SGE_PF_GTS_A),
			     val | INGRESSQID_V(q->cntxt_id));
	} else {
		writel(val | INGRESSQID_V(q->bar2_qid),
		       q->bar2_addr + SGE_UDB_GTS);
		wmb();
	}
	spin_unlock(&adap->sge.intrq_lock);
	return credits;
}

 
static irqreturn_t t4_intr_msi(int irq, void *cookie)
{
	struct adapter *adap = cookie;

	if (adap->flags & CXGB4_MASTER_PF)
		t4_slow_intr_handler(adap);
	process_intrq(adap);
	return IRQ_HANDLED;
}

 
static irqreturn_t t4_intr_intx(int irq, void *cookie)
{
	struct adapter *adap = cookie;

	t4_write_reg(adap, MYPF_REG(PCIE_PF_CLI_A), 0);
	if (((adap->flags & CXGB4_MASTER_PF) && t4_slow_intr_handler(adap)) |
	    process_intrq(adap))
		return IRQ_HANDLED;
	return IRQ_NONE;              
}

 
irq_handler_t t4_intr_handler(struct adapter *adap)
{
	if (adap->flags & CXGB4_USING_MSIX)
		return t4_sge_intr_msix;
	if (adap->flags & CXGB4_USING_MSI)
		return t4_intr_msi;
	return t4_intr_intx;
}

static void sge_rx_timer_cb(struct timer_list *t)
{
	unsigned long m;
	unsigned int i;
	struct adapter *adap = from_timer(adap, t, sge.rx_timer);
	struct sge *s = &adap->sge;

	for (i = 0; i < BITS_TO_LONGS(s->egr_sz); i++)
		for (m = s->starving_fl[i]; m; m &= m - 1) {
			struct sge_eth_rxq *rxq;
			unsigned int id = __ffs(m) + i * BITS_PER_LONG;
			struct sge_fl *fl = s->egr_map[id];

			clear_bit(id, s->starving_fl);
			smp_mb__after_atomic();

			if (fl_starving(adap, fl)) {
				rxq = container_of(fl, struct sge_eth_rxq, fl);
				if (napi_reschedule(&rxq->rspq.napi))
					fl->starving++;
				else
					set_bit(id, s->starving_fl);
			}
		}
	 
	if (!(adap->flags & CXGB4_MASTER_PF))
		goto done;

	t4_idma_monitor(adap, &s->idma_monitor, HZ, RX_QCHECK_PERIOD);

done:
	mod_timer(&s->rx_timer, jiffies + RX_QCHECK_PERIOD);
}

static void sge_tx_timer_cb(struct timer_list *t)
{
	struct adapter *adap = from_timer(adap, t, sge.tx_timer);
	struct sge *s = &adap->sge;
	unsigned long m, period;
	unsigned int i, budget;

	for (i = 0; i < BITS_TO_LONGS(s->egr_sz); i++)
		for (m = s->txq_maperr[i]; m; m &= m - 1) {
			unsigned long id = __ffs(m) + i * BITS_PER_LONG;
			struct sge_uld_txq *txq = s->egr_map[id];

			clear_bit(id, s->txq_maperr);
			tasklet_schedule(&txq->qresume_tsk);
		}

	if (!is_t4(adap->params.chip)) {
		struct sge_eth_txq *q = &s->ptptxq;
		int avail;

		spin_lock(&adap->ptp_lock);
		avail = reclaimable(&q->q);

		if (avail) {
			free_tx_desc(adap, &q->q, avail, false);
			q->q.in_use -= avail;
		}
		spin_unlock(&adap->ptp_lock);
	}

	budget = MAX_TIMER_TX_RECLAIM;
	i = s->ethtxq_rover;
	do {
		budget -= t4_sge_eth_txq_egress_update(adap, &s->ethtxq[i],
						       budget);
		if (!budget)
			break;

		if (++i >= s->ethqsets)
			i = 0;
	} while (i != s->ethtxq_rover);
	s->ethtxq_rover = i;

	if (budget == 0) {
		 
		period = 2;
	} else {
		 
		period = TX_QCHECK_PERIOD;
	}

	mod_timer(&s->tx_timer, jiffies + period);
}

 
static void __iomem *bar2_address(struct adapter *adapter,
				  unsigned int qid,
				  enum t4_bar2_qtype qtype,
				  unsigned int *pbar2_qid)
{
	u64 bar2_qoffset;
	int ret;

	ret = t4_bar2_sge_qregs(adapter, qid, qtype, 0,
				&bar2_qoffset, pbar2_qid);
	if (ret)
		return NULL;

	return adapter->bar2 + bar2_qoffset;
}

 
int t4_sge_alloc_rxq(struct adapter *adap, struct sge_rspq *iq, bool fwevtq,
		     struct net_device *dev, int intr_idx,
		     struct sge_fl *fl, rspq_handler_t hnd,
		     rspq_flush_handler_t flush_hnd, int cong)
{
	int ret, flsz = 0;
	struct fw_iq_cmd c;
	struct sge *s = &adap->sge;
	struct port_info *pi = netdev_priv(dev);
	int relaxed = !(adap->flags & CXGB4_ROOT_NO_RELAXED_ORDERING);

	 
	iq->size = roundup(iq->size, 16);

	iq->desc = alloc_ring(adap->pdev_dev, iq->size, iq->iqe_len, 0,
			      &iq->phys_addr, NULL, 0,
			      dev_to_node(adap->pdev_dev));
	if (!iq->desc)
		return -ENOMEM;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = htonl(FW_CMD_OP_V(FW_IQ_CMD) | FW_CMD_REQUEST_F |
			    FW_CMD_WRITE_F | FW_CMD_EXEC_F |
			    FW_IQ_CMD_PFN_V(adap->pf) | FW_IQ_CMD_VFN_V(0));
	c.alloc_to_len16 = htonl(FW_IQ_CMD_ALLOC_F | FW_IQ_CMD_IQSTART_F |
				 FW_LEN16(c));
	c.type_to_iqandstindex = htonl(FW_IQ_CMD_TYPE_V(FW_IQ_TYPE_FL_INT_CAP) |
		FW_IQ_CMD_IQASYNCH_V(fwevtq) | FW_IQ_CMD_VIID_V(pi->viid) |
		FW_IQ_CMD_IQANDST_V(intr_idx < 0) |
		FW_IQ_CMD_IQANUD_V(UPDATEDELIVERY_INTERRUPT_X) |
		FW_IQ_CMD_IQANDSTINDEX_V(intr_idx >= 0 ? intr_idx :
							-intr_idx - 1));
	c.iqdroprss_to_iqesize = htons(FW_IQ_CMD_IQPCIECH_V(pi->tx_chan) |
		FW_IQ_CMD_IQGTSMODE_F |
		FW_IQ_CMD_IQINTCNTTHRESH_V(iq->pktcnt_idx) |
		FW_IQ_CMD_IQESIZE_V(ilog2(iq->iqe_len) - 4));
	c.iqsize = htons(iq->size);
	c.iqaddr = cpu_to_be64(iq->phys_addr);
	if (cong >= 0)
		c.iqns_to_fl0congen = htonl(FW_IQ_CMD_IQFLINTCONGEN_F |
				FW_IQ_CMD_IQTYPE_V(cong ? FW_IQ_IQTYPE_NIC
							:  FW_IQ_IQTYPE_OFLD));

	if (fl) {
		unsigned int chip_ver =
			CHELSIO_CHIP_VERSION(adap->params.chip);

		 
		if (fl->size < s->fl_starve_thres - 1 + 2 * 8)
			fl->size = s->fl_starve_thres - 1 + 2 * 8;
		fl->size = roundup(fl->size, 8);
		fl->desc = alloc_ring(adap->pdev_dev, fl->size, sizeof(__be64),
				      sizeof(struct rx_sw_desc), &fl->addr,
				      &fl->sdesc, s->stat_len,
				      dev_to_node(adap->pdev_dev));
		if (!fl->desc)
			goto fl_nomem;

		flsz = fl->size / 8 + s->stat_len / sizeof(struct tx_desc);
		c.iqns_to_fl0congen |= htonl(FW_IQ_CMD_FL0PACKEN_F |
					     FW_IQ_CMD_FL0FETCHRO_V(relaxed) |
					     FW_IQ_CMD_FL0DATARO_V(relaxed) |
					     FW_IQ_CMD_FL0PADEN_F);
		if (cong >= 0)
			c.iqns_to_fl0congen |=
				htonl(FW_IQ_CMD_FL0CNGCHMAP_V(cong) |
				      FW_IQ_CMD_FL0CONGCIF_F |
				      FW_IQ_CMD_FL0CONGEN_F);
		 
		c.fl0dcaen_to_fl0cidxfthresh =
			htons(FW_IQ_CMD_FL0FBMIN_V(chip_ver <= CHELSIO_T5 ?
						   FETCHBURSTMIN_128B_X :
						   FETCHBURSTMIN_64B_T6_X) |
			      FW_IQ_CMD_FL0FBMAX_V((chip_ver <= CHELSIO_T5) ?
						   FETCHBURSTMAX_512B_X :
						   FETCHBURSTMAX_256B_X));
		c.fl0size = htons(flsz);
		c.fl0addr = cpu_to_be64(fl->addr);
	}

	ret = t4_wr_mbox(adap, adap->mbox, &c, sizeof(c), &c);
	if (ret)
		goto err;

	netif_napi_add(dev, &iq->napi, napi_rx_handler);
	iq->cur_desc = iq->desc;
	iq->cidx = 0;
	iq->gen = 1;
	iq->next_intr_params = iq->intr_params;
	iq->cntxt_id = ntohs(c.iqid);
	iq->abs_id = ntohs(c.physiqid);
	iq->bar2_addr = bar2_address(adap,
				     iq->cntxt_id,
				     T4_BAR2_QTYPE_INGRESS,
				     &iq->bar2_qid);
	iq->size--;                            
	iq->netdev = dev;
	iq->handler = hnd;
	iq->flush_handler = flush_hnd;

	memset(&iq->lro_mgr, 0, sizeof(struct t4_lro_mgr));
	skb_queue_head_init(&iq->lro_mgr.lroq);

	 
	iq->offset = fl ? 0 : -1;

	adap->sge.ingr_map[iq->cntxt_id - adap->sge.ingr_start] = iq;

	if (fl) {
		fl->cntxt_id = ntohs(c.fl0id);
		fl->avail = fl->pend_cred = 0;
		fl->pidx = fl->cidx = 0;
		fl->alloc_failed = fl->large_alloc_failed = fl->starving = 0;
		adap->sge.egr_map[fl->cntxt_id - adap->sge.egr_start] = fl;

		 
		fl->bar2_addr = bar2_address(adap,
					     fl->cntxt_id,
					     T4_BAR2_QTYPE_EGRESS,
					     &fl->bar2_qid);
		refill_fl(adap, fl, fl_cap(fl), GFP_KERNEL);
	}

	 
	if (!is_t4(adap->params.chip) && cong >= 0) {
		u32 param, val, ch_map = 0;
		int i;
		u16 cng_ch_bits_log = adap->params.arch.cng_ch_bits_log;

		param = (FW_PARAMS_MNEM_V(FW_PARAMS_MNEM_DMAQ) |
			 FW_PARAMS_PARAM_X_V(FW_PARAMS_PARAM_DMAQ_CONM_CTXT) |
			 FW_PARAMS_PARAM_YZ_V(iq->cntxt_id));
		if (cong == 0) {
			val = CONMCTXT_CNGTPMODE_V(CONMCTXT_CNGTPMODE_QUEUE_X);
		} else {
			val =
			    CONMCTXT_CNGTPMODE_V(CONMCTXT_CNGTPMODE_CHANNEL_X);
			for (i = 0; i < 4; i++) {
				if (cong & (1 << i))
					ch_map |= 1 << (i << cng_ch_bits_log);
			}
			val |= CONMCTXT_CNGCHMAP_V(ch_map);
		}
		ret = t4_set_params(adap, adap->mbox, adap->pf, 0, 1,
				    &param, &val);
		if (ret)
			dev_warn(adap->pdev_dev, "Failed to set Congestion"
				 " Manager Context for Ingress Queue %d: %d\n",
				 iq->cntxt_id, -ret);
	}

	return 0;

fl_nomem:
	ret = -ENOMEM;
err:
	if (iq->desc) {
		dma_free_coherent(adap->pdev_dev, iq->size * iq->iqe_len,
				  iq->desc, iq->phys_addr);
		iq->desc = NULL;
	}
	if (fl && fl->desc) {
		kfree(fl->sdesc);
		fl->sdesc = NULL;
		dma_free_coherent(adap->pdev_dev, flsz * sizeof(struct tx_desc),
				  fl->desc, fl->addr);
		fl->desc = NULL;
	}
	return ret;
}

static void init_txq(struct adapter *adap, struct sge_txq *q, unsigned int id)
{
	q->cntxt_id = id;
	q->bar2_addr = bar2_address(adap,
				    q->cntxt_id,
				    T4_BAR2_QTYPE_EGRESS,
				    &q->bar2_qid);
	q->in_use = 0;
	q->cidx = q->pidx = 0;
	q->stops = q->restarts = 0;
	q->stat = (void *)&q->desc[q->size];
	spin_lock_init(&q->db_lock);
	adap->sge.egr_map[id - adap->sge.egr_start] = q;
}

 
int t4_sge_alloc_eth_txq(struct adapter *adap, struct sge_eth_txq *txq,
			 struct net_device *dev, struct netdev_queue *netdevq,
			 unsigned int iqid, u8 dbqt)
{
	unsigned int chip_ver = CHELSIO_CHIP_VERSION(adap->params.chip);
	struct port_info *pi = netdev_priv(dev);
	struct sge *s = &adap->sge;
	struct fw_eq_eth_cmd c;
	int ret, nentries;

	 
	nentries = txq->q.size + s->stat_len / sizeof(struct tx_desc);

	txq->q.desc = alloc_ring(adap->pdev_dev, txq->q.size,
			sizeof(struct tx_desc), sizeof(struct tx_sw_desc),
			&txq->q.phys_addr, &txq->q.sdesc, s->stat_len,
			netdev_queue_numa_node_read(netdevq));
	if (!txq->q.desc)
		return -ENOMEM;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = htonl(FW_CMD_OP_V(FW_EQ_ETH_CMD) | FW_CMD_REQUEST_F |
			    FW_CMD_WRITE_F | FW_CMD_EXEC_F |
			    FW_EQ_ETH_CMD_PFN_V(adap->pf) |
			    FW_EQ_ETH_CMD_VFN_V(0));
	c.alloc_to_len16 = htonl(FW_EQ_ETH_CMD_ALLOC_F |
				 FW_EQ_ETH_CMD_EQSTART_F | FW_LEN16(c));

	 
	c.autoequiqe_to_viid = htonl(((chip_ver <= CHELSIO_T5) ?
				      FW_EQ_ETH_CMD_AUTOEQUIQE_F :
				      FW_EQ_ETH_CMD_AUTOEQUEQE_F) |
				     FW_EQ_ETH_CMD_VIID_V(pi->viid));

	c.fetchszm_to_iqid =
		htonl(FW_EQ_ETH_CMD_HOSTFCMODE_V((chip_ver <= CHELSIO_T5) ?
						 HOSTFCMODE_INGRESS_QUEUE_X :
						 HOSTFCMODE_STATUS_PAGE_X) |
		      FW_EQ_ETH_CMD_PCIECHN_V(pi->tx_chan) |
		      FW_EQ_ETH_CMD_FETCHRO_F | FW_EQ_ETH_CMD_IQID_V(iqid));

	 
	c.dcaen_to_eqsize =
		htonl(FW_EQ_ETH_CMD_FBMIN_V(chip_ver <= CHELSIO_T5
					    ? FETCHBURSTMIN_64B_X
					    : FETCHBURSTMIN_64B_T6_X) |
		      FW_EQ_ETH_CMD_FBMAX_V(FETCHBURSTMAX_512B_X) |
		      FW_EQ_ETH_CMD_CIDXFTHRESH_V(CIDXFLUSHTHRESH_32_X) |
		      FW_EQ_ETH_CMD_CIDXFTHRESHO_V(chip_ver == CHELSIO_T5) |
		      FW_EQ_ETH_CMD_EQSIZE_V(nentries));

	c.eqaddr = cpu_to_be64(txq->q.phys_addr);

	 
	if (dbqt)
		c.timeren_timerix =
			cpu_to_be32(FW_EQ_ETH_CMD_TIMEREN_F |
				    FW_EQ_ETH_CMD_TIMERIX_V(txq->dbqtimerix));

	ret = t4_wr_mbox(adap, adap->mbox, &c, sizeof(c), &c);
	if (ret) {
		kfree(txq->q.sdesc);
		txq->q.sdesc = NULL;
		dma_free_coherent(adap->pdev_dev,
				  nentries * sizeof(struct tx_desc),
				  txq->q.desc, txq->q.phys_addr);
		txq->q.desc = NULL;
		return ret;
	}

	txq->q.q_type = CXGB4_TXQ_ETH;
	init_txq(adap, &txq->q, FW_EQ_ETH_CMD_EQID_G(ntohl(c.eqid_pkd)));
	txq->txq = netdevq;
	txq->tso = 0;
	txq->uso = 0;
	txq->tx_cso = 0;
	txq->vlan_ins = 0;
	txq->mapping_err = 0;
	txq->dbqt = dbqt;

	return 0;
}

int t4_sge_alloc_ctrl_txq(struct adapter *adap, struct sge_ctrl_txq *txq,
			  struct net_device *dev, unsigned int iqid,
			  unsigned int cmplqid)
{
	unsigned int chip_ver = CHELSIO_CHIP_VERSION(adap->params.chip);
	struct port_info *pi = netdev_priv(dev);
	struct sge *s = &adap->sge;
	struct fw_eq_ctrl_cmd c;
	int ret, nentries;

	 
	nentries = txq->q.size + s->stat_len / sizeof(struct tx_desc);

	txq->q.desc = alloc_ring(adap->pdev_dev, nentries,
				 sizeof(struct tx_desc), 0, &txq->q.phys_addr,
				 NULL, 0, dev_to_node(adap->pdev_dev));
	if (!txq->q.desc)
		return -ENOMEM;

	c.op_to_vfn = htonl(FW_CMD_OP_V(FW_EQ_CTRL_CMD) | FW_CMD_REQUEST_F |
			    FW_CMD_WRITE_F | FW_CMD_EXEC_F |
			    FW_EQ_CTRL_CMD_PFN_V(adap->pf) |
			    FW_EQ_CTRL_CMD_VFN_V(0));
	c.alloc_to_len16 = htonl(FW_EQ_CTRL_CMD_ALLOC_F |
				 FW_EQ_CTRL_CMD_EQSTART_F | FW_LEN16(c));
	c.cmpliqid_eqid = htonl(FW_EQ_CTRL_CMD_CMPLIQID_V(cmplqid));
	c.physeqid_pkd = htonl(0);
	c.fetchszm_to_iqid =
		htonl(FW_EQ_CTRL_CMD_HOSTFCMODE_V(HOSTFCMODE_STATUS_PAGE_X) |
		      FW_EQ_CTRL_CMD_PCIECHN_V(pi->tx_chan) |
		      FW_EQ_CTRL_CMD_FETCHRO_F | FW_EQ_CTRL_CMD_IQID_V(iqid));
	c.dcaen_to_eqsize =
		htonl(FW_EQ_CTRL_CMD_FBMIN_V(chip_ver <= CHELSIO_T5
					     ? FETCHBURSTMIN_64B_X
					     : FETCHBURSTMIN_64B_T6_X) |
		      FW_EQ_CTRL_CMD_FBMAX_V(FETCHBURSTMAX_512B_X) |
		      FW_EQ_CTRL_CMD_CIDXFTHRESH_V(CIDXFLUSHTHRESH_32_X) |
		      FW_EQ_CTRL_CMD_EQSIZE_V(nentries));
	c.eqaddr = cpu_to_be64(txq->q.phys_addr);

	ret = t4_wr_mbox(adap, adap->mbox, &c, sizeof(c), &c);
	if (ret) {
		dma_free_coherent(adap->pdev_dev,
				  nentries * sizeof(struct tx_desc),
				  txq->q.desc, txq->q.phys_addr);
		txq->q.desc = NULL;
		return ret;
	}

	txq->q.q_type = CXGB4_TXQ_CTRL;
	init_txq(adap, &txq->q, FW_EQ_CTRL_CMD_EQID_G(ntohl(c.cmpliqid_eqid)));
	txq->adap = adap;
	skb_queue_head_init(&txq->sendq);
	tasklet_setup(&txq->qresume_tsk, restart_ctrlq);
	txq->full = 0;
	return 0;
}

int t4_sge_mod_ctrl_txq(struct adapter *adap, unsigned int eqid,
			unsigned int cmplqid)
{
	u32 param, val;

	param = (FW_PARAMS_MNEM_V(FW_PARAMS_MNEM_DMAQ) |
		 FW_PARAMS_PARAM_X_V(FW_PARAMS_PARAM_DMAQ_EQ_CMPLIQID_CTRL) |
		 FW_PARAMS_PARAM_YZ_V(eqid));
	val = cmplqid;
	return t4_set_params(adap, adap->mbox, adap->pf, 0, 1, &param, &val);
}

static int t4_sge_alloc_ofld_txq(struct adapter *adap, struct sge_txq *q,
				 struct net_device *dev, u32 cmd, u32 iqid)
{
	unsigned int chip_ver = CHELSIO_CHIP_VERSION(adap->params.chip);
	struct port_info *pi = netdev_priv(dev);
	struct sge *s = &adap->sge;
	struct fw_eq_ofld_cmd c;
	u32 fb_min, nentries;
	int ret;

	 
	nentries = q->size + s->stat_len / sizeof(struct tx_desc);
	q->desc = alloc_ring(adap->pdev_dev, q->size, sizeof(struct tx_desc),
			     sizeof(struct tx_sw_desc), &q->phys_addr,
			     &q->sdesc, s->stat_len, NUMA_NO_NODE);
	if (!q->desc)
		return -ENOMEM;

	if (chip_ver <= CHELSIO_T5)
		fb_min = FETCHBURSTMIN_64B_X;
	else
		fb_min = FETCHBURSTMIN_64B_T6_X;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = htonl(FW_CMD_OP_V(cmd) | FW_CMD_REQUEST_F |
			    FW_CMD_WRITE_F | FW_CMD_EXEC_F |
			    FW_EQ_OFLD_CMD_PFN_V(adap->pf) |
			    FW_EQ_OFLD_CMD_VFN_V(0));
	c.alloc_to_len16 = htonl(FW_EQ_OFLD_CMD_ALLOC_F |
				 FW_EQ_OFLD_CMD_EQSTART_F | FW_LEN16(c));
	c.fetchszm_to_iqid =
		htonl(FW_EQ_OFLD_CMD_HOSTFCMODE_V(HOSTFCMODE_STATUS_PAGE_X) |
		      FW_EQ_OFLD_CMD_PCIECHN_V(pi->tx_chan) |
		      FW_EQ_OFLD_CMD_FETCHRO_F | FW_EQ_OFLD_CMD_IQID_V(iqid));
	c.dcaen_to_eqsize =
		htonl(FW_EQ_OFLD_CMD_FBMIN_V(fb_min) |
		      FW_EQ_OFLD_CMD_FBMAX_V(FETCHBURSTMAX_512B_X) |
		      FW_EQ_OFLD_CMD_CIDXFTHRESH_V(CIDXFLUSHTHRESH_32_X) |
		      FW_EQ_OFLD_CMD_EQSIZE_V(nentries));
	c.eqaddr = cpu_to_be64(q->phys_addr);

	ret = t4_wr_mbox(adap, adap->mbox, &c, sizeof(c), &c);
	if (ret) {
		kfree(q->sdesc);
		q->sdesc = NULL;
		dma_free_coherent(adap->pdev_dev,
				  nentries * sizeof(struct tx_desc),
				  q->desc, q->phys_addr);
		q->desc = NULL;
		return ret;
	}

	init_txq(adap, q, FW_EQ_OFLD_CMD_EQID_G(ntohl(c.eqid_pkd)));
	return 0;
}

int t4_sge_alloc_uld_txq(struct adapter *adap, struct sge_uld_txq *txq,
			 struct net_device *dev, unsigned int iqid,
			 unsigned int uld_type)
{
	u32 cmd = FW_EQ_OFLD_CMD;
	int ret;

	if (unlikely(uld_type == CXGB4_TX_CRYPTO))
		cmd = FW_EQ_CTRL_CMD;

	ret = t4_sge_alloc_ofld_txq(adap, &txq->q, dev, cmd, iqid);
	if (ret)
		return ret;

	txq->q.q_type = CXGB4_TXQ_ULD;
	txq->adap = adap;
	skb_queue_head_init(&txq->sendq);
	tasklet_setup(&txq->qresume_tsk, restart_ofldq);
	txq->full = 0;
	txq->mapping_err = 0;
	return 0;
}

int t4_sge_alloc_ethofld_txq(struct adapter *adap, struct sge_eohw_txq *txq,
			     struct net_device *dev, u32 iqid)
{
	int ret;

	ret = t4_sge_alloc_ofld_txq(adap, &txq->q, dev, FW_EQ_OFLD_CMD, iqid);
	if (ret)
		return ret;

	txq->q.q_type = CXGB4_TXQ_ULD;
	spin_lock_init(&txq->lock);
	txq->adap = adap;
	txq->tso = 0;
	txq->uso = 0;
	txq->tx_cso = 0;
	txq->vlan_ins = 0;
	txq->mapping_err = 0;
	return 0;
}

void free_txq(struct adapter *adap, struct sge_txq *q)
{
	struct sge *s = &adap->sge;

	dma_free_coherent(adap->pdev_dev,
			  q->size * sizeof(struct tx_desc) + s->stat_len,
			  q->desc, q->phys_addr);
	q->cntxt_id = 0;
	q->sdesc = NULL;
	q->desc = NULL;
}

void free_rspq_fl(struct adapter *adap, struct sge_rspq *rq,
		  struct sge_fl *fl)
{
	struct sge *s = &adap->sge;
	unsigned int fl_id = fl ? fl->cntxt_id : 0xffff;

	adap->sge.ingr_map[rq->cntxt_id - adap->sge.ingr_start] = NULL;
	t4_iq_free(adap, adap->mbox, adap->pf, 0, FW_IQ_TYPE_FL_INT_CAP,
		   rq->cntxt_id, fl_id, 0xffff);
	dma_free_coherent(adap->pdev_dev, (rq->size + 1) * rq->iqe_len,
			  rq->desc, rq->phys_addr);
	netif_napi_del(&rq->napi);
	rq->netdev = NULL;
	rq->cntxt_id = rq->abs_id = 0;
	rq->desc = NULL;

	if (fl) {
		free_rx_bufs(adap, fl, fl->avail);
		dma_free_coherent(adap->pdev_dev, fl->size * 8 + s->stat_len,
				  fl->desc, fl->addr);
		kfree(fl->sdesc);
		fl->sdesc = NULL;
		fl->cntxt_id = 0;
		fl->desc = NULL;
	}
}

 
void t4_free_ofld_rxqs(struct adapter *adap, int n, struct sge_ofld_rxq *q)
{
	for ( ; n; n--, q++)
		if (q->rspq.desc)
			free_rspq_fl(adap, &q->rspq,
				     q->fl.size ? &q->fl : NULL);
}

void t4_sge_free_ethofld_txq(struct adapter *adap, struct sge_eohw_txq *txq)
{
	if (txq->q.desc) {
		t4_ofld_eq_free(adap, adap->mbox, adap->pf, 0,
				txq->q.cntxt_id);
		free_tx_desc(adap, &txq->q, txq->q.in_use, false);
		kfree(txq->q.sdesc);
		free_txq(adap, &txq->q);
	}
}

 
void t4_free_sge_resources(struct adapter *adap)
{
	int i;
	struct sge_eth_rxq *eq;
	struct sge_eth_txq *etq;

	 
	for (i = 0; i < adap->sge.ethqsets; i++) {
		eq = &adap->sge.ethrxq[i];
		if (eq->rspq.desc)
			t4_iq_stop(adap, adap->mbox, adap->pf, 0,
				   FW_IQ_TYPE_FL_INT_CAP,
				   eq->rspq.cntxt_id,
				   eq->fl.size ? eq->fl.cntxt_id : 0xffff,
				   0xffff);
	}

	 
	for (i = 0; i < adap->sge.ethqsets; i++) {
		eq = &adap->sge.ethrxq[i];
		if (eq->rspq.desc)
			free_rspq_fl(adap, &eq->rspq,
				     eq->fl.size ? &eq->fl : NULL);
		if (eq->msix) {
			cxgb4_free_msix_idx_in_bmap(adap, eq->msix->idx);
			eq->msix = NULL;
		}

		etq = &adap->sge.ethtxq[i];
		if (etq->q.desc) {
			t4_eth_eq_free(adap, adap->mbox, adap->pf, 0,
				       etq->q.cntxt_id);
			__netif_tx_lock_bh(etq->txq);
			free_tx_desc(adap, &etq->q, etq->q.in_use, true);
			__netif_tx_unlock_bh(etq->txq);
			kfree(etq->q.sdesc);
			free_txq(adap, &etq->q);
		}
	}

	 
	for (i = 0; i < ARRAY_SIZE(adap->sge.ctrlq); i++) {
		struct sge_ctrl_txq *cq = &adap->sge.ctrlq[i];

		if (cq->q.desc) {
			tasklet_kill(&cq->qresume_tsk);
			t4_ctrl_eq_free(adap, adap->mbox, adap->pf, 0,
					cq->q.cntxt_id);
			__skb_queue_purge(&cq->sendq);
			free_txq(adap, &cq->q);
		}
	}

	if (adap->sge.fw_evtq.desc) {
		free_rspq_fl(adap, &adap->sge.fw_evtq, NULL);
		if (adap->sge.fwevtq_msix_idx >= 0)
			cxgb4_free_msix_idx_in_bmap(adap,
						    adap->sge.fwevtq_msix_idx);
	}

	if (adap->sge.nd_msix_idx >= 0)
		cxgb4_free_msix_idx_in_bmap(adap, adap->sge.nd_msix_idx);

	if (adap->sge.intrq.desc)
		free_rspq_fl(adap, &adap->sge.intrq, NULL);

	if (!is_t4(adap->params.chip)) {
		etq = &adap->sge.ptptxq;
		if (etq->q.desc) {
			t4_eth_eq_free(adap, adap->mbox, adap->pf, 0,
				       etq->q.cntxt_id);
			spin_lock_bh(&adap->ptp_lock);
			free_tx_desc(adap, &etq->q, etq->q.in_use, true);
			spin_unlock_bh(&adap->ptp_lock);
			kfree(etq->q.sdesc);
			free_txq(adap, &etq->q);
		}
	}

	 
	memset(adap->sge.egr_map, 0,
	       adap->sge.egr_sz * sizeof(*adap->sge.egr_map));
}

void t4_sge_start(struct adapter *adap)
{
	adap->sge.ethtxq_rover = 0;
	mod_timer(&adap->sge.rx_timer, jiffies + RX_QCHECK_PERIOD);
	mod_timer(&adap->sge.tx_timer, jiffies + TX_QCHECK_PERIOD);
}

 
void t4_sge_stop(struct adapter *adap)
{
	int i;
	struct sge *s = &adap->sge;

	if (s->rx_timer.function)
		del_timer_sync(&s->rx_timer);
	if (s->tx_timer.function)
		del_timer_sync(&s->tx_timer);

	if (is_offload(adap)) {
		struct sge_uld_txq_info *txq_info;

		txq_info = adap->sge.uld_txq_info[CXGB4_TX_OFLD];
		if (txq_info) {
			struct sge_uld_txq *txq = txq_info->uldtxq;

			for_each_ofldtxq(&adap->sge, i) {
				if (txq->q.desc)
					tasklet_kill(&txq->qresume_tsk);
			}
		}
	}

	if (is_pci_uld(adap)) {
		struct sge_uld_txq_info *txq_info;

		txq_info = adap->sge.uld_txq_info[CXGB4_TX_CRYPTO];
		if (txq_info) {
			struct sge_uld_txq *txq = txq_info->uldtxq;

			for_each_ofldtxq(&adap->sge, i) {
				if (txq->q.desc)
					tasklet_kill(&txq->qresume_tsk);
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(s->ctrlq); i++) {
		struct sge_ctrl_txq *cq = &s->ctrlq[i];

		if (cq->q.desc)
			tasklet_kill(&cq->qresume_tsk);
	}
}

 

static int t4_sge_init_soft(struct adapter *adap)
{
	struct sge *s = &adap->sge;
	u32 fl_small_pg, fl_large_pg, fl_small_mtu, fl_large_mtu;
	u32 timer_value_0_and_1, timer_value_2_and_3, timer_value_4_and_5;
	u32 ingress_rx_threshold;

	 
	if ((t4_read_reg(adap, SGE_CONTROL_A) & RXPKTCPLMODE_F) !=
	    RXPKTCPLMODE_V(RXPKTCPLMODE_SPLIT_X)) {
		dev_err(adap->pdev_dev, "bad SGE CPL MODE\n");
		return -EINVAL;
	}

	 
	#define READ_FL_BUF(x) \
		t4_read_reg(adap, SGE_FL_BUFFER_SIZE0_A+(x)*sizeof(u32))

	fl_small_pg = READ_FL_BUF(RX_SMALL_PG_BUF);
	fl_large_pg = READ_FL_BUF(RX_LARGE_PG_BUF);
	fl_small_mtu = READ_FL_BUF(RX_SMALL_MTU_BUF);
	fl_large_mtu = READ_FL_BUF(RX_LARGE_MTU_BUF);

	 
	if (fl_large_pg <= fl_small_pg)
		fl_large_pg = 0;

	#undef READ_FL_BUF

	 
	if (fl_small_pg != PAGE_SIZE ||
	    (fl_large_pg & (fl_large_pg-1)) != 0) {
		dev_err(adap->pdev_dev, "bad SGE FL page buffer sizes [%d, %d]\n",
			fl_small_pg, fl_large_pg);
		return -EINVAL;
	}
	if (fl_large_pg)
		s->fl_pg_order = ilog2(fl_large_pg) - PAGE_SHIFT;

	if (fl_small_mtu < FL_MTU_SMALL_BUFSIZE(adap) ||
	    fl_large_mtu < FL_MTU_LARGE_BUFSIZE(adap)) {
		dev_err(adap->pdev_dev, "bad SGE FL MTU sizes [%d, %d]\n",
			fl_small_mtu, fl_large_mtu);
		return -EINVAL;
	}

	 
	timer_value_0_and_1 = t4_read_reg(adap, SGE_TIMER_VALUE_0_AND_1_A);
	timer_value_2_and_3 = t4_read_reg(adap, SGE_TIMER_VALUE_2_AND_3_A);
	timer_value_4_and_5 = t4_read_reg(adap, SGE_TIMER_VALUE_4_AND_5_A);
	s->timer_val[0] = core_ticks_to_us(adap,
		TIMERVALUE0_G(timer_value_0_and_1));
	s->timer_val[1] = core_ticks_to_us(adap,
		TIMERVALUE1_G(timer_value_0_and_1));
	s->timer_val[2] = core_ticks_to_us(adap,
		TIMERVALUE2_G(timer_value_2_and_3));
	s->timer_val[3] = core_ticks_to_us(adap,
		TIMERVALUE3_G(timer_value_2_and_3));
	s->timer_val[4] = core_ticks_to_us(adap,
		TIMERVALUE4_G(timer_value_4_and_5));
	s->timer_val[5] = core_ticks_to_us(adap,
		TIMERVALUE5_G(timer_value_4_and_5));

	ingress_rx_threshold = t4_read_reg(adap, SGE_INGRESS_RX_THRESHOLD_A);
	s->counter_val[0] = THRESHOLD_0_G(ingress_rx_threshold);
	s->counter_val[1] = THRESHOLD_1_G(ingress_rx_threshold);
	s->counter_val[2] = THRESHOLD_2_G(ingress_rx_threshold);
	s->counter_val[3] = THRESHOLD_3_G(ingress_rx_threshold);

	return 0;
}

 
int t4_sge_init(struct adapter *adap)
{
	struct sge *s = &adap->sge;
	u32 sge_control, sge_conm_ctrl;
	int ret, egress_threshold;

	 
	sge_control = t4_read_reg(adap, SGE_CONTROL_A);
	s->pktshift = PKTSHIFT_G(sge_control);
	s->stat_len = (sge_control & EGRSTATUSPAGESIZE_F) ? 128 : 64;

	s->fl_align = t4_fl_pkt_align(adap);
	ret = t4_sge_init_soft(adap);
	if (ret < 0)
		return ret;

	 
	sge_conm_ctrl = t4_read_reg(adap, SGE_CONM_CTRL_A);
	switch (CHELSIO_CHIP_VERSION(adap->params.chip)) {
	case CHELSIO_T4:
		egress_threshold = EGRTHRESHOLD_G(sge_conm_ctrl);
		break;
	case CHELSIO_T5:
		egress_threshold = EGRTHRESHOLDPACKING_G(sge_conm_ctrl);
		break;
	case CHELSIO_T6:
		egress_threshold = T6_EGRTHRESHOLDPACKING_G(sge_conm_ctrl);
		break;
	default:
		dev_err(adap->pdev_dev, "Unsupported Chip version %d\n",
			CHELSIO_CHIP_VERSION(adap->params.chip));
		return -EINVAL;
	}
	s->fl_starve_thres = 2*egress_threshold + 1;

	t4_idma_monitor_init(adap, &s->idma_monitor);

	 
	timer_setup(&s->rx_timer, sge_rx_timer_cb, 0);
	timer_setup(&s->tx_timer, sge_tx_timer_cb, 0);

	spin_lock_init(&s->intrq_lock);

	return 0;
}
