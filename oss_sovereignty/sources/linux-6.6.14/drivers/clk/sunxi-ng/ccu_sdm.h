


#ifndef _CCU_SDM_H
#define _CCU_SDM_H

#include <linux/clk-provider.h>

#include "ccu_common.h"

struct ccu_sdm_setting {
	unsigned long	rate;

	
	u32		pattern;

	
	u32		m;
	u32		n;
};

struct ccu_sdm_internal {
	struct ccu_sdm_setting	*table;
	u32		table_size;
	
	u32		enable;
	
	u32		tuning_enable;
	u16		tuning_reg;
};

#define _SUNXI_CCU_SDM(_table, _enable,			\
		       _reg, _reg_enable)		\
	{						\
		.table		= _table,		\
		.table_size	= ARRAY_SIZE(_table),	\
		.enable		= _enable,		\
		.tuning_enable	= _reg_enable,		\
		.tuning_reg	= _reg,			\
	}

bool ccu_sdm_helper_is_enabled(struct ccu_common *common,
			       struct ccu_sdm_internal *sdm);
void ccu_sdm_helper_enable(struct ccu_common *common,
			   struct ccu_sdm_internal *sdm,
			   unsigned long rate);
void ccu_sdm_helper_disable(struct ccu_common *common,
			    struct ccu_sdm_internal *sdm);

bool ccu_sdm_helper_has_rate(struct ccu_common *common,
			     struct ccu_sdm_internal *sdm,
			     unsigned long rate);

unsigned long ccu_sdm_helper_read_rate(struct ccu_common *common,
				       struct ccu_sdm_internal *sdm,
				       u32 m, u32 n);

int ccu_sdm_helper_get_factors(struct ccu_common *common,
			       struct ccu_sdm_internal *sdm,
			       unsigned long rate,
			       unsigned long *m, unsigned long *n);

#endif
