#ifndef ASPEED_PINMUX_H
#define ASPEED_PINMUX_H
#include <linux/regmap.h>
#define ASPEED_IP_SCU		0
#define ASPEED_IP_GFX		1
#define ASPEED_IP_LPC		2
#define ASPEED_NR_PINMUX_IPS	3
struct aspeed_sig_desc {
	unsigned int ip;
	unsigned int reg;
	u32 mask;
	u32 enable;
	u32 disable;
};
struct aspeed_sig_expr {
	const char *signal;
	const char *function;
	int ndescs;
	const struct aspeed_sig_desc *descs;
};
struct aspeed_pin_desc {
	const char *name;
	const struct aspeed_sig_expr ***prios;
};
#define SIG_DESC_IP_BIT(ip, reg, idx, val) \
	{ ip, reg, BIT_MASK(idx), val, (((val) + 1) & 1) }
#define SIG_DESC_BIT(reg, idx, val) \
	SIG_DESC_IP_BIT(ASPEED_IP_SCU, reg, idx, val)
#define SIG_DESC_IP_SET(ip, reg, idx) SIG_DESC_IP_BIT(ip, reg, idx, 1)
#define SIG_DESC_SET(reg, idx) SIG_DESC_IP_BIT(ASPEED_IP_SCU, reg, idx, 1)
#define SIG_DESC_CLEAR(reg, idx) { ASPEED_IP_SCU, reg, BIT_MASK(idx), 0, 0 }
#define SIG_DESC_LIST_SYM(sig, group) sig_descs_ ## sig ## _ ## group
#define SIG_DESC_LIST_DECL(sig, group, ...) \
	static const struct aspeed_sig_desc SIG_DESC_LIST_SYM(sig, group)[] = \
		{ __VA_ARGS__ }
#define SIG_EXPR_SYM(sig, group) sig_expr_ ## sig ## _ ## group
#define SIG_EXPR_DECL_(sig, group, func) \
	static const struct aspeed_sig_expr SIG_EXPR_SYM(sig, group) = \
	{ \
		.signal = #sig, \
		.function = #func, \
		.ndescs = ARRAY_SIZE(SIG_DESC_LIST_SYM(sig, group)), \
		.descs = &(SIG_DESC_LIST_SYM(sig, group))[0], \
	}
#define SIG_EXPR_DECL(sig, group, func, ...) \
	SIG_DESC_LIST_DECL(sig, group, __VA_ARGS__); \
	SIG_EXPR_DECL_(sig, group, func)
#define SIG_EXPR_PTR(sig, group) (&SIG_EXPR_SYM(sig, group))
#define SIG_EXPR_LIST_SYM(sig, group) sig_exprs_ ## sig ## _ ## group
#define SIG_EXPR_LIST_DECL(sig, group, ...) \
	static const struct aspeed_sig_expr *SIG_EXPR_LIST_SYM(sig, group)[] =\
		{ __VA_ARGS__, NULL }
#define stringify(x) #x
#define istringify(x) stringify(x)
#define SIG_EXPR_LIST_ALIAS(pin, sig, group) \
	static const struct aspeed_sig_expr *\
		SIG_EXPR_LIST_SYM(pin, sig)[ARRAY_SIZE(SIG_EXPR_LIST_SYM(sig, group))] \
		__attribute__((alias(istringify(SIG_EXPR_LIST_SYM(sig, group)))))
#define SIG_EXPR_LIST_DECL_SESG(pin, sig, func, ...) \
	SIG_DESC_LIST_DECL(sig, func, __VA_ARGS__); \
	SIG_EXPR_DECL_(sig, func, func); \
	SIG_EXPR_LIST_DECL(sig, func, SIG_EXPR_PTR(sig, func)); \
	SIG_EXPR_LIST_ALIAS(pin, sig, func)
#define SIG_EXPR_LIST_DECL_SEMG(pin, sig, group, func, ...) \
	SIG_DESC_LIST_DECL(sig, group, __VA_ARGS__); \
	SIG_EXPR_DECL_(sig, group, func); \
	SIG_EXPR_LIST_DECL(sig, group, SIG_EXPR_PTR(sig, group)); \
	SIG_EXPR_LIST_ALIAS(pin, sig, group)
#define SIG_EXPR_LIST_DECL_DESG(pin, sig, f0, f1) \
	SIG_EXPR_LIST_DECL(sig, f0, \
			   SIG_EXPR_PTR(sig, f0), \
			   SIG_EXPR_PTR(sig, f1)); \
	SIG_EXPR_LIST_ALIAS(pin, sig, f0)
#define SIG_EXPR_LIST_PTR(sig, group) SIG_EXPR_LIST_SYM(sig, group)
#define PIN_EXPRS_SYM(pin) pin_exprs_ ## pin
#define PIN_EXPRS_PTR(pin) (&PIN_EXPRS_SYM(pin)[0])
#define PIN_SYM(pin) pin_ ## pin
#define PIN_DECL_(pin, ...) \
	static const struct aspeed_sig_expr **PIN_EXPRS_SYM(pin)[] = \
		{ __VA_ARGS__, NULL }; \
	static const struct aspeed_pin_desc PIN_SYM(pin) = \
		{ #pin, PIN_EXPRS_PTR(pin) }
#define PIN_DECL_1(pin, other, sig) \
	SIG_EXPR_LIST_DECL_SESG(pin, other, other); \
	PIN_DECL_(pin, SIG_EXPR_LIST_PTR(pin, sig), \
		  SIG_EXPR_LIST_PTR(pin, other))
#define SSSF_PIN_DECL(pin, other, sig, ...) \
	SIG_EXPR_LIST_DECL_SESG(pin, sig, sig, __VA_ARGS__); \
	SIG_EXPR_LIST_DECL_SESG(pin, other, other); \
	PIN_DECL_(pin, SIG_EXPR_LIST_PTR(pin, sig), \
		  SIG_EXPR_LIST_PTR(pin, other)); \
	FUNC_GROUP_DECL(sig, pin)
#define PIN_DECL_2(pin, other, high, low) \
	SIG_EXPR_LIST_DECL_SESG(pin, other, other); \
	PIN_DECL_(pin, \
			SIG_EXPR_LIST_PTR(pin, high), \
			SIG_EXPR_LIST_PTR(pin, low), \
			SIG_EXPR_LIST_PTR(pin, other))
#define PIN_DECL_3(pin, other, high, medium, low) \
	SIG_EXPR_LIST_DECL_SESG(pin, other, other); \
	PIN_DECL_(pin, \
			SIG_EXPR_LIST_PTR(pin, high), \
			SIG_EXPR_LIST_PTR(pin, medium), \
			SIG_EXPR_LIST_PTR(pin, low), \
			SIG_EXPR_LIST_PTR(pin, other))
#define PIN_DECL_4(pin, other, prio1, prio2, prio3, prio4) \
	SIG_EXPR_LIST_DECL_SESG(pin, other, other); \
	PIN_DECL_(pin, \
			SIG_EXPR_LIST_PTR(pin, prio1), \
			SIG_EXPR_LIST_PTR(pin, prio2), \
			SIG_EXPR_LIST_PTR(pin, prio3), \
			SIG_EXPR_LIST_PTR(pin, prio4), \
			SIG_EXPR_LIST_PTR(pin, other))
#define GROUP_SYM(group) group_pins_ ## group
#define GROUP_DECL(group, ...) \
	static const int GROUP_SYM(group)[] = { __VA_ARGS__ }
#define FUNC_SYM(func) func_groups_ ## func
#define FUNC_DECL_(func, ...) \
	static const char *FUNC_SYM(func)[] = { __VA_ARGS__ }
#define FUNC_DECL_1(func, group) FUNC_DECL_(func, #group)
#define FUNC_DECL_2(func, one, two) FUNC_DECL_(func, #one, #two)
#define FUNC_DECL_3(func, one, two, three) FUNC_DECL_(func, #one, #two, #three)
#define FUNC_GROUP_DECL(func, ...) \
	GROUP_DECL(func, __VA_ARGS__); \
	FUNC_DECL_(func, #func)
#define GPIO_PIN_DECL(pin, gpio) \
	SIG_EXPR_LIST_DECL_SESG(pin, gpio, gpio); \
	PIN_DECL_(pin, SIG_EXPR_LIST_PTR(pin, gpio))
struct aspeed_pin_group {
	const char *name;
	const unsigned int *pins;
	const unsigned int npins;
};
#define ASPEED_PINCTRL_GROUP(name_) { \
	.name = #name_, \
	.pins = &(GROUP_SYM(name_))[0], \
	.npins = ARRAY_SIZE(GROUP_SYM(name_)), \
}
struct aspeed_pin_function {
	const char *name;
	const char *const *groups;
	unsigned int ngroups;
};
#define ASPEED_PINCTRL_FUNC(name_, ...) { \
	.name = #name_, \
	.groups = &FUNC_SYM(name_)[0], \
	.ngroups = ARRAY_SIZE(FUNC_SYM(name_)), \
}
struct aspeed_pinmux_data;
struct aspeed_pinmux_ops {
	int (*eval)(struct aspeed_pinmux_data *ctx,
		    const struct aspeed_sig_expr *expr, bool enabled);
	int (*set)(struct aspeed_pinmux_data *ctx,
		   const struct aspeed_sig_expr *expr, bool enabled);
};
struct aspeed_pinmux_data {
	struct device *dev;
	struct regmap *maps[ASPEED_NR_PINMUX_IPS];
	const struct aspeed_pinmux_ops *ops;
	const struct aspeed_pin_group *groups;
	const unsigned int ngroups;
	const struct aspeed_pin_function *functions;
	const unsigned int nfunctions;
};
int aspeed_sig_desc_eval(const struct aspeed_sig_desc *desc, bool enabled,
			 struct regmap *map);
int aspeed_sig_expr_eval(struct aspeed_pinmux_data *ctx,
			 const struct aspeed_sig_expr *expr, bool enabled);
static inline int aspeed_sig_expr_set(struct aspeed_pinmux_data *ctx,
				      const struct aspeed_sig_expr *expr,
				      bool enabled)
{
	return ctx->ops->set(ctx, expr, enabled);
}
#endif  
