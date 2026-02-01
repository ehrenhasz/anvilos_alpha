
 
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/stat.h>
#include <linux/slab.h>

struct xgene_reboot_context {
	struct device *dev;
	void *csr;
	u32 mask;
	struct notifier_block restart_handler;
};

static int xgene_restart_handler(struct notifier_block *this,
				 unsigned long mode, void *cmd)
{
	struct xgene_reboot_context *ctx =
		container_of(this, struct xgene_reboot_context,
			     restart_handler);

	 
	writel(ctx->mask, ctx->csr);

	mdelay(1000);

	dev_emerg(ctx->dev, "Unable to restart system\n");

	return NOTIFY_DONE;
}

static int xgene_reboot_probe(struct platform_device *pdev)
{
	struct xgene_reboot_context *ctx;
	struct device *dev = &pdev->dev;
	int err;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->csr = of_iomap(dev->of_node, 0);
	if (!ctx->csr) {
		dev_err(dev, "can not map resource\n");
		return -ENODEV;
	}

	if (of_property_read_u32(dev->of_node, "mask", &ctx->mask))
		ctx->mask = 0xFFFFFFFF;

	ctx->dev = dev;
	ctx->restart_handler.notifier_call = xgene_restart_handler;
	ctx->restart_handler.priority = 128;
	err = register_restart_handler(&ctx->restart_handler);
	if (err) {
		iounmap(ctx->csr);
		dev_err(dev, "cannot register restart handler (err=%d)\n", err);
	}

	return err;
}

static const struct of_device_id xgene_reboot_of_match[] = {
	{ .compatible = "apm,xgene-reboot" },
	{}
};

static struct platform_driver xgene_reboot_driver = {
	.probe = xgene_reboot_probe,
	.driver = {
		.name = "xgene-reboot",
		.of_match_table = xgene_reboot_of_match,
	},
};

static int __init xgene_reboot_init(void)
{
	return platform_driver_register(&xgene_reboot_driver);
}
device_initcall(xgene_reboot_init);
