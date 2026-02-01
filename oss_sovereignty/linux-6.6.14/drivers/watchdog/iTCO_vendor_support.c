
 

 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

 
#define DRV_NAME	"iTCO_vendor_support"
#define DRV_VERSION	"1.04"

 
#include <linux/module.h>		 
#include <linux/moduleparam.h>		 
#include <linux/types.h>		 
#include <linux/errno.h>		 
#include <linux/kernel.h>		 
#include <linux/init.h>			 
#include <linux/ioport.h>		 
#include <linux/io.h>			 

#include "iTCO_vendor.h"

 
 
#define SUPERMICRO_OLD_BOARD	1
 
#define SUPERMICRO_NEW_BOARD	2
 
#define BROKEN_BIOS		911

int iTCO_vendorsupport;
EXPORT_SYMBOL(iTCO_vendorsupport);

module_param_named(vendorsupport, iTCO_vendorsupport, int, 0);
MODULE_PARM_DESC(vendorsupport, "iTCO vendor specific support mode, default="
			"0 (none), 1=SuperMicro Pent3, 911=Broken SMI BIOS");

 

 

static void supermicro_old_pre_start(struct resource *smires)
{
	unsigned long val32;

	 
	val32 = inl(smires->start);
	val32 &= 0xffffdfff;	 
	outl(val32, smires->start);	 
}

static void supermicro_old_pre_stop(struct resource *smires)
{
	unsigned long val32;

	 
	val32 = inl(smires->start);
	val32 |= 0x00002000;	 
	outl(val32, smires->start);	 
}

 

static void broken_bios_start(struct resource *smires)
{
	unsigned long val32;

	val32 = inl(smires->start);
	 
	val32 &= 0xffffdffe;
	outl(val32, smires->start);
}

static void broken_bios_stop(struct resource *smires)
{
	unsigned long val32;

	val32 = inl(smires->start);
	 
	val32 |= 0x00002001;
	outl(val32, smires->start);
}

 

void iTCO_vendor_pre_start(struct resource *smires,
			   unsigned int heartbeat)
{
	switch (iTCO_vendorsupport) {
	case SUPERMICRO_OLD_BOARD:
		supermicro_old_pre_start(smires);
		break;
	case BROKEN_BIOS:
		broken_bios_start(smires);
		break;
	}
}
EXPORT_SYMBOL(iTCO_vendor_pre_start);

void iTCO_vendor_pre_stop(struct resource *smires)
{
	switch (iTCO_vendorsupport) {
	case SUPERMICRO_OLD_BOARD:
		supermicro_old_pre_stop(smires);
		break;
	case BROKEN_BIOS:
		broken_bios_stop(smires);
		break;
	}
}
EXPORT_SYMBOL(iTCO_vendor_pre_stop);

int iTCO_vendor_check_noreboot_on(void)
{
	switch (iTCO_vendorsupport) {
	case SUPERMICRO_OLD_BOARD:
		return 0;
	default:
		return 1;
	}
}
EXPORT_SYMBOL(iTCO_vendor_check_noreboot_on);

static int __init iTCO_vendor_init_module(void)
{
	if (iTCO_vendorsupport == SUPERMICRO_NEW_BOARD) {
		pr_warn("Option vendorsupport=%d is no longer supported, "
			"please use the w83627hf_wdt driver instead\n",
			SUPERMICRO_NEW_BOARD);
		return -EINVAL;
	}
	pr_info("vendor-support=%d\n", iTCO_vendorsupport);
	return 0;
}

static void __exit iTCO_vendor_exit_module(void)
{
	pr_info("Module Unloaded\n");
}

module_init(iTCO_vendor_init_module);
module_exit(iTCO_vendor_exit_module);

MODULE_AUTHOR("Wim Van Sebroeck <wim@iguana.be>, "
		"R. Seretny <lkpatches@paypc.com>");
MODULE_DESCRIPTION("Intel TCO Vendor Specific WatchDog Timer Driver Support");
MODULE_VERSION(DRV_VERSION);
MODULE_LICENSE("GPL");
