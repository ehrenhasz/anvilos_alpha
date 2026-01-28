#ifndef __PLAT_S3C_SDHCI_H
#define __PLAT_S3C_SDHCI_H __FILE__
#include <linux/platform_data/mmc-sdhci-s3c.h>
#include "devs.h"
extern void s3c_sdhci_set_platdata(struct s3c_sdhci_platdata *pd,
				    struct s3c_sdhci_platdata *set);
extern void s3c_sdhci0_set_platdata(struct s3c_sdhci_platdata *pd);
extern void s3c_sdhci1_set_platdata(struct s3c_sdhci_platdata *pd);
extern void s3c_sdhci2_set_platdata(struct s3c_sdhci_platdata *pd);
extern void s3c_sdhci3_set_platdata(struct s3c_sdhci_platdata *pd);
extern struct s3c_sdhci_platdata s3c_hsmmc0_def_platdata;
extern struct s3c_sdhci_platdata s3c_hsmmc1_def_platdata;
extern struct s3c_sdhci_platdata s3c_hsmmc2_def_platdata;
extern struct s3c_sdhci_platdata s3c_hsmmc3_def_platdata;
extern void s3c64xx_setup_sdhci0_cfg_gpio(struct platform_device *, int w);
extern void s3c64xx_setup_sdhci1_cfg_gpio(struct platform_device *, int w);
extern void s3c64xx_setup_sdhci2_cfg_gpio(struct platform_device *, int w);
#ifdef CONFIG_S3C64XX_SETUP_SDHCI
static inline void s3c6400_default_sdhci0(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC
	s3c_hsmmc0_def_platdata.cfg_gpio = s3c64xx_setup_sdhci0_cfg_gpio;
#endif
}
static inline void s3c6400_default_sdhci1(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC1
	s3c_hsmmc1_def_platdata.cfg_gpio = s3c64xx_setup_sdhci1_cfg_gpio;
#endif
}
static inline void s3c6400_default_sdhci2(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC2
	s3c_hsmmc2_def_platdata.cfg_gpio = s3c64xx_setup_sdhci2_cfg_gpio;
#endif
}
static inline void s3c6410_default_sdhci0(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC
	s3c_hsmmc0_def_platdata.cfg_gpio = s3c64xx_setup_sdhci0_cfg_gpio;
#endif
}
static inline void s3c6410_default_sdhci1(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC1
	s3c_hsmmc1_def_platdata.cfg_gpio = s3c64xx_setup_sdhci1_cfg_gpio;
#endif
}
static inline void s3c6410_default_sdhci2(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC2
	s3c_hsmmc2_def_platdata.cfg_gpio = s3c64xx_setup_sdhci2_cfg_gpio;
#endif
}
#else
static inline void s3c6410_default_sdhci0(void) { }
static inline void s3c6410_default_sdhci1(void) { }
static inline void s3c6410_default_sdhci2(void) { }
static inline void s3c6400_default_sdhci0(void) { }
static inline void s3c6400_default_sdhci1(void) { }
static inline void s3c6400_default_sdhci2(void) { }
#endif  
static inline void s3c_sdhci_setname(int id, char *name)
{
	switch (id) {
#ifdef CONFIG_S3C_DEV_HSMMC
	case 0:
		s3c_device_hsmmc0.name = name;
		break;
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	case 1:
		s3c_device_hsmmc1.name = name;
		break;
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	case 2:
		s3c_device_hsmmc2.name = name;
		break;
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	case 3:
		s3c_device_hsmmc3.name = name;
		break;
#endif
	default:
		break;
	}
}
#endif  
