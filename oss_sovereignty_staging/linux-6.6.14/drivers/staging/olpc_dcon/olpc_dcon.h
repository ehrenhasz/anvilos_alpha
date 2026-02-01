 
#ifndef OLPC_DCON_H_
#define OLPC_DCON_H_

#include <linux/notifier.h>
#include <linux/workqueue.h>

 

#define DCON_REG_ID		 0
#define DCON_REG_MODE		 1

#define MODE_PASSTHRU	BIT(0)
#define MODE_SLEEP	BIT(1)
#define MODE_SLEEP_AUTO	BIT(2)
#define MODE_BL_ENABLE	BIT(3)
#define MODE_BLANK	BIT(4)
#define MODE_CSWIZZLE	BIT(5)
#define MODE_COL_AA	BIT(6)
#define MODE_MONO_LUMA	BIT(7)
#define MODE_SCAN_INT	BIT(8)
#define MODE_CLOCKDIV	BIT(9)
#define MODE_DEBUG	BIT(14)
#define MODE_SELFTEST	BIT(15)

#define DCON_REG_HRES		0x2
#define DCON_REG_HTOTAL		0x3
#define DCON_REG_HSYNC_WIDTH	0x4
#define DCON_REG_VRES		0x5
#define DCON_REG_VTOTAL		0x6
#define DCON_REG_VSYNC_WIDTH	0x7
#define DCON_REG_TIMEOUT	0x8
#define DCON_REG_SCAN_INT	0x9
#define DCON_REG_BRIGHT		0xa
#define DCON_REG_MEM_OPT_A	0x41
#define DCON_REG_MEM_OPT_B	0x42

 
#define MEM_DLL_CLOCK_DELAY	BIT(0)
 
#define MEM_POWER_DOWN		BIT(8)
 
#define MEM_SOFT_RESET		BIT(0)

 

#define DCONSTAT_SCANINT	0
#define DCONSTAT_SCANINT_DCON	1
#define DCONSTAT_DISPLAYLOAD	2
#define DCONSTAT_MISSED		3

 

#define DCON_SOURCE_DCON        0
#define DCON_SOURCE_CPU         1

 
#define DCON_IRQ                6

struct dcon_priv {
	struct i2c_client *client;
	struct fb_info *fbinfo;
	struct backlight_device *bl_dev;

	wait_queue_head_t waitq;
	struct work_struct switch_source;
	struct notifier_block reboot_nb;

	 
	u8 disp_mode;

	 
	u8 bl_val;

	 
	int curr_src;

	 
	int pending_src;

	 
	bool switched;
	ktime_t irq_time;
	ktime_t load_time;

	 
	bool mono;
	bool asleep;
	 
	bool ignore_fb_events;
};

struct dcon_platform_data {
	int (*init)(struct dcon_priv *dcon);
	void (*bus_stabilize_wiggle)(void);
	void (*set_dconload)(int load);
	int (*read_status)(u8 *status);
};

struct dcon_gpio {
	const char *name;
	unsigned long flags;
};

#include <linux/interrupt.h>

irqreturn_t dcon_interrupt(int irq, void *id);

extern struct dcon_platform_data dcon_pdata_xo_1;
extern struct dcon_platform_data dcon_pdata_xo_1_5;

#endif
