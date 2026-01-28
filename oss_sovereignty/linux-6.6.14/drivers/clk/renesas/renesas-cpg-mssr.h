#ifndef __CLK_RENESAS_CPG_MSSR_H__
#define __CLK_RENESAS_CPG_MSSR_H__
struct cpg_core_clk {
	const char *name;
	unsigned int id;
	unsigned int type;
	unsigned int parent;	 
	unsigned int div;
	unsigned int mult;
	unsigned int offset;
};
enum clk_types {
	CLK_TYPE_IN,		 
	CLK_TYPE_FF,		 
	CLK_TYPE_DIV6P1,	 
	CLK_TYPE_DIV6_RO,	 
	CLK_TYPE_FR,		 
	CLK_TYPE_CUSTOM,
};
#define DEF_TYPE(_name, _id, _type...)	\
	{ .name = _name, .id = _id, .type = _type }
#define DEF_BASE(_name, _id, _type, _parent...)	\
	DEF_TYPE(_name, _id, _type, .parent = _parent)
#define DEF_INPUT(_name, _id) \
	DEF_TYPE(_name, _id, CLK_TYPE_IN)
#define DEF_FIXED(_name, _id, _parent, _div, _mult)	\
	DEF_BASE(_name, _id, CLK_TYPE_FF, _parent, .div = _div, .mult = _mult)
#define DEF_DIV6P1(_name, _id, _parent, _offset)	\
	DEF_BASE(_name, _id, CLK_TYPE_DIV6P1, _parent, .offset = _offset)
#define DEF_DIV6_RO(_name, _id, _parent, _offset, _div)	\
	DEF_BASE(_name, _id, CLK_TYPE_DIV6_RO, _parent, .offset = _offset, .div = _div, .mult = 1)
#define DEF_RATE(_name, _id, _rate)	\
	DEF_TYPE(_name, _id, CLK_TYPE_FR, .mult = _rate)
struct mssr_mod_clk {
	const char *name;
	unsigned int id;
	unsigned int parent;	 
};
#define MOD_CLK_PACK(x)	((x) - ((x) / 100) * (100 - 32))
#define MOD_CLK_ID(x)	(MOD_CLK_BASE + MOD_CLK_PACK(x))
#define DEF_MOD(_name, _mod, _parent...)	\
	{ .name = _name, .id = MOD_CLK_ID(_mod), .parent = _parent }
#define MOD_CLK_PACK_10(x)	((x / 10) * 32 + (x % 10))
#define MOD_CLK_ID_10(x)	(MOD_CLK_BASE + MOD_CLK_PACK_10(x))
#define DEF_MOD_STB(_name, _mod, _parent...)	\
	{ .name = _name, .id = MOD_CLK_ID_10(_mod), .parent = _parent }
struct device_node;
enum clk_reg_layout {
	CLK_REG_LAYOUT_RCAR_GEN2_AND_GEN3 = 0,
	CLK_REG_LAYOUT_RZ_A,
	CLK_REG_LAYOUT_RCAR_GEN4,
};
struct cpg_mssr_info {
	const struct cpg_core_clk *early_core_clks;
	unsigned int num_early_core_clks;
	const struct mssr_mod_clk *early_mod_clks;
	unsigned int num_early_mod_clks;
	const struct cpg_core_clk *core_clks;
	unsigned int num_core_clks;
	unsigned int last_dt_core_clk;
	unsigned int num_total_core_clks;
	enum clk_reg_layout reg_layout;
	const struct mssr_mod_clk *mod_clks;
	unsigned int num_mod_clks;
	unsigned int num_hw_mod_clks;
	const unsigned int *crit_mod_clks;
	unsigned int num_crit_mod_clks;
	const unsigned int *core_pm_clks;
	unsigned int num_core_pm_clks;
	int (*init)(struct device *dev);
	struct clk *(*cpg_clk_register)(struct device *dev,
					const struct cpg_core_clk *core,
					const struct cpg_mssr_info *info,
					struct clk **clks, void __iomem *base,
					struct raw_notifier_head *notifiers);
};
extern const struct cpg_mssr_info r7s9210_cpg_mssr_info;
extern const struct cpg_mssr_info r8a7742_cpg_mssr_info;
extern const struct cpg_mssr_info r8a7743_cpg_mssr_info;
extern const struct cpg_mssr_info r8a7745_cpg_mssr_info;
extern const struct cpg_mssr_info r8a77470_cpg_mssr_info;
extern const struct cpg_mssr_info r8a774a1_cpg_mssr_info;
extern const struct cpg_mssr_info r8a774b1_cpg_mssr_info;
extern const struct cpg_mssr_info r8a774c0_cpg_mssr_info;
extern const struct cpg_mssr_info r8a774e1_cpg_mssr_info;
extern const struct cpg_mssr_info r8a7790_cpg_mssr_info;
extern const struct cpg_mssr_info r8a7791_cpg_mssr_info;
extern const struct cpg_mssr_info r8a7792_cpg_mssr_info;
extern const struct cpg_mssr_info r8a7794_cpg_mssr_info;
extern const struct cpg_mssr_info r8a7795_cpg_mssr_info;
extern const struct cpg_mssr_info r8a7796_cpg_mssr_info;
extern const struct cpg_mssr_info r8a77965_cpg_mssr_info;
extern const struct cpg_mssr_info r8a77970_cpg_mssr_info;
extern const struct cpg_mssr_info r8a77980_cpg_mssr_info;
extern const struct cpg_mssr_info r8a77990_cpg_mssr_info;
extern const struct cpg_mssr_info r8a77995_cpg_mssr_info;
extern const struct cpg_mssr_info r8a779a0_cpg_mssr_info;
extern const struct cpg_mssr_info r8a779f0_cpg_mssr_info;
extern const struct cpg_mssr_info r8a779g0_cpg_mssr_info;
void __init cpg_mssr_early_init(struct device_node *np,
				const struct cpg_mssr_info *info);
extern void mssr_mod_nullify(struct mssr_mod_clk *mod_clks,
			     unsigned int num_mod_clks,
			     const unsigned int *clks, unsigned int n);
#endif
