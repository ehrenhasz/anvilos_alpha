#ifndef _CLK_KONA_H
#define _CLK_KONA_H
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/clk-provider.h>
#define	BILLION		1000000000
#define PARENT_COUNT_MAX	((u32)U8_MAX)
#define BAD_CLK_INDEX		U8_MAX	 
#define BAD_CLK_NAME		((const char *)-1)
#define BAD_SCALED_DIV_VALUE	U64_MAX
#define FLAG(type, flag)		BCM_CLK_ ## type ## _FLAGS_ ## flag
#define FLAG_SET(obj, type, flag)	((obj)->flags |= FLAG(type, flag))
#define FLAG_CLEAR(obj, type, flag)	((obj)->flags &= ~(FLAG(type, flag)))
#define FLAG_FLIP(obj, type, flag)	((obj)->flags ^= FLAG(type, flag))
#define FLAG_TEST(obj, type, flag)	(!!((obj)->flags & FLAG(type, flag)))
#define ccu_policy_exists(ccu_policy)	((ccu_policy)->enable.offset != 0)
#define policy_exists(policy)		((policy)->offset != 0)
#define gate_exists(gate)		FLAG_TEST(gate, GATE, EXISTS)
#define gate_is_enabled(gate)		FLAG_TEST(gate, GATE, ENABLED)
#define gate_is_hw_controllable(gate)	FLAG_TEST(gate, GATE, HW)
#define gate_is_sw_controllable(gate)	FLAG_TEST(gate, GATE, SW)
#define gate_is_sw_managed(gate)	FLAG_TEST(gate, GATE, SW_MANAGED)
#define gate_is_no_disable(gate)	FLAG_TEST(gate, GATE, NO_DISABLE)
#define gate_flip_enabled(gate)		FLAG_FLIP(gate, GATE, ENABLED)
#define hyst_exists(hyst)		((hyst)->offset != 0)
#define divider_exists(div)		FLAG_TEST(div, DIV, EXISTS)
#define divider_is_fixed(div)		FLAG_TEST(div, DIV, FIXED)
#define divider_has_fraction(div)	(!divider_is_fixed(div) && \
						(div)->u.s.frac_width > 0)
#define selector_exists(sel)		((sel)->width != 0)
#define trigger_exists(trig)		FLAG_TEST(trig, TRIG, EXISTS)
#define policy_lvm_en_exists(enable)	((enable)->offset != 0)
#define policy_ctl_exists(control)	((control)->offset != 0)
enum bcm_clk_type {
	bcm_clk_none,		 
	bcm_clk_bus,
	bcm_clk_core,
	bcm_clk_peri
};
struct bcm_clk_policy {
	u32 offset;		 
	u32 bit;		 
};
#define POLICY(_offset, _bit)						\
	{								\
		.offset = (_offset),					\
		.bit = (_bit),						\
	}
struct bcm_clk_gate {
	u32 offset;		 
	u32 status_bit;		 
	u32 en_bit;		 
	u32 hw_sw_sel_bit;	 
	u32 flags;		 
};
#define BCM_CLK_GATE_FLAGS_EXISTS	((u32)1 << 0)	 
#define BCM_CLK_GATE_FLAGS_HW		((u32)1 << 1)	 
#define BCM_CLK_GATE_FLAGS_SW		((u32)1 << 2)	 
#define BCM_CLK_GATE_FLAGS_NO_DISABLE	((u32)1 << 3)	 
#define BCM_CLK_GATE_FLAGS_SW_MANAGED	((u32)1 << 4)	 
#define BCM_CLK_GATE_FLAGS_ENABLED	((u32)1 << 5)	 
#define HW_SW_GATE(_offset, _status_bit, _en_bit, _hw_sw_sel_bit)	\
	{								\
		.offset = (_offset),					\
		.status_bit = (_status_bit),				\
		.en_bit = (_en_bit),					\
		.hw_sw_sel_bit = (_hw_sw_sel_bit),			\
		.flags = FLAG(GATE, HW)|FLAG(GATE, SW)|			\
			FLAG(GATE, SW_MANAGED)|FLAG(GATE, ENABLED)|	\
			FLAG(GATE, EXISTS),				\
	}
#define HW_SW_GATE_AUTO(_offset, _status_bit, _en_bit, _hw_sw_sel_bit)	\
	{								\
		.offset = (_offset),					\
		.status_bit = (_status_bit),				\
		.en_bit = (_en_bit),					\
		.hw_sw_sel_bit = (_hw_sw_sel_bit),			\
		.flags = FLAG(GATE, HW)|FLAG(GATE, SW)|			\
			FLAG(GATE, EXISTS),				\
	}
#define HW_ENABLE_GATE(_offset, _status_bit, _en_bit, _hw_sw_sel_bit)	\
	{								\
		.offset = (_offset),					\
		.status_bit = (_status_bit),				\
		.en_bit = (_en_bit),					\
		.hw_sw_sel_bit = (_hw_sw_sel_bit),			\
		.flags = FLAG(GATE, HW)|FLAG(GATE, SW)|			\
			FLAG(GATE, NO_DISABLE)|FLAG(GATE, EXISTS),	\
	}
#define SW_ONLY_GATE(_offset, _status_bit, _en_bit)			\
	{								\
		.offset = (_offset),					\
		.status_bit = (_status_bit),				\
		.en_bit = (_en_bit),					\
		.flags = FLAG(GATE, SW)|FLAG(GATE, SW_MANAGED)|		\
			FLAG(GATE, ENABLED)|FLAG(GATE, EXISTS),		\
	}
#define HW_ONLY_GATE(_offset, _status_bit)				\
	{								\
		.offset = (_offset),					\
		.status_bit = (_status_bit),				\
		.flags = FLAG(GATE, HW)|FLAG(GATE, EXISTS),		\
	}
struct bcm_clk_hyst {
	u32 offset;		 
	u32 en_bit;		 
	u32 val_bit;		 
};
#define HYST(_offset, _en_bit, _val_bit)				\
	{								\
		.offset = (_offset),					\
		.en_bit = (_en_bit),					\
		.val_bit = (_val_bit),					\
	}
struct bcm_clk_div {
	union {
		struct {	 
			u32 offset;	 
			u32 shift;	 
			u32 width;	 
			u32 frac_width;	 
			u64 scaled_div;	 
		} s;
		u32 fixed;	 
	} u;
	u32 flags;		 
};
#define BCM_CLK_DIV_FLAGS_EXISTS	((u32)1 << 0)	 
#define BCM_CLK_DIV_FLAGS_FIXED		((u32)1 << 1)	 
#define FIXED_DIVIDER(_value)						\
	{								\
		.u.fixed = (_value),					\
		.flags = FLAG(DIV, EXISTS)|FLAG(DIV, FIXED),		\
	}
#define DIVIDER(_offset, _shift, _width)				\
	{								\
		.u.s.offset = (_offset),				\
		.u.s.shift = (_shift),					\
		.u.s.width = (_width),					\
		.u.s.scaled_div = BAD_SCALED_DIV_VALUE,			\
		.flags = FLAG(DIV, EXISTS),				\
	}
#define FRAC_DIVIDER(_offset, _shift, _width, _frac_width)		\
	{								\
		.u.s.offset = (_offset),				\
		.u.s.shift = (_shift),					\
		.u.s.width = (_width),					\
		.u.s.frac_width = (_frac_width),			\
		.u.s.scaled_div = BAD_SCALED_DIV_VALUE,			\
		.flags = FLAG(DIV, EXISTS),				\
	}
struct bcm_clk_sel {
	u32 offset;		 
	u32 shift;		 
	u32 width;		 
	u32 parent_count;	 
	u32 *parent_sel;	 
	u8 clk_index;		 
};
#define SELECTOR(_offset, _shift, _width)				\
	{								\
		.offset = (_offset),					\
		.shift = (_shift),					\
		.width = (_width),					\
		.clk_index = BAD_CLK_INDEX,				\
	}
struct bcm_clk_trig {
	u32 offset;		 
	u32 bit;		 
	u32 flags;		 
};
#define BCM_CLK_TRIG_FLAGS_EXISTS	((u32)1 << 0)	 
#define TRIGGER(_offset, _bit)						\
	{								\
		.offset = (_offset),					\
		.bit = (_bit),						\
		.flags = FLAG(TRIG, EXISTS),				\
	}
struct peri_clk_data {
	struct bcm_clk_policy policy;
	struct bcm_clk_gate gate;
	struct bcm_clk_hyst hyst;
	struct bcm_clk_trig pre_trig;
	struct bcm_clk_div pre_div;
	struct bcm_clk_trig trig;
	struct bcm_clk_div div;
	struct bcm_clk_sel sel;
	const char *clocks[];	 
};
#define CLOCKS(...)	{ __VA_ARGS__, NULL, }
#define NO_CLOCKS	{ NULL, }	 
struct kona_clk {
	struct clk_hw hw;
	struct clk_init_data init_data;	 
	struct ccu_data *ccu;	 
	enum bcm_clk_type type;
	union {
		void *data;
		struct peri_clk_data *peri;
	} u;
};
#define to_kona_clk(_hw) \
	container_of(_hw, struct kona_clk, hw)
#define KONA_CLK(_ccu_name, _clk_name, _type)				\
	{								\
		.init_data	= {					\
			.name = #_clk_name,				\
			.ops = &kona_ ## _type ## _clk_ops,		\
		},							\
		.ccu		= &_ccu_name ## _ccu_data,		\
		.type		= bcm_clk_ ## _type,			\
		.u.data		= &_clk_name ## _data,			\
	}
#define LAST_KONA_CLK	{ .type = bcm_clk_none }
struct bcm_lvm_en {
	u32 offset;		 
	u32 bit;		 
};
#define CCU_LVM_EN(_offset, _bit)					\
	{								\
		.offset = (_offset),					\
		.bit = (_bit),						\
	}
struct bcm_policy_ctl {
	u32 offset;		 
	u32 go_bit;
	u32 atl_bit;		 
	u32 ac_bit;
};
#define CCU_POLICY_CTL(_offset, _go_bit, _ac_bit, _atl_bit)		\
	{								\
		.offset = (_offset),					\
		.go_bit = (_go_bit),					\
		.ac_bit = (_ac_bit),					\
		.atl_bit = (_atl_bit),					\
	}
struct ccu_policy {
	struct bcm_lvm_en enable;
	struct bcm_policy_ctl control;
};
struct ccu_data {
	void __iomem *base;	 
	spinlock_t lock;	 
	bool write_enabled;	 
	struct ccu_policy policy;
	struct device_node *node;
	size_t clk_num;
	const char *name;
	u32 range;		 
	struct kona_clk kona_clks[];	 
};
#define KONA_CCU_COMMON(_prefix, _name, _ccuname)			    \
	.name		= #_name "_ccu",				    \
	.lock		= __SPIN_LOCK_UNLOCKED(_name ## _ccu_data.lock),    \
	.clk_num	= _prefix ## _ ## _ccuname ## _CCU_CLOCK_COUNT
extern struct clk_ops kona_peri_clk_ops;
extern u64 scaled_div_max(struct bcm_clk_div *div);
extern u64 scaled_div_build(struct bcm_clk_div *div, u32 div_value,
				u32 billionths);
extern void __init kona_dt_ccu_setup(struct ccu_data *ccu,
				struct device_node *node);
extern bool __init kona_ccu_init(struct ccu_data *ccu);
#endif  
