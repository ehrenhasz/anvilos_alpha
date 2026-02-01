
 

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>

#include <linux/w1.h>

 
#define MXC_W1_CONTROL		0x00
# define MXC_W1_CONTROL_RDST	BIT(3)
# define MXC_W1_CONTROL_WR(x)	BIT(5 - (x))
# define MXC_W1_CONTROL_PST	BIT(6)
# define MXC_W1_CONTROL_RPP	BIT(7)
#define MXC_W1_TIME_DIVIDER	0x02
#define MXC_W1_RESET		0x04
# define MXC_W1_RESET_RST	BIT(0)

struct mxc_w1_device {
	void __iomem *regs;
	struct clk *clk;
	struct w1_bus_master bus_master;
};

 
static u8 mxc_w1_ds2_reset_bus(void *data)
{
	struct mxc_w1_device *dev = data;
	ktime_t timeout;

	writeb(MXC_W1_CONTROL_RPP, dev->regs + MXC_W1_CONTROL);

	 
	timeout = ktime_add_us(ktime_get(), 1500);

	udelay(511 + 512);

	do {
		u8 ctrl = readb(dev->regs + MXC_W1_CONTROL);

		 
		if (!(ctrl & MXC_W1_CONTROL_RPP))
			return !(ctrl & MXC_W1_CONTROL_PST);
	} while (ktime_before(ktime_get(), timeout));

	return 1;
}

 
static u8 mxc_w1_ds2_touch_bit(void *data, u8 bit)
{
	struct mxc_w1_device *dev = data;
	ktime_t timeout;

	writeb(MXC_W1_CONTROL_WR(bit), dev->regs + MXC_W1_CONTROL);

	 
	timeout = ktime_add_us(ktime_get(), 200);

	udelay(60);

	do {
		u8 ctrl = readb(dev->regs + MXC_W1_CONTROL);

		 
		if (!(ctrl & MXC_W1_CONTROL_WR(bit)))
			return !!(ctrl & MXC_W1_CONTROL_RDST);
	} while (ktime_before(ktime_get(), timeout));

	return 0;
}

static int mxc_w1_probe(struct platform_device *pdev)
{
	struct mxc_w1_device *mdev;
	unsigned long clkrate;
	unsigned int clkdiv;
	int err;

	mdev = devm_kzalloc(&pdev->dev, sizeof(struct mxc_w1_device),
			    GFP_KERNEL);
	if (!mdev)
		return -ENOMEM;

	mdev->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(mdev->clk))
		return PTR_ERR(mdev->clk);

	err = clk_prepare_enable(mdev->clk);
	if (err)
		return err;

	clkrate = clk_get_rate(mdev->clk);
	if (clkrate < 10000000)
		dev_warn(&pdev->dev,
			 "Low clock frequency causes improper function\n");

	clkdiv = DIV_ROUND_CLOSEST(clkrate, 1000000);
	clkrate /= clkdiv;
	if ((clkrate < 980000) || (clkrate > 1020000))
		dev_warn(&pdev->dev,
			 "Incorrect time base frequency %lu Hz\n", clkrate);

	mdev->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(mdev->regs)) {
		err = PTR_ERR(mdev->regs);
		goto out_disable_clk;
	}

	 
	writeb(MXC_W1_RESET_RST, mdev->regs + MXC_W1_RESET);
	writeb(0, mdev->regs + MXC_W1_RESET);

	writeb(clkdiv - 1, mdev->regs + MXC_W1_TIME_DIVIDER);

	mdev->bus_master.data = mdev;
	mdev->bus_master.reset_bus = mxc_w1_ds2_reset_bus;
	mdev->bus_master.touch_bit = mxc_w1_ds2_touch_bit;

	platform_set_drvdata(pdev, mdev);

	err = w1_add_master_device(&mdev->bus_master);
	if (err)
		goto out_disable_clk;

	return 0;

out_disable_clk:
	clk_disable_unprepare(mdev->clk);
	return err;
}

 
static int mxc_w1_remove(struct platform_device *pdev)
{
	struct mxc_w1_device *mdev = platform_get_drvdata(pdev);

	w1_remove_master_device(&mdev->bus_master);

	clk_disable_unprepare(mdev->clk);

	return 0;
}

static const struct of_device_id mxc_w1_dt_ids[] = {
	{ .compatible = "fsl,imx21-owire" },
	{   }
};
MODULE_DEVICE_TABLE(of, mxc_w1_dt_ids);

static struct platform_driver mxc_w1_driver = {
	.driver = {
		.name = "mxc_w1",
		.of_match_table = mxc_w1_dt_ids,
	},
	.probe = mxc_w1_probe,
	.remove = mxc_w1_remove,
};
module_platform_driver(mxc_w1_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Freescale Semiconductors Inc");
MODULE_DESCRIPTION("Driver for One-Wire on MXC");
