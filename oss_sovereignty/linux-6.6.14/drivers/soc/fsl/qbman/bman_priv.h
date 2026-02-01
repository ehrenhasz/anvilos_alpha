 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "dpaa_sys.h"

#include <soc/fsl/bman.h>

 
#define BM_PIRQ_RCRI	0x00000002	 

 
#define BMAN_REV10 0x0100
#define BMAN_REV20 0x0200
#define BMAN_REV21 0x0201
extern u16 bman_ip_rev;	 

extern struct gen_pool *bm_bpalloc;

struct bm_portal_config {
	 
	void  *addr_virt_ce;
	void __iomem *addr_virt_ci;
	 
	struct list_head list;
	struct device *dev;
	 
	 
	int cpu;
	 
	int irq;
};

struct bman_portal *bman_create_affine_portal(
			const struct bm_portal_config *config);
 
int bman_p_irqsource_add(struct bman_portal *p, u32 bits);

 
#define BM_PIRQ_VISIBLE	BM_PIRQ_RCRI

const struct bm_portal_config *
bman_get_bm_portal_config(const struct bman_portal *portal);

int bman_requires_cleanup(void);
void bman_done_cleanup(void);

int bm_shutdown_pool(u32 bpid);
