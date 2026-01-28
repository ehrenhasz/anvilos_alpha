

#include "dpaa_sys.h"

#include <soc/fsl/qman.h>
#include <linux/dma-mapping.h>
#include <linux/iommu.h>

#if defined(CONFIG_FSL_PAMU)
#include <asm/fsl_pamu_stash.h>
#endif

struct qm_mcr_querywq {
	u8 verb;
	u8 result;
	u16 channel_wq; 
	u8 __reserved[28];
	u32 wq_len[8];
} __packed;

static inline u16 qm_mcr_querywq_get_chan(const struct qm_mcr_querywq *wq)
{
	return wq->channel_wq >> 3;
}

struct __qm_mcr_querycongestion {
	u32 state[8];
};


struct qm_mcr_querycongestion {
	u8 verb;
	u8 result;
	u8 __reserved[30];
	
	struct __qm_mcr_querycongestion state;
} __packed;


struct qm_mcr_querycgr {
	u8 verb;
	u8 result;
	u16 __reserved1;
	struct __qm_mc_cgr cgr; 
	u8 __reserved2[6];
	u8 i_bcnt_hi;	
	__be32 i_bcnt_lo;	
	u8 __reserved3[3];
	u8 a_bcnt_hi;	
	__be32 a_bcnt_lo;	
	__be32 cscn_targ_swp[4];
} __packed;

static inline u64 qm_mcr_querycgr_i_get64(const struct qm_mcr_querycgr *q)
{
	return ((u64)q->i_bcnt_hi << 32) | be32_to_cpu(q->i_bcnt_lo);
}
static inline u64 qm_mcr_querycgr_a_get64(const struct qm_mcr_querycgr *q)
{
	return ((u64)q->a_bcnt_hi << 32) | be32_to_cpu(q->a_bcnt_lo);
}




#define CGR_BITS_PER_WORD 5
#define CGR_WORD(x)	((x) >> CGR_BITS_PER_WORD)
#define CGR_BIT(x)	(BIT(31) >> ((x) & 0x1f))
#define CGR_NUM	(sizeof(struct __qm_mcr_querycongestion) << 3)

struct qman_cgrs {
	struct __qm_mcr_querycongestion q;
};

static inline void qman_cgrs_init(struct qman_cgrs *c)
{
	memset(c, 0, sizeof(*c));
}

static inline void qman_cgrs_fill(struct qman_cgrs *c)
{
	memset(c, 0xff, sizeof(*c));
}

static inline int qman_cgrs_get(struct qman_cgrs *c, u8 cgr)
{
	return c->q.state[CGR_WORD(cgr)] & CGR_BIT(cgr);
}

static inline void qman_cgrs_cp(struct qman_cgrs *dest,
				const struct qman_cgrs *src)
{
	*dest = *src;
}

static inline void qman_cgrs_and(struct qman_cgrs *dest,
			const struct qman_cgrs *a, const struct qman_cgrs *b)
{
	int ret;
	u32 *_d = dest->q.state;
	const u32 *_a = a->q.state;
	const u32 *_b = b->q.state;

	for (ret = 0; ret < 8; ret++)
		*_d++ = *_a++ & *_b++;
}

static inline void qman_cgrs_xor(struct qman_cgrs *dest,
			const struct qman_cgrs *a, const struct qman_cgrs *b)
{
	int ret;
	u32 *_d = dest->q.state;
	const u32 *_a = a->q.state;
	const u32 *_b = b->q.state;

	for (ret = 0; ret < 8; ret++)
		*_d++ = *_a++ ^ *_b++;
}

void qman_init_cgr_all(void);

struct qm_portal_config {
	
	void *addr_virt_ce;
	void __iomem *addr_virt_ci;
	struct device *dev;
	struct iommu_domain *iommu_domain;
	
	struct list_head list;
	
	
	int cpu;
	
	int irq;
	
	u16 channel;
	
	u32 pools;
};


#define QMAN_REV11 0x0101
#define QMAN_REV12 0x0102
#define QMAN_REV20 0x0200
#define QMAN_REV30 0x0300
#define QMAN_REV31 0x0301
#define QMAN_REV32 0x0302
extern u16 qman_ip_rev; 

#define QM_FQID_RANGE_START 1 
extern struct gen_pool *qm_fqalloc; 
extern struct gen_pool *qm_qpalloc; 
extern struct gen_pool *qm_cgralloc; 
u32 qm_get_pools_sdqcr(void);

int qman_wq_alloc(void);
#ifdef CONFIG_FSL_PAMU
#define qman_liodn_fixup __qman_liodn_fixup
#else
static inline void qman_liodn_fixup(u16 channel)
{
}
#endif
void __qman_liodn_fixup(u16 channel);
void qman_set_sdest(u16 channel, unsigned int cpu_idx);

struct qman_portal *qman_create_affine_portal(
			const struct qm_portal_config *config,
			const struct qman_cgrs *cgrs);
const struct qm_portal_config *qman_destroy_affine_portal(void);


int qman_query_fq(struct qman_fq *fq, struct qm_fqd *fqd);

int qman_alloc_fq_table(u32 num_fqids);




#define QM_SDQCR_SOURCE_CHANNELS	0x0
#define QM_SDQCR_SOURCE_SPECIFICWQ	0x40000000
#define QM_SDQCR_COUNT_EXACT1		0x0
#define QM_SDQCR_COUNT_UPTO3		0x20000000
#define QM_SDQCR_DEDICATED_PRECEDENCE	0x10000000
#define QM_SDQCR_TYPE_MASK		0x03000000
#define QM_SDQCR_TYPE_NULL		0x0
#define QM_SDQCR_TYPE_PRIO_QOS		0x01000000
#define QM_SDQCR_TYPE_ACTIVE_QOS	0x02000000
#define QM_SDQCR_TYPE_ACTIVE		0x03000000
#define QM_SDQCR_TOKEN_MASK		0x00ff0000
#define QM_SDQCR_TOKEN_SET(v)		(((v) & 0xff) << 16)
#define QM_SDQCR_TOKEN_GET(v)		(((v) >> 16) & 0xff)
#define QM_SDQCR_CHANNELS_DEDICATED	0x00008000
#define QM_SDQCR_SPECIFICWQ_MASK	0x000000f7
#define QM_SDQCR_SPECIFICWQ_DEDICATED	0x00000000
#define QM_SDQCR_SPECIFICWQ_POOL(n)	((n) << 4)
#define QM_SDQCR_SPECIFICWQ_WQ(n)	(n)


#define QM_VDQCR_FQID_MASK		0x00ffffff
#define QM_VDQCR_FQID(n)		((n) & QM_VDQCR_FQID_MASK)


#define QM_PIRQ_DQAVAIL	0x0000ffff


#define QM_DQAVAIL_PORTAL	0x8000		
#define QM_DQAVAIL_POOL(n)	(0x8000 >> (n))	
#define QM_DQAVAIL_MASK		0xffff

#define QM_PIRQ_VISIBLE	(QM_PIRQ_SLOW | QM_PIRQ_DQRI)

extern struct qman_portal *affine_portals[NR_CPUS];
extern struct qman_portal *qman_dma_portal;
const struct qm_portal_config *qman_get_qm_portal_config(
						struct qman_portal *portal);

unsigned int qm_get_fqid_maxcnt(void);

int qman_shutdown_fq(u32 fqid);

int qman_requires_cleanup(void);
void qman_done_cleanup(void);
void qman_enable_irqs(void);
