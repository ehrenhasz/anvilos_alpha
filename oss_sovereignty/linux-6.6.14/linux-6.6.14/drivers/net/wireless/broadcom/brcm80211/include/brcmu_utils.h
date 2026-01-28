#ifndef	_BRCMU_UTILS_H_
#define	_BRCMU_UTILS_H_
#include <linux/skbuff.h>
#define SPINWAIT(exp, us) { \
	uint countdown = (us) + 9; \
	while ((exp) && (countdown >= 10)) {\
		udelay(10); \
		countdown -= 10; \
	} \
}
#define PKTQ_LEN_DEFAULT        128	 
#define PKTQ_MAX_PREC           16	 
#define BCME_STRLEN		64	 
#define	PKTBUFSZ	2048
#ifndef setbit
#ifndef NBBY			 
#define	NBBY	8		 
#endif				 
#define	setbit(a, i)	(((u8 *)a)[(i)/NBBY] |= 1<<((i)%NBBY))
#define	clrbit(a, i)	(((u8 *)a)[(i)/NBBY] &= ~(1<<((i)%NBBY)))
#define	isset(a, i)	(((const u8 *)a)[(i)/NBBY] & (1<<((i)%NBBY)))
#define	isclr(a, i)	((((const u8 *)a)[(i)/NBBY] & (1<<((i)%NBBY))) == 0)
#endif				 
#define	NBITS(type)	(sizeof(type) * 8)
#define NBITVAL(nbits)	(1 << (nbits))
#define MAXBITVAL(nbits)	((1 << (nbits)) - 1)
#define	NBITMASK(nbits)	MAXBITVAL(nbits)
#define MAXNBVAL(nbyte)	MAXBITVAL((nbyte) * 8)
#define CRC16_INIT_VALUE 0xffff	 
#define CRC16_GOOD_VALUE 0xf0b8	 
#define ETHER_ADDR_STR_LEN	18
struct pktq_prec {
	struct sk_buff_head skblist;
	u16 max;		 
};
struct pktq {
	u16 num_prec;	 
	u16 hi_prec;	 
	u16 max;	 
	u16 len;	 
	struct pktq_prec q[PKTQ_MAX_PREC];
};
static inline int pktq_plen(struct pktq *pq, int prec)
{
	return pq->q[prec].skblist.qlen;
}
static inline int pktq_pavail(struct pktq *pq, int prec)
{
	return pq->q[prec].max - pq->q[prec].skblist.qlen;
}
static inline bool pktq_pfull(struct pktq *pq, int prec)
{
	return pq->q[prec].skblist.qlen >= pq->q[prec].max;
}
static inline bool pktq_pempty(struct pktq *pq, int prec)
{
	return skb_queue_empty(&pq->q[prec].skblist);
}
static inline struct sk_buff *pktq_ppeek(struct pktq *pq, int prec)
{
	return skb_peek(&pq->q[prec].skblist);
}
static inline struct sk_buff *pktq_ppeek_tail(struct pktq *pq, int prec)
{
	return skb_peek_tail(&pq->q[prec].skblist);
}
struct sk_buff *brcmu_pktq_penq(struct pktq *pq, int prec, struct sk_buff *p);
struct sk_buff *brcmu_pktq_penq_head(struct pktq *pq, int prec,
				     struct sk_buff *p);
struct sk_buff *brcmu_pktq_pdeq(struct pktq *pq, int prec);
struct sk_buff *brcmu_pktq_pdeq_tail(struct pktq *pq, int prec);
struct sk_buff *brcmu_pktq_pdeq_match(struct pktq *pq, int prec,
				      bool (*match_fn)(struct sk_buff *p,
						       void *arg),
				      void *arg);
struct sk_buff *brcmu_pkt_buf_get_skb(uint len);
void brcmu_pkt_buf_free_skb(struct sk_buff *skb);
void brcmu_pktq_pflush(struct pktq *pq, int prec, bool dir,
		       bool (*fn)(struct sk_buff *, void *), void *arg);
int brcmu_pktq_mlen(struct pktq *pq, uint prec_bmp);
struct sk_buff *brcmu_pktq_mdeq(struct pktq *pq, uint prec_bmp, int *prec_out);
static inline int pktq_len(struct pktq *pq)
{
	return (int)pq->len;
}
static inline int pktq_max(struct pktq *pq)
{
	return (int)pq->max;
}
static inline int pktq_avail(struct pktq *pq)
{
	return (int)(pq->max - pq->len);
}
static inline bool pktq_full(struct pktq *pq)
{
	return pq->len >= pq->max;
}
static inline bool pktq_empty(struct pktq *pq)
{
	return pq->len == 0;
}
void brcmu_pktq_init(struct pktq *pq, int num_prec, int max_len);
struct sk_buff *brcmu_pktq_peek_tail(struct pktq *pq, int *prec_out);
void brcmu_pktq_flush(struct pktq *pq, bool dir,
		      bool (*fn)(struct sk_buff *, void *), void *arg);
struct ipv4_addr;
static inline void brcmu_maskset32(u32 *var, u32 mask, u8 shift, u32 value)
{
	value = (value << shift) & mask;
	*var = (*var & ~mask) | value;
}
static inline u32 brcmu_maskget32(u32 var, u32 mask, u8 shift)
{
	return (var & mask) >> shift;
}
static inline void brcmu_maskset16(u16 *var, u16 mask, u8 shift, u16 value)
{
	value = (value << shift) & mask;
	*var = (*var & ~mask) | value;
}
static inline u16 brcmu_maskget16(u16 var, u16 mask, u8 shift)
{
	return (var & mask) >> shift;
}
#ifdef DEBUG
void brcmu_prpkt(const char *msg, struct sk_buff *p0);
#else
#define brcmu_prpkt(a, b)
#endif				 
#ifdef DEBUG
__printf(3, 4)
void brcmu_dbg_hex_dump(const void *data, size_t size, const char *fmt, ...);
#else
__printf(3, 4)
static inline
void brcmu_dbg_hex_dump(const void *data, size_t size, const char *fmt, ...)
{
}
#endif
#define BRCMU_BOARDREV_LEN	8
#define BRCMU_DOTREV_LEN	16
char *brcmu_boardrev_str(u32 brev, char *buf);
char *brcmu_dotrev_str(u32 dotrev, char *buf);
#endif				 
