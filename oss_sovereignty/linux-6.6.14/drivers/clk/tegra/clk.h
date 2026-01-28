#ifndef __TEGRA_CLK_H
#define __TEGRA_CLK_H
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/delay.h>
#define CLK_OUT_ENB_L			0x010
#define CLK_OUT_ENB_H			0x014
#define CLK_OUT_ENB_U			0x018
#define CLK_OUT_ENB_V			0x360
#define CLK_OUT_ENB_W			0x364
#define CLK_OUT_ENB_X			0x280
#define CLK_OUT_ENB_Y			0x298
#define CLK_ENB_PLLP_OUT_CPU		BIT(31)
#define CLK_OUT_ENB_SET_L		0x320
#define CLK_OUT_ENB_CLR_L		0x324
#define CLK_OUT_ENB_SET_H		0x328
#define CLK_OUT_ENB_CLR_H		0x32c
#define CLK_OUT_ENB_SET_U		0x330
#define CLK_OUT_ENB_CLR_U		0x334
#define CLK_OUT_ENB_SET_V		0x440
#define CLK_OUT_ENB_CLR_V		0x444
#define CLK_OUT_ENB_SET_W		0x448
#define CLK_OUT_ENB_CLR_W		0x44c
#define CLK_OUT_ENB_SET_X		0x284
#define CLK_OUT_ENB_CLR_X		0x288
#define CLK_OUT_ENB_SET_Y		0x29c
#define CLK_OUT_ENB_CLR_Y		0x2a0
#define RST_DEVICES_L			0x004
#define RST_DEVICES_H			0x008
#define RST_DEVICES_U			0x00C
#define RST_DEVICES_V			0x358
#define RST_DEVICES_W			0x35C
#define RST_DEVICES_X			0x28C
#define RST_DEVICES_Y			0x2a4
#define RST_DEVICES_SET_L		0x300
#define RST_DEVICES_CLR_L		0x304
#define RST_DEVICES_SET_H		0x308
#define RST_DEVICES_CLR_H		0x30c
#define RST_DEVICES_SET_U		0x310
#define RST_DEVICES_CLR_U		0x314
#define RST_DEVICES_SET_V		0x430
#define RST_DEVICES_CLR_V		0x434
#define RST_DEVICES_SET_W		0x438
#define RST_DEVICES_CLR_W		0x43c
#define RST_DEVICES_SET_X		0x290
#define RST_DEVICES_CLR_X		0x294
#define RST_DEVICES_SET_Y		0x2a8
#define RST_DEVICES_CLR_Y		0x2ac
#define TEGRA210_CLK_ENB_VLD_MSK_L	0xdcd7dff9
#define TEGRA210_CLK_ENB_VLD_MSK_H	0x87d1f3e7
#define TEGRA210_CLK_ENB_VLD_MSK_U	0xf3fed3fa
#define TEGRA210_CLK_ENB_VLD_MSK_V	0xffc18cfb
#define TEGRA210_CLK_ENB_VLD_MSK_W	0x793fb7ff
#define TEGRA210_CLK_ENB_VLD_MSK_X	0x3fe66fff
#define TEGRA210_CLK_ENB_VLD_MSK_Y	0xfc1fc7ff
struct tegra_clk_sync_source {
	struct		clk_hw hw;
	unsigned long	rate;
	unsigned long	max_rate;
};
#define to_clk_sync_source(_hw)					\
	container_of(_hw, struct tegra_clk_sync_source, hw)
extern const struct clk_ops tegra_clk_sync_source_ops;
extern int *periph_clk_enb_refcnt;
struct clk *tegra_clk_register_sync_source(const char *name,
					   unsigned long max_rate);
struct tegra_clk_frac_div {
	struct clk_hw	hw;
	void __iomem	*reg;
	u8		flags;
	u8		shift;
	u8		width;
	u8		frac_width;
	spinlock_t	*lock;
};
#define to_clk_frac_div(_hw) container_of(_hw, struct tegra_clk_frac_div, hw)
#define TEGRA_DIVIDER_ROUND_UP BIT(0)
#define TEGRA_DIVIDER_FIXED BIT(1)
#define TEGRA_DIVIDER_INT BIT(2)
#define TEGRA_DIVIDER_UART BIT(3)
extern const struct clk_ops tegra_clk_frac_div_ops;
struct clk *tegra_clk_register_divider(const char *name,
		const char *parent_name, void __iomem *reg,
		unsigned long flags, u8 clk_divider_flags, u8 shift, u8 width,
		u8 frac_width, spinlock_t *lock);
struct clk *tegra_clk_register_mc(const char *name, const char *parent_name,
				  void __iomem *reg, spinlock_t *lock);
struct tegra_clk_pll_freq_table {
	unsigned long	input_rate;
	unsigned long	output_rate;
	u32		n;
	u32		m;
	u8		p;
	u8		cpcon;
	u16		sdm_data;
};
struct pdiv_map {
	u8 pdiv;
	u8 hw_val;
};
struct div_nmp {
	u8		divn_shift;
	u8		divn_width;
	u8		divm_shift;
	u8		divm_width;
	u8		divp_shift;
	u8		divp_width;
	u8		override_divn_shift;
	u8		override_divm_shift;
	u8		override_divp_shift;
};
#define MAX_PLL_MISC_REG_COUNT	6
struct tegra_clk_pll;
struct tegra_clk_pll_params {
	unsigned long	input_min;
	unsigned long	input_max;
	unsigned long	cf_min;
	unsigned long	cf_max;
	unsigned long	vco_min;
	unsigned long	vco_max;
	u32		base_reg;
	u32		misc_reg;
	u32		lock_reg;
	u32		lock_mask;
	u32		lock_enable_bit_idx;
	u32		iddq_reg;
	u32		iddq_bit_idx;
	u32		reset_reg;
	u32		reset_bit_idx;
	u32		sdm_din_reg;
	u32		sdm_din_mask;
	u32		sdm_ctrl_reg;
	u32		sdm_ctrl_en_mask;
	u32		ssc_ctrl_reg;
	u32		ssc_ctrl_en_mask;
	u32		aux_reg;
	u32		dyn_ramp_reg;
	u32		ext_misc_reg[MAX_PLL_MISC_REG_COUNT];
	u32		pmc_divnm_reg;
	u32		pmc_divp_reg;
	u32		flags;
	int		stepa_shift;
	int		stepb_shift;
	int		lock_delay;
	int		max_p;
	bool		defaults_set;
	const struct pdiv_map *pdiv_tohw;
	struct div_nmp	*div_nmp;
	struct tegra_clk_pll_freq_table	*freq_table;
	unsigned long	fixed_rate;
	u16		mdiv_default;
	u32	(*round_p_to_pdiv)(u32 p, u32 *pdiv);
	void	(*set_gain)(struct tegra_clk_pll_freq_table *cfg);
	int	(*calc_rate)(struct clk_hw *hw,
			struct tegra_clk_pll_freq_table *cfg,
			unsigned long rate, unsigned long parent_rate);
	unsigned long	(*adjust_vco)(struct tegra_clk_pll_params *pll_params,
				unsigned long parent_rate);
	void	(*set_defaults)(struct tegra_clk_pll *pll);
	int	(*dyn_ramp)(struct tegra_clk_pll *pll,
			struct tegra_clk_pll_freq_table *cfg);
	int	(*pre_rate_change)(void);
	void	(*post_rate_change)(void);
};
#define TEGRA_PLL_USE_LOCK BIT(0)
#define TEGRA_PLL_HAS_CPCON BIT(1)
#define TEGRA_PLL_SET_LFCON BIT(2)
#define TEGRA_PLL_SET_DCCON BIT(3)
#define TEGRA_PLLU BIT(4)
#define TEGRA_PLLM BIT(5)
#define TEGRA_PLL_FIXED BIT(6)
#define TEGRA_PLLE_CONFIGURE BIT(7)
#define TEGRA_PLL_LOCK_MISC BIT(8)
#define TEGRA_PLL_BYPASS BIT(9)
#define TEGRA_PLL_HAS_LOCK_ENABLE BIT(10)
#define TEGRA_MDIV_NEW BIT(11)
#define TEGRA_PLLMB BIT(12)
#define TEGRA_PLL_VCO_OUT BIT(13)
struct tegra_clk_pll {
	struct clk_hw	hw;
	void __iomem	*clk_base;
	void __iomem	*pmc;
	spinlock_t	*lock;
	struct tegra_clk_pll_params	*params;
};
#define to_clk_pll(_hw) container_of(_hw, struct tegra_clk_pll, hw)
struct tegra_audio_clk_info {
	char *name;
	struct tegra_clk_pll_params *pll_params;
	int clk_id;
	char *parent;
};
extern const struct clk_ops tegra_clk_pll_ops;
extern const struct clk_ops tegra_clk_plle_ops;
struct clk *tegra_clk_register_pll(const char *name, const char *parent_name,
		void __iomem *clk_base, void __iomem *pmc,
		unsigned long flags, struct tegra_clk_pll_params *pll_params,
		spinlock_t *lock);
struct clk *tegra_clk_register_plle(const char *name, const char *parent_name,
		void __iomem *clk_base, void __iomem *pmc,
		unsigned long flags, struct tegra_clk_pll_params *pll_params,
		spinlock_t *lock);
struct clk *tegra_clk_register_pllxc(const char *name, const char *parent_name,
			    void __iomem *clk_base, void __iomem *pmc,
			    unsigned long flags,
			    struct tegra_clk_pll_params *pll_params,
			    spinlock_t *lock);
struct clk *tegra_clk_register_pllm(const char *name, const char *parent_name,
			   void __iomem *clk_base, void __iomem *pmc,
			   unsigned long flags,
			   struct tegra_clk_pll_params *pll_params,
			   spinlock_t *lock);
struct clk *tegra_clk_register_pllc(const char *name, const char *parent_name,
			   void __iomem *clk_base, void __iomem *pmc,
			   unsigned long flags,
			   struct tegra_clk_pll_params *pll_params,
			   spinlock_t *lock);
struct clk *tegra_clk_register_pllre(const char *name, const char *parent_name,
			   void __iomem *clk_base, void __iomem *pmc,
			   unsigned long flags,
			   struct tegra_clk_pll_params *pll_params,
			   spinlock_t *lock, unsigned long parent_rate);
struct clk *tegra_clk_register_pllre_tegra210(const char *name,
			   const char *parent_name, void __iomem *clk_base,
			   void __iomem *pmc, unsigned long flags,
			   struct tegra_clk_pll_params *pll_params,
			   spinlock_t *lock, unsigned long parent_rate);
struct clk *tegra_clk_register_plle_tegra114(const char *name,
				const char *parent_name,
				void __iomem *clk_base, unsigned long flags,
				struct tegra_clk_pll_params *pll_params,
				spinlock_t *lock);
struct clk *tegra_clk_register_plle_tegra210(const char *name,
				const char *parent_name,
				void __iomem *clk_base, unsigned long flags,
				struct tegra_clk_pll_params *pll_params,
				spinlock_t *lock);
struct clk *tegra_clk_register_pllc_tegra210(const char *name,
				const char *parent_name, void __iomem *clk_base,
				void __iomem *pmc, unsigned long flags,
				struct tegra_clk_pll_params *pll_params,
				spinlock_t *lock);
struct clk *tegra_clk_register_pllss_tegra210(const char *name,
				const char *parent_name, void __iomem *clk_base,
				unsigned long flags,
				struct tegra_clk_pll_params *pll_params,
				spinlock_t *lock);
struct clk *tegra_clk_register_pllss(const char *name, const char *parent_name,
			   void __iomem *clk_base, unsigned long flags,
			   struct tegra_clk_pll_params *pll_params,
			   spinlock_t *lock);
struct clk *tegra_clk_register_pllmb(const char *name, const char *parent_name,
			   void __iomem *clk_base, void __iomem *pmc,
			   unsigned long flags,
			   struct tegra_clk_pll_params *pll_params,
			   spinlock_t *lock);
struct clk *tegra_clk_register_pllu(const char *name, const char *parent_name,
				void __iomem *clk_base, unsigned long flags,
				struct tegra_clk_pll_params *pll_params,
				spinlock_t *lock);
struct clk *tegra_clk_register_pllu_tegra114(const char *name,
				const char *parent_name,
				void __iomem *clk_base, unsigned long flags,
				struct tegra_clk_pll_params *pll_params,
				spinlock_t *lock);
struct clk *tegra_clk_register_pllu_tegra210(const char *name,
				const char *parent_name,
				void __iomem *clk_base, unsigned long flags,
				struct tegra_clk_pll_params *pll_params,
				spinlock_t *lock);
struct tegra_clk_pll_out {
	struct clk_hw	hw;
	void __iomem	*reg;
	u8		enb_bit_idx;
	u8		rst_bit_idx;
	spinlock_t	*lock;
	u8		flags;
};
#define to_clk_pll_out(_hw) container_of(_hw, struct tegra_clk_pll_out, hw)
extern const struct clk_ops tegra_clk_pll_out_ops;
struct clk *tegra_clk_register_pll_out(const char *name,
		const char *parent_name, void __iomem *reg, u8 enb_bit_idx,
		u8 rst_bit_idx, unsigned long flags, u8 pll_div_flags,
		spinlock_t *lock);
struct tegra_clk_periph_regs {
	u32 enb_reg;
	u32 enb_set_reg;
	u32 enb_clr_reg;
	u32 rst_reg;
	u32 rst_set_reg;
	u32 rst_clr_reg;
};
struct tegra_clk_periph_gate {
	u32			magic;
	struct clk_hw		hw;
	void __iomem		*clk_base;
	u8			flags;
	int			clk_num;
	int			*enable_refcnt;
	const struct tegra_clk_periph_regs *regs;
};
#define to_clk_periph_gate(_hw)					\
	container_of(_hw, struct tegra_clk_periph_gate, hw)
#define TEGRA_CLK_PERIPH_GATE_MAGIC 0x17760309
#define TEGRA_PERIPH_NO_RESET BIT(0)
#define TEGRA_PERIPH_ON_APB BIT(2)
#define TEGRA_PERIPH_WAR_1005168 BIT(3)
#define TEGRA_PERIPH_NO_DIV BIT(4)
#define TEGRA_PERIPH_NO_GATE BIT(5)
extern const struct clk_ops tegra_clk_periph_gate_ops;
struct clk *tegra_clk_register_periph_gate(const char *name,
		const char *parent_name, u8 gate_flags, void __iomem *clk_base,
		unsigned long flags, int clk_num, int *enable_refcnt);
struct tegra_clk_periph_fixed {
	struct clk_hw hw;
	void __iomem *base;
	const struct tegra_clk_periph_regs *regs;
	unsigned int mul;
	unsigned int div;
	unsigned int num;
};
struct clk *tegra_clk_register_periph_fixed(const char *name,
					    const char *parent,
					    unsigned long flags,
					    void __iomem *base,
					    unsigned int mul,
					    unsigned int div,
					    unsigned int num);
struct tegra_clk_periph {
	u32			magic;
	struct clk_hw		hw;
	struct clk_mux		mux;
	struct tegra_clk_frac_div	divider;
	struct tegra_clk_periph_gate	gate;
	const struct clk_ops	*mux_ops;
	const struct clk_ops	*div_ops;
	const struct clk_ops	*gate_ops;
};
#define to_clk_periph(_hw) container_of(_hw, struct tegra_clk_periph, hw)
#define TEGRA_CLK_PERIPH_MAGIC 0x18221223
extern const struct clk_ops tegra_clk_periph_ops;
struct clk *tegra_clk_register_periph(const char *name,
		const char * const *parent_names, int num_parents,
		struct tegra_clk_periph *periph, void __iomem *clk_base,
		u32 offset, unsigned long flags);
struct clk *tegra_clk_register_periph_nodiv(const char *name,
		const char * const *parent_names, int num_parents,
		struct tegra_clk_periph *periph, void __iomem *clk_base,
		u32 offset);
#define TEGRA_CLK_PERIPH(_mux_shift, _mux_mask, _mux_flags,		\
			 _div_shift, _div_width, _div_frac_width,	\
			 _div_flags, _clk_num,\
			 _gate_flags, _table, _lock)			\
	{								\
		.mux = {						\
			.flags = _mux_flags,				\
			.shift = _mux_shift,				\
			.mask = _mux_mask,				\
			.table = _table,				\
			.lock = _lock,					\
		},							\
		.divider = {						\
			.flags = _div_flags,				\
			.shift = _div_shift,				\
			.width = _div_width,				\
			.frac_width = _div_frac_width,			\
			.lock = _lock,					\
		},							\
		.gate = {						\
			.flags = _gate_flags,				\
			.clk_num = _clk_num,				\
		},							\
		.mux_ops = &clk_mux_ops,				\
		.div_ops = &tegra_clk_frac_div_ops,			\
		.gate_ops = &tegra_clk_periph_gate_ops,			\
	}
struct tegra_periph_init_data {
	const char *name;
	int clk_id;
	union {
		const char *const *parent_names;
		const char *parent_name;
	} p;
	int num_parents;
	struct tegra_clk_periph periph;
	u32 offset;
	const char *con_id;
	const char *dev_id;
	unsigned long flags;
};
#define TEGRA_INIT_DATA_TABLE(_name, _con_id, _dev_id, _parent_names, _offset,\
			_mux_shift, _mux_mask, _mux_flags, _div_shift,	\
			_div_width, _div_frac_width, _div_flags,	\
			_clk_num, _gate_flags, _clk_id, _table,		\
			_flags, _lock) \
	{								\
		.name = _name,						\
		.clk_id = _clk_id,					\
		.p.parent_names = _parent_names,			\
		.num_parents = ARRAY_SIZE(_parent_names),		\
		.periph = TEGRA_CLK_PERIPH(_mux_shift, _mux_mask,	\
					   _mux_flags, _div_shift,	\
					   _div_width, _div_frac_width,	\
					   _div_flags, _clk_num,	\
					   _gate_flags, _table, _lock),	\
		.offset = _offset,					\
		.con_id = _con_id,					\
		.dev_id = _dev_id,					\
		.flags = _flags						\
	}
#define TEGRA_INIT_DATA(_name, _con_id, _dev_id, _parent_names, _offset,\
			_mux_shift, _mux_width, _mux_flags, _div_shift,	\
			_div_width, _div_frac_width, _div_flags, \
			_clk_num, _gate_flags, _clk_id)	\
	TEGRA_INIT_DATA_TABLE(_name, _con_id, _dev_id, _parent_names, _offset,\
			_mux_shift, BIT(_mux_width) - 1, _mux_flags,	\
			_div_shift, _div_width, _div_frac_width, _div_flags, \
			_clk_num, _gate_flags, _clk_id,\
			NULL, 0, NULL)
struct clk *tegra_clk_register_periph_data(void __iomem *clk_base,
					   struct tegra_periph_init_data *init);
struct tegra_clk_super_mux {
	struct clk_hw	hw;
	void __iomem	*reg;
	struct tegra_clk_frac_div frac_div;
	const struct clk_ops	*div_ops;
	u8		width;
	u8		flags;
	u8		div2_index;
	u8		pllx_index;
	spinlock_t	*lock;
};
#define to_clk_super_mux(_hw) container_of(_hw, struct tegra_clk_super_mux, hw)
#define TEGRA_DIVIDER_2 BIT(0)
#define TEGRA210_CPU_CLK BIT(1)
#define TEGRA20_SUPER_CLK BIT(2)
extern const struct clk_ops tegra_clk_super_ops;
struct clk *tegra_clk_register_super_mux(const char *name,
		const char **parent_names, u8 num_parents,
		unsigned long flags, void __iomem *reg, u8 clk_super_flags,
		u8 width, u8 pllx_index, u8 div2_index, spinlock_t *lock);
struct clk *tegra_clk_register_super_clk(const char *name,
		const char * const *parent_names, u8 num_parents,
		unsigned long flags, void __iomem *reg, u8 clk_super_flags,
		spinlock_t *lock);
struct clk *tegra_clk_register_super_cclk(const char *name,
		const char * const *parent_names, u8 num_parents,
		unsigned long flags, void __iomem *reg, u8 clk_super_flags,
		spinlock_t *lock);
int tegra_cclk_pre_pllx_rate_change(void);
void tegra_cclk_post_pllx_rate_change(void);
struct tegra_sdmmc_mux {
	struct clk_hw		hw;
	void __iomem		*reg;
	spinlock_t		*lock;
	const struct clk_ops	*gate_ops;
	struct tegra_clk_periph_gate	gate;
	u8			div_flags;
};
#define to_clk_sdmmc_mux(_hw) container_of(_hw, struct tegra_sdmmc_mux, hw)
struct clk *tegra_clk_register_sdmmc_mux_div(const char *name,
		void __iomem *clk_base, u32 offset, u32 clk_num, u8 div_flags,
		unsigned long flags, void *lock);
struct tegra_clk_init_table {
	unsigned int	clk_id;
	unsigned int	parent_id;
	unsigned long	rate;
	int		state;
};
struct tegra_clk_duplicate {
	int			clk_id;
	struct clk_lookup	lookup;
};
#define TEGRA_CLK_DUPLICATE(_clk_id, _dev, _con) \
	{					\
		.clk_id = _clk_id,		\
		.lookup = {			\
			.dev_id = _dev,		\
			.con_id = _con,		\
		},				\
	}
struct tegra_clk {
	int			dt_id;
	bool			present;
};
struct tegra_devclk {
	int		dt_id;
	char		*dev_id;
	char		*con_id;
};
void tegra_init_special_resets(unsigned int num, int (*assert)(unsigned long),
			       int (*deassert)(unsigned long));
void tegra_init_from_table(struct tegra_clk_init_table *tbl,
		struct clk *clks[], int clk_max);
void tegra_init_dup_clks(struct tegra_clk_duplicate *dup_list,
		struct clk *clks[], int clk_max);
const struct tegra_clk_periph_regs *get_reg_bank(int clkid);
struct clk **tegra_clk_init(void __iomem *clk_base, int num, int periph_banks);
struct clk **tegra_lookup_dt_id(int clk_id, struct tegra_clk *tegra_clk);
void tegra_add_of_provider(struct device_node *np, void *clk_src_onecell_get);
void tegra_register_devclks(struct tegra_devclk *dev_clks, int num);
void tegra_audio_clk_init(void __iomem *clk_base,
			void __iomem *pmc_base, struct tegra_clk *tegra_clks,
			struct tegra_audio_clk_info *audio_info,
			unsigned int num_plls, unsigned long sync_max_rate);
void tegra_periph_clk_init(void __iomem *clk_base, void __iomem *pmc_base,
			struct tegra_clk *tegra_clks,
			struct tegra_clk_pll_params *pll_params);
void tegra_fixed_clk_init(struct tegra_clk *tegra_clks);
int tegra_osc_clk_init(void __iomem *clk_base, struct tegra_clk *clks,
		       unsigned long *input_freqs, unsigned int num,
		       unsigned int clk_m_div, unsigned long *osc_freq,
		       unsigned long *pll_ref_freq);
void tegra_super_clk_gen4_init(void __iomem *clk_base,
			void __iomem *pmc_base, struct tegra_clk *tegra_clks,
			struct tegra_clk_pll_params *pll_params);
void tegra_super_clk_gen5_init(void __iomem *clk_base,
			void __iomem *pmc_base, struct tegra_clk *tegra_clks,
			struct tegra_clk_pll_params *pll_params);
#ifdef CONFIG_TEGRA124_CLK_EMC
struct clk *tegra124_clk_register_emc(void __iomem *base, struct device_node *np,
				      spinlock_t *lock);
bool tegra124_clk_emc_driver_available(struct clk_hw *emc_hw);
#else
static inline struct clk *
tegra124_clk_register_emc(void __iomem *base, struct device_node *np,
			  spinlock_t *lock)
{
	return NULL;
}
static inline bool tegra124_clk_emc_driver_available(struct clk_hw *emc_hw)
{
	return false;
}
#endif
void tegra114_clock_tune_cpu_trimmers_high(void);
void tegra114_clock_tune_cpu_trimmers_low(void);
void tegra114_clock_tune_cpu_trimmers_init(void);
void tegra114_clock_assert_dfll_dvco_reset(void);
void tegra114_clock_deassert_dfll_dvco_reset(void);
typedef void (*tegra_clk_apply_init_table_func)(void);
extern tegra_clk_apply_init_table_func tegra_clk_apply_init_table;
int tegra_pll_wait_for_lock(struct tegra_clk_pll *pll);
u16 tegra_pll_get_fixed_mdiv(struct clk_hw *hw, unsigned long input_rate);
int tegra_pll_p_div_to_hw(struct tegra_clk_pll *pll, u8 p_div);
int div_frac_get(unsigned long rate, unsigned parent_rate, u8 width,
		 u8 frac_width, u8 flags);
void tegra_clk_osc_resume(void __iomem *clk_base);
void tegra_clk_set_pllp_out_cpu(bool enable);
void tegra_clk_periph_suspend(void);
void tegra_clk_periph_resume(void);
#define fence_udelay(delay, reg)	\
	do {				\
		readl(reg);		\
		udelay(delay);		\
	} while (0)
bool tegra20_clk_emc_driver_available(struct clk_hw *emc_hw);
struct clk *tegra20_clk_register_emc(void __iomem *ioaddr, bool low_jitter);
struct clk *tegra210_clk_register_emc(struct device_node *np,
				      void __iomem *regs);
struct clk *tegra_clk_dev_register(struct clk_hw *hw);
#endif  
