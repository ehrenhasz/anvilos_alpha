#ifndef _SUN4I_DRV_H_
#define _SUN4I_DRV_H_
#include <linux/clk.h>
#include <linux/list.h>
#include <linux/regmap.h>
struct sun4i_drv {
	struct list_head	engine_list;
	struct list_head	frontend_list;
	struct list_head	tcon_list;
};
#endif  
