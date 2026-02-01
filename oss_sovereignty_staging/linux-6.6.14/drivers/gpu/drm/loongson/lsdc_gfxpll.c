
 

#include <linux/delay.h>

#include <drm/drm_file.h>
#include <drm/drm_managed.h>
#include <drm/drm_print.h>

#include "lsdc_drv.h"

 

struct loongson_gfxpll_bitmap {
	 
	unsigned div_out_dc    : 7;   
	unsigned div_out_gmc   : 7;   
	unsigned div_out_gpu   : 7;   
	unsigned loopc         : 9;   
	unsigned _reserved_1_  : 2;   

	 
	unsigned div_ref       : 7;    
	unsigned locked        : 1;    
	unsigned sel_out_dc    : 1;    
	unsigned sel_out_gmc   : 1;    
	unsigned sel_out_gpu   : 1;    
	unsigned set_param     : 1;    
	unsigned bypass        : 1;    
	unsigned powerdown     : 1;    
	unsigned _reserved_2_  : 18;   
};

union loongson_gfxpll_reg_bitmap {
	struct loongson_gfxpll_bitmap bitmap;
	u32 w[2];
	u64 d;
};

static void __gfxpll_rreg(struct loongson_gfxpll *this,
			  union loongson_gfxpll_reg_bitmap *reg)
{
#if defined(CONFIG_64BIT)
	reg->d = readq(this->mmio);
#else
	reg->w[0] = readl(this->mmio);
	reg->w[1] = readl(this->mmio + 4);
#endif
}

 

static int loongson_gfxpll_update(struct loongson_gfxpll * const this,
				  struct loongson_gfxpll_parms const *pin)
{
	 

	return 0;
}

static void loongson_gfxpll_get_rates(struct loongson_gfxpll * const this,
				      unsigned int *dc,
				      unsigned int *gmc,
				      unsigned int *gpu)
{
	struct loongson_gfxpll_parms *pparms = &this->parms;
	union loongson_gfxpll_reg_bitmap gfxpll_reg;
	unsigned int pre_output;
	unsigned int dc_mhz;
	unsigned int gmc_mhz;
	unsigned int gpu_mhz;

	__gfxpll_rreg(this, &gfxpll_reg);

	pparms->div_ref = gfxpll_reg.bitmap.div_ref;
	pparms->loopc = gfxpll_reg.bitmap.loopc;

	pparms->div_out_dc = gfxpll_reg.bitmap.div_out_dc;
	pparms->div_out_gmc = gfxpll_reg.bitmap.div_out_gmc;
	pparms->div_out_gpu = gfxpll_reg.bitmap.div_out_gpu;

	pre_output = pparms->ref_clock / pparms->div_ref * pparms->loopc;

	dc_mhz = pre_output / pparms->div_out_dc / 1000;
	gmc_mhz = pre_output / pparms->div_out_gmc / 1000;
	gpu_mhz = pre_output / pparms->div_out_gpu / 1000;

	if (dc)
		*dc = dc_mhz;

	if (gmc)
		*gmc = gmc_mhz;

	if (gpu)
		*gpu = gpu_mhz;
}

static void loongson_gfxpll_print(struct loongson_gfxpll * const this,
				  struct drm_printer *p,
				  bool verbose)
{
	struct loongson_gfxpll_parms *parms = &this->parms;
	unsigned int dc, gmc, gpu;

	if (verbose) {
		drm_printf(p, "reference clock: %u\n", parms->ref_clock);
		drm_printf(p, "div_ref = %u\n", parms->div_ref);
		drm_printf(p, "loopc = %u\n", parms->loopc);

		drm_printf(p, "div_out_dc = %u\n", parms->div_out_dc);
		drm_printf(p, "div_out_gmc = %u\n", parms->div_out_gmc);
		drm_printf(p, "div_out_gpu = %u\n", parms->div_out_gpu);
	}

	this->funcs->get_rates(this, &dc, &gmc, &gpu);

	drm_printf(p, "dc: %uMHz, gmc: %uMHz, gpu: %uMHz\n", dc, gmc, gpu);
}

 

static void loongson_gfxpll_fini(struct drm_device *ddev, void *data)
{
	struct loongson_gfxpll *this = (struct loongson_gfxpll *)data;

	iounmap(this->mmio);

	kfree(this);
}

static int loongson_gfxpll_init(struct loongson_gfxpll * const this)
{
	struct loongson_gfxpll_parms *pparms = &this->parms;
	struct drm_printer printer = drm_info_printer(this->ddev->dev);

	pparms->ref_clock = LSDC_PLL_REF_CLK_KHZ;

	this->mmio = ioremap(this->reg_base, this->reg_size);
	if (IS_ERR_OR_NULL(this->mmio))
		return -ENOMEM;

	this->funcs->print(this, &printer, false);

	return 0;
}

static const struct loongson_gfxpll_funcs lsdc_gmc_gpu_funcs = {
	.init = loongson_gfxpll_init,
	.update = loongson_gfxpll_update,
	.get_rates = loongson_gfxpll_get_rates,
	.print = loongson_gfxpll_print,
};

int loongson_gfxpll_create(struct drm_device *ddev,
			   struct loongson_gfxpll **ppout)
{
	struct lsdc_device *ldev = to_lsdc(ddev);
	const struct loongson_gfx_desc *gfx = to_loongson_gfx(ldev->descp);
	struct loongson_gfxpll *this;
	int ret;

	this = kzalloc(sizeof(*this), GFP_KERNEL);
	if (IS_ERR_OR_NULL(this))
		return -ENOMEM;

	this->ddev = ddev;
	this->reg_size = gfx->gfxpll.reg_size;
	this->reg_base = gfx->conf_reg_base + gfx->gfxpll.reg_offset;
	this->funcs = &lsdc_gmc_gpu_funcs;

	ret = this->funcs->init(this);
	if (unlikely(ret)) {
		kfree(this);
		return ret;
	}

	*ppout = this;

	return drmm_add_action_or_reset(ddev, loongson_gfxpll_fini, this);
}
