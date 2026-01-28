#ifndef __GVT_DEBUG_H__
#define __GVT_DEBUG_H__
#define gvt_err(fmt, args...) \
	pr_err("gvt: "fmt, ##args)
#define gvt_vgpu_err(fmt, args...)					\
do {									\
	if (IS_ERR_OR_NULL(vgpu))					\
		pr_err("gvt: "fmt, ##args);			\
	else								\
		pr_err("gvt: vgpu %d: "fmt, vgpu->id, ##args);\
} while (0)
#define gvt_dbg_core(fmt, args...) \
	pr_debug("gvt: core: "fmt, ##args)
#define gvt_dbg_irq(fmt, args...) \
	pr_debug("gvt: irq: "fmt, ##args)
#define gvt_dbg_mm(fmt, args...) \
	pr_debug("gvt: mm: "fmt, ##args)
#define gvt_dbg_mmio(fmt, args...) \
	pr_debug("gvt: mmio: "fmt, ##args)
#define gvt_dbg_dpy(fmt, args...) \
	pr_debug("gvt: dpy: "fmt, ##args)
#define gvt_dbg_el(fmt, args...) \
	pr_debug("gvt: el: "fmt, ##args)
#define gvt_dbg_sched(fmt, args...) \
	pr_debug("gvt: sched: "fmt, ##args)
#define gvt_dbg_render(fmt, args...) \
	pr_debug("gvt: render: "fmt, ##args)
#define gvt_dbg_cmd(fmt, args...) \
	pr_debug("gvt: cmd: "fmt, ##args)
#endif
