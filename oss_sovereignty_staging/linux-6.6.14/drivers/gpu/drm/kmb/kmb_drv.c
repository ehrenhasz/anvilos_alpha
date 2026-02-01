
 

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/mfd/syscon.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_fbdev_dma.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_module.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>

#include "kmb_drv.h"
#include "kmb_dsi.h"
#include "kmb_regs.h"

static int kmb_display_clk_enable(struct kmb_drm_private *kmb)
{
	int ret = 0;

	ret = clk_prepare_enable(kmb->kmb_clk.clk_lcd);
	if (ret) {
		drm_err(&kmb->drm, "Failed to enable LCD clock: %d\n", ret);
		return ret;
	}
	DRM_INFO("SUCCESS : enabled LCD clocks\n");
	return 0;
}

static int kmb_initialize_clocks(struct kmb_drm_private *kmb, struct device *dev)
{
	int ret = 0;
	struct regmap *msscam;

	kmb->kmb_clk.clk_lcd = devm_clk_get(dev, "clk_lcd");
	if (IS_ERR(kmb->kmb_clk.clk_lcd)) {
		drm_err(&kmb->drm, "clk_get() failed clk_lcd\n");
		return PTR_ERR(kmb->kmb_clk.clk_lcd);
	}

	kmb->kmb_clk.clk_pll0 = devm_clk_get(dev, "clk_pll0");
	if (IS_ERR(kmb->kmb_clk.clk_pll0)) {
		drm_err(&kmb->drm, "clk_get() failed clk_pll0 ");
		return PTR_ERR(kmb->kmb_clk.clk_pll0);
	}
	kmb->sys_clk_mhz = clk_get_rate(kmb->kmb_clk.clk_pll0) / 1000000;
	drm_info(&kmb->drm, "system clk = %d Mhz", kmb->sys_clk_mhz);

	ret =  kmb_dsi_clk_init(kmb->kmb_dsi);

	 
	clk_set_rate(kmb->kmb_clk.clk_lcd, KMB_LCD_DEFAULT_CLK);
	if (clk_get_rate(kmb->kmb_clk.clk_lcd) != KMB_LCD_DEFAULT_CLK) {
		drm_err(&kmb->drm, "failed to set to clk_lcd to %d\n",
			KMB_LCD_DEFAULT_CLK);
		return -1;
	}
	drm_dbg(&kmb->drm, "clk_lcd = %ld\n", clk_get_rate(kmb->kmb_clk.clk_lcd));

	ret = kmb_display_clk_enable(kmb);
	if (ret)
		return ret;

	msscam = syscon_regmap_lookup_by_compatible("intel,keembay-msscam");
	if (IS_ERR(msscam)) {
		drm_err(&kmb->drm, "failed to get msscam syscon");
		return -1;
	}

	 
	regmap_update_bits(msscam, MSS_CAM_CLK_CTRL, 0x1fff, 0x1fff);
	regmap_update_bits(msscam, MSS_CAM_RSTN_CTRL, 0xffffffff, 0xffffffff);
	return 0;
}

static void kmb_display_clk_disable(struct kmb_drm_private *kmb)
{
	clk_disable_unprepare(kmb->kmb_clk.clk_lcd);
}

static void __iomem *kmb_map_mmio(struct drm_device *drm,
				  struct platform_device *pdev,
				  char *name)
{
	struct resource *res;
	void __iomem *mem;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (!res) {
		drm_err(drm, "failed to get resource for %s", name);
		return ERR_PTR(-ENOMEM);
	}
	mem = devm_ioremap_resource(drm->dev, res);
	if (IS_ERR(mem))
		drm_err(drm, "failed to ioremap %s registers", name);
	return mem;
}

static int kmb_hw_init(struct drm_device *drm, unsigned long flags)
{
	struct kmb_drm_private *kmb = to_kmb(drm);
	struct platform_device *pdev = to_platform_device(drm->dev);
	int irq_lcd;
	int ret = 0;

	 
	kmb->lcd_mmio = kmb_map_mmio(drm, pdev, "lcd");
	if (IS_ERR(kmb->lcd_mmio)) {
		drm_err(&kmb->drm, "failed to map LCD registers\n");
		return -ENOMEM;
	}

	 
	ret = kmb_dsi_map_mmio(kmb->kmb_dsi);
	if (ret)
		return ret;

	 
	kmb_initialize_clocks(kmb, &pdev->dev);

	 

	 
	irq_lcd = platform_get_irq(pdev, 0);
	if (irq_lcd < 0) {
		ret = irq_lcd;
		drm_err(&kmb->drm, "irq_lcd not found");
		goto setup_fail;
	}

	 
	ret = of_reserved_mem_device_init(drm->dev);
	if (ret && ret != -ENODEV)
		return ret;

	spin_lock_init(&kmb->irq_lock);

	kmb->irq_lcd = irq_lcd;

	return 0;

 setup_fail:
	of_reserved_mem_device_release(drm->dev);

	return ret;
}

static const struct drm_mode_config_funcs kmb_mode_config_funcs = {
	.fb_create = drm_gem_fb_create,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static int kmb_setup_mode_config(struct drm_device *drm)
{
	int ret;
	struct kmb_drm_private *kmb = to_kmb(drm);

	ret = drmm_mode_config_init(drm);
	if (ret)
		return ret;
	drm->mode_config.min_width = KMB_FB_MIN_WIDTH;
	drm->mode_config.min_height = KMB_FB_MIN_HEIGHT;
	drm->mode_config.max_width = KMB_FB_MAX_WIDTH;
	drm->mode_config.max_height = KMB_FB_MAX_HEIGHT;
	drm->mode_config.preferred_depth = 24;
	drm->mode_config.funcs = &kmb_mode_config_funcs;

	ret = kmb_setup_crtc(drm);
	if (ret < 0) {
		drm_err(drm, "failed to create crtc\n");
		return ret;
	}
	ret = kmb_dsi_encoder_init(drm, kmb->kmb_dsi);
	 
	kmb->crtc.port = of_graph_get_port_by_id(drm->dev->of_node, 0);
	ret = drm_vblank_init(drm, drm->mode_config.num_crtc);
	if (ret < 0) {
		drm_err(drm, "failed to initialize vblank\n");
		pm_runtime_disable(drm->dev);
		return ret;
	}

	drm_mode_config_reset(drm);
	return 0;
}

static irqreturn_t handle_lcd_irq(struct drm_device *dev)
{
	unsigned long status, val, val1;
	int plane_id, dma0_state, dma1_state;
	struct kmb_drm_private *kmb = to_kmb(dev);
	u32 ctrl = 0;

	status = kmb_read_lcd(kmb, LCD_INT_STATUS);

	spin_lock(&kmb->irq_lock);
	if (status & LCD_INT_EOF) {
		kmb_write_lcd(kmb, LCD_INT_CLEAR, LCD_INT_EOF);

		 
		for (plane_id = LAYER_0;
				plane_id < KMB_MAX_PLANES; plane_id++) {
			if (kmb->plane_status[plane_id].disable) {
				kmb_clr_bitmask_lcd(kmb,
						    LCD_LAYERn_DMA_CFG
						    (plane_id),
						    LCD_DMA_LAYER_ENABLE);

				kmb_clr_bitmask_lcd(kmb, LCD_CONTROL,
						    kmb->plane_status[plane_id].ctrl);

				ctrl = kmb_read_lcd(kmb, LCD_CONTROL);
				if (!(ctrl & (LCD_CTRL_VL1_ENABLE |
				    LCD_CTRL_VL2_ENABLE |
				    LCD_CTRL_GL1_ENABLE |
				    LCD_CTRL_GL2_ENABLE))) {
					 
					kmb_clr_bitmask_lcd(kmb, LCD_CONTROL,
							    LCD_CTRL_PIPELINE_DMA);
				}

				kmb->plane_status[plane_id].disable = false;
			}
		}
		if (kmb->kmb_under_flow) {
			 
			dma0_state = (kmb->layer_no == 0) ?
			    LCD_VIDEO0_DMA0_STATE : LCD_VIDEO1_DMA0_STATE;
			dma1_state = (kmb->layer_no == 0) ?
			    LCD_VIDEO0_DMA1_STATE : LCD_VIDEO1_DMA1_STATE;

			do {
				kmb_write_lcd(kmb, LCD_FIFO_FLUSH, 1);
				val = kmb_read_lcd(kmb, dma0_state)
				    & LCD_DMA_STATE_ACTIVE;
				val1 = kmb_read_lcd(kmb, dma1_state)
				    & LCD_DMA_STATE_ACTIVE;
			} while ((val || val1));
			 
			kmb_clr_bitmask_lcd(kmb,
					    LCD_LAYERn_DMA_CFG(kmb->layer_no),
					    LCD_DMA_LAYER_ENABLE);
			kmb_write_lcd(kmb, LCD_FIFO_FLUSH, 1);
			kmb->kmb_flush_done = 1;
			kmb->kmb_under_flow = 0;
		}
	}

	if (status & LCD_INT_LINE_CMP) {
		 
		kmb_write_lcd(kmb, LCD_INT_CLEAR, LCD_INT_LINE_CMP);
	}

	if (status & LCD_INT_VERT_COMP) {
		 
		val = kmb_read_lcd(kmb, LCD_VSTATUS);
		val = (val & LCD_VSTATUS_VERTICAL_STATUS_MASK);
		switch (val) {
		case LCD_VSTATUS_COMPARE_VSYNC:
			 
			kmb_write_lcd(kmb, LCD_INT_CLEAR, LCD_INT_VERT_COMP);
			if (kmb->kmb_flush_done) {
				kmb_set_bitmask_lcd(kmb,
						    LCD_LAYERn_DMA_CFG
						    (kmb->layer_no),
						    LCD_DMA_LAYER_ENABLE);
				kmb->kmb_flush_done = 0;
			}
			drm_crtc_handle_vblank(&kmb->crtc);
			break;
		case LCD_VSTATUS_COMPARE_BACKPORCH:
		case LCD_VSTATUS_COMPARE_ACTIVE:
		case LCD_VSTATUS_COMPARE_FRONT_PORCH:
			kmb_write_lcd(kmb, LCD_INT_CLEAR, LCD_INT_VERT_COMP);
			break;
		}
	}
	if (status & LCD_INT_DMA_ERR) {
		val =
		    (status & LCD_INT_DMA_ERR &
		     kmb_read_lcd(kmb, LCD_INT_ENABLE));
		 
		if (val & (LAYER0_DMA_FIFO_UNDERFLOW |
			   LAYER0_DMA_CB_FIFO_UNDERFLOW |
			   LAYER0_DMA_CR_FIFO_UNDERFLOW)) {
			kmb->kmb_under_flow++;
			drm_info(&kmb->drm,
				 "!LAYER0:VL0 DMA UNDERFLOW val = 0x%lx,under_flow=%d",
			     val, kmb->kmb_under_flow);
			 
			kmb_clr_bitmask_lcd(kmb, LCD_INT_ENABLE,
					    LAYER0_DMA_FIFO_UNDERFLOW |
					    LAYER0_DMA_CB_FIFO_UNDERFLOW |
					    LAYER0_DMA_CR_FIFO_UNDERFLOW);
			kmb_set_bitmask_lcd(kmb, LCD_INT_CLEAR,
					    LAYER0_DMA_CB_FIFO_UNDERFLOW |
					    LAYER0_DMA_FIFO_UNDERFLOW |
					    LAYER0_DMA_CR_FIFO_UNDERFLOW);
			 
			kmb_clr_bitmask_lcd(kmb, LCD_LAYERn_DMA_CFG(0),
					    LCD_DMA_LAYER_CONT_PING_PONG_UPDATE);

			kmb->layer_no = 0;
		}

		if (val & LAYER0_DMA_FIFO_OVERFLOW)
			drm_dbg(&kmb->drm,
				"LAYER0:VL0 DMA OVERFLOW val = 0x%lx", val);
		if (val & LAYER0_DMA_CB_FIFO_OVERFLOW)
			drm_dbg(&kmb->drm,
				"LAYER0:VL0 DMA CB OVERFLOW val = 0x%lx", val);
		if (val & LAYER0_DMA_CR_FIFO_OVERFLOW)
			drm_dbg(&kmb->drm,
				"LAYER0:VL0 DMA CR OVERFLOW val = 0x%lx", val);

		 
		if (val & (LAYER1_DMA_FIFO_UNDERFLOW |
			   LAYER1_DMA_CB_FIFO_UNDERFLOW |
			   LAYER1_DMA_CR_FIFO_UNDERFLOW)) {
			kmb->kmb_under_flow++;
			drm_info(&kmb->drm,
				 "!LAYER1:VL1 DMA UNDERFLOW val = 0x%lx, under_flow=%d",
			     val, kmb->kmb_under_flow);
			 
			kmb_clr_bitmask_lcd(kmb, LCD_INT_ENABLE,
					    LAYER1_DMA_FIFO_UNDERFLOW |
					    LAYER1_DMA_CB_FIFO_UNDERFLOW |
					    LAYER1_DMA_CR_FIFO_UNDERFLOW);
			kmb_set_bitmask_lcd(kmb, LCD_INT_CLEAR,
					    LAYER1_DMA_CB_FIFO_UNDERFLOW |
					    LAYER1_DMA_FIFO_UNDERFLOW |
					    LAYER1_DMA_CR_FIFO_UNDERFLOW);
			 
			kmb_clr_bitmask_lcd(kmb, LCD_LAYERn_DMA_CFG(1),
					    LCD_DMA_LAYER_CONT_PING_PONG_UPDATE);
			kmb->layer_no = 1;
		}

		 
		if (val & LAYER1_DMA_FIFO_OVERFLOW)
			drm_dbg(&kmb->drm,
				"LAYER1:VL1 DMA OVERFLOW val = 0x%lx", val);
		if (val & LAYER1_DMA_CB_FIFO_OVERFLOW)
			drm_dbg(&kmb->drm,
				"LAYER1:VL1 DMA CB OVERFLOW val = 0x%lx", val);
		if (val & LAYER1_DMA_CR_FIFO_OVERFLOW)
			drm_dbg(&kmb->drm,
				"LAYER1:VL1 DMA CR OVERFLOW val = 0x%lx", val);

		 
		if (val & LAYER2_DMA_FIFO_UNDERFLOW)
			drm_dbg(&kmb->drm,
				"LAYER2:GL0 DMA UNDERFLOW val = 0x%lx", val);
		if (val & LAYER2_DMA_FIFO_OVERFLOW)
			drm_dbg(&kmb->drm,
				"LAYER2:GL0 DMA OVERFLOW val = 0x%lx", val);

		 
		if (val & LAYER3_DMA_FIFO_UNDERFLOW)
			drm_dbg(&kmb->drm,
				"LAYER3:GL1 DMA UNDERFLOW val = 0x%lx", val);
		if (val & LAYER3_DMA_FIFO_OVERFLOW)
			drm_dbg(&kmb->drm,
				"LAYER3:GL1 DMA OVERFLOW val = 0x%lx", val);
	}

	spin_unlock(&kmb->irq_lock);

	if (status & LCD_INT_LAYER) {
		 
		kmb_write_lcd(kmb, LCD_INT_CLEAR, LCD_INT_LAYER);
	}

	 
	kmb_set_bitmask_lcd(kmb, LCD_INT_CLEAR, 1);
	return IRQ_HANDLED;
}

 
static irqreturn_t kmb_isr(int irq, void *arg)
{
	struct drm_device *dev = (struct drm_device *)arg;

	handle_lcd_irq(dev);
	return IRQ_HANDLED;
}

static void kmb_irq_reset(struct drm_device *drm)
{
	kmb_write_lcd(to_kmb(drm), LCD_INT_CLEAR, 0xFFFF);
	kmb_write_lcd(to_kmb(drm), LCD_INT_ENABLE, 0);
}

static int kmb_irq_install(struct drm_device *drm, unsigned int irq)
{
	if (irq == IRQ_NOTCONNECTED)
		return -ENOTCONN;

	kmb_irq_reset(drm);

	return request_irq(irq, kmb_isr, 0, drm->driver->name, drm);
}

static void kmb_irq_uninstall(struct drm_device *drm)
{
	struct kmb_drm_private *kmb = to_kmb(drm);

	kmb_irq_reset(drm);
	free_irq(kmb->irq_lcd, drm);
}

DEFINE_DRM_GEM_DMA_FOPS(fops);

static const struct drm_driver kmb_driver = {
	.driver_features = DRIVER_GEM |
	    DRIVER_MODESET | DRIVER_ATOMIC,
	 
	.fops = &fops,
	DRM_GEM_DMA_DRIVER_OPS_VMAP,
	.name = "kmb-drm",
	.desc = "KEEMBAY DISPLAY DRIVER",
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
};

static int kmb_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct drm_device *drm = dev_get_drvdata(dev);
	struct kmb_drm_private *kmb = to_kmb(drm);

	drm_dev_unregister(drm);
	drm_kms_helper_poll_fini(drm);
	of_node_put(kmb->crtc.port);
	kmb->crtc.port = NULL;
	pm_runtime_get_sync(drm->dev);
	kmb_irq_uninstall(drm);
	pm_runtime_put_sync(drm->dev);
	pm_runtime_disable(drm->dev);

	of_reserved_mem_device_release(drm->dev);

	 
	kmb_display_clk_disable(kmb);

	dev_set_drvdata(dev, NULL);

	 
	kmb_dsi_host_unregister(kmb->kmb_dsi);
	drm_atomic_helper_shutdown(drm);
	return 0;
}

static int kmb_probe(struct platform_device *pdev)
{
	struct device *dev = get_device(&pdev->dev);
	struct kmb_drm_private *kmb;
	int ret = 0;
	struct device_node *dsi_in;
	struct device_node *dsi_node;
	struct platform_device *dsi_pdev;

	 
	dsi_in = of_graph_get_endpoint_by_regs(dev->of_node, 0, 0);
	if (!dsi_in) {
		DRM_ERROR("Failed to get dsi_in node info from DT");
		return -EINVAL;
	}
	dsi_node = of_graph_get_remote_port_parent(dsi_in);
	if (!dsi_node) {
		of_node_put(dsi_in);
		DRM_ERROR("Failed to get dsi node from DT\n");
		return -EINVAL;
	}

	dsi_pdev = of_find_device_by_node(dsi_node);
	if (!dsi_pdev) {
		of_node_put(dsi_in);
		of_node_put(dsi_node);
		DRM_ERROR("Failed to get dsi platform device\n");
		return -EINVAL;
	}

	of_node_put(dsi_in);
	of_node_put(dsi_node);
	ret = kmb_dsi_host_bridge_init(get_device(&dsi_pdev->dev));

	if (ret == -EPROBE_DEFER) {
		return -EPROBE_DEFER;
	} else if (ret) {
		DRM_ERROR("probe failed to initialize DSI host bridge\n");
		return ret;
	}

	 
	kmb = devm_drm_dev_alloc(dev, &kmb_driver,
				 struct kmb_drm_private, drm);
	if (IS_ERR(kmb))
		return PTR_ERR(kmb);

	dev_set_drvdata(dev, &kmb->drm);

	 
	kmb->kmb_dsi = kmb_dsi_init(dsi_pdev);
	if (IS_ERR(kmb->kmb_dsi)) {
		drm_err(&kmb->drm, "failed to initialize DSI\n");
		ret = PTR_ERR(kmb->kmb_dsi);
		goto err_free1;
	}

	kmb->kmb_dsi->dev = &dsi_pdev->dev;
	kmb->kmb_dsi->pdev = dsi_pdev;
	ret = kmb_hw_init(&kmb->drm, 0);
	if (ret)
		goto err_free1;

	ret = kmb_setup_mode_config(&kmb->drm);
	if (ret)
		goto err_free;

	ret = kmb_irq_install(&kmb->drm, kmb->irq_lcd);
	if (ret < 0) {
		drm_err(&kmb->drm, "failed to install IRQ handler\n");
		goto err_irq;
	}

	drm_kms_helper_poll_init(&kmb->drm);

	 
	ret = drm_dev_register(&kmb->drm, 0);
	if (ret)
		goto err_register;

	drm_fbdev_dma_setup(&kmb->drm, 0);

	return 0;

 err_register:
	drm_kms_helper_poll_fini(&kmb->drm);
 err_irq:
	pm_runtime_disable(kmb->drm.dev);
 err_free:
	drm_crtc_cleanup(&kmb->crtc);
	drm_mode_config_cleanup(&kmb->drm);
 err_free1:
	dev_set_drvdata(dev, NULL);
	kmb_dsi_host_unregister(kmb->kmb_dsi);

	return ret;
}

static const struct of_device_id kmb_of_match[] = {
	{.compatible = "intel,keembay-display"},
	{},
};

MODULE_DEVICE_TABLE(of, kmb_of_match);

static int __maybe_unused kmb_pm_suspend(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);
	struct kmb_drm_private *kmb = to_kmb(drm);

	drm_kms_helper_poll_disable(drm);

	kmb->state = drm_atomic_helper_suspend(drm);
	if (IS_ERR(kmb->state)) {
		drm_kms_helper_poll_enable(drm);
		return PTR_ERR(kmb->state);
	}

	return 0;
}

static int __maybe_unused kmb_pm_resume(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);
	struct kmb_drm_private *kmb = drm ? to_kmb(drm) : NULL;

	if (!kmb)
		return 0;

	drm_atomic_helper_resume(drm, kmb->state);
	drm_kms_helper_poll_enable(drm);

	return 0;
}

static SIMPLE_DEV_PM_OPS(kmb_pm_ops, kmb_pm_suspend, kmb_pm_resume);

static struct platform_driver kmb_platform_driver = {
	.probe = kmb_probe,
	.remove = kmb_remove,
	.driver = {
		.name = "kmb-drm",
		.pm = &kmb_pm_ops,
		.of_match_table = kmb_of_match,
	},
};

drm_module_platform_driver(kmb_platform_driver);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Keembay Display driver");
MODULE_LICENSE("GPL v2");
