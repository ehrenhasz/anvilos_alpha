#ifndef __ASM_CDMM_H
#define __ASM_CDMM_H
#include <linux/device.h>
#include <linux/mod_devicetable.h>
struct mips_cdmm_device {
	struct device		dev;
	unsigned int		cpu;
	struct resource		res;
	unsigned int		type;
	unsigned int		rev;
};
struct mips_cdmm_driver {
	struct device_driver	drv;
	int			(*probe)(struct mips_cdmm_device *);
	int			(*remove)(struct mips_cdmm_device *);
	void			(*shutdown)(struct mips_cdmm_device *);
	int			(*cpu_down)(struct mips_cdmm_device *);
	int			(*cpu_up)(struct mips_cdmm_device *);
	const struct mips_cdmm_device_id *id_table;
};
phys_addr_t mips_cdmm_phys_base(void);
extern struct bus_type mips_cdmm_bustype;
void __iomem *mips_cdmm_early_probe(unsigned int dev_type);
#define to_mips_cdmm_device(d)	container_of(d, struct mips_cdmm_device, dev)
#define mips_cdmm_get_drvdata(d)	dev_get_drvdata(&d->dev)
#define mips_cdmm_set_drvdata(d, p)	dev_set_drvdata(&d->dev, p)
int mips_cdmm_driver_register(struct mips_cdmm_driver *);
void mips_cdmm_driver_unregister(struct mips_cdmm_driver *);
#define module_mips_cdmm_driver(__mips_cdmm_driver) \
	module_driver(__mips_cdmm_driver, mips_cdmm_driver_register, \
			mips_cdmm_driver_unregister)
#define builtin_mips_cdmm_driver(__mips_cdmm_driver) \
	builtin_driver(__mips_cdmm_driver, mips_cdmm_driver_register)
#ifdef CONFIG_MIPS_EJTAG_FDC_EARLYCON
int setup_early_fdc_console(void);
#else
static inline int setup_early_fdc_console(void)
{
	return -ENODEV;
}
#endif
#endif  
