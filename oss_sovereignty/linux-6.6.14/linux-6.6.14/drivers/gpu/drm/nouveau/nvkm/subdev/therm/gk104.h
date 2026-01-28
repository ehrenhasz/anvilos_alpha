#ifndef __GK104_THERM_H__
#define __GK104_THERM_H__
#define gk104_therm(p) (container_of((p), struct gk104_therm, base))
#include <subdev/therm.h>
#include "priv.h"
#include "gf100.h"
struct gk104_clkgate_engine_info {
	enum nvkm_subdev_type type;
	int inst;
	u8 offset;
};
struct gk104_therm {
	struct nvkm_therm base;
	const struct gk104_clkgate_engine_info *clkgate_order;
	const struct gf100_idle_filter *idle_filter;
};
extern const struct gk104_clkgate_engine_info gk104_clkgate_engine_info[];
extern const struct gf100_idle_filter gk104_idle_filter;
#endif
