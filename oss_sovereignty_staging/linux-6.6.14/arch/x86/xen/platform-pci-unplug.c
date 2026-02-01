

 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/io.h>
#include <linux/export.h>

#include <xen/xen.h>
#include <xen/platform_pci.h>
#include "xen-ops.h"

#define XEN_PLATFORM_ERR_MAGIC -1
#define XEN_PLATFORM_ERR_PROTOCOL -2
#define XEN_PLATFORM_ERR_BLACKLIST -3

 
static int xen_platform_pci_unplug;
static int xen_emul_unplug;

static int check_platform_magic(void)
{
	short magic;
	char protocol;

	magic = inw(XEN_IOPORT_MAGIC);
	if (magic != XEN_IOPORT_MAGIC_VAL) {
		pr_err("Xen Platform PCI: unrecognised magic value\n");
		return XEN_PLATFORM_ERR_MAGIC;
	}

	protocol = inb(XEN_IOPORT_PROTOVER);

	pr_debug("Xen Platform PCI: I/O protocol version %d\n",
			protocol);

	switch (protocol) {
	case 1:
		outw(XEN_IOPORT_LINUX_PRODNUM, XEN_IOPORT_PRODNUM);
		outl(XEN_IOPORT_LINUX_DRVVER, XEN_IOPORT_DRVVER);
		if (inw(XEN_IOPORT_MAGIC) != XEN_IOPORT_MAGIC_VAL) {
			pr_err("Xen Platform: blacklisted by host\n");
			return XEN_PLATFORM_ERR_BLACKLIST;
		}
		break;
	default:
		pr_warn("Xen Platform PCI: unknown I/O protocol version\n");
		return XEN_PLATFORM_ERR_PROTOCOL;
	}

	return 0;
}

bool xen_has_pv_devices(void)
{
	if (!xen_domain())
		return false;

	 
	if (xen_pv_domain() || xen_pvh_domain())
		return true;

	 
	if (xen_platform_pci_unplug == 0)
		return false;

	if (xen_platform_pci_unplug & XEN_UNPLUG_NEVER)
		return false;

	if (xen_platform_pci_unplug & XEN_UNPLUG_ALL)
		return true;

	 
	if (xen_platform_pci_unplug & XEN_UNPLUG_UNNECESSARY)
		return true;

	 
	return false;
}
EXPORT_SYMBOL_GPL(xen_has_pv_devices);

static bool __xen_has_pv_device(int state)
{
	 
	if (xen_hvm_domain() && (xen_platform_pci_unplug & state))
		return true;

	return xen_has_pv_devices();
}

bool xen_has_pv_nic_devices(void)
{
	return __xen_has_pv_device(XEN_UNPLUG_ALL_NICS | XEN_UNPLUG_ALL);
}
EXPORT_SYMBOL_GPL(xen_has_pv_nic_devices);

bool xen_has_pv_disk_devices(void)
{
	return __xen_has_pv_device(XEN_UNPLUG_ALL_IDE_DISKS |
				   XEN_UNPLUG_AUX_IDE_DISKS | XEN_UNPLUG_ALL);
}
EXPORT_SYMBOL_GPL(xen_has_pv_disk_devices);

 
bool xen_has_pv_and_legacy_disk_devices(void)
{
	if (!xen_domain())
		return false;

	 
	if (xen_pv_domain())
		return false;

	if (xen_platform_pci_unplug & XEN_UNPLUG_UNNECESSARY)
		return true;

	return false;
}
EXPORT_SYMBOL_GPL(xen_has_pv_and_legacy_disk_devices);

void xen_unplug_emulated_devices(void)
{
	int r;

	 
	if (xen_pvh_domain())
		return;

	 
	if (xen_emul_unplug & XEN_UNPLUG_NEVER)
		return;
	 
	r = check_platform_magic();
	 
	if (r && !(r == XEN_PLATFORM_ERR_MAGIC &&
			(xen_emul_unplug & XEN_UNPLUG_UNNECESSARY)))
		return;
	 
	if (!xen_emul_unplug) {
		if (xen_must_unplug_nics()) {
			pr_info("Netfront and the Xen platform PCI driver have "
					"been compiled for this kernel: unplug emulated NICs.\n");
			xen_emul_unplug |= XEN_UNPLUG_ALL_NICS;
		}
		if (xen_must_unplug_disks()) {
			pr_info("Blkfront and the Xen platform PCI driver have "
					"been compiled for this kernel: unplug emulated disks.\n"
					"You might have to change the root device\n"
					"from /dev/hd[a-d] to /dev/xvd[a-d]\n"
					"in your root= kernel command line option\n");
			xen_emul_unplug |= XEN_UNPLUG_ALL_IDE_DISKS;
		}
	}
	 
	if (!(xen_emul_unplug & XEN_UNPLUG_UNNECESSARY))
		outw(xen_emul_unplug, XEN_IOPORT_UNPLUG);
	xen_platform_pci_unplug = xen_emul_unplug;
}

static int __init parse_xen_emul_unplug(char *arg)
{
	char *p, *q;
	int l;

	for (p = arg; p; p = q) {
		q = strchr(p, ',');
		if (q) {
			l = q - p;
			q++;
		} else {
			l = strlen(p);
		}
		if (!strncmp(p, "all", l))
			xen_emul_unplug |= XEN_UNPLUG_ALL;
		else if (!strncmp(p, "ide-disks", l))
			xen_emul_unplug |= XEN_UNPLUG_ALL_IDE_DISKS;
		else if (!strncmp(p, "aux-ide-disks", l))
			xen_emul_unplug |= XEN_UNPLUG_AUX_IDE_DISKS;
		else if (!strncmp(p, "nics", l))
			xen_emul_unplug |= XEN_UNPLUG_ALL_NICS;
		else if (!strncmp(p, "unnecessary", l))
			xen_emul_unplug |= XEN_UNPLUG_UNNECESSARY;
		else if (!strncmp(p, "never", l))
			xen_emul_unplug |= XEN_UNPLUG_NEVER;
		else
			pr_warn("unrecognised option '%s' "
				 "in parameter 'xen_emul_unplug'\n", p);
	}
	return 0;
}
early_param("xen_emul_unplug", parse_xen_emul_unplug);
