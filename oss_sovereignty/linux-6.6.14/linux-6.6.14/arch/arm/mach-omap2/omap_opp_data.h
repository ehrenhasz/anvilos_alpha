#ifndef __ARCH_ARM_MACH_OMAP2_OMAP_OPP_DATA_H
#define __ARCH_ARM_MACH_OMAP2_OMAP_OPP_DATA_H
#include "omap_hwmod.h"
#include "voltage.h"
struct omap_opp_def {
	char *hwmod_name;
	unsigned long freq;
	unsigned long u_volt;
	bool default_available;
};
#define OPP_INITIALIZER(_hwmod_name, _enabled, _freq, _uv)	\
{								\
	.hwmod_name	= _hwmod_name,				\
	.default_available	= _enabled,			\
	.freq		= _freq,				\
	.u_volt		= _uv,					\
}
#define VOLT_DATA_DEFINE(_v_nom, _efuse_offs, _errminlimit, _errgain)  \
{								       \
	.volt_nominal	= _v_nom,				       \
	.sr_efuse_offs	= _efuse_offs,				       \
	.sr_errminlimit = _errminlimit,				       \
	.vp_errgain	= _errgain				       \
}
extern struct omap_volt_data omap34xx_vddmpu_volt_data[];
extern struct omap_volt_data omap34xx_vddcore_volt_data[];
extern struct omap_volt_data omap36xx_vddmpu_volt_data[];
extern struct omap_volt_data omap36xx_vddcore_volt_data[];
extern struct omap_volt_data omap443x_vdd_mpu_volt_data[];
extern struct omap_volt_data omap443x_vdd_iva_volt_data[];
extern struct omap_volt_data omap443x_vdd_core_volt_data[];
extern struct omap_volt_data omap446x_vdd_mpu_volt_data[];
extern struct omap_volt_data omap446x_vdd_iva_volt_data[];
extern struct omap_volt_data omap446x_vdd_core_volt_data[];
#endif		 
