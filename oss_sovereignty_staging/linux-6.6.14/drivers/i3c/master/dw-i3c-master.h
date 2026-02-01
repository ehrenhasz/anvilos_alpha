 
 

#include <linux/clk.h>
#include <linux/i3c/master.h>
#include <linux/reset.h>
#include <linux/types.h>

#define DW_I3C_MAX_DEVS 32

struct dw_i3c_master_caps {
	u8 cmdfifodepth;
	u8 datafifodepth;
};

struct dw_i3c_dat_entry {
	u8 addr;
	struct i3c_dev_desc *ibi_dev;
};

struct dw_i3c_master {
	struct i3c_master_controller base;
	u16 maxdevs;
	u16 datstartaddr;
	u32 free_pos;
	struct {
		struct list_head list;
		struct dw_i3c_xfer *cur;
		spinlock_t lock;
	} xferqueue;
	struct dw_i3c_master_caps caps;
	void __iomem *regs;
	struct reset_control *core_rst;
	struct clk *core_clk;
	char version[5];
	char type[5];
	bool ibi_capable;

	 
	struct dw_i3c_dat_entry devs[DW_I3C_MAX_DEVS];
	spinlock_t devs_lock;

	 
	const struct dw_i3c_platform_ops *platform_ops;
};

struct dw_i3c_platform_ops {
	 
	int (*init)(struct dw_i3c_master *i3c);

	 
	void (*set_dat_ibi)(struct dw_i3c_master *i3c,
			    struct i3c_dev_desc *dev, bool enable, u32 *reg);
};

extern int dw_i3c_common_probe(struct dw_i3c_master *master,
			       struct platform_device *pdev);
extern void dw_i3c_common_remove(struct dw_i3c_master *master);

