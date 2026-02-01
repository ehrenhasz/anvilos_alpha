 
 

#ifndef _IMG_IR_HW_H_
#define _IMG_IR_HW_H_

#include <linux/kernel.h>
#include <media/rc-core.h>

 

#define IMG_IR_CODETYPE_PULSELEN	0x0	 
#define IMG_IR_CODETYPE_PULSEDIST	0x1	 
#define IMG_IR_CODETYPE_BIPHASE		0x2	 
#define IMG_IR_CODETYPE_2BITPULSEPOS	0x3	 


 

 
struct img_ir_control {
	unsigned decoden:1;
	unsigned code_type:2;
	unsigned hdrtog:1;
	unsigned ldrdec:1;
	unsigned decodinpol:1;
	unsigned bitorien:1;
	unsigned d1validsel:1;
	unsigned bitinv:1;
	unsigned decodend2:1;
	unsigned bitoriend2:1;
	unsigned bitinvd2:1;
};

 
struct img_ir_timing_range {
	u16 min;
	u16 max;
};

 
struct img_ir_symbol_timing {
	struct img_ir_timing_range pulse;
	struct img_ir_timing_range space;
};

 
struct img_ir_free_timing {
	 
	u8 minlen;
	u8 maxlen;
	u16 ft_min;
};

 
struct img_ir_timings {
	struct img_ir_symbol_timing ldr, s00, s01, s10, s11;
	struct img_ir_free_timing ft;
};

 
struct img_ir_filter {
	u64 data;
	u64 mask;
	u8 minlen;
	u8 maxlen;
};

 
struct img_ir_timing_regvals {
	u32 ldr, s00, s01, s10, s11, ft;
};

#define IMG_IR_SCANCODE		0	 
#define IMG_IR_REPEATCODE	1	 

 
struct img_ir_scancode_req {
	enum rc_proto protocol;
	u32 scancode;
	u8 toggle;
};

 
struct img_ir_decoder {
	 
	u64				type;
	unsigned int			tolerance;
	unsigned int			unit;
	struct img_ir_timings		timings;
	struct img_ir_timings		rtimings;
	unsigned int			repeat;
	struct img_ir_control		control;

	 
	int (*scancode)(int len, u64 raw, u64 enabled_protocols,
			struct img_ir_scancode_req *request);
	int (*filter)(const struct rc_scancode_filter *in,
		      struct img_ir_filter *out, u64 protocols);
};

extern struct img_ir_decoder img_ir_nec;
extern struct img_ir_decoder img_ir_jvc;
extern struct img_ir_decoder img_ir_sony;
extern struct img_ir_decoder img_ir_sharp;
extern struct img_ir_decoder img_ir_sanyo;
extern struct img_ir_decoder img_ir_rc5;
extern struct img_ir_decoder img_ir_rc6;

 
struct img_ir_reg_timings {
	u32				ctrl;
	struct img_ir_timing_regvals	timings;
	struct img_ir_timing_regvals	rtimings;
};

struct img_ir_priv;

#ifdef CONFIG_IR_IMG_HW

enum img_ir_mode {
	IMG_IR_M_NORMAL,
	IMG_IR_M_REPEATING,
#ifdef CONFIG_PM_SLEEP
	IMG_IR_M_WAKE,
#endif
};

 
struct img_ir_priv_hw {
	unsigned int			ct_quirks[4];
	struct rc_dev			*rdev;
	struct notifier_block		clk_nb;
	struct timer_list		end_timer;
	struct timer_list		suspend_timer;
	const struct img_ir_decoder	*decoder;
	u64				enabled_protocols;
	unsigned long			clk_hz;
	struct img_ir_reg_timings	reg_timings;
	unsigned int			flags;
	struct img_ir_filter		filters[RC_FILTER_MAX];

	enum img_ir_mode		mode;
	bool				stopping;
	u32				suspend_irqen;
	u32				quirk_suspend_irq;
};

static inline bool img_ir_hw_enabled(struct img_ir_priv_hw *hw)
{
	return hw->rdev;
};

void img_ir_isr_hw(struct img_ir_priv *priv, u32 irq_status);
void img_ir_setup_hw(struct img_ir_priv *priv);
int img_ir_probe_hw(struct img_ir_priv *priv);
void img_ir_remove_hw(struct img_ir_priv *priv);

#ifdef CONFIG_PM_SLEEP
int img_ir_suspend(struct device *dev);
int img_ir_resume(struct device *dev);
#else
#define img_ir_suspend NULL
#define img_ir_resume NULL
#endif

#else

struct img_ir_priv_hw {
};

static inline bool img_ir_hw_enabled(struct img_ir_priv_hw *hw)
{
	return false;
};
static inline void img_ir_isr_hw(struct img_ir_priv *priv, u32 irq_status)
{
}
static inline void img_ir_setup_hw(struct img_ir_priv *priv)
{
}
static inline int img_ir_probe_hw(struct img_ir_priv *priv)
{
	return -ENODEV;
}
static inline void img_ir_remove_hw(struct img_ir_priv *priv)
{
}

#define img_ir_suspend NULL
#define img_ir_resume NULL

#endif  

#endif  
