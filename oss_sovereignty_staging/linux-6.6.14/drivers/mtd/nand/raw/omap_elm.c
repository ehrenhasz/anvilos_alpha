
 

#define DRIVER_NAME	"omap-elm"

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/pm_runtime.h>
#include <linux/platform_data/elm.h>

#define ELM_SYSCONFIG			0x010
#define ELM_IRQSTATUS			0x018
#define ELM_IRQENABLE			0x01c
#define ELM_LOCATION_CONFIG		0x020
#define ELM_PAGE_CTRL			0x080
#define ELM_SYNDROME_FRAGMENT_0		0x400
#define ELM_SYNDROME_FRAGMENT_1		0x404
#define ELM_SYNDROME_FRAGMENT_2		0x408
#define ELM_SYNDROME_FRAGMENT_3		0x40c
#define ELM_SYNDROME_FRAGMENT_4		0x410
#define ELM_SYNDROME_FRAGMENT_5		0x414
#define ELM_SYNDROME_FRAGMENT_6		0x418
#define ELM_LOCATION_STATUS		0x800
#define ELM_ERROR_LOCATION_0		0x880

 
#define INTR_STATUS_PAGE_VALID		BIT(8)

 
#define INTR_EN_PAGE_MASK		BIT(8)

 
#define ECC_BCH_LEVEL_MASK		0x3

 
#define ELM_SYNDROME_VALID		BIT(16)

 
#define ECC_CORRECTABLE_MASK		BIT(8)
#define ECC_NB_ERRORS_MASK		0x1f

 
#define ECC_ERROR_LOCATION_MASK		0x1fff

#define ELM_ECC_SIZE			0x7ff

#define SYNDROME_FRAGMENT_REG_SIZE	0x40
#define ERROR_LOCATION_SIZE		0x100

struct elm_registers {
	u32 elm_irqenable;
	u32 elm_sysconfig;
	u32 elm_location_config;
	u32 elm_page_ctrl;
	u32 elm_syndrome_fragment_6[ERROR_VECTOR_MAX];
	u32 elm_syndrome_fragment_5[ERROR_VECTOR_MAX];
	u32 elm_syndrome_fragment_4[ERROR_VECTOR_MAX];
	u32 elm_syndrome_fragment_3[ERROR_VECTOR_MAX];
	u32 elm_syndrome_fragment_2[ERROR_VECTOR_MAX];
	u32 elm_syndrome_fragment_1[ERROR_VECTOR_MAX];
	u32 elm_syndrome_fragment_0[ERROR_VECTOR_MAX];
};

struct elm_info {
	struct device *dev;
	void __iomem *elm_base;
	struct completion elm_completion;
	struct list_head list;
	enum bch_ecc bch_type;
	struct elm_registers elm_regs;
	int ecc_steps;
	int ecc_syndrome_size;
};

static LIST_HEAD(elm_devices);

static void elm_write_reg(struct elm_info *info, int offset, u32 val)
{
	writel(val, info->elm_base + offset);
}

static u32 elm_read_reg(struct elm_info *info, int offset)
{
	return readl(info->elm_base + offset);
}

 
int elm_config(struct device *dev, enum bch_ecc bch_type,
	int ecc_steps, int ecc_step_size, int ecc_syndrome_size)
{
	u32 reg_val;
	struct elm_info *info = dev_get_drvdata(dev);

	if (!info) {
		dev_err(dev, "Unable to configure elm - device not probed?\n");
		return -EPROBE_DEFER;
	}
	 
	if (ecc_step_size > ((ELM_ECC_SIZE + 1) / 2)) {
		dev_err(dev, "unsupported config ecc-size=%d\n", ecc_step_size);
		return -EINVAL;
	}
	 
	if (ecc_steps > ERROR_VECTOR_MAX && ecc_steps % ERROR_VECTOR_MAX) {
		dev_err(dev, "unsupported config ecc-step=%d\n", ecc_steps);
		return -EINVAL;
	}

	reg_val = (bch_type & ECC_BCH_LEVEL_MASK) | (ELM_ECC_SIZE << 16);
	elm_write_reg(info, ELM_LOCATION_CONFIG, reg_val);
	info->bch_type		= bch_type;
	info->ecc_steps		= ecc_steps;
	info->ecc_syndrome_size	= ecc_syndrome_size;

	return 0;
}
EXPORT_SYMBOL(elm_config);

 
static void elm_configure_page_mode(struct elm_info *info, int index,
		bool enable)
{
	u32 reg_val;

	reg_val = elm_read_reg(info, ELM_PAGE_CTRL);
	if (enable)
		reg_val |= BIT(index);	 
	else
		reg_val &= ~BIT(index);	 

	elm_write_reg(info, ELM_PAGE_CTRL, reg_val);
}

 
static void elm_load_syndrome(struct elm_info *info,
		struct elm_errorvec *err_vec, u8 *ecc)
{
	int i, offset;
	u32 val;

	for (i = 0; i < info->ecc_steps; i++) {

		 
		if (err_vec[i].error_reported) {
			elm_configure_page_mode(info, i, true);
			offset = ELM_SYNDROME_FRAGMENT_0 +
				SYNDROME_FRAGMENT_REG_SIZE * i;
			switch (info->bch_type) {
			case BCH8_ECC:
				 
				val = (__force u32)cpu_to_be32(*(u32 *)&ecc[9]);
				elm_write_reg(info, offset, val);

				 
				offset += 4;
				val = (__force u32)cpu_to_be32(*(u32 *)&ecc[5]);
				elm_write_reg(info, offset, val);

				 
				offset += 4;
				val = (__force u32)cpu_to_be32(*(u32 *)&ecc[1]);
				elm_write_reg(info, offset, val);

				 
				offset += 4;
				val = ecc[0];
				elm_write_reg(info, offset, val);
				break;
			case BCH4_ECC:
				 
				val = ((__force u32)cpu_to_be32(*(u32 *)&ecc[3]) >> 4) |
					((ecc[2] & 0xf) << 28);
				elm_write_reg(info, offset, val);

				 
				offset += 4;
				val = (__force u32)cpu_to_be32(*(u32 *)&ecc[0]) >> 12;
				elm_write_reg(info, offset, val);
				break;
			case BCH16_ECC:
				val = (__force u32)cpu_to_be32(*(u32 *)&ecc[22]);
				elm_write_reg(info, offset, val);
				offset += 4;
				val = (__force u32)cpu_to_be32(*(u32 *)&ecc[18]);
				elm_write_reg(info, offset, val);
				offset += 4;
				val = (__force u32)cpu_to_be32(*(u32 *)&ecc[14]);
				elm_write_reg(info, offset, val);
				offset += 4;
				val = (__force u32)cpu_to_be32(*(u32 *)&ecc[10]);
				elm_write_reg(info, offset, val);
				offset += 4;
				val = (__force u32)cpu_to_be32(*(u32 *)&ecc[6]);
				elm_write_reg(info, offset, val);
				offset += 4;
				val = (__force u32)cpu_to_be32(*(u32 *)&ecc[2]);
				elm_write_reg(info, offset, val);
				offset += 4;
				val = (__force u32)cpu_to_be32(*(u32 *)&ecc[0]) >> 16;
				elm_write_reg(info, offset, val);
				break;
			default:
				pr_err("invalid config bch_type\n");
			}
		}

		 
		ecc += info->ecc_syndrome_size;
	}
}

 
static void elm_start_processing(struct elm_info *info,
		struct elm_errorvec *err_vec)
{
	int i, offset;
	u32 reg_val;

	 
	for (i = 0; i < info->ecc_steps; i++) {
		if (err_vec[i].error_reported) {
			offset = ELM_SYNDROME_FRAGMENT_6 +
				SYNDROME_FRAGMENT_REG_SIZE * i;
			reg_val = elm_read_reg(info, offset);
			reg_val |= ELM_SYNDROME_VALID;
			elm_write_reg(info, offset, reg_val);
		}
	}
}

 
static void elm_error_correction(struct elm_info *info,
		struct elm_errorvec *err_vec)
{
	int i, j;
	int offset;
	u32 reg_val;

	for (i = 0; i < info->ecc_steps; i++) {

		 
		if (err_vec[i].error_reported) {
			offset = ELM_LOCATION_STATUS + ERROR_LOCATION_SIZE * i;
			reg_val = elm_read_reg(info, offset);

			 
			if (reg_val & ECC_CORRECTABLE_MASK) {
				offset = ELM_ERROR_LOCATION_0 +
					ERROR_LOCATION_SIZE * i;

				 
				err_vec[i].error_count = reg_val &
					ECC_NB_ERRORS_MASK;

				 
				for (j = 0; j < err_vec[i].error_count; j++) {

					reg_val = elm_read_reg(info, offset);
					err_vec[i].error_loc[j] = reg_val &
						ECC_ERROR_LOCATION_MASK;

					 
					offset += 4;
				}
			} else {
				err_vec[i].error_uncorrectable = true;
			}

			 
			elm_write_reg(info, ELM_IRQSTATUS, BIT(i));

			 
			elm_configure_page_mode(info, i, false);
		}
	}
}

 
void elm_decode_bch_error_page(struct device *dev, u8 *ecc_calc,
		struct elm_errorvec *err_vec)
{
	struct elm_info *info = dev_get_drvdata(dev);
	u32 reg_val;

	 
	reg_val = elm_read_reg(info, ELM_IRQSTATUS);
	elm_write_reg(info, ELM_IRQSTATUS, reg_val & INTR_STATUS_PAGE_VALID);
	elm_write_reg(info, ELM_IRQENABLE, INTR_EN_PAGE_MASK);

	 
	elm_load_syndrome(info, err_vec, ecc_calc);

	 
	elm_start_processing(info, err_vec);

	 
	wait_for_completion(&info->elm_completion);

	 
	reg_val = elm_read_reg(info, ELM_IRQENABLE);
	elm_write_reg(info, ELM_IRQENABLE, reg_val & ~INTR_EN_PAGE_MASK);
	elm_error_correction(info, err_vec);
}
EXPORT_SYMBOL(elm_decode_bch_error_page);

static irqreturn_t elm_isr(int this_irq, void *dev_id)
{
	u32 reg_val;
	struct elm_info *info = dev_id;

	reg_val = elm_read_reg(info, ELM_IRQSTATUS);

	 
	if (reg_val & INTR_STATUS_PAGE_VALID) {
		elm_write_reg(info, ELM_IRQSTATUS,
				reg_val & INTR_STATUS_PAGE_VALID);
		complete(&info->elm_completion);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static int elm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct elm_info *info;
	int irq;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = &pdev->dev;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	info->elm_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(info->elm_base))
		return PTR_ERR(info->elm_base);

	ret = devm_request_irq(&pdev->dev, irq, elm_isr, 0,
			       pdev->name, info);
	if (ret) {
		dev_err(&pdev->dev, "failure requesting %d\n", irq);
		return ret;
	}

	pm_runtime_enable(&pdev->dev);
	if (pm_runtime_get_sync(&pdev->dev) < 0) {
		ret = -EINVAL;
		pm_runtime_put_sync(&pdev->dev);
		pm_runtime_disable(&pdev->dev);
		dev_err(&pdev->dev, "can't enable clock\n");
		return ret;
	}

	init_completion(&info->elm_completion);
	INIT_LIST_HEAD(&info->list);
	list_add(&info->list, &elm_devices);
	platform_set_drvdata(pdev, info);
	return ret;
}

static void elm_remove(struct platform_device *pdev)
{
	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
}

#ifdef CONFIG_PM_SLEEP
 
static int elm_context_save(struct elm_info *info)
{
	struct elm_registers *regs = &info->elm_regs;
	enum bch_ecc bch_type = info->bch_type;
	u32 offset = 0, i;

	regs->elm_irqenable       = elm_read_reg(info, ELM_IRQENABLE);
	regs->elm_sysconfig       = elm_read_reg(info, ELM_SYSCONFIG);
	regs->elm_location_config = elm_read_reg(info, ELM_LOCATION_CONFIG);
	regs->elm_page_ctrl       = elm_read_reg(info, ELM_PAGE_CTRL);
	for (i = 0; i < ERROR_VECTOR_MAX; i++) {
		offset = i * SYNDROME_FRAGMENT_REG_SIZE;
		switch (bch_type) {
		case BCH16_ECC:
			regs->elm_syndrome_fragment_6[i] = elm_read_reg(info,
					ELM_SYNDROME_FRAGMENT_6 + offset);
			regs->elm_syndrome_fragment_5[i] = elm_read_reg(info,
					ELM_SYNDROME_FRAGMENT_5 + offset);
			regs->elm_syndrome_fragment_4[i] = elm_read_reg(info,
					ELM_SYNDROME_FRAGMENT_4 + offset);
			fallthrough;
		case BCH8_ECC:
			regs->elm_syndrome_fragment_3[i] = elm_read_reg(info,
					ELM_SYNDROME_FRAGMENT_3 + offset);
			regs->elm_syndrome_fragment_2[i] = elm_read_reg(info,
					ELM_SYNDROME_FRAGMENT_2 + offset);
			fallthrough;
		case BCH4_ECC:
			regs->elm_syndrome_fragment_1[i] = elm_read_reg(info,
					ELM_SYNDROME_FRAGMENT_1 + offset);
			regs->elm_syndrome_fragment_0[i] = elm_read_reg(info,
					ELM_SYNDROME_FRAGMENT_0 + offset);
			break;
		default:
			return -EINVAL;
		}
		 
		regs->elm_syndrome_fragment_6[i] = elm_read_reg(info,
					ELM_SYNDROME_FRAGMENT_6 + offset);
	}
	return 0;
}

 
static int elm_context_restore(struct elm_info *info)
{
	struct elm_registers *regs = &info->elm_regs;
	enum bch_ecc bch_type = info->bch_type;
	u32 offset = 0, i;

	elm_write_reg(info, ELM_IRQENABLE,	 regs->elm_irqenable);
	elm_write_reg(info, ELM_SYSCONFIG,	 regs->elm_sysconfig);
	elm_write_reg(info, ELM_LOCATION_CONFIG, regs->elm_location_config);
	elm_write_reg(info, ELM_PAGE_CTRL,	 regs->elm_page_ctrl);
	for (i = 0; i < ERROR_VECTOR_MAX; i++) {
		offset = i * SYNDROME_FRAGMENT_REG_SIZE;
		switch (bch_type) {
		case BCH16_ECC:
			elm_write_reg(info, ELM_SYNDROME_FRAGMENT_6 + offset,
					regs->elm_syndrome_fragment_6[i]);
			elm_write_reg(info, ELM_SYNDROME_FRAGMENT_5 + offset,
					regs->elm_syndrome_fragment_5[i]);
			elm_write_reg(info, ELM_SYNDROME_FRAGMENT_4 + offset,
					regs->elm_syndrome_fragment_4[i]);
			fallthrough;
		case BCH8_ECC:
			elm_write_reg(info, ELM_SYNDROME_FRAGMENT_3 + offset,
					regs->elm_syndrome_fragment_3[i]);
			elm_write_reg(info, ELM_SYNDROME_FRAGMENT_2 + offset,
					regs->elm_syndrome_fragment_2[i]);
			fallthrough;
		case BCH4_ECC:
			elm_write_reg(info, ELM_SYNDROME_FRAGMENT_1 + offset,
					regs->elm_syndrome_fragment_1[i]);
			elm_write_reg(info, ELM_SYNDROME_FRAGMENT_0 + offset,
					regs->elm_syndrome_fragment_0[i]);
			break;
		default:
			return -EINVAL;
		}
		 
		elm_write_reg(info, ELM_SYNDROME_FRAGMENT_6 + offset,
					regs->elm_syndrome_fragment_6[i] &
							 ELM_SYNDROME_VALID);
	}
	return 0;
}

static int elm_suspend(struct device *dev)
{
	struct elm_info *info = dev_get_drvdata(dev);
	elm_context_save(info);
	pm_runtime_put_sync(dev);
	return 0;
}

static int elm_resume(struct device *dev)
{
	struct elm_info *info = dev_get_drvdata(dev);
	pm_runtime_get_sync(dev);
	elm_context_restore(info);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(elm_pm_ops, elm_suspend, elm_resume);

#ifdef CONFIG_OF
static const struct of_device_id elm_of_match[] = {
	{ .compatible = "ti,am3352-elm" },
	{ .compatible = "ti,am64-elm" },
	{},
};
MODULE_DEVICE_TABLE(of, elm_of_match);
#endif

static struct platform_driver elm_driver = {
	.driver	= {
		.name	= DRIVER_NAME,
		.of_match_table = of_match_ptr(elm_of_match),
		.pm	= &elm_pm_ops,
	},
	.probe	= elm_probe,
	.remove_new = elm_remove,
};

module_platform_driver(elm_driver);

MODULE_DESCRIPTION("ELM driver for BCH error correction");
MODULE_AUTHOR("Texas Instruments");
MODULE_ALIAS("platform:" DRIVER_NAME);
MODULE_LICENSE("GPL v2");
