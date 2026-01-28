


#ifndef __LINUX_HSI_OMAP_SSI_H__
#define __LINUX_HSI_OMAP_SSI_H__

#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/hsi/hsi.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/io.h>

#define SSI_MAX_CHANNELS	8
#define SSI_MAX_GDD_LCH		8
#define SSI_BYTES_TO_FRAMES(x) ((((x) - 1) >> 2) + 1)

#define SSI_WAKE_EN 0


struct omap_ssm_ctx {
	u32	mode;
	u32	channels;
	u32	frame_size;
	union	{
			u32	timeout; 
			struct	{
					u32	arb_mode;
					u32	divisor;
			}; 
	};
};


struct omap_ssi_port {
	struct device		*dev;
	struct device           *pdev;
	dma_addr_t		sst_dma;
	dma_addr_t		ssr_dma;
	void __iomem		*sst_base;
	void __iomem		*ssr_base;
	spinlock_t		wk_lock;
	spinlock_t		lock;
	unsigned int		channels;
	struct list_head	txqueue[SSI_MAX_CHANNELS];
	struct list_head	rxqueue[SSI_MAX_CHANNELS];
	struct list_head	brkqueue;
	struct list_head	errqueue;
	struct delayed_work	errqueue_work;
	unsigned int		irq;
	int			wake_irq;
	struct gpio_desc	*wake_gpio;
	bool			wktest:1; 
	unsigned long		flags;
	unsigned int		wk_refcount;
	struct work_struct	work;
	
	u32			sys_mpu_enable; 
	struct omap_ssm_ctx	sst;
	struct omap_ssm_ctx	ssr;
	u32			loss_count;
	u32			port_id;
#ifdef CONFIG_DEBUG_FS
	struct dentry *dir;
#endif
};


struct gdd_trn {
	struct hsi_msg		*msg;
	struct scatterlist	*sg;
};


struct omap_ssi_controller {
	struct device		*dev;
	void __iomem		*sys;
	void __iomem		*gdd;
	struct clk		*fck;
	unsigned int		gdd_irq;
	struct tasklet_struct	gdd_tasklet;
	struct gdd_trn		gdd_trn[SSI_MAX_GDD_LCH];
	spinlock_t		lock;
	struct notifier_block	fck_nb;
	unsigned long		fck_rate;
	u32			loss_count;
	u32			max_speed;
	
	u32			gdd_gcr;
	int			(*get_loss)(struct device *dev);
	struct omap_ssi_port	**port;
#ifdef CONFIG_DEBUG_FS
	struct dentry *dir;
#endif
};

void omap_ssi_port_update_fclk(struct hsi_controller *ssi,
			       struct omap_ssi_port *omap_port);

extern struct platform_driver ssi_port_pdriver;

#endif 
