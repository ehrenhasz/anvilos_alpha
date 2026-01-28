#ifndef _IMG_IR_RAW_H_
#define _IMG_IR_RAW_H_
struct img_ir_priv;
#ifdef CONFIG_IR_IMG_RAW
struct img_ir_priv_raw {
	struct rc_dev		*rdev;
	struct timer_list	timer;
	u32			last_status;
};
static inline bool img_ir_raw_enabled(struct img_ir_priv_raw *raw)
{
	return raw->rdev;
};
void img_ir_isr_raw(struct img_ir_priv *priv, u32 irq_status);
void img_ir_setup_raw(struct img_ir_priv *priv);
int img_ir_probe_raw(struct img_ir_priv *priv);
void img_ir_remove_raw(struct img_ir_priv *priv);
#else
struct img_ir_priv_raw {
};
static inline bool img_ir_raw_enabled(struct img_ir_priv_raw *raw)
{
	return false;
};
static inline void img_ir_isr_raw(struct img_ir_priv *priv, u32 irq_status)
{
}
static inline void img_ir_setup_raw(struct img_ir_priv *priv)
{
}
static inline int img_ir_probe_raw(struct img_ir_priv *priv)
{
	return -ENODEV;
}
static inline void img_ir_remove_raw(struct img_ir_priv *priv)
{
}
#endif  
#endif  
