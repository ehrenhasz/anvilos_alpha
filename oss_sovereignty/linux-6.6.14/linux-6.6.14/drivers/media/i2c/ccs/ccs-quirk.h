#ifndef __CCS_QUIRK__
#define __CCS_QUIRK__
struct ccs_sensor;
struct ccs_quirk {
	int (*limits)(struct ccs_sensor *sensor);
	int (*post_poweron)(struct ccs_sensor *sensor);
	int (*pre_streamon)(struct ccs_sensor *sensor);
	int (*post_streamoff)(struct ccs_sensor *sensor);
	unsigned long (*pll_flags)(struct ccs_sensor *sensor);
	int (*init)(struct ccs_sensor *sensor);
	int (*reg_access)(struct ccs_sensor *sensor, bool write, u32 *reg,
			  u32 *val);
	unsigned long flags;
};
#define CCS_QUIRK_FLAG_8BIT_READ_ONLY			(1 << 0)
struct ccs_reg_8 {
	u16 reg;
	u8 val;
};
#define CCS_MK_QUIRK_REG_8(_reg, _val) \
	{				\
		.reg = (u16)_reg,	\
		.val = _val,		\
	}
#define ccs_call_quirk(sensor, _quirk, ...)				\
	((sensor)->minfo.quirk &&					\
	 (sensor)->minfo.quirk->_quirk ?				\
	 (sensor)->minfo.quirk->_quirk(sensor, ##__VA_ARGS__) : 0)
#define ccs_needs_quirk(sensor, _quirk)		\
	((sensor)->minfo.quirk ?			\
	 (sensor)->minfo.quirk->flags & _quirk : 0)
extern const struct ccs_quirk smiapp_jt8ev1_quirk;
extern const struct ccs_quirk smiapp_imx125es_quirk;
extern const struct ccs_quirk smiapp_jt8ew9_quirk;
extern const struct ccs_quirk smiapp_tcm8500md_quirk;
#endif  
