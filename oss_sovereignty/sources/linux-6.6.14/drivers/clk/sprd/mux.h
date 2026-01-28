






#ifndef _SPRD_MUX_H_
#define _SPRD_MUX_H_

#include "common.h"


struct sprd_mux_ssel {
	u8		shift;
	u8		width;
	const u8	*table;
};

struct sprd_mux {
	struct sprd_mux_ssel mux;
	struct sprd_clk_common	common;
};

#define _SPRD_MUX_CLK(_shift, _width, _table)		\
	{						\
		.shift	= _shift,			\
		.width	= _width,			\
		.table	= _table,			\
	}

#define SPRD_MUX_CLK_HW_INIT_FN(_struct, _name, _parents, _table,	\
				_reg, _shift, _width, _flags, _fn)	\
	struct sprd_mux _struct = {					\
		.mux	= _SPRD_MUX_CLK(_shift, _width, _table),	\
		.common	= {						\
			.regmap		= NULL,				\
			.reg		= _reg,				\
			.hw.init = _fn(_name, _parents,			\
				       &sprd_mux_ops, _flags),		\
		}							\
	}

#define SPRD_MUX_CLK_TABLE(_struct, _name, _parents, _table,		\
			   _reg, _shift, _width, _flags)		\
	SPRD_MUX_CLK_HW_INIT_FN(_struct, _name, _parents, _table,	\
				_reg, _shift, _width, _flags,		\
				CLK_HW_INIT_PARENTS)

#define SPRD_MUX_CLK(_struct, _name, _parents, _reg,		\
		     _shift, _width, _flags)			\
	SPRD_MUX_CLK_TABLE(_struct, _name, _parents, NULL,	\
			   _reg, _shift, _width, _flags)

#define SPRD_MUX_CLK_DATA_TABLE(_struct, _name, _parents, _table,	\
				_reg, _shift, _width, _flags)		\
	SPRD_MUX_CLK_HW_INIT_FN(_struct, _name, _parents, _table,	\
				_reg, _shift, _width, _flags,		\
				CLK_HW_INIT_PARENTS_DATA)

#define SPRD_MUX_CLK_DATA(_struct, _name, _parents, _reg,		\
			  _shift, _width, _flags)			\
	SPRD_MUX_CLK_DATA_TABLE(_struct, _name, _parents, NULL,		\
				_reg, _shift, _width, _flags)

static inline struct sprd_mux *hw_to_sprd_mux(const struct clk_hw *hw)
{
	struct sprd_clk_common *common = hw_to_sprd_clk_common(hw);

	return container_of(common, struct sprd_mux, common);
}

extern const struct clk_ops sprd_mux_ops;

u8 sprd_mux_helper_get_parent(const struct sprd_clk_common *common,
			      const struct sprd_mux_ssel *mux);
int sprd_mux_helper_set_parent(const struct sprd_clk_common *common,
			       const struct sprd_mux_ssel *mux,
			       u8 index);

#endif 
