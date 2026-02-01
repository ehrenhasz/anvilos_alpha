
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/suspend.h>
#include <linux/workqueue.h>
#include <linux/scatterlist.h>
#include <linux/io.h>
#include <linux/log2.h>
#include <linux/slab.h>
#include <linux/glob.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include <linux/libata.h>
#include <asm/byteorder.h>
#include <asm/unaligned.h>
#include <linux/cdrom.h>
#include <linux/ratelimit.h>
#include <linux/leds.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <asm/setup.h>

#define CREATE_TRACE_POINTS
#include <trace/events/libata.h>

#include "libata.h"
#include "libata-transport.h"

const struct ata_port_operations ata_base_port_ops = {
	.prereset		= ata_std_prereset,
	.postreset		= ata_std_postreset,
	.error_handler		= ata_std_error_handler,
	.sched_eh		= ata_std_sched_eh,
	.end_eh			= ata_std_end_eh,
};

const struct ata_port_operations sata_port_ops = {
	.inherits		= &ata_base_port_ops,

	.qc_defer		= ata_std_qc_defer,
	.hardreset		= sata_std_hardreset,
};
EXPORT_SYMBOL_GPL(sata_port_ops);

static unsigned int ata_dev_init_params(struct ata_device *dev,
					u16 heads, u16 sectors);
static unsigned int ata_dev_set_xfermode(struct ata_device *dev);
static void ata_dev_xfermask(struct ata_device *dev);
static unsigned long ata_dev_blacklisted(const struct ata_device *dev);

atomic_t ata_print_id = ATOMIC_INIT(0);

#ifdef CONFIG_ATA_FORCE
struct ata_force_param {
	const char	*name;
	u8		cbl;
	u8		spd_limit;
	unsigned int	xfer_mask;
	unsigned int	horkage_on;
	unsigned int	horkage_off;
	u16		lflags_on;
	u16		lflags_off;
};

struct ata_force_ent {
	int			port;
	int			device;
	struct ata_force_param	param;
};

static struct ata_force_ent *ata_force_tbl;
static int ata_force_tbl_size;

static char ata_force_param_buf[COMMAND_LINE_SIZE] __initdata;
 
module_param_string(force, ata_force_param_buf, sizeof(ata_force_param_buf), 0);
MODULE_PARM_DESC(force, "Force ATA configurations including cable type, link speed and transfer mode (see Documentation/admin-guide/kernel-parameters.rst for details)");
#endif

static int atapi_enabled = 1;
module_param(atapi_enabled, int, 0444);
MODULE_PARM_DESC(atapi_enabled, "Enable discovery of ATAPI devices (0=off, 1=on [default])");

static int atapi_dmadir = 0;
module_param(atapi_dmadir, int, 0444);
MODULE_PARM_DESC(atapi_dmadir, "Enable ATAPI DMADIR bridge support (0=off [default], 1=on)");

int atapi_passthru16 = 1;
module_param(atapi_passthru16, int, 0444);
MODULE_PARM_DESC(atapi_passthru16, "Enable ATA_16 passthru for ATAPI devices (0=off, 1=on [default])");

int libata_fua = 0;
module_param_named(fua, libata_fua, int, 0444);
MODULE_PARM_DESC(fua, "FUA support (0=off [default], 1=on)");

static int ata_ignore_hpa;
module_param_named(ignore_hpa, ata_ignore_hpa, int, 0644);
MODULE_PARM_DESC(ignore_hpa, "Ignore HPA limit (0=keep BIOS limits, 1=ignore limits, using full disk)");

static int libata_dma_mask = ATA_DMA_MASK_ATA|ATA_DMA_MASK_ATAPI|ATA_DMA_MASK_CFA;
module_param_named(dma, libata_dma_mask, int, 0444);
MODULE_PARM_DESC(dma, "DMA enable/disable (0x1==ATA, 0x2==ATAPI, 0x4==CF)");

static int ata_probe_timeout;
module_param(ata_probe_timeout, int, 0444);
MODULE_PARM_DESC(ata_probe_timeout, "Set ATA probing timeout (seconds)");

int libata_noacpi = 0;
module_param_named(noacpi, libata_noacpi, int, 0444);
MODULE_PARM_DESC(noacpi, "Disable the use of ACPI in probe/suspend/resume (0=off [default], 1=on)");

int libata_allow_tpm = 0;
module_param_named(allow_tpm, libata_allow_tpm, int, 0444);
MODULE_PARM_DESC(allow_tpm, "Permit the use of TPM commands (0=off [default], 1=on)");

static int atapi_an;
module_param(atapi_an, int, 0444);
MODULE_PARM_DESC(atapi_an, "Enable ATAPI AN media presence notification (0=0ff [default], 1=on)");

MODULE_AUTHOR("Jeff Garzik");
MODULE_DESCRIPTION("Library module for ATA devices");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

static inline bool ata_dev_print_info(struct ata_device *dev)
{
	struct ata_eh_context *ehc = &dev->link->eh_context;

	return ehc->i.flags & ATA_EHI_PRINTINFO;
}

static bool ata_sstatus_online(u32 sstatus)
{
	return (sstatus & 0xf) == 0x3;
}

 
struct ata_link *ata_link_next(struct ata_link *link, struct ata_port *ap,
			       enum ata_link_iter_mode mode)
{
	BUG_ON(mode != ATA_LITER_EDGE &&
	       mode != ATA_LITER_PMP_FIRST && mode != ATA_LITER_HOST_FIRST);

	 
	if (!link)
		switch (mode) {
		case ATA_LITER_EDGE:
		case ATA_LITER_PMP_FIRST:
			if (sata_pmp_attached(ap))
				return ap->pmp_link;
			fallthrough;
		case ATA_LITER_HOST_FIRST:
			return &ap->link;
		}

	 
	if (link == &ap->link)
		switch (mode) {
		case ATA_LITER_HOST_FIRST:
			if (sata_pmp_attached(ap))
				return ap->pmp_link;
			fallthrough;
		case ATA_LITER_PMP_FIRST:
			if (unlikely(ap->slave_link))
				return ap->slave_link;
			fallthrough;
		case ATA_LITER_EDGE:
			return NULL;
		}

	 
	if (unlikely(link == ap->slave_link))
		return NULL;

	 
	if (++link < ap->pmp_link + ap->nr_pmp_links)
		return link;

	if (mode == ATA_LITER_PMP_FIRST)
		return &ap->link;

	return NULL;
}
EXPORT_SYMBOL_GPL(ata_link_next);

 
struct ata_device *ata_dev_next(struct ata_device *dev, struct ata_link *link,
				enum ata_dev_iter_mode mode)
{
	BUG_ON(mode != ATA_DITER_ENABLED && mode != ATA_DITER_ENABLED_REVERSE &&
	       mode != ATA_DITER_ALL && mode != ATA_DITER_ALL_REVERSE);

	 
	if (!dev)
		switch (mode) {
		case ATA_DITER_ENABLED:
		case ATA_DITER_ALL:
			dev = link->device;
			goto check;
		case ATA_DITER_ENABLED_REVERSE:
		case ATA_DITER_ALL_REVERSE:
			dev = link->device + ata_link_max_devices(link) - 1;
			goto check;
		}

 next:
	 
	switch (mode) {
	case ATA_DITER_ENABLED:
	case ATA_DITER_ALL:
		if (++dev < link->device + ata_link_max_devices(link))
			goto check;
		return NULL;
	case ATA_DITER_ENABLED_REVERSE:
	case ATA_DITER_ALL_REVERSE:
		if (--dev >= link->device)
			goto check;
		return NULL;
	}

 check:
	if ((mode == ATA_DITER_ENABLED || mode == ATA_DITER_ENABLED_REVERSE) &&
	    !ata_dev_enabled(dev))
		goto next;
	return dev;
}
EXPORT_SYMBOL_GPL(ata_dev_next);

 
struct ata_link *ata_dev_phys_link(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;

	if (!ap->slave_link)
		return dev->link;
	if (!dev->devno)
		return &ap->link;
	return ap->slave_link;
}

#ifdef CONFIG_ATA_FORCE
 
void ata_force_cbl(struct ata_port *ap)
{
	int i;

	for (i = ata_force_tbl_size - 1; i >= 0; i--) {
		const struct ata_force_ent *fe = &ata_force_tbl[i];

		if (fe->port != -1 && fe->port != ap->print_id)
			continue;

		if (fe->param.cbl == ATA_CBL_NONE)
			continue;

		ap->cbl = fe->param.cbl;
		ata_port_notice(ap, "FORCE: cable set to %s\n", fe->param.name);
		return;
	}
}

 
static void ata_force_link_limits(struct ata_link *link)
{
	bool did_spd = false;
	int linkno = link->pmp;
	int i;

	if (ata_is_host_link(link))
		linkno += 15;

	for (i = ata_force_tbl_size - 1; i >= 0; i--) {
		const struct ata_force_ent *fe = &ata_force_tbl[i];

		if (fe->port != -1 && fe->port != link->ap->print_id)
			continue;

		if (fe->device != -1 && fe->device != linkno)
			continue;

		 
		if (!did_spd && fe->param.spd_limit) {
			link->hw_sata_spd_limit = (1 << fe->param.spd_limit) - 1;
			ata_link_notice(link, "FORCE: PHY spd limit set to %s\n",
					fe->param.name);
			did_spd = true;
		}

		 
		if (fe->param.lflags_on) {
			link->flags |= fe->param.lflags_on;
			ata_link_notice(link,
					"FORCE: link flag 0x%x forced -> 0x%x\n",
					fe->param.lflags_on, link->flags);
		}
		if (fe->param.lflags_off) {
			link->flags &= ~fe->param.lflags_off;
			ata_link_notice(link,
				"FORCE: link flag 0x%x cleared -> 0x%x\n",
				fe->param.lflags_off, link->flags);
		}
	}
}

 
static void ata_force_xfermask(struct ata_device *dev)
{
	int devno = dev->link->pmp + dev->devno;
	int alt_devno = devno;
	int i;

	 
	if (ata_is_host_link(dev->link))
		alt_devno += 15;

	for (i = ata_force_tbl_size - 1; i >= 0; i--) {
		const struct ata_force_ent *fe = &ata_force_tbl[i];
		unsigned int pio_mask, mwdma_mask, udma_mask;

		if (fe->port != -1 && fe->port != dev->link->ap->print_id)
			continue;

		if (fe->device != -1 && fe->device != devno &&
		    fe->device != alt_devno)
			continue;

		if (!fe->param.xfer_mask)
			continue;

		ata_unpack_xfermask(fe->param.xfer_mask,
				    &pio_mask, &mwdma_mask, &udma_mask);
		if (udma_mask)
			dev->udma_mask = udma_mask;
		else if (mwdma_mask) {
			dev->udma_mask = 0;
			dev->mwdma_mask = mwdma_mask;
		} else {
			dev->udma_mask = 0;
			dev->mwdma_mask = 0;
			dev->pio_mask = pio_mask;
		}

		ata_dev_notice(dev, "FORCE: xfer_mask set to %s\n",
			       fe->param.name);
		return;
	}
}

 
static void ata_force_horkage(struct ata_device *dev)
{
	int devno = dev->link->pmp + dev->devno;
	int alt_devno = devno;
	int i;

	 
	if (ata_is_host_link(dev->link))
		alt_devno += 15;

	for (i = 0; i < ata_force_tbl_size; i++) {
		const struct ata_force_ent *fe = &ata_force_tbl[i];

		if (fe->port != -1 && fe->port != dev->link->ap->print_id)
			continue;

		if (fe->device != -1 && fe->device != devno &&
		    fe->device != alt_devno)
			continue;

		if (!(~dev->horkage & fe->param.horkage_on) &&
		    !(dev->horkage & fe->param.horkage_off))
			continue;

		dev->horkage |= fe->param.horkage_on;
		dev->horkage &= ~fe->param.horkage_off;

		ata_dev_notice(dev, "FORCE: horkage modified (%s)\n",
			       fe->param.name);
	}
}
#else
static inline void ata_force_link_limits(struct ata_link *link) { }
static inline void ata_force_xfermask(struct ata_device *dev) { }
static inline void ata_force_horkage(struct ata_device *dev) { }
#endif

 
int atapi_cmd_type(u8 opcode)
{
	switch (opcode) {
	case GPCMD_READ_10:
	case GPCMD_READ_12:
		return ATAPI_READ;

	case GPCMD_WRITE_10:
	case GPCMD_WRITE_12:
	case GPCMD_WRITE_AND_VERIFY_10:
		return ATAPI_WRITE;

	case GPCMD_READ_CD:
	case GPCMD_READ_CD_MSF:
		return ATAPI_READ_CD;

	case ATA_16:
	case ATA_12:
		if (atapi_passthru16)
			return ATAPI_PASS_THRU;
		fallthrough;
	default:
		return ATAPI_MISC;
	}
}
EXPORT_SYMBOL_GPL(atapi_cmd_type);

static const u8 ata_rw_cmds[] = {
	 
	ATA_CMD_READ_MULTI,
	ATA_CMD_WRITE_MULTI,
	ATA_CMD_READ_MULTI_EXT,
	ATA_CMD_WRITE_MULTI_EXT,
	0,
	0,
	0,
	0,
	 
	ATA_CMD_PIO_READ,
	ATA_CMD_PIO_WRITE,
	ATA_CMD_PIO_READ_EXT,
	ATA_CMD_PIO_WRITE_EXT,
	0,
	0,
	0,
	0,
	 
	ATA_CMD_READ,
	ATA_CMD_WRITE,
	ATA_CMD_READ_EXT,
	ATA_CMD_WRITE_EXT,
	0,
	0,
	0,
	ATA_CMD_WRITE_FUA_EXT
};

 
static bool ata_set_rwcmd_protocol(struct ata_device *dev,
				   struct ata_taskfile *tf)
{
	u8 cmd;

	int index, fua, lba48, write;

	fua = (tf->flags & ATA_TFLAG_FUA) ? 4 : 0;
	lba48 = (tf->flags & ATA_TFLAG_LBA48) ? 2 : 0;
	write = (tf->flags & ATA_TFLAG_WRITE) ? 1 : 0;

	if (dev->flags & ATA_DFLAG_PIO) {
		tf->protocol = ATA_PROT_PIO;
		index = dev->multi_count ? 0 : 8;
	} else if (lba48 && (dev->link->ap->flags & ATA_FLAG_PIO_LBA48)) {
		 
		tf->protocol = ATA_PROT_PIO;
		index = dev->multi_count ? 0 : 8;
	} else {
		tf->protocol = ATA_PROT_DMA;
		index = 16;
	}

	cmd = ata_rw_cmds[index + fua + lba48 + write];
	if (!cmd)
		return false;

	tf->command = cmd;

	return true;
}

 
u64 ata_tf_read_block(const struct ata_taskfile *tf, struct ata_device *dev)
{
	u64 block = 0;

	if (tf->flags & ATA_TFLAG_LBA) {
		if (tf->flags & ATA_TFLAG_LBA48) {
			block |= (u64)tf->hob_lbah << 40;
			block |= (u64)tf->hob_lbam << 32;
			block |= (u64)tf->hob_lbal << 24;
		} else
			block |= (tf->device & 0xf) << 24;

		block |= tf->lbah << 16;
		block |= tf->lbam << 8;
		block |= tf->lbal;
	} else {
		u32 cyl, head, sect;

		cyl = tf->lbam | (tf->lbah << 8);
		head = tf->device & 0xf;
		sect = tf->lbal;

		if (!sect) {
			ata_dev_warn(dev,
				     "device reported invalid CHS sector 0\n");
			return U64_MAX;
		}

		block = (cyl * dev->heads + head) * dev->sectors + sect - 1;
	}

	return block;
}

 
static inline void ata_set_tf_cdl(struct ata_queued_cmd *qc, int cdl)
{
	struct ata_taskfile *tf = &qc->tf;

	if (tf->protocol == ATA_PROT_NCQ)
		tf->auxiliary |= cdl;
	else
		tf->feature |= cdl;

	 
	qc->flags |= ATA_QCFLAG_HAS_CDL | ATA_QCFLAG_RESULT_TF;
}

 
int ata_build_rw_tf(struct ata_queued_cmd *qc, u64 block, u32 n_block,
		    unsigned int tf_flags, int cdl, int class)
{
	struct ata_taskfile *tf = &qc->tf;
	struct ata_device *dev = qc->dev;

	tf->flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE;
	tf->flags |= tf_flags;

	if (ata_ncq_enabled(dev)) {
		 
		if (!lba_48_ok(block, n_block))
			return -ERANGE;

		tf->protocol = ATA_PROT_NCQ;
		tf->flags |= ATA_TFLAG_LBA | ATA_TFLAG_LBA48;

		if (tf->flags & ATA_TFLAG_WRITE)
			tf->command = ATA_CMD_FPDMA_WRITE;
		else
			tf->command = ATA_CMD_FPDMA_READ;

		tf->nsect = qc->hw_tag << 3;
		tf->hob_feature = (n_block >> 8) & 0xff;
		tf->feature = n_block & 0xff;

		tf->hob_lbah = (block >> 40) & 0xff;
		tf->hob_lbam = (block >> 32) & 0xff;
		tf->hob_lbal = (block >> 24) & 0xff;
		tf->lbah = (block >> 16) & 0xff;
		tf->lbam = (block >> 8) & 0xff;
		tf->lbal = block & 0xff;

		tf->device = ATA_LBA;
		if (tf->flags & ATA_TFLAG_FUA)
			tf->device |= 1 << 7;

		if (dev->flags & ATA_DFLAG_NCQ_PRIO_ENABLED &&
		    class == IOPRIO_CLASS_RT)
			tf->hob_nsect |= ATA_PRIO_HIGH << ATA_SHIFT_PRIO;

		if ((dev->flags & ATA_DFLAG_CDL_ENABLED) && cdl)
			ata_set_tf_cdl(qc, cdl);

	} else if (dev->flags & ATA_DFLAG_LBA) {
		tf->flags |= ATA_TFLAG_LBA;

		if ((dev->flags & ATA_DFLAG_CDL_ENABLED) && cdl)
			ata_set_tf_cdl(qc, cdl);

		 
		if (!(tf->flags & ATA_TFLAG_FUA) &&
		    !(qc->flags & ATA_QCFLAG_HAS_CDL) &&
		    lba_28_ok(block, n_block)) {
			 
			tf->device |= (block >> 24) & 0xf;
		} else if (lba_48_ok(block, n_block)) {
			if (!(dev->flags & ATA_DFLAG_LBA48))
				return -ERANGE;

			 
			tf->flags |= ATA_TFLAG_LBA48;

			tf->hob_nsect = (n_block >> 8) & 0xff;

			tf->hob_lbah = (block >> 40) & 0xff;
			tf->hob_lbam = (block >> 32) & 0xff;
			tf->hob_lbal = (block >> 24) & 0xff;
		} else {
			 
			return -ERANGE;
		}

		if (unlikely(!ata_set_rwcmd_protocol(dev, tf)))
			return -EINVAL;

		tf->nsect = n_block & 0xff;

		tf->lbah = (block >> 16) & 0xff;
		tf->lbam = (block >> 8) & 0xff;
		tf->lbal = block & 0xff;

		tf->device |= ATA_LBA;
	} else {
		 
		u32 sect, head, cyl, track;

		 
		if (!lba_28_ok(block, n_block))
			return -ERANGE;

		if (unlikely(!ata_set_rwcmd_protocol(dev, tf)))
			return -EINVAL;

		 
		track = (u32)block / dev->sectors;
		cyl   = track / dev->heads;
		head  = track % dev->heads;
		sect  = (u32)block % dev->sectors + 1;

		 
		if ((cyl >> 16) || (head >> 4) || (sect >> 8) || (!sect))
			return -ERANGE;

		tf->nsect = n_block & 0xff;  
		tf->lbal = sect;
		tf->lbam = cyl;
		tf->lbah = cyl >> 8;
		tf->device |= head;
	}

	return 0;
}

 
unsigned int ata_pack_xfermask(unsigned int pio_mask,
			       unsigned int mwdma_mask,
			       unsigned int udma_mask)
{
	return	((pio_mask << ATA_SHIFT_PIO) & ATA_MASK_PIO) |
		((mwdma_mask << ATA_SHIFT_MWDMA) & ATA_MASK_MWDMA) |
		((udma_mask << ATA_SHIFT_UDMA) & ATA_MASK_UDMA);
}
EXPORT_SYMBOL_GPL(ata_pack_xfermask);

 
void ata_unpack_xfermask(unsigned int xfer_mask, unsigned int *pio_mask,
			 unsigned int *mwdma_mask, unsigned int *udma_mask)
{
	if (pio_mask)
		*pio_mask = (xfer_mask & ATA_MASK_PIO) >> ATA_SHIFT_PIO;
	if (mwdma_mask)
		*mwdma_mask = (xfer_mask & ATA_MASK_MWDMA) >> ATA_SHIFT_MWDMA;
	if (udma_mask)
		*udma_mask = (xfer_mask & ATA_MASK_UDMA) >> ATA_SHIFT_UDMA;
}

static const struct ata_xfer_ent {
	int shift, bits;
	u8 base;
} ata_xfer_tbl[] = {
	{ ATA_SHIFT_PIO, ATA_NR_PIO_MODES, XFER_PIO_0 },
	{ ATA_SHIFT_MWDMA, ATA_NR_MWDMA_MODES, XFER_MW_DMA_0 },
	{ ATA_SHIFT_UDMA, ATA_NR_UDMA_MODES, XFER_UDMA_0 },
	{ -1, },
};

 
u8 ata_xfer_mask2mode(unsigned int xfer_mask)
{
	int highbit = fls(xfer_mask) - 1;
	const struct ata_xfer_ent *ent;

	for (ent = ata_xfer_tbl; ent->shift >= 0; ent++)
		if (highbit >= ent->shift && highbit < ent->shift + ent->bits)
			return ent->base + highbit - ent->shift;
	return 0xff;
}
EXPORT_SYMBOL_GPL(ata_xfer_mask2mode);

 
unsigned int ata_xfer_mode2mask(u8 xfer_mode)
{
	const struct ata_xfer_ent *ent;

	for (ent = ata_xfer_tbl; ent->shift >= 0; ent++)
		if (xfer_mode >= ent->base && xfer_mode < ent->base + ent->bits)
			return ((2 << (ent->shift + xfer_mode - ent->base)) - 1)
				& ~((1 << ent->shift) - 1);
	return 0;
}
EXPORT_SYMBOL_GPL(ata_xfer_mode2mask);

 
int ata_xfer_mode2shift(u8 xfer_mode)
{
	const struct ata_xfer_ent *ent;

	for (ent = ata_xfer_tbl; ent->shift >= 0; ent++)
		if (xfer_mode >= ent->base && xfer_mode < ent->base + ent->bits)
			return ent->shift;
	return -1;
}
EXPORT_SYMBOL_GPL(ata_xfer_mode2shift);

 
const char *ata_mode_string(unsigned int xfer_mask)
{
	static const char * const xfer_mode_str[] = {
		"PIO0",
		"PIO1",
		"PIO2",
		"PIO3",
		"PIO4",
		"PIO5",
		"PIO6",
		"MWDMA0",
		"MWDMA1",
		"MWDMA2",
		"MWDMA3",
		"MWDMA4",
		"UDMA/16",
		"UDMA/25",
		"UDMA/33",
		"UDMA/44",
		"UDMA/66",
		"UDMA/100",
		"UDMA/133",
		"UDMA7",
	};
	int highbit;

	highbit = fls(xfer_mask) - 1;
	if (highbit >= 0 && highbit < ARRAY_SIZE(xfer_mode_str))
		return xfer_mode_str[highbit];
	return "<n/a>";
}
EXPORT_SYMBOL_GPL(ata_mode_string);

const char *sata_spd_string(unsigned int spd)
{
	static const char * const spd_str[] = {
		"1.5 Gbps",
		"3.0 Gbps",
		"6.0 Gbps",
	};

	if (spd == 0 || (spd - 1) >= ARRAY_SIZE(spd_str))
		return "<unknown>";
	return spd_str[spd - 1];
}

 
unsigned int ata_dev_classify(const struct ata_taskfile *tf)
{
	 
	if (tf->lbam == 0 && tf->lbah == 0)
		return ATA_DEV_ATA;

	if (tf->lbam == 0x14 && tf->lbah == 0xeb)
		return ATA_DEV_ATAPI;

	if (tf->lbam == 0x69 && tf->lbah == 0x96)
		return ATA_DEV_PMP;

	if (tf->lbam == 0x3c && tf->lbah == 0xc3)
		return ATA_DEV_SEMB;

	if (tf->lbam == 0xcd && tf->lbah == 0xab)
		return ATA_DEV_ZAC;

	return ATA_DEV_UNKNOWN;
}
EXPORT_SYMBOL_GPL(ata_dev_classify);

 

void ata_id_string(const u16 *id, unsigned char *s,
		   unsigned int ofs, unsigned int len)
{
	unsigned int c;

	BUG_ON(len & 1);

	while (len > 0) {
		c = id[ofs] >> 8;
		*s = c;
		s++;

		c = id[ofs] & 0xff;
		*s = c;
		s++;

		ofs++;
		len -= 2;
	}
}
EXPORT_SYMBOL_GPL(ata_id_string);

 
void ata_id_c_string(const u16 *id, unsigned char *s,
		     unsigned int ofs, unsigned int len)
{
	unsigned char *p;

	ata_id_string(id, s, ofs, len - 1);

	p = s + strnlen(s, len - 1);
	while (p > s && p[-1] == ' ')
		p--;
	*p = '\0';
}
EXPORT_SYMBOL_GPL(ata_id_c_string);

static u64 ata_id_n_sectors(const u16 *id)
{
	if (ata_id_has_lba(id)) {
		if (ata_id_has_lba48(id))
			return ata_id_u64(id, ATA_ID_LBA_CAPACITY_2);

		return ata_id_u32(id, ATA_ID_LBA_CAPACITY);
	}

	if (ata_id_current_chs_valid(id))
		return (u32)id[ATA_ID_CUR_CYLS] * (u32)id[ATA_ID_CUR_HEADS] *
		       (u32)id[ATA_ID_CUR_SECTORS];

	return (u32)id[ATA_ID_CYLS] * (u32)id[ATA_ID_HEADS] *
	       (u32)id[ATA_ID_SECTORS];
}

u64 ata_tf_to_lba48(const struct ata_taskfile *tf)
{
	u64 sectors = 0;

	sectors |= ((u64)(tf->hob_lbah & 0xff)) << 40;
	sectors |= ((u64)(tf->hob_lbam & 0xff)) << 32;
	sectors |= ((u64)(tf->hob_lbal & 0xff)) << 24;
	sectors |= (tf->lbah & 0xff) << 16;
	sectors |= (tf->lbam & 0xff) << 8;
	sectors |= (tf->lbal & 0xff);

	return sectors;
}

u64 ata_tf_to_lba(const struct ata_taskfile *tf)
{
	u64 sectors = 0;

	sectors |= (tf->device & 0x0f) << 24;
	sectors |= (tf->lbah & 0xff) << 16;
	sectors |= (tf->lbam & 0xff) << 8;
	sectors |= (tf->lbal & 0xff);

	return sectors;
}

 
static int ata_read_native_max_address(struct ata_device *dev, u64 *max_sectors)
{
	unsigned int err_mask;
	struct ata_taskfile tf;
	int lba48 = ata_id_has_lba48(dev->id);

	ata_tf_init(dev, &tf);

	 
	tf.flags |= ATA_TFLAG_DEVICE | ATA_TFLAG_ISADDR;

	if (lba48) {
		tf.command = ATA_CMD_READ_NATIVE_MAX_EXT;
		tf.flags |= ATA_TFLAG_LBA48;
	} else
		tf.command = ATA_CMD_READ_NATIVE_MAX;

	tf.protocol = ATA_PROT_NODATA;
	tf.device |= ATA_LBA;

	err_mask = ata_exec_internal(dev, &tf, NULL, DMA_NONE, NULL, 0, 0);
	if (err_mask) {
		ata_dev_warn(dev,
			     "failed to read native max address (err_mask=0x%x)\n",
			     err_mask);
		if (err_mask == AC_ERR_DEV && (tf.error & ATA_ABORTED))
			return -EACCES;
		return -EIO;
	}

	if (lba48)
		*max_sectors = ata_tf_to_lba48(&tf) + 1;
	else
		*max_sectors = ata_tf_to_lba(&tf) + 1;
	if (dev->horkage & ATA_HORKAGE_HPA_SIZE)
		(*max_sectors)--;
	return 0;
}

 
static int ata_set_max_sectors(struct ata_device *dev, u64 new_sectors)
{
	unsigned int err_mask;
	struct ata_taskfile tf;
	int lba48 = ata_id_has_lba48(dev->id);

	new_sectors--;

	ata_tf_init(dev, &tf);

	tf.flags |= ATA_TFLAG_DEVICE | ATA_TFLAG_ISADDR;

	if (lba48) {
		tf.command = ATA_CMD_SET_MAX_EXT;
		tf.flags |= ATA_TFLAG_LBA48;

		tf.hob_lbal = (new_sectors >> 24) & 0xff;
		tf.hob_lbam = (new_sectors >> 32) & 0xff;
		tf.hob_lbah = (new_sectors >> 40) & 0xff;
	} else {
		tf.command = ATA_CMD_SET_MAX;

		tf.device |= (new_sectors >> 24) & 0xf;
	}

	tf.protocol = ATA_PROT_NODATA;
	tf.device |= ATA_LBA;

	tf.lbal = (new_sectors >> 0) & 0xff;
	tf.lbam = (new_sectors >> 8) & 0xff;
	tf.lbah = (new_sectors >> 16) & 0xff;

	err_mask = ata_exec_internal(dev, &tf, NULL, DMA_NONE, NULL, 0, 0);
	if (err_mask) {
		ata_dev_warn(dev,
			     "failed to set max address (err_mask=0x%x)\n",
			     err_mask);
		if (err_mask == AC_ERR_DEV &&
		    (tf.error & (ATA_ABORTED | ATA_IDNF)))
			return -EACCES;
		return -EIO;
	}

	return 0;
}

 
static int ata_hpa_resize(struct ata_device *dev)
{
	bool print_info = ata_dev_print_info(dev);
	bool unlock_hpa = ata_ignore_hpa || dev->flags & ATA_DFLAG_UNLOCK_HPA;
	u64 sectors = ata_id_n_sectors(dev->id);
	u64 native_sectors;
	int rc;

	 
	if ((dev->class != ATA_DEV_ATA && dev->class != ATA_DEV_ZAC) ||
	    !ata_id_has_lba(dev->id) || !ata_id_hpa_enabled(dev->id) ||
	    (dev->horkage & ATA_HORKAGE_BROKEN_HPA))
		return 0;

	 
	rc = ata_read_native_max_address(dev, &native_sectors);
	if (rc) {
		 
		if (rc == -EACCES || !unlock_hpa) {
			ata_dev_warn(dev,
				     "HPA support seems broken, skipping HPA handling\n");
			dev->horkage |= ATA_HORKAGE_BROKEN_HPA;

			 
			if (rc == -EACCES)
				rc = 0;
		}

		return rc;
	}
	dev->n_native_sectors = native_sectors;

	 
	if (native_sectors <= sectors || !unlock_hpa) {
		if (!print_info || native_sectors == sectors)
			return 0;

		if (native_sectors > sectors)
			ata_dev_info(dev,
				"HPA detected: current %llu, native %llu\n",
				(unsigned long long)sectors,
				(unsigned long long)native_sectors);
		else if (native_sectors < sectors)
			ata_dev_warn(dev,
				"native sectors (%llu) is smaller than sectors (%llu)\n",
				(unsigned long long)native_sectors,
				(unsigned long long)sectors);
		return 0;
	}

	 
	rc = ata_set_max_sectors(dev, native_sectors);
	if (rc == -EACCES) {
		 
		ata_dev_warn(dev,
			     "device aborted resize (%llu -> %llu), skipping HPA handling\n",
			     (unsigned long long)sectors,
			     (unsigned long long)native_sectors);
		dev->horkage |= ATA_HORKAGE_BROKEN_HPA;
		return 0;
	} else if (rc)
		return rc;

	 
	rc = ata_dev_reread_id(dev, 0);
	if (rc) {
		ata_dev_err(dev,
			    "failed to re-read IDENTIFY data after HPA resizing\n");
		return rc;
	}

	if (print_info) {
		u64 new_sectors = ata_id_n_sectors(dev->id);
		ata_dev_info(dev,
			"HPA unlocked: %llu -> %llu, native %llu\n",
			(unsigned long long)sectors,
			(unsigned long long)new_sectors,
			(unsigned long long)native_sectors);
	}

	return 0;
}

 

static inline void ata_dump_id(struct ata_device *dev, const u16 *id)
{
	ata_dev_dbg(dev,
		"49==0x%04x  53==0x%04x  63==0x%04x  64==0x%04x  75==0x%04x\n"
		"80==0x%04x  81==0x%04x  82==0x%04x  83==0x%04x  84==0x%04x\n"
		"88==0x%04x  93==0x%04x\n",
		id[49], id[53], id[63], id[64], id[75], id[80],
		id[81], id[82], id[83], id[84], id[88], id[93]);
}

 
unsigned int ata_id_xfermask(const u16 *id)
{
	unsigned int pio_mask, mwdma_mask, udma_mask;

	 
	if (id[ATA_ID_FIELD_VALID] & (1 << 1)) {
		pio_mask = id[ATA_ID_PIO_MODES] & 0x03;
		pio_mask <<= 3;
		pio_mask |= 0x7;
	} else {
		 
		u8 mode = (id[ATA_ID_OLD_PIO_MODES] >> 8) & 0xFF;
		if (mode < 5)	 
			pio_mask = (2 << mode) - 1;
		else
			pio_mask = 1;

		 
	}

	mwdma_mask = id[ATA_ID_MWDMA_MODES] & 0x07;

	if (ata_id_is_cfa(id)) {
		 
		int pio = (id[ATA_ID_CFA_MODES] >> 0) & 0x7;
		int dma = (id[ATA_ID_CFA_MODES] >> 3) & 0x7;

		if (pio)
			pio_mask |= (1 << 5);
		if (pio > 1)
			pio_mask |= (1 << 6);
		if (dma)
			mwdma_mask |= (1 << 3);
		if (dma > 1)
			mwdma_mask |= (1 << 4);
	}

	udma_mask = 0;
	if (id[ATA_ID_FIELD_VALID] & (1 << 2))
		udma_mask = id[ATA_ID_UDMA_MODES] & 0xff;

	return ata_pack_xfermask(pio_mask, mwdma_mask, udma_mask);
}
EXPORT_SYMBOL_GPL(ata_id_xfermask);

static void ata_qc_complete_internal(struct ata_queued_cmd *qc)
{
	struct completion *waiting = qc->private_data;

	complete(waiting);
}

 
static unsigned ata_exec_internal_sg(struct ata_device *dev,
				     struct ata_taskfile *tf, const u8 *cdb,
				     int dma_dir, struct scatterlist *sgl,
				     unsigned int n_elem, unsigned int timeout)
{
	struct ata_link *link = dev->link;
	struct ata_port *ap = link->ap;
	u8 command = tf->command;
	int auto_timeout = 0;
	struct ata_queued_cmd *qc;
	unsigned int preempted_tag;
	u32 preempted_sactive;
	u64 preempted_qc_active;
	int preempted_nr_active_links;
	DECLARE_COMPLETION_ONSTACK(wait);
	unsigned long flags;
	unsigned int err_mask;
	int rc;

	spin_lock_irqsave(ap->lock, flags);

	 
	if (ata_port_is_frozen(ap)) {
		spin_unlock_irqrestore(ap->lock, flags);
		return AC_ERR_SYSTEM;
	}

	 
	qc = __ata_qc_from_tag(ap, ATA_TAG_INTERNAL);

	qc->tag = ATA_TAG_INTERNAL;
	qc->hw_tag = 0;
	qc->scsicmd = NULL;
	qc->ap = ap;
	qc->dev = dev;
	ata_qc_reinit(qc);

	preempted_tag = link->active_tag;
	preempted_sactive = link->sactive;
	preempted_qc_active = ap->qc_active;
	preempted_nr_active_links = ap->nr_active_links;
	link->active_tag = ATA_TAG_POISON;
	link->sactive = 0;
	ap->qc_active = 0;
	ap->nr_active_links = 0;

	 
	qc->tf = *tf;
	if (cdb)
		memcpy(qc->cdb, cdb, ATAPI_CDB_LEN);

	 
	if (tf->protocol == ATAPI_PROT_DMA && (dev->flags & ATA_DFLAG_DMADIR) &&
	    dma_dir == DMA_FROM_DEVICE)
		qc->tf.feature |= ATAPI_DMADIR;

	qc->flags |= ATA_QCFLAG_RESULT_TF;
	qc->dma_dir = dma_dir;
	if (dma_dir != DMA_NONE) {
		unsigned int i, buflen = 0;
		struct scatterlist *sg;

		for_each_sg(sgl, sg, n_elem, i)
			buflen += sg->length;

		ata_sg_init(qc, sgl, n_elem);
		qc->nbytes = buflen;
	}

	qc->private_data = &wait;
	qc->complete_fn = ata_qc_complete_internal;

	ata_qc_issue(qc);

	spin_unlock_irqrestore(ap->lock, flags);

	if (!timeout) {
		if (ata_probe_timeout)
			timeout = ata_probe_timeout * 1000;
		else {
			timeout = ata_internal_cmd_timeout(dev, command);
			auto_timeout = 1;
		}
	}

	ata_eh_release(ap);

	rc = wait_for_completion_timeout(&wait, msecs_to_jiffies(timeout));

	ata_eh_acquire(ap);

	ata_sff_flush_pio_task(ap);

	if (!rc) {
		spin_lock_irqsave(ap->lock, flags);

		 
		if (qc->flags & ATA_QCFLAG_ACTIVE) {
			qc->err_mask |= AC_ERR_TIMEOUT;

			ata_port_freeze(ap);

			ata_dev_warn(dev, "qc timeout after %u msecs (cmd 0x%x)\n",
				     timeout, command);
		}

		spin_unlock_irqrestore(ap->lock, flags);
	}

	 
	if (ap->ops->post_internal_cmd)
		ap->ops->post_internal_cmd(qc);

	 
	if (qc->flags & ATA_QCFLAG_EH) {
		if (qc->result_tf.status & (ATA_ERR | ATA_DF))
			qc->err_mask |= AC_ERR_DEV;

		if (!qc->err_mask)
			qc->err_mask |= AC_ERR_OTHER;

		if (qc->err_mask & ~AC_ERR_OTHER)
			qc->err_mask &= ~AC_ERR_OTHER;
	} else if (qc->tf.command == ATA_CMD_REQ_SENSE_DATA) {
		qc->result_tf.status |= ATA_SENSE;
	}

	 
	spin_lock_irqsave(ap->lock, flags);

	*tf = qc->result_tf;
	err_mask = qc->err_mask;

	ata_qc_free(qc);
	link->active_tag = preempted_tag;
	link->sactive = preempted_sactive;
	ap->qc_active = preempted_qc_active;
	ap->nr_active_links = preempted_nr_active_links;

	spin_unlock_irqrestore(ap->lock, flags);

	if ((err_mask & AC_ERR_TIMEOUT) && auto_timeout)
		ata_internal_cmd_timed_out(dev, command);

	return err_mask;
}

 
unsigned ata_exec_internal(struct ata_device *dev,
			   struct ata_taskfile *tf, const u8 *cdb,
			   int dma_dir, void *buf, unsigned int buflen,
			   unsigned int timeout)
{
	struct scatterlist *psg = NULL, sg;
	unsigned int n_elem = 0;

	if (dma_dir != DMA_NONE) {
		WARN_ON(!buf);
		sg_init_one(&sg, buf, buflen);
		psg = &sg;
		n_elem++;
	}

	return ata_exec_internal_sg(dev, tf, cdb, dma_dir, psg, n_elem,
				    timeout);
}

 
unsigned int ata_pio_need_iordy(const struct ata_device *adev)
{
	 
	if (adev->link->ap->pflags & ATA_PFLAG_RESETTING)
		return 0;
	 
	if (adev->link->ap->flags & ATA_FLAG_NO_IORDY)
		return 0;
	 
	if (ata_id_is_cfa(adev->id)
	    && (adev->pio_mode == XFER_PIO_5 || adev->pio_mode == XFER_PIO_6))
		return 0;
	 
	if (adev->pio_mode > XFER_PIO_2)
		return 1;
	 
	if (ata_id_has_iordy(adev->id))
		return 1;
	return 0;
}
EXPORT_SYMBOL_GPL(ata_pio_need_iordy);

 
static u32 ata_pio_mask_no_iordy(const struct ata_device *adev)
{
	 
	if (adev->id[ATA_ID_FIELD_VALID] & 2) {	 
		u16 pio = adev->id[ATA_ID_EIDE_PIO];
		 
		if (pio) {
			 
			if (pio > 240)	 
				return 3 << ATA_SHIFT_PIO;
			return 7 << ATA_SHIFT_PIO;
		}
	}
	return 3 << ATA_SHIFT_PIO;
}

 
unsigned int ata_do_dev_read_id(struct ata_device *dev,
				struct ata_taskfile *tf, __le16 *id)
{
	return ata_exec_internal(dev, tf, NULL, DMA_FROM_DEVICE,
				     id, sizeof(id[0]) * ATA_ID_WORDS, 0);
}
EXPORT_SYMBOL_GPL(ata_do_dev_read_id);

 
int ata_dev_read_id(struct ata_device *dev, unsigned int *p_class,
		    unsigned int flags, u16 *id)
{
	struct ata_port *ap = dev->link->ap;
	unsigned int class = *p_class;
	struct ata_taskfile tf;
	unsigned int err_mask = 0;
	const char *reason;
	bool is_semb = class == ATA_DEV_SEMB;
	int may_fallback = 1, tried_spinup = 0;
	int rc;

retry:
	ata_tf_init(dev, &tf);

	switch (class) {
	case ATA_DEV_SEMB:
		class = ATA_DEV_ATA;	 
		fallthrough;
	case ATA_DEV_ATA:
	case ATA_DEV_ZAC:
		tf.command = ATA_CMD_ID_ATA;
		break;
	case ATA_DEV_ATAPI:
		tf.command = ATA_CMD_ID_ATAPI;
		break;
	default:
		rc = -ENODEV;
		reason = "unsupported class";
		goto err_out;
	}

	tf.protocol = ATA_PROT_PIO;

	 
	tf.flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE;

	 
	tf.flags |= ATA_TFLAG_POLLING;

	if (ap->ops->read_id)
		err_mask = ap->ops->read_id(dev, &tf, (__le16 *)id);
	else
		err_mask = ata_do_dev_read_id(dev, &tf, (__le16 *)id);

	if (err_mask) {
		if (err_mask & AC_ERR_NODEV_HINT) {
			ata_dev_dbg(dev, "NODEV after polling detection\n");
			return -ENOENT;
		}

		if (is_semb) {
			ata_dev_info(dev,
		     "IDENTIFY failed on device w/ SEMB sig, disabled\n");
			 
			*p_class = ATA_DEV_SEMB_UNSUP;
			return 0;
		}

		if ((err_mask == AC_ERR_DEV) && (tf.error & ATA_ABORTED)) {
			 
			if (may_fallback) {
				may_fallback = 0;

				if (class == ATA_DEV_ATA)
					class = ATA_DEV_ATAPI;
				else
					class = ATA_DEV_ATA;
				goto retry;
			}

			 
			ata_dev_dbg(dev,
				    "both IDENTIFYs aborted, assuming NODEV\n");
			return -ENOENT;
		}

		rc = -EIO;
		reason = "I/O error";
		goto err_out;
	}

	if (dev->horkage & ATA_HORKAGE_DUMP_ID) {
		ata_dev_info(dev, "dumping IDENTIFY data, "
			    "class=%d may_fallback=%d tried_spinup=%d\n",
			    class, may_fallback, tried_spinup);
		print_hex_dump(KERN_INFO, "", DUMP_PREFIX_OFFSET,
			       16, 2, id, ATA_ID_WORDS * sizeof(*id), true);
	}

	 
	may_fallback = 0;

	swap_buf_le16(id, ATA_ID_WORDS);

	 
	rc = -EINVAL;
	reason = "device reports invalid type";

	if (class == ATA_DEV_ATA || class == ATA_DEV_ZAC) {
		if (!ata_id_is_ata(id) && !ata_id_is_cfa(id))
			goto err_out;
		if (ap->host->flags & ATA_HOST_IGNORE_ATA &&
							ata_id_is_ata(id)) {
			ata_dev_dbg(dev,
				"host indicates ignore ATA devices, ignored\n");
			return -ENOENT;
		}
	} else {
		if (ata_id_is_ata(id))
			goto err_out;
	}

	if (!tried_spinup && (id[2] == 0x37c8 || id[2] == 0x738c)) {
		tried_spinup = 1;
		 
		err_mask = ata_dev_set_feature(dev, SETFEATURES_SPINUP, 0);
		if (err_mask && id[2] != 0x738c) {
			rc = -EIO;
			reason = "SPINUP failed";
			goto err_out;
		}
		 
		if (id[2] == 0x37c8)
			goto retry;
	}

	if ((flags & ATA_READID_POSTRESET) &&
	    (class == ATA_DEV_ATA || class == ATA_DEV_ZAC)) {
		 
		if (ata_id_major_version(id) < 4 || !ata_id_has_lba(id)) {
			err_mask = ata_dev_init_params(dev, id[3], id[6]);
			if (err_mask) {
				rc = -EIO;
				reason = "INIT_DEV_PARAMS failed";
				goto err_out;
			}

			 
			flags &= ~ATA_READID_POSTRESET;
			goto retry;
		}
	}

	*p_class = class;

	return 0;

 err_out:
	ata_dev_warn(dev, "failed to IDENTIFY (%s, err_mask=0x%x)\n",
		     reason, err_mask);
	return rc;
}

 
void ata_dev_power_set_standby(struct ata_device *dev)
{
	unsigned long ap_flags = dev->link->ap->flags;
	struct ata_taskfile tf;
	unsigned int err_mask;

	 
	if (dev->class != ATA_DEV_ATA && dev->class != ATA_DEV_ZAC)
		return;

	 
	if ((ap_flags & ATA_FLAG_NO_POWEROFF_SPINDOWN) &&
	    system_state == SYSTEM_POWER_OFF)
		return;

	if ((ap_flags & ATA_FLAG_NO_HIBERNATE_SPINDOWN) &&
	    system_entering_hibernation())
		return;

	ata_tf_init(dev, &tf);
	tf.flags |= ATA_TFLAG_DEVICE | ATA_TFLAG_ISADDR;
	tf.protocol = ATA_PROT_NODATA;
	tf.command = ATA_CMD_STANDBYNOW1;

	ata_dev_notice(dev, "Entering standby power mode\n");

	err_mask = ata_exec_internal(dev, &tf, NULL, DMA_NONE, NULL, 0, 0);
	if (err_mask)
		ata_dev_err(dev, "STANDBY IMMEDIATE failed (err_mask=0x%x)\n",
			    err_mask);
}

 
void ata_dev_power_set_active(struct ata_device *dev)
{
	struct ata_taskfile tf;
	unsigned int err_mask;

	 
	if (dev->class != ATA_DEV_ATA && dev->class != ATA_DEV_ZAC)
		return;

	ata_tf_init(dev, &tf);
	tf.flags |= ATA_TFLAG_DEVICE | ATA_TFLAG_ISADDR;
	tf.protocol = ATA_PROT_NODATA;
	tf.command = ATA_CMD_VERIFY;
	tf.nsect = 1;
	if (dev->flags & ATA_DFLAG_LBA) {
		tf.flags |= ATA_TFLAG_LBA;
		tf.device |= ATA_LBA;
	} else {
		 
		tf.lbal = 0x1;  
	}

	ata_dev_notice(dev, "Entering active power mode\n");

	err_mask = ata_exec_internal(dev, &tf, NULL, DMA_NONE, NULL, 0, 0);
	if (err_mask)
		ata_dev_err(dev, "VERIFY failed (err_mask=0x%x)\n",
			    err_mask);
}

 
unsigned int ata_read_log_page(struct ata_device *dev, u8 log,
			       u8 page, void *buf, unsigned int sectors)
{
	unsigned long ap_flags = dev->link->ap->flags;
	struct ata_taskfile tf;
	unsigned int err_mask;
	bool dma = false;

	ata_dev_dbg(dev, "read log page - log 0x%x, page 0x%x\n", log, page);

	 
	if (ap_flags & ATA_FLAG_NO_LOG_PAGE)
		return AC_ERR_DEV;

retry:
	ata_tf_init(dev, &tf);
	if (ata_dma_enabled(dev) && ata_id_has_read_log_dma_ext(dev->id) &&
	    !(dev->horkage & ATA_HORKAGE_NO_DMA_LOG)) {
		tf.command = ATA_CMD_READ_LOG_DMA_EXT;
		tf.protocol = ATA_PROT_DMA;
		dma = true;
	} else {
		tf.command = ATA_CMD_READ_LOG_EXT;
		tf.protocol = ATA_PROT_PIO;
		dma = false;
	}
	tf.lbal = log;
	tf.lbam = page;
	tf.nsect = sectors;
	tf.hob_nsect = sectors >> 8;
	tf.flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_LBA48 | ATA_TFLAG_DEVICE;

	err_mask = ata_exec_internal(dev, &tf, NULL, DMA_FROM_DEVICE,
				     buf, sectors * ATA_SECT_SIZE, 0);

	if (err_mask) {
		if (dma) {
			dev->horkage |= ATA_HORKAGE_NO_DMA_LOG;
			if (!ata_port_is_frozen(dev->link->ap))
				goto retry;
		}
		ata_dev_err(dev,
			    "Read log 0x%02x page 0x%02x failed, Emask 0x%x\n",
			    (unsigned int)log, (unsigned int)page, err_mask);
	}

	return err_mask;
}

static int ata_log_supported(struct ata_device *dev, u8 log)
{
	struct ata_port *ap = dev->link->ap;

	if (dev->horkage & ATA_HORKAGE_NO_LOG_DIR)
		return 0;

	if (ata_read_log_page(dev, ATA_LOG_DIRECTORY, 0, ap->sector_buf, 1))
		return 0;
	return get_unaligned_le16(&ap->sector_buf[log * 2]);
}

static bool ata_identify_page_supported(struct ata_device *dev, u8 page)
{
	struct ata_port *ap = dev->link->ap;
	unsigned int err, i;

	if (dev->horkage & ATA_HORKAGE_NO_ID_DEV_LOG)
		return false;

	if (!ata_log_supported(dev, ATA_LOG_IDENTIFY_DEVICE)) {
		 
		if (ata_id_major_version(dev->id) >= 10)
			ata_dev_warn(dev,
				"ATA Identify Device Log not supported\n");
		dev->horkage |= ATA_HORKAGE_NO_ID_DEV_LOG;
		return false;
	}

	 
	err = ata_read_log_page(dev, ATA_LOG_IDENTIFY_DEVICE, 0, ap->sector_buf,
				1);
	if (err)
		return false;

	for (i = 0; i < ap->sector_buf[8]; i++) {
		if (ap->sector_buf[9 + i] == page)
			return true;
	}

	return false;
}

static int ata_do_link_spd_horkage(struct ata_device *dev)
{
	struct ata_link *plink = ata_dev_phys_link(dev);
	u32 target, target_limit;

	if (!sata_scr_valid(plink))
		return 0;

	if (dev->horkage & ATA_HORKAGE_1_5_GBPS)
		target = 1;
	else
		return 0;

	target_limit = (1 << target) - 1;

	 
	if (plink->sata_spd_limit <= target_limit)
		return 0;

	plink->sata_spd_limit = target_limit;

	 
	if (plink->sata_spd > target) {
		ata_dev_info(dev, "applying link speed limit horkage to %s\n",
			     sata_spd_string(target));
		return -EAGAIN;
	}
	return 0;
}

static inline u8 ata_dev_knobble(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;

	if (ata_dev_blacklisted(dev) & ATA_HORKAGE_BRIDGE_OK)
		return 0;

	return ((ap->cbl == ATA_CBL_SATA) && (!ata_id_is_sata(dev->id)));
}

static void ata_dev_config_ncq_send_recv(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;
	unsigned int err_mask;

	if (!ata_log_supported(dev, ATA_LOG_NCQ_SEND_RECV)) {
		ata_dev_warn(dev, "NCQ Send/Recv Log not supported\n");
		return;
	}
	err_mask = ata_read_log_page(dev, ATA_LOG_NCQ_SEND_RECV,
				     0, ap->sector_buf, 1);
	if (!err_mask) {
		u8 *cmds = dev->ncq_send_recv_cmds;

		dev->flags |= ATA_DFLAG_NCQ_SEND_RECV;
		memcpy(cmds, ap->sector_buf, ATA_LOG_NCQ_SEND_RECV_SIZE);

		if (dev->horkage & ATA_HORKAGE_NO_NCQ_TRIM) {
			ata_dev_dbg(dev, "disabling queued TRIM support\n");
			cmds[ATA_LOG_NCQ_SEND_RECV_DSM_OFFSET] &=
				~ATA_LOG_NCQ_SEND_RECV_DSM_TRIM;
		}
	}
}

static void ata_dev_config_ncq_non_data(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;
	unsigned int err_mask;

	if (!ata_log_supported(dev, ATA_LOG_NCQ_NON_DATA)) {
		ata_dev_warn(dev,
			     "NCQ Send/Recv Log not supported\n");
		return;
	}
	err_mask = ata_read_log_page(dev, ATA_LOG_NCQ_NON_DATA,
				     0, ap->sector_buf, 1);
	if (!err_mask) {
		u8 *cmds = dev->ncq_non_data_cmds;

		memcpy(cmds, ap->sector_buf, ATA_LOG_NCQ_NON_DATA_SIZE);
	}
}

static void ata_dev_config_ncq_prio(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;
	unsigned int err_mask;

	if (!ata_identify_page_supported(dev, ATA_LOG_SATA_SETTINGS))
		return;

	err_mask = ata_read_log_page(dev,
				     ATA_LOG_IDENTIFY_DEVICE,
				     ATA_LOG_SATA_SETTINGS,
				     ap->sector_buf,
				     1);
	if (err_mask)
		goto not_supported;

	if (!(ap->sector_buf[ATA_LOG_NCQ_PRIO_OFFSET] & BIT(3)))
		goto not_supported;

	dev->flags |= ATA_DFLAG_NCQ_PRIO;

	return;

not_supported:
	dev->flags &= ~ATA_DFLAG_NCQ_PRIO_ENABLED;
	dev->flags &= ~ATA_DFLAG_NCQ_PRIO;
}

static bool ata_dev_check_adapter(struct ata_device *dev,
				  unsigned short vendor_id)
{
	struct pci_dev *pcidev = NULL;
	struct device *parent_dev = NULL;

	for (parent_dev = dev->tdev.parent; parent_dev != NULL;
	     parent_dev = parent_dev->parent) {
		if (dev_is_pci(parent_dev)) {
			pcidev = to_pci_dev(parent_dev);
			if (pcidev->vendor == vendor_id)
				return true;
			break;
		}
	}

	return false;
}

static int ata_dev_config_ncq(struct ata_device *dev,
			       char *desc, size_t desc_sz)
{
	struct ata_port *ap = dev->link->ap;
	int hdepth = 0, ddepth = ata_id_queue_depth(dev->id);
	unsigned int err_mask;
	char *aa_desc = "";

	if (!ata_id_has_ncq(dev->id)) {
		desc[0] = '\0';
		return 0;
	}
	if (!IS_ENABLED(CONFIG_SATA_HOST))
		return 0;
	if (dev->horkage & ATA_HORKAGE_NONCQ) {
		snprintf(desc, desc_sz, "NCQ (not used)");
		return 0;
	}

	if (dev->horkage & ATA_HORKAGE_NO_NCQ_ON_ATI &&
	    ata_dev_check_adapter(dev, PCI_VENDOR_ID_ATI)) {
		snprintf(desc, desc_sz, "NCQ (not used)");
		return 0;
	}

	if (ap->flags & ATA_FLAG_NCQ) {
		hdepth = min(ap->scsi_host->can_queue, ATA_MAX_QUEUE);
		dev->flags |= ATA_DFLAG_NCQ;
	}

	if (!(dev->horkage & ATA_HORKAGE_BROKEN_FPDMA_AA) &&
		(ap->flags & ATA_FLAG_FPDMA_AA) &&
		ata_id_has_fpdma_aa(dev->id)) {
		err_mask = ata_dev_set_feature(dev, SETFEATURES_SATA_ENABLE,
			SATA_FPDMA_AA);
		if (err_mask) {
			ata_dev_err(dev,
				    "failed to enable AA (error_mask=0x%x)\n",
				    err_mask);
			if (err_mask != AC_ERR_DEV) {
				dev->horkage |= ATA_HORKAGE_BROKEN_FPDMA_AA;
				return -EIO;
			}
		} else
			aa_desc = ", AA";
	}

	if (hdepth >= ddepth)
		snprintf(desc, desc_sz, "NCQ (depth %d)%s", ddepth, aa_desc);
	else
		snprintf(desc, desc_sz, "NCQ (depth %d/%d)%s", hdepth,
			ddepth, aa_desc);

	if ((ap->flags & ATA_FLAG_FPDMA_AUX)) {
		if (ata_id_has_ncq_send_and_recv(dev->id))
			ata_dev_config_ncq_send_recv(dev);
		if (ata_id_has_ncq_non_data(dev->id))
			ata_dev_config_ncq_non_data(dev);
		if (ata_id_has_ncq_prio(dev->id))
			ata_dev_config_ncq_prio(dev);
	}

	return 0;
}

static void ata_dev_config_sense_reporting(struct ata_device *dev)
{
	unsigned int err_mask;

	if (!ata_id_has_sense_reporting(dev->id))
		return;

	if (ata_id_sense_reporting_enabled(dev->id))
		return;

	err_mask = ata_dev_set_feature(dev, SETFEATURE_SENSE_DATA, 0x1);
	if (err_mask) {
		ata_dev_dbg(dev,
			    "failed to enable Sense Data Reporting, Emask 0x%x\n",
			    err_mask);
	}
}

static void ata_dev_config_zac(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;
	unsigned int err_mask;
	u8 *identify_buf = ap->sector_buf;

	dev->zac_zones_optimal_open = U32_MAX;
	dev->zac_zones_optimal_nonseq = U32_MAX;
	dev->zac_zones_max_open = U32_MAX;

	 
	if (dev->class == ATA_DEV_ZAC)
		dev->flags |= ATA_DFLAG_ZAC;
	else if (ata_id_zoned_cap(dev->id) == 0x01)
		 
		dev->flags |= ATA_DFLAG_ZAC;

	if (!(dev->flags & ATA_DFLAG_ZAC))
		return;

	if (!ata_identify_page_supported(dev, ATA_LOG_ZONED_INFORMATION)) {
		ata_dev_warn(dev,
			     "ATA Zoned Information Log not supported\n");
		return;
	}

	 
	err_mask = ata_read_log_page(dev, ATA_LOG_IDENTIFY_DEVICE,
				     ATA_LOG_ZONED_INFORMATION,
				     identify_buf, 1);
	if (!err_mask) {
		u64 zoned_cap, opt_open, opt_nonseq, max_open;

		zoned_cap = get_unaligned_le64(&identify_buf[8]);
		if ((zoned_cap >> 63))
			dev->zac_zoned_cap = (zoned_cap & 1);
		opt_open = get_unaligned_le64(&identify_buf[24]);
		if ((opt_open >> 63))
			dev->zac_zones_optimal_open = (u32)opt_open;
		opt_nonseq = get_unaligned_le64(&identify_buf[32]);
		if ((opt_nonseq >> 63))
			dev->zac_zones_optimal_nonseq = (u32)opt_nonseq;
		max_open = get_unaligned_le64(&identify_buf[40]);
		if ((max_open >> 63))
			dev->zac_zones_max_open = (u32)max_open;
	}
}

static void ata_dev_config_trusted(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;
	u64 trusted_cap;
	unsigned int err;

	if (!ata_id_has_trusted(dev->id))
		return;

	if (!ata_identify_page_supported(dev, ATA_LOG_SECURITY)) {
		ata_dev_warn(dev,
			     "Security Log not supported\n");
		return;
	}

	err = ata_read_log_page(dev, ATA_LOG_IDENTIFY_DEVICE, ATA_LOG_SECURITY,
			ap->sector_buf, 1);
	if (err)
		return;

	trusted_cap = get_unaligned_le64(&ap->sector_buf[40]);
	if (!(trusted_cap & (1ULL << 63))) {
		ata_dev_dbg(dev,
			    "Trusted Computing capability qword not valid!\n");
		return;
	}

	if (trusted_cap & (1 << 0))
		dev->flags |= ATA_DFLAG_TRUSTED;
}

static void ata_dev_config_cdl(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;
	unsigned int err_mask;
	bool cdl_enabled;
	u64 val;

	if (ata_id_major_version(dev->id) < 12)
		goto not_supported;

	if (!ata_log_supported(dev, ATA_LOG_IDENTIFY_DEVICE) ||
	    !ata_identify_page_supported(dev, ATA_LOG_SUPPORTED_CAPABILITIES) ||
	    !ata_identify_page_supported(dev, ATA_LOG_CURRENT_SETTINGS))
		goto not_supported;

	err_mask = ata_read_log_page(dev, ATA_LOG_IDENTIFY_DEVICE,
				     ATA_LOG_SUPPORTED_CAPABILITIES,
				     ap->sector_buf, 1);
	if (err_mask)
		goto not_supported;

	 
	val = get_unaligned_le64(&ap->sector_buf[168]);
	if (!(val & BIT_ULL(63)) || !(val & BIT_ULL(0)))
		goto not_supported;

	 
	if (!(val & BIT_ULL(1)))
		ata_dev_warn(dev,
			"Command duration guideline is not supported\n");

	 
	val = get_unaligned_le64(&ap->sector_buf[8]);
	if (!(val & BIT_ULL(63)) || !(val & BIT_ULL(47))) {
		ata_dev_warn(dev,
			"CDL supported but Successful NCQ Command Sense Data is not supported\n");
		goto not_supported;
	}

	 
	if (!ata_id_has_ncq_autosense(dev->id)) {
		ata_dev_warn(dev,
			"CDL supported but NCQ autosense is not supported\n");
		goto not_supported;
	}

	 
	err_mask = ata_read_log_page(dev, ATA_LOG_IDENTIFY_DEVICE,
				     ATA_LOG_CURRENT_SETTINGS,
				     ap->sector_buf, 1);
	if (err_mask)
		goto not_supported;

	val = get_unaligned_le64(&ap->sector_buf[8]);
	cdl_enabled = val & BIT_ULL(63) && val & BIT_ULL(21);
	if (dev->flags & ATA_DFLAG_CDL_ENABLED) {
		if (!cdl_enabled) {
			 
			err_mask = ata_dev_set_feature(dev, SETFEATURES_CDL, 1);
			if (err_mask) {
				ata_dev_err(dev,
					    "Enable CDL feature failed\n");
				goto not_supported;
			}
		}
	} else {
		if (cdl_enabled) {
			 
			err_mask = ata_dev_set_feature(dev, SETFEATURES_CDL, 0);
			if (err_mask) {
				ata_dev_err(dev,
					    "Disable CDL feature failed\n");
				goto not_supported;
			}
		}
	}

	 
	if (!(val & BIT_ULL(63)) || !(val & BIT_ULL(18))) {
		err_mask = ata_dev_set_feature(dev,
					SETFEATURE_SENSE_DATA_SUCC_NCQ, 0x1);
		if (err_mask) {
			ata_dev_warn(dev,
				     "failed to enable Sense Data for successful NCQ commands, Emask 0x%x\n",
				     err_mask);
			goto not_supported;
		}
	}

	 
	if (!ap->ncq_sense_buf) {
		ap->ncq_sense_buf = kmalloc(ATA_LOG_SENSE_NCQ_SIZE, GFP_KERNEL);
		if (!ap->ncq_sense_buf)
			goto not_supported;
	}

	 
	err_mask = ata_read_log_page(dev, ATA_LOG_CDL, 0, ap->sector_buf, 1);
	if (err_mask) {
		ata_dev_warn(dev, "Read Command Duration Limits log failed\n");
		goto not_supported;
	}

	memcpy(dev->cdl, ap->sector_buf, ATA_LOG_CDL_SIZE);
	dev->flags |= ATA_DFLAG_CDL;

	return;

not_supported:
	dev->flags &= ~(ATA_DFLAG_CDL | ATA_DFLAG_CDL_ENABLED);
	kfree(ap->ncq_sense_buf);
	ap->ncq_sense_buf = NULL;
}

static int ata_dev_config_lba(struct ata_device *dev)
{
	const u16 *id = dev->id;
	const char *lba_desc;
	char ncq_desc[32];
	int ret;

	dev->flags |= ATA_DFLAG_LBA;

	if (ata_id_has_lba48(id)) {
		lba_desc = "LBA48";
		dev->flags |= ATA_DFLAG_LBA48;
		if (dev->n_sectors >= (1UL << 28) &&
		    ata_id_has_flush_ext(id))
			dev->flags |= ATA_DFLAG_FLUSH_EXT;
	} else {
		lba_desc = "LBA";
	}

	 
	ret = ata_dev_config_ncq(dev, ncq_desc, sizeof(ncq_desc));

	 
	if (ata_dev_print_info(dev))
		ata_dev_info(dev,
			     "%llu sectors, multi %u: %s %s\n",
			     (unsigned long long)dev->n_sectors,
			     dev->multi_count, lba_desc, ncq_desc);

	return ret;
}

static void ata_dev_config_chs(struct ata_device *dev)
{
	const u16 *id = dev->id;

	if (ata_id_current_chs_valid(id)) {
		 
		dev->cylinders = id[54];
		dev->heads     = id[55];
		dev->sectors   = id[56];
	} else {
		 
		dev->cylinders	= id[1];
		dev->heads	= id[3];
		dev->sectors	= id[6];
	}

	 
	if (ata_dev_print_info(dev))
		ata_dev_info(dev,
			     "%llu sectors, multi %u, CHS %u/%u/%u\n",
			     (unsigned long long)dev->n_sectors,
			     dev->multi_count, dev->cylinders,
			     dev->heads, dev->sectors);
}

static void ata_dev_config_fua(struct ata_device *dev)
{
	 
	if (!libata_fua)
		goto nofua;

	 
	if (!(dev->flags & ATA_DFLAG_LBA48) || !ata_id_has_fua(dev->id))
		goto nofua;

	 
	if (!ata_ncq_supported(dev) || (dev->horkage & ATA_HORKAGE_NO_FUA))
		goto nofua;

	dev->flags |= ATA_DFLAG_FUA;

	return;

nofua:
	dev->flags &= ~ATA_DFLAG_FUA;
}

static void ata_dev_config_devslp(struct ata_device *dev)
{
	u8 *sata_setting = dev->link->ap->sector_buf;
	unsigned int err_mask;
	int i, j;

	 
	if (!ata_id_has_devslp(dev->id) ||
	    !ata_identify_page_supported(dev, ATA_LOG_SATA_SETTINGS))
		return;

	err_mask = ata_read_log_page(dev,
				     ATA_LOG_IDENTIFY_DEVICE,
				     ATA_LOG_SATA_SETTINGS,
				     sata_setting, 1);
	if (err_mask)
		return;

	dev->flags |= ATA_DFLAG_DEVSLP;
	for (i = 0; i < ATA_LOG_DEVSLP_SIZE; i++) {
		j = ATA_LOG_DEVSLP_OFFSET + i;
		dev->devslp_timing[i] = sata_setting[j];
	}
}

static void ata_dev_config_cpr(struct ata_device *dev)
{
	unsigned int err_mask;
	size_t buf_len;
	int i, nr_cpr = 0;
	struct ata_cpr_log *cpr_log = NULL;
	u8 *desc, *buf = NULL;

	if (ata_id_major_version(dev->id) < 11)
		goto out;

	buf_len = ata_log_supported(dev, ATA_LOG_CONCURRENT_POSITIONING_RANGES);
	if (buf_len == 0)
		goto out;

	 
	buf_len <<= 9;
	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		goto out;

	err_mask = ata_read_log_page(dev, ATA_LOG_CONCURRENT_POSITIONING_RANGES,
				     0, buf, buf_len >> 9);
	if (err_mask)
		goto out;

	nr_cpr = buf[0];
	if (!nr_cpr)
		goto out;

	cpr_log = kzalloc(struct_size(cpr_log, cpr, nr_cpr), GFP_KERNEL);
	if (!cpr_log)
		goto out;

	cpr_log->nr_cpr = nr_cpr;
	desc = &buf[64];
	for (i = 0; i < nr_cpr; i++, desc += 32) {
		cpr_log->cpr[i].num = desc[0];
		cpr_log->cpr[i].num_storage_elements = desc[1];
		cpr_log->cpr[i].start_lba = get_unaligned_le64(&desc[8]);
		cpr_log->cpr[i].num_lbas = get_unaligned_le64(&desc[16]);
	}

out:
	swap(dev->cpr_log, cpr_log);
	kfree(cpr_log);
	kfree(buf);
}

static void ata_dev_print_features(struct ata_device *dev)
{
	if (!(dev->flags & ATA_DFLAG_FEATURES_MASK))
		return;

	ata_dev_info(dev,
		     "Features:%s%s%s%s%s%s%s%s\n",
		     dev->flags & ATA_DFLAG_FUA ? " FUA" : "",
		     dev->flags & ATA_DFLAG_TRUSTED ? " Trust" : "",
		     dev->flags & ATA_DFLAG_DA ? " Dev-Attention" : "",
		     dev->flags & ATA_DFLAG_DEVSLP ? " Dev-Sleep" : "",
		     dev->flags & ATA_DFLAG_NCQ_SEND_RECV ? " NCQ-sndrcv" : "",
		     dev->flags & ATA_DFLAG_NCQ_PRIO ? " NCQ-prio" : "",
		     dev->flags & ATA_DFLAG_CDL ? " CDL" : "",
		     dev->cpr_log ? " CPR" : "");
}

 
int ata_dev_configure(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;
	bool print_info = ata_dev_print_info(dev);
	const u16 *id = dev->id;
	unsigned int xfer_mask;
	unsigned int err_mask;
	char revbuf[7];		 
	char fwrevbuf[ATA_ID_FW_REV_LEN+1];
	char modelbuf[ATA_ID_PROD_LEN+1];
	int rc;

	if (!ata_dev_enabled(dev)) {
		ata_dev_dbg(dev, "no device\n");
		return 0;
	}

	 
	dev->horkage |= ata_dev_blacklisted(dev);
	ata_force_horkage(dev);

	if (dev->horkage & ATA_HORKAGE_DISABLE) {
		ata_dev_info(dev, "unsupported device, disabling\n");
		ata_dev_disable(dev);
		return 0;
	}

	if ((!atapi_enabled || (ap->flags & ATA_FLAG_NO_ATAPI)) &&
	    dev->class == ATA_DEV_ATAPI) {
		ata_dev_warn(dev, "WARNING: ATAPI is %s, device ignored\n",
			     atapi_enabled ? "not supported with this driver"
			     : "disabled");
		ata_dev_disable(dev);
		return 0;
	}

	rc = ata_do_link_spd_horkage(dev);
	if (rc)
		return rc;

	 
	if ((dev->horkage & ATA_HORKAGE_WD_BROKEN_LPM) &&
	    (id[ATA_ID_SATA_CAPABILITY] & 0xe) == 0x2)
		dev->horkage |= ATA_HORKAGE_NOLPM;

	if (ap->flags & ATA_FLAG_NO_LPM)
		dev->horkage |= ATA_HORKAGE_NOLPM;

	if (dev->horkage & ATA_HORKAGE_NOLPM) {
		ata_dev_warn(dev, "LPM support broken, forcing max_power\n");
		dev->link->ap->target_lpm_policy = ATA_LPM_MAX_POWER;
	}

	 
	rc = ata_acpi_on_devcfg(dev);
	if (rc)
		return rc;

	 
	rc = ata_hpa_resize(dev);
	if (rc)
		return rc;

	 
	ata_dev_dbg(dev,
		    "%s: cfg 49:%04x 82:%04x 83:%04x 84:%04x "
		    "85:%04x 86:%04x 87:%04x 88:%04x\n",
		    __func__,
		    id[49], id[82], id[83], id[84],
		    id[85], id[86], id[87], id[88]);

	 
	dev->flags &= ~ATA_DFLAG_CFG_MASK;
	dev->max_sectors = 0;
	dev->cdb_len = 0;
	dev->n_sectors = 0;
	dev->cylinders = 0;
	dev->heads = 0;
	dev->sectors = 0;
	dev->multi_count = 0;

	 

	 
	xfer_mask = ata_id_xfermask(id);

	ata_dump_id(dev, id);

	 
	ata_id_c_string(dev->id, fwrevbuf, ATA_ID_FW_REV,
			sizeof(fwrevbuf));

	ata_id_c_string(dev->id, modelbuf, ATA_ID_PROD,
			sizeof(modelbuf));

	 
	if (dev->class == ATA_DEV_ATA || dev->class == ATA_DEV_ZAC) {
		if (ata_id_is_cfa(id)) {
			 
			if (id[ATA_ID_CFA_KEY_MGMT] & 1)
				ata_dev_warn(dev,
	"supports DRM functions and may not be fully accessible\n");
			snprintf(revbuf, 7, "CFA");
		} else {
			snprintf(revbuf, 7, "ATA-%d", ata_id_major_version(id));
			 
			if (ata_id_has_tpm(id))
				ata_dev_warn(dev,
	"supports DRM functions and may not be fully accessible\n");
		}

		dev->n_sectors = ata_id_n_sectors(id);

		 
		if ((dev->id[47] >> 8) == 0x80 && (dev->id[59] & 0x100)) {
			unsigned int max = dev->id[47] & 0xff;
			unsigned int cnt = dev->id[59] & 0xff;
			 
			if (is_power_of_2(max) && is_power_of_2(cnt))
				if (cnt <= max)
					dev->multi_count = cnt;
		}

		 
		if (print_info)
			ata_dev_info(dev, "%s: %s, %s, max %s\n",
				     revbuf, modelbuf, fwrevbuf,
				     ata_mode_string(xfer_mask));

		if (ata_id_has_lba(id)) {
			rc = ata_dev_config_lba(dev);
			if (rc)
				return rc;
		} else {
			ata_dev_config_chs(dev);
		}

		ata_dev_config_fua(dev);
		ata_dev_config_devslp(dev);
		ata_dev_config_sense_reporting(dev);
		ata_dev_config_zac(dev);
		ata_dev_config_trusted(dev);
		ata_dev_config_cpr(dev);
		ata_dev_config_cdl(dev);
		dev->cdb_len = 32;

		if (print_info)
			ata_dev_print_features(dev);
	}

	 
	else if (dev->class == ATA_DEV_ATAPI) {
		const char *cdb_intr_string = "";
		const char *atapi_an_string = "";
		const char *dma_dir_string = "";
		u32 sntf;

		rc = atapi_cdb_len(id);
		if ((rc < 12) || (rc > ATAPI_CDB_LEN)) {
			ata_dev_warn(dev, "unsupported CDB len %d\n", rc);
			rc = -EINVAL;
			goto err_out_nosup;
		}
		dev->cdb_len = (unsigned int) rc;

		 
		if (atapi_an &&
		    (ap->flags & ATA_FLAG_AN) && ata_id_has_atapi_AN(id) &&
		    (!sata_pmp_attached(ap) ||
		     sata_scr_read(&ap->link, SCR_NOTIFICATION, &sntf) == 0)) {
			 
			err_mask = ata_dev_set_feature(dev,
					SETFEATURES_SATA_ENABLE, SATA_AN);
			if (err_mask)
				ata_dev_err(dev,
					    "failed to enable ATAPI AN (err_mask=0x%x)\n",
					    err_mask);
			else {
				dev->flags |= ATA_DFLAG_AN;
				atapi_an_string = ", ATAPI AN";
			}
		}

		if (ata_id_cdb_intr(dev->id)) {
			dev->flags |= ATA_DFLAG_CDB_INTR;
			cdb_intr_string = ", CDB intr";
		}

		if (atapi_dmadir || (dev->horkage & ATA_HORKAGE_ATAPI_DMADIR) || atapi_id_dmadir(dev->id)) {
			dev->flags |= ATA_DFLAG_DMADIR;
			dma_dir_string = ", DMADIR";
		}

		if (ata_id_has_da(dev->id)) {
			dev->flags |= ATA_DFLAG_DA;
			zpodd_init(dev);
		}

		 
		if (print_info)
			ata_dev_info(dev,
				     "ATAPI: %s, %s, max %s%s%s%s\n",
				     modelbuf, fwrevbuf,
				     ata_mode_string(xfer_mask),
				     cdb_intr_string, atapi_an_string,
				     dma_dir_string);
	}

	 
	dev->max_sectors = ATA_MAX_SECTORS;
	if (dev->flags & ATA_DFLAG_LBA48)
		dev->max_sectors = ATA_MAX_SECTORS_LBA48;

	 
	if (ata_dev_knobble(dev)) {
		if (print_info)
			ata_dev_info(dev, "applying bridge limits\n");
		dev->udma_mask &= ATA_UDMA5;
		dev->max_sectors = ATA_MAX_SECTORS;
	}

	if ((dev->class == ATA_DEV_ATAPI) &&
	    (atapi_command_packet_set(id) == TYPE_TAPE)) {
		dev->max_sectors = ATA_MAX_SECTORS_TAPE;
		dev->horkage |= ATA_HORKAGE_STUCK_ERR;
	}

	if (dev->horkage & ATA_HORKAGE_MAX_SEC_128)
		dev->max_sectors = min_t(unsigned int, ATA_MAX_SECTORS_128,
					 dev->max_sectors);

	if (dev->horkage & ATA_HORKAGE_MAX_SEC_1024)
		dev->max_sectors = min_t(unsigned int, ATA_MAX_SECTORS_1024,
					 dev->max_sectors);

	if (dev->horkage & ATA_HORKAGE_MAX_SEC_LBA48)
		dev->max_sectors = ATA_MAX_SECTORS_LBA48;

	if (ap->ops->dev_config)
		ap->ops->dev_config(dev);

	if (dev->horkage & ATA_HORKAGE_DIAGNOSTIC) {
		 

		if (print_info) {
			ata_dev_warn(dev,
"Drive reports diagnostics failure. This may indicate a drive\n");
			ata_dev_warn(dev,
"fault or invalid emulation. Contact drive vendor for information.\n");
		}
	}

	if ((dev->horkage & ATA_HORKAGE_FIRMWARE_WARN) && print_info) {
		ata_dev_warn(dev, "WARNING: device requires firmware update to be fully functional\n");
		ata_dev_warn(dev, "         contact the vendor or visit http://ata.wiki.kernel.org\n");
	}

	return 0;

err_out_nosup:
	return rc;
}

 

int ata_cable_40wire(struct ata_port *ap)
{
	return ATA_CBL_PATA40;
}
EXPORT_SYMBOL_GPL(ata_cable_40wire);

 

int ata_cable_80wire(struct ata_port *ap)
{
	return ATA_CBL_PATA80;
}
EXPORT_SYMBOL_GPL(ata_cable_80wire);

 

int ata_cable_unknown(struct ata_port *ap)
{
	return ATA_CBL_PATA_UNK;
}
EXPORT_SYMBOL_GPL(ata_cable_unknown);

 
int ata_cable_ignore(struct ata_port *ap)
{
	return ATA_CBL_PATA_IGN;
}
EXPORT_SYMBOL_GPL(ata_cable_ignore);

 

int ata_cable_sata(struct ata_port *ap)
{
	return ATA_CBL_SATA;
}
EXPORT_SYMBOL_GPL(ata_cable_sata);

 
static void sata_print_link_status(struct ata_link *link)
{
	u32 sstatus, scontrol, tmp;

	if (sata_scr_read(link, SCR_STATUS, &sstatus))
		return;
	if (sata_scr_read(link, SCR_CONTROL, &scontrol))
		return;

	if (ata_phys_link_online(link)) {
		tmp = (sstatus >> 4) & 0xf;
		ata_link_info(link, "SATA link up %s (SStatus %X SControl %X)\n",
			      sata_spd_string(tmp), sstatus, scontrol);
	} else {
		ata_link_info(link, "SATA link down (SStatus %X SControl %X)\n",
			      sstatus, scontrol);
	}
}

 

struct ata_device *ata_dev_pair(struct ata_device *adev)
{
	struct ata_link *link = adev->link;
	struct ata_device *pair = &link->device[1 - adev->devno];
	if (!ata_dev_enabled(pair))
		return NULL;
	return pair;
}
EXPORT_SYMBOL_GPL(ata_dev_pair);

 
int sata_down_spd_limit(struct ata_link *link, u32 spd_limit)
{
	u32 sstatus, spd, mask;
	int rc, bit;

	if (!sata_scr_valid(link))
		return -EOPNOTSUPP;

	 
	rc = sata_scr_read(link, SCR_STATUS, &sstatus);
	if (rc == 0 && ata_sstatus_online(sstatus))
		spd = (sstatus >> 4) & 0xf;
	else
		spd = link->sata_spd;

	mask = link->sata_spd_limit;
	if (mask <= 1)
		return -EINVAL;

	 
	bit = fls(mask) - 1;
	mask &= ~(1 << bit);

	 
	if (spd > 1)
		mask &= (1 << (spd - 1)) - 1;
	else if (link->sata_spd)
		return -EINVAL;

	 
	if (!mask)
		return -EINVAL;

	if (spd_limit) {
		if (mask & ((1 << spd_limit) - 1))
			mask &= (1 << spd_limit) - 1;
		else {
			bit = ffs(mask) - 1;
			mask = 1 << bit;
		}
	}

	link->sata_spd_limit = mask;

	ata_link_warn(link, "limiting SATA link speed to %s\n",
		      sata_spd_string(fls(mask)));

	return 0;
}

#ifdef CONFIG_ATA_ACPI
 
u8 ata_timing_cycle2mode(unsigned int xfer_shift, int cycle)
{
	u8 base_mode = 0xff, last_mode = 0xff;
	const struct ata_xfer_ent *ent;
	const struct ata_timing *t;

	for (ent = ata_xfer_tbl; ent->shift >= 0; ent++)
		if (ent->shift == xfer_shift)
			base_mode = ent->base;

	for (t = ata_timing_find_mode(base_mode);
	     t && ata_xfer_mode2shift(t->mode) == xfer_shift; t++) {
		unsigned short this_cycle;

		switch (xfer_shift) {
		case ATA_SHIFT_PIO:
		case ATA_SHIFT_MWDMA:
			this_cycle = t->cycle;
			break;
		case ATA_SHIFT_UDMA:
			this_cycle = t->udma;
			break;
		default:
			return 0xff;
		}

		if (cycle > this_cycle)
			break;

		last_mode = t->mode;
	}

	return last_mode;
}
#endif

 
int ata_down_xfermask_limit(struct ata_device *dev, unsigned int sel)
{
	char buf[32];
	unsigned int orig_mask, xfer_mask;
	unsigned int pio_mask, mwdma_mask, udma_mask;
	int quiet, highbit;

	quiet = !!(sel & ATA_DNXFER_QUIET);
	sel &= ~ATA_DNXFER_QUIET;

	xfer_mask = orig_mask = ata_pack_xfermask(dev->pio_mask,
						  dev->mwdma_mask,
						  dev->udma_mask);
	ata_unpack_xfermask(xfer_mask, &pio_mask, &mwdma_mask, &udma_mask);

	switch (sel) {
	case ATA_DNXFER_PIO:
		highbit = fls(pio_mask) - 1;
		pio_mask &= ~(1 << highbit);
		break;

	case ATA_DNXFER_DMA:
		if (udma_mask) {
			highbit = fls(udma_mask) - 1;
			udma_mask &= ~(1 << highbit);
			if (!udma_mask)
				return -ENOENT;
		} else if (mwdma_mask) {
			highbit = fls(mwdma_mask) - 1;
			mwdma_mask &= ~(1 << highbit);
			if (!mwdma_mask)
				return -ENOENT;
		}
		break;

	case ATA_DNXFER_40C:
		udma_mask &= ATA_UDMA_MASK_40C;
		break;

	case ATA_DNXFER_FORCE_PIO0:
		pio_mask &= 1;
		fallthrough;
	case ATA_DNXFER_FORCE_PIO:
		mwdma_mask = 0;
		udma_mask = 0;
		break;

	default:
		BUG();
	}

	xfer_mask &= ata_pack_xfermask(pio_mask, mwdma_mask, udma_mask);

	if (!(xfer_mask & ATA_MASK_PIO) || xfer_mask == orig_mask)
		return -ENOENT;

	if (!quiet) {
		if (xfer_mask & (ATA_MASK_MWDMA | ATA_MASK_UDMA))
			snprintf(buf, sizeof(buf), "%s:%s",
				 ata_mode_string(xfer_mask),
				 ata_mode_string(xfer_mask & ATA_MASK_PIO));
		else
			snprintf(buf, sizeof(buf), "%s",
				 ata_mode_string(xfer_mask));

		ata_dev_warn(dev, "limiting speed to %s\n", buf);
	}

	ata_unpack_xfermask(xfer_mask, &dev->pio_mask, &dev->mwdma_mask,
			    &dev->udma_mask);

	return 0;
}

static int ata_dev_set_mode(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;
	struct ata_eh_context *ehc = &dev->link->eh_context;
	const bool nosetxfer = dev->horkage & ATA_HORKAGE_NOSETXFER;
	const char *dev_err_whine = "";
	int ign_dev_err = 0;
	unsigned int err_mask = 0;
	int rc;

	dev->flags &= ~ATA_DFLAG_PIO;
	if (dev->xfer_shift == ATA_SHIFT_PIO)
		dev->flags |= ATA_DFLAG_PIO;

	if (nosetxfer && ap->flags & ATA_FLAG_SATA && ata_id_is_sata(dev->id))
		dev_err_whine = " (SET_XFERMODE skipped)";
	else {
		if (nosetxfer)
			ata_dev_warn(dev,
				     "NOSETXFER but PATA detected - can't "
				     "skip SETXFER, might malfunction\n");
		err_mask = ata_dev_set_xfermode(dev);
	}

	if (err_mask & ~AC_ERR_DEV)
		goto fail;

	 
	ehc->i.flags |= ATA_EHI_POST_SETMODE;
	rc = ata_dev_revalidate(dev, ATA_DEV_UNKNOWN, 0);
	ehc->i.flags &= ~ATA_EHI_POST_SETMODE;
	if (rc)
		return rc;

	if (dev->xfer_shift == ATA_SHIFT_PIO) {
		 
		if (ata_id_is_cfa(dev->id))
			ign_dev_err = 1;
		 
		if (ata_id_major_version(dev->id) == 0 &&
					dev->pio_mode <= XFER_PIO_2)
			ign_dev_err = 1;
		 
		if (!ata_id_has_iordy(dev->id) && dev->pio_mode <= XFER_PIO_2)
			ign_dev_err = 1;
	}
	 
	if (dev->xfer_shift == ATA_SHIFT_MWDMA &&
	    dev->dma_mode == XFER_MW_DMA_0 &&
	    (dev->id[63] >> 8) & 1)
		ign_dev_err = 1;

	 
	if (dev->xfer_mode == ata_xfer_mask2mode(ata_id_xfermask(dev->id)))
		ign_dev_err = 1;

	if (err_mask & AC_ERR_DEV) {
		if (!ign_dev_err)
			goto fail;
		else
			dev_err_whine = " (device error ignored)";
	}

	ata_dev_dbg(dev, "xfer_shift=%u, xfer_mode=0x%x\n",
		    dev->xfer_shift, (int)dev->xfer_mode);

	if (!(ehc->i.flags & ATA_EHI_QUIET) ||
	    ehc->i.flags & ATA_EHI_DID_HARDRESET)
		ata_dev_info(dev, "configured for %s%s\n",
			     ata_mode_string(ata_xfer_mode2mask(dev->xfer_mode)),
			     dev_err_whine);

	return 0;

 fail:
	ata_dev_err(dev, "failed to set xfermode (err_mask=0x%x)\n", err_mask);
	return -EIO;
}

 

int ata_do_set_mode(struct ata_link *link, struct ata_device **r_failed_dev)
{
	struct ata_port *ap = link->ap;
	struct ata_device *dev;
	int rc = 0, used_dma = 0, found = 0;

	 
	ata_for_each_dev(dev, link, ENABLED) {
		unsigned int pio_mask, dma_mask;
		unsigned int mode_mask;

		mode_mask = ATA_DMA_MASK_ATA;
		if (dev->class == ATA_DEV_ATAPI)
			mode_mask = ATA_DMA_MASK_ATAPI;
		else if (ata_id_is_cfa(dev->id))
			mode_mask = ATA_DMA_MASK_CFA;

		ata_dev_xfermask(dev);
		ata_force_xfermask(dev);

		pio_mask = ata_pack_xfermask(dev->pio_mask, 0, 0);

		if (libata_dma_mask & mode_mask)
			dma_mask = ata_pack_xfermask(0, dev->mwdma_mask,
						     dev->udma_mask);
		else
			dma_mask = 0;

		dev->pio_mode = ata_xfer_mask2mode(pio_mask);
		dev->dma_mode = ata_xfer_mask2mode(dma_mask);

		found = 1;
		if (ata_dma_enabled(dev))
			used_dma = 1;
	}
	if (!found)
		goto out;

	 
	ata_for_each_dev(dev, link, ENABLED) {
		if (dev->pio_mode == 0xff) {
			ata_dev_warn(dev, "no PIO support\n");
			rc = -EINVAL;
			goto out;
		}

		dev->xfer_mode = dev->pio_mode;
		dev->xfer_shift = ATA_SHIFT_PIO;
		if (ap->ops->set_piomode)
			ap->ops->set_piomode(ap, dev);
	}

	 
	ata_for_each_dev(dev, link, ENABLED) {
		if (!ata_dma_enabled(dev))
			continue;

		dev->xfer_mode = dev->dma_mode;
		dev->xfer_shift = ata_xfer_mode2shift(dev->dma_mode);
		if (ap->ops->set_dmamode)
			ap->ops->set_dmamode(ap, dev);
	}

	 
	ata_for_each_dev(dev, link, ENABLED) {
		rc = ata_dev_set_mode(dev);
		if (rc)
			goto out;
	}

	 
	if (used_dma && (ap->host->flags & ATA_HOST_SIMPLEX))
		ap->host->simplex_claimed = ap;

 out:
	if (rc)
		*r_failed_dev = dev;
	return rc;
}
EXPORT_SYMBOL_GPL(ata_do_set_mode);

 
int ata_wait_ready(struct ata_link *link, unsigned long deadline,
		   int (*check_ready)(struct ata_link *link))
{
	unsigned long start = jiffies;
	unsigned long nodev_deadline;
	int warned = 0;

	 
	if (link->ap->host->flags & ATA_HOST_PARALLEL_SCAN)
		nodev_deadline = ata_deadline(start, ATA_TMOUT_FF_WAIT_LONG);
	else
		nodev_deadline = ata_deadline(start, ATA_TMOUT_FF_WAIT);

	 
	WARN_ON(link == link->ap->slave_link);

	if (time_after(nodev_deadline, deadline))
		nodev_deadline = deadline;

	while (1) {
		unsigned long now = jiffies;
		int ready, tmp;

		ready = tmp = check_ready(link);
		if (ready > 0)
			return 0;

		 
		if (ready == -ENODEV) {
			if (ata_link_online(link))
				ready = 0;
			else if ((link->ap->flags & ATA_FLAG_SATA) &&
				 !ata_link_offline(link) &&
				 time_before(now, nodev_deadline))
				ready = 0;
		}

		if (ready)
			return ready;
		if (time_after(now, deadline))
			return -EBUSY;

		if (!warned && time_after(now, start + 5 * HZ) &&
		    (deadline - now > 3 * HZ)) {
			ata_link_warn(link,
				"link is slow to respond, please be patient "
				"(ready=%d)\n", tmp);
			warned = 1;
		}

		ata_msleep(link->ap, 50);
	}
}

 
int ata_wait_after_reset(struct ata_link *link, unsigned long deadline,
				int (*check_ready)(struct ata_link *link))
{
	ata_msleep(link->ap, ATA_WAIT_AFTER_RESET);

	return ata_wait_ready(link, deadline, check_ready);
}
EXPORT_SYMBOL_GPL(ata_wait_after_reset);

 
int ata_std_prereset(struct ata_link *link, unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	struct ata_eh_context *ehc = &link->eh_context;
	const unsigned int *timing = sata_ehc_deb_timing(ehc);
	int rc;

	 
	if (ehc->i.action & ATA_EH_HARDRESET)
		return 0;

	 
	if (ap->flags & ATA_FLAG_SATA) {
		rc = sata_link_resume(link, timing, deadline);
		 
		if (rc && rc != -EOPNOTSUPP)
			ata_link_warn(link,
				      "failed to resume link for reset (errno=%d)\n",
				      rc);
	}

	 
	if (ata_phys_link_offline(link))
		ehc->i.action &= ~ATA_EH_SOFTRESET;

	return 0;
}
EXPORT_SYMBOL_GPL(ata_std_prereset);

 
int sata_std_hardreset(struct ata_link *link, unsigned int *class,
		       unsigned long deadline)
{
	const unsigned int *timing = sata_ehc_deb_timing(&link->eh_context);
	bool online;
	int rc;

	 
	rc = sata_link_hardreset(link, timing, deadline, &online, NULL);
	return online ? -EAGAIN : rc;
}
EXPORT_SYMBOL_GPL(sata_std_hardreset);

 
void ata_std_postreset(struct ata_link *link, unsigned int *classes)
{
	u32 serror;

	 
	if (!sata_scr_read(link, SCR_ERROR, &serror))
		sata_scr_write(link, SCR_ERROR, serror);

	 
	sata_print_link_status(link);
}
EXPORT_SYMBOL_GPL(ata_std_postreset);

 
static int ata_dev_same_device(struct ata_device *dev, unsigned int new_class,
			       const u16 *new_id)
{
	const u16 *old_id = dev->id;
	unsigned char model[2][ATA_ID_PROD_LEN + 1];
	unsigned char serial[2][ATA_ID_SERNO_LEN + 1];

	if (dev->class != new_class) {
		ata_dev_info(dev, "class mismatch %d != %d\n",
			     dev->class, new_class);
		return 0;
	}

	ata_id_c_string(old_id, model[0], ATA_ID_PROD, sizeof(model[0]));
	ata_id_c_string(new_id, model[1], ATA_ID_PROD, sizeof(model[1]));
	ata_id_c_string(old_id, serial[0], ATA_ID_SERNO, sizeof(serial[0]));
	ata_id_c_string(new_id, serial[1], ATA_ID_SERNO, sizeof(serial[1]));

	if (strcmp(model[0], model[1])) {
		ata_dev_info(dev, "model number mismatch '%s' != '%s'\n",
			     model[0], model[1]);
		return 0;
	}

	if (strcmp(serial[0], serial[1])) {
		ata_dev_info(dev, "serial number mismatch '%s' != '%s'\n",
			     serial[0], serial[1]);
		return 0;
	}

	return 1;
}

 
int ata_dev_reread_id(struct ata_device *dev, unsigned int readid_flags)
{
	unsigned int class = dev->class;
	u16 *id = (void *)dev->link->ap->sector_buf;
	int rc;

	 
	rc = ata_dev_read_id(dev, &class, readid_flags, id);
	if (rc)
		return rc;

	 
	if (!ata_dev_same_device(dev, class, id))
		return -ENODEV;

	memcpy(dev->id, id, sizeof(id[0]) * ATA_ID_WORDS);
	return 0;
}

 
int ata_dev_revalidate(struct ata_device *dev, unsigned int new_class,
		       unsigned int readid_flags)
{
	u64 n_sectors = dev->n_sectors;
	u64 n_native_sectors = dev->n_native_sectors;
	int rc;

	if (!ata_dev_enabled(dev))
		return -ENODEV;

	 
	if (ata_class_enabled(new_class) && new_class == ATA_DEV_PMP) {
		ata_dev_info(dev, "class mismatch %u != %u\n",
			     dev->class, new_class);
		rc = -ENODEV;
		goto fail;
	}

	 
	rc = ata_dev_reread_id(dev, readid_flags);
	if (rc)
		goto fail;

	 
	rc = ata_dev_configure(dev);
	if (rc)
		goto fail;

	 
	if (dev->class != ATA_DEV_ATA || !n_sectors ||
	    dev->n_sectors == n_sectors)
		return 0;

	 
	ata_dev_warn(dev, "n_sectors mismatch %llu != %llu\n",
		     (unsigned long long)n_sectors,
		     (unsigned long long)dev->n_sectors);

	 
	if (dev->n_native_sectors == n_native_sectors &&
	    dev->n_sectors > n_sectors && dev->n_sectors == n_native_sectors) {
		ata_dev_warn(dev,
			     "new n_sectors matches native, probably "
			     "late HPA unlock, n_sectors updated\n");
		 
		return 0;
	}

	 
	if (dev->n_native_sectors == n_native_sectors &&
	    dev->n_sectors < n_sectors && n_sectors == n_native_sectors &&
	    !(dev->horkage & ATA_HORKAGE_BROKEN_HPA)) {
		ata_dev_warn(dev,
			     "old n_sectors matches native, probably "
			     "late HPA lock, will try to unlock HPA\n");
		 
		dev->flags |= ATA_DFLAG_UNLOCK_HPA;
		rc = -EIO;
	} else
		rc = -ENODEV;

	 
	dev->n_native_sectors = n_native_sectors;
	dev->n_sectors = n_sectors;
 fail:
	ata_dev_err(dev, "revalidation failed (errno=%d)\n", rc);
	return rc;
}

struct ata_blacklist_entry {
	const char *model_num;
	const char *model_rev;
	unsigned long horkage;
};

static const struct ata_blacklist_entry ata_device_blacklist [] = {
	 
	{ "WDC AC11000H",	NULL,		ATA_HORKAGE_NODMA },
	{ "WDC AC22100H",	NULL,		ATA_HORKAGE_NODMA },
	{ "WDC AC32500H",	NULL,		ATA_HORKAGE_NODMA },
	{ "WDC AC33100H",	NULL,		ATA_HORKAGE_NODMA },
	{ "WDC AC31600H",	NULL,		ATA_HORKAGE_NODMA },
	{ "WDC AC32100H",	"24.09P07",	ATA_HORKAGE_NODMA },
	{ "WDC AC23200L",	"21.10N21",	ATA_HORKAGE_NODMA },
	{ "Compaq CRD-8241B", 	NULL,		ATA_HORKAGE_NODMA },
	{ "CRD-8400B",		NULL, 		ATA_HORKAGE_NODMA },
	{ "CRD-848[02]B",	NULL,		ATA_HORKAGE_NODMA },
	{ "CRD-84",		NULL,		ATA_HORKAGE_NODMA },
	{ "SanDisk SDP3B",	NULL,		ATA_HORKAGE_NODMA },
	{ "SanDisk SDP3B-64",	NULL,		ATA_HORKAGE_NODMA },
	{ "SANYO CD-ROM CRD",	NULL,		ATA_HORKAGE_NODMA },
	{ "HITACHI CDR-8",	NULL,		ATA_HORKAGE_NODMA },
	{ "HITACHI CDR-8[34]35",NULL,		ATA_HORKAGE_NODMA },
	{ "Toshiba CD-ROM XM-6202B", NULL,	ATA_HORKAGE_NODMA },
	{ "TOSHIBA CD-ROM XM-1702BC", NULL,	ATA_HORKAGE_NODMA },
	{ "CD-532E-A", 		NULL,		ATA_HORKAGE_NODMA },
	{ "E-IDE CD-ROM CR-840",NULL,		ATA_HORKAGE_NODMA },
	{ "CD-ROM Drive/F5A",	NULL,		ATA_HORKAGE_NODMA },
	{ "WPI CDD-820", 	NULL,		ATA_HORKAGE_NODMA },
	{ "SAMSUNG CD-ROM SC-148C", NULL,	ATA_HORKAGE_NODMA },
	{ "SAMSUNG CD-ROM SC",	NULL,		ATA_HORKAGE_NODMA },
	{ "ATAPI CD-ROM DRIVE 40X MAXIMUM",NULL,ATA_HORKAGE_NODMA },
	{ "_NEC DV5800A", 	NULL,		ATA_HORKAGE_NODMA },
	{ "SAMSUNG CD-ROM SN-124", "N001",	ATA_HORKAGE_NODMA },
	{ "Seagate STT20000A", NULL,		ATA_HORKAGE_NODMA },
	{ " 2GB ATA Flash Disk", "ADMA428M",	ATA_HORKAGE_NODMA },
	{ "VRFDFC22048UCHC-TE*", NULL,		ATA_HORKAGE_NODMA },
	 
	{ "Config  Disk",	NULL,		ATA_HORKAGE_DISABLE },
	 
	{ "ASMT109x- Config",	NULL,		ATA_HORKAGE_DISABLE },

	 
	{ "TORiSAN DVD-ROM DRD-N216", NULL,	ATA_HORKAGE_MAX_SEC_128 },
	{ "QUANTUM DAT    DAT72-000", NULL,	ATA_HORKAGE_ATAPI_MOD16_DMA },
	{ "Slimtype DVD A  DS8A8SH", NULL,	ATA_HORKAGE_MAX_SEC_LBA48 },
	{ "Slimtype DVD A  DS8A9SH", NULL,	ATA_HORKAGE_MAX_SEC_LBA48 },

	 
	{ "ST380013AS",		"3.20",		ATA_HORKAGE_MAX_SEC_1024 },

	 
	{ "LITEON CX1-JB*-HP",	NULL,		ATA_HORKAGE_MAX_SEC_1024 },
	{ "LITEON EP1-*",	NULL,		ATA_HORKAGE_MAX_SEC_1024 },

	 

	 
	 
	{ "WDC WD740ADFD-00",	NULL,		ATA_HORKAGE_NONCQ },
	{ "WDC WD740ADFD-00NLR1", NULL,		ATA_HORKAGE_NONCQ },
	 
	{ "FUJITSU MHT2060BH",	NULL,		ATA_HORKAGE_NONCQ },
	 
	{ "Maxtor *",		"BANC*",	ATA_HORKAGE_NONCQ },
	{ "Maxtor 7V300F0",	"VA111630",	ATA_HORKAGE_NONCQ },
	{ "ST380817AS",		"3.42",		ATA_HORKAGE_NONCQ },
	{ "ST3160023AS",	"3.42",		ATA_HORKAGE_NONCQ },
	{ "OCZ CORE_SSD",	"02.10104",	ATA_HORKAGE_NONCQ },

	 
	{ "ST31500341AS",	"SD1[5-9]",	ATA_HORKAGE_NONCQ |
						ATA_HORKAGE_FIRMWARE_WARN },

	{ "ST31000333AS",	"SD1[5-9]",	ATA_HORKAGE_NONCQ |
						ATA_HORKAGE_FIRMWARE_WARN },

	{ "ST3640[36]23AS",	"SD1[5-9]",	ATA_HORKAGE_NONCQ |
						ATA_HORKAGE_FIRMWARE_WARN },

	{ "ST3320[68]13AS",	"SD1[5-9]",	ATA_HORKAGE_NONCQ |
						ATA_HORKAGE_FIRMWARE_WARN },

	 
	{ "ST1000LM024 HN-M101MBB", NULL,	ATA_HORKAGE_BROKEN_FPDMA_AA |
						ATA_HORKAGE_NOLPM },
	{ "VB0250EAVER",	"HPG7",		ATA_HORKAGE_BROKEN_FPDMA_AA },

	 
	{ "HTS541060G9SA00",    "MB3OC60D",     ATA_HORKAGE_NONCQ },
	{ "HTS541080G9SA00",    "MB4OC60D",     ATA_HORKAGE_NONCQ },
	{ "HTS541010G9SA00",    "MBZOC60D",     ATA_HORKAGE_NONCQ },

	 
	{ "C300-CTFDDAC128MAG",	"0001",		ATA_HORKAGE_NONCQ },

	 
	{ "SanDisk SD[789]*",	NULL,		ATA_HORKAGE_MAX_TRIM_128M },

	 
	{ "HDS724040KLSA80",	"KFAOA20N",	ATA_HORKAGE_BROKEN_HPA },
	{ "WDC WD3200JD-00KLB0", "WD-WCAMR1130137", ATA_HORKAGE_BROKEN_HPA },
	{ "WDC WD2500JD-00HBB0", "WD-WMAL71490727", ATA_HORKAGE_BROKEN_HPA },
	{ "MAXTOR 6L080L4",	"A93.0500",	ATA_HORKAGE_BROKEN_HPA },

	 
	{ "OCZ-VERTEX",		    "1.30",	ATA_HORKAGE_BROKEN_HPA },

	 
	{ "ST340823A",		NULL,		ATA_HORKAGE_HPA_SIZE },
	{ "ST320413A",		NULL,		ATA_HORKAGE_HPA_SIZE },
	{ "ST310211A",		NULL,		ATA_HORKAGE_HPA_SIZE },

	 
	{ "QUANTUM FIREBALLlct10 05", "A03.0900", ATA_HORKAGE_IVB },
	 
	{ "TSSTcorp CDDVDW SH-S202[HJN]", "SB0[01]",  ATA_HORKAGE_IVB },

	 
	{ "MTRON MSP-SATA*",		NULL,	ATA_HORKAGE_BRIDGE_OK },
	{ "BUFFALO HD-QSU2/R5",		NULL,	ATA_HORKAGE_BRIDGE_OK },

	 
	{ "WD My Book",			NULL,	ATA_HORKAGE_1_5_GBPS },
	{ "Seagate FreeAgent GoFlex",	NULL,	ATA_HORKAGE_1_5_GBPS },

	 
	{ "PIONEER DVD-RW  DVRTD08",	NULL,	ATA_HORKAGE_NOSETXFER },
	{ "PIONEER DVD-RW  DVRTD08A",	NULL,	ATA_HORKAGE_NOSETXFER },
	{ "PIONEER DVD-RW  DVR-215",	NULL,	ATA_HORKAGE_NOSETXFER },
	{ "PIONEER DVD-RW  DVR-212D",	NULL,	ATA_HORKAGE_NOSETXFER },
	{ "PIONEER DVD-RW  DVR-216D",	NULL,	ATA_HORKAGE_NOSETXFER },

	 
	{ "PIONEER BD-RW   BDR-207M",	NULL,	ATA_HORKAGE_NOLPM },
	{ "PIONEER BD-RW   BDR-205",	NULL,	ATA_HORKAGE_NOLPM },

	 
	{ "CT500BX100SSD1",		NULL,	ATA_HORKAGE_NOLPM },

	 
	{ "Crucial_CT512MX100*",	"MU01",	ATA_HORKAGE_NO_NCQ_TRIM |
						ATA_HORKAGE_ZERO_AFTER_TRIM |
						ATA_HORKAGE_NOLPM },
	 
	{ "Crucial_CT512MX100*",	NULL,	ATA_HORKAGE_ZERO_AFTER_TRIM |
						ATA_HORKAGE_NOLPM },

	 
	{ "Crucial_CT480M500*",		NULL,	ATA_HORKAGE_NO_NCQ_TRIM |
						ATA_HORKAGE_ZERO_AFTER_TRIM |
						ATA_HORKAGE_NOLPM },
	{ "Crucial_CT960M500*",		NULL,	ATA_HORKAGE_NO_NCQ_TRIM |
						ATA_HORKAGE_ZERO_AFTER_TRIM |
						ATA_HORKAGE_NOLPM },

	 
	{ "SAMSUNG MZMPC128HBFU-000MV", "CXM14M1Q", ATA_HORKAGE_NOLPM },
	{ "SAMSUNG SSD PM830 mSATA *",  "CXM13D1Q", ATA_HORKAGE_NOLPM },
	{ "SAMSUNG MZ7TD256HAFV-000L9", NULL,       ATA_HORKAGE_NOLPM },
	{ "SAMSUNG MZ7TE512HMHP-000L1", "EXT06L0Q", ATA_HORKAGE_NOLPM },

	 
	{ "Micron_M500IT_*",		"MU01",	ATA_HORKAGE_NO_NCQ_TRIM |
						ATA_HORKAGE_ZERO_AFTER_TRIM },
	{ "Micron_M500_*",		NULL,	ATA_HORKAGE_NO_NCQ_TRIM |
						ATA_HORKAGE_ZERO_AFTER_TRIM },
	{ "Micron_M5[15]0_*",		"MU01",	ATA_HORKAGE_NO_NCQ_TRIM |
						ATA_HORKAGE_ZERO_AFTER_TRIM },
	{ "Micron_1100_*",		NULL,	ATA_HORKAGE_NO_NCQ_TRIM |
						ATA_HORKAGE_ZERO_AFTER_TRIM, },
	{ "Crucial_CT*M500*",		NULL,	ATA_HORKAGE_NO_NCQ_TRIM |
						ATA_HORKAGE_ZERO_AFTER_TRIM },
	{ "Crucial_CT*M550*",		"MU01",	ATA_HORKAGE_NO_NCQ_TRIM |
						ATA_HORKAGE_ZERO_AFTER_TRIM },
	{ "Crucial_CT*MX100*",		"MU01",	ATA_HORKAGE_NO_NCQ_TRIM |
						ATA_HORKAGE_ZERO_AFTER_TRIM },
	{ "Samsung SSD 840 EVO*",	NULL,	ATA_HORKAGE_NO_NCQ_TRIM |
						ATA_HORKAGE_NO_DMA_LOG |
						ATA_HORKAGE_ZERO_AFTER_TRIM },
	{ "Samsung SSD 840*",		NULL,	ATA_HORKAGE_NO_NCQ_TRIM |
						ATA_HORKAGE_ZERO_AFTER_TRIM },
	{ "Samsung SSD 850*",		NULL,	ATA_HORKAGE_NO_NCQ_TRIM |
						ATA_HORKAGE_ZERO_AFTER_TRIM },
	{ "Samsung SSD 860*",		NULL,	ATA_HORKAGE_NO_NCQ_TRIM |
						ATA_HORKAGE_ZERO_AFTER_TRIM |
						ATA_HORKAGE_NO_NCQ_ON_ATI },
	{ "Samsung SSD 870*",		NULL,	ATA_HORKAGE_NO_NCQ_TRIM |
						ATA_HORKAGE_ZERO_AFTER_TRIM |
						ATA_HORKAGE_NO_NCQ_ON_ATI },
	{ "SAMSUNG*MZ7LH*",		NULL,	ATA_HORKAGE_NO_NCQ_TRIM |
						ATA_HORKAGE_ZERO_AFTER_TRIM |
						ATA_HORKAGE_NO_NCQ_ON_ATI, },
	{ "FCCT*M500*",			NULL,	ATA_HORKAGE_NO_NCQ_TRIM |
						ATA_HORKAGE_ZERO_AFTER_TRIM },

	 
	{ "SuperSSpeed S238*",		NULL,	ATA_HORKAGE_NOTRIM },
	{ "M88V29*",			NULL,	ATA_HORKAGE_NOTRIM },

	 

	 
	{ "INTEL*SSDSC2MH*",		NULL,	0 },

	{ "Micron*",			NULL,	ATA_HORKAGE_ZERO_AFTER_TRIM },
	{ "Crucial*",			NULL,	ATA_HORKAGE_ZERO_AFTER_TRIM },
	{ "INTEL*SSD*", 		NULL,	ATA_HORKAGE_ZERO_AFTER_TRIM },
	{ "SSD*INTEL*",			NULL,	ATA_HORKAGE_ZERO_AFTER_TRIM },
	{ "Samsung*SSD*",		NULL,	ATA_HORKAGE_ZERO_AFTER_TRIM },
	{ "SAMSUNG*SSD*",		NULL,	ATA_HORKAGE_ZERO_AFTER_TRIM },
	{ "SAMSUNG*MZ7KM*",		NULL,	ATA_HORKAGE_ZERO_AFTER_TRIM },
	{ "ST[1248][0248]0[FH]*",	NULL,	ATA_HORKAGE_ZERO_AFTER_TRIM },

	 
	{ "WDC WD800JD-*",		NULL,	ATA_HORKAGE_WD_BROKEN_LPM },
	{ "WDC WD1200JD-*",		NULL,	ATA_HORKAGE_WD_BROKEN_LPM },
	{ "WDC WD1600JD-*",		NULL,	ATA_HORKAGE_WD_BROKEN_LPM },
	{ "WDC WD2000JD-*",		NULL,	ATA_HORKAGE_WD_BROKEN_LPM },
	{ "WDC WD2500JD-*",		NULL,	ATA_HORKAGE_WD_BROKEN_LPM },
	{ "WDC WD3000JD-*",		NULL,	ATA_HORKAGE_WD_BROKEN_LPM },
	{ "WDC WD3200JD-*",		NULL,	ATA_HORKAGE_WD_BROKEN_LPM },

	 
	{ "SATADOM-ML 3ME",		NULL,	ATA_HORKAGE_NO_LOG_DIR },

	 
	{ "Maxtor",		"BANC1G10",	ATA_HORKAGE_NO_FUA },
	{ "WDC*WD2500J*",	NULL,		ATA_HORKAGE_NO_FUA },
	{ "OCZ-VERTEX*",	NULL,		ATA_HORKAGE_NO_FUA },
	{ "INTEL*SSDSC2CT*",	NULL,		ATA_HORKAGE_NO_FUA },

	 
	{ }
};

static unsigned long ata_dev_blacklisted(const struct ata_device *dev)
{
	unsigned char model_num[ATA_ID_PROD_LEN + 1];
	unsigned char model_rev[ATA_ID_FW_REV_LEN + 1];
	const struct ata_blacklist_entry *ad = ata_device_blacklist;

	ata_id_c_string(dev->id, model_num, ATA_ID_PROD, sizeof(model_num));
	ata_id_c_string(dev->id, model_rev, ATA_ID_FW_REV, sizeof(model_rev));

	while (ad->model_num) {
		if (glob_match(ad->model_num, model_num)) {
			if (ad->model_rev == NULL)
				return ad->horkage;
			if (glob_match(ad->model_rev, model_rev))
				return ad->horkage;
		}
		ad++;
	}
	return 0;
}

static int ata_dma_blacklisted(const struct ata_device *dev)
{
	 
	if ((dev->link->ap->flags & ATA_FLAG_PIO_POLLING) &&
	    (dev->flags & ATA_DFLAG_CDB_INTR))
		return 1;
	return (dev->horkage & ATA_HORKAGE_NODMA) ? 1 : 0;
}

 

static int ata_is_40wire(struct ata_device *dev)
{
	if (dev->horkage & ATA_HORKAGE_IVB)
		return ata_drive_40wire_relaxed(dev->id);
	return ata_drive_40wire(dev->id);
}

 

static int cable_is_40wire(struct ata_port *ap)
{
	struct ata_link *link;
	struct ata_device *dev;

	 
	if (ap->cbl == ATA_CBL_PATA40)
		return 1;

	 
	if (ap->cbl == ATA_CBL_PATA80 || ap->cbl == ATA_CBL_SATA)
		return 0;

	 
	if (ap->cbl == ATA_CBL_PATA40_SHORT)
		return 0;

	 
	ata_for_each_link(link, ap, EDGE) {
		ata_for_each_dev(dev, link, ENABLED) {
			if (!ata_is_40wire(dev))
				return 0;
		}
	}
	return 1;
}

 
static void ata_dev_xfermask(struct ata_device *dev)
{
	struct ata_link *link = dev->link;
	struct ata_port *ap = link->ap;
	struct ata_host *host = ap->host;
	unsigned int xfer_mask;

	 
	xfer_mask = ata_pack_xfermask(ap->pio_mask,
				      ap->mwdma_mask, ap->udma_mask);

	 
	xfer_mask &= ata_pack_xfermask(dev->pio_mask,
				       dev->mwdma_mask, dev->udma_mask);
	xfer_mask &= ata_id_xfermask(dev->id);

	 
	if (ata_dev_pair(dev)) {
		 
		xfer_mask &= ~(0x03 << (ATA_SHIFT_PIO + 5));
		 
		xfer_mask &= ~(0x03 << (ATA_SHIFT_MWDMA + 3));
	}

	if (ata_dma_blacklisted(dev)) {
		xfer_mask &= ~(ATA_MASK_MWDMA | ATA_MASK_UDMA);
		ata_dev_warn(dev,
			     "device is on DMA blacklist, disabling DMA\n");
	}

	if ((host->flags & ATA_HOST_SIMPLEX) &&
	    host->simplex_claimed && host->simplex_claimed != ap) {
		xfer_mask &= ~(ATA_MASK_MWDMA | ATA_MASK_UDMA);
		ata_dev_warn(dev,
			     "simplex DMA is claimed by other device, disabling DMA\n");
	}

	if (ap->flags & ATA_FLAG_NO_IORDY)
		xfer_mask &= ata_pio_mask_no_iordy(dev);

	if (ap->ops->mode_filter)
		xfer_mask = ap->ops->mode_filter(dev, xfer_mask);

	 
	if (xfer_mask & (0xF8 << ATA_SHIFT_UDMA))
		 
		if (cable_is_40wire(ap)) {
			ata_dev_warn(dev,
				     "limited to UDMA/33 due to 40-wire cable\n");
			xfer_mask &= ~(0xF8 << ATA_SHIFT_UDMA);
		}

	ata_unpack_xfermask(xfer_mask, &dev->pio_mask,
			    &dev->mwdma_mask, &dev->udma_mask);
}

 

static unsigned int ata_dev_set_xfermode(struct ata_device *dev)
{
	struct ata_taskfile tf;

	 
	ata_dev_dbg(dev, "set features - xfer mode\n");

	 
	ata_tf_init(dev, &tf);
	tf.command = ATA_CMD_SET_FEATURES;
	tf.feature = SETFEATURES_XFER;
	tf.flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE | ATA_TFLAG_POLLING;
	tf.protocol = ATA_PROT_NODATA;
	 
	if (ata_pio_need_iordy(dev))
		tf.nsect = dev->xfer_mode;
	 
 	else if (ata_id_has_iordy(dev->id))
		tf.nsect = 0x01;
	else  
		return 0;

	 
	return ata_exec_internal(dev, &tf, NULL, DMA_NONE, NULL, 0, 15000);
}

 
unsigned int ata_dev_set_feature(struct ata_device *dev, u8 subcmd, u8 action)
{
	struct ata_taskfile tf;
	unsigned int timeout = 0;

	 
	ata_dev_dbg(dev, "set features\n");

	ata_tf_init(dev, &tf);
	tf.command = ATA_CMD_SET_FEATURES;
	tf.feature = subcmd;
	tf.flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE;
	tf.protocol = ATA_PROT_NODATA;
	tf.nsect = action;

	if (subcmd == SETFEATURES_SPINUP)
		timeout = ata_probe_timeout ?
			  ata_probe_timeout * 1000 : SETFEATURES_SPINUP_TIMEOUT;

	return ata_exec_internal(dev, &tf, NULL, DMA_NONE, NULL, 0, timeout);
}
EXPORT_SYMBOL_GPL(ata_dev_set_feature);

 
static unsigned int ata_dev_init_params(struct ata_device *dev,
					u16 heads, u16 sectors)
{
	struct ata_taskfile tf;
	unsigned int err_mask;

	 
	if (sectors < 1 || sectors > 255 || heads < 1 || heads > 16)
		return AC_ERR_INVALID;

	 
	ata_dev_dbg(dev, "init dev params \n");

	ata_tf_init(dev, &tf);
	tf.command = ATA_CMD_INIT_DEV_PARAMS;
	tf.flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE;
	tf.protocol = ATA_PROT_NODATA;
	tf.nsect = sectors;
	tf.device |= (heads - 1) & 0x0f;  

	err_mask = ata_exec_internal(dev, &tf, NULL, DMA_NONE, NULL, 0, 0);
	 
	if (err_mask == AC_ERR_DEV && (tf.error & ATA_ABORTED))
		err_mask = 0;

	return err_mask;
}

 
int atapi_check_dma(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;

	 
	if (!(qc->dev->horkage & ATA_HORKAGE_ATAPI_MOD16_DMA) &&
	    unlikely(qc->nbytes & 15))
		return 1;

	if (ap->ops->check_atapi_dma)
		return ap->ops->check_atapi_dma(qc);

	return 0;
}

 
int ata_std_qc_defer(struct ata_queued_cmd *qc)
{
	struct ata_link *link = qc->dev->link;

	if (ata_is_ncq(qc->tf.protocol)) {
		if (!ata_tag_valid(link->active_tag))
			return 0;
	} else {
		if (!ata_tag_valid(link->active_tag) && !link->sactive)
			return 0;
	}

	return ATA_DEFER_LINK;
}
EXPORT_SYMBOL_GPL(ata_std_qc_defer);

enum ata_completion_errors ata_noop_qc_prep(struct ata_queued_cmd *qc)
{
	return AC_ERR_OK;
}
EXPORT_SYMBOL_GPL(ata_noop_qc_prep);

 
void ata_sg_init(struct ata_queued_cmd *qc, struct scatterlist *sg,
		 unsigned int n_elem)
{
	qc->sg = sg;
	qc->n_elem = n_elem;
	qc->cursg = qc->sg;
}

#ifdef CONFIG_HAS_DMA

 
static void ata_sg_clean(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct scatterlist *sg = qc->sg;
	int dir = qc->dma_dir;

	WARN_ON_ONCE(sg == NULL);

	if (qc->n_elem)
		dma_unmap_sg(ap->dev, sg, qc->orig_n_elem, dir);

	qc->flags &= ~ATA_QCFLAG_DMAMAP;
	qc->sg = NULL;
}

 
static int ata_sg_setup(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	unsigned int n_elem;

	n_elem = dma_map_sg(ap->dev, qc->sg, qc->n_elem, qc->dma_dir);
	if (n_elem < 1)
		return -1;

	qc->orig_n_elem = qc->n_elem;
	qc->n_elem = n_elem;
	qc->flags |= ATA_QCFLAG_DMAMAP;

	return 0;
}

#else  

static inline void ata_sg_clean(struct ata_queued_cmd *qc) {}
static inline int ata_sg_setup(struct ata_queued_cmd *qc) { return -1; }

#endif  

 
void swap_buf_le16(u16 *buf, unsigned int buf_words)
{
#ifdef __BIG_ENDIAN
	unsigned int i;

	for (i = 0; i < buf_words; i++)
		buf[i] = le16_to_cpu(buf[i]);
#endif  
}

 
void ata_qc_free(struct ata_queued_cmd *qc)
{
	qc->flags = 0;
	if (ata_tag_valid(qc->tag))
		qc->tag = ATA_TAG_POISON;
}

void __ata_qc_complete(struct ata_queued_cmd *qc)
{
	struct ata_port *ap;
	struct ata_link *link;

	WARN_ON_ONCE(qc == NULL);  
	WARN_ON_ONCE(!(qc->flags & ATA_QCFLAG_ACTIVE));
	ap = qc->ap;
	link = qc->dev->link;

	if (likely(qc->flags & ATA_QCFLAG_DMAMAP))
		ata_sg_clean(qc);

	 
	if (ata_is_ncq(qc->tf.protocol)) {
		link->sactive &= ~(1 << qc->hw_tag);
		if (!link->sactive)
			ap->nr_active_links--;
	} else {
		link->active_tag = ATA_TAG_POISON;
		ap->nr_active_links--;
	}

	 
	if (unlikely(qc->flags & ATA_QCFLAG_CLEAR_EXCL &&
		     ap->excl_link == link))
		ap->excl_link = NULL;

	 
	qc->flags &= ~ATA_QCFLAG_ACTIVE;
	ap->qc_active &= ~(1ULL << qc->tag);

	 
	qc->complete_fn(qc);
}

static void fill_result_tf(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;

	qc->result_tf.flags = qc->tf.flags;
	ap->ops->qc_fill_rtf(qc);
}

static void ata_verify_xfer(struct ata_queued_cmd *qc)
{
	struct ata_device *dev = qc->dev;

	if (!ata_is_data(qc->tf.protocol))
		return;

	if ((dev->mwdma_mask || dev->udma_mask) && ata_is_pio(qc->tf.protocol))
		return;

	dev->flags &= ~ATA_DFLAG_DUBIOUS_XFER;
}

 
void ata_qc_complete(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct ata_device *dev = qc->dev;
	struct ata_eh_info *ehi = &dev->link->eh_info;

	 
	ledtrig_disk_activity(!!(qc->tf.flags & ATA_TFLAG_WRITE));

	 
	if (unlikely(qc->err_mask))
		qc->flags |= ATA_QCFLAG_EH;

	 
	if (unlikely(ata_tag_internal(qc->tag))) {
		fill_result_tf(qc);
		trace_ata_qc_complete_internal(qc);
		__ata_qc_complete(qc);
		return;
	}

	 
	if (unlikely(qc->flags & ATA_QCFLAG_EH)) {
		fill_result_tf(qc);
		trace_ata_qc_complete_failed(qc);
		ata_qc_schedule_eh(qc);
		return;
	}

	WARN_ON_ONCE(ata_port_is_frozen(ap));

	 
	if (qc->flags & ATA_QCFLAG_RESULT_TF)
		fill_result_tf(qc);

	trace_ata_qc_complete_done(qc);

	 
	if (qc->flags & ATA_QCFLAG_HAS_CDL &&
	    qc->result_tf.status & ATA_SENSE) {
		 
		qc->scsicmd->flags |= SCMD_FORCE_EH_SUCCESS;
		qc->flags |= ATA_QCFLAG_EH_SUCCESS_CMD;
		ehi->dev_action[dev->devno] |= ATA_EH_GET_SUCCESS_SENSE;

		 
		ap->pflags |= ATA_PFLAG_EH_PENDING;
		ata_qc_schedule_eh(qc);
		return;
	}

	 
	switch (qc->tf.command) {
	case ATA_CMD_SET_FEATURES:
		if (qc->tf.feature != SETFEATURES_WC_ON &&
		    qc->tf.feature != SETFEATURES_WC_OFF &&
		    qc->tf.feature != SETFEATURES_RA_ON &&
		    qc->tf.feature != SETFEATURES_RA_OFF)
			break;
		fallthrough;
	case ATA_CMD_INIT_DEV_PARAMS:  
	case ATA_CMD_SET_MULTI:  
		 
		ehi->dev_action[dev->devno] |= ATA_EH_REVALIDATE;
		ata_port_schedule_eh(ap);
		break;

	case ATA_CMD_SLEEP:
		dev->flags |= ATA_DFLAG_SLEEPING;
		break;
	}

	if (unlikely(dev->flags & ATA_DFLAG_DUBIOUS_XFER))
		ata_verify_xfer(qc);

	__ata_qc_complete(qc);
}
EXPORT_SYMBOL_GPL(ata_qc_complete);

 
u64 ata_qc_get_active(struct ata_port *ap)
{
	u64 qc_active = ap->qc_active;

	 
	if (qc_active & (1ULL << ATA_TAG_INTERNAL)) {
		qc_active |= (1 << 0);
		qc_active &= ~(1ULL << ATA_TAG_INTERNAL);
	}

	return qc_active;
}
EXPORT_SYMBOL_GPL(ata_qc_get_active);

 
void ata_qc_issue(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct ata_link *link = qc->dev->link;
	u8 prot = qc->tf.protocol;

	 
	WARN_ON_ONCE(ata_tag_valid(link->active_tag));

	if (ata_is_ncq(prot)) {
		WARN_ON_ONCE(link->sactive & (1 << qc->hw_tag));

		if (!link->sactive)
			ap->nr_active_links++;
		link->sactive |= 1 << qc->hw_tag;
	} else {
		WARN_ON_ONCE(link->sactive);

		ap->nr_active_links++;
		link->active_tag = qc->tag;
	}

	qc->flags |= ATA_QCFLAG_ACTIVE;
	ap->qc_active |= 1ULL << qc->tag;

	 
	if (ata_is_data(prot) && (!qc->sg || !qc->n_elem || !qc->nbytes))
		goto sys_err;

	if (ata_is_dma(prot) || (ata_is_pio(prot) &&
				 (ap->flags & ATA_FLAG_PIO_DMA)))
		if (ata_sg_setup(qc))
			goto sys_err;

	 
	if (unlikely(qc->dev->flags & ATA_DFLAG_SLEEPING)) {
		link->eh_info.action |= ATA_EH_RESET;
		ata_ehi_push_desc(&link->eh_info, "waking up from sleep");
		ata_link_abort(link);
		return;
	}

	trace_ata_qc_prep(qc);
	qc->err_mask |= ap->ops->qc_prep(qc);
	if (unlikely(qc->err_mask))
		goto err;
	trace_ata_qc_issue(qc);
	qc->err_mask |= ap->ops->qc_issue(qc);
	if (unlikely(qc->err_mask))
		goto err;
	return;

sys_err:
	qc->err_mask |= AC_ERR_SYSTEM;
err:
	ata_qc_complete(qc);
}

 
bool ata_phys_link_online(struct ata_link *link)
{
	u32 sstatus;

	if (sata_scr_read(link, SCR_STATUS, &sstatus) == 0 &&
	    ata_sstatus_online(sstatus))
		return true;
	return false;
}

 
bool ata_phys_link_offline(struct ata_link *link)
{
	u32 sstatus;

	if (sata_scr_read(link, SCR_STATUS, &sstatus) == 0 &&
	    !ata_sstatus_online(sstatus))
		return true;
	return false;
}

 
bool ata_link_online(struct ata_link *link)
{
	struct ata_link *slave = link->ap->slave_link;

	WARN_ON(link == slave);	 

	return ata_phys_link_online(link) ||
		(slave && ata_phys_link_online(slave));
}
EXPORT_SYMBOL_GPL(ata_link_online);

 
bool ata_link_offline(struct ata_link *link)
{
	struct ata_link *slave = link->ap->slave_link;

	WARN_ON(link == slave);	 

	return ata_phys_link_offline(link) &&
		(!slave || ata_phys_link_offline(slave));
}
EXPORT_SYMBOL_GPL(ata_link_offline);

#ifdef CONFIG_PM
static void ata_port_request_pm(struct ata_port *ap, pm_message_t mesg,
				unsigned int action, unsigned int ehi_flags,
				bool async)
{
	struct ata_link *link;
	unsigned long flags;

	spin_lock_irqsave(ap->lock, flags);

	 
	if (ap->pflags & ATA_PFLAG_PM_PENDING) {
		spin_unlock_irqrestore(ap->lock, flags);
		ata_port_wait_eh(ap);
		spin_lock_irqsave(ap->lock, flags);
	}

	 
	ap->pm_mesg = mesg;
	ap->pflags |= ATA_PFLAG_PM_PENDING;
	ata_for_each_link(link, ap, HOST_FIRST) {
		link->eh_info.action |= action;
		link->eh_info.flags |= ehi_flags;
	}

	ata_port_schedule_eh(ap);

	spin_unlock_irqrestore(ap->lock, flags);

	if (!async)
		ata_port_wait_eh(ap);
}

 
static const unsigned int ata_port_suspend_ehi = ATA_EHI_QUIET
						 | ATA_EHI_NO_AUTOPSY
						 | ATA_EHI_NO_RECOVERY;

static void ata_port_suspend(struct ata_port *ap, pm_message_t mesg)
{
	 
	cancel_delayed_work_sync(&ap->scsi_rescan_task);

	ata_port_request_pm(ap, mesg, 0, ata_port_suspend_ehi, false);
}

static void ata_port_suspend_async(struct ata_port *ap, pm_message_t mesg)
{
	 
	cancel_delayed_work_sync(&ap->scsi_rescan_task);

	ata_port_request_pm(ap, mesg, 0, ata_port_suspend_ehi, true);
}

static int ata_port_pm_suspend(struct device *dev)
{
	struct ata_port *ap = to_ata_port(dev);

	if (pm_runtime_suspended(dev))
		return 0;

	ata_port_suspend(ap, PMSG_SUSPEND);
	return 0;
}

static int ata_port_pm_freeze(struct device *dev)
{
	struct ata_port *ap = to_ata_port(dev);

	if (pm_runtime_suspended(dev))
		return 0;

	ata_port_suspend(ap, PMSG_FREEZE);
	return 0;
}

static int ata_port_pm_poweroff(struct device *dev)
{
	ata_port_suspend(to_ata_port(dev), PMSG_HIBERNATE);
	return 0;
}

static const unsigned int ata_port_resume_ehi = ATA_EHI_NO_AUTOPSY
						| ATA_EHI_QUIET;

static void ata_port_resume(struct ata_port *ap, pm_message_t mesg)
{
	ata_port_request_pm(ap, mesg, ATA_EH_RESET, ata_port_resume_ehi, false);
}

static void ata_port_resume_async(struct ata_port *ap, pm_message_t mesg)
{
	ata_port_request_pm(ap, mesg, ATA_EH_RESET, ata_port_resume_ehi, true);
}

static int ata_port_pm_resume(struct device *dev)
{
	ata_port_resume_async(to_ata_port(dev), PMSG_RESUME);
	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	return 0;
}

 
static int ata_port_runtime_idle(struct device *dev)
{
	struct ata_port *ap = to_ata_port(dev);
	struct ata_link *link;
	struct ata_device *adev;

	ata_for_each_link(link, ap, HOST_FIRST) {
		ata_for_each_dev(adev, link, ENABLED)
			if (adev->class == ATA_DEV_ATAPI &&
			    !zpodd_dev_enabled(adev))
				return -EBUSY;
	}

	return 0;
}

static int ata_port_runtime_suspend(struct device *dev)
{
	ata_port_suspend(to_ata_port(dev), PMSG_AUTO_SUSPEND);
	return 0;
}

static int ata_port_runtime_resume(struct device *dev)
{
	ata_port_resume(to_ata_port(dev), PMSG_AUTO_RESUME);
	return 0;
}

static const struct dev_pm_ops ata_port_pm_ops = {
	.suspend = ata_port_pm_suspend,
	.resume = ata_port_pm_resume,
	.freeze = ata_port_pm_freeze,
	.thaw = ata_port_pm_resume,
	.poweroff = ata_port_pm_poweroff,
	.restore = ata_port_pm_resume,

	.runtime_suspend = ata_port_runtime_suspend,
	.runtime_resume = ata_port_runtime_resume,
	.runtime_idle = ata_port_runtime_idle,
};

 
void ata_sas_port_suspend(struct ata_port *ap)
{
	ata_port_suspend_async(ap, PMSG_SUSPEND);
}
EXPORT_SYMBOL_GPL(ata_sas_port_suspend);

void ata_sas_port_resume(struct ata_port *ap)
{
	ata_port_resume_async(ap, PMSG_RESUME);
}
EXPORT_SYMBOL_GPL(ata_sas_port_resume);

 
void ata_host_suspend(struct ata_host *host, pm_message_t mesg)
{
	host->dev->power.power_state = mesg;
}
EXPORT_SYMBOL_GPL(ata_host_suspend);

 
void ata_host_resume(struct ata_host *host)
{
	host->dev->power.power_state = PMSG_ON;
}
EXPORT_SYMBOL_GPL(ata_host_resume);
#endif

const struct device_type ata_port_type = {
	.name = ATA_PORT_TYPE_NAME,
#ifdef CONFIG_PM
	.pm = &ata_port_pm_ops,
#endif
};

 
void ata_dev_init(struct ata_device *dev)
{
	struct ata_link *link = ata_dev_phys_link(dev);
	struct ata_port *ap = link->ap;
	unsigned long flags;

	 
	link->sata_spd_limit = link->hw_sata_spd_limit;
	link->sata_spd = 0;

	 
	spin_lock_irqsave(ap->lock, flags);
	dev->flags &= ~ATA_DFLAG_INIT_MASK;
	dev->horkage = 0;
	spin_unlock_irqrestore(ap->lock, flags);

	memset((void *)dev + ATA_DEVICE_CLEAR_BEGIN, 0,
	       ATA_DEVICE_CLEAR_END - ATA_DEVICE_CLEAR_BEGIN);
	dev->pio_mask = UINT_MAX;
	dev->mwdma_mask = UINT_MAX;
	dev->udma_mask = UINT_MAX;
}

 
void ata_link_init(struct ata_port *ap, struct ata_link *link, int pmp)
{
	int i;

	 
	memset((void *)link + ATA_LINK_CLEAR_BEGIN, 0,
	       ATA_LINK_CLEAR_END - ATA_LINK_CLEAR_BEGIN);

	link->ap = ap;
	link->pmp = pmp;
	link->active_tag = ATA_TAG_POISON;
	link->hw_sata_spd_limit = UINT_MAX;

	 
	for (i = 0; i < ATA_MAX_DEVICES; i++) {
		struct ata_device *dev = &link->device[i];

		dev->link = link;
		dev->devno = dev - link->device;
#ifdef CONFIG_ATA_ACPI
		dev->gtf_filter = ata_acpi_gtf_filter;
#endif
		ata_dev_init(dev);
	}
}

 
int sata_link_init_spd(struct ata_link *link)
{
	u8 spd;
	int rc;

	rc = sata_scr_read(link, SCR_CONTROL, &link->saved_scontrol);
	if (rc)
		return rc;

	spd = (link->saved_scontrol >> 4) & 0xf;
	if (spd)
		link->hw_sata_spd_limit &= (1 << spd) - 1;

	ata_force_link_limits(link);

	link->sata_spd_limit = link->hw_sata_spd_limit;

	return 0;
}

 
struct ata_port *ata_port_alloc(struct ata_host *host)
{
	struct ata_port *ap;

	ap = kzalloc(sizeof(*ap), GFP_KERNEL);
	if (!ap)
		return NULL;

	ap->pflags |= ATA_PFLAG_INITIALIZING | ATA_PFLAG_FROZEN;
	ap->lock = &host->lock;
	ap->print_id = -1;
	ap->local_port_no = -1;
	ap->host = host;
	ap->dev = host->dev;

	mutex_init(&ap->scsi_scan_mutex);
	INIT_DELAYED_WORK(&ap->hotplug_task, ata_scsi_hotplug);
	INIT_DELAYED_WORK(&ap->scsi_rescan_task, ata_scsi_dev_rescan);
	INIT_LIST_HEAD(&ap->eh_done_q);
	init_waitqueue_head(&ap->eh_wait_q);
	init_completion(&ap->park_req_pending);
	timer_setup(&ap->fastdrain_timer, ata_eh_fastdrain_timerfn,
		    TIMER_DEFERRABLE);

	ap->cbl = ATA_CBL_NONE;

	ata_link_init(ap, &ap->link, 0);

#ifdef ATA_IRQ_TRAP
	ap->stats.unhandled_irq = 1;
	ap->stats.idle_irq = 1;
#endif
	ata_sff_port_init(ap);

	return ap;
}

static void ata_devres_release(struct device *gendev, void *res)
{
	struct ata_host *host = dev_get_drvdata(gendev);
	int i;

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];

		if (!ap)
			continue;

		if (ap->scsi_host)
			scsi_host_put(ap->scsi_host);

	}

	dev_set_drvdata(gendev, NULL);
	ata_host_put(host);
}

static void ata_host_release(struct kref *kref)
{
	struct ata_host *host = container_of(kref, struct ata_host, kref);
	int i;

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];

		kfree(ap->pmp_link);
		kfree(ap->slave_link);
		kfree(ap->ncq_sense_buf);
		kfree(ap);
		host->ports[i] = NULL;
	}
	kfree(host);
}

void ata_host_get(struct ata_host *host)
{
	kref_get(&host->kref);
}

void ata_host_put(struct ata_host *host)
{
	kref_put(&host->kref, ata_host_release);
}
EXPORT_SYMBOL_GPL(ata_host_put);

 
struct ata_host *ata_host_alloc(struct device *dev, int max_ports)
{
	struct ata_host *host;
	size_t sz;
	int i;
	void *dr;

	 
	sz = sizeof(struct ata_host) + (max_ports + 1) * sizeof(void *);
	host = kzalloc(sz, GFP_KERNEL);
	if (!host)
		return NULL;

	if (!devres_open_group(dev, NULL, GFP_KERNEL))
		goto err_free;

	dr = devres_alloc(ata_devres_release, 0, GFP_KERNEL);
	if (!dr)
		goto err_out;

	devres_add(dev, dr);
	dev_set_drvdata(dev, host);

	spin_lock_init(&host->lock);
	mutex_init(&host->eh_mutex);
	host->dev = dev;
	host->n_ports = max_ports;
	kref_init(&host->kref);

	 
	for (i = 0; i < max_ports; i++) {
		struct ata_port *ap;

		ap = ata_port_alloc(host);
		if (!ap)
			goto err_out;

		ap->port_no = i;
		host->ports[i] = ap;
	}

	devres_remove_group(dev, NULL);
	return host;

 err_out:
	devres_release_group(dev, NULL);
 err_free:
	kfree(host);
	return NULL;
}
EXPORT_SYMBOL_GPL(ata_host_alloc);

 
struct ata_host *ata_host_alloc_pinfo(struct device *dev,
				      const struct ata_port_info * const * ppi,
				      int n_ports)
{
	const struct ata_port_info *pi = &ata_dummy_port_info;
	struct ata_host *host;
	int i, j;

	host = ata_host_alloc(dev, n_ports);
	if (!host)
		return NULL;

	for (i = 0, j = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];

		if (ppi[j])
			pi = ppi[j++];

		ap->pio_mask = pi->pio_mask;
		ap->mwdma_mask = pi->mwdma_mask;
		ap->udma_mask = pi->udma_mask;
		ap->flags |= pi->flags;
		ap->link.flags |= pi->link_flags;
		ap->ops = pi->port_ops;

		if (!host->ops && (pi->port_ops != &ata_dummy_port_ops))
			host->ops = pi->port_ops;
	}

	return host;
}
EXPORT_SYMBOL_GPL(ata_host_alloc_pinfo);

static void ata_host_stop(struct device *gendev, void *res)
{
	struct ata_host *host = dev_get_drvdata(gendev);
	int i;

	WARN_ON(!(host->flags & ATA_HOST_STARTED));

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];

		if (ap->ops->port_stop)
			ap->ops->port_stop(ap);
	}

	if (host->ops->host_stop)
		host->ops->host_stop(host);
}

 
static void ata_finalize_port_ops(struct ata_port_operations *ops)
{
	static DEFINE_SPINLOCK(lock);
	const struct ata_port_operations *cur;
	void **begin = (void **)ops;
	void **end = (void **)&ops->inherits;
	void **pp;

	if (!ops || !ops->inherits)
		return;

	spin_lock(&lock);

	for (cur = ops->inherits; cur; cur = cur->inherits) {
		void **inherit = (void **)cur;

		for (pp = begin; pp < end; pp++, inherit++)
			if (!*pp)
				*pp = *inherit;
	}

	for (pp = begin; pp < end; pp++)
		if (IS_ERR(*pp))
			*pp = NULL;

	ops->inherits = NULL;

	spin_unlock(&lock);
}

 
int ata_host_start(struct ata_host *host)
{
	int have_stop = 0;
	void *start_dr = NULL;
	int i, rc;

	if (host->flags & ATA_HOST_STARTED)
		return 0;

	ata_finalize_port_ops(host->ops);

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];

		ata_finalize_port_ops(ap->ops);

		if (!host->ops && !ata_port_is_dummy(ap))
			host->ops = ap->ops;

		if (ap->ops->port_stop)
			have_stop = 1;
	}

	if (host->ops && host->ops->host_stop)
		have_stop = 1;

	if (have_stop) {
		start_dr = devres_alloc(ata_host_stop, 0, GFP_KERNEL);
		if (!start_dr)
			return -ENOMEM;
	}

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];

		if (ap->ops->port_start) {
			rc = ap->ops->port_start(ap);
			if (rc) {
				if (rc != -ENODEV)
					dev_err(host->dev,
						"failed to start port %d (errno=%d)\n",
						i, rc);
				goto err_out;
			}
		}
		ata_eh_freeze_port(ap);
	}

	if (start_dr)
		devres_add(host->dev, start_dr);
	host->flags |= ATA_HOST_STARTED;
	return 0;

 err_out:
	while (--i >= 0) {
		struct ata_port *ap = host->ports[i];

		if (ap->ops->port_stop)
			ap->ops->port_stop(ap);
	}
	devres_free(start_dr);
	return rc;
}
EXPORT_SYMBOL_GPL(ata_host_start);

 
void ata_host_init(struct ata_host *host, struct device *dev,
		   struct ata_port_operations *ops)
{
	spin_lock_init(&host->lock);
	mutex_init(&host->eh_mutex);
	host->n_tags = ATA_MAX_QUEUE;
	host->dev = dev;
	host->ops = ops;
	kref_init(&host->kref);
}
EXPORT_SYMBOL_GPL(ata_host_init);

void ata_port_probe(struct ata_port *ap)
{
	struct ata_eh_info *ehi = &ap->link.eh_info;
	unsigned long flags;

	 
	spin_lock_irqsave(ap->lock, flags);

	ehi->probe_mask |= ATA_ALL_DEVICES;
	ehi->action |= ATA_EH_RESET;
	ehi->flags |= ATA_EHI_NO_AUTOPSY | ATA_EHI_QUIET;

	ap->pflags &= ~ATA_PFLAG_INITIALIZING;
	ap->pflags |= ATA_PFLAG_LOADING;
	ata_port_schedule_eh(ap);

	spin_unlock_irqrestore(ap->lock, flags);
}
EXPORT_SYMBOL_GPL(ata_port_probe);

static void async_port_probe(void *data, async_cookie_t cookie)
{
	struct ata_port *ap = data;

	 
	if (!(ap->host->flags & ATA_HOST_PARALLEL_SCAN) && ap->port_no != 0)
		async_synchronize_cookie(cookie);

	ata_port_probe(ap);
	ata_port_wait_eh(ap);

	 
	async_synchronize_cookie(cookie);

	ata_scsi_scan_host(ap, 1);
}

 
int ata_host_register(struct ata_host *host, const struct scsi_host_template *sht)
{
	int i, rc;

	host->n_tags = clamp(sht->can_queue, 1, ATA_MAX_QUEUE);

	 
	if (!(host->flags & ATA_HOST_STARTED)) {
		dev_err(host->dev, "BUG: trying to register unstarted host\n");
		WARN_ON(1);
		return -EINVAL;
	}

	 
	for (i = host->n_ports; host->ports[i]; i++)
		kfree(host->ports[i]);

	 
	for (i = 0; i < host->n_ports; i++) {
		host->ports[i]->print_id = atomic_inc_return(&ata_print_id);
		host->ports[i]->local_port_no = i + 1;
	}

	 
	for (i = 0; i < host->n_ports; i++) {
		rc = ata_tport_add(host->dev,host->ports[i]);
		if (rc) {
			goto err_tadd;
		}
	}

	rc = ata_scsi_add_hosts(host, sht);
	if (rc)
		goto err_tadd;

	 
	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];
		unsigned int xfer_mask;

		 
		if (ap->cbl == ATA_CBL_NONE && (ap->flags & ATA_FLAG_SATA))
			ap->cbl = ATA_CBL_SATA;

		 
		sata_link_init_spd(&ap->link);
		if (ap->slave_link)
			sata_link_init_spd(ap->slave_link);

		 
		xfer_mask = ata_pack_xfermask(ap->pio_mask, ap->mwdma_mask,
					      ap->udma_mask);

		if (!ata_port_is_dummy(ap)) {
			ata_port_info(ap, "%cATA max %s %s\n",
				      (ap->flags & ATA_FLAG_SATA) ? 'S' : 'P',
				      ata_mode_string(xfer_mask),
				      ap->link.eh_info.desc);
			ata_ehi_clear_desc(&ap->link.eh_info);
		} else
			ata_port_info(ap, "DUMMY\n");
	}

	 
	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];
		ap->cookie = async_schedule(async_port_probe, ap);
	}

	return 0;

 err_tadd:
	while (--i >= 0) {
		ata_tport_delete(host->ports[i]);
	}
	return rc;

}
EXPORT_SYMBOL_GPL(ata_host_register);

 
int ata_host_activate(struct ata_host *host, int irq,
		      irq_handler_t irq_handler, unsigned long irq_flags,
		      const struct scsi_host_template *sht)
{
	int i, rc;
	char *irq_desc;

	rc = ata_host_start(host);
	if (rc)
		return rc;

	 
	if (!irq) {
		WARN_ON(irq_handler);
		return ata_host_register(host, sht);
	}

	irq_desc = devm_kasprintf(host->dev, GFP_KERNEL, "%s[%s]",
				  dev_driver_string(host->dev),
				  dev_name(host->dev));
	if (!irq_desc)
		return -ENOMEM;

	rc = devm_request_irq(host->dev, irq, irq_handler, irq_flags,
			      irq_desc, host);
	if (rc)
		return rc;

	for (i = 0; i < host->n_ports; i++)
		ata_port_desc(host->ports[i], "irq %d", irq);

	rc = ata_host_register(host, sht);
	 
	if (rc)
		devm_free_irq(host->dev, irq, host);

	return rc;
}
EXPORT_SYMBOL_GPL(ata_host_activate);

 
static void ata_port_detach(struct ata_port *ap)
{
	unsigned long flags;
	struct ata_link *link;
	struct ata_device *dev;

	 
	ata_port_wait_eh(ap);

	mutex_lock(&ap->scsi_scan_mutex);
	spin_lock_irqsave(ap->lock, flags);

	 
	ata_for_each_link(link, ap, HOST_FIRST) {
		ata_for_each_dev(dev, link, ALL) {
			if (dev->sdev) {
				spin_unlock_irqrestore(ap->lock, flags);
				scsi_remove_device(dev->sdev);
				spin_lock_irqsave(ap->lock, flags);
				dev->sdev = NULL;
			}
		}
	}

	 
	ap->pflags |= ATA_PFLAG_UNLOADING;
	ata_port_schedule_eh(ap);

	spin_unlock_irqrestore(ap->lock, flags);
	mutex_unlock(&ap->scsi_scan_mutex);

	 
	ata_port_wait_eh(ap);

	 
	WARN_ON(!(ap->pflags & ATA_PFLAG_UNLOADED));

	cancel_delayed_work_sync(&ap->hotplug_task);
	cancel_delayed_work_sync(&ap->scsi_rescan_task);

	 
	ata_for_each_link(link, ap, HOST_FIRST) {
		ata_for_each_dev(dev, link, ALL) {
			if (zpodd_dev_enabled(dev))
				zpodd_exit(dev);
		}
	}
	if (ap->pmp_link) {
		int i;
		for (i = 0; i < SATA_PMP_MAX_PORTS; i++)
			ata_tlink_delete(&ap->pmp_link[i]);
	}
	 
	scsi_remove_host(ap->scsi_host);
	ata_tport_delete(ap);
}

 
void ata_host_detach(struct ata_host *host)
{
	int i;

	for (i = 0; i < host->n_ports; i++) {
		 
		async_synchronize_cookie(host->ports[i]->cookie + 1);
		ata_port_detach(host->ports[i]);
	}

	 
	ata_acpi_dissociate(host);
}
EXPORT_SYMBOL_GPL(ata_host_detach);

#ifdef CONFIG_PCI

 
void ata_pci_remove_one(struct pci_dev *pdev)
{
	struct ata_host *host = pci_get_drvdata(pdev);

	ata_host_detach(host);
}
EXPORT_SYMBOL_GPL(ata_pci_remove_one);

void ata_pci_shutdown_one(struct pci_dev *pdev)
{
	struct ata_host *host = pci_get_drvdata(pdev);
	int i;

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];

		ap->pflags |= ATA_PFLAG_FROZEN;

		 
		if (ap->ops->freeze)
			ap->ops->freeze(ap);

		 
		if (ap->ops->port_stop)
			ap->ops->port_stop(ap);
	}
}
EXPORT_SYMBOL_GPL(ata_pci_shutdown_one);

 
int pci_test_config_bits(struct pci_dev *pdev, const struct pci_bits *bits)
{
	unsigned long tmp = 0;

	switch (bits->width) {
	case 1: {
		u8 tmp8 = 0;
		pci_read_config_byte(pdev, bits->reg, &tmp8);
		tmp = tmp8;
		break;
	}
	case 2: {
		u16 tmp16 = 0;
		pci_read_config_word(pdev, bits->reg, &tmp16);
		tmp = tmp16;
		break;
	}
	case 4: {
		u32 tmp32 = 0;
		pci_read_config_dword(pdev, bits->reg, &tmp32);
		tmp = tmp32;
		break;
	}

	default:
		return -EINVAL;
	}

	tmp &= bits->mask;

	return (tmp == bits->val) ? 1 : 0;
}
EXPORT_SYMBOL_GPL(pci_test_config_bits);

#ifdef CONFIG_PM
void ata_pci_device_do_suspend(struct pci_dev *pdev, pm_message_t mesg)
{
	pci_save_state(pdev);
	pci_disable_device(pdev);

	if (mesg.event & PM_EVENT_SLEEP)
		pci_set_power_state(pdev, PCI_D3hot);
}
EXPORT_SYMBOL_GPL(ata_pci_device_do_suspend);

int ata_pci_device_do_resume(struct pci_dev *pdev)
{
	int rc;

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);

	rc = pcim_enable_device(pdev);
	if (rc) {
		dev_err(&pdev->dev,
			"failed to enable device after resume (%d)\n", rc);
		return rc;
	}

	pci_set_master(pdev);
	return 0;
}
EXPORT_SYMBOL_GPL(ata_pci_device_do_resume);

int ata_pci_device_suspend(struct pci_dev *pdev, pm_message_t mesg)
{
	struct ata_host *host = pci_get_drvdata(pdev);

	ata_host_suspend(host, mesg);

	ata_pci_device_do_suspend(pdev, mesg);

	return 0;
}
EXPORT_SYMBOL_GPL(ata_pci_device_suspend);

int ata_pci_device_resume(struct pci_dev *pdev)
{
	struct ata_host *host = pci_get_drvdata(pdev);
	int rc;

	rc = ata_pci_device_do_resume(pdev);
	if (rc == 0)
		ata_host_resume(host);
	return rc;
}
EXPORT_SYMBOL_GPL(ata_pci_device_resume);
#endif  
#endif  

 
void ata_platform_remove_one(struct platform_device *pdev)
{
	struct ata_host *host = platform_get_drvdata(pdev);

	ata_host_detach(host);
}
EXPORT_SYMBOL_GPL(ata_platform_remove_one);

#ifdef CONFIG_ATA_FORCE

#define force_cbl(name, flag)				\
	{ #name,	.cbl		= (flag) }

#define force_spd_limit(spd, val)			\
	{ #spd,	.spd_limit		= (val) }

#define force_xfer(mode, shift)				\
	{ #mode,	.xfer_mask	= (1UL << (shift)) }

#define force_lflag_on(name, flags)			\
	{ #name,	.lflags_on	= (flags) }

#define force_lflag_onoff(name, flags)			\
	{ "no" #name,	.lflags_on	= (flags) },	\
	{ #name,	.lflags_off	= (flags) }

#define force_horkage_on(name, flag)			\
	{ #name,	.horkage_on	= (flag) }

#define force_horkage_onoff(name, flag)			\
	{ "no" #name,	.horkage_on	= (flag) },	\
	{ #name,	.horkage_off	= (flag) }

static const struct ata_force_param force_tbl[] __initconst = {
	force_cbl(40c,			ATA_CBL_PATA40),
	force_cbl(80c,			ATA_CBL_PATA80),
	force_cbl(short40c,		ATA_CBL_PATA40_SHORT),
	force_cbl(unk,			ATA_CBL_PATA_UNK),
	force_cbl(ign,			ATA_CBL_PATA_IGN),
	force_cbl(sata,			ATA_CBL_SATA),

	force_spd_limit(1.5Gbps,	1),
	force_spd_limit(3.0Gbps,	2),

	force_xfer(pio0,		ATA_SHIFT_PIO + 0),
	force_xfer(pio1,		ATA_SHIFT_PIO + 1),
	force_xfer(pio2,		ATA_SHIFT_PIO + 2),
	force_xfer(pio3,		ATA_SHIFT_PIO + 3),
	force_xfer(pio4,		ATA_SHIFT_PIO + 4),
	force_xfer(pio5,		ATA_SHIFT_PIO + 5),
	force_xfer(pio6,		ATA_SHIFT_PIO + 6),
	force_xfer(mwdma0,		ATA_SHIFT_MWDMA + 0),
	force_xfer(mwdma1,		ATA_SHIFT_MWDMA + 1),
	force_xfer(mwdma2,		ATA_SHIFT_MWDMA + 2),
	force_xfer(mwdma3,		ATA_SHIFT_MWDMA + 3),
	force_xfer(mwdma4,		ATA_SHIFT_MWDMA + 4),
	force_xfer(udma0,		ATA_SHIFT_UDMA + 0),
	force_xfer(udma16,		ATA_SHIFT_UDMA + 0),
	force_xfer(udma/16,		ATA_SHIFT_UDMA + 0),
	force_xfer(udma1,		ATA_SHIFT_UDMA + 1),
	force_xfer(udma25,		ATA_SHIFT_UDMA + 1),
	force_xfer(udma/25,		ATA_SHIFT_UDMA + 1),
	force_xfer(udma2,		ATA_SHIFT_UDMA + 2),
	force_xfer(udma33,		ATA_SHIFT_UDMA + 2),
	force_xfer(udma/33,		ATA_SHIFT_UDMA + 2),
	force_xfer(udma3,		ATA_SHIFT_UDMA + 3),
	force_xfer(udma44,		ATA_SHIFT_UDMA + 3),
	force_xfer(udma/44,		ATA_SHIFT_UDMA + 3),
	force_xfer(udma4,		ATA_SHIFT_UDMA + 4),
	force_xfer(udma66,		ATA_SHIFT_UDMA + 4),
	force_xfer(udma/66,		ATA_SHIFT_UDMA + 4),
	force_xfer(udma5,		ATA_SHIFT_UDMA + 5),
	force_xfer(udma100,		ATA_SHIFT_UDMA + 5),
	force_xfer(udma/100,		ATA_SHIFT_UDMA + 5),
	force_xfer(udma6,		ATA_SHIFT_UDMA + 6),
	force_xfer(udma133,		ATA_SHIFT_UDMA + 6),
	force_xfer(udma/133,		ATA_SHIFT_UDMA + 6),
	force_xfer(udma7,		ATA_SHIFT_UDMA + 7),

	force_lflag_on(nohrst,		ATA_LFLAG_NO_HRST),
	force_lflag_on(nosrst,		ATA_LFLAG_NO_SRST),
	force_lflag_on(norst,		ATA_LFLAG_NO_HRST | ATA_LFLAG_NO_SRST),
	force_lflag_on(rstonce,		ATA_LFLAG_RST_ONCE),
	force_lflag_onoff(dbdelay,	ATA_LFLAG_NO_DEBOUNCE_DELAY),

	force_horkage_onoff(ncq,	ATA_HORKAGE_NONCQ),
	force_horkage_onoff(ncqtrim,	ATA_HORKAGE_NO_NCQ_TRIM),
	force_horkage_onoff(ncqati,	ATA_HORKAGE_NO_NCQ_ON_ATI),

	force_horkage_onoff(trim,	ATA_HORKAGE_NOTRIM),
	force_horkage_on(trim_zero,	ATA_HORKAGE_ZERO_AFTER_TRIM),
	force_horkage_on(max_trim_128m, ATA_HORKAGE_MAX_TRIM_128M),

	force_horkage_onoff(dma,	ATA_HORKAGE_NODMA),
	force_horkage_on(atapi_dmadir,	ATA_HORKAGE_ATAPI_DMADIR),
	force_horkage_on(atapi_mod16_dma, ATA_HORKAGE_ATAPI_MOD16_DMA),

	force_horkage_onoff(dmalog,	ATA_HORKAGE_NO_DMA_LOG),
	force_horkage_onoff(iddevlog,	ATA_HORKAGE_NO_ID_DEV_LOG),
	force_horkage_onoff(logdir,	ATA_HORKAGE_NO_LOG_DIR),

	force_horkage_on(max_sec_128,	ATA_HORKAGE_MAX_SEC_128),
	force_horkage_on(max_sec_1024,	ATA_HORKAGE_MAX_SEC_1024),
	force_horkage_on(max_sec_lba48,	ATA_HORKAGE_MAX_SEC_LBA48),

	force_horkage_onoff(lpm,	ATA_HORKAGE_NOLPM),
	force_horkage_onoff(setxfer,	ATA_HORKAGE_NOSETXFER),
	force_horkage_on(dump_id,	ATA_HORKAGE_DUMP_ID),
	force_horkage_onoff(fua,	ATA_HORKAGE_NO_FUA),

	force_horkage_on(disable,	ATA_HORKAGE_DISABLE),
};

static int __init ata_parse_force_one(char **cur,
				      struct ata_force_ent *force_ent,
				      const char **reason)
{
	char *start = *cur, *p = *cur;
	char *id, *val, *endp;
	const struct ata_force_param *match_fp = NULL;
	int nr_matches = 0, i;

	 
	while (*p != '\0' && *p != ',')
		p++;

	if (*p == '\0')
		*cur = p;
	else
		*cur = p + 1;

	*p = '\0';

	 
	p = strchr(start, ':');
	if (!p) {
		val = strstrip(start);
		goto parse_val;
	}
	*p = '\0';

	id = strstrip(start);
	val = strstrip(p + 1);

	 
	p = strchr(id, '.');
	if (p) {
		*p++ = '\0';
		force_ent->device = simple_strtoul(p, &endp, 10);
		if (p == endp || *endp != '\0') {
			*reason = "invalid device";
			return -EINVAL;
		}
	}

	force_ent->port = simple_strtoul(id, &endp, 10);
	if (id == endp || *endp != '\0') {
		*reason = "invalid port/link";
		return -EINVAL;
	}

 parse_val:
	 
	for (i = 0; i < ARRAY_SIZE(force_tbl); i++) {
		const struct ata_force_param *fp = &force_tbl[i];

		if (strncasecmp(val, fp->name, strlen(val)))
			continue;

		nr_matches++;
		match_fp = fp;

		if (strcasecmp(val, fp->name) == 0) {
			nr_matches = 1;
			break;
		}
	}

	if (!nr_matches) {
		*reason = "unknown value";
		return -EINVAL;
	}
	if (nr_matches > 1) {
		*reason = "ambiguous value";
		return -EINVAL;
	}

	force_ent->param = *match_fp;

	return 0;
}

static void __init ata_parse_force_param(void)
{
	int idx = 0, size = 1;
	int last_port = -1, last_device = -1;
	char *p, *cur, *next;

	 
	for (p = ata_force_param_buf; *p; p++)
		if (*p == ',')
			size++;

	ata_force_tbl = kcalloc(size, sizeof(ata_force_tbl[0]), GFP_KERNEL);
	if (!ata_force_tbl) {
		printk(KERN_WARNING "ata: failed to extend force table, "
		       "libata.force ignored\n");
		return;
	}

	 
	for (cur = ata_force_param_buf; *cur != '\0'; cur = next) {
		const char *reason = "";
		struct ata_force_ent te = { .port = -1, .device = -1 };

		next = cur;
		if (ata_parse_force_one(&next, &te, &reason)) {
			printk(KERN_WARNING "ata: failed to parse force "
			       "parameter \"%s\" (%s)\n",
			       cur, reason);
			continue;
		}

		if (te.port == -1) {
			te.port = last_port;
			te.device = last_device;
		}

		ata_force_tbl[idx++] = te;

		last_port = te.port;
		last_device = te.device;
	}

	ata_force_tbl_size = idx;
}

static void ata_free_force_param(void)
{
	kfree(ata_force_tbl);
}
#else
static inline void ata_parse_force_param(void) { }
static inline void ata_free_force_param(void) { }
#endif

static int __init ata_init(void)
{
	int rc;

	ata_parse_force_param();

	rc = ata_sff_init();
	if (rc) {
		ata_free_force_param();
		return rc;
	}

	libata_transport_init();
	ata_scsi_transport_template = ata_attach_transport();
	if (!ata_scsi_transport_template) {
		ata_sff_exit();
		rc = -ENOMEM;
		goto err_out;
	}

	printk(KERN_DEBUG "libata version " DRV_VERSION " loaded.\n");
	return 0;

err_out:
	return rc;
}

static void __exit ata_exit(void)
{
	ata_release_transport(ata_scsi_transport_template);
	libata_transport_exit();
	ata_sff_exit();
	ata_free_force_param();
}

subsys_initcall(ata_init);
module_exit(ata_exit);

static DEFINE_RATELIMIT_STATE(ratelimit, HZ / 5, 1);

int ata_ratelimit(void)
{
	return __ratelimit(&ratelimit);
}
EXPORT_SYMBOL_GPL(ata_ratelimit);

 
void ata_msleep(struct ata_port *ap, unsigned int msecs)
{
	bool owns_eh = ap && ap->host->eh_owner == current;

	if (owns_eh)
		ata_eh_release(ap);

	if (msecs < 20) {
		unsigned long usecs = msecs * USEC_PER_MSEC;
		usleep_range(usecs, usecs + 50);
	} else {
		msleep(msecs);
	}

	if (owns_eh)
		ata_eh_acquire(ap);
}
EXPORT_SYMBOL_GPL(ata_msleep);

 
u32 ata_wait_register(struct ata_port *ap, void __iomem *reg, u32 mask, u32 val,
		      unsigned int interval, unsigned int timeout)
{
	unsigned long deadline;
	u32 tmp;

	tmp = ioread32(reg);

	 
	deadline = ata_deadline(jiffies, timeout);

	while ((tmp & mask) == val && time_before(jiffies, deadline)) {
		ata_msleep(ap, interval);
		tmp = ioread32(reg);
	}

	return tmp;
}
EXPORT_SYMBOL_GPL(ata_wait_register);

 
static unsigned int ata_dummy_qc_issue(struct ata_queued_cmd *qc)
{
	return AC_ERR_SYSTEM;
}

static void ata_dummy_error_handler(struct ata_port *ap)
{
	 
}

struct ata_port_operations ata_dummy_port_ops = {
	.qc_prep		= ata_noop_qc_prep,
	.qc_issue		= ata_dummy_qc_issue,
	.error_handler		= ata_dummy_error_handler,
	.sched_eh		= ata_std_sched_eh,
	.end_eh			= ata_std_end_eh,
};
EXPORT_SYMBOL_GPL(ata_dummy_port_ops);

const struct ata_port_info ata_dummy_port_info = {
	.port_ops		= &ata_dummy_port_ops,
};
EXPORT_SYMBOL_GPL(ata_dummy_port_info);

void ata_print_version(const struct device *dev, const char *version)
{
	dev_printk(KERN_DEBUG, dev, "version %s\n", version);
}
EXPORT_SYMBOL(ata_print_version);

EXPORT_TRACEPOINT_SYMBOL_GPL(ata_tf_load);
EXPORT_TRACEPOINT_SYMBOL_GPL(ata_exec_command);
EXPORT_TRACEPOINT_SYMBOL_GPL(ata_bmdma_setup);
EXPORT_TRACEPOINT_SYMBOL_GPL(ata_bmdma_start);
EXPORT_TRACEPOINT_SYMBOL_GPL(ata_bmdma_status);
