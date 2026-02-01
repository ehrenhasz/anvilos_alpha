
 

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/clk/ti.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/list.h>
#include <linux/regmap.h>
#include <linux/string_helpers.h>
#include <linux/memblock.h>
#include <linux/device.h>

#include "clock.h"

#undef pr_fmt
#define pr_fmt(fmt) "%s: " fmt, __func__

static LIST_HEAD(clk_hw_omap_clocks);
struct ti_clk_ll_ops *ti_clk_ll_ops;
static struct device_node *clocks_node_ptr[CLK_MAX_MEMMAPS];

struct ti_clk_features ti_clk_features;

struct clk_iomap {
	struct regmap *regmap;
	void __iomem *mem;
};

static struct clk_iomap *clk_memmaps[CLK_MAX_MEMMAPS];

static void clk_memmap_writel(u32 val, const struct clk_omap_reg *reg)
{
	struct clk_iomap *io = clk_memmaps[reg->index];

	if (reg->ptr)
		writel_relaxed(val, reg->ptr);
	else if (io->regmap)
		regmap_write(io->regmap, reg->offset, val);
	else
		writel_relaxed(val, io->mem + reg->offset);
}

static void _clk_rmw(u32 val, u32 mask, void __iomem *ptr)
{
	u32 v;

	v = readl_relaxed(ptr);
	v &= ~mask;
	v |= val;
	writel_relaxed(v, ptr);
}

static void clk_memmap_rmw(u32 val, u32 mask, const struct clk_omap_reg *reg)
{
	struct clk_iomap *io = clk_memmaps[reg->index];

	if (reg->ptr) {
		_clk_rmw(val, mask, reg->ptr);
	} else if (io->regmap) {
		regmap_update_bits(io->regmap, reg->offset, mask, val);
	} else {
		_clk_rmw(val, mask, io->mem + reg->offset);
	}
}

static u32 clk_memmap_readl(const struct clk_omap_reg *reg)
{
	u32 val;
	struct clk_iomap *io = clk_memmaps[reg->index];

	if (reg->ptr)
		val = readl_relaxed(reg->ptr);
	else if (io->regmap)
		regmap_read(io->regmap, reg->offset, &val);
	else
		val = readl_relaxed(io->mem + reg->offset);

	return val;
}

 
int ti_clk_setup_ll_ops(struct ti_clk_ll_ops *ops)
{
	if (ti_clk_ll_ops) {
		pr_err("Attempt to register ll_ops multiple times.\n");
		return -EBUSY;
	}

	ti_clk_ll_ops = ops;
	ops->clk_readl = clk_memmap_readl;
	ops->clk_writel = clk_memmap_writel;
	ops->clk_rmw = clk_memmap_rmw;

	return 0;
}

 
static struct device_node *ti_find_clock_provider(struct device_node *from,
						  const char *name)
{
	struct device_node *np;
	bool found = false;
	const char *n;
	char *tmp;

	tmp = kstrdup_and_replace(name, '-', '_', GFP_KERNEL);
	if (!tmp)
		return NULL;

	 
	for_each_of_allnodes_from(from, np) {
		if (of_property_read_string_index(np, "clock-output-names",
						  0, &n))
			continue;

		if (!strncmp(n, tmp, strlen(tmp))) {
			of_node_get(np);
			found = true;
			break;
		}
	}
	kfree(tmp);

	if (found) {
		of_node_put(from);
		return np;
	}

	 
	return of_find_node_by_name(from, name);
}

 
void __init ti_dt_clocks_register(struct ti_dt_clk oclks[])
{
	struct ti_dt_clk *c;
	struct device_node *node, *parent, *child;
	struct clk *clk;
	struct of_phandle_args clkspec;
	char buf[64];
	char *ptr;
	char *tags[2];
	int i;
	int num_args;
	int ret;
	static bool clkctrl_nodes_missing;
	static bool has_clkctrl_data;
	static bool compat_mode;

	compat_mode = ti_clk_get_features()->flags & TI_CLK_CLKCTRL_COMPAT;

	for (c = oclks; c->node_name != NULL; c++) {
		strcpy(buf, c->node_name);
		ptr = buf;
		for (i = 0; i < 2; i++)
			tags[i] = NULL;
		num_args = 0;
		while (*ptr) {
			if (*ptr == ':') {
				if (num_args >= 2) {
					pr_warn("Bad number of tags on %s\n",
						c->node_name);
					return;
				}
				tags[num_args++] = ptr + 1;
				*ptr = 0;
			}
			ptr++;
		}

		if (num_args && clkctrl_nodes_missing)
			continue;

		node = ti_find_clock_provider(NULL, buf);
		if (num_args && compat_mode) {
			parent = node;
			child = of_get_child_by_name(parent, "clock");
			if (!child)
				child = of_get_child_by_name(parent, "clk");
			if (child) {
				of_node_put(parent);
				node = child;
			}
		}

		clkspec.np = node;
		clkspec.args_count = num_args;
		for (i = 0; i < num_args; i++) {
			ret = kstrtoint(tags[i], i ? 10 : 16, clkspec.args + i);
			if (ret) {
				pr_warn("Bad tag in %s at %d: %s\n",
					c->node_name, i, tags[i]);
				of_node_put(node);
				return;
			}
		}
		clk = of_clk_get_from_provider(&clkspec);
		of_node_put(node);
		if (!IS_ERR(clk)) {
			c->lk.clk = clk;
			clkdev_add(&c->lk);
		} else {
			if (num_args && !has_clkctrl_data) {
				struct device_node *np;

				np = of_find_compatible_node(NULL, NULL,
							     "ti,clkctrl");
				if (np) {
					has_clkctrl_data = true;
					of_node_put(np);
				} else {
					clkctrl_nodes_missing = true;

					pr_warn("missing clkctrl nodes, please update your dts.\n");
					continue;
				}
			}

			pr_warn("failed to lookup clock node %s, ret=%ld\n",
				c->node_name, PTR_ERR(clk));
		}
	}
}

struct clk_init_item {
	struct device_node *node;
	void *user;
	ti_of_clk_init_cb_t func;
	struct list_head link;
};

static LIST_HEAD(retry_list);

 
int __init ti_clk_retry_init(struct device_node *node, void *user,
			     ti_of_clk_init_cb_t func)
{
	struct clk_init_item *retry;

	pr_debug("%pOFn: adding to retry list...\n", node);
	retry = kzalloc(sizeof(*retry), GFP_KERNEL);
	if (!retry)
		return -ENOMEM;

	retry->node = node;
	retry->func = func;
	retry->user = user;
	list_add(&retry->link, &retry_list);

	return 0;
}

 
int ti_clk_get_reg_addr(struct device_node *node, int index,
			struct clk_omap_reg *reg)
{
	u32 val;
	int i;

	for (i = 0; i < CLK_MAX_MEMMAPS; i++) {
		if (clocks_node_ptr[i] == node->parent)
			break;
		if (clocks_node_ptr[i] == node->parent->parent)
			break;
	}

	if (i == CLK_MAX_MEMMAPS) {
		pr_err("clk-provider not found for %pOFn!\n", node);
		return -ENOENT;
	}

	reg->index = i;

	if (of_property_read_u32_index(node, "reg", index, &val)) {
		if (of_property_read_u32_index(node->parent, "reg",
					       index, &val)) {
			pr_err("%pOFn or parent must have reg[%d]!\n",
			       node, index);
			return -EINVAL;
		}
	}

	reg->offset = val;
	reg->ptr = NULL;

	return 0;
}

void ti_clk_latch(struct clk_omap_reg *reg, s8 shift)
{
	u32 latch;

	if (shift < 0)
		return;

	latch = 1 << shift;

	ti_clk_ll_ops->clk_rmw(latch, latch, reg);
	ti_clk_ll_ops->clk_rmw(0, latch, reg);
	ti_clk_ll_ops->clk_readl(reg);  
}

 
int __init omap2_clk_provider_init(struct device_node *parent, int index,
				   struct regmap *syscon, void __iomem *mem)
{
	struct device_node *clocks;
	struct clk_iomap *io;

	 
	clocks = of_get_child_by_name(parent, "clocks");
	if (!clocks) {
		pr_err("%pOFn missing 'clocks' child node.\n", parent);
		return -EINVAL;
	}

	 
	clocks_node_ptr[index] = clocks;

	io = kzalloc(sizeof(*io), GFP_KERNEL);
	if (!io)
		return -ENOMEM;

	io->regmap = syscon;
	io->mem = mem;

	clk_memmaps[index] = io;

	return 0;
}

 
void __init omap2_clk_legacy_provider_init(int index, void __iomem *mem)
{
	struct clk_iomap *io;

	io = memblock_alloc(sizeof(*io), SMP_CACHE_BYTES);
	if (!io)
		panic("%s: Failed to allocate %zu bytes\n", __func__,
		      sizeof(*io));

	io->mem = mem;

	clk_memmaps[index] = io;
}

 
void ti_dt_clk_init_retry_clks(void)
{
	struct clk_init_item *retry;
	struct clk_init_item *tmp;
	int retries = 5;

	while (!list_empty(&retry_list) && retries) {
		list_for_each_entry_safe(retry, tmp, &retry_list, link) {
			pr_debug("retry-init: %pOFn\n", retry->node);
			retry->func(retry->user, retry->node);
			list_del(&retry->link);
			kfree(retry);
		}
		retries--;
	}
}

static const struct of_device_id simple_clk_match_table[] __initconst = {
	{ .compatible = "fixed-clock" },
	{ .compatible = "fixed-factor-clock" },
	{ }
};

 
const char *ti_dt_clk_name(struct device_node *np)
{
	const char *name;

	if (!of_property_read_string_index(np, "clock-output-names", 0,
					   &name))
		return name;

	return np->name;
}

 
void __init ti_clk_add_aliases(void)
{
	struct device_node *np;
	struct clk *clk;

	for_each_matching_node(np, simple_clk_match_table) {
		struct of_phandle_args clkspec;

		clkspec.np = np;
		clk = of_clk_get_from_provider(&clkspec);

		ti_clk_add_alias(clk, ti_dt_clk_name(np));
	}
}

 
void __init ti_clk_setup_features(struct ti_clk_features *features)
{
	memcpy(&ti_clk_features, features, sizeof(*features));
}

 
const struct ti_clk_features *ti_clk_get_features(void)
{
	return &ti_clk_features;
}

 
void omap2_clk_enable_init_clocks(const char **clk_names, u8 num_clocks)
{
	struct clk *init_clk;
	int i;

	for (i = 0; i < num_clocks; i++) {
		init_clk = clk_get(NULL, clk_names[i]);
		if (WARN(IS_ERR(init_clk), "could not find init clock %s\n",
			 clk_names[i]))
			continue;
		clk_prepare_enable(init_clk);
	}
}

 
int ti_clk_add_alias(struct clk *clk, const char *con)
{
	struct clk_lookup *cl;

	if (!clk)
		return 0;

	if (IS_ERR(clk))
		return PTR_ERR(clk);

	cl = kzalloc(sizeof(*cl), GFP_KERNEL);
	if (!cl)
		return -ENOMEM;

	cl->con_id = con;
	cl->clk = clk;

	clkdev_add(cl);

	return 0;
}

 
struct clk *of_ti_clk_register(struct device_node *node, struct clk_hw *hw,
			       const char *con)
{
	struct clk *clk;
	int ret;

	ret = of_clk_hw_register(node, hw);
	if (ret)
		return ERR_PTR(ret);

	clk = hw->clk;
	ret = ti_clk_add_alias(clk, con);
	if (ret) {
		clk_unregister(clk);
		return ERR_PTR(ret);
	}

	return clk;
}

 
struct clk *of_ti_clk_register_omap_hw(struct device_node *node,
				       struct clk_hw *hw, const char *con)
{
	struct clk *clk;
	struct clk_hw_omap *oclk;

	clk = of_ti_clk_register(node, hw, con);
	if (IS_ERR(clk))
		return clk;

	oclk = to_clk_hw_omap(hw);

	list_add(&oclk->node, &clk_hw_omap_clocks);

	return clk;
}

 
int omap2_clk_for_each(int (*fn)(struct clk_hw_omap *hw))
{
	int ret;
	struct clk_hw_omap *hw;

	list_for_each_entry(hw, &clk_hw_omap_clocks, node) {
		ret = (*fn)(hw);
		if (ret)
			break;
	}

	return ret;
}

 
bool omap2_clk_is_hw_omap(struct clk_hw *hw)
{
	struct clk_hw_omap *oclk;

	list_for_each_entry(oclk, &clk_hw_omap_clocks, node) {
		if (&oclk->hw == hw)
			return true;
	}

	return false;
}
