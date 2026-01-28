#ifndef __LSDC_PIXPLL_H__
#define __LSDC_PIXPLL_H__
#include <drm/drm_device.h>
struct lsdc_pixpll_parms {
	unsigned int ref_clock;
	unsigned int div_ref;
	unsigned int loopc;
	unsigned int div_out;
};
struct lsdc_pixpll;
struct lsdc_pixpll_funcs {
	int (*setup)(struct lsdc_pixpll * const this);
	int (*compute)(struct lsdc_pixpll * const this,
		       unsigned int clock,
		       struct lsdc_pixpll_parms *pout);
	int (*update)(struct lsdc_pixpll * const this,
		      struct lsdc_pixpll_parms const *pin);
	unsigned int (*get_rate)(struct lsdc_pixpll * const this);
	void (*print)(struct lsdc_pixpll * const this,
		      struct drm_printer *printer);
};
struct lsdc_pixpll {
	const struct lsdc_pixpll_funcs *funcs;
	struct drm_device *ddev;
	u32 reg_base;
	u32 reg_size;
	void __iomem *mmio;
	struct lsdc_pixpll_parms *priv;
};
int lsdc_pixpll_init(struct lsdc_pixpll * const this,
		     struct drm_device *ddev,
		     unsigned int index);
#endif
