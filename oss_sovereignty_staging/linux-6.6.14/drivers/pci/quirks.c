
 

#include <linux/bitfield.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/pci.h>
#include <linux/isa-dma.h>  
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/ktime.h>
#include <linux/mm.h>
#include <linux/nvme.h>
#include <linux/platform_data/x86/apple.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/switchtec.h>
#include "pci.h"

 
bool pcie_failed_link_retrain(struct pci_dev *dev)
{
	static const struct pci_device_id ids[] = {
		{ PCI_VDEVICE(ASMEDIA, 0x2824) },  
		{}
	};
	u16 lnksta, lnkctl2;

	if (!pci_is_pcie(dev) || !pcie_downstream_port(dev) ||
	    !pcie_cap_has_lnkctl2(dev) || !dev->link_active_reporting)
		return false;

	pcie_capability_read_word(dev, PCI_EXP_LNKCTL2, &lnkctl2);
	pcie_capability_read_word(dev, PCI_EXP_LNKSTA, &lnksta);
	if ((lnksta & (PCI_EXP_LNKSTA_LBMS | PCI_EXP_LNKSTA_DLLLA)) ==
	    PCI_EXP_LNKSTA_LBMS) {
		pci_info(dev, "broken device, retraining non-functional downstream link at 2.5GT/s\n");

		lnkctl2 &= ~PCI_EXP_LNKCTL2_TLS;
		lnkctl2 |= PCI_EXP_LNKCTL2_TLS_2_5GT;
		pcie_capability_write_word(dev, PCI_EXP_LNKCTL2, lnkctl2);

		if (pcie_retrain_link(dev, false)) {
			pci_info(dev, "retraining failed\n");
			return false;
		}

		pcie_capability_read_word(dev, PCI_EXP_LNKSTA, &lnksta);
	}

	if ((lnksta & PCI_EXP_LNKSTA_DLLLA) &&
	    (lnkctl2 & PCI_EXP_LNKCTL2_TLS) == PCI_EXP_LNKCTL2_TLS_2_5GT &&
	    pci_match_id(ids, dev)) {
		u32 lnkcap;

		pci_info(dev, "removing 2.5GT/s downstream link speed restriction\n");
		pcie_capability_read_dword(dev, PCI_EXP_LNKCAP, &lnkcap);
		lnkctl2 &= ~PCI_EXP_LNKCTL2_TLS;
		lnkctl2 |= lnkcap & PCI_EXP_LNKCAP_SLS;
		pcie_capability_write_word(dev, PCI_EXP_LNKCTL2, lnkctl2);

		if (pcie_retrain_link(dev, false)) {
			pci_info(dev, "retraining failed\n");
			return false;
		}
	}

	return true;
}

static ktime_t fixup_debug_start(struct pci_dev *dev,
				 void (*fn)(struct pci_dev *dev))
{
	if (initcall_debug)
		pci_info(dev, "calling  %pS @ %i\n", fn, task_pid_nr(current));

	return ktime_get();
}

static void fixup_debug_report(struct pci_dev *dev, ktime_t calltime,
			       void (*fn)(struct pci_dev *dev))
{
	ktime_t delta, rettime;
	unsigned long long duration;

	rettime = ktime_get();
	delta = ktime_sub(rettime, calltime);
	duration = (unsigned long long) ktime_to_ns(delta) >> 10;
	if (initcall_debug || duration > 10000)
		pci_info(dev, "%pS took %lld usecs\n", fn, duration);
}

static void pci_do_fixups(struct pci_dev *dev, struct pci_fixup *f,
			  struct pci_fixup *end)
{
	ktime_t calltime;

	for (; f < end; f++)
		if ((f->class == (u32) (dev->class >> f->class_shift) ||
		     f->class == (u32) PCI_ANY_ID) &&
		    (f->vendor == dev->vendor ||
		     f->vendor == (u16) PCI_ANY_ID) &&
		    (f->device == dev->device ||
		     f->device == (u16) PCI_ANY_ID)) {
			void (*hook)(struct pci_dev *dev);
#ifdef CONFIG_HAVE_ARCH_PREL32_RELOCATIONS
			hook = offset_to_ptr(&f->hook_offset);
#else
			hook = f->hook;
#endif
			calltime = fixup_debug_start(dev, hook);
			hook(dev);
			fixup_debug_report(dev, calltime, hook);
		}
}

extern struct pci_fixup __start_pci_fixups_early[];
extern struct pci_fixup __end_pci_fixups_early[];
extern struct pci_fixup __start_pci_fixups_header[];
extern struct pci_fixup __end_pci_fixups_header[];
extern struct pci_fixup __start_pci_fixups_final[];
extern struct pci_fixup __end_pci_fixups_final[];
extern struct pci_fixup __start_pci_fixups_enable[];
extern struct pci_fixup __end_pci_fixups_enable[];
extern struct pci_fixup __start_pci_fixups_resume[];
extern struct pci_fixup __end_pci_fixups_resume[];
extern struct pci_fixup __start_pci_fixups_resume_early[];
extern struct pci_fixup __end_pci_fixups_resume_early[];
extern struct pci_fixup __start_pci_fixups_suspend[];
extern struct pci_fixup __end_pci_fixups_suspend[];
extern struct pci_fixup __start_pci_fixups_suspend_late[];
extern struct pci_fixup __end_pci_fixups_suspend_late[];

static bool pci_apply_fixup_final_quirks;

void pci_fixup_device(enum pci_fixup_pass pass, struct pci_dev *dev)
{
	struct pci_fixup *start, *end;

	switch (pass) {
	case pci_fixup_early:
		start = __start_pci_fixups_early;
		end = __end_pci_fixups_early;
		break;

	case pci_fixup_header:
		start = __start_pci_fixups_header;
		end = __end_pci_fixups_header;
		break;

	case pci_fixup_final:
		if (!pci_apply_fixup_final_quirks)
			return;
		start = __start_pci_fixups_final;
		end = __end_pci_fixups_final;
		break;

	case pci_fixup_enable:
		start = __start_pci_fixups_enable;
		end = __end_pci_fixups_enable;
		break;

	case pci_fixup_resume:
		start = __start_pci_fixups_resume;
		end = __end_pci_fixups_resume;
		break;

	case pci_fixup_resume_early:
		start = __start_pci_fixups_resume_early;
		end = __end_pci_fixups_resume_early;
		break;

	case pci_fixup_suspend:
		start = __start_pci_fixups_suspend;
		end = __end_pci_fixups_suspend;
		break;

	case pci_fixup_suspend_late:
		start = __start_pci_fixups_suspend_late;
		end = __end_pci_fixups_suspend_late;
		break;

	default:
		 
		return;
	}
	pci_do_fixups(dev, start, end);
}
EXPORT_SYMBOL(pci_fixup_device);

static int __init pci_apply_final_quirks(void)
{
	struct pci_dev *dev = NULL;
	u8 cls = 0;
	u8 tmp;

	if (pci_cache_line_size)
		pr_info("PCI: CLS %u bytes\n", pci_cache_line_size << 2);

	pci_apply_fixup_final_quirks = true;
	for_each_pci_dev(dev) {
		pci_fixup_device(pci_fixup_final, dev);
		 
		if (!pci_cache_line_size) {
			pci_read_config_byte(dev, PCI_CACHE_LINE_SIZE, &tmp);
			if (!cls)
				cls = tmp;
			if (!tmp || cls == tmp)
				continue;

			pci_info(dev, "CLS mismatch (%u != %u), using %u bytes\n",
			         cls << 2, tmp << 2,
				 pci_dfl_cache_line_size << 2);
			pci_cache_line_size = pci_dfl_cache_line_size;
		}
	}

	if (!pci_cache_line_size) {
		pr_info("PCI: CLS %u bytes, default %u\n", cls << 2,
			pci_dfl_cache_line_size << 2);
		pci_cache_line_size = cls ? cls : pci_dfl_cache_line_size;
	}

	return 0;
}
fs_initcall_sync(pci_apply_final_quirks);

 
static void quirk_mmio_always_on(struct pci_dev *dev)
{
	dev->mmio_always_on = 1;
}
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_ANY_ID, PCI_ANY_ID,
				PCI_CLASS_BRIDGE_HOST, 8, quirk_mmio_always_on);

 
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_MELLANOX, PCI_DEVICE_ID_MELLANOX_TAVOR, pci_disable_parity);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_MELLANOX, PCI_DEVICE_ID_MELLANOX_TAVOR_BRIDGE, pci_disable_parity);

 
static void quirk_passive_release(struct pci_dev *dev)
{
	struct pci_dev *d = NULL;
	unsigned char dlc;

	 
	while ((d = pci_get_device(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371SB_0, d))) {
		pci_read_config_byte(d, 0x82, &dlc);
		if (!(dlc & 1<<1)) {
			pci_info(d, "PIIX3: Enabling Passive Release\n");
			dlc |= 1<<1;
			pci_write_config_byte(d, 0x82, dlc);
		}
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82441,	quirk_passive_release);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82441,	quirk_passive_release);

#ifdef CONFIG_X86_32
 
static void quirk_isa_dma_hangs(struct pci_dev *dev)
{
	if (!isa_dma_bridge_buggy) {
		isa_dma_bridge_buggy = 1;
		pci_info(dev, "Activating ISA DMA hang workarounds\n");
	}
}
 
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C586_0,	quirk_isa_dma_hangs);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C596,	quirk_isa_dma_hangs);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82371SB_0,  quirk_isa_dma_hangs);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AL,	PCI_DEVICE_ID_AL_M1533,		quirk_isa_dma_hangs);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_NEC,	PCI_DEVICE_ID_NEC_CBUS_1,	quirk_isa_dma_hangs);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_NEC,	PCI_DEVICE_ID_NEC_CBUS_2,	quirk_isa_dma_hangs);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_NEC,	PCI_DEVICE_ID_NEC_CBUS_3,	quirk_isa_dma_hangs);
#endif

#ifdef CONFIG_HAS_IOPORT
 
static void quirk_tigerpoint_bm_sts(struct pci_dev *dev)
{
	u32 pmbase;
	u16 pm1a;

	pci_read_config_dword(dev, 0x40, &pmbase);
	pmbase = pmbase & 0xff80;
	pm1a = inw(pmbase);

	if (pm1a & 0x10) {
		pci_info(dev, FW_BUG "Tiger Point LPC.BM_STS cleared\n");
		outw(0x10, pmbase);
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_TGP_LPC, quirk_tigerpoint_bm_sts);
#endif

 
static void quirk_nopcipci(struct pci_dev *dev)
{
	if ((pci_pci_problems & PCIPCI_FAIL) == 0) {
		pci_info(dev, "Disabling direct PCI/PCI transfers\n");
		pci_pci_problems |= PCIPCI_FAIL;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_5597,		quirk_nopcipci);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_496,		quirk_nopcipci);

static void quirk_nopciamd(struct pci_dev *dev)
{
	u8 rev;
	pci_read_config_byte(dev, 0x08, &rev);
	if (rev == 0x13) {
		 
		pci_info(dev, "Chipset erratum: Disabling direct PCI/AGP transfers\n");
		pci_pci_problems |= PCIAGP_FAIL;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD,	PCI_DEVICE_ID_AMD_8151_0,	quirk_nopciamd);

 
static void quirk_triton(struct pci_dev *dev)
{
	if ((pci_pci_problems&PCIPCI_TRITON) == 0) {
		pci_info(dev, "Limiting direct PCI/PCI transfers\n");
		pci_pci_problems |= PCIPCI_TRITON;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82437,	quirk_triton);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82437VX,	quirk_triton);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82439,	quirk_triton);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82439TX,	quirk_triton);

 
static void quirk_vialatency(struct pci_dev *dev)
{
	struct pci_dev *p;
	u8 busarb;

	 
	p = pci_get_device(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C686, NULL);
	if (p != NULL) {

		 
		if (p->revision < 0x40 || p->revision > 0x42)
			goto exit;
	} else {
		p = pci_get_device(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_8231, NULL);
		if (p == NULL)	 
			goto exit;

		 
		if (p->revision < 0x10 || p->revision > 0x12)
			goto exit;
	}

	 
	pci_read_config_byte(dev, 0x76, &busarb);

	 
	busarb &= ~(1<<5);
	busarb |= (1<<4);
	pci_write_config_byte(dev, 0x76, busarb);
	pci_info(dev, "Applying VIA southbridge workaround\n");
exit:
	pci_dev_put(p);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8363_0,	quirk_vialatency);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8371_1,	quirk_vialatency);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8361,		quirk_vialatency);
 
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8363_0,	quirk_vialatency);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8371_1,	quirk_vialatency);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8361,		quirk_vialatency);

 
static void quirk_viaetbf(struct pci_dev *dev)
{
	if ((pci_pci_problems&PCIPCI_VIAETBF) == 0) {
		pci_info(dev, "Limiting direct PCI/PCI transfers\n");
		pci_pci_problems |= PCIPCI_VIAETBF;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C597_0,	quirk_viaetbf);

static void quirk_vsfx(struct pci_dev *dev)
{
	if ((pci_pci_problems&PCIPCI_VSFX) == 0) {
		pci_info(dev, "Limiting direct PCI/PCI transfers\n");
		pci_pci_problems |= PCIPCI_VSFX;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C576,	quirk_vsfx);

 
static void quirk_alimagik(struct pci_dev *dev)
{
	if ((pci_pci_problems&PCIPCI_ALIMAGIK) == 0) {
		pci_info(dev, "Limiting direct PCI/PCI transfers\n");
		pci_pci_problems |= PCIPCI_ALIMAGIK|PCIPCI_TRITON;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AL,	PCI_DEVICE_ID_AL_M1647,		quirk_alimagik);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AL,	PCI_DEVICE_ID_AL_M1651,		quirk_alimagik);

 
static void quirk_natoma(struct pci_dev *dev)
{
	if ((pci_pci_problems&PCIPCI_NATOMA) == 0) {
		pci_info(dev, "Limiting direct PCI/PCI transfers\n");
		pci_pci_problems |= PCIPCI_NATOMA;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82441,	quirk_natoma);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82443LX_0,	quirk_natoma);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82443LX_1,	quirk_natoma);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82443BX_0,	quirk_natoma);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82443BX_1,	quirk_natoma);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82443BX_2,	quirk_natoma);

 
static void quirk_citrine(struct pci_dev *dev)
{
	dev->cfg_size = 0xA0;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_IBM,	PCI_DEVICE_ID_IBM_CITRINE,	quirk_citrine);

 
static void quirk_nfp6000(struct pci_dev *dev)
{
	dev->cfg_size = 0x600;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_NETRONOME,	PCI_DEVICE_ID_NETRONOME_NFP4000,	quirk_nfp6000);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_NETRONOME,	PCI_DEVICE_ID_NETRONOME_NFP6000,	quirk_nfp6000);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_NETRONOME,	PCI_DEVICE_ID_NETRONOME_NFP5000,	quirk_nfp6000);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_NETRONOME,	PCI_DEVICE_ID_NETRONOME_NFP6000_VF,	quirk_nfp6000);

 
static void quirk_extend_bar_to_page(struct pci_dev *dev)
{
	int i;

	for (i = 0; i < PCI_STD_NUM_BARS; i++) {
		struct resource *r = &dev->resource[i];

		if (r->flags & IORESOURCE_MEM && resource_size(r) < PAGE_SIZE) {
			r->end = PAGE_SIZE - 1;
			r->start = 0;
			r->flags |= IORESOURCE_UNSET;
			pci_info(dev, "expanded BAR %d to page size: %pR\n",
				 i, r);
		}
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_IBM, 0x034a, quirk_extend_bar_to_page);

 
static void quirk_s3_64M(struct pci_dev *dev)
{
	struct resource *r = &dev->resource[0];

	if ((r->start & 0x3ffffff) || r->end != r->start + 0x3ffffff) {
		r->flags |= IORESOURCE_UNSET;
		r->start = 0;
		r->end = 0x3ffffff;
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_S3,	PCI_DEVICE_ID_S3_868,		quirk_s3_64M);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_S3,	PCI_DEVICE_ID_S3_968,		quirk_s3_64M);

static void quirk_io(struct pci_dev *dev, int pos, unsigned int size,
		     const char *name)
{
	u32 region;
	struct pci_bus_region bus_region;
	struct resource *res = dev->resource + pos;

	pci_read_config_dword(dev, PCI_BASE_ADDRESS_0 + (pos << 2), &region);

	if (!region)
		return;

	res->name = pci_name(dev);
	res->flags = region & ~PCI_BASE_ADDRESS_IO_MASK;
	res->flags |=
		(IORESOURCE_IO | IORESOURCE_PCI_FIXED | IORESOURCE_SIZEALIGN);
	region &= ~(size - 1);

	 
	bus_region.start = region;
	bus_region.end = region + size - 1;
	pcibios_bus_to_resource(dev->bus, res, &bus_region);

	pci_info(dev, FW_BUG "%s quirk: reg 0x%x: %pR\n",
		 name, PCI_BASE_ADDRESS_0 + (pos << 2), res);
}

 
static void quirk_cs5536_vsa(struct pci_dev *dev)
{
	static char *name = "CS5536 ISA bridge";

	if (pci_resource_len(dev, 0) != 8) {
		quirk_io(dev, 0,   8, name);	 
		quirk_io(dev, 1, 256, name);	 
		quirk_io(dev, 2,  64, name);	 
		pci_info(dev, "%s bug detected (incorrect header); workaround applied\n",
			 name);
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_CS5536_ISA, quirk_cs5536_vsa);

static void quirk_io_region(struct pci_dev *dev, int port,
			    unsigned int size, int nr, const char *name)
{
	u16 region;
	struct pci_bus_region bus_region;
	struct resource *res = dev->resource + nr;

	pci_read_config_word(dev, port, &region);
	region &= ~(size - 1);

	if (!region)
		return;

	res->name = pci_name(dev);
	res->flags = IORESOURCE_IO;

	 
	bus_region.start = region;
	bus_region.end = region + size - 1;
	pcibios_bus_to_resource(dev->bus, res, &bus_region);

	if (!pci_claim_resource(dev, nr))
		pci_info(dev, "quirk: %pR claimed by %s\n", res, name);
}

 
static void quirk_ati_exploding_mce(struct pci_dev *dev)
{
	pci_info(dev, "ATI Northbridge, reserving I/O ports 0x3b0 to 0x3bb\n");
	 
	request_region(0x3b0, 0x0C, "RadeonIGP");
	request_region(0x3d3, 0x01, "RadeonIGP");
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI,	PCI_DEVICE_ID_ATI_RS100,   quirk_ati_exploding_mce);

 
static void quirk_amd_dwc_class(struct pci_dev *pdev)
{
	u32 class = pdev->class;

	 
	pdev->class = PCI_CLASS_SERIAL_USB_DEVICE;
	pci_info(pdev, "PCI class overridden (%#08x -> %#08x) so dwc3 driver can claim this instead of xhci\n",
		 class, pdev->class);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_NL_USB,
		quirk_amd_dwc_class);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_VANGOGH_USB,
		quirk_amd_dwc_class);

 
static void quirk_synopsys_haps(struct pci_dev *pdev)
{
	u32 class = pdev->class;

	switch (pdev->device) {
	case PCI_DEVICE_ID_SYNOPSYS_HAPSUSB3:
	case PCI_DEVICE_ID_SYNOPSYS_HAPSUSB3_AXI:
	case PCI_DEVICE_ID_SYNOPSYS_HAPSUSB31:
		pdev->class = PCI_CLASS_SERIAL_USB_DEVICE;
		pci_info(pdev, "PCI class overridden (%#08x -> %#08x) so dwc3 driver can claim this instead of xhci\n",
			 class, pdev->class);
		break;
	}
}
DECLARE_PCI_FIXUP_CLASS_HEADER(PCI_VENDOR_ID_SYNOPSYS, PCI_ANY_ID,
			       PCI_CLASS_SERIAL_USB_XHCI, 0,
			       quirk_synopsys_haps);

 
static void quirk_ali7101_acpi(struct pci_dev *dev)
{
	quirk_io_region(dev, 0xE0, 64, PCI_BRIDGE_RESOURCES, "ali7101 ACPI");
	quirk_io_region(dev, 0xE2, 32, PCI_BRIDGE_RESOURCES+1, "ali7101 SMB");
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AL,	PCI_DEVICE_ID_AL_M7101,		quirk_ali7101_acpi);

static void piix4_io_quirk(struct pci_dev *dev, const char *name, unsigned int port, unsigned int enable)
{
	u32 devres;
	u32 mask, size, base;

	pci_read_config_dword(dev, port, &devres);
	if ((devres & enable) != enable)
		return;
	mask = (devres >> 16) & 15;
	base = devres & 0xffff;
	size = 16;
	for (;;) {
		unsigned int bit = size >> 1;
		if ((bit & mask) == bit)
			break;
		size = bit;
	}
	 
	base &= -size;
	pci_info(dev, "%s PIO at %04x-%04x\n", name, base, base + size - 1);
}

static void piix4_mem_quirk(struct pci_dev *dev, const char *name, unsigned int port, unsigned int enable)
{
	u32 devres;
	u32 mask, size, base;

	pci_read_config_dword(dev, port, &devres);
	if ((devres & enable) != enable)
		return;
	base = devres & 0xffff0000;
	mask = (devres & 0x3f) << 16;
	size = 128 << 16;
	for (;;) {
		unsigned int bit = size >> 1;
		if ((bit & mask) == bit)
			break;
		size = bit;
	}

	 
	base &= -size;
	pci_info(dev, "%s MMIO at %04x-%04x\n", name, base, base + size - 1);
}

 
static void quirk_piix4_acpi(struct pci_dev *dev)
{
	u32 res_a;

	quirk_io_region(dev, 0x40, 64, PCI_BRIDGE_RESOURCES, "PIIX4 ACPI");
	quirk_io_region(dev, 0x90, 16, PCI_BRIDGE_RESOURCES+1, "PIIX4 SMB");

	 
	pci_read_config_dword(dev, 0x5c, &res_a);

	piix4_io_quirk(dev, "PIIX4 devres B", 0x60, 3 << 21);
	piix4_io_quirk(dev, "PIIX4 devres C", 0x64, 3 << 21);

	 

	 
	if (res_a & (1 << 29)) {
		piix4_io_quirk(dev, "PIIX4 devres E", 0x68, 1 << 20);
		piix4_mem_quirk(dev, "PIIX4 devres F", 0x6c, 1 << 7);
	}
	 
	if (res_a & (1 << 30)) {
		piix4_io_quirk(dev, "PIIX4 devres G", 0x70, 1 << 20);
		piix4_mem_quirk(dev, "PIIX4 devres H", 0x74, 1 << 7);
	}
	piix4_io_quirk(dev, "PIIX4 devres I", 0x78, 1 << 20);
	piix4_io_quirk(dev, "PIIX4 devres J", 0x7c, 1 << 20);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82371AB_3,	quirk_piix4_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82443MX_3,	quirk_piix4_acpi);

#define ICH_PMBASE	0x40
#define ICH_ACPI_CNTL	0x44
#define  ICH4_ACPI_EN	0x10
#define  ICH6_ACPI_EN	0x80
#define ICH4_GPIOBASE	0x58
#define ICH4_GPIO_CNTL	0x5c
#define  ICH4_GPIO_EN	0x10
#define ICH6_GPIOBASE	0x48
#define ICH6_GPIO_CNTL	0x4c
#define  ICH6_GPIO_EN	0x10

 
static void quirk_ich4_lpc_acpi(struct pci_dev *dev)
{
	u8 enable;

	 
	pci_read_config_byte(dev, ICH_ACPI_CNTL, &enable);
	if (enable & ICH4_ACPI_EN)
		quirk_io_region(dev, ICH_PMBASE, 128, PCI_BRIDGE_RESOURCES,
				 "ICH4 ACPI/GPIO/TCO");

	pci_read_config_byte(dev, ICH4_GPIO_CNTL, &enable);
	if (enable & ICH4_GPIO_EN)
		quirk_io_region(dev, ICH4_GPIOBASE, 64, PCI_BRIDGE_RESOURCES+1,
				"ICH4 GPIO");
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82801AA_0,		quirk_ich4_lpc_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82801AB_0,		quirk_ich4_lpc_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82801BA_0,		quirk_ich4_lpc_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82801BA_10,	quirk_ich4_lpc_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82801CA_0,		quirk_ich4_lpc_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82801CA_12,	quirk_ich4_lpc_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82801DB_0,		quirk_ich4_lpc_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82801DB_12,	quirk_ich4_lpc_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82801EB_0,		quirk_ich4_lpc_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_ESB_1,		quirk_ich4_lpc_acpi);

static void ich6_lpc_acpi_gpio(struct pci_dev *dev)
{
	u8 enable;

	pci_read_config_byte(dev, ICH_ACPI_CNTL, &enable);
	if (enable & ICH6_ACPI_EN)
		quirk_io_region(dev, ICH_PMBASE, 128, PCI_BRIDGE_RESOURCES,
				 "ICH6 ACPI/GPIO/TCO");

	pci_read_config_byte(dev, ICH6_GPIO_CNTL, &enable);
	if (enable & ICH6_GPIO_EN)
		quirk_io_region(dev, ICH6_GPIOBASE, 64, PCI_BRIDGE_RESOURCES+1,
				"ICH6 GPIO");
}

static void ich6_lpc_generic_decode(struct pci_dev *dev, unsigned int reg,
				    const char *name, int dynsize)
{
	u32 val;
	u32 size, base;

	pci_read_config_dword(dev, reg, &val);

	 
	if (!(val & 1))
		return;
	base = val & 0xfffc;
	if (dynsize) {
		 
		size = 16;
	} else {
		size = 128;
	}
	base &= ~(size-1);

	 
	pci_info(dev, "%s PIO at %04x-%04x\n", name, base, base+size-1);
}

static void quirk_ich6_lpc(struct pci_dev *dev)
{
	 
	ich6_lpc_acpi_gpio(dev);

	 
	ich6_lpc_generic_decode(dev, 0x84, "LPC Generic IO decode 1", 0);
	ich6_lpc_generic_decode(dev, 0x88, "LPC Generic IO decode 2", 1);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH6_0, quirk_ich6_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH6_1, quirk_ich6_lpc);

static void ich7_lpc_generic_decode(struct pci_dev *dev, unsigned int reg,
				    const char *name)
{
	u32 val;
	u32 mask, base;

	pci_read_config_dword(dev, reg, &val);

	 
	if (!(val & 1))
		return;

	 
	base = val & 0xfffc;
	mask = (val >> 16) & 0xfc;
	mask |= 3;

	 
	pci_info(dev, "%s PIO at %04x (mask %04x)\n", name, base, mask);
}

 
static void quirk_ich7_lpc(struct pci_dev *dev)
{
	 
	ich6_lpc_acpi_gpio(dev);

	 
	ich7_lpc_generic_decode(dev, 0x84, "ICH7 LPC Generic IO decode 1");
	ich7_lpc_generic_decode(dev, 0x88, "ICH7 LPC Generic IO decode 2");
	ich7_lpc_generic_decode(dev, 0x8c, "ICH7 LPC Generic IO decode 3");
	ich7_lpc_generic_decode(dev, 0x90, "ICH7 LPC Generic IO decode 4");
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH7_0, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH7_1, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH7_31, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH8_0, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH8_2, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH8_3, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH8_1, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH8_4, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH9_2, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH9_4, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH9_7, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH9_8, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,   PCI_DEVICE_ID_INTEL_ICH10_1, quirk_ich7_lpc);

 
static void quirk_vt82c586_acpi(struct pci_dev *dev)
{
	if (dev->revision & 0x10)
		quirk_io_region(dev, 0x48, 256, PCI_BRIDGE_RESOURCES,
				"vt82c586 ACPI");
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C586_3,	quirk_vt82c586_acpi);

 
static void quirk_vt82c686_acpi(struct pci_dev *dev)
{
	quirk_vt82c586_acpi(dev);

	quirk_io_region(dev, 0x70, 128, PCI_BRIDGE_RESOURCES+1,
				 "vt82c686 HW-mon");

	quirk_io_region(dev, 0x90, 16, PCI_BRIDGE_RESOURCES+2, "vt82c686 SMB");
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C686_4,	quirk_vt82c686_acpi);

 
static void quirk_vt8235_acpi(struct pci_dev *dev)
{
	quirk_io_region(dev, 0x88, 128, PCI_BRIDGE_RESOURCES, "vt8235 PM");
	quirk_io_region(dev, 0xd0, 16, PCI_BRIDGE_RESOURCES+1, "vt8235 SMB");
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8235,	quirk_vt8235_acpi);

 
static void quirk_xio2000a(struct pci_dev *dev)
{
	struct pci_dev *pdev;
	u16 command;

	pci_warn(dev, "TI XIO2000a quirk detected; secondary bus fast back-to-back transfers disabled\n");
	list_for_each_entry(pdev, &dev->subordinate->devices, bus_list) {
		pci_read_config_word(pdev, PCI_COMMAND, &command);
		if (command & PCI_COMMAND_FAST_BACK)
			pci_write_config_word(pdev, PCI_COMMAND, command & ~PCI_COMMAND_FAST_BACK);
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_TI, PCI_DEVICE_ID_TI_XIO2000A,
			quirk_xio2000a);

#ifdef CONFIG_X86_IO_APIC

#include <asm/io_apic.h>

 
static void quirk_via_ioapic(struct pci_dev *dev)
{
	u8 tmp;

	if (nr_ioapics < 1)
		tmp = 0;     
	else
		tmp = 0x1f;  

	pci_info(dev, "%s VIA external APIC routing\n",
		 tmp ? "Enabling" : "Disabling");

	 
	pci_write_config_byte(dev, 0x58, tmp);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C686,	quirk_via_ioapic);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C686,	quirk_via_ioapic);

 
static void quirk_via_vt8237_bypass_apic_deassert(struct pci_dev *dev)
{
	u8 misc_control2;
#define BYPASS_APIC_DEASSERT 8

	pci_read_config_byte(dev, 0x5B, &misc_control2);
	if (!(misc_control2 & BYPASS_APIC_DEASSERT)) {
		pci_info(dev, "Bypassing VIA 8237 APIC De-Assert Message\n");
		pci_write_config_byte(dev, 0x5B, misc_control2|BYPASS_APIC_DEASSERT);
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8237,		quirk_via_vt8237_bypass_apic_deassert);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8237,		quirk_via_vt8237_bypass_apic_deassert);

 
static void quirk_amd_ioapic(struct pci_dev *dev)
{
	if (dev->revision >= 0x02) {
		pci_warn(dev, "I/O APIC: AMD Erratum #22 may be present. In the event of instability try\n");
		pci_warn(dev, "        : booting with the \"noapic\" option\n");
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD,	PCI_DEVICE_ID_AMD_VIPER_7410,	quirk_amd_ioapic);
#endif  

#if defined(CONFIG_ARM64) && defined(CONFIG_PCI_ATS)

static void quirk_cavium_sriov_rnm_link(struct pci_dev *dev)
{
	 
	if (dev->subsystem_device == 0xa118)
		dev->sriov->link = dev->devfn;
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_CAVIUM, 0xa018, quirk_cavium_sriov_rnm_link);
#endif

 
static void quirk_amd_8131_mmrbc(struct pci_dev *dev)
{
	if (dev->subordinate && dev->revision <= 0x12) {
		pci_info(dev, "AMD8131 rev %x detected; disabling PCI-X MMRBC\n",
			 dev->revision);
		dev->subordinate->bus_flags |= PCI_BUS_FLAGS_NO_MMRBC;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_8131_BRIDGE, quirk_amd_8131_mmrbc);

 
static void quirk_via_acpi(struct pci_dev *d)
{
	u8 irq;

	 
	pci_read_config_byte(d, 0x42, &irq);
	irq &= 0xf;
	if (irq && (irq != 2))
		d->irq = irq;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C586_3,	quirk_via_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C686_4,	quirk_via_acpi);

 
static int via_vlink_dev_lo = -1, via_vlink_dev_hi = 18;

static void quirk_via_bridge(struct pci_dev *dev)
{
	 
	switch (dev->device) {
	case PCI_DEVICE_ID_VIA_82C686:
		 
		via_vlink_dev_lo = PCI_SLOT(dev->devfn);
		via_vlink_dev_hi = PCI_SLOT(dev->devfn);
		break;
	case PCI_DEVICE_ID_VIA_8237:
	case PCI_DEVICE_ID_VIA_8237A:
		via_vlink_dev_lo = 15;
		break;
	case PCI_DEVICE_ID_VIA_8235:
		via_vlink_dev_lo = 16;
		break;
	case PCI_DEVICE_ID_VIA_8231:
	case PCI_DEVICE_ID_VIA_8233_0:
	case PCI_DEVICE_ID_VIA_8233A:
	case PCI_DEVICE_ID_VIA_8233C_0:
		via_vlink_dev_lo = 17;
		break;
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C686,	quirk_via_bridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8231,		quirk_via_bridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8233_0,	quirk_via_bridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8233A,	quirk_via_bridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8233C_0,	quirk_via_bridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8235,		quirk_via_bridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8237,		quirk_via_bridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8237A,	quirk_via_bridge);

 
static void quirk_via_vlink(struct pci_dev *dev)
{
	u8 irq, new_irq;

	 
	if (via_vlink_dev_lo == -1)
		return;

	new_irq = dev->irq;

	 
	if (!new_irq || new_irq > 15)
		return;

	 
	if (dev->bus->number != 0 || PCI_SLOT(dev->devfn) > via_vlink_dev_hi ||
	    PCI_SLOT(dev->devfn) < via_vlink_dev_lo)
		return;

	 
	pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq);
	if (new_irq != irq) {
		pci_info(dev, "VIA VLink IRQ fixup, from %d to %d\n",
			irq, new_irq);
		udelay(15);	 
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, new_irq);
	}
}
DECLARE_PCI_FIXUP_ENABLE(PCI_VENDOR_ID_VIA, PCI_ANY_ID, quirk_via_vlink);

 
static void quirk_vt82c598_id(struct pci_dev *dev)
{
	pci_write_config_byte(dev, 0xfc, 0);
	pci_read_config_word(dev, PCI_DEVICE_ID, &dev->device);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C597_0,	quirk_vt82c598_id);

 
static void quirk_cardbus_legacy(struct pci_dev *dev)
{
	pci_write_config_dword(dev, PCI_CB_LEGACY_MODE_BASE, 0);
}
DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_ANY_ID, PCI_ANY_ID,
			PCI_CLASS_BRIDGE_CARDBUS, 8, quirk_cardbus_legacy);
DECLARE_PCI_FIXUP_CLASS_RESUME_EARLY(PCI_ANY_ID, PCI_ANY_ID,
			PCI_CLASS_BRIDGE_CARDBUS, 8, quirk_cardbus_legacy);

 
static void quirk_amd_ordering(struct pci_dev *dev)
{
	u32 pcic;
	pci_read_config_dword(dev, 0x4C, &pcic);
	if ((pcic & 6) != 6) {
		pcic |= 6;
		pci_warn(dev, "BIOS failed to enable PCI standards compliance; fixing this error\n");
		pci_write_config_dword(dev, 0x4C, pcic);
		pci_read_config_dword(dev, 0x84, &pcic);
		pcic |= (1 << 23);	 
		pci_write_config_dword(dev, 0x84, pcic);
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD,	PCI_DEVICE_ID_AMD_FE_GATE_700C, quirk_amd_ordering);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_AMD,	PCI_DEVICE_ID_AMD_FE_GATE_700C, quirk_amd_ordering);

 
static void quirk_dunord(struct pci_dev *dev)
{
	struct resource *r = &dev->resource[1];

	r->flags |= IORESOURCE_UNSET;
	r->start = 0;
	r->end = 0xffffff;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_DUNORD,	PCI_DEVICE_ID_DUNORD_I3000,	quirk_dunord);

 
static void quirk_transparent_bridge(struct pci_dev *dev)
{
	dev->transparent = 1;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82380FB,	quirk_transparent_bridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_TOSHIBA,	0x605,	quirk_transparent_bridge);

 
static void quirk_mediagx_master(struct pci_dev *dev)
{
	u8 reg;

	pci_read_config_byte(dev, 0x41, &reg);
	if (reg & 2) {
		reg &= ~2;
		pci_info(dev, "Fixup for MediaGX/Geode Slave Disconnect Boundary (0x41=0x%02x)\n",
			 reg);
		pci_write_config_byte(dev, 0x41, reg);
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_CYRIX,	PCI_DEVICE_ID_CYRIX_PCI_MASTER, quirk_mediagx_master);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_CYRIX,	PCI_DEVICE_ID_CYRIX_PCI_MASTER, quirk_mediagx_master);

 
static void quirk_disable_pxb(struct pci_dev *pdev)
{
	u16 config;

	if (pdev->revision != 0x04)		 
		return;
	pci_read_config_word(pdev, 0x40, &config);
	if (config & (1<<6)) {
		config &= ~(1<<6);
		pci_write_config_word(pdev, 0x40, config);
		pci_info(pdev, "C0 revision 450NX. Disabling PCI restreaming\n");
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82454NX,	quirk_disable_pxb);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82454NX,	quirk_disable_pxb);

static void quirk_amd_ide_mode(struct pci_dev *pdev)
{
	 
	u8 tmp;

	pci_read_config_byte(pdev, PCI_CLASS_DEVICE, &tmp);
	if (tmp == 0x01) {
		pci_read_config_byte(pdev, 0x40, &tmp);
		pci_write_config_byte(pdev, 0x40, tmp|1);
		pci_write_config_byte(pdev, 0x9, 1);
		pci_write_config_byte(pdev, 0xa, 6);
		pci_write_config_byte(pdev, 0x40, tmp);

		pdev->class = PCI_CLASS_STORAGE_SATA_AHCI;
		pci_info(pdev, "set SATA to AHCI mode\n");
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_IXP600_SATA, quirk_amd_ide_mode);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_IXP600_SATA, quirk_amd_ide_mode);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_IXP700_SATA, quirk_amd_ide_mode);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_IXP700_SATA, quirk_amd_ide_mode);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_HUDSON2_SATA_IDE, quirk_amd_ide_mode);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_HUDSON2_SATA_IDE, quirk_amd_ide_mode);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, 0x7900, quirk_amd_ide_mode);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_AMD, 0x7900, quirk_amd_ide_mode);

 
static void quirk_svwks_csb5ide(struct pci_dev *pdev)
{
	u8 prog;
	pci_read_config_byte(pdev, PCI_CLASS_PROG, &prog);
	if (prog & 5) {
		prog &= ~5;
		pdev->class &= ~5;
		pci_write_config_byte(pdev, PCI_CLASS_PROG, prog);
		 
	}
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_SERVERWORKS, PCI_DEVICE_ID_SERVERWORKS_CSB5IDE, quirk_svwks_csb5ide);

 
static void quirk_ide_samemode(struct pci_dev *pdev)
{
	u8 prog;

	pci_read_config_byte(pdev, PCI_CLASS_PROG, &prog);

	if (((prog & 1) && !(prog & 4)) || ((prog & 4) && !(prog & 1))) {
		pci_info(pdev, "IDE mode mismatch; forcing legacy mode\n");
		prog &= ~5;
		pdev->class &= ~5;
		pci_write_config_byte(pdev, PCI_CLASS_PROG, prog);
	}
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82801CA_10, quirk_ide_samemode);

 
static void quirk_no_ata_d3(struct pci_dev *pdev)
{
	pdev->dev_flags |= PCI_DEV_FLAGS_NO_D3;
}
 
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_SERVERWORKS, PCI_ANY_ID,
				PCI_CLASS_STORAGE_IDE, 8, quirk_no_ata_d3);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_ATI, PCI_ANY_ID,
				PCI_CLASS_STORAGE_IDE, 8, quirk_no_ata_d3);
 
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_AL, PCI_ANY_ID,
				PCI_CLASS_STORAGE_IDE, 8, quirk_no_ata_d3);
 
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_VIA, PCI_ANY_ID,
				PCI_CLASS_STORAGE_IDE, 8, quirk_no_ata_d3);

 
static void quirk_eisa_bridge(struct pci_dev *dev)
{
	dev->class = PCI_CLASS_BRIDGE_EISA << 8;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82375,	quirk_eisa_bridge);

 
static int asus_hides_smbus;

static void asus_hides_smbus_hostbridge(struct pci_dev *dev)
{
	if (unlikely(dev->subsystem_vendor == PCI_VENDOR_ID_ASUSTEK)) {
		if (dev->device == PCI_DEVICE_ID_INTEL_82845_HB)
			switch (dev->subsystem_device) {
			case 0x8025:  
			case 0x8070:  
			case 0x8088:  
			case 0x1626:  
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82845G_HB)
			switch (dev->subsystem_device) {
			case 0x80b1:  
			case 0x80b2:  
			case 0x8093:  
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82850_HB)
			switch (dev->subsystem_device) {
			case 0x8030:  
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_7205_0)
			switch (dev->subsystem_device) {
			case 0x8070:  
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_E7501_MCH)
			switch (dev->subsystem_device) {
			case 0x80c9:  
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82855GM_HB)
			switch (dev->subsystem_device) {
			case 0x1751:  
			case 0x1821:  
			case 0x1897:  
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82855PM_HB)
			switch (dev->subsystem_device) {
			case 0x184b:  
			case 0x186a:  
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82865_HB)
			switch (dev->subsystem_device) {
			case 0x80f2:  
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82915GM_HB)
			switch (dev->subsystem_device) {
			case 0x1882:  
			case 0x1977:  
				asus_hides_smbus = 1;
			}
	} else if (unlikely(dev->subsystem_vendor == PCI_VENDOR_ID_HP)) {
		if (dev->device ==  PCI_DEVICE_ID_INTEL_82855PM_HB)
			switch (dev->subsystem_device) {
			case 0x088C:  
			case 0x0890:  
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82865_HB)
			switch (dev->subsystem_device) {
			case 0x12bc:  
			case 0x12bd:  
			case 0x006a:  
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82875_HB)
			switch (dev->subsystem_device) {
			case 0x12bf:  
				asus_hides_smbus = 1;
			}
	} else if (unlikely(dev->subsystem_vendor == PCI_VENDOR_ID_SAMSUNG)) {
		if (dev->device ==  PCI_DEVICE_ID_INTEL_82855PM_HB)
			switch (dev->subsystem_device) {
			case 0xC00C:  
				asus_hides_smbus = 1;
		}
	} else if (unlikely(dev->subsystem_vendor == PCI_VENDOR_ID_COMPAQ)) {
		if (dev->device == PCI_DEVICE_ID_INTEL_82855PM_HB)
			switch (dev->subsystem_device) {
			case 0x0058:  
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82810_IG3)
			switch (dev->subsystem_device) {
			case 0xB16C:  
				 
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82801DB_2)
			switch (dev->subsystem_device) {
			case 0x00b8:  
			case 0x00b9:  
			case 0x00ba:  
				 
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82815_CGC)
			switch (dev->subsystem_device) {
			case 0x001A:  
				 
				asus_hides_smbus = 1;
			}
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82845_HB,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82845G_HB,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82850_HB,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82865_HB,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82875_HB,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_7205_0,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_E7501_MCH,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82855PM_HB,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82855GM_HB,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82915GM_HB, asus_hides_smbus_hostbridge);

DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82810_IG3,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801DB_2,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82815_CGC,	asus_hides_smbus_hostbridge);

static void asus_hides_smbus_lpc(struct pci_dev *dev)
{
	u16 val;

	if (likely(!asus_hides_smbus))
		return;

	pci_read_config_word(dev, 0xF2, &val);
	if (val & 0x8) {
		pci_write_config_word(dev, 0xF2, val & (~0x8));
		pci_read_config_word(dev, 0xF2, &val);
		if (val & 0x8)
			pci_info(dev, "i801 SMBus device continues to play 'hide and seek'! 0x%x\n",
				 val);
		else
			pci_info(dev, "Enabled i801 SMBus device\n");
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801AA_0,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801DB_0,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801BA_0,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801CA_0,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801CA_12,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801DB_12,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801EB_0,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801AA_0,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801DB_0,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801BA_0,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801CA_0,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801CA_12,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801DB_12,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801EB_0,	asus_hides_smbus_lpc);

 
static void __iomem *asus_rcba_base;
static void asus_hides_smbus_lpc_ich6_suspend(struct pci_dev *dev)
{
	u32 rcba;

	if (likely(!asus_hides_smbus))
		return;
	WARN_ON(asus_rcba_base);

	pci_read_config_dword(dev, 0xF0, &rcba);
	 
	asus_rcba_base = ioremap(rcba & 0xFFFFC000, 0x4000);
	if (asus_rcba_base == NULL)
		return;
}

static void asus_hides_smbus_lpc_ich6_resume_early(struct pci_dev *dev)
{
	u32 val;

	if (likely(!asus_hides_smbus || !asus_rcba_base))
		return;

	 
	val = readl(asus_rcba_base + 0x3418);

	 
	writel(val & 0xFFFFFFF7, asus_rcba_base + 0x3418);
}

static void asus_hides_smbus_lpc_ich6_resume(struct pci_dev *dev)
{
	if (likely(!asus_hides_smbus || !asus_rcba_base))
		return;

	iounmap(asus_rcba_base);
	asus_rcba_base = NULL;
	pci_info(dev, "Enabled ICH6/i801 SMBus device\n");
}

static void asus_hides_smbus_lpc_ich6(struct pci_dev *dev)
{
	asus_hides_smbus_lpc_ich6_suspend(dev);
	asus_hides_smbus_lpc_ich6_resume_early(dev);
	asus_hides_smbus_lpc_ich6_resume(dev);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH6_1,	asus_hides_smbus_lpc_ich6);
DECLARE_PCI_FIXUP_SUSPEND(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH6_1,	asus_hides_smbus_lpc_ich6_suspend);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH6_1,	asus_hides_smbus_lpc_ich6_resume);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH6_1,	asus_hides_smbus_lpc_ich6_resume_early);

 
static void quirk_sis_96x_smbus(struct pci_dev *dev)
{
	u8 val = 0;
	pci_read_config_byte(dev, 0x77, &val);
	if (val & 0x10) {
		pci_info(dev, "Enabling SiS 96x SMBus\n");
		pci_write_config_byte(dev, 0x77, val & ~0x10);
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_961,		quirk_sis_96x_smbus);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_962,		quirk_sis_96x_smbus);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_963,		quirk_sis_96x_smbus);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_LPC,		quirk_sis_96x_smbus);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_961,		quirk_sis_96x_smbus);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_962,		quirk_sis_96x_smbus);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_963,		quirk_sis_96x_smbus);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_LPC,		quirk_sis_96x_smbus);

 
#define SIS_DETECT_REGISTER 0x40

static void quirk_sis_503(struct pci_dev *dev)
{
	u8 reg;
	u16 devid;

	pci_read_config_byte(dev, SIS_DETECT_REGISTER, &reg);
	pci_write_config_byte(dev, SIS_DETECT_REGISTER, reg | (1 << 6));
	pci_read_config_word(dev, PCI_DEVICE_ID, &devid);
	if (((devid & 0xfff0) != 0x0960) && (devid != 0x0018)) {
		pci_write_config_byte(dev, SIS_DETECT_REGISTER, reg);
		return;
	}

	 
	dev->device = devid;
	quirk_sis_96x_smbus(dev);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_503,		quirk_sis_503);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_503,		quirk_sis_503);

 
static void asus_hides_ac97_lpc(struct pci_dev *dev)
{
	u8 val;
	int asus_hides_ac97 = 0;

	if (likely(dev->subsystem_vendor == PCI_VENDOR_ID_ASUSTEK)) {
		if (dev->device == PCI_DEVICE_ID_VIA_8237)
			asus_hides_ac97 = 1;
	}

	if (!asus_hides_ac97)
		return;

	pci_read_config_byte(dev, 0x50, &val);
	if (val & 0xc0) {
		pci_write_config_byte(dev, 0x50, val & (~0xc0));
		pci_read_config_byte(dev, 0x50, &val);
		if (val & 0xc0)
			pci_info(dev, "Onboard AC97/MC97 devices continue to play 'hide and seek'! 0x%x\n",
				 val);
		else
			pci_info(dev, "Enabled onboard AC97/MC97 devices\n");
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8237, asus_hides_ac97_lpc);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8237, asus_hides_ac97_lpc);

#if defined(CONFIG_ATA) || defined(CONFIG_ATA_MODULE)

 
static void quirk_jmicron_ata(struct pci_dev *pdev)
{
	u32 conf1, conf5, class;
	u8 hdr;

	 
	if (PCI_FUNC(pdev->devfn))
		return;

	pci_read_config_dword(pdev, 0x40, &conf1);
	pci_read_config_dword(pdev, 0x80, &conf5);

	conf1 &= ~0x00CFF302;  
	conf5 &= ~(1 << 24);   

	switch (pdev->device) {
	case PCI_DEVICE_ID_JMICRON_JMB360:  
	case PCI_DEVICE_ID_JMICRON_JMB362:  
	case PCI_DEVICE_ID_JMICRON_JMB364:  
		 
		conf1 |= 0x0002A100;  
		break;

	case PCI_DEVICE_ID_JMICRON_JMB365:
	case PCI_DEVICE_ID_JMICRON_JMB366:
		 
		conf5 |= (1 << 24);
		fallthrough;
	case PCI_DEVICE_ID_JMICRON_JMB361:
	case PCI_DEVICE_ID_JMICRON_JMB363:
	case PCI_DEVICE_ID_JMICRON_JMB369:
		 
		 
		conf1 |= 0x00C2A1B3;  
		break;

	case PCI_DEVICE_ID_JMICRON_JMB368:
		 
		conf1 |= 0x00C00000;  
		break;
	}

	pci_write_config_dword(pdev, 0x40, conf1);
	pci_write_config_dword(pdev, 0x80, conf5);

	 
	pci_read_config_byte(pdev, PCI_HEADER_TYPE, &hdr);
	pdev->hdr_type = hdr & 0x7f;
	pdev->multifunction = !!(hdr & 0x80);

	pci_read_config_dword(pdev, PCI_CLASS_REVISION, &class);
	pdev->class = class >> 8;
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB360, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB361, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB362, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB363, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB364, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB365, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB366, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB368, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB369, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB360, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB361, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB362, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB363, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB364, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB365, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB366, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB368, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB369, quirk_jmicron_ata);

#endif

static void quirk_jmicron_async_suspend(struct pci_dev *dev)
{
	if (dev->multifunction) {
		device_disable_async_suspend(&dev->dev);
		pci_info(dev, "async suspend disabled to avoid multi-function power-on ordering issue\n");
	}
}
DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_VENDOR_ID_JMICRON, PCI_ANY_ID, PCI_CLASS_STORAGE_IDE, 8, quirk_jmicron_async_suspend);
DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_VENDOR_ID_JMICRON, PCI_ANY_ID, PCI_CLASS_STORAGE_SATA_AHCI, 0, quirk_jmicron_async_suspend);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_JMICRON, 0x2362, quirk_jmicron_async_suspend);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_JMICRON, 0x236f, quirk_jmicron_async_suspend);

#ifdef CONFIG_X86_IO_APIC
static void quirk_alder_ioapic(struct pci_dev *pdev)
{
	int i;

	if ((pdev->class >> 8) != 0xff00)
		return;

	 
	if (pci_resource_start(pdev, 0) && pci_resource_len(pdev, 0))
		insert_resource(&iomem_resource, &pdev->resource[0]);

	 
	for (i = 1; i < PCI_STD_NUM_BARS; i++)
		memset(&pdev->resource[i], 0, sizeof(pdev->resource[i]));
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_EESSC,	quirk_alder_ioapic);
#endif

static void quirk_no_msi(struct pci_dev *dev)
{
	pci_info(dev, "avoiding MSI to work around a hardware defect\n");
	dev->no_msi = 1;
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4386, quirk_no_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4387, quirk_no_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4388, quirk_no_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4389, quirk_no_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x438a, quirk_no_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x438b, quirk_no_msi);

static void quirk_pcie_mch(struct pci_dev *pdev)
{
	pdev->no_msi = 1;
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_E7520_MCH,	quirk_pcie_mch);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_E7320_MCH,	quirk_pcie_mch);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_E7525_MCH,	quirk_pcie_mch);

DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_VENDOR_ID_HUAWEI, 0x1610, PCI_CLASS_BRIDGE_PCI, 8, quirk_pcie_mch);

 
static void quirk_huawei_pcie_sva(struct pci_dev *pdev)
{
	struct property_entry properties[] = {
		PROPERTY_ENTRY_BOOL("dma-can-stall"),
		{},
	};

	if (pdev->revision != 0x21 && pdev->revision != 0x30)
		return;

	pdev->pasid_no_tlp = 1;

	 
	if (!pdev->dev.of_node &&
	    device_create_managed_software_node(&pdev->dev, properties, NULL))
		pci_warn(pdev, "could not add stall property");
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_HUAWEI, 0xa250, quirk_huawei_pcie_sva);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_HUAWEI, 0xa251, quirk_huawei_pcie_sva);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_HUAWEI, 0xa255, quirk_huawei_pcie_sva);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_HUAWEI, 0xa256, quirk_huawei_pcie_sva);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_HUAWEI, 0xa258, quirk_huawei_pcie_sva);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_HUAWEI, 0xa259, quirk_huawei_pcie_sva);

 
static void quirk_pcie_pxh(struct pci_dev *dev)
{
	dev->no_msi = 1;
	pci_warn(dev, "PXH quirk detected; SHPC device MSI disabled\n");
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXHD_0,	quirk_pcie_pxh);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXHD_1,	quirk_pcie_pxh);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXH_0,	quirk_pcie_pxh);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXH_1,	quirk_pcie_pxh);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXHV,	quirk_pcie_pxh);

 
static void quirk_intel_pcie_pm(struct pci_dev *dev)
{
	pci_pm_d3hot_delay = 120;
	dev->no_d1d2 = 1;
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25e2, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25e3, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25e4, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25e5, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25e6, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25e7, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25f7, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25f8, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25f9, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25fa, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2601, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2602, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2603, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2604, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2605, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2606, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2607, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2608, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2609, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x260a, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x260b, quirk_intel_pcie_pm);

static void quirk_d3hot_delay(struct pci_dev *dev, unsigned int delay)
{
	if (dev->d3hot_delay >= delay)
		return;

	dev->d3hot_delay = delay;
	pci_info(dev, "extending delay after power-on from D3hot to %d msec\n",
		 dev->d3hot_delay);
}

static void quirk_radeon_pm(struct pci_dev *dev)
{
	if (dev->subsystem_vendor == PCI_VENDOR_ID_APPLE &&
	    dev->subsystem_device == 0x00e2)
		quirk_d3hot_delay(dev, 20);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x6741, quirk_radeon_pm);

 
static void quirk_nvidia_hda_pm(struct pci_dev *dev)
{
	quirk_d3hot_delay(dev, 20);
}
DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_VENDOR_ID_NVIDIA, PCI_ANY_ID,
			      PCI_CLASS_MULTIMEDIA_HD_AUDIO, 8,
			      quirk_nvidia_hda_pm);

 
static void quirk_ryzen_xhci_d3hot(struct pci_dev *dev)
{
	quirk_d3hot_delay(dev, 20);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD, 0x15e0, quirk_ryzen_xhci_d3hot);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD, 0x15e1, quirk_ryzen_xhci_d3hot);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD, 0x1639, quirk_ryzen_xhci_d3hot);

#ifdef CONFIG_X86_IO_APIC
static int dmi_disable_ioapicreroute(const struct dmi_system_id *d)
{
	noioapicreroute = 1;
	pr_info("%s detected: disable boot interrupt reroute\n", d->ident);

	return 0;
}

static const struct dmi_system_id boot_interrupt_dmi_table[] = {
	 
	{
		.callback = dmi_disable_ioapicreroute,
		.ident = "ASUSTek Computer INC. M2N-LR",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "ASUSTek Computer INC."),
			DMI_MATCH(DMI_PRODUCT_NAME, "M2N-LR"),
		},
	},
	{}
};

 
static void quirk_reroute_to_boot_interrupts_intel(struct pci_dev *dev)
{
	dmi_check_system(boot_interrupt_dmi_table);
	if (noioapicquirk || noioapicreroute)
		return;

	dev->irq_reroute_variant = INTEL_IRQ_REROUTE_VARIANT;
	pci_info(dev, "rerouting interrupts for [%04x:%04x]\n",
		 dev->vendor, dev->device);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_80333_0,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_80333_1,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ESB2_0,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXH_0,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXH_1,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXHV,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_80332_0,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_80332_1,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_80333_0,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_80333_1,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ESB2_0,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXH_0,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXH_1,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXHV,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_80332_0,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_80332_1,	quirk_reroute_to_boot_interrupts_intel);

 

 
#define INTEL_6300_IOAPIC_ABAR		0x40	 
#define INTEL_6300_DISABLE_BOOT_IRQ	(1<<14)

#define INTEL_CIPINTRC_CFG_OFFSET	0x14C	 
#define INTEL_CIPINTRC_DIS_INTX_ICH	(1<<25)

static void quirk_disable_intel_boot_interrupt(struct pci_dev *dev)
{
	u16 pci_config_word;
	u32 pci_config_dword;

	if (noioapicquirk)
		return;

	switch (dev->device) {
	case PCI_DEVICE_ID_INTEL_ESB_10:
		pci_read_config_word(dev, INTEL_6300_IOAPIC_ABAR,
				     &pci_config_word);
		pci_config_word |= INTEL_6300_DISABLE_BOOT_IRQ;
		pci_write_config_word(dev, INTEL_6300_IOAPIC_ABAR,
				      pci_config_word);
		break;
	case 0x3c28:	 
	case 0x0e28:	 
	case 0x2f28:	 
	case 0x6f28:	 
	case 0x2034:	 
		pci_read_config_dword(dev, INTEL_CIPINTRC_CFG_OFFSET,
				      &pci_config_dword);
		pci_config_dword |= INTEL_CIPINTRC_DIS_INTX_ICH;
		pci_write_config_dword(dev, INTEL_CIPINTRC_CFG_OFFSET,
				       pci_config_dword);
		break;
	default:
		return;
	}
	pci_info(dev, "disabled boot interrupts on device [%04x:%04x]\n",
		 dev->vendor, dev->device);
}
 
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ESB_10,
		quirk_disable_intel_boot_interrupt);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ESB_10,
		quirk_disable_intel_boot_interrupt);

 
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x3c28,
		quirk_disable_intel_boot_interrupt);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x0e28,
		quirk_disable_intel_boot_interrupt);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2f28,
		quirk_disable_intel_boot_interrupt);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x6f28,
		quirk_disable_intel_boot_interrupt);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2034,
		quirk_disable_intel_boot_interrupt);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	0x3c28,
		quirk_disable_intel_boot_interrupt);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	0x0e28,
		quirk_disable_intel_boot_interrupt);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	0x2f28,
		quirk_disable_intel_boot_interrupt);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	0x6f28,
		quirk_disable_intel_boot_interrupt);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	0x2034,
		quirk_disable_intel_boot_interrupt);

 
#define BC_HT1000_FEATURE_REG		0x64
#define BC_HT1000_PIC_REGS_ENABLE	(1<<0)
#define BC_HT1000_MAP_IDX		0xC00
#define BC_HT1000_MAP_DATA		0xC01

static void quirk_disable_broadcom_boot_interrupt(struct pci_dev *dev)
{
	u32 pci_config_dword;
	u8 irq;

	if (noioapicquirk)
		return;

	pci_read_config_dword(dev, BC_HT1000_FEATURE_REG, &pci_config_dword);
	pci_write_config_dword(dev, BC_HT1000_FEATURE_REG, pci_config_dword |
			BC_HT1000_PIC_REGS_ENABLE);

	for (irq = 0x10; irq < 0x10 + 32; irq++) {
		outb(irq, BC_HT1000_MAP_IDX);
		outb(0x00, BC_HT1000_MAP_DATA);
	}

	pci_write_config_dword(dev, BC_HT1000_FEATURE_REG, pci_config_dword);

	pci_info(dev, "disabled boot interrupts on device [%04x:%04x]\n",
		 dev->vendor, dev->device);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_SERVERWORKS,   PCI_DEVICE_ID_SERVERWORKS_HT1000SB,	quirk_disable_broadcom_boot_interrupt);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_SERVERWORKS,   PCI_DEVICE_ID_SERVERWORKS_HT1000SB,	quirk_disable_broadcom_boot_interrupt);

 

 
#define AMD_813X_MISC			0x40
#define AMD_813X_NOIOAMODE		(1<<0)
#define AMD_813X_REV_B1			0x12
#define AMD_813X_REV_B2			0x13

static void quirk_disable_amd_813x_boot_interrupt(struct pci_dev *dev)
{
	u32 pci_config_dword;

	if (noioapicquirk)
		return;
	if ((dev->revision == AMD_813X_REV_B1) ||
	    (dev->revision == AMD_813X_REV_B2))
		return;

	pci_read_config_dword(dev, AMD_813X_MISC, &pci_config_dword);
	pci_config_dword &= ~AMD_813X_NOIOAMODE;
	pci_write_config_dword(dev, AMD_813X_MISC, pci_config_dword);

	pci_info(dev, "disabled boot interrupts on device [%04x:%04x]\n",
		 dev->vendor, dev->device);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD,	PCI_DEVICE_ID_AMD_8131_BRIDGE,	quirk_disable_amd_813x_boot_interrupt);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_AMD,	PCI_DEVICE_ID_AMD_8131_BRIDGE,	quirk_disable_amd_813x_boot_interrupt);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD,	PCI_DEVICE_ID_AMD_8132_BRIDGE,	quirk_disable_amd_813x_boot_interrupt);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_AMD,	PCI_DEVICE_ID_AMD_8132_BRIDGE,	quirk_disable_amd_813x_boot_interrupt);

#define AMD_8111_PCI_IRQ_ROUTING	0x56

static void quirk_disable_amd_8111_boot_interrupt(struct pci_dev *dev)
{
	u16 pci_config_word;

	if (noioapicquirk)
		return;

	pci_read_config_word(dev, AMD_8111_PCI_IRQ_ROUTING, &pci_config_word);
	if (!pci_config_word) {
		pci_info(dev, "boot interrupts on device [%04x:%04x] already disabled\n",
			 dev->vendor, dev->device);
		return;
	}
	pci_write_config_word(dev, AMD_8111_PCI_IRQ_ROUTING, 0);
	pci_info(dev, "disabled boot interrupts on device [%04x:%04x]\n",
		 dev->vendor, dev->device);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD,   PCI_DEVICE_ID_AMD_8111_SMBUS,	quirk_disable_amd_8111_boot_interrupt);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_AMD,   PCI_DEVICE_ID_AMD_8111_SMBUS,	quirk_disable_amd_8111_boot_interrupt);
#endif  

 
static void quirk_tc86c001_ide(struct pci_dev *dev)
{
	struct resource *r = &dev->resource[0];

	if (r->start & 0x8) {
		r->flags |= IORESOURCE_UNSET;
		r->start = 0;
		r->end = 0xf;
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_TOSHIBA_2,
			 PCI_DEVICE_ID_TOSHIBA_TC86C001_IDE,
			 quirk_tc86c001_ide);

 
static void quirk_plx_pci9050(struct pci_dev *dev)
{
	unsigned int bar;

	 
	if (dev->revision >= 2)
		return;
	for (bar = 0; bar <= 1; bar++)
		if (pci_resource_len(dev, bar) == 0x80 &&
		    (pci_resource_start(dev, bar) & 0x80)) {
			struct resource *r = &dev->resource[bar];
			pci_info(dev, "Re-allocating PLX PCI 9050 BAR %u to length 256 to avoid bit 7 bug\n",
				 bar);
			r->flags |= IORESOURCE_UNSET;
			r->start = 0;
			r->end = 0xff;
		}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
			 quirk_plx_pci9050);
 
DECLARE_PCI_FIXUP_HEADER(0x1402, 0x2000, quirk_plx_pci9050);
DECLARE_PCI_FIXUP_HEADER(0x1402, 0x2600, quirk_plx_pci9050);

static void quirk_netmos(struct pci_dev *dev)
{
	unsigned int num_parallel = (dev->subsystem_device & 0xf0) >> 4;
	unsigned int num_serial = dev->subsystem_device & 0xf;

	 
	switch (dev->device) {
	case PCI_DEVICE_ID_NETMOS_9835:
		 
		if (dev->subsystem_vendor == PCI_VENDOR_ID_IBM &&
				dev->subsystem_device == 0x0299)
			return;
		fallthrough;
	case PCI_DEVICE_ID_NETMOS_9735:
	case PCI_DEVICE_ID_NETMOS_9745:
	case PCI_DEVICE_ID_NETMOS_9845:
	case PCI_DEVICE_ID_NETMOS_9855:
		if (num_parallel) {
			pci_info(dev, "Netmos %04x (%u parallel, %u serial); changing class SERIAL to OTHER (use parport_serial)\n",
				dev->device, num_parallel, num_serial);
			dev->class = (PCI_CLASS_COMMUNICATION_OTHER << 8) |
			    (dev->class & 0xff);
		}
	}
}
DECLARE_PCI_FIXUP_CLASS_HEADER(PCI_VENDOR_ID_NETMOS, PCI_ANY_ID,
			 PCI_CLASS_COMMUNICATION_SERIAL, 8, quirk_netmos);

static void quirk_e100_interrupt(struct pci_dev *dev)
{
	u16 command, pmcsr;
	u8 __iomem *csr;
	u8 cmd_hi;

	switch (dev->device) {
	 
	case 0x1029:
	case 0x1030 ... 0x1034:
	case 0x1038 ... 0x103E:
	case 0x1050 ... 0x1057:
	case 0x1059:
	case 0x1064 ... 0x106B:
	case 0x1091 ... 0x1095:
	case 0x1209:
	case 0x1229:
	case 0x2449:
	case 0x2459:
	case 0x245D:
	case 0x27DC:
		break;
	default:
		return;
	}

	 
	pci_read_config_word(dev, PCI_COMMAND, &command);

	if (!(command & PCI_COMMAND_MEMORY) || !pci_resource_start(dev, 0))
		return;

	 
	if (dev->pm_cap) {
		pci_read_config_word(dev, dev->pm_cap + PCI_PM_CTRL, &pmcsr);
		if ((pmcsr & PCI_PM_CTRL_STATE_MASK) != PCI_D0)
			return;
	}

	 
	csr = ioremap(pci_resource_start(dev, 0), 8);
	if (!csr) {
		pci_warn(dev, "Can't map e100 registers\n");
		return;
	}

	cmd_hi = readb(csr + 3);
	if (cmd_hi == 0) {
		pci_warn(dev, "Firmware left e100 interrupts enabled; disabling\n");
		writeb(1, csr + 3);
	}

	iounmap(csr);
}
DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_VENDOR_ID_INTEL, PCI_ANY_ID,
			PCI_CLASS_NETWORK_ETHERNET, 8, quirk_e100_interrupt);

 
static void quirk_disable_aspm_l0s(struct pci_dev *dev)
{
	pci_info(dev, "Disabling L0s\n");
	pci_disable_link_state(dev, PCIE_LINK_STATE_L0S);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10a7, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10a9, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10b6, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10c6, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10c7, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10c8, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10d6, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10db, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10dd, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10e1, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10ec, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10f1, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10f4, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1508, quirk_disable_aspm_l0s);

static void quirk_disable_aspm_l0s_l1(struct pci_dev *dev)
{
	pci_info(dev, "Disabling ASPM L0s/L1\n");
	pci_disable_link_state(dev, PCIE_LINK_STATE_L0S | PCIE_LINK_STATE_L1);
}

 
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ASMEDIA, 0x1080, quirk_disable_aspm_l0s_l1);

 
static void quirk_enable_clear_retrain_link(struct pci_dev *dev)
{
	dev->clear_retrain_link = 1;
	pci_info(dev, "Enable PCIe Retrain Link quirk\n");
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_PERICOM, 0xe110, quirk_enable_clear_retrain_link);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_PERICOM, 0xe111, quirk_enable_clear_retrain_link);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_PERICOM, 0xe130, quirk_enable_clear_retrain_link);

static void fixup_rev1_53c810(struct pci_dev *dev)
{
	u32 class = dev->class;

	 
	if (class)
		return;

	dev->class = PCI_CLASS_STORAGE_SCSI << 8;
	pci_info(dev, "NCR 53c810 rev 1 PCI class overridden (%#08x -> %#08x)\n",
		 class, dev->class);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_NCR, PCI_DEVICE_ID_NCR_53C810, fixup_rev1_53c810);

 
static void quirk_p64h2_1k_io(struct pci_dev *dev)
{
	u16 en1k;

	pci_read_config_word(dev, 0x40, &en1k);

	if (en1k & 0x200) {
		pci_info(dev, "Enable I/O Space to 1KB granularity\n");
		dev->io_window_1k = 1;
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x1460, quirk_p64h2_1k_io);

 
static void quirk_nvidia_ck804_pcie_aer_ext_cap(struct pci_dev *dev)
{
	uint8_t b;

	if (pci_read_config_byte(dev, 0xf41, &b) == 0) {
		if (!(b & 0x20)) {
			pci_write_config_byte(dev, 0xf41, b | 0x20);
			pci_info(dev, "Linking AER extended capability\n");
		}
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_NVIDIA,  PCI_DEVICE_ID_NVIDIA_CK804_PCIE,
			quirk_nvidia_ck804_pcie_aer_ext_cap);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_NVIDIA,  PCI_DEVICE_ID_NVIDIA_CK804_PCIE,
			quirk_nvidia_ck804_pcie_aer_ext_cap);

static void quirk_via_cx700_pci_parking_caching(struct pci_dev *dev)
{
	 

	 
	struct pci_dev *p = pci_get_device(PCI_VENDOR_ID_VIA,
		PCI_DEVICE_ID_VIA_8235_USB_2, NULL);
	uint8_t b;

	 
	p = pci_get_device(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_8235_USB_2, p);
	if (!p)
		return;
	pci_dev_put(p);

	if (pci_read_config_byte(dev, 0x76, &b) == 0) {
		if (b & 0x40) {
			 
			pci_write_config_byte(dev, 0x76, b ^ 0x40);

			pci_info(dev, "Disabling VIA CX700 PCI parking\n");
		}
	}

	if (pci_read_config_byte(dev, 0x72, &b) == 0) {
		if (b != 0) {
			 
			pci_write_config_byte(dev, 0x72, 0x0);

			 
			pci_write_config_byte(dev, 0x75, 0x1);

			 
			pci_write_config_byte(dev, 0x77, 0x0);

			pci_info(dev, "Disabling VIA CX700 PCI caching\n");
		}
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA, 0x324e, quirk_via_cx700_pci_parking_caching);

static void quirk_brcm_5719_limit_mrrs(struct pci_dev *dev)
{
	u32 rev;

	pci_read_config_dword(dev, 0xf4, &rev);

	 
	if (rev == 0x05719000) {
		int readrq = pcie_get_readrq(dev);
		if (readrq > 2048)
			pcie_set_readrq(dev, 2048);
	}
}
DECLARE_PCI_FIXUP_ENABLE(PCI_VENDOR_ID_BROADCOM,
			 PCI_DEVICE_ID_TIGON3_5719,
			 quirk_brcm_5719_limit_mrrs);

 
static void quirk_unhide_mch_dev6(struct pci_dev *dev)
{
	u8 reg;

	if (pci_read_config_byte(dev, 0xF4, &reg) == 0 && !(reg & 0x02)) {
		pci_info(dev, "Enabling MCH 'Overflow' Device\n");
		pci_write_config_byte(dev, 0xF4, reg | 0x02);
	}
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82865_HB,
			quirk_unhide_mch_dev6);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82875_HB,
			quirk_unhide_mch_dev6);

#ifdef CONFIG_PCI_MSI
 
static void quirk_disable_all_msi(struct pci_dev *dev)
{
	pci_no_msi();
	pci_warn(dev, "MSI quirk detected; MSI disabled\n");
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_SERVERWORKS, PCI_DEVICE_ID_SERVERWORKS_GCNB_LE, quirk_disable_all_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RS400_200, quirk_disable_all_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RS480, quirk_disable_all_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_VT3336, quirk_disable_all_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_VT3351, quirk_disable_all_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_VT3364, quirk_disable_all_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_8380_0, quirk_disable_all_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_SI, 0x0761, quirk_disable_all_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_SAMSUNG, 0xa5e3, quirk_disable_all_msi);

 
static void quirk_disable_msi(struct pci_dev *dev)
{
	if (dev->subordinate) {
		pci_warn(dev, "MSI quirk detected; subordinate MSI disabled\n");
		dev->subordinate->bus_flags |= PCI_BUS_FLAGS_NO_MSI;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_8131_BRIDGE, quirk_disable_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA, 0xa238, quirk_disable_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x5a3f, quirk_disable_msi);

 
static void quirk_amd_780_apc_msi(struct pci_dev *host_bridge)
{
	struct pci_dev *apc_bridge;

	apc_bridge = pci_get_slot(host_bridge->bus, PCI_DEVFN(1, 0));
	if (apc_bridge) {
		if (apc_bridge->device == 0x9602)
			quirk_disable_msi(apc_bridge);
		pci_dev_put(apc_bridge);
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD, 0x9600, quirk_amd_780_apc_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD, 0x9601, quirk_amd_780_apc_msi);

 
static int msi_ht_cap_enabled(struct pci_dev *dev)
{
	int pos, ttl = PCI_FIND_CAP_TTL;

	pos = pci_find_ht_capability(dev, HT_CAPTYPE_MSI_MAPPING);
	while (pos && ttl--) {
		u8 flags;

		if (pci_read_config_byte(dev, pos + HT_MSI_FLAGS,
					 &flags) == 0) {
			pci_info(dev, "Found %s HT MSI Mapping\n",
				flags & HT_MSI_FLAGS_ENABLE ?
				"enabled" : "disabled");
			return (flags & HT_MSI_FLAGS_ENABLE) != 0;
		}

		pos = pci_find_next_ht_capability(dev, pos,
						  HT_CAPTYPE_MSI_MAPPING);
	}
	return 0;
}

 
static void quirk_msi_ht_cap(struct pci_dev *dev)
{
	if (!msi_ht_cap_enabled(dev))
		quirk_disable_msi(dev);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_SERVERWORKS, PCI_DEVICE_ID_SERVERWORKS_HT2000_PCIE,
			quirk_msi_ht_cap);

 
static void quirk_nvidia_ck804_msi_ht_cap(struct pci_dev *dev)
{
	struct pci_dev *pdev;

	 
	pdev = pci_get_slot(dev->bus, 0);
	if (!pdev)
		return;
	if (!msi_ht_cap_enabled(pdev))
		quirk_msi_ht_cap(dev);
	pci_dev_put(pdev);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_CK804_PCIE,
			quirk_nvidia_ck804_msi_ht_cap);

 
static void ht_enable_msi_mapping(struct pci_dev *dev)
{
	int pos, ttl = PCI_FIND_CAP_TTL;

	pos = pci_find_ht_capability(dev, HT_CAPTYPE_MSI_MAPPING);
	while (pos && ttl--) {
		u8 flags;

		if (pci_read_config_byte(dev, pos + HT_MSI_FLAGS,
					 &flags) == 0) {
			pci_info(dev, "Enabling HT MSI Mapping\n");

			pci_write_config_byte(dev, pos + HT_MSI_FLAGS,
					      flags | HT_MSI_FLAGS_ENABLE);
		}
		pos = pci_find_next_ht_capability(dev, pos,
						  HT_CAPTYPE_MSI_MAPPING);
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_SERVERWORKS,
			 PCI_DEVICE_ID_SERVERWORKS_HT1000_PXB,
			 ht_enable_msi_mapping);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_8132_BRIDGE,
			 ht_enable_msi_mapping);

 
static void nvenet_msi_disable(struct pci_dev *dev)
{
	const char *board_name = dmi_get_system_info(DMI_BOARD_NAME);

	if (board_name &&
	    (strstr(board_name, "P5N32-SLI PREMIUM") ||
	     strstr(board_name, "P5N32-E SLI"))) {
		pci_info(dev, "Disabling MSI for MCP55 NIC on P5N32-SLI\n");
		dev->no_msi = 1;
	}
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA,
			PCI_DEVICE_ID_NVIDIA_NVENET_15,
			nvenet_msi_disable);

 
static void pci_quirk_nvidia_tegra_disable_rp_msi(struct pci_dev *dev)
{
	dev->no_msi = 1;
}
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_NVIDIA, 0x1ad0,
			      PCI_CLASS_BRIDGE_PCI, 8,
			      pci_quirk_nvidia_tegra_disable_rp_msi);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_NVIDIA, 0x1ad1,
			      PCI_CLASS_BRIDGE_PCI, 8,
			      pci_quirk_nvidia_tegra_disable_rp_msi);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_NVIDIA, 0x1ad2,
			      PCI_CLASS_BRIDGE_PCI, 8,
			      pci_quirk_nvidia_tegra_disable_rp_msi);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0bf0,
			      PCI_CLASS_BRIDGE_PCI, 8,
			      pci_quirk_nvidia_tegra_disable_rp_msi);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0bf1,
			      PCI_CLASS_BRIDGE_PCI, 8,
			      pci_quirk_nvidia_tegra_disable_rp_msi);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0e1c,
			      PCI_CLASS_BRIDGE_PCI, 8,
			      pci_quirk_nvidia_tegra_disable_rp_msi);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0e1d,
			      PCI_CLASS_BRIDGE_PCI, 8,
			      pci_quirk_nvidia_tegra_disable_rp_msi);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0e12,
			      PCI_CLASS_BRIDGE_PCI, 8,
			      pci_quirk_nvidia_tegra_disable_rp_msi);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0e13,
			      PCI_CLASS_BRIDGE_PCI, 8,
			      pci_quirk_nvidia_tegra_disable_rp_msi);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0fae,
			      PCI_CLASS_BRIDGE_PCI, 8,
			      pci_quirk_nvidia_tegra_disable_rp_msi);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0faf,
			      PCI_CLASS_BRIDGE_PCI, 8,
			      pci_quirk_nvidia_tegra_disable_rp_msi);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_NVIDIA, 0x10e5,
			      PCI_CLASS_BRIDGE_PCI, 8,
			      pci_quirk_nvidia_tegra_disable_rp_msi);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_NVIDIA, 0x10e6,
			      PCI_CLASS_BRIDGE_PCI, 8,
			      pci_quirk_nvidia_tegra_disable_rp_msi);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_NVIDIA, 0x229a,
			      PCI_CLASS_BRIDGE_PCI, 8,
			      pci_quirk_nvidia_tegra_disable_rp_msi);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_NVIDIA, 0x229c,
			      PCI_CLASS_BRIDGE_PCI, 8,
			      pci_quirk_nvidia_tegra_disable_rp_msi);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_NVIDIA, 0x229e,
			      PCI_CLASS_BRIDGE_PCI, 8,
			      pci_quirk_nvidia_tegra_disable_rp_msi);

 
static void nvbridge_check_legacy_irq_routing(struct pci_dev *dev)
{
	u32 cfg;

	if (!pci_find_capability(dev, PCI_CAP_ID_HT))
		return;

	pci_read_config_dword(dev, 0x74, &cfg);

	if (cfg & ((1 << 2) | (1 << 15))) {
		pr_info("Rewriting IRQ routing register on MCP55\n");
		cfg &= ~((1 << 2) | (1 << 15));
		pci_write_config_dword(dev, 0x74, cfg);
	}
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA,
			PCI_DEVICE_ID_NVIDIA_MCP55_BRIDGE_V0,
			nvbridge_check_legacy_irq_routing);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA,
			PCI_DEVICE_ID_NVIDIA_MCP55_BRIDGE_V4,
			nvbridge_check_legacy_irq_routing);

static int ht_check_msi_mapping(struct pci_dev *dev)
{
	int pos, ttl = PCI_FIND_CAP_TTL;
	int found = 0;

	 
	pos = pci_find_ht_capability(dev, HT_CAPTYPE_MSI_MAPPING);
	while (pos && ttl--) {
		u8 flags;

		if (found < 1)
			found = 1;
		if (pci_read_config_byte(dev, pos + HT_MSI_FLAGS,
					 &flags) == 0) {
			if (flags & HT_MSI_FLAGS_ENABLE) {
				if (found < 2) {
					found = 2;
					break;
				}
			}
		}
		pos = pci_find_next_ht_capability(dev, pos,
						  HT_CAPTYPE_MSI_MAPPING);
	}

	return found;
}

static int host_bridge_with_leaf(struct pci_dev *host_bridge)
{
	struct pci_dev *dev;
	int pos;
	int i, dev_no;
	int found = 0;

	dev_no = host_bridge->devfn >> 3;
	for (i = dev_no + 1; i < 0x20; i++) {
		dev = pci_get_slot(host_bridge->bus, PCI_DEVFN(i, 0));
		if (!dev)
			continue;

		 
		pos = pci_find_ht_capability(dev, HT_CAPTYPE_SLAVE);
		if (pos != 0) {
			pci_dev_put(dev);
			break;
		}

		if (ht_check_msi_mapping(dev)) {
			found = 1;
			pci_dev_put(dev);
			break;
		}
		pci_dev_put(dev);
	}

	return found;
}

#define PCI_HT_CAP_SLAVE_CTRL0     4     
#define PCI_HT_CAP_SLAVE_CTRL1     8     

static int is_end_of_ht_chain(struct pci_dev *dev)
{
	int pos, ctrl_off;
	int end = 0;
	u16 flags, ctrl;

	pos = pci_find_ht_capability(dev, HT_CAPTYPE_SLAVE);

	if (!pos)
		goto out;

	pci_read_config_word(dev, pos + PCI_CAP_FLAGS, &flags);

	ctrl_off = ((flags >> 10) & 1) ?
			PCI_HT_CAP_SLAVE_CTRL0 : PCI_HT_CAP_SLAVE_CTRL1;
	pci_read_config_word(dev, pos + ctrl_off, &ctrl);

	if (ctrl & (1 << 6))
		end = 1;

out:
	return end;
}

static void nv_ht_enable_msi_mapping(struct pci_dev *dev)
{
	struct pci_dev *host_bridge;
	int pos;
	int i, dev_no;
	int found = 0;

	dev_no = dev->devfn >> 3;
	for (i = dev_no; i >= 0; i--) {
		host_bridge = pci_get_slot(dev->bus, PCI_DEVFN(i, 0));
		if (!host_bridge)
			continue;

		pos = pci_find_ht_capability(host_bridge, HT_CAPTYPE_SLAVE);
		if (pos != 0) {
			found = 1;
			break;
		}
		pci_dev_put(host_bridge);
	}

	if (!found)
		return;

	 
	if (host_bridge == dev && is_end_of_ht_chain(host_bridge) &&
	    host_bridge_with_leaf(host_bridge))
		goto out;

	 
	if (msi_ht_cap_enabled(host_bridge))
		goto out;

	ht_enable_msi_mapping(dev);

out:
	pci_dev_put(host_bridge);
}

static void ht_disable_msi_mapping(struct pci_dev *dev)
{
	int pos, ttl = PCI_FIND_CAP_TTL;

	pos = pci_find_ht_capability(dev, HT_CAPTYPE_MSI_MAPPING);
	while (pos && ttl--) {
		u8 flags;

		if (pci_read_config_byte(dev, pos + HT_MSI_FLAGS,
					 &flags) == 0) {
			pci_info(dev, "Disabling HT MSI Mapping\n");

			pci_write_config_byte(dev, pos + HT_MSI_FLAGS,
					      flags & ~HT_MSI_FLAGS_ENABLE);
		}
		pos = pci_find_next_ht_capability(dev, pos,
						  HT_CAPTYPE_MSI_MAPPING);
	}
}

static void __nv_msi_ht_cap_quirk(struct pci_dev *dev, int all)
{
	struct pci_dev *host_bridge;
	int pos;
	int found;

	if (!pci_msi_enabled())
		return;

	 
	found = ht_check_msi_mapping(dev);

	 
	if (found == 0)
		return;

	 
	host_bridge = pci_get_domain_bus_and_slot(pci_domain_nr(dev->bus), 0,
						  PCI_DEVFN(0, 0));
	if (host_bridge == NULL) {
		pci_warn(dev, "nv_msi_ht_cap_quirk didn't locate host bridge\n");
		return;
	}

	pos = pci_find_ht_capability(host_bridge, HT_CAPTYPE_SLAVE);
	if (pos != 0) {
		 
		if (found == 1) {
			 
			if (all)
				ht_enable_msi_mapping(dev);
			else
				nv_ht_enable_msi_mapping(dev);
		}
		goto out;
	}

	 
	if (found == 1)
		goto out;

	 
	ht_disable_msi_mapping(dev);

out:
	pci_dev_put(host_bridge);
}

static void nv_msi_ht_cap_quirk_all(struct pci_dev *dev)
{
	return __nv_msi_ht_cap_quirk(dev, 1);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AL, PCI_ANY_ID, nv_msi_ht_cap_quirk_all);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_AL, PCI_ANY_ID, nv_msi_ht_cap_quirk_all);

static void nv_msi_ht_cap_quirk_leaf(struct pci_dev *dev)
{
	return __nv_msi_ht_cap_quirk(dev, 0);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_NVIDIA, PCI_ANY_ID, nv_msi_ht_cap_quirk_leaf);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_NVIDIA, PCI_ANY_ID, nv_msi_ht_cap_quirk_leaf);

static void quirk_msi_intx_disable_bug(struct pci_dev *dev)
{
	dev->dev_flags |= PCI_DEV_FLAGS_MSI_INTX_DISABLE_BUG;
}

static void quirk_msi_intx_disable_ati_bug(struct pci_dev *dev)
{
	struct pci_dev *p;

	 
	p = pci_get_device(PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_SBX00_SMBUS,
			   NULL);
	if (!p)
		return;

	if ((p->revision < 0x3B) && (p->revision >= 0x30))
		dev->dev_flags |= PCI_DEV_FLAGS_MSI_INTX_DISABLE_BUG;
	pci_dev_put(p);
}

static void quirk_msi_intx_disable_qca_bug(struct pci_dev *dev)
{
	 
	if (dev->revision < 0x18) {
		pci_info(dev, "set MSI_INTX_DISABLE_BUG flag\n");
		dev->dev_flags |= PCI_DEV_FLAGS_MSI_INTX_DISABLE_BUG;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_BROADCOM,
			PCI_DEVICE_ID_TIGON3_5780,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_BROADCOM,
			PCI_DEVICE_ID_TIGON3_5780S,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_BROADCOM,
			PCI_DEVICE_ID_TIGON3_5714,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_BROADCOM,
			PCI_DEVICE_ID_TIGON3_5714S,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_BROADCOM,
			PCI_DEVICE_ID_TIGON3_5715,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_BROADCOM,
			PCI_DEVICE_ID_TIGON3_5715S,
			quirk_msi_intx_disable_bug);

DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4390,
			quirk_msi_intx_disable_ati_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4391,
			quirk_msi_intx_disable_ati_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4392,
			quirk_msi_intx_disable_ati_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4393,
			quirk_msi_intx_disable_ati_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4394,
			quirk_msi_intx_disable_ati_bug);

DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4373,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4374,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4375,
			quirk_msi_intx_disable_bug);

DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATTANSIC, 0x1062,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATTANSIC, 0x1063,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATTANSIC, 0x2060,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATTANSIC, 0x2062,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATTANSIC, 0x1073,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATTANSIC, 0x1083,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATTANSIC, 0x1090,
			quirk_msi_intx_disable_qca_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATTANSIC, 0x1091,
			quirk_msi_intx_disable_qca_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATTANSIC, 0x10a0,
			quirk_msi_intx_disable_qca_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATTANSIC, 0x10a1,
			quirk_msi_intx_disable_qca_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATTANSIC, 0xe091,
			quirk_msi_intx_disable_qca_bug);

 
static void quirk_al_msi_disable(struct pci_dev *dev)
{
	dev->no_msi = 1;
	pci_warn(dev, "Disabling MSI/MSI-X\n");
}
DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_VENDOR_ID_AMAZON_ANNAPURNA_LABS, 0x0031,
			      PCI_CLASS_BRIDGE_PCI, 8, quirk_al_msi_disable);
#endif  

 
static void quirk_hotplug_bridge(struct pci_dev *dev)
{
	dev->is_hotplug_bridge = 1;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_HINT, 0x0020, quirk_hotplug_bridge);

 
#ifdef CONFIG_MMC_RICOH_MMC
static void ricoh_mmc_fixup_rl5c476(struct pci_dev *dev)
{
	u8 write_enable;
	u8 write_target;
	u8 disable;

	 
	if (PCI_FUNC(dev->devfn))
		return;

	pci_read_config_byte(dev, 0xB7, &disable);
	if (disable & 0x02)
		return;

	pci_read_config_byte(dev, 0x8E, &write_enable);
	pci_write_config_byte(dev, 0x8E, 0xAA);
	pci_read_config_byte(dev, 0x8D, &write_target);
	pci_write_config_byte(dev, 0x8D, 0xB7);
	pci_write_config_byte(dev, 0xB7, disable | 0x02);
	pci_write_config_byte(dev, 0x8E, write_enable);
	pci_write_config_byte(dev, 0x8D, write_target);

	pci_notice(dev, "proprietary Ricoh MMC controller disabled (via CardBus function)\n");
	pci_notice(dev, "MMC cards are now supported by standard SDHCI controller\n");
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_RICOH, PCI_DEVICE_ID_RICOH_RL5C476, ricoh_mmc_fixup_rl5c476);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_RICOH, PCI_DEVICE_ID_RICOH_RL5C476, ricoh_mmc_fixup_rl5c476);

static void ricoh_mmc_fixup_r5c832(struct pci_dev *dev)
{
	u8 write_enable;
	u8 disable;

	 
	if (PCI_FUNC(dev->devfn))
		return;
	 
	if (dev->device == PCI_DEVICE_ID_RICOH_R5CE822 ||
	    dev->device == PCI_DEVICE_ID_RICOH_R5CE823) {
		pci_write_config_byte(dev, 0xf9, 0xfc);
		pci_write_config_byte(dev, 0x150, 0x10);
		pci_write_config_byte(dev, 0xf9, 0x00);
		pci_write_config_byte(dev, 0xfc, 0x01);
		pci_write_config_byte(dev, 0xe1, 0x32);
		pci_write_config_byte(dev, 0xfc, 0x00);

		pci_notice(dev, "MMC controller base frequency changed to 50Mhz.\n");
	}

	pci_read_config_byte(dev, 0xCB, &disable);

	if (disable & 0x02)
		return;

	pci_read_config_byte(dev, 0xCA, &write_enable);
	pci_write_config_byte(dev, 0xCA, 0x57);
	pci_write_config_byte(dev, 0xCB, disable | 0x02);
	pci_write_config_byte(dev, 0xCA, write_enable);

	pci_notice(dev, "proprietary Ricoh MMC controller disabled (via FireWire function)\n");
	pci_notice(dev, "MMC cards are now supported by standard SDHCI controller\n");

}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_RICOH, PCI_DEVICE_ID_RICOH_R5C832, ricoh_mmc_fixup_r5c832);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_RICOH, PCI_DEVICE_ID_RICOH_R5C832, ricoh_mmc_fixup_r5c832);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_RICOH, PCI_DEVICE_ID_RICOH_R5CE822, ricoh_mmc_fixup_r5c832);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_RICOH, PCI_DEVICE_ID_RICOH_R5CE822, ricoh_mmc_fixup_r5c832);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_RICOH, PCI_DEVICE_ID_RICOH_R5CE823, ricoh_mmc_fixup_r5c832);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_RICOH, PCI_DEVICE_ID_RICOH_R5CE823, ricoh_mmc_fixup_r5c832);
#endif  

#ifdef CONFIG_DMAR_TABLE
#define VTUNCERRMSK_REG	0x1ac
#define VTD_MSK_SPEC_ERRORS	(1 << 31)
 
static void vtd_mask_spec_errors(struct pci_dev *dev)
{
	u32 word;

	pci_read_config_dword(dev, VTUNCERRMSK_REG, &word);
	pci_write_config_dword(dev, VTUNCERRMSK_REG, word | VTD_MSK_SPEC_ERRORS);
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x342e, vtd_mask_spec_errors);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x3c28, vtd_mask_spec_errors);
#endif

static void fixup_ti816x_class(struct pci_dev *dev)
{
	u32 class = dev->class;

	 
	dev->class = PCI_CLASS_MULTIMEDIA_VIDEO << 8;
	pci_info(dev, "PCI class overridden (%#08x -> %#08x)\n",
		 class, dev->class);
}
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_TI, 0xb800,
			      PCI_CLASS_NOT_DEFINED, 8, fixup_ti816x_class);

 
static void fixup_mpss_256(struct pci_dev *dev)
{
	dev->pcie_mpss = 1;  
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_SOLARFLARE,
			PCI_DEVICE_ID_SOLARFLARE_SFC4000A_0, fixup_mpss_256);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_SOLARFLARE,
			PCI_DEVICE_ID_SOLARFLARE_SFC4000A_1, fixup_mpss_256);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_SOLARFLARE,
			PCI_DEVICE_ID_SOLARFLARE_SFC4000B, fixup_mpss_256);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_ASMEDIA, 0x0612, fixup_mpss_256);

 
static void quirk_intel_mc_errata(struct pci_dev *dev)
{
	int err;
	u16 rcc;

	if (pcie_bus_config == PCIE_BUS_TUNE_OFF ||
	    pcie_bus_config == PCIE_BUS_DEFAULT)
		return;

	 
	err = pci_read_config_word(dev, 0x48, &rcc);
	if (err) {
		pci_err(dev, "Error attempting to read the read completion coalescing register\n");
		return;
	}

	if (!(rcc & (1 << 10)))
		return;

	rcc &= ~(1 << 10);

	err = pci_write_config_word(dev, 0x48, rcc);
	if (err) {
		pci_err(dev, "Error attempting to write the read completion coalescing register\n");
		return;
	}

	pr_info_once("Read completion coalescing disabled due to hardware erratum relating to 256B MPS\n");
}
 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25c0, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25d0, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25d4, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25d8, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25e2, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25e3, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25e4, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25e5, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25e6, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25e7, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25f7, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25f8, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25f9, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25fa, quirk_intel_mc_errata);
 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65c0, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65e2, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65e3, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65e4, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65e5, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65e6, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65e7, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65f7, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65f8, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65f9, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65fa, quirk_intel_mc_errata);

 
static void quirk_intel_ntb(struct pci_dev *dev)
{
	int rc;
	u8 val;

	rc = pci_read_config_byte(dev, 0x00D0, &val);
	if (rc)
		return;

	dev->resource[2].end = dev->resource[2].start + ((u64) 1 << val) - 1;

	rc = pci_read_config_byte(dev, 0x00D1, &val);
	if (rc)
		return;

	dev->resource[4].end = dev->resource[4].start + ((u64) 1 << val) - 1;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x0e08, quirk_intel_ntb);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x0e0d, quirk_intel_ntb);

 
#define I915_DEIER_REG 0x4400c
static void disable_igfx_irq(struct pci_dev *dev)
{
	void __iomem *regs = pci_iomap(dev, 0, 0);
	if (regs == NULL) {
		pci_warn(dev, "igfx quirk: Can't iomap PCI device\n");
		return;
	}

	 
	if (readl(regs + I915_DEIER_REG) != 0) {
		pci_warn(dev, "BIOS left Intel GPU interrupts enabled; disabling\n");

		writel(0, regs + I915_DEIER_REG);
	}

	pci_iounmap(dev, regs);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x0042, disable_igfx_irq);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x0046, disable_igfx_irq);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x004a, disable_igfx_irq);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x0102, disable_igfx_irq);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x0106, disable_igfx_irq);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x010a, disable_igfx_irq);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x0152, disable_igfx_irq);

 
static void quirk_remove_d3hot_delay(struct pci_dev *dev)
{
	dev->d3hot_delay = 0;
}
 
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x0412, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x0c00, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x0c0c, quirk_remove_d3hot_delay);
 
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x8c02, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x8c18, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x8c1c, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x8c20, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x8c22, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x8c26, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x8c2d, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x8c31, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x8c3a, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x8c3d, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x8c4e, quirk_remove_d3hot_delay);
 
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x2280, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x2298, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x229c, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x22b0, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x22b5, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x22b7, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x22b8, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x22d8, quirk_remove_d3hot_delay);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x22dc, quirk_remove_d3hot_delay);

 
static void quirk_broken_intx_masking(struct pci_dev *dev)
{
	dev->broken_intx_masking = 1;
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_CHELSIO, 0x0030,
			quirk_broken_intx_masking);
DECLARE_PCI_FIXUP_FINAL(0x1814, 0x0601,  
			quirk_broken_intx_masking);
DECLARE_PCI_FIXUP_FINAL(0x1b7c, 0x0004,  
			quirk_broken_intx_masking);

 
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_REALTEK, 0x8169,
			quirk_broken_intx_masking);

 
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1572, quirk_broken_intx_masking);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1574, quirk_broken_intx_masking);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1580, quirk_broken_intx_masking);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1581, quirk_broken_intx_masking);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1583, quirk_broken_intx_masking);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1584, quirk_broken_intx_masking);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1585, quirk_broken_intx_masking);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1586, quirk_broken_intx_masking);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1587, quirk_broken_intx_masking);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1588, quirk_broken_intx_masking);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1589, quirk_broken_intx_masking);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x158a, quirk_broken_intx_masking);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x158b, quirk_broken_intx_masking);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x37d0, quirk_broken_intx_masking);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x37d1, quirk_broken_intx_masking);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x37d2, quirk_broken_intx_masking);

static u16 mellanox_broken_intx_devs[] = {
	PCI_DEVICE_ID_MELLANOX_HERMON_SDR,
	PCI_DEVICE_ID_MELLANOX_HERMON_DDR,
	PCI_DEVICE_ID_MELLANOX_HERMON_QDR,
	PCI_DEVICE_ID_MELLANOX_HERMON_DDR_GEN2,
	PCI_DEVICE_ID_MELLANOX_HERMON_QDR_GEN2,
	PCI_DEVICE_ID_MELLANOX_HERMON_EN,
	PCI_DEVICE_ID_MELLANOX_HERMON_EN_GEN2,
	PCI_DEVICE_ID_MELLANOX_CONNECTX_EN,
	PCI_DEVICE_ID_MELLANOX_CONNECTX_EN_T_GEN2,
	PCI_DEVICE_ID_MELLANOX_CONNECTX_EN_GEN2,
	PCI_DEVICE_ID_MELLANOX_CONNECTX_EN_5_GEN2,
	PCI_DEVICE_ID_MELLANOX_CONNECTX2,
	PCI_DEVICE_ID_MELLANOX_CONNECTX3,
	PCI_DEVICE_ID_MELLANOX_CONNECTX3_PRO,
};

#define CONNECTX_4_CURR_MAX_MINOR 99
#define CONNECTX_4_INTX_SUPPORT_MINOR 14

 
static void mellanox_check_broken_intx_masking(struct pci_dev *pdev)
{
	__be32 __iomem *fw_ver;
	u16 fw_major;
	u16 fw_minor;
	u16 fw_subminor;
	u32 fw_maj_min;
	u32 fw_sub_min;
	int i;

	for (i = 0; i < ARRAY_SIZE(mellanox_broken_intx_devs); i++) {
		if (pdev->device == mellanox_broken_intx_devs[i]) {
			pdev->broken_intx_masking = 1;
			return;
		}
	}

	 
	if (pdev->device == PCI_DEVICE_ID_MELLANOX_CONNECTIB)
		return;

	if (pdev->device != PCI_DEVICE_ID_MELLANOX_CONNECTX4 &&
	    pdev->device != PCI_DEVICE_ID_MELLANOX_CONNECTX4_LX)
		return;

	 
	if (pci_enable_device_mem(pdev)) {
		pci_warn(pdev, "Can't enable device memory\n");
		return;
	}

	fw_ver = ioremap(pci_resource_start(pdev, 0), 4);
	if (!fw_ver) {
		pci_warn(pdev, "Can't map ConnectX-4 initialization segment\n");
		goto out;
	}

	 
	fw_maj_min = ioread32be(fw_ver);
	fw_sub_min = ioread32be(fw_ver + 1);
	fw_major = fw_maj_min & 0xffff;
	fw_minor = fw_maj_min >> 16;
	fw_subminor = fw_sub_min & 0xffff;
	if (fw_minor > CONNECTX_4_CURR_MAX_MINOR ||
	    fw_minor < CONNECTX_4_INTX_SUPPORT_MINOR) {
		pci_warn(pdev, "ConnectX-4: FW %u.%u.%u doesn't support INTx masking, disabling. Please upgrade FW to %d.14.1100 and up for INTx support\n",
			 fw_major, fw_minor, fw_subminor, pdev->device ==
			 PCI_DEVICE_ID_MELLANOX_CONNECTX4 ? 12 : 14);
		pdev->broken_intx_masking = 1;
	}

	iounmap(fw_ver);

out:
	pci_disable_device(pdev);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_MELLANOX, PCI_ANY_ID,
			mellanox_check_broken_intx_masking);

static void quirk_no_bus_reset(struct pci_dev *dev)
{
	dev->dev_flags |= PCI_DEV_FLAGS_NO_BUS_RESET;
}

 
static void quirk_nvidia_no_bus_reset(struct pci_dev *dev)
{
	if ((dev->device & 0xffc0) == 0x2340)
		quirk_no_bus_reset(dev);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_NVIDIA, PCI_ANY_ID,
			 quirk_nvidia_no_bus_reset);

 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_ATHEROS, 0x0030, quirk_no_bus_reset);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_ATHEROS, 0x0032, quirk_no_bus_reset);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_ATHEROS, 0x003c, quirk_no_bus_reset);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_ATHEROS, 0x0033, quirk_no_bus_reset);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_ATHEROS, 0x0034, quirk_no_bus_reset);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_ATHEROS, 0x003e, quirk_no_bus_reset);

 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_CAVIUM, 0xa100, quirk_no_bus_reset);

 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_TI, 0xb005, quirk_no_bus_reset);

static void quirk_no_pm_reset(struct pci_dev *dev)
{
	 
	if (!pci_is_root_bus(dev->bus))
		dev->dev_flags |= PCI_DEV_FLAGS_NO_PM_RESET;
}

 
DECLARE_PCI_FIXUP_CLASS_HEADER(PCI_VENDOR_ID_ATI, PCI_ANY_ID,
			       PCI_CLASS_DISPLAY_VGA, 8, quirk_no_pm_reset);

 
static void quirk_thunderbolt_hotplug_msi(struct pci_dev *pdev)
{
	if (pdev->is_hotplug_bridge &&
	    (pdev->device != PCI_DEVICE_ID_INTEL_CACTUS_RIDGE_4C ||
	     pdev->revision <= 1))
		pdev->no_msi = 1;
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_LIGHT_RIDGE,
			quirk_thunderbolt_hotplug_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_EAGLE_RIDGE,
			quirk_thunderbolt_hotplug_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_LIGHT_PEAK,
			quirk_thunderbolt_hotplug_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_CACTUS_RIDGE_4C,
			quirk_thunderbolt_hotplug_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_PORT_RIDGE,
			quirk_thunderbolt_hotplug_msi);

#ifdef CONFIG_ACPI
 
static void quirk_apple_poweroff_thunderbolt(struct pci_dev *dev)
{
	acpi_handle bridge, SXIO, SXFP, SXLV;

	if (!x86_apple_machine)
		return;
	if (pci_pcie_type(dev) != PCI_EXP_TYPE_UPSTREAM)
		return;

	 
	if (!pm_suspend_via_firmware())
		return;

	bridge = ACPI_HANDLE(&dev->dev);
	if (!bridge)
		return;

	 
	if (ACPI_FAILURE(acpi_get_handle(bridge, "DSB0.NHI0.SXIO", &SXIO))
	    || ACPI_FAILURE(acpi_get_handle(bridge, "DSB0.NHI0.SXFP", &SXFP))
	    || ACPI_FAILURE(acpi_get_handle(bridge, "DSB0.NHI0.SXLV", &SXLV)))
		return;
	pci_info(dev, "quirk: cutting power to Thunderbolt controller...\n");

	 
	acpi_execute_simple_method(SXIO, NULL, 1);
	acpi_execute_simple_method(SXFP, NULL, 0);
	msleep(300);
	acpi_execute_simple_method(SXLV, NULL, 0);
	acpi_execute_simple_method(SXIO, NULL, 0);
	acpi_execute_simple_method(SXLV, NULL, 0);
}
DECLARE_PCI_FIXUP_SUSPEND_LATE(PCI_VENDOR_ID_INTEL,
			       PCI_DEVICE_ID_INTEL_CACTUS_RIDGE_4C,
			       quirk_apple_poweroff_thunderbolt);
#endif

 
static int reset_intel_82599_sfp_virtfn(struct pci_dev *dev, bool probe)
{
	 
	if (!probe)
		pcie_flr(dev);
	return 0;
}

#define SOUTH_CHICKEN2		0xc2004
#define PCH_PP_STATUS		0xc7200
#define PCH_PP_CONTROL		0xc7204
#define MSG_CTL			0x45010
#define NSDE_PWR_STATE		0xd0100
#define IGD_OPERATION_TIMEOUT	10000      

static int reset_ivb_igd(struct pci_dev *dev, bool probe)
{
	void __iomem *mmio_base;
	unsigned long timeout;
	u32 val;

	if (probe)
		return 0;

	mmio_base = pci_iomap(dev, 0, 0);
	if (!mmio_base)
		return -ENOMEM;

	iowrite32(0x00000002, mmio_base + MSG_CTL);

	 
	iowrite32(0x00000005, mmio_base + SOUTH_CHICKEN2);

	val = ioread32(mmio_base + PCH_PP_CONTROL) & 0xfffffffe;
	iowrite32(val, mmio_base + PCH_PP_CONTROL);

	timeout = jiffies + msecs_to_jiffies(IGD_OPERATION_TIMEOUT);
	do {
		val = ioread32(mmio_base + PCH_PP_STATUS);
		if ((val & 0xb0000000) == 0)
			goto reset_complete;
		msleep(10);
	} while (time_before(jiffies, timeout));
	pci_warn(dev, "timeout during reset\n");

reset_complete:
	iowrite32(0x00000002, mmio_base + NSDE_PWR_STATE);

	pci_iounmap(dev, mmio_base);
	return 0;
}

 
static int reset_chelsio_generic_dev(struct pci_dev *dev, bool probe)
{
	u16 old_command;
	u16 msix_flags;

	 
	if ((dev->device & 0xf000) != 0x4000)
		return -ENOTTY;

	 
	if (probe)
		return 0;

	 
	pci_read_config_word(dev, PCI_COMMAND, &old_command);
	pci_write_config_word(dev, PCI_COMMAND,
			      old_command | PCI_COMMAND_MASTER);

	 
	pci_save_state(dev);

	 
	pci_read_config_word(dev, dev->msix_cap+PCI_MSIX_FLAGS, &msix_flags);
	if ((msix_flags & PCI_MSIX_FLAGS_ENABLE) == 0)
		pci_write_config_word(dev, dev->msix_cap+PCI_MSIX_FLAGS,
				      msix_flags |
				      PCI_MSIX_FLAGS_ENABLE |
				      PCI_MSIX_FLAGS_MASKALL);

	pcie_flr(dev);

	 
	pci_restore_state(dev);
	pci_write_config_word(dev, PCI_COMMAND, old_command);
	return 0;
}

#define PCI_DEVICE_ID_INTEL_82599_SFP_VF   0x10ed
#define PCI_DEVICE_ID_INTEL_IVB_M_VGA      0x0156
#define PCI_DEVICE_ID_INTEL_IVB_M2_VGA     0x0166

 
static int nvme_disable_and_flr(struct pci_dev *dev, bool probe)
{
	void __iomem *bar;
	u16 cmd;
	u32 cfg;

	if (dev->class != PCI_CLASS_STORAGE_EXPRESS ||
	    pcie_reset_flr(dev, PCI_RESET_PROBE) || !pci_resource_start(dev, 0))
		return -ENOTTY;

	if (probe)
		return 0;

	bar = pci_iomap(dev, 0, NVME_REG_CC + sizeof(cfg));
	if (!bar)
		return -ENOTTY;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	pci_write_config_word(dev, PCI_COMMAND, cmd | PCI_COMMAND_MEMORY);

	cfg = readl(bar + NVME_REG_CC);

	 
	if (cfg & NVME_CC_ENABLE) {
		u32 cap = readl(bar + NVME_REG_CAP);
		unsigned long timeout;

		 
		cfg &= ~(NVME_CC_SHN_MASK | NVME_CC_ENABLE);

		writel(cfg, bar + NVME_REG_CC);

		 

		 
		timeout = ((NVME_CAP_TIMEOUT(cap) + 1) * HZ / 2) + jiffies;

		for (;;) {
			u32 status = readl(bar + NVME_REG_CSTS);

			 
			if (!(status & NVME_CSTS_RDY))
				break;

			msleep(100);

			if (time_after(jiffies, timeout)) {
				pci_warn(dev, "Timeout waiting for NVMe ready status to clear after disable\n");
				break;
			}
		}
	}

	pci_iounmap(dev, bar);

	pcie_flr(dev);

	return 0;
}

 
static int delay_250ms_after_flr(struct pci_dev *dev, bool probe)
{
	if (probe)
		return pcie_reset_flr(dev, PCI_RESET_PROBE);

	pcie_reset_flr(dev, PCI_RESET_DO_RESET);

	msleep(250);

	return 0;
}

#define PCI_DEVICE_ID_HINIC_VF      0x375E
#define HINIC_VF_FLR_TYPE           0x1000
#define HINIC_VF_FLR_CAP_BIT        (1UL << 30)
#define HINIC_VF_OP                 0xE80
#define HINIC_VF_FLR_PROC_BIT       (1UL << 18)
#define HINIC_OPERATION_TIMEOUT     15000	 

 
static int reset_hinic_vf_dev(struct pci_dev *pdev, bool probe)
{
	unsigned long timeout;
	void __iomem *bar;
	u32 val;

	if (probe)
		return 0;

	bar = pci_iomap(pdev, 0, 0);
	if (!bar)
		return -ENOTTY;

	 
	val = ioread32be(bar + HINIC_VF_FLR_TYPE);
	if (!(val & HINIC_VF_FLR_CAP_BIT)) {
		pci_iounmap(pdev, bar);
		return -ENOTTY;
	}

	 
	val = ioread32be(bar + HINIC_VF_OP);
	val = val | HINIC_VF_FLR_PROC_BIT;
	iowrite32be(val, bar + HINIC_VF_OP);

	pcie_flr(pdev);

	 
	pci_write_config_word(pdev, PCI_VENDOR_ID, 0);

	 
	timeout = jiffies + msecs_to_jiffies(HINIC_OPERATION_TIMEOUT);
	do {
		val = ioread32be(bar + HINIC_VF_OP);
		if (!(val & HINIC_VF_FLR_PROC_BIT))
			goto reset_complete;
		msleep(20);
	} while (time_before(jiffies, timeout));

	val = ioread32be(bar + HINIC_VF_OP);
	if (!(val & HINIC_VF_FLR_PROC_BIT))
		goto reset_complete;

	pci_warn(pdev, "Reset dev timeout, FLR ack reg: %#010x\n", val);

reset_complete:
	pci_iounmap(pdev, bar);

	return 0;
}

static const struct pci_dev_reset_methods pci_dev_reset_methods[] = {
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82599_SFP_VF,
		 reset_intel_82599_sfp_virtfn },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_IVB_M_VGA,
		reset_ivb_igd },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_IVB_M2_VGA,
		reset_ivb_igd },
	{ PCI_VENDOR_ID_SAMSUNG, 0xa804, nvme_disable_and_flr },
	{ PCI_VENDOR_ID_INTEL, 0x0953, delay_250ms_after_flr },
	{ PCI_VENDOR_ID_INTEL, 0x0a54, delay_250ms_after_flr },
	{ PCI_VENDOR_ID_SOLIDIGM, 0xf1ac, delay_250ms_after_flr },
	{ PCI_VENDOR_ID_CHELSIO, PCI_ANY_ID,
		reset_chelsio_generic_dev },
	{ PCI_VENDOR_ID_HUAWEI, PCI_DEVICE_ID_HINIC_VF,
		reset_hinic_vf_dev },
	{ 0 }
};

 
int pci_dev_specific_reset(struct pci_dev *dev, bool probe)
{
	const struct pci_dev_reset_methods *i;

	for (i = pci_dev_reset_methods; i->reset; i++) {
		if ((i->vendor == dev->vendor ||
		     i->vendor == (u16)PCI_ANY_ID) &&
		    (i->device == dev->device ||
		     i->device == (u16)PCI_ANY_ID))
			return i->reset(dev, probe);
	}

	return -ENOTTY;
}

static void quirk_dma_func0_alias(struct pci_dev *dev)
{
	if (PCI_FUNC(dev->devfn) != 0)
		pci_add_dma_alias(dev, PCI_DEVFN(PCI_SLOT(dev->devfn), 0), 1);
}

 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_RICOH, 0xe832, quirk_dma_func0_alias);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_RICOH, 0xe476, quirk_dma_func0_alias);

static void quirk_dma_func1_alias(struct pci_dev *dev)
{
	if (PCI_FUNC(dev->devfn) != 1)
		pci_add_dma_alias(dev, PCI_DEVFN(PCI_SLOT(dev->devfn), 1), 1);
}

 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_MARVELL_EXT, 0x9120,
			 quirk_dma_func1_alias);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_MARVELL_EXT, 0x9123,
			 quirk_dma_func1_alias);
 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_MARVELL_EXT, 0x9125,
			 quirk_dma_func1_alias);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_MARVELL_EXT, 0x9128,
			 quirk_dma_func1_alias);
 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_MARVELL_EXT, 0x9130,
			 quirk_dma_func1_alias);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_MARVELL_EXT, 0x9170,
			 quirk_dma_func1_alias);
 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_MARVELL_EXT, 0x9172,
			 quirk_dma_func1_alias);
 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_MARVELL_EXT, 0x917a,
			 quirk_dma_func1_alias);
 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_MARVELL_EXT, 0x9182,
			 quirk_dma_func1_alias);
 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_MARVELL_EXT, 0x9183,
			 quirk_dma_func1_alias);
 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_MARVELL_EXT, 0x91a0,
			 quirk_dma_func1_alias);
 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_MARVELL_EXT, 0x9215,
			 quirk_dma_func1_alias);
 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_MARVELL_EXT, 0x9220,
			 quirk_dma_func1_alias);
 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_MARVELL_EXT, 0x9230,
			 quirk_dma_func1_alias);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_MARVELL_EXT, 0x9235,
			 quirk_dma_func1_alias);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_TTI, 0x0642,
			 quirk_dma_func1_alias);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_TTI, 0x0645,
			 quirk_dma_func1_alias);
 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_JMICRON,
			 PCI_DEVICE_ID_JMICRON_JMB388_ESD,
			 quirk_dma_func1_alias);
 
DECLARE_PCI_FIXUP_HEADER(0x1c28,  
			 0x0122,  
			 quirk_dma_func1_alias);

 
static const struct pci_device_id fixed_dma_alias_tbl[] = {
	{ PCI_DEVICE_SUB(PCI_VENDOR_ID_ADAPTEC2, 0x0285,
			 PCI_VENDOR_ID_ADAPTEC2, 0x02bb),  
	  .driver_data = PCI_DEVFN(1, 0) },
	{ PCI_DEVICE_SUB(PCI_VENDOR_ID_ADAPTEC2, 0x0285,
			 PCI_VENDOR_ID_ADAPTEC2, 0x02bc),  
	  .driver_data = PCI_DEVFN(1, 0) },
	{ 0 }
};

static void quirk_fixed_dma_alias(struct pci_dev *dev)
{
	const struct pci_device_id *id;

	id = pci_match_id(fixed_dma_alias_tbl, dev);
	if (id)
		pci_add_dma_alias(dev, id->driver_data, 1);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_ADAPTEC2, 0x0285, quirk_fixed_dma_alias);

 
static void quirk_use_pcie_bridge_dma_alias(struct pci_dev *pdev)
{
	if (!pci_is_root_bus(pdev->bus) &&
	    pdev->hdr_type == PCI_HEADER_TYPE_BRIDGE &&
	    !pci_is_pcie(pdev) && pci_is_pcie(pdev->bus->self) &&
	    pci_pcie_type(pdev->bus->self) != PCI_EXP_TYPE_PCI_BRIDGE)
		pdev->dev_flags |= PCI_DEV_FLAG_PCIE_BRIDGE_ALIAS;
}
 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_ASMEDIA, 0x1080,
			 quirk_use_pcie_bridge_dma_alias);
 
DECLARE_PCI_FIXUP_HEADER(0x10e3, 0x8113, quirk_use_pcie_bridge_dma_alias);
 
DECLARE_PCI_FIXUP_HEADER(0x1283, 0x8892, quirk_use_pcie_bridge_dma_alias);
 
DECLARE_PCI_FIXUP_HEADER(0x1283, 0x8893, quirk_use_pcie_bridge_dma_alias);
 
DECLARE_PCI_FIXUP_HEADER(0x8086, 0x244e, quirk_use_pcie_bridge_dma_alias);

 
static void quirk_mic_x200_dma_alias(struct pci_dev *pdev)
{
	pci_add_dma_alias(pdev, PCI_DEVFN(0x10, 0x0), 1);
	pci_add_dma_alias(pdev, PCI_DEVFN(0x11, 0x0), 1);
	pci_add_dma_alias(pdev, PCI_DEVFN(0x12, 0x3), 1);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x2260, quirk_mic_x200_dma_alias);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x2264, quirk_mic_x200_dma_alias);

 
static void quirk_pex_vca_alias(struct pci_dev *pdev)
{
	const unsigned int num_pci_slots = 0x20;
	unsigned int slot;

	for (slot = 0; slot < num_pci_slots; slot++)
		pci_add_dma_alias(pdev, PCI_DEVFN(slot, 0x0), 5);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x2954, quirk_pex_vca_alias);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x2955, quirk_pex_vca_alias);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x2956, quirk_pex_vca_alias);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x2958, quirk_pex_vca_alias);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x2959, quirk_pex_vca_alias);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x295A, quirk_pex_vca_alias);

 
static void quirk_bridge_cavm_thrx2_pcie_root(struct pci_dev *pdev)
{
	pdev->dev_flags |= PCI_DEV_FLAGS_BRIDGE_XLATE_ROOT;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_BROADCOM, 0x9000,
				quirk_bridge_cavm_thrx2_pcie_root);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_BROADCOM, 0x9084,
				quirk_bridge_cavm_thrx2_pcie_root);

 
static void quirk_tw686x_class(struct pci_dev *pdev)
{
	u32 class = pdev->class;

	 
	pdev->class = (PCI_CLASS_MULTIMEDIA_OTHER << 8) | 0x01;
	pci_info(pdev, "TW686x PCI class overridden (%#08x -> %#08x)\n",
		 class, pdev->class);
}
DECLARE_PCI_FIXUP_CLASS_EARLY(0x1797, 0x6864, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_tw686x_class);
DECLARE_PCI_FIXUP_CLASS_EARLY(0x1797, 0x6865, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_tw686x_class);
DECLARE_PCI_FIXUP_CLASS_EARLY(0x1797, 0x6868, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_tw686x_class);
DECLARE_PCI_FIXUP_CLASS_EARLY(0x1797, 0x6869, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_tw686x_class);

 
static void quirk_relaxedordering_disable(struct pci_dev *dev)
{
	dev->dev_flags |= PCI_DEV_FLAGS_NO_RELAXED_ORDERING;
	pci_info(dev, "Disable Relaxed Ordering Attributes to avoid PCIe Completion erratum\n");
}

 
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x6f01, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x6f02, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x6f03, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x6f04, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x6f05, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x6f06, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x6f07, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x6f08, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x6f09, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x6f0a, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x6f0b, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x6f0c, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x6f0d, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x6f0e, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x2f01, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x2f02, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x2f03, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x2f04, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x2f05, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x2f06, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x2f07, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x2f08, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x2f09, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x2f0a, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x2f0b, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x2f0c, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x2f0d, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_INTEL, 0x2f0e, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);

 
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_AMD, 0x1a00, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_AMD, 0x1a01, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_AMD, 0x1a02, PCI_CLASS_NOT_DEFINED, 8,
			      quirk_relaxedordering_disable);

 
static void quirk_disable_root_port_attributes(struct pci_dev *pdev)
{
	struct pci_dev *root_port = pcie_find_root_port(pdev);

	if (!root_port) {
		pci_warn(pdev, "PCIe Completion erratum may cause device errors\n");
		return;
	}

	pci_info(root_port, "Disabling No Snoop/Relaxed Ordering Attributes to avoid PCIe Completion erratum in %s\n",
		 dev_name(&pdev->dev));
	pcie_capability_clear_and_set_word(root_port, PCI_EXP_DEVCTL,
					   PCI_EXP_DEVCTL_RELAX_EN |
					   PCI_EXP_DEVCTL_NOSNOOP_EN, 0);
}

 
static void quirk_chelsio_T5_disable_root_port_attributes(struct pci_dev *pdev)
{
	 
	if ((pdev->device & 0xff00) == 0x5400)
		quirk_disable_root_port_attributes(pdev);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_CHELSIO, PCI_ANY_ID,
			 quirk_chelsio_T5_disable_root_port_attributes);

 
static int pci_acs_ctrl_enabled(u16 acs_ctrl_req, u16 acs_ctrl_ena)
{
	if ((acs_ctrl_req & acs_ctrl_ena) == acs_ctrl_req)
		return 1;
	return 0;
}

 
static int pci_quirk_amd_sb_acs(struct pci_dev *dev, u16 acs_flags)
{
#ifdef CONFIG_ACPI
	struct acpi_table_header *header = NULL;
	acpi_status status;

	 
	if (!dev->multifunction || !pci_is_root_bus(dev->bus))
		return -ENODEV;

	 
	status = acpi_get_table("IVRS", 0, &header);
	if (ACPI_FAILURE(status))
		return -ENODEV;

	acpi_put_table(header);

	 
	acs_flags &= (PCI_ACS_RR | PCI_ACS_CR | PCI_ACS_EC | PCI_ACS_DT);

	return pci_acs_ctrl_enabled(acs_flags, PCI_ACS_RR | PCI_ACS_CR);
#else
	return -ENODEV;
#endif
}

static bool pci_quirk_cavium_acs_match(struct pci_dev *dev)
{
	if (!pci_is_pcie(dev) || pci_pcie_type(dev) != PCI_EXP_TYPE_ROOT_PORT)
		return false;

	switch (dev->device) {
	 
	case 0xa000 ... 0xa7ff:  
	case 0xaf84:   
	case 0xb884:   
		return true;
	default:
		return false;
	}
}

static int pci_quirk_cavium_acs(struct pci_dev *dev, u16 acs_flags)
{
	if (!pci_quirk_cavium_acs_match(dev))
		return -ENOTTY;

	 
	return pci_acs_ctrl_enabled(acs_flags,
		PCI_ACS_SV | PCI_ACS_RR | PCI_ACS_CR | PCI_ACS_UF);
}

static int pci_quirk_xgene_acs(struct pci_dev *dev, u16 acs_flags)
{
	 
	return pci_acs_ctrl_enabled(acs_flags,
		PCI_ACS_SV | PCI_ACS_RR | PCI_ACS_CR | PCI_ACS_UF);
}

 
static int pci_quirk_zhaoxin_pcie_ports_acs(struct pci_dev *dev, u16 acs_flags)
{
	if (!pci_is_pcie(dev) ||
	    ((pci_pcie_type(dev) != PCI_EXP_TYPE_ROOT_PORT) &&
	     (pci_pcie_type(dev) != PCI_EXP_TYPE_DOWNSTREAM)))
		return -ENOTTY;

	 
	switch (dev->device) {
	case 0x0710 ... 0x071e:
	case 0x0721:
	case 0x0723 ... 0x0752:
		return pci_acs_ctrl_enabled(acs_flags,
			PCI_ACS_SV | PCI_ACS_RR | PCI_ACS_CR | PCI_ACS_UF);
	}

	return false;
}

 
static const u16 pci_quirk_intel_pch_acs_ids[] = {
	 
	0x3b42, 0x3b43, 0x3b44, 0x3b45, 0x3b46, 0x3b47, 0x3b48, 0x3b49,
	0x3b4a, 0x3b4b, 0x3b4c, 0x3b4d, 0x3b4e, 0x3b4f, 0x3b50, 0x3b51,
	 
	0x1c10, 0x1c11, 0x1c12, 0x1c13, 0x1c14, 0x1c15, 0x1c16, 0x1c17,
	0x1c18, 0x1c19, 0x1c1a, 0x1c1b, 0x1c1c, 0x1c1d, 0x1c1e, 0x1c1f,
	 
	0x1e10, 0x1e11, 0x1e12, 0x1e13, 0x1e14, 0x1e15, 0x1e16, 0x1e17,
	0x1e18, 0x1e19, 0x1e1a, 0x1e1b, 0x1e1c, 0x1e1d, 0x1e1e, 0x1e1f,
	 
	0x8c10, 0x8c11, 0x8c12, 0x8c13, 0x8c14, 0x8c15, 0x8c16, 0x8c17,
	0x8c18, 0x8c19, 0x8c1a, 0x8c1b, 0x8c1c, 0x8c1d, 0x8c1e, 0x8c1f,
	 
	0x9c10, 0x9c11, 0x9c12, 0x9c13, 0x9c14, 0x9c15, 0x9c16, 0x9c17,
	0x9c18, 0x9c19, 0x9c1a, 0x9c1b,
	 
	0x9c90, 0x9c91, 0x9c92, 0x9c93, 0x9c94, 0x9c95, 0x9c96, 0x9c97,
	0x9c98, 0x9c99, 0x9c9a, 0x9c9b,
	 
	0x1d10, 0x1d12, 0x1d14, 0x1d16, 0x1d18, 0x1d1a, 0x1d1c, 0x1d1e,
	 
	0x8d10, 0x8d11, 0x8d12, 0x8d13, 0x8d14, 0x8d15, 0x8d16, 0x8d17,
	0x8d18, 0x8d19, 0x8d1a, 0x8d1b, 0x8d1c, 0x8d1d, 0x8d1e,
	 
	0x8c90, 0x8c92, 0x8c94, 0x8c96, 0x8c98, 0x8c9a, 0x8c9c, 0x8c9e,
};

static bool pci_quirk_intel_pch_acs_match(struct pci_dev *dev)
{
	int i;

	 
	if (!pci_is_pcie(dev) || pci_pcie_type(dev) != PCI_EXP_TYPE_ROOT_PORT)
		return false;

	for (i = 0; i < ARRAY_SIZE(pci_quirk_intel_pch_acs_ids); i++)
		if (pci_quirk_intel_pch_acs_ids[i] == dev->device)
			return true;

	return false;
}

static int pci_quirk_intel_pch_acs(struct pci_dev *dev, u16 acs_flags)
{
	if (!pci_quirk_intel_pch_acs_match(dev))
		return -ENOTTY;

	if (dev->dev_flags & PCI_DEV_FLAGS_ACS_ENABLED_QUIRK)
		return pci_acs_ctrl_enabled(acs_flags,
			PCI_ACS_SV | PCI_ACS_RR | PCI_ACS_CR | PCI_ACS_UF);

	return pci_acs_ctrl_enabled(acs_flags, 0);
}

 
static int pci_quirk_qcom_rp_acs(struct pci_dev *dev, u16 acs_flags)
{
	return pci_acs_ctrl_enabled(acs_flags,
		PCI_ACS_SV | PCI_ACS_RR | PCI_ACS_CR | PCI_ACS_UF);
}

 
static int pci_quirk_nxp_rp_acs(struct pci_dev *dev, u16 acs_flags)
{
	return pci_acs_ctrl_enabled(acs_flags,
		PCI_ACS_SV | PCI_ACS_RR | PCI_ACS_CR | PCI_ACS_UF);
}

static int pci_quirk_al_acs(struct pci_dev *dev, u16 acs_flags)
{
	if (pci_pcie_type(dev) != PCI_EXP_TYPE_ROOT_PORT)
		return -ENOTTY;

	 
	acs_flags &= ~(PCI_ACS_SV | PCI_ACS_RR | PCI_ACS_CR | PCI_ACS_UF);

	return acs_flags ? 0 : 1;
}

 
static bool pci_quirk_intel_spt_pch_acs_match(struct pci_dev *dev)
{
	if (!pci_is_pcie(dev) || pci_pcie_type(dev) != PCI_EXP_TYPE_ROOT_PORT)
		return false;

	switch (dev->device) {
	case 0xa110 ... 0xa11f: case 0xa167 ... 0xa16a:  
	case 0xa290 ... 0xa29f: case 0xa2e7 ... 0xa2ee:  
	case 0x9d10 ... 0x9d1b:  
		return true;
	}

	return false;
}

#define INTEL_SPT_ACS_CTRL (PCI_ACS_CAP + 4)

static int pci_quirk_intel_spt_pch_acs(struct pci_dev *dev, u16 acs_flags)
{
	int pos;
	u32 cap, ctrl;

	if (!pci_quirk_intel_spt_pch_acs_match(dev))
		return -ENOTTY;

	pos = dev->acs_cap;
	if (!pos)
		return -ENOTTY;

	 
	pci_read_config_dword(dev, pos + PCI_ACS_CAP, &cap);
	acs_flags &= (cap | PCI_ACS_EC);

	pci_read_config_dword(dev, pos + INTEL_SPT_ACS_CTRL, &ctrl);

	return pci_acs_ctrl_enabled(acs_flags, ctrl);
}

static int pci_quirk_mf_endpoint_acs(struct pci_dev *dev, u16 acs_flags)
{
	 
	return pci_acs_ctrl_enabled(acs_flags,
		PCI_ACS_SV | PCI_ACS_TB | PCI_ACS_RR |
		PCI_ACS_CR | PCI_ACS_UF | PCI_ACS_DT);
}

static int pci_quirk_rciep_acs(struct pci_dev *dev, u16 acs_flags)
{
	 
	if (pci_pcie_type(dev) != PCI_EXP_TYPE_RC_END)
		return -ENOTTY;

	return pci_acs_ctrl_enabled(acs_flags,
		PCI_ACS_SV | PCI_ACS_RR | PCI_ACS_CR | PCI_ACS_UF);
}

static int pci_quirk_brcm_acs(struct pci_dev *dev, u16 acs_flags)
{
	 
	return pci_acs_ctrl_enabled(acs_flags,
		PCI_ACS_SV | PCI_ACS_RR | PCI_ACS_CR | PCI_ACS_UF);
}

 
static int  pci_quirk_wangxun_nic_acs(struct pci_dev *dev, u16 acs_flags)
{
	switch (dev->device) {
	case 0x0100 ... 0x010F:
	case 0x1001:
	case 0x2001:
		return pci_acs_ctrl_enabled(acs_flags,
			PCI_ACS_SV | PCI_ACS_RR | PCI_ACS_CR | PCI_ACS_UF);
	}

	return false;
}

static const struct pci_dev_acs_enabled {
	u16 vendor;
	u16 device;
	int (*acs_enabled)(struct pci_dev *dev, u16 acs_flags);
} pci_dev_acs_enabled[] = {
	{ PCI_VENDOR_ID_ATI, 0x4385, pci_quirk_amd_sb_acs },
	{ PCI_VENDOR_ID_ATI, 0x439c, pci_quirk_amd_sb_acs },
	{ PCI_VENDOR_ID_ATI, 0x4383, pci_quirk_amd_sb_acs },
	{ PCI_VENDOR_ID_ATI, 0x439d, pci_quirk_amd_sb_acs },
	{ PCI_VENDOR_ID_ATI, 0x4384, pci_quirk_amd_sb_acs },
	{ PCI_VENDOR_ID_ATI, 0x4399, pci_quirk_amd_sb_acs },
	{ PCI_VENDOR_ID_AMD, 0x780f, pci_quirk_amd_sb_acs },
	{ PCI_VENDOR_ID_AMD, 0x7809, pci_quirk_amd_sb_acs },
	{ PCI_VENDOR_ID_SOLARFLARE, 0x0903, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_SOLARFLARE, 0x0923, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_SOLARFLARE, 0x0A03, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x10C6, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x10DB, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x10DD, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x10E1, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x10F1, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x10F7, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x10F8, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x10F9, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x10FA, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x10FB, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x10FC, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x1507, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x1514, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x151C, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x1529, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x152A, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x154D, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x154F, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x1551, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x1558, pci_quirk_mf_endpoint_acs },
	 
	{ PCI_VENDOR_ID_INTEL, 0x1509, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x150E, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x150F, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x1510, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x1511, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x1516, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x1527, pci_quirk_mf_endpoint_acs },
	 
	{ PCI_VENDOR_ID_INTEL, 0x10C9, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x10E6, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x10E7, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x10E8, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x150A, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x150D, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x1518, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x1526, pci_quirk_mf_endpoint_acs },
	 
	{ PCI_VENDOR_ID_INTEL, 0x10A7, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x10A9, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x10D6, pci_quirk_mf_endpoint_acs },
	 
	{ PCI_VENDOR_ID_INTEL, 0x1521, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x1522, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x1523, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x1524, pci_quirk_mf_endpoint_acs },
	 
	{ PCI_VENDOR_ID_INTEL, 0x105E, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x105F, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x1060, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x10D9, pci_quirk_mf_endpoint_acs },
	 
	{ PCI_VENDOR_ID_INTEL, 0x15b7, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, 0x15b8, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_INTEL, PCI_ANY_ID, pci_quirk_rciep_acs },
	 
	{ PCI_VENDOR_ID_QCOM, 0x0400, pci_quirk_qcom_rp_acs },
	{ PCI_VENDOR_ID_QCOM, 0x0401, pci_quirk_qcom_rp_acs },
	 
	{ PCI_VENDOR_ID_HXT, 0x0401, pci_quirk_qcom_rp_acs },
	 
	{ PCI_VENDOR_ID_INTEL, PCI_ANY_ID, pci_quirk_intel_pch_acs },
	{ PCI_VENDOR_ID_INTEL, PCI_ANY_ID, pci_quirk_intel_spt_pch_acs },
	{ 0x19a2, 0x710, pci_quirk_mf_endpoint_acs },  
	{ 0x10df, 0x720, pci_quirk_mf_endpoint_acs },  
	 
	{ PCI_VENDOR_ID_CAVIUM, PCI_ANY_ID, pci_quirk_cavium_acs },
	 
	{ PCI_VENDOR_ID_CAVIUM, 0xA026, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_CAVIUM, 0xA059, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_CAVIUM, 0xA060, pci_quirk_mf_endpoint_acs },
	 
	{ PCI_VENDOR_ID_AMCC, 0xE004, pci_quirk_xgene_acs },
	 
	{ PCI_VENDOR_ID_AMPERE, 0xE005, pci_quirk_xgene_acs },
	{ PCI_VENDOR_ID_AMPERE, 0xE006, pci_quirk_xgene_acs },
	{ PCI_VENDOR_ID_AMPERE, 0xE007, pci_quirk_xgene_acs },
	{ PCI_VENDOR_ID_AMPERE, 0xE008, pci_quirk_xgene_acs },
	{ PCI_VENDOR_ID_AMPERE, 0xE009, pci_quirk_xgene_acs },
	{ PCI_VENDOR_ID_AMPERE, 0xE00A, pci_quirk_xgene_acs },
	{ PCI_VENDOR_ID_AMPERE, 0xE00B, pci_quirk_xgene_acs },
	{ PCI_VENDOR_ID_AMPERE, 0xE00C, pci_quirk_xgene_acs },
	 
	{ PCI_VENDOR_ID_BROADCOM, 0x16D7, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_BROADCOM, 0x1750, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_BROADCOM, 0x1751, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_BROADCOM, 0x1752, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_BROADCOM, 0xD714, pci_quirk_brcm_acs },
	 
	{ PCI_VENDOR_ID_AMAZON_ANNAPURNA_LABS, 0x0031, pci_quirk_al_acs },
	 
	{ PCI_VENDOR_ID_ZHAOXIN, 0x3038, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_ZHAOXIN, 0x3104, pci_quirk_mf_endpoint_acs },
	{ PCI_VENDOR_ID_ZHAOXIN, 0x9083, pci_quirk_mf_endpoint_acs },
	 
	 
	{ PCI_VENDOR_ID_NXP, 0x8d81, pci_quirk_nxp_rp_acs },
	{ PCI_VENDOR_ID_NXP, 0x8da1, pci_quirk_nxp_rp_acs },
	{ PCI_VENDOR_ID_NXP, 0x8d83, pci_quirk_nxp_rp_acs },
	 
	{ PCI_VENDOR_ID_NXP, 0x8d80, pci_quirk_nxp_rp_acs },
	{ PCI_VENDOR_ID_NXP, 0x8da0, pci_quirk_nxp_rp_acs },
	{ PCI_VENDOR_ID_NXP, 0x8d82, pci_quirk_nxp_rp_acs },
	 
	{ PCI_VENDOR_ID_NXP, 0x8d90, pci_quirk_nxp_rp_acs },
	{ PCI_VENDOR_ID_NXP, 0x8db0, pci_quirk_nxp_rp_acs },
	{ PCI_VENDOR_ID_NXP, 0x8d92, pci_quirk_nxp_rp_acs },
	 
	{ PCI_VENDOR_ID_NXP, 0x8d91, pci_quirk_nxp_rp_acs },
	{ PCI_VENDOR_ID_NXP, 0x8db1, pci_quirk_nxp_rp_acs },
	{ PCI_VENDOR_ID_NXP, 0x8d93, pci_quirk_nxp_rp_acs },
	 
	{ PCI_VENDOR_ID_NXP, 0x8d89, pci_quirk_nxp_rp_acs },
	{ PCI_VENDOR_ID_NXP, 0x8da9, pci_quirk_nxp_rp_acs },
	{ PCI_VENDOR_ID_NXP, 0x8d8b, pci_quirk_nxp_rp_acs },
	 
	{ PCI_VENDOR_ID_NXP, 0x8d88, pci_quirk_nxp_rp_acs },
	{ PCI_VENDOR_ID_NXP, 0x8da8, pci_quirk_nxp_rp_acs },
	{ PCI_VENDOR_ID_NXP, 0x8d8a, pci_quirk_nxp_rp_acs },
	 
	{ PCI_VENDOR_ID_NXP, 0x8d98, pci_quirk_nxp_rp_acs },
	{ PCI_VENDOR_ID_NXP, 0x8db8, pci_quirk_nxp_rp_acs },
	{ PCI_VENDOR_ID_NXP, 0x8d9a, pci_quirk_nxp_rp_acs },
	 
	{ PCI_VENDOR_ID_NXP, 0x8d99, pci_quirk_nxp_rp_acs },
	{ PCI_VENDOR_ID_NXP, 0x8db9, pci_quirk_nxp_rp_acs },
	{ PCI_VENDOR_ID_NXP, 0x8d9b, pci_quirk_nxp_rp_acs },
	 
	{ PCI_VENDOR_ID_ZHAOXIN, PCI_ANY_ID, pci_quirk_zhaoxin_pcie_ports_acs },
	 
	{ PCI_VENDOR_ID_WANGXUN, PCI_ANY_ID, pci_quirk_wangxun_nic_acs },
	{ 0 }
};

 
int pci_dev_specific_acs_enabled(struct pci_dev *dev, u16 acs_flags)
{
	const struct pci_dev_acs_enabled *i;
	int ret;

	 
	for (i = pci_dev_acs_enabled; i->acs_enabled; i++) {
		if ((i->vendor == dev->vendor ||
		     i->vendor == (u16)PCI_ANY_ID) &&
		    (i->device == dev->device ||
		     i->device == (u16)PCI_ANY_ID)) {
			ret = i->acs_enabled(dev, acs_flags);
			if (ret >= 0)
				return ret;
		}
	}

	return -ENOTTY;
}

 
#define INTEL_LPC_RCBA_REG 0xf0
 
#define INTEL_LPC_RCBA_MASK 0xffffc000
 
#define INTEL_LPC_RCBA_ENABLE (1 << 0)

 
#define INTEL_BSPR_REG 0x1104
 
#define INTEL_BSPR_REG_BPNPD (1 << 8)
 
#define INTEL_BSPR_REG_BPPD  (1 << 9)

 
#define INTEL_UPDCR_REG 0x1014
 
#define INTEL_UPDCR_REG_MASK 0x3f

static int pci_quirk_enable_intel_lpc_acs(struct pci_dev *dev)
{
	u32 rcba, bspr, updcr;
	void __iomem *rcba_mem;

	 
	pci_bus_read_config_dword(dev->bus, PCI_DEVFN(31, 0),
				  INTEL_LPC_RCBA_REG, &rcba);
	if (!(rcba & INTEL_LPC_RCBA_ENABLE))
		return -EINVAL;

	rcba_mem = ioremap(rcba & INTEL_LPC_RCBA_MASK,
				   PAGE_ALIGN(INTEL_UPDCR_REG));
	if (!rcba_mem)
		return -ENOMEM;

	 
	bspr = readl(rcba_mem + INTEL_BSPR_REG);
	bspr &= INTEL_BSPR_REG_BPNPD | INTEL_BSPR_REG_BPPD;
	if (bspr != (INTEL_BSPR_REG_BPNPD | INTEL_BSPR_REG_BPPD)) {
		updcr = readl(rcba_mem + INTEL_UPDCR_REG);
		if (updcr & INTEL_UPDCR_REG_MASK) {
			pci_info(dev, "Disabling UPDCR peer decodes\n");
			updcr &= ~INTEL_UPDCR_REG_MASK;
			writel(updcr, rcba_mem + INTEL_UPDCR_REG);
		}
	}

	iounmap(rcba_mem);
	return 0;
}

 
#define INTEL_MPC_REG 0xd8
 
#define INTEL_MPC_REG_IRBNCE (1 << 26)

static void pci_quirk_enable_intel_rp_mpc_acs(struct pci_dev *dev)
{
	u32 mpc;

	 
	pci_read_config_dword(dev, INTEL_MPC_REG, &mpc);
	if (!(mpc & INTEL_MPC_REG_IRBNCE)) {
		pci_info(dev, "Enabling MPC IRBNCE\n");
		mpc |= INTEL_MPC_REG_IRBNCE;
		pci_write_config_word(dev, INTEL_MPC_REG, mpc);
	}
}

 
static int pci_quirk_enable_intel_pch_acs(struct pci_dev *dev)
{
	if (!pci_quirk_intel_pch_acs_match(dev))
		return -ENOTTY;

	if (pci_quirk_enable_intel_lpc_acs(dev)) {
		pci_warn(dev, "Failed to enable Intel PCH ACS quirk\n");
		return 0;
	}

	pci_quirk_enable_intel_rp_mpc_acs(dev);

	dev->dev_flags |= PCI_DEV_FLAGS_ACS_ENABLED_QUIRK;

	pci_info(dev, "Intel PCH root port ACS workaround enabled\n");

	return 0;
}

static int pci_quirk_enable_intel_spt_pch_acs(struct pci_dev *dev)
{
	int pos;
	u32 cap, ctrl;

	if (!pci_quirk_intel_spt_pch_acs_match(dev))
		return -ENOTTY;

	pos = dev->acs_cap;
	if (!pos)
		return -ENOTTY;

	pci_read_config_dword(dev, pos + PCI_ACS_CAP, &cap);
	pci_read_config_dword(dev, pos + INTEL_SPT_ACS_CTRL, &ctrl);

	ctrl |= (cap & PCI_ACS_SV);
	ctrl |= (cap & PCI_ACS_RR);
	ctrl |= (cap & PCI_ACS_CR);
	ctrl |= (cap & PCI_ACS_UF);

	if (pci_ats_disabled() || dev->external_facing || dev->untrusted)
		ctrl |= (cap & PCI_ACS_TB);

	pci_write_config_dword(dev, pos + INTEL_SPT_ACS_CTRL, ctrl);

	pci_info(dev, "Intel SPT PCH root port ACS workaround enabled\n");

	return 0;
}

static int pci_quirk_disable_intel_spt_pch_acs_redir(struct pci_dev *dev)
{
	int pos;
	u32 cap, ctrl;

	if (!pci_quirk_intel_spt_pch_acs_match(dev))
		return -ENOTTY;

	pos = dev->acs_cap;
	if (!pos)
		return -ENOTTY;

	pci_read_config_dword(dev, pos + PCI_ACS_CAP, &cap);
	pci_read_config_dword(dev, pos + INTEL_SPT_ACS_CTRL, &ctrl);

	ctrl &= ~(PCI_ACS_RR | PCI_ACS_CR | PCI_ACS_EC);

	pci_write_config_dword(dev, pos + INTEL_SPT_ACS_CTRL, ctrl);

	pci_info(dev, "Intel SPT PCH root port workaround: disabled ACS redirect\n");

	return 0;
}

static const struct pci_dev_acs_ops {
	u16 vendor;
	u16 device;
	int (*enable_acs)(struct pci_dev *dev);
	int (*disable_acs_redir)(struct pci_dev *dev);
} pci_dev_acs_ops[] = {
	{ PCI_VENDOR_ID_INTEL, PCI_ANY_ID,
	    .enable_acs = pci_quirk_enable_intel_pch_acs,
	},
	{ PCI_VENDOR_ID_INTEL, PCI_ANY_ID,
	    .enable_acs = pci_quirk_enable_intel_spt_pch_acs,
	    .disable_acs_redir = pci_quirk_disable_intel_spt_pch_acs_redir,
	},
};

int pci_dev_specific_enable_acs(struct pci_dev *dev)
{
	const struct pci_dev_acs_ops *p;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(pci_dev_acs_ops); i++) {
		p = &pci_dev_acs_ops[i];
		if ((p->vendor == dev->vendor ||
		     p->vendor == (u16)PCI_ANY_ID) &&
		    (p->device == dev->device ||
		     p->device == (u16)PCI_ANY_ID) &&
		    p->enable_acs) {
			ret = p->enable_acs(dev);
			if (ret >= 0)
				return ret;
		}
	}

	return -ENOTTY;
}

int pci_dev_specific_disable_acs_redir(struct pci_dev *dev)
{
	const struct pci_dev_acs_ops *p;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(pci_dev_acs_ops); i++) {
		p = &pci_dev_acs_ops[i];
		if ((p->vendor == dev->vendor ||
		     p->vendor == (u16)PCI_ANY_ID) &&
		    (p->device == dev->device ||
		     p->device == (u16)PCI_ANY_ID) &&
		    p->disable_acs_redir) {
			ret = p->disable_acs_redir(dev);
			if (ret >= 0)
				return ret;
		}
	}

	return -ENOTTY;
}

 
static void quirk_intel_qat_vf_cap(struct pci_dev *pdev)
{
	int pos, i = 0, ret;
	u8 next_cap;
	u16 reg16, *cap;
	struct pci_cap_saved_state *state;

	 
	if (pdev->pcie_cap || pci_find_capability(pdev, PCI_CAP_ID_EXP))
		return;

	 
	pos = pci_find_capability(pdev, PCI_CAP_ID_MSI);
	if (!pos)
		return;

	 
	pci_read_config_byte(pdev, pos + 1, &next_cap);
	if (next_cap)
		return;

	 
	pos = 0x50;
	pci_read_config_word(pdev, pos, &reg16);
	if (reg16 == (0x0000 | PCI_CAP_ID_EXP)) {
		u32 status;
#ifndef PCI_EXP_SAVE_REGS
#define PCI_EXP_SAVE_REGS     7
#endif
		int size = PCI_EXP_SAVE_REGS * sizeof(u16);

		pdev->pcie_cap = pos;
		pci_read_config_word(pdev, pos + PCI_EXP_FLAGS, &reg16);
		pdev->pcie_flags_reg = reg16;
		pci_read_config_word(pdev, pos + PCI_EXP_DEVCAP, &reg16);
		pdev->pcie_mpss = reg16 & PCI_EXP_DEVCAP_PAYLOAD;

		pdev->cfg_size = PCI_CFG_SPACE_EXP_SIZE;
		ret = pci_read_config_dword(pdev, PCI_CFG_SPACE_SIZE, &status);
		if ((ret != PCIBIOS_SUCCESSFUL) || (PCI_POSSIBLE_ERROR(status)))
			pdev->cfg_size = PCI_CFG_SPACE_SIZE;

		if (pci_find_saved_cap(pdev, PCI_CAP_ID_EXP))
			return;

		 
		state = kzalloc(sizeof(*state) + size, GFP_KERNEL);
		if (!state)
			return;

		state->cap.cap_nr = PCI_CAP_ID_EXP;
		state->cap.cap_extended = 0;
		state->cap.size = size;
		cap = (u16 *)&state->cap.data[0];
		pcie_capability_read_word(pdev, PCI_EXP_DEVCTL, &cap[i++]);
		pcie_capability_read_word(pdev, PCI_EXP_LNKCTL, &cap[i++]);
		pcie_capability_read_word(pdev, PCI_EXP_SLTCTL, &cap[i++]);
		pcie_capability_read_word(pdev, PCI_EXP_RTCTL,  &cap[i++]);
		pcie_capability_read_word(pdev, PCI_EXP_DEVCTL2, &cap[i++]);
		pcie_capability_read_word(pdev, PCI_EXP_LNKCTL2, &cap[i++]);
		pcie_capability_read_word(pdev, PCI_EXP_SLTCTL2, &cap[i++]);
		hlist_add_head(&state->next, &pdev->saved_cap_space);
	}
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x443, quirk_intel_qat_vf_cap);

 
static void quirk_no_flr(struct pci_dev *dev)
{
	dev->dev_flags |= PCI_DEV_FLAGS_NO_FLR_RESET;
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_AMD, 0x1487, quirk_no_flr);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_AMD, 0x148c, quirk_no_flr);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_AMD, 0x149c, quirk_no_flr);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_AMD, 0x7901, quirk_no_flr);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x1502, quirk_no_flr);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x1503, quirk_no_flr);

 
static void quirk_no_flr_snet(struct pci_dev *dev)
{
	if (dev->revision == 0x1)
		quirk_no_flr(dev);
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_SOLIDRUN, 0x1000, quirk_no_flr_snet);

static void quirk_no_ext_tags(struct pci_dev *pdev)
{
	struct pci_host_bridge *bridge = pci_find_host_bridge(pdev->bus);

	if (!bridge)
		return;

	bridge->no_ext_tags = 1;
	pci_info(pdev, "disabling Extended Tags (this device can't handle them)\n");

	pci_walk_bus(bridge->bus, pci_configure_extended_tags, NULL);
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_SERVERWORKS, 0x0132, quirk_no_ext_tags);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_SERVERWORKS, 0x0140, quirk_no_ext_tags);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_SERVERWORKS, 0x0141, quirk_no_ext_tags);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_SERVERWORKS, 0x0142, quirk_no_ext_tags);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_SERVERWORKS, 0x0144, quirk_no_ext_tags);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_SERVERWORKS, 0x0420, quirk_no_ext_tags);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_SERVERWORKS, 0x0422, quirk_no_ext_tags);

#ifdef CONFIG_PCI_ATS
static void quirk_no_ats(struct pci_dev *pdev)
{
	pci_info(pdev, "disabling ATS\n");
	pdev->ats_cap = 0;
}

 
static void quirk_amd_harvest_no_ats(struct pci_dev *pdev)
{
	if (pdev->device == 0x15d8) {
		if (pdev->revision == 0xcf &&
		    pdev->subsystem_vendor == 0xea50 &&
		    (pdev->subsystem_device == 0xce19 ||
		     pdev->subsystem_device == 0xcc10 ||
		     pdev->subsystem_device == 0xcc08))
			quirk_no_ats(pdev);
	} else {
		quirk_no_ats(pdev);
	}
}

 
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x98e4, quirk_amd_harvest_no_ats);
 
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x6900, quirk_amd_harvest_no_ats);
 
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x7310, quirk_amd_harvest_no_ats);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x7312, quirk_amd_harvest_no_ats);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x7318, quirk_amd_harvest_no_ats);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x7319, quirk_amd_harvest_no_ats);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x731a, quirk_amd_harvest_no_ats);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x731b, quirk_amd_harvest_no_ats);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x731e, quirk_amd_harvest_no_ats);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x731f, quirk_amd_harvest_no_ats);
 
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x7340, quirk_amd_harvest_no_ats);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x7341, quirk_amd_harvest_no_ats);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x7347, quirk_amd_harvest_no_ats);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x734f, quirk_amd_harvest_no_ats);
 
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x15d8, quirk_amd_harvest_no_ats);

 
static void quirk_intel_e2000_no_ats(struct pci_dev *pdev)
{
	if (pdev->revision < 0x20)
		quirk_no_ats(pdev);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1451, quirk_intel_e2000_no_ats);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1452, quirk_intel_e2000_no_ats);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1453, quirk_intel_e2000_no_ats);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1454, quirk_intel_e2000_no_ats);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1455, quirk_intel_e2000_no_ats);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1457, quirk_intel_e2000_no_ats);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1459, quirk_intel_e2000_no_ats);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x145a, quirk_intel_e2000_no_ats);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x145c, quirk_intel_e2000_no_ats);
#endif  

 
static void quirk_fsl_no_msi(struct pci_dev *pdev)
{
	if (pci_pcie_type(pdev) == PCI_EXP_TYPE_ROOT_PORT)
		pdev->no_msi = 1;
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_FREESCALE, PCI_ANY_ID, quirk_fsl_no_msi);

 
static void pci_create_device_link(struct pci_dev *pdev, unsigned int consumer,
				   unsigned int supplier, unsigned int class,
				   unsigned int class_shift)
{
	struct pci_dev *supplier_pdev;

	if (PCI_FUNC(pdev->devfn) != consumer)
		return;

	supplier_pdev = pci_get_domain_bus_and_slot(pci_domain_nr(pdev->bus),
				pdev->bus->number,
				PCI_DEVFN(PCI_SLOT(pdev->devfn), supplier));
	if (!supplier_pdev || (supplier_pdev->class >> class_shift) != class) {
		pci_dev_put(supplier_pdev);
		return;
	}

	if (device_link_add(&pdev->dev, &supplier_pdev->dev,
			    DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME))
		pci_info(pdev, "D0 power state depends on %s\n",
			 pci_name(supplier_pdev));
	else
		pci_err(pdev, "Cannot enforce power dependency on %s\n",
			pci_name(supplier_pdev));

	pm_runtime_allow(&pdev->dev);
	pci_dev_put(supplier_pdev);
}

 
static void quirk_gpu_hda(struct pci_dev *hda)
{
	pci_create_device_link(hda, 1, 0, PCI_BASE_CLASS_DISPLAY, 16);
}
DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_VENDOR_ID_ATI, PCI_ANY_ID,
			      PCI_CLASS_MULTIMEDIA_HD_AUDIO, 8, quirk_gpu_hda);
DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_VENDOR_ID_AMD, PCI_ANY_ID,
			      PCI_CLASS_MULTIMEDIA_HD_AUDIO, 8, quirk_gpu_hda);
DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_VENDOR_ID_NVIDIA, PCI_ANY_ID,
			      PCI_CLASS_MULTIMEDIA_HD_AUDIO, 8, quirk_gpu_hda);

 
static void quirk_gpu_usb(struct pci_dev *usb)
{
	pci_create_device_link(usb, 2, 0, PCI_BASE_CLASS_DISPLAY, 16);
}
DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_VENDOR_ID_NVIDIA, PCI_ANY_ID,
			      PCI_CLASS_SERIAL_USB, 8, quirk_gpu_usb);
DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_VENDOR_ID_ATI, PCI_ANY_ID,
			      PCI_CLASS_SERIAL_USB, 8, quirk_gpu_usb);

 
#define PCI_CLASS_SERIAL_UNKNOWN	0x0c80
static void quirk_gpu_usb_typec_ucsi(struct pci_dev *ucsi)
{
	pci_create_device_link(ucsi, 3, 0, PCI_BASE_CLASS_DISPLAY, 16);
}
DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_VENDOR_ID_NVIDIA, PCI_ANY_ID,
			      PCI_CLASS_SERIAL_UNKNOWN, 8,
			      quirk_gpu_usb_typec_ucsi);
DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_VENDOR_ID_ATI, PCI_ANY_ID,
			      PCI_CLASS_SERIAL_UNKNOWN, 8,
			      quirk_gpu_usb_typec_ucsi);

 
static void quirk_nvidia_hda(struct pci_dev *gpu)
{
	u8 hdr_type;
	u32 val;

	 
	if (gpu->device < PCI_DEVICE_ID_NVIDIA_GEFORCE_320M)
		return;

	 
	pci_read_config_dword(gpu, 0x488, &val);
	if (val & BIT(25))
		return;

	pci_info(gpu, "Enabling HDA controller\n");
	pci_write_config_dword(gpu, 0x488, val | BIT(25));

	 
	pci_read_config_byte(gpu, PCI_HEADER_TYPE, &hdr_type);
	gpu->multifunction = !!(hdr_type & 0x80);
}
DECLARE_PCI_FIXUP_CLASS_HEADER(PCI_VENDOR_ID_NVIDIA, PCI_ANY_ID,
			       PCI_BASE_CLASS_DISPLAY, 16, quirk_nvidia_hda);
DECLARE_PCI_FIXUP_CLASS_RESUME_EARLY(PCI_VENDOR_ID_NVIDIA, PCI_ANY_ID,
			       PCI_BASE_CLASS_DISPLAY, 16, quirk_nvidia_hda);

 
int pci_idt_bus_quirk(struct pci_bus *bus, int devfn, u32 *l, int timeout)
{
	int pos;
	u16 ctrl = 0;
	bool found;
	struct pci_dev *bridge = bus->self;

	pos = bridge->acs_cap;

	 
	if (pos) {
		pci_read_config_word(bridge, pos + PCI_ACS_CTRL, &ctrl);
		if (ctrl & PCI_ACS_SV)
			pci_write_config_word(bridge, pos + PCI_ACS_CTRL,
					      ctrl & ~PCI_ACS_SV);
	}

	found = pci_bus_generic_read_dev_vendor_id(bus, devfn, l, timeout);

	 
	if (found)
		pci_bus_write_config_word(bus, devfn, PCI_VENDOR_ID, 0);

	 
	if (ctrl & PCI_ACS_SV)
		pci_write_config_word(bridge, pos + PCI_ACS_CTRL, ctrl);

	return found;
}

 
static void quirk_switchtec_ntb_dma_alias(struct pci_dev *pdev)
{
	void __iomem *mmio;
	struct ntb_info_regs __iomem *mmio_ntb;
	struct ntb_ctrl_regs __iomem *mmio_ctrl;
	u64 partition_map;
	u8 partition;
	int pp;

	if (pci_enable_device(pdev)) {
		pci_err(pdev, "Cannot enable Switchtec device\n");
		return;
	}

	mmio = pci_iomap(pdev, 0, 0);
	if (mmio == NULL) {
		pci_disable_device(pdev);
		pci_err(pdev, "Cannot iomap Switchtec device\n");
		return;
	}

	pci_info(pdev, "Setting Switchtec proxy ID aliases\n");

	mmio_ntb = mmio + SWITCHTEC_GAS_NTB_OFFSET;
	mmio_ctrl = (void __iomem *) mmio_ntb + SWITCHTEC_NTB_REG_CTRL_OFFSET;

	partition = ioread8(&mmio_ntb->partition_id);

	partition_map = ioread32(&mmio_ntb->ep_map);
	partition_map |= ((u64) ioread32(&mmio_ntb->ep_map + 4)) << 32;
	partition_map &= ~(1ULL << partition);

	for (pp = 0; pp < (sizeof(partition_map) * 8); pp++) {
		struct ntb_ctrl_regs __iomem *mmio_peer_ctrl;
		u32 table_sz = 0;
		int te;

		if (!(partition_map & (1ULL << pp)))
			continue;

		pci_dbg(pdev, "Processing partition %d\n", pp);

		mmio_peer_ctrl = &mmio_ctrl[pp];

		table_sz = ioread16(&mmio_peer_ctrl->req_id_table_size);
		if (!table_sz) {
			pci_warn(pdev, "Partition %d table_sz 0\n", pp);
			continue;
		}

		if (table_sz > 512) {
			pci_warn(pdev,
				 "Invalid Switchtec partition %d table_sz %d\n",
				 pp, table_sz);
			continue;
		}

		for (te = 0; te < table_sz; te++) {
			u32 rid_entry;
			u8 devfn;

			rid_entry = ioread32(&mmio_peer_ctrl->req_id_table[te]);
			devfn = (rid_entry >> 1) & 0xFF;
			pci_dbg(pdev,
				"Aliasing Partition %d Proxy ID %02x.%d\n",
				pp, PCI_SLOT(devfn), PCI_FUNC(devfn));
			pci_add_dma_alias(pdev, devfn, 1);
		}
	}

	pci_iounmap(pdev, mmio);
	pci_disable_device(pdev);
}
#define SWITCHTEC_QUIRK(vid) \
	DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_VENDOR_ID_MICROSEMI, vid, \
		PCI_CLASS_BRIDGE_OTHER, 8, quirk_switchtec_ntb_dma_alias)

SWITCHTEC_QUIRK(0x8531);   
SWITCHTEC_QUIRK(0x8532);   
SWITCHTEC_QUIRK(0x8533);   
SWITCHTEC_QUIRK(0x8534);   
SWITCHTEC_QUIRK(0x8535);   
SWITCHTEC_QUIRK(0x8536);   
SWITCHTEC_QUIRK(0x8541);   
SWITCHTEC_QUIRK(0x8542);   
SWITCHTEC_QUIRK(0x8543);   
SWITCHTEC_QUIRK(0x8544);   
SWITCHTEC_QUIRK(0x8545);   
SWITCHTEC_QUIRK(0x8546);   
SWITCHTEC_QUIRK(0x8551);   
SWITCHTEC_QUIRK(0x8552);   
SWITCHTEC_QUIRK(0x8553);   
SWITCHTEC_QUIRK(0x8554);   
SWITCHTEC_QUIRK(0x8555);   
SWITCHTEC_QUIRK(0x8556);   
SWITCHTEC_QUIRK(0x8561);   
SWITCHTEC_QUIRK(0x8562);   
SWITCHTEC_QUIRK(0x8563);   
SWITCHTEC_QUIRK(0x8564);   
SWITCHTEC_QUIRK(0x8565);   
SWITCHTEC_QUIRK(0x8566);   
SWITCHTEC_QUIRK(0x8571);   
SWITCHTEC_QUIRK(0x8572);   
SWITCHTEC_QUIRK(0x8573);   
SWITCHTEC_QUIRK(0x8574);   
SWITCHTEC_QUIRK(0x8575);   
SWITCHTEC_QUIRK(0x8576);   
SWITCHTEC_QUIRK(0x4000);   
SWITCHTEC_QUIRK(0x4084);   
SWITCHTEC_QUIRK(0x4068);   
SWITCHTEC_QUIRK(0x4052);   
SWITCHTEC_QUIRK(0x4036);   
SWITCHTEC_QUIRK(0x4028);   
SWITCHTEC_QUIRK(0x4100);   
SWITCHTEC_QUIRK(0x4184);   
SWITCHTEC_QUIRK(0x4168);   
SWITCHTEC_QUIRK(0x4152);   
SWITCHTEC_QUIRK(0x4136);   
SWITCHTEC_QUIRK(0x4128);   
SWITCHTEC_QUIRK(0x4200);   
SWITCHTEC_QUIRK(0x4284);   
SWITCHTEC_QUIRK(0x4268);   
SWITCHTEC_QUIRK(0x4252);   
SWITCHTEC_QUIRK(0x4236);   
SWITCHTEC_QUIRK(0x4228);   
SWITCHTEC_QUIRK(0x4352);   
SWITCHTEC_QUIRK(0x4336);   
SWITCHTEC_QUIRK(0x4328);   
SWITCHTEC_QUIRK(0x4452);   
SWITCHTEC_QUIRK(0x4436);   
SWITCHTEC_QUIRK(0x4428);   
SWITCHTEC_QUIRK(0x4552);   
SWITCHTEC_QUIRK(0x4536);   
SWITCHTEC_QUIRK(0x4528);   
SWITCHTEC_QUIRK(0x5000);   
SWITCHTEC_QUIRK(0x5084);   
SWITCHTEC_QUIRK(0x5068);   
SWITCHTEC_QUIRK(0x5052);   
SWITCHTEC_QUIRK(0x5036);   
SWITCHTEC_QUIRK(0x5028);   
SWITCHTEC_QUIRK(0x5100);   
SWITCHTEC_QUIRK(0x5184);   
SWITCHTEC_QUIRK(0x5168);   
SWITCHTEC_QUIRK(0x5152);   
SWITCHTEC_QUIRK(0x5136);   
SWITCHTEC_QUIRK(0x5128);   
SWITCHTEC_QUIRK(0x5200);   
SWITCHTEC_QUIRK(0x5284);   
SWITCHTEC_QUIRK(0x5268);   
SWITCHTEC_QUIRK(0x5252);   
SWITCHTEC_QUIRK(0x5236);   
SWITCHTEC_QUIRK(0x5228);   
SWITCHTEC_QUIRK(0x5300);   
SWITCHTEC_QUIRK(0x5384);   
SWITCHTEC_QUIRK(0x5368);   
SWITCHTEC_QUIRK(0x5352);   
SWITCHTEC_QUIRK(0x5336);   
SWITCHTEC_QUIRK(0x5328);   
SWITCHTEC_QUIRK(0x5400);   
SWITCHTEC_QUIRK(0x5484);   
SWITCHTEC_QUIRK(0x5468);   
SWITCHTEC_QUIRK(0x5452);   
SWITCHTEC_QUIRK(0x5436);   
SWITCHTEC_QUIRK(0x5428);   
SWITCHTEC_QUIRK(0x5500);   
SWITCHTEC_QUIRK(0x5584);   
SWITCHTEC_QUIRK(0x5568);   
SWITCHTEC_QUIRK(0x5552);   
SWITCHTEC_QUIRK(0x5536);   
SWITCHTEC_QUIRK(0x5528);   

 
static void quirk_plx_ntb_dma_alias(struct pci_dev *pdev)
{
	pci_info(pdev, "Setting PLX NTB proxy ID aliases\n");
	 
	pci_add_dma_alias(pdev, 0, 256);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_PLX, 0x87b0, quirk_plx_ntb_dma_alias);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_PLX, 0x87b1, quirk_plx_ntb_dma_alias);

 
static void quirk_reset_lenovo_thinkpad_p50_nvgpu(struct pci_dev *pdev)
{
	void __iomem *map;
	int ret;

	if (pdev->subsystem_vendor != PCI_VENDOR_ID_LENOVO ||
	    pdev->subsystem_device != 0x222e ||
	    !pci_reset_supported(pdev))
		return;

	if (pci_enable_device_mem(pdev))
		return;

	 
	map = pci_iomap(pdev, 0, 0x23000);
	if (!map) {
		pci_err(pdev, "Can't map MMIO space\n");
		goto out_disable;
	}

	 
	if (ioread32(map + 0x2240c) & 0x2) {
		pci_info(pdev, FW_BUG "GPU left initialized by EFI, resetting\n");
		ret = pci_reset_bus(pdev);
		if (ret < 0)
			pci_err(pdev, "Failed to reset GPU: %d\n", ret);
	}

	iounmap(map);
out_disable:
	pci_disable_device(pdev);
}
DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_VENDOR_ID_NVIDIA, 0x13b1,
			      PCI_CLASS_DISPLAY_VGA, 8,
			      quirk_reset_lenovo_thinkpad_p50_nvgpu);

 
static void pci_fixup_no_d0_pme(struct pci_dev *dev)
{
	pci_info(dev, "PME# does not work under D0, disabling it\n");
	dev->pme_support &= ~(PCI_PM_CAP_PME_D0 >> PCI_PM_CAP_PME_SHIFT);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ASMEDIA, 0x2142, pci_fixup_no_d0_pme);

 
static void pci_fixup_no_msi_no_pme(struct pci_dev *dev)
{
#ifdef CONFIG_PCI_MSI
	pci_info(dev, "MSI is not implemented on this device, disabling it\n");
	dev->no_msi = 1;
#endif
	pci_info(dev, "PME# is unreliable, disabling it\n");
	dev->pme_support = 0;
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_PERICOM, 0x400e, pci_fixup_no_msi_no_pme);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_PERICOM, 0x400f, pci_fixup_no_msi_no_pme);

static void apex_pci_fixup_class(struct pci_dev *pdev)
{
	pdev->class = (PCI_CLASS_SYSTEM_OTHER << 8) | pdev->class;
}
DECLARE_PCI_FIXUP_CLASS_HEADER(0x1ac1, 0x089a,
			       PCI_CLASS_NOT_DEFINED, 8, apex_pci_fixup_class);

 
#define PI7C9X2Gxxx_MODE_REG		0x74
#define PI7C9X2Gxxx_STORE_FORWARD_MODE	BIT(0)
static void pci_fixup_pericom_acs_store_forward(struct pci_dev *pdev)
{
	struct pci_dev *upstream;
	u16 val;

	 
	if (pci_pcie_type(pdev) != PCI_EXP_TYPE_DOWNSTREAM)
		return;

	 
	if (!pdev->acs_cap)
		return;
	pci_read_config_word(pdev, pdev->acs_cap + PCI_ACS_CTRL, &val);
	if (!(val & PCI_ACS_RR))
		return;

	upstream = pci_upstream_bridge(pdev);
	if (!upstream)
		return;

	pci_read_config_word(upstream, PI7C9X2Gxxx_MODE_REG, &val);
	if (!(val & PI7C9X2Gxxx_STORE_FORWARD_MODE)) {
		pci_info(upstream, "Setting PI7C9X2Gxxx store-forward mode to avoid ACS erratum\n");
		pci_write_config_word(upstream, PI7C9X2Gxxx_MODE_REG, val |
				      PI7C9X2Gxxx_STORE_FORWARD_MODE);
	}
}
 
DECLARE_PCI_FIXUP_ENABLE(PCI_VENDOR_ID_PERICOM, 0x2404,
			 pci_fixup_pericom_acs_store_forward);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_PERICOM, 0x2404,
			 pci_fixup_pericom_acs_store_forward);
DECLARE_PCI_FIXUP_ENABLE(PCI_VENDOR_ID_PERICOM, 0x2304,
			 pci_fixup_pericom_acs_store_forward);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_PERICOM, 0x2304,
			 pci_fixup_pericom_acs_store_forward);
DECLARE_PCI_FIXUP_ENABLE(PCI_VENDOR_ID_PERICOM, 0x2303,
			 pci_fixup_pericom_acs_store_forward);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_PERICOM, 0x2303,
			 pci_fixup_pericom_acs_store_forward);

static void nvidia_ion_ahci_fixup(struct pci_dev *pdev)
{
	pdev->dev_flags |= PCI_DEV_FLAGS_HAS_MSI_MASKING;
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_NVIDIA, 0x0ab8, nvidia_ion_ahci_fixup);

static void rom_bar_overlap_defect(struct pci_dev *dev)
{
	pci_info(dev, "working around ROM BAR overlap defect\n");
	dev->rom_bar_overlap = 1;
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x1533, rom_bar_overlap_defect);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x1536, rom_bar_overlap_defect);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x1537, rom_bar_overlap_defect);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x1538, rom_bar_overlap_defect);

#ifdef CONFIG_PCIEASPM
 
static void aspm_l1_acceptable_latency(struct pci_dev *dev)
{
	u32 l1_lat = FIELD_GET(PCI_EXP_DEVCAP_L1, dev->devcap);

	if (l1_lat < 7) {
		dev->devcap |= FIELD_PREP(PCI_EXP_DEVCAP_L1, 7);
		pci_info(dev, "ASPM: overriding L1 acceptable latency from %#x to 0x7\n",
			 l1_lat);
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x4f80, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x4f81, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x4f82, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x4f83, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x4f84, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x4f85, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x4f86, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x4f87, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x4f88, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x5690, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x5691, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x5692, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x5693, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x5694, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x5695, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x56a0, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x56a1, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x56a2, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x56a3, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x56a4, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x56a5, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x56a6, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x56b0, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x56b1, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x56c0, aspm_l1_acceptable_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x56c1, aspm_l1_acceptable_latency);
#endif

#ifdef CONFIG_PCIE_DPC
 
static void dpc_log_size(struct pci_dev *dev)
{
	u16 dpc, val;

	dpc = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_DPC);
	if (!dpc)
		return;

	pci_read_config_word(dev, dpc + PCI_EXP_DPC_CAP, &val);
	if (!(val & PCI_EXP_DPC_CAP_RP_EXT))
		return;

	if (!((val & PCI_EXP_DPC_RP_PIO_LOG_SIZE) >> 8)) {
		pci_info(dev, "Overriding RP PIO Log Size to 4\n");
		dev->dpc_rp_log_size = 4;
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x461f, dpc_log_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x462f, dpc_log_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x463f, dpc_log_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x466e, dpc_log_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x8a1d, dpc_log_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x8a1f, dpc_log_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x8a21, dpc_log_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x8a23, dpc_log_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x9a23, dpc_log_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x9a25, dpc_log_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x9a27, dpc_log_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x9a29, dpc_log_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x9a2b, dpc_log_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x9a2d, dpc_log_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x9a2f, dpc_log_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x9a31, dpc_log_size);
#endif

 
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_XILINX, 0x5020, of_pci_make_dev_node);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_XILINX, 0x5021, of_pci_make_dev_node);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_REDHAT, 0x0005, of_pci_make_dev_node);

 
static void pci_fixup_d3cold_delay_1sec(struct pci_dev *pdev)
{
	pdev->d3cold_delay = 1000;
}
DECLARE_PCI_FIXUP_FINAL(0x5555, 0x0004, pci_fixup_d3cold_delay_1sec);
