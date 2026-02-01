 

#ifndef __SH_PFC_H
#define __SH_PFC_H

#include <linux/bug.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/spinlock.h>
#include <linux/stringify.h>

enum {
	PINMUX_TYPE_NONE,
	PINMUX_TYPE_FUNCTION,
	PINMUX_TYPE_GPIO,
	PINMUX_TYPE_OUTPUT,
	PINMUX_TYPE_INPUT,
};

#define SH_PFC_PIN_NONE			U16_MAX

#define SH_PFC_PIN_CFG_INPUT		(1 << 0)
#define SH_PFC_PIN_CFG_OUTPUT		(1 << 1)
#define SH_PFC_PIN_CFG_PULL_UP		(1 << 2)
#define SH_PFC_PIN_CFG_PULL_DOWN	(1 << 3)
#define SH_PFC_PIN_CFG_PULL_UP_DOWN	(SH_PFC_PIN_CFG_PULL_UP | \
					 SH_PFC_PIN_CFG_PULL_DOWN)

#define SH_PFC_PIN_CFG_IO_VOLTAGE_MASK	GENMASK(5, 4)
#define SH_PFC_PIN_CFG_IO_VOLTAGE_18_25	(1 << 4)
#define SH_PFC_PIN_CFG_IO_VOLTAGE_18_33	(2 << 4)
#define SH_PFC_PIN_CFG_IO_VOLTAGE_25_33	(3 << 4)

#define SH_PFC_PIN_CFG_DRIVE_STRENGTH	(1 << 6)

#define SH_PFC_PIN_CFG_NO_GPIO		(1 << 31)

struct sh_pfc_pin {
	const char *name;
	unsigned int configs;
	u16 pin;
	u16 enum_id;
};

#define SH_PFC_PIN_GROUP_ALIAS(alias, _name) {				\
	.name = #alias,							\
	.pins = _name##_pins,						\
	.mux = _name##_mux,						\
	.nr_pins = ARRAY_SIZE(_name##_pins) +				\
	BUILD_BUG_ON_ZERO(sizeof(_name##_pins) != sizeof(_name##_mux)),	\
}
#define SH_PFC_PIN_GROUP(name)	SH_PFC_PIN_GROUP_ALIAS(name, name)

 
#define SH_PFC_PIN_GROUP_SUBSET(_name, data, first, n) {		\
	.name = #_name,							\
	.pins = data##_pins + first,					\
	.mux = data##_mux + first,					\
	.nr_pins = n +							\
	BUILD_BUG_ON_ZERO(first + n > ARRAY_SIZE(data##_pins)) +	\
	BUILD_BUG_ON_ZERO(first + n > ARRAY_SIZE(data##_mux)),		\
}

 
#define BUS_DATA_PIN_GROUP(base, n, ...)				\
	SH_PFC_PIN_GROUP_SUBSET(base##n##__VA_ARGS__, base##__VA_ARGS__, 0, n)

struct sh_pfc_pin_group {
	const char *name;
	const unsigned int *pins;
	const unsigned int *mux;
	unsigned int nr_pins;
};

#define SH_PFC_FUNCTION(n) {						\
	.name = #n,							\
	.groups = n##_groups,						\
	.nr_groups = ARRAY_SIZE(n##_groups),				\
}

struct sh_pfc_function {
	const char *name;
	const char * const *groups;
	unsigned int nr_groups;
};

struct pinmux_func {
	u16 enum_id;
	const char *name;
};

struct pinmux_cfg_reg {
	u32 reg;
	u8 reg_width, field_width;
#ifdef DEBUG
	u16 nr_enum_ids;	 
#define SET_NR_ENUM_IDS(n)	.nr_enum_ids = n,
#else
#define SET_NR_ENUM_IDS(n)
#endif
	const u16 *enum_ids;
	const s8 *var_field_width;
};

#define GROUP(...)	__VA_ARGS__

 
#define PINMUX_CFG_REG(name, r, r_width, f_width, ids)			\
	.reg = r, .reg_width = r_width,					\
	.field_width = f_width + BUILD_BUG_ON_ZERO(r_width % f_width) +	\
	BUILD_BUG_ON_ZERO(sizeof((const u16 []) { ids }) / sizeof(u16) != \
			  (r_width / f_width) << f_width),		\
	.enum_ids = (const u16 [(r_width / f_width) << f_width]) { ids }

 
#define PINMUX_CFG_REG_VAR(name, r, r_width, f_widths, ids)		\
	.reg = r, .reg_width = r_width,					\
	.var_field_width = (const s8 []) { f_widths, 0 },		\
	SET_NR_ENUM_IDS(sizeof((const u16 []) { ids }) / sizeof(u16))	\
	.enum_ids = (const u16 []) { ids }

struct pinmux_drive_reg_field {
	u16 pin;
	u8 offset;
	u8 size;
};

struct pinmux_drive_reg {
	u32 reg;
	const struct pinmux_drive_reg_field fields[10];
};

#define PINMUX_DRIVE_REG(name, r) \
	.reg = r, \
	.fields =

struct pinmux_bias_reg {	 
	u32 puen;		 
	u32 pud;		 
	const u16 pins[32];
};

#define PINMUX_BIAS_REG(name1, r1, name2, r2) \
	.puen = r1,	\
	.pud = r2,	\
	.pins =

struct pinmux_ioctrl_reg {
	u32 reg;
};

struct pinmux_data_reg {
	u32 reg;
	u8 reg_width;
	const u16 *enum_ids;
};

 
#define PINMUX_DATA_REG(name, r, r_width, ids)				\
	.reg = r, .reg_width = r_width +				\
	BUILD_BUG_ON_ZERO(sizeof((const u16 []) { ids }) / sizeof(u16) != \
			  r_width),					\
	.enum_ids = (const u16 [r_width]) { ids }

struct pinmux_irq {
	const short *gpios;
};

 
#define PINMUX_IRQ(ids...) {						\
	.gpios = (const short []) { ids, -1 }				\
}

struct pinmux_range {
	u16 begin;
	u16 end;
	u16 force;
};

struct sh_pfc_window {
	phys_addr_t phys;
	void __iomem *virt;
	unsigned long size;
};

struct sh_pfc_pin_range;

struct sh_pfc {
	struct device *dev;
	const struct sh_pfc_soc_info *info;
	spinlock_t lock;

	unsigned int num_windows;
	struct sh_pfc_window *windows;
	unsigned int num_irqs;
	unsigned int *irqs;

	struct sh_pfc_pin_range *ranges;
	unsigned int nr_ranges;

	unsigned int nr_gpio_pins;

	struct sh_pfc_chip *gpio;
	u32 *saved_regs;
};

struct sh_pfc_soc_operations {
	int (*init)(struct sh_pfc *pfc);
	unsigned int (*get_bias)(struct sh_pfc *pfc, unsigned int pin);
	void (*set_bias)(struct sh_pfc *pfc, unsigned int pin,
			 unsigned int bias);
	int (*pin_to_pocctrl)(unsigned int pin, u32 *pocctrl);
	int (*pin_to_portcr)(unsigned int pin);
};

struct sh_pfc_soc_info {
	const char *name;
	const struct sh_pfc_soc_operations *ops;

#ifdef CONFIG_PINCTRL_SH_PFC_GPIO
	struct pinmux_range input;
	struct pinmux_range output;
	const struct pinmux_irq *gpio_irq;
	unsigned int gpio_irq_size;
#endif

	struct pinmux_range function;

	const struct sh_pfc_pin *pins;
	unsigned int nr_pins;
	const struct sh_pfc_pin_group *groups;
	unsigned int nr_groups;
	const struct sh_pfc_function *functions;
	unsigned int nr_functions;

#ifdef CONFIG_PINCTRL_SH_FUNC_GPIO
	const struct pinmux_func *func_gpios;
	unsigned int nr_func_gpios;
#endif

	const struct pinmux_cfg_reg *cfg_regs;
	const struct pinmux_drive_reg *drive_regs;
	const struct pinmux_bias_reg *bias_regs;
	const struct pinmux_ioctrl_reg *ioctrl_regs;
	const struct pinmux_data_reg *data_regs;

	const u16 *pinmux_data;
	unsigned int pinmux_data_size;

	u32 unlock_reg;		 
};

extern const struct sh_pfc_soc_info emev2_pinmux_info;
extern const struct sh_pfc_soc_info r8a73a4_pinmux_info;
extern const struct sh_pfc_soc_info r8a7740_pinmux_info;
extern const struct sh_pfc_soc_info r8a7742_pinmux_info;
extern const struct sh_pfc_soc_info r8a7743_pinmux_info;
extern const struct sh_pfc_soc_info r8a7744_pinmux_info;
extern const struct sh_pfc_soc_info r8a7745_pinmux_info;
extern const struct sh_pfc_soc_info r8a77470_pinmux_info;
extern const struct sh_pfc_soc_info r8a774a1_pinmux_info;
extern const struct sh_pfc_soc_info r8a774b1_pinmux_info;
extern const struct sh_pfc_soc_info r8a774c0_pinmux_info;
extern const struct sh_pfc_soc_info r8a774e1_pinmux_info;
extern const struct sh_pfc_soc_info r8a7778_pinmux_info;
extern const struct sh_pfc_soc_info r8a7779_pinmux_info;
extern const struct sh_pfc_soc_info r8a7790_pinmux_info;
extern const struct sh_pfc_soc_info r8a7791_pinmux_info;
extern const struct sh_pfc_soc_info r8a7792_pinmux_info;
extern const struct sh_pfc_soc_info r8a7793_pinmux_info;
extern const struct sh_pfc_soc_info r8a7794_pinmux_info;
extern const struct sh_pfc_soc_info r8a77951_pinmux_info;
extern const struct sh_pfc_soc_info r8a77960_pinmux_info;
extern const struct sh_pfc_soc_info r8a77961_pinmux_info;
extern const struct sh_pfc_soc_info r8a77965_pinmux_info;
extern const struct sh_pfc_soc_info r8a77970_pinmux_info;
extern const struct sh_pfc_soc_info r8a77980_pinmux_info;
extern const struct sh_pfc_soc_info r8a77990_pinmux_info;
extern const struct sh_pfc_soc_info r8a77995_pinmux_info;
extern const struct sh_pfc_soc_info r8a779a0_pinmux_info;
extern const struct sh_pfc_soc_info r8a779f0_pinmux_info;
extern const struct sh_pfc_soc_info r8a779g0_pinmux_info;
extern const struct sh_pfc_soc_info sh7203_pinmux_info;
extern const struct sh_pfc_soc_info sh7264_pinmux_info;
extern const struct sh_pfc_soc_info sh7269_pinmux_info;
extern const struct sh_pfc_soc_info sh73a0_pinmux_info;
extern const struct sh_pfc_soc_info sh7720_pinmux_info;
extern const struct sh_pfc_soc_info sh7722_pinmux_info;
extern const struct sh_pfc_soc_info sh7723_pinmux_info;
extern const struct sh_pfc_soc_info sh7724_pinmux_info;
extern const struct sh_pfc_soc_info sh7734_pinmux_info;
extern const struct sh_pfc_soc_info sh7757_pinmux_info;
extern const struct sh_pfc_soc_info sh7785_pinmux_info;
extern const struct sh_pfc_soc_info sh7786_pinmux_info;
extern const struct sh_pfc_soc_info shx3_pinmux_info;

 

 

 
#define PINMUX_DATA(data_or_mark, ids...)	data_or_mark, ids, 0

 
#define PINMUX_IPSR_NOGP(ipsr, fn)					\
	PINMUX_DATA(fn##_MARK, FN_##fn)

 
#define PINMUX_IPSR_GPSR(ipsr, fn)					\
	PINMUX_DATA(fn##_MARK, FN_##fn, FN_##ipsr)

 
#define PINMUX_IPSR_NOGM(ipsr, fn, msel)				\
	PINMUX_DATA(fn##_MARK, FN_##fn, FN_##msel)

 
#define PINMUX_IPSR_NOFN(gpsr, fn, gsel)				\
	PINMUX_DATA(fn##_MARK, FN_##gpsr, FN_##gsel)

 
#define PINMUX_IPSR_MSEL(ipsr, fn, msel)				\
	PINMUX_DATA(fn##_MARK, FN_##msel, FN_##fn, FN_##ipsr)

 
#define PINMUX_IPSR_PHYS_MSEL(ipsr, fn, psel, msel) \
	PINMUX_DATA(fn##_MARK, FN_##psel, FN_##msel, FN_##fn, FN_##ipsr)

 
#define PINMUX_IPSR_PHYS(ipsr, fn, psel) \
	PINMUX_DATA(fn##_MARK, FN_##psel, FN_##ipsr)

 
#define PINMUX_SINGLE(fn)						\
	PINMUX_DATA(fn##_MARK, FN_##fn)

 

#define PORT_GP_CFG_1(bank, pin, fn, sfx, cfg)				\
	fn(bank, pin, GP_##bank##_##pin, sfx, cfg)
#define PORT_GP_1(bank, pin, fn, sfx)	PORT_GP_CFG_1(bank, pin, fn, sfx, 0)

#define PORT_GP_CFG_2(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_1(bank, 0,  fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 1,  fn, sfx, cfg)
#define PORT_GP_2(bank, fn, sfx)	PORT_GP_CFG_2(bank, fn, sfx, 0)

#define PORT_GP_CFG_4(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_2(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 2,  fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 3,  fn, sfx, cfg)
#define PORT_GP_4(bank, fn, sfx)	PORT_GP_CFG_4(bank, fn, sfx, 0)

#define PORT_GP_CFG_6(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_4(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 4,  fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 5,  fn, sfx, cfg)
#define PORT_GP_6(bank, fn, sfx)	PORT_GP_CFG_6(bank, fn, sfx, 0)

#define PORT_GP_CFG_7(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_6(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 6,  fn, sfx, cfg)
#define PORT_GP_7(bank, fn, sfx)	PORT_GP_CFG_7(bank, fn, sfx, 0)

#define PORT_GP_CFG_8(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_7(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 7,  fn, sfx, cfg)
#define PORT_GP_8(bank, fn, sfx)	PORT_GP_CFG_8(bank, fn, sfx, 0)

#define PORT_GP_CFG_9(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_8(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 8,  fn, sfx, cfg)
#define PORT_GP_9(bank, fn, sfx)	PORT_GP_CFG_9(bank, fn, sfx, 0)

#define PORT_GP_CFG_10(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_9(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 9,  fn, sfx, cfg)
#define PORT_GP_10(bank, fn, sfx)	PORT_GP_CFG_10(bank, fn, sfx, 0)

#define PORT_GP_CFG_11(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_10(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 10, fn, sfx, cfg)
#define PORT_GP_11(bank, fn, sfx)	PORT_GP_CFG_11(bank, fn, sfx, 0)

#define PORT_GP_CFG_12(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_11(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 11, fn, sfx, cfg)
#define PORT_GP_12(bank, fn, sfx)	PORT_GP_CFG_12(bank, fn, sfx, 0)

#define PORT_GP_CFG_13(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_12(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 12, fn, sfx, cfg)
#define PORT_GP_13(bank, fn, sfx)	PORT_GP_CFG_13(bank, fn, sfx, 0)

#define PORT_GP_CFG_14(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_13(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 13, fn, sfx, cfg)
#define PORT_GP_14(bank, fn, sfx)	PORT_GP_CFG_14(bank, fn, sfx, 0)

#define PORT_GP_CFG_15(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_14(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 14, fn, sfx, cfg)
#define PORT_GP_15(bank, fn, sfx)	PORT_GP_CFG_15(bank, fn, sfx, 0)

#define PORT_GP_CFG_16(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_15(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 15, fn, sfx, cfg)
#define PORT_GP_16(bank, fn, sfx)	PORT_GP_CFG_16(bank, fn, sfx, 0)

#define PORT_GP_CFG_17(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_16(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 16, fn, sfx, cfg)
#define PORT_GP_17(bank, fn, sfx)	PORT_GP_CFG_17(bank, fn, sfx, 0)

#define PORT_GP_CFG_18(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_17(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 17, fn, sfx, cfg)
#define PORT_GP_18(bank, fn, sfx)	PORT_GP_CFG_18(bank, fn, sfx, 0)

#define PORT_GP_CFG_19(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_18(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 18, fn, sfx, cfg)
#define PORT_GP_19(bank, fn, sfx)	PORT_GP_CFG_19(bank, fn, sfx, 0)

#define PORT_GP_CFG_20(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_19(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 19, fn, sfx, cfg)
#define PORT_GP_20(bank, fn, sfx)	PORT_GP_CFG_20(bank, fn, sfx, 0)

#define PORT_GP_CFG_21(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_20(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 20, fn, sfx, cfg)
#define PORT_GP_21(bank, fn, sfx)	PORT_GP_CFG_21(bank, fn, sfx, 0)

#define PORT_GP_CFG_22(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_21(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 21, fn, sfx, cfg)
#define PORT_GP_22(bank, fn, sfx)	PORT_GP_CFG_22(bank, fn, sfx, 0)

#define PORT_GP_CFG_23(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_22(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 22, fn, sfx, cfg)
#define PORT_GP_23(bank, fn, sfx)	PORT_GP_CFG_23(bank, fn, sfx, 0)

#define PORT_GP_CFG_24(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_23(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 23, fn, sfx, cfg)
#define PORT_GP_24(bank, fn, sfx)	PORT_GP_CFG_24(bank, fn, sfx, 0)

#define PORT_GP_CFG_25(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_24(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 24, fn, sfx, cfg)
#define PORT_GP_25(bank, fn, sfx)	PORT_GP_CFG_25(bank, fn, sfx, 0)

#define PORT_GP_CFG_26(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_25(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 25, fn, sfx, cfg)
#define PORT_GP_26(bank, fn, sfx)	PORT_GP_CFG_26(bank, fn, sfx, 0)

#define PORT_GP_CFG_27(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_26(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 26, fn, sfx, cfg)
#define PORT_GP_27(bank, fn, sfx)	PORT_GP_CFG_27(bank, fn, sfx, 0)

#define PORT_GP_CFG_28(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_27(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 27, fn, sfx, cfg)
#define PORT_GP_28(bank, fn, sfx)	PORT_GP_CFG_28(bank, fn, sfx, 0)

#define PORT_GP_CFG_29(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_28(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 28, fn, sfx, cfg)
#define PORT_GP_29(bank, fn, sfx)	PORT_GP_CFG_29(bank, fn, sfx, 0)

#define PORT_GP_CFG_30(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_29(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 29, fn, sfx, cfg)
#define PORT_GP_30(bank, fn, sfx)	PORT_GP_CFG_30(bank, fn, sfx, 0)

#define PORT_GP_CFG_31(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_30(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 30, fn, sfx, cfg)
#define PORT_GP_31(bank, fn, sfx)	PORT_GP_CFG_31(bank, fn, sfx, 0)

#define PORT_GP_CFG_32(bank, fn, sfx, cfg)				\
	PORT_GP_CFG_31(bank, fn, sfx, cfg),				\
	PORT_GP_CFG_1(bank, 31, fn, sfx, cfg)
#define PORT_GP_32(bank, fn, sfx)	PORT_GP_CFG_32(bank, fn, sfx, 0)

#define PORT_GP_32_REV(bank, fn, sfx)					\
	PORT_GP_1(bank, 31, fn, sfx), PORT_GP_1(bank, 30, fn, sfx),	\
	PORT_GP_1(bank, 29, fn, sfx), PORT_GP_1(bank, 28, fn, sfx),	\
	PORT_GP_1(bank, 27, fn, sfx), PORT_GP_1(bank, 26, fn, sfx),	\
	PORT_GP_1(bank, 25, fn, sfx), PORT_GP_1(bank, 24, fn, sfx),	\
	PORT_GP_1(bank, 23, fn, sfx), PORT_GP_1(bank, 22, fn, sfx),	\
	PORT_GP_1(bank, 21, fn, sfx), PORT_GP_1(bank, 20, fn, sfx),	\
	PORT_GP_1(bank, 19, fn, sfx), PORT_GP_1(bank, 18, fn, sfx),	\
	PORT_GP_1(bank, 17, fn, sfx), PORT_GP_1(bank, 16, fn, sfx),	\
	PORT_GP_1(bank, 15, fn, sfx), PORT_GP_1(bank, 14, fn, sfx),	\
	PORT_GP_1(bank, 13, fn, sfx), PORT_GP_1(bank, 12, fn, sfx),	\
	PORT_GP_1(bank, 11, fn, sfx), PORT_GP_1(bank, 10, fn, sfx),	\
	PORT_GP_1(bank, 9,  fn, sfx), PORT_GP_1(bank, 8,  fn, sfx),	\
	PORT_GP_1(bank, 7,  fn, sfx), PORT_GP_1(bank, 6,  fn, sfx),	\
	PORT_GP_1(bank, 5,  fn, sfx), PORT_GP_1(bank, 4,  fn, sfx),	\
	PORT_GP_1(bank, 3,  fn, sfx), PORT_GP_1(bank, 2,  fn, sfx),	\
	PORT_GP_1(bank, 1,  fn, sfx), PORT_GP_1(bank, 0,  fn, sfx)

 
#define _GP_ALL(bank, pin, name, sfx, cfg)	name##_##sfx
#define GP_ALL(str)			CPU_ALL_GP(_GP_ALL, str)

 
#define _GP_GPIO(bank, _pin, _name, sfx, cfg) {				\
	.pin = (bank * 32) + _pin,					\
	.name = __stringify(_name),					\
	.enum_id = _name##_DATA,					\
	.configs = cfg,							\
}
#define PINMUX_GPIO_GP_ALL()		CPU_ALL_GP(_GP_GPIO, unused)

 
#define _GP_DATA(bank, pin, name, sfx, cfg)	PINMUX_DATA(name##_DATA, name##_FN)
#define PINMUX_DATA_GP_ALL()		CPU_ALL_GP(_GP_DATA, unused)

 
#define _GP_ENTRY(bank, pin, name, sfx, cfg)				\
	deprecated)); char name[(bank * 32) + pin] __attribute__((deprecated
#define GP_ASSIGN_LAST()						\
	GP_LAST = sizeof(union {					\
		char dummy[0] __attribute__((deprecated,		\
		CPU_ALL_GP(_GP_ENTRY, unused),				\
		deprecated));						\
	})

 

#define PORT_1(pn, fn, pfx, sfx) fn(pn, pfx, sfx)

#define PORT_10(pn, fn, pfx, sfx)					  \
	PORT_1(pn,   fn, pfx##0, sfx), PORT_1(pn+1, fn, pfx##1, sfx),	  \
	PORT_1(pn+2, fn, pfx##2, sfx), PORT_1(pn+3, fn, pfx##3, sfx),	  \
	PORT_1(pn+4, fn, pfx##4, sfx), PORT_1(pn+5, fn, pfx##5, sfx),	  \
	PORT_1(pn+6, fn, pfx##6, sfx), PORT_1(pn+7, fn, pfx##7, sfx),	  \
	PORT_1(pn+8, fn, pfx##8, sfx), PORT_1(pn+9, fn, pfx##9, sfx)

#define PORT_90(pn, fn, pfx, sfx)					  \
	PORT_10(pn+10, fn, pfx##1, sfx), PORT_10(pn+20, fn, pfx##2, sfx), \
	PORT_10(pn+30, fn, pfx##3, sfx), PORT_10(pn+40, fn, pfx##4, sfx), \
	PORT_10(pn+50, fn, pfx##5, sfx), PORT_10(pn+60, fn, pfx##6, sfx), \
	PORT_10(pn+70, fn, pfx##7, sfx), PORT_10(pn+80, fn, pfx##8, sfx), \
	PORT_10(pn+90, fn, pfx##9, sfx)

 
#define _PORT_ALL(pn, pfx, sfx)		pfx##_##sfx
#define PORT_ALL(str)			CPU_ALL_PORT(_PORT_ALL, PORT, str)

 
#define PINMUX_GPIO(_pin)						\
	[GPIO_##_pin] = {						\
		.pin = (u16)-1,						\
		.name = __stringify(GPIO_##_pin),			\
		.enum_id = _pin##_DATA,					\
	}

 
#define SH_PFC_PIN_CFG(_pin, cfgs) {					\
	.pin = _pin,							\
	.name = __stringify(PORT##_pin),				\
	.enum_id = PORT##_pin##_DATA,					\
	.configs = cfgs,						\
}

 
#define _PORT_DATA(pn, pfx, sfx)					\
	PINMUX_DATA(PORT##pfx##_DATA, PORT##pfx##_FN0,			\
		    PORT##pfx##_OUT, PORT##pfx##_IN)
#define PINMUX_DATA_ALL()		CPU_ALL_PORT(_PORT_DATA, , unused)

 
#define _PORT_ENTRY(pn, pfx, sfx)					\
	deprecated)); char pfx[pn] __attribute__((deprecated
#define PORT_ASSIGN_LAST()						\
	PORT_LAST = sizeof(union {					\
		char dummy[0] __attribute__((deprecated,		\
		CPU_ALL_PORT(_PORT_ENTRY, PORT, unused),		\
		deprecated));						\
	})

 
#define PINMUX_GPIO_FN(gpio, base, data_or_mark)			\
	[gpio - (base)] = {						\
		.name = __stringify(gpio),				\
		.enum_id = data_or_mark,				\
	}
#define GPIO_FN(str)							\
	PINMUX_GPIO_FN(GPIO_FN_##str, PINMUX_FN_BASE, str##_MARK)

 

#define PIN_NOGP_CFG(pin, name, fn, cfg)	fn(pin, name, cfg)
#define PIN_NOGP(pin, name, fn)			fn(pin, name, 0)

 
#define _NOGP_ALL(pin, name, cfg)		PIN_##pin
#define NOGP_ALL()				CPU_ALL_NOGP(_NOGP_ALL)

 
#define _NOGP_PINMUX(_pin, _name, cfg) {				\
	.pin = PIN_##_pin,						\
	.name = "PIN_" _name,						\
	.configs = SH_PFC_PIN_CFG_NO_GPIO | cfg,			\
}
#define PINMUX_NOGP_ALL()		CPU_ALL_NOGP(_NOGP_PINMUX)

 
#define PORTCR(nr, reg) {						\
	PINMUX_CFG_REG_VAR("PORT" nr "CR", reg, 8, GROUP(-2, 2, -1, 3),	\
			   GROUP(					\
		 		\
		 						\
		0, PORT##nr##_OUT, PORT##nr##_IN, 0,			\
		 				\
		 						\
		PORT##nr##_FN0, PORT##nr##_FN1,				\
		PORT##nr##_FN2, PORT##nr##_FN3,				\
		PORT##nr##_FN4, PORT##nr##_FN5,				\
		PORT##nr##_FN6, PORT##nr##_FN7				\
	))								\
}

 
#define RCAR_GP_PIN(bank, pin)		(((bank) * 32) + (pin))

 
const struct pinmux_bias_reg *
rcar_pin_to_bias_reg(const struct sh_pfc_soc_info *info, unsigned int pin,
		     unsigned int *bit);
unsigned int rcar_pinmux_get_bias(struct sh_pfc *pfc, unsigned int pin);
void rcar_pinmux_set_bias(struct sh_pfc *pfc, unsigned int pin,
			  unsigned int bias);

unsigned int rmobile_pinmux_get_bias(struct sh_pfc *pfc, unsigned int pin);
void rmobile_pinmux_set_bias(struct sh_pfc *pfc, unsigned int pin,
			     unsigned int bias);

#endif  
