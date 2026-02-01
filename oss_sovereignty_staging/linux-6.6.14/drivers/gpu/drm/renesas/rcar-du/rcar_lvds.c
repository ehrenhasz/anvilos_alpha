
 

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/media-bus-format.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include <linux/sys_soc.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>

#include "rcar_lvds.h"
#include "rcar_lvds_regs.h"

struct rcar_lvds;

 
enum rcar_lvds_mode {
	RCAR_LVDS_MODE_JEIDA = 0,
	RCAR_LVDS_MODE_MIRROR = 1,
	RCAR_LVDS_MODE_VESA = 4,
};

enum rcar_lvds_link_type {
	RCAR_LVDS_SINGLE_LINK = 0,
	RCAR_LVDS_DUAL_LINK_EVEN_ODD_PIXELS = 1,
	RCAR_LVDS_DUAL_LINK_ODD_EVEN_PIXELS = 2,
};

#define RCAR_LVDS_QUIRK_LANES		BIT(0)	 
#define RCAR_LVDS_QUIRK_GEN3_LVEN	BIT(1)	 
#define RCAR_LVDS_QUIRK_PWD		BIT(2)	 
#define RCAR_LVDS_QUIRK_EXT_PLL		BIT(3)	 
#define RCAR_LVDS_QUIRK_DUAL_LINK	BIT(4)	 

struct rcar_lvds_device_info {
	unsigned int gen;
	unsigned int quirks;
	void (*pll_setup)(struct rcar_lvds *lvds, unsigned int freq);
};

struct rcar_lvds {
	struct device *dev;
	const struct rcar_lvds_device_info *info;
	struct reset_control *rstc;

	struct drm_bridge bridge;

	struct drm_bridge *next_bridge;
	struct drm_panel *panel;

	void __iomem *mmio;
	struct {
		struct clk *mod;		 
		struct clk *extal;		 
		struct clk *dotclkin[2];	 
	} clocks;

	struct drm_bridge *companion;
	enum rcar_lvds_link_type link_type;
};

#define bridge_to_rcar_lvds(b) \
	container_of(b, struct rcar_lvds, bridge)

static u32 rcar_lvds_read(struct rcar_lvds *lvds, u32 reg)
{
	return ioread32(lvds->mmio + reg);
}

static void rcar_lvds_write(struct rcar_lvds *lvds, u32 reg, u32 data)
{
	iowrite32(data, lvds->mmio + reg);
}

 

static void rcar_lvds_pll_setup_gen2(struct rcar_lvds *lvds, unsigned int freq)
{
	u32 val;

	if (freq < 39000000)
		val = LVDPLLCR_CEEN | LVDPLLCR_COSEL | LVDPLLCR_PLLDLYCNT_38M;
	else if (freq < 61000000)
		val = LVDPLLCR_CEEN | LVDPLLCR_COSEL | LVDPLLCR_PLLDLYCNT_60M;
	else if (freq < 121000000)
		val = LVDPLLCR_CEEN | LVDPLLCR_COSEL | LVDPLLCR_PLLDLYCNT_121M;
	else
		val = LVDPLLCR_PLLDLYCNT_150M;

	rcar_lvds_write(lvds, LVDPLLCR, val);
}

static void rcar_lvds_pll_setup_gen3(struct rcar_lvds *lvds, unsigned int freq)
{
	u32 val;

	if (freq < 42000000)
		val = LVDPLLCR_PLLDIVCNT_42M;
	else if (freq < 85000000)
		val = LVDPLLCR_PLLDIVCNT_85M;
	else if (freq < 128000000)
		val = LVDPLLCR_PLLDIVCNT_128M;
	else
		val = LVDPLLCR_PLLDIVCNT_148M;

	rcar_lvds_write(lvds, LVDPLLCR, val);
}

struct pll_info {
	unsigned long diff;
	unsigned int pll_m;
	unsigned int pll_n;
	unsigned int pll_e;
	unsigned int div;
	u32 clksel;
};

static void rcar_lvds_d3_e3_pll_calc(struct rcar_lvds *lvds, struct clk *clk,
				     unsigned long target, struct pll_info *pll,
				     u32 clksel, bool dot_clock_only)
{
	unsigned int div7 = dot_clock_only ? 1 : 7;
	unsigned long output;
	unsigned long fin;
	unsigned int m_min;
	unsigned int m_max;
	unsigned int m;
	int error;

	if (!clk)
		return;

	 

	fin = clk_get_rate(clk);
	if (fin < 12000000 || fin > 192000000)
		return;

	 
	m_min = max_t(unsigned int, 1, DIV_ROUND_UP(fin, 24000000));
	m_max = min_t(unsigned int, 8, fin / 12000000);

	for (m = m_min; m <= m_max; ++m) {
		unsigned long fpfd;
		unsigned int n_min;
		unsigned int n_max;
		unsigned int n;

		 
		fpfd = fin / m;
		n_min = max_t(unsigned int, 60, DIV_ROUND_UP(900000000, fpfd));
		n_max = min_t(unsigned int, 120, 1800000000 / fpfd);

		for (n = n_min; n < n_max; ++n) {
			unsigned long fvco;
			unsigned int e_min;
			unsigned int e;

			 
			fvco = fpfd * n;
			e_min = fvco > 1039500000 ? 1 : 0;

			for (e = e_min; e < 3; ++e) {
				unsigned long fout;
				unsigned long diff;
				unsigned int div;

				 
				fout = fvco / (1 << e) / div7;
				div = max(1UL, DIV_ROUND_CLOSEST(fout, target));
				diff = abs(fout / div - target);

				if (diff < pll->diff) {
					pll->diff = diff;
					pll->pll_m = m;
					pll->pll_n = n;
					pll->pll_e = e;
					pll->div = div;
					pll->clksel = clksel;

					if (diff == 0)
						goto done;
				}
			}
		}
	}

done:
	output = fin * pll->pll_n / pll->pll_m / (1 << pll->pll_e)
	       / div7 / pll->div;
	error = (long)(output - target) * 10000 / (long)target;

	dev_dbg(lvds->dev,
		"%pC %lu Hz -> Fout %lu Hz (target %lu Hz, error %d.%02u%%), PLL M/N/E/DIV %u/%u/%u/%u\n",
		clk, fin, output, target, error / 100,
		error < 0 ? -error % 100 : error % 100,
		pll->pll_m, pll->pll_n, pll->pll_e, pll->div);
}

static void rcar_lvds_pll_setup_d3_e3(struct rcar_lvds *lvds,
				      unsigned int freq, bool dot_clock_only)
{
	struct pll_info pll = { .diff = (unsigned long)-1 };
	u32 lvdpllcr;

	rcar_lvds_d3_e3_pll_calc(lvds, lvds->clocks.dotclkin[0], freq, &pll,
				 LVDPLLCR_CKSEL_DU_DOTCLKIN(0), dot_clock_only);
	rcar_lvds_d3_e3_pll_calc(lvds, lvds->clocks.dotclkin[1], freq, &pll,
				 LVDPLLCR_CKSEL_DU_DOTCLKIN(1), dot_clock_only);
	rcar_lvds_d3_e3_pll_calc(lvds, lvds->clocks.extal, freq, &pll,
				 LVDPLLCR_CKSEL_EXTAL, dot_clock_only);

	lvdpllcr = LVDPLLCR_PLLON | pll.clksel | LVDPLLCR_CLKOUT
		 | LVDPLLCR_PLLN(pll.pll_n - 1) | LVDPLLCR_PLLM(pll.pll_m - 1);

	if (pll.pll_e > 0)
		lvdpllcr |= LVDPLLCR_STP_CLKOUTE | LVDPLLCR_OUTCLKSEL
			 |  LVDPLLCR_PLLE(pll.pll_e - 1);

	if (dot_clock_only)
		lvdpllcr |= LVDPLLCR_OCKSEL;

	rcar_lvds_write(lvds, LVDPLLCR, lvdpllcr);

	if (pll.div > 1)
		 
		rcar_lvds_write(lvds, LVDDIV, LVDDIV_DIVSEL |
				LVDDIV_DIVRESET | LVDDIV_DIV(pll.div - 1));
	else
		rcar_lvds_write(lvds, LVDDIV, 0);
}

 

static enum rcar_lvds_mode rcar_lvds_get_lvds_mode(struct rcar_lvds *lvds,
					const struct drm_connector *connector)
{
	const struct drm_display_info *info;
	enum rcar_lvds_mode mode;

	 
	if (!lvds->panel)
		return RCAR_LVDS_MODE_JEIDA;

	info = &connector->display_info;
	if (!info->num_bus_formats || !info->bus_formats) {
		dev_warn(lvds->dev,
			 "no LVDS bus format reported, using JEIDA\n");
		return RCAR_LVDS_MODE_JEIDA;
	}

	switch (info->bus_formats[0]) {
	case MEDIA_BUS_FMT_RGB666_1X7X3_SPWG:
	case MEDIA_BUS_FMT_RGB888_1X7X4_JEIDA:
		mode = RCAR_LVDS_MODE_JEIDA;
		break;
	case MEDIA_BUS_FMT_RGB888_1X7X4_SPWG:
		mode = RCAR_LVDS_MODE_VESA;
		break;
	default:
		dev_warn(lvds->dev,
			 "unsupported LVDS bus format 0x%04x, using JEIDA\n",
			 info->bus_formats[0]);
		return RCAR_LVDS_MODE_JEIDA;
	}

	if (info->bus_flags & DRM_BUS_FLAG_DATA_LSB_TO_MSB)
		mode |= RCAR_LVDS_MODE_MIRROR;

	return mode;
}

static void rcar_lvds_enable(struct drm_bridge *bridge,
			     struct drm_atomic_state *state,
			     struct drm_crtc *crtc,
			     struct drm_connector *connector)
{
	struct rcar_lvds *lvds = bridge_to_rcar_lvds(bridge);
	u32 lvdhcr;
	u32 lvdcr0;
	int ret;

	ret = pm_runtime_resume_and_get(lvds->dev);
	if (ret)
		return;

	 
	if (lvds->link_type != RCAR_LVDS_SINGLE_LINK && lvds->companion)
		rcar_lvds_enable(lvds->companion, state, crtc, connector);

	 
	rcar_lvds_write(lvds, LVDCTRCR, LVDCTRCR_CTR3SEL_ZERO |
			LVDCTRCR_CTR2SEL_DISP | LVDCTRCR_CTR1SEL_VSYNC |
			LVDCTRCR_CTR0SEL_HSYNC);

	if (lvds->info->quirks & RCAR_LVDS_QUIRK_LANES)
		lvdhcr = LVDCHCR_CHSEL_CH(0, 0) | LVDCHCR_CHSEL_CH(1, 3)
		       | LVDCHCR_CHSEL_CH(2, 2) | LVDCHCR_CHSEL_CH(3, 1);
	else
		lvdhcr = LVDCHCR_CHSEL_CH(0, 0) | LVDCHCR_CHSEL_CH(1, 1)
		       | LVDCHCR_CHSEL_CH(2, 2) | LVDCHCR_CHSEL_CH(3, 3);

	rcar_lvds_write(lvds, LVDCHCR, lvdhcr);

	if (lvds->info->quirks & RCAR_LVDS_QUIRK_DUAL_LINK) {
		u32 lvdstripe = 0;

		if (lvds->link_type != RCAR_LVDS_SINGLE_LINK) {
			 
			bool swap_pixels = lvds->link_type ==
				RCAR_LVDS_DUAL_LINK_ODD_EVEN_PIXELS;

			 
			lvdstripe = LVDSTRIPE_ST_ON
				  | (lvds->companion && swap_pixels ?
				     LVDSTRIPE_ST_SWAP : 0);
		}
		rcar_lvds_write(lvds, LVDSTRIPE, lvdstripe);
	}

	 
	if ((lvds->link_type == RCAR_LVDS_SINGLE_LINK || lvds->companion) &&
	    !(lvds->info->quirks & RCAR_LVDS_QUIRK_EXT_PLL)) {
		const struct drm_crtc_state *crtc_state =
			drm_atomic_get_new_crtc_state(state, crtc);
		const struct drm_display_mode *mode =
			&crtc_state->adjusted_mode;

		lvds->info->pll_setup(lvds, mode->clock * 1000);
	}

	 
	lvdcr0 = rcar_lvds_get_lvds_mode(lvds, connector) << LVDCR0_LVMD_SHIFT;

	if (lvds->bridge.encoder) {
		if (drm_crtc_index(crtc) == 2)
			lvdcr0 |= LVDCR0_DUSEL;
	}

	rcar_lvds_write(lvds, LVDCR0, lvdcr0);

	 
	rcar_lvds_write(lvds, LVDCR1,
			LVDCR1_CHSTBY(3) | LVDCR1_CHSTBY(2) |
			LVDCR1_CHSTBY(1) | LVDCR1_CHSTBY(0) | LVDCR1_CLKSTBY);

	if (lvds->info->gen < 3) {
		 
		lvdcr0 |= LVDCR0_BEN | LVDCR0_LVEN;
		rcar_lvds_write(lvds, LVDCR0, lvdcr0);
	}

	if (!(lvds->info->quirks & RCAR_LVDS_QUIRK_EXT_PLL)) {
		 
		lvdcr0 |= LVDCR0_PLLON;
		rcar_lvds_write(lvds, LVDCR0, lvdcr0);
	}

	if (lvds->info->quirks & RCAR_LVDS_QUIRK_PWD) {
		 
		lvdcr0 |= LVDCR0_PWD;
		rcar_lvds_write(lvds, LVDCR0, lvdcr0);
	}

	if (lvds->info->quirks & RCAR_LVDS_QUIRK_GEN3_LVEN) {
		 
		lvdcr0 |= LVDCR0_LVEN;
		if (!(lvds->info->quirks & RCAR_LVDS_QUIRK_PWD))
			rcar_lvds_write(lvds, LVDCR0, lvdcr0);
	}

	if (!(lvds->info->quirks & RCAR_LVDS_QUIRK_EXT_PLL)) {
		 
		usleep_range(100, 150);
	}

	 
	lvdcr0 |= LVDCR0_LVRES;
	rcar_lvds_write(lvds, LVDCR0, lvdcr0);
}

static void rcar_lvds_disable(struct drm_bridge *bridge)
{
	struct rcar_lvds *lvds = bridge_to_rcar_lvds(bridge);
	u32 lvdcr0;

	 
	lvdcr0 = rcar_lvds_read(lvds, LVDCR0);

	lvdcr0 &= ~LVDCR0_LVRES;
	rcar_lvds_write(lvds, LVDCR0, lvdcr0);

	if (lvds->info->quirks & RCAR_LVDS_QUIRK_GEN3_LVEN) {
		lvdcr0 &= ~LVDCR0_LVEN;
		rcar_lvds_write(lvds, LVDCR0, lvdcr0);
	}

	if (lvds->info->quirks & RCAR_LVDS_QUIRK_PWD) {
		lvdcr0 &= ~LVDCR0_PWD;
		rcar_lvds_write(lvds, LVDCR0, lvdcr0);
	}

	if (!(lvds->info->quirks & RCAR_LVDS_QUIRK_EXT_PLL)) {
		lvdcr0 &= ~LVDCR0_PLLON;
		rcar_lvds_write(lvds, LVDCR0, lvdcr0);
	}

	rcar_lvds_write(lvds, LVDCR0, 0);
	rcar_lvds_write(lvds, LVDCR1, 0);

	 
	if (!(lvds->info->quirks & RCAR_LVDS_QUIRK_EXT_PLL))
		rcar_lvds_write(lvds, LVDPLLCR, 0);

	 
	if (lvds->link_type != RCAR_LVDS_SINGLE_LINK && lvds->companion)
		rcar_lvds_disable(lvds->companion);

	pm_runtime_put_sync(lvds->dev);
}

 

int rcar_lvds_pclk_enable(struct drm_bridge *bridge, unsigned long freq,
			  bool dot_clk_only)
{
	struct rcar_lvds *lvds = bridge_to_rcar_lvds(bridge);
	int ret;

	if (WARN_ON(!(lvds->info->quirks & RCAR_LVDS_QUIRK_EXT_PLL)))
		return -ENODEV;

	dev_dbg(lvds->dev, "enabling LVDS PLL, freq=%luHz\n", freq);

	ret = pm_runtime_resume_and_get(lvds->dev);
	if (ret)
		return ret;

	rcar_lvds_pll_setup_d3_e3(lvds, freq, dot_clk_only);

	return 0;
}
EXPORT_SYMBOL_GPL(rcar_lvds_pclk_enable);

void rcar_lvds_pclk_disable(struct drm_bridge *bridge, bool dot_clk_only)
{
	struct rcar_lvds *lvds = bridge_to_rcar_lvds(bridge);

	if (WARN_ON(!(lvds->info->quirks & RCAR_LVDS_QUIRK_EXT_PLL)))
		return;

	dev_dbg(lvds->dev, "disabling LVDS PLL\n");

	if (!dot_clk_only)
		rcar_lvds_disable(bridge);

	rcar_lvds_write(lvds, LVDPLLCR, 0);

	pm_runtime_put_sync(lvds->dev);
}
EXPORT_SYMBOL_GPL(rcar_lvds_pclk_disable);

 

static void rcar_lvds_atomic_enable(struct drm_bridge *bridge,
				    struct drm_bridge_state *old_bridge_state)
{
	struct drm_atomic_state *state = old_bridge_state->base.state;
	struct drm_connector *connector;
	struct drm_crtc *crtc;

	connector = drm_atomic_get_new_connector_for_encoder(state,
							     bridge->encoder);
	crtc = drm_atomic_get_new_connector_state(state, connector)->crtc;

	rcar_lvds_enable(bridge, state, crtc, connector);
}

static void rcar_lvds_atomic_disable(struct drm_bridge *bridge,
				     struct drm_bridge_state *old_bridge_state)
{
	struct rcar_lvds *lvds = bridge_to_rcar_lvds(bridge);

	 
	if (lvds->info->quirks & RCAR_LVDS_QUIRK_EXT_PLL)
		return;

	rcar_lvds_disable(bridge);
}

static bool rcar_lvds_mode_fixup(struct drm_bridge *bridge,
				 const struct drm_display_mode *mode,
				 struct drm_display_mode *adjusted_mode)
{
	struct rcar_lvds *lvds = bridge_to_rcar_lvds(bridge);
	int min_freq;

	 
	min_freq = lvds->info->quirks & RCAR_LVDS_QUIRK_EXT_PLL ? 5000 : 31000;
	adjusted_mode->clock = clamp(adjusted_mode->clock, min_freq, 148500);

	return true;
}

static int rcar_lvds_attach(struct drm_bridge *bridge,
			    enum drm_bridge_attach_flags flags)
{
	struct rcar_lvds *lvds = bridge_to_rcar_lvds(bridge);

	if (!lvds->next_bridge)
		return 0;

	return drm_bridge_attach(bridge->encoder, lvds->next_bridge, bridge,
				 flags);
}

static const struct drm_bridge_funcs rcar_lvds_bridge_ops = {
	.attach = rcar_lvds_attach,
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state,
	.atomic_reset = drm_atomic_helper_bridge_reset,
	.atomic_enable = rcar_lvds_atomic_enable,
	.atomic_disable = rcar_lvds_atomic_disable,
	.mode_fixup = rcar_lvds_mode_fixup,
};

bool rcar_lvds_dual_link(struct drm_bridge *bridge)
{
	struct rcar_lvds *lvds = bridge_to_rcar_lvds(bridge);

	return lvds->link_type != RCAR_LVDS_SINGLE_LINK;
}
EXPORT_SYMBOL_GPL(rcar_lvds_dual_link);

bool rcar_lvds_is_connected(struct drm_bridge *bridge)
{
	struct rcar_lvds *lvds = bridge_to_rcar_lvds(bridge);

	return lvds->next_bridge != NULL;
}
EXPORT_SYMBOL_GPL(rcar_lvds_is_connected);

 

static int rcar_lvds_parse_dt_companion(struct rcar_lvds *lvds)
{
	const struct of_device_id *match;
	struct device_node *companion;
	struct device_node *port0, *port1;
	struct rcar_lvds *companion_lvds;
	struct device *dev = lvds->dev;
	int dual_link;
	int ret = 0;

	 
	companion = of_parse_phandle(dev->of_node, "renesas,companion", 0);
	if (!companion)
		return 0;

	 
	match = of_match_device(dev->driver->of_match_table, dev);
	if (!of_device_is_compatible(companion, match->compatible)) {
		dev_err(dev, "Companion LVDS encoder is invalid\n");
		ret = -ENXIO;
		goto done;
	}

	 
	port0 = of_graph_get_port_by_id(dev->of_node, 1);
	port1 = of_graph_get_port_by_id(companion, 1);
	dual_link = drm_of_lvds_get_dual_link_pixel_order(port0, port1);
	of_node_put(port0);
	of_node_put(port1);

	switch (dual_link) {
	case DRM_LVDS_DUAL_LINK_ODD_EVEN_PIXELS:
		lvds->link_type = RCAR_LVDS_DUAL_LINK_ODD_EVEN_PIXELS;
		break;
	case DRM_LVDS_DUAL_LINK_EVEN_ODD_PIXELS:
		lvds->link_type = RCAR_LVDS_DUAL_LINK_EVEN_ODD_PIXELS;
		break;
	default:
		 
		if (lvds->next_bridge->timings &&
		    lvds->next_bridge->timings->dual_link)
			lvds->link_type = RCAR_LVDS_DUAL_LINK_EVEN_ODD_PIXELS;
		else
			lvds->link_type = RCAR_LVDS_SINGLE_LINK;
	}

	if (lvds->link_type == RCAR_LVDS_SINGLE_LINK) {
		dev_dbg(dev, "Single-link configuration detected\n");
		goto done;
	}

	lvds->companion = of_drm_find_bridge(companion);
	if (!lvds->companion) {
		ret = -EPROBE_DEFER;
		goto done;
	}

	dev_dbg(dev,
		"Dual-link configuration detected (companion encoder %pOF)\n",
		companion);

	if (lvds->link_type == RCAR_LVDS_DUAL_LINK_ODD_EVEN_PIXELS)
		dev_dbg(dev, "Data swapping required\n");

	 
	companion_lvds = bridge_to_rcar_lvds(lvds->companion);
	companion_lvds->link_type = lvds->link_type;

done:
	of_node_put(companion);

	return ret;
}

static int rcar_lvds_parse_dt(struct rcar_lvds *lvds)
{
	int ret;

	ret = drm_of_find_panel_or_bridge(lvds->dev->of_node, 1, 0,
					  &lvds->panel, &lvds->next_bridge);
	if (ret)
		goto done;

	if (lvds->panel) {
		lvds->next_bridge = devm_drm_panel_bridge_add(lvds->dev,
							      lvds->panel);
		if (IS_ERR_OR_NULL(lvds->next_bridge)) {
			ret = -EINVAL;
			goto done;
		}
	}

	if (lvds->info->quirks & RCAR_LVDS_QUIRK_DUAL_LINK)
		ret = rcar_lvds_parse_dt_companion(lvds);

done:
	 
	if (lvds->info->quirks & RCAR_LVDS_QUIRK_EXT_PLL)
		return ret == -ENODEV ? 0 : ret;

	return ret;
}

static struct clk *rcar_lvds_get_clock(struct rcar_lvds *lvds, const char *name,
				       bool optional)
{
	struct clk *clk;

	clk = devm_clk_get(lvds->dev, name);
	if (!IS_ERR(clk))
		return clk;

	if (PTR_ERR(clk) == -ENOENT && optional)
		return NULL;

	dev_err_probe(lvds->dev, PTR_ERR(clk), "failed to get %s clock\n",
		      name ? name : "module");

	return clk;
}

static int rcar_lvds_get_clocks(struct rcar_lvds *lvds)
{
	lvds->clocks.mod = rcar_lvds_get_clock(lvds, NULL, false);
	if (IS_ERR(lvds->clocks.mod))
		return PTR_ERR(lvds->clocks.mod);

	 
	if (!(lvds->info->quirks & RCAR_LVDS_QUIRK_EXT_PLL))
		return 0;

	lvds->clocks.extal = rcar_lvds_get_clock(lvds, "extal", true);
	if (IS_ERR(lvds->clocks.extal))
		return PTR_ERR(lvds->clocks.extal);

	lvds->clocks.dotclkin[0] = rcar_lvds_get_clock(lvds, "dclkin.0", true);
	if (IS_ERR(lvds->clocks.dotclkin[0]))
		return PTR_ERR(lvds->clocks.dotclkin[0]);

	lvds->clocks.dotclkin[1] = rcar_lvds_get_clock(lvds, "dclkin.1", true);
	if (IS_ERR(lvds->clocks.dotclkin[1]))
		return PTR_ERR(lvds->clocks.dotclkin[1]);

	 
	if (!lvds->clocks.extal && !lvds->clocks.dotclkin[0] &&
	    !lvds->clocks.dotclkin[1]) {
		dev_err(lvds->dev,
			"no input clock (extal, dclkin.0 or dclkin.1)\n");
		return -EINVAL;
	}

	return 0;
}

static const struct rcar_lvds_device_info rcar_lvds_r8a7790es1_info = {
	.gen = 2,
	.quirks = RCAR_LVDS_QUIRK_LANES,
	.pll_setup = rcar_lvds_pll_setup_gen2,
};

static const struct soc_device_attribute lvds_quirk_matches[] = {
	{
		.soc_id = "r8a7790", .revision = "ES1.*",
		.data = &rcar_lvds_r8a7790es1_info,
	},
	{   }
};

static int rcar_lvds_probe(struct platform_device *pdev)
{
	const struct soc_device_attribute *attr;
	struct rcar_lvds *lvds;
	int ret;

	lvds = devm_kzalloc(&pdev->dev, sizeof(*lvds), GFP_KERNEL);
	if (lvds == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, lvds);

	lvds->dev = &pdev->dev;
	lvds->info = of_device_get_match_data(&pdev->dev);

	attr = soc_device_match(lvds_quirk_matches);
	if (attr)
		lvds->info = attr->data;

	ret = rcar_lvds_parse_dt(lvds);
	if (ret < 0)
		return ret;

	lvds->bridge.funcs = &rcar_lvds_bridge_ops;
	lvds->bridge.of_node = pdev->dev.of_node;

	lvds->mmio = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(lvds->mmio))
		return PTR_ERR(lvds->mmio);

	ret = rcar_lvds_get_clocks(lvds);
	if (ret < 0)
		return ret;

	lvds->rstc = devm_reset_control_get_exclusive(&pdev->dev, NULL);
	if (IS_ERR(lvds->rstc))
		return dev_err_probe(&pdev->dev, PTR_ERR(lvds->rstc),
				     "failed to get cpg reset\n");

	pm_runtime_enable(&pdev->dev);

	drm_bridge_add(&lvds->bridge);

	return 0;
}

static void rcar_lvds_remove(struct platform_device *pdev)
{
	struct rcar_lvds *lvds = platform_get_drvdata(pdev);

	drm_bridge_remove(&lvds->bridge);

	pm_runtime_disable(&pdev->dev);
}

static const struct rcar_lvds_device_info rcar_lvds_gen2_info = {
	.gen = 2,
	.pll_setup = rcar_lvds_pll_setup_gen2,
};

static const struct rcar_lvds_device_info rcar_lvds_gen3_info = {
	.gen = 3,
	.quirks = RCAR_LVDS_QUIRK_PWD,
	.pll_setup = rcar_lvds_pll_setup_gen3,
};

static const struct rcar_lvds_device_info rcar_lvds_r8a77970_info = {
	.gen = 3,
	.quirks = RCAR_LVDS_QUIRK_PWD | RCAR_LVDS_QUIRK_GEN3_LVEN,
	.pll_setup = rcar_lvds_pll_setup_gen2,
};

static const struct rcar_lvds_device_info rcar_lvds_r8a77990_info = {
	.gen = 3,
	.quirks = RCAR_LVDS_QUIRK_GEN3_LVEN | RCAR_LVDS_QUIRK_EXT_PLL
		| RCAR_LVDS_QUIRK_DUAL_LINK,
};

static const struct rcar_lvds_device_info rcar_lvds_r8a77995_info = {
	.gen = 3,
	.quirks = RCAR_LVDS_QUIRK_GEN3_LVEN | RCAR_LVDS_QUIRK_PWD
		| RCAR_LVDS_QUIRK_EXT_PLL | RCAR_LVDS_QUIRK_DUAL_LINK,
};

static const struct of_device_id rcar_lvds_of_table[] = {
	{ .compatible = "renesas,r8a7742-lvds", .data = &rcar_lvds_gen2_info },
	{ .compatible = "renesas,r8a7743-lvds", .data = &rcar_lvds_gen2_info },
	{ .compatible = "renesas,r8a7744-lvds", .data = &rcar_lvds_gen2_info },
	{ .compatible = "renesas,r8a774a1-lvds", .data = &rcar_lvds_gen3_info },
	{ .compatible = "renesas,r8a774b1-lvds", .data = &rcar_lvds_gen3_info },
	{ .compatible = "renesas,r8a774c0-lvds", .data = &rcar_lvds_r8a77990_info },
	{ .compatible = "renesas,r8a774e1-lvds", .data = &rcar_lvds_gen3_info },
	{ .compatible = "renesas,r8a7790-lvds", .data = &rcar_lvds_gen2_info },
	{ .compatible = "renesas,r8a7791-lvds", .data = &rcar_lvds_gen2_info },
	{ .compatible = "renesas,r8a7793-lvds", .data = &rcar_lvds_gen2_info },
	{ .compatible = "renesas,r8a7795-lvds", .data = &rcar_lvds_gen3_info },
	{ .compatible = "renesas,r8a7796-lvds", .data = &rcar_lvds_gen3_info },
	{ .compatible = "renesas,r8a77961-lvds", .data = &rcar_lvds_gen3_info },
	{ .compatible = "renesas,r8a77965-lvds", .data = &rcar_lvds_gen3_info },
	{ .compatible = "renesas,r8a77970-lvds", .data = &rcar_lvds_r8a77970_info },
	{ .compatible = "renesas,r8a77980-lvds", .data = &rcar_lvds_gen3_info },
	{ .compatible = "renesas,r8a77990-lvds", .data = &rcar_lvds_r8a77990_info },
	{ .compatible = "renesas,r8a77995-lvds", .data = &rcar_lvds_r8a77995_info },
	{ }
};

MODULE_DEVICE_TABLE(of, rcar_lvds_of_table);

static int rcar_lvds_runtime_suspend(struct device *dev)
{
	struct rcar_lvds *lvds = dev_get_drvdata(dev);

	clk_disable_unprepare(lvds->clocks.mod);

	reset_control_assert(lvds->rstc);

	return 0;
}

static int rcar_lvds_runtime_resume(struct device *dev)
{
	struct rcar_lvds *lvds = dev_get_drvdata(dev);
	int ret;

	ret = reset_control_deassert(lvds->rstc);
	if (ret)
		return ret;

	ret = clk_prepare_enable(lvds->clocks.mod);
	if (ret < 0)
		goto err_reset_assert;

	return 0;

err_reset_assert:
	reset_control_assert(lvds->rstc);

	return ret;
}

static const struct dev_pm_ops rcar_lvds_pm_ops = {
	SET_RUNTIME_PM_OPS(rcar_lvds_runtime_suspend, rcar_lvds_runtime_resume, NULL)
};

static struct platform_driver rcar_lvds_platform_driver = {
	.probe		= rcar_lvds_probe,
	.remove_new	= rcar_lvds_remove,
	.driver		= {
		.name	= "rcar-lvds",
		.pm	= &rcar_lvds_pm_ops,
		.of_match_table = rcar_lvds_of_table,
	},
};

module_platform_driver(rcar_lvds_platform_driver);

MODULE_AUTHOR("Laurent Pinchart <laurent.pinchart@ideasonboard.com>");
MODULE_DESCRIPTION("Renesas R-Car LVDS Encoder Driver");
MODULE_LICENSE("GPL");
