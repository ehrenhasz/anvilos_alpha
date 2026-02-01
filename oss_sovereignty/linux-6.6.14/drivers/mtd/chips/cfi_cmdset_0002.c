
 

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/byteorder.h>

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/mtd/map.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/cfi.h>
#include <linux/mtd/xip.h>

#define AMD_BOOTLOC_BUG
#define FORCE_WORD_WRITE 0

#define MAX_RETRIES 3

#define SST49LF004B		0x0060
#define SST49LF040B		0x0050
#define SST49LF008A		0x005a
#define AT49BV6416		0x00d6
#define S29GL064N_MN12		0x0c01

 
#define CFI_SR_DRB		BIT(7)
#define CFI_SR_ESB		BIT(5)
#define CFI_SR_PSB		BIT(4)
#define CFI_SR_WBASB		BIT(3)
#define CFI_SR_SLSB		BIT(1)

enum cfi_quirks {
	CFI_QUIRK_DQ_TRUE_DATA = BIT(0),
};

static int cfi_amdstd_read (struct mtd_info *, loff_t, size_t, size_t *, u_char *);
static int cfi_amdstd_write_words(struct mtd_info *, loff_t, size_t, size_t *, const u_char *);
#if !FORCE_WORD_WRITE
static int cfi_amdstd_write_buffers(struct mtd_info *, loff_t, size_t, size_t *, const u_char *);
#endif
static int cfi_amdstd_erase_chip(struct mtd_info *, struct erase_info *);
static int cfi_amdstd_erase_varsize(struct mtd_info *, struct erase_info *);
static void cfi_amdstd_sync (struct mtd_info *);
static int cfi_amdstd_suspend (struct mtd_info *);
static void cfi_amdstd_resume (struct mtd_info *);
static int cfi_amdstd_reboot(struct notifier_block *, unsigned long, void *);
static int cfi_amdstd_get_fact_prot_info(struct mtd_info *, size_t,
					 size_t *, struct otp_info *);
static int cfi_amdstd_get_user_prot_info(struct mtd_info *, size_t,
					 size_t *, struct otp_info *);
static int cfi_amdstd_secsi_read (struct mtd_info *, loff_t, size_t, size_t *, u_char *);
static int cfi_amdstd_read_fact_prot_reg(struct mtd_info *, loff_t, size_t,
					 size_t *, u_char *);
static int cfi_amdstd_read_user_prot_reg(struct mtd_info *, loff_t, size_t,
					 size_t *, u_char *);
static int cfi_amdstd_write_user_prot_reg(struct mtd_info *, loff_t, size_t,
					  size_t *, const u_char *);
static int cfi_amdstd_lock_user_prot_reg(struct mtd_info *, loff_t, size_t);

static int cfi_amdstd_panic_write(struct mtd_info *mtd, loff_t to, size_t len,
				  size_t *retlen, const u_char *buf);

static void cfi_amdstd_destroy(struct mtd_info *);

struct mtd_info *cfi_cmdset_0002(struct map_info *, int);
static struct mtd_info *cfi_amdstd_setup (struct mtd_info *);

static int get_chip(struct map_info *map, struct flchip *chip, unsigned long adr, int mode);
static void put_chip(struct map_info *map, struct flchip *chip, unsigned long adr);
#include "fwh_lock.h"

static int cfi_atmel_lock(struct mtd_info *mtd, loff_t ofs, uint64_t len);
static int cfi_atmel_unlock(struct mtd_info *mtd, loff_t ofs, uint64_t len);

static int cfi_ppb_lock(struct mtd_info *mtd, loff_t ofs, uint64_t len);
static int cfi_ppb_unlock(struct mtd_info *mtd, loff_t ofs, uint64_t len);
static int cfi_ppb_is_locked(struct mtd_info *mtd, loff_t ofs, uint64_t len);

static struct mtd_chip_driver cfi_amdstd_chipdrv = {
	.probe		= NULL,  
	.destroy	= cfi_amdstd_destroy,
	.name		= "cfi_cmdset_0002",
	.module		= THIS_MODULE
};

 
static int cfi_use_status_reg(struct cfi_private *cfi)
{
	struct cfi_pri_amdstd *extp = cfi->cmdset_priv;
	u8 poll_mask = CFI_POLL_STATUS_REG | CFI_POLL_DQ;

	return extp && extp->MinorVersion >= '5' &&
		(extp->SoftwareFeatures & poll_mask) == CFI_POLL_STATUS_REG;
}

static int cfi_check_err_status(struct map_info *map, struct flchip *chip,
				unsigned long adr)
{
	struct cfi_private *cfi = map->fldrv_priv;
	map_word status;

	if (!cfi_use_status_reg(cfi))
		return 0;

	cfi_send_gen_cmd(0x70, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);
	status = map_read(map, adr);

	 
	if (!map_word_bitsset(map, status, CMD(CFI_SR_DRB)))
		return 0;

	if (map_word_bitsset(map, status, CMD(0x3a))) {
		unsigned long chipstatus = MERGESTATUS(status);

		if (chipstatus & CFI_SR_ESB)
			pr_err("%s erase operation failed, status %lx\n",
			       map->name, chipstatus);
		if (chipstatus & CFI_SR_PSB)
			pr_err("%s program operation failed, status %lx\n",
			       map->name, chipstatus);
		if (chipstatus & CFI_SR_WBASB)
			pr_err("%s buffer program command aborted, status %lx\n",
			       map->name, chipstatus);
		if (chipstatus & CFI_SR_SLSB)
			pr_err("%s sector write protected, status %lx\n",
			       map->name, chipstatus);

		 
		if (chipstatus & (CFI_SR_ESB | CFI_SR_PSB))
			return 1;
	}
	return 0;
}

 


#ifdef DEBUG_CFI_FEATURES
static void cfi_tell_features(struct cfi_pri_amdstd *extp)
{
	const char* erase_suspend[3] = {
		"Not supported", "Read only", "Read/write"
	};
	const char* top_bottom[6] = {
		"No WP", "8x8KiB sectors at top & bottom, no WP",
		"Bottom boot", "Top boot",
		"Uniform, Bottom WP", "Uniform, Top WP"
	};

	printk("  Silicon revision: %d\n", extp->SiliconRevision >> 1);
	printk("  Address sensitive unlock: %s\n",
	       (extp->SiliconRevision & 1) ? "Not required" : "Required");

	if (extp->EraseSuspend < ARRAY_SIZE(erase_suspend))
		printk("  Erase Suspend: %s\n", erase_suspend[extp->EraseSuspend]);
	else
		printk("  Erase Suspend: Unknown value %d\n", extp->EraseSuspend);

	if (extp->BlkProt == 0)
		printk("  Block protection: Not supported\n");
	else
		printk("  Block protection: %d sectors per group\n", extp->BlkProt);


	printk("  Temporary block unprotect: %s\n",
	       extp->TmpBlkUnprotect ? "Supported" : "Not supported");
	printk("  Block protect/unprotect scheme: %d\n", extp->BlkProtUnprot);
	printk("  Number of simultaneous operations: %d\n", extp->SimultaneousOps);
	printk("  Burst mode: %s\n",
	       extp->BurstMode ? "Supported" : "Not supported");
	if (extp->PageMode == 0)
		printk("  Page mode: Not supported\n");
	else
		printk("  Page mode: %d word page\n", extp->PageMode << 2);

	printk("  Vpp Supply Minimum Program/Erase Voltage: %d.%d V\n",
	       extp->VppMin >> 4, extp->VppMin & 0xf);
	printk("  Vpp Supply Maximum Program/Erase Voltage: %d.%d V\n",
	       extp->VppMax >> 4, extp->VppMax & 0xf);

	if (extp->TopBottom < ARRAY_SIZE(top_bottom))
		printk("  Top/Bottom Boot Block: %s\n", top_bottom[extp->TopBottom]);
	else
		printk("  Top/Bottom Boot Block: Unknown value %d\n", extp->TopBottom);
}
#endif

#ifdef AMD_BOOTLOC_BUG
 
static void fixup_amd_bootblock(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	struct cfi_pri_amdstd *extp = cfi->cmdset_priv;
	__u8 major = extp->MajorVersion;
	__u8 minor = extp->MinorVersion;

	if (((major << 8) | minor) < 0x3131) {
		 

		pr_debug("%s: JEDEC Vendor ID is 0x%02X Device ID is 0x%02X\n",
			map->name, cfi->mfr, cfi->id);

		 
		if (((cfi->id == 0xBA) || (cfi->id == 0x22BA)) &&

			 
			(cfi->mfr == CFI_MFR_MACRONIX)) {
			pr_debug("%s: Macronix MX29LV400C with bottom boot block"
				" detected\n", map->name);
			extp->TopBottom = 2;	 
		} else
		if (cfi->id & 0x80) {
			printk(KERN_WARNING "%s: JEDEC Device ID is 0x%02X. Assuming broken CFI table.\n", map->name, cfi->id);
			extp->TopBottom = 3;	 
		} else {
			extp->TopBottom = 2;	 
		}

		pr_debug("%s: AMD CFI PRI V%c.%c has no boot block field;"
			" deduced %s from Device ID\n", map->name, major, minor,
			extp->TopBottom == 2 ? "bottom" : "top");
	}
}
#endif

#if !FORCE_WORD_WRITE
static void fixup_use_write_buffers(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;

	if (cfi->mfr == CFI_MFR_AMD && cfi->id == 0x2201)
		return;

	if (cfi->cfiq->BufWriteTimeoutTyp) {
		pr_debug("Using buffer write method\n");
		mtd->_write = cfi_amdstd_write_buffers;
	}
}
#endif  

 
static void fixup_convert_atmel_pri(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	struct cfi_pri_amdstd *extp = cfi->cmdset_priv;
	struct cfi_pri_atmel atmel_pri;

	memcpy(&atmel_pri, extp, sizeof(atmel_pri));
	memset((char *)extp + 5, 0, sizeof(*extp) - 5);

	if (atmel_pri.Features & 0x02)
		extp->EraseSuspend = 2;

	 
	if (cfi->id == AT49BV6416) {
		if (atmel_pri.BottomBoot)
			extp->TopBottom = 3;
		else
			extp->TopBottom = 2;
	} else {
		if (atmel_pri.BottomBoot)
			extp->TopBottom = 2;
		else
			extp->TopBottom = 3;
	}

	 
	cfi->cfiq->BufWriteTimeoutTyp = 0;
	cfi->cfiq->BufWriteTimeoutMax = 0;
}

static void fixup_use_secsi(struct mtd_info *mtd)
{
	 
	mtd->_read_user_prot_reg = cfi_amdstd_secsi_read;
	mtd->_read_fact_prot_reg = cfi_amdstd_secsi_read;
}

static void fixup_use_erase_chip(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	if ((cfi->cfiq->NumEraseRegions == 1) &&
		((cfi->cfiq->EraseRegionInfo[0] & 0xffff) == 0)) {
		mtd->_erase = cfi_amdstd_erase_chip;
	}

}

 
static void fixup_use_atmel_lock(struct mtd_info *mtd)
{
	mtd->_lock = cfi_atmel_lock;
	mtd->_unlock = cfi_atmel_unlock;
	mtd->flags |= MTD_POWERUP_LOCK;
}

static void fixup_old_sst_eraseregion(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;

	 
	cfi->cfiq->NumEraseRegions = 1;
}

static void fixup_sst39vf(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;

	fixup_old_sst_eraseregion(mtd);

	cfi->addr_unlock1 = 0x5555;
	cfi->addr_unlock2 = 0x2AAA;
}

static void fixup_sst39vf_rev_b(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;

	fixup_old_sst_eraseregion(mtd);

	cfi->addr_unlock1 = 0x555;
	cfi->addr_unlock2 = 0x2AA;

	cfi->sector_erase_cmd = CMD(0x50);
}

static void fixup_sst38vf640x_sectorsize(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;

	fixup_sst39vf_rev_b(mtd);

	 
	cfi->cfiq->EraseRegionInfo[0] = 0x002003ff;
	pr_warn("%s: Bad 38VF640x CFI data; adjusting sector size from 64 to 8KiB\n",
		mtd->name);
}

static void fixup_s29gl064n_sectors(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;

	if ((cfi->cfiq->EraseRegionInfo[0] & 0xffff) == 0x003f) {
		cfi->cfiq->EraseRegionInfo[0] |= 0x0040;
		pr_warn("%s: Bad S29GL064N CFI data; adjust from 64 to 128 sectors\n",
			mtd->name);
	}
}

static void fixup_s29gl032n_sectors(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;

	if ((cfi->cfiq->EraseRegionInfo[1] & 0xffff) == 0x007e) {
		cfi->cfiq->EraseRegionInfo[1] &= ~0x0040;
		pr_warn("%s: Bad S29GL032N CFI data; adjust from 127 to 63 sectors\n",
			mtd->name);
	}
}

static void fixup_s29ns512p_sectors(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;

	 
	cfi->cfiq->EraseRegionInfo[0] = 0x020001ff;
	pr_warn("%s: Bad S29NS512P CFI data; adjust to 512 sectors\n",
		mtd->name);
}

static void fixup_quirks(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;

	if (cfi->mfr == CFI_MFR_AMD && cfi->id == S29GL064N_MN12)
		cfi->quirks |= CFI_QUIRK_DQ_TRUE_DATA;
}

 
static struct cfi_fixup cfi_nopri_fixup_table[] = {
	{ CFI_MFR_SST, 0x234a, fixup_sst39vf },  
	{ CFI_MFR_SST, 0x234b, fixup_sst39vf },  
	{ CFI_MFR_SST, 0x235a, fixup_sst39vf },  
	{ CFI_MFR_SST, 0x235b, fixup_sst39vf },  
	{ CFI_MFR_SST, 0x235c, fixup_sst39vf_rev_b },  
	{ CFI_MFR_SST, 0x235d, fixup_sst39vf_rev_b },  
	{ CFI_MFR_SST, 0x236c, fixup_sst39vf_rev_b },  
	{ CFI_MFR_SST, 0x236d, fixup_sst39vf_rev_b },  
	{ 0, 0, NULL }
};

static struct cfi_fixup cfi_fixup_table[] = {
	{ CFI_MFR_ATMEL, CFI_ID_ANY, fixup_convert_atmel_pri },
#ifdef AMD_BOOTLOC_BUG
	{ CFI_MFR_AMD, CFI_ID_ANY, fixup_amd_bootblock },
	{ CFI_MFR_AMIC, CFI_ID_ANY, fixup_amd_bootblock },
	{ CFI_MFR_MACRONIX, CFI_ID_ANY, fixup_amd_bootblock },
#endif
	{ CFI_MFR_AMD, 0x0050, fixup_use_secsi },
	{ CFI_MFR_AMD, 0x0053, fixup_use_secsi },
	{ CFI_MFR_AMD, 0x0055, fixup_use_secsi },
	{ CFI_MFR_AMD, 0x0056, fixup_use_secsi },
	{ CFI_MFR_AMD, 0x005C, fixup_use_secsi },
	{ CFI_MFR_AMD, 0x005F, fixup_use_secsi },
	{ CFI_MFR_AMD, S29GL064N_MN12, fixup_s29gl064n_sectors },
	{ CFI_MFR_AMD, 0x1301, fixup_s29gl064n_sectors },
	{ CFI_MFR_AMD, 0x1a00, fixup_s29gl032n_sectors },
	{ CFI_MFR_AMD, 0x1a01, fixup_s29gl032n_sectors },
	{ CFI_MFR_AMD, 0x3f00, fixup_s29ns512p_sectors },
	{ CFI_MFR_SST, 0x536a, fixup_sst38vf640x_sectorsize },  
	{ CFI_MFR_SST, 0x536b, fixup_sst38vf640x_sectorsize },  
	{ CFI_MFR_SST, 0x536c, fixup_sst38vf640x_sectorsize },  
	{ CFI_MFR_SST, 0x536d, fixup_sst38vf640x_sectorsize },  
#if !FORCE_WORD_WRITE
	{ CFI_MFR_ANY, CFI_ID_ANY, fixup_use_write_buffers },
#endif
	{ CFI_MFR_ANY, CFI_ID_ANY, fixup_quirks },
	{ 0, 0, NULL }
};
static struct cfi_fixup jedec_fixup_table[] = {
	{ CFI_MFR_SST, SST49LF004B, fixup_use_fwh_lock },
	{ CFI_MFR_SST, SST49LF040B, fixup_use_fwh_lock },
	{ CFI_MFR_SST, SST49LF008A, fixup_use_fwh_lock },
	{ 0, 0, NULL }
};

static struct cfi_fixup fixup_table[] = {
	 
	{ CFI_MFR_ANY, CFI_ID_ANY, fixup_use_erase_chip },
	{ CFI_MFR_ATMEL, AT49BV6416, fixup_use_atmel_lock },
	{ 0, 0, NULL }
};


static void cfi_fixup_major_minor(struct cfi_private *cfi,
				  struct cfi_pri_amdstd *extp)
{
	if (cfi->mfr == CFI_MFR_SAMSUNG) {
		if ((extp->MajorVersion == '0' && extp->MinorVersion == '0') ||
		    (extp->MajorVersion == '3' && extp->MinorVersion == '3')) {
			 
			printk(KERN_NOTICE "  Fixing Samsung's Amd/Fujitsu"
			       " Extended Query version to 1.%c\n",
			       extp->MinorVersion);
			extp->MajorVersion = '1';
		}
	}

	 
	if (cfi->mfr == CFI_MFR_SST && (cfi->id >> 4) == 0x0536) {
		extp->MajorVersion = '1';
		extp->MinorVersion = '0';
	}
}

static int is_m29ew(struct cfi_private *cfi)
{
	if (cfi->mfr == CFI_MFR_INTEL &&
	    ((cfi->device_type == CFI_DEVICETYPE_X8 && (cfi->id & 0xff) == 0x7e) ||
	     (cfi->device_type == CFI_DEVICETYPE_X16 && cfi->id == 0x227e)))
		return 1;
	return 0;
}

 
static void cfi_fixup_m29ew_erase_suspend(struct map_info *map,
					  unsigned long adr)
{
	struct cfi_private *cfi = map->fldrv_priv;
	 
	if (is_m29ew(cfi))
		map_write(map, CMD(0xF0), adr);
}

 
static void cfi_fixup_m29ew_delay_after_resume(struct cfi_private *cfi)
{
	 
	if (is_m29ew(cfi))
		cfi_udelay(500);
}

struct mtd_info *cfi_cmdset_0002(struct map_info *map, int primary)
{
	struct cfi_private *cfi = map->fldrv_priv;
	struct device_node __maybe_unused *np = map->device_node;
	struct mtd_info *mtd;
	int i;

	mtd = kzalloc(sizeof(*mtd), GFP_KERNEL);
	if (!mtd)
		return NULL;
	mtd->priv = map;
	mtd->type = MTD_NORFLASH;

	 
	mtd->_erase   = cfi_amdstd_erase_varsize;
	mtd->_write   = cfi_amdstd_write_words;
	mtd->_read    = cfi_amdstd_read;
	mtd->_sync    = cfi_amdstd_sync;
	mtd->_suspend = cfi_amdstd_suspend;
	mtd->_resume  = cfi_amdstd_resume;
	mtd->_read_user_prot_reg = cfi_amdstd_read_user_prot_reg;
	mtd->_read_fact_prot_reg = cfi_amdstd_read_fact_prot_reg;
	mtd->_get_fact_prot_info = cfi_amdstd_get_fact_prot_info;
	mtd->_get_user_prot_info = cfi_amdstd_get_user_prot_info;
	mtd->_write_user_prot_reg = cfi_amdstd_write_user_prot_reg;
	mtd->_lock_user_prot_reg = cfi_amdstd_lock_user_prot_reg;
	mtd->flags   = MTD_CAP_NORFLASH;
	mtd->name    = map->name;
	mtd->writesize = 1;
	mtd->writebufsize = cfi_interleave(cfi) << cfi->cfiq->MaxBufWriteSize;

	pr_debug("MTD %s(): write buffer size %d\n", __func__,
			mtd->writebufsize);

	mtd->_panic_write = cfi_amdstd_panic_write;
	mtd->reboot_notifier.notifier_call = cfi_amdstd_reboot;

	if (cfi->cfi_mode==CFI_MODE_CFI){
		unsigned char bootloc;
		__u16 adr = primary?cfi->cfiq->P_ADR:cfi->cfiq->A_ADR;
		struct cfi_pri_amdstd *extp;

		extp = (struct cfi_pri_amdstd*)cfi_read_pri(map, adr, sizeof(*extp), "Amd/Fujitsu");
		if (extp) {
			 
			cfi_fixup_major_minor(cfi, extp);

			 
			if (extp->MajorVersion != '1' ||
			    (extp->MajorVersion == '1' && (extp->MinorVersion < '0' || extp->MinorVersion > '5'))) {
				printk(KERN_ERR "  Unknown Amd/Fujitsu Extended Query "
				       "version %c.%c (%#02x/%#02x).\n",
				       extp->MajorVersion, extp->MinorVersion,
				       extp->MajorVersion, extp->MinorVersion);
				kfree(extp);
				kfree(mtd);
				return NULL;
			}

			printk(KERN_INFO "  Amd/Fujitsu Extended Query version %c.%c.\n",
			       extp->MajorVersion, extp->MinorVersion);

			 
			cfi->cmdset_priv = extp;

			 
			cfi_fixup(mtd, cfi_fixup_table);

#ifdef DEBUG_CFI_FEATURES
			 
			cfi_tell_features(extp);
#endif

#ifdef CONFIG_OF
			if (np && of_property_read_bool(
				    np, "use-advanced-sector-protection")
			    && extp->BlkProtUnprot == 8) {
				printk(KERN_INFO "  Advanced Sector Protection (PPB Locking) supported\n");
				mtd->_lock = cfi_ppb_lock;
				mtd->_unlock = cfi_ppb_unlock;
				mtd->_is_locked = cfi_ppb_is_locked;
			}
#endif

			bootloc = extp->TopBottom;
			if ((bootloc < 2) || (bootloc > 5)) {
				printk(KERN_WARNING "%s: CFI contains unrecognised boot "
				       "bank location (%d). Assuming bottom.\n",
				       map->name, bootloc);
				bootloc = 2;
			}

			if (bootloc == 3 && cfi->cfiq->NumEraseRegions > 1) {
				printk(KERN_WARNING "%s: Swapping erase regions for top-boot CFI table.\n", map->name);

				for (i=0; i<cfi->cfiq->NumEraseRegions / 2; i++) {
					int j = (cfi->cfiq->NumEraseRegions-1)-i;

					swap(cfi->cfiq->EraseRegionInfo[i],
					     cfi->cfiq->EraseRegionInfo[j]);
				}
			}
			 
			cfi->addr_unlock1 = 0x555;
			cfi->addr_unlock2 = 0x2aa;
		}
		cfi_fixup(mtd, cfi_nopri_fixup_table);

		if (!cfi->addr_unlock1 || !cfi->addr_unlock2) {
			kfree(mtd);
			return NULL;
		}

	}  
	else if (cfi->cfi_mode == CFI_MODE_JEDEC) {
		 
		cfi_fixup(mtd, jedec_fixup_table);
	}
	 
	cfi_fixup(mtd, fixup_table);

	for (i=0; i< cfi->numchips; i++) {
		cfi->chips[i].word_write_time = 1<<cfi->cfiq->WordWriteTimeoutTyp;
		cfi->chips[i].buffer_write_time = 1<<cfi->cfiq->BufWriteTimeoutTyp;
		cfi->chips[i].erase_time = 1<<cfi->cfiq->BlockEraseTimeoutTyp;
		 
		if (cfi->cfiq->BufWriteTimeoutTyp &&
		    cfi->cfiq->BufWriteTimeoutMax)
			cfi->chips[i].buffer_write_time_max =
				1 << (cfi->cfiq->BufWriteTimeoutTyp +
				      cfi->cfiq->BufWriteTimeoutMax);
		else
			cfi->chips[i].buffer_write_time_max = 0;

		cfi->chips[i].buffer_write_time_max =
			max(cfi->chips[i].buffer_write_time_max, 2000);

		cfi->chips[i].ref_point_counter = 0;
		init_waitqueue_head(&(cfi->chips[i].wq));
	}

	map->fldrv = &cfi_amdstd_chipdrv;

	return cfi_amdstd_setup(mtd);
}
struct mtd_info *cfi_cmdset_0006(struct map_info *map, int primary) __attribute__((alias("cfi_cmdset_0002")));
struct mtd_info *cfi_cmdset_0701(struct map_info *map, int primary) __attribute__((alias("cfi_cmdset_0002")));
EXPORT_SYMBOL_GPL(cfi_cmdset_0002);
EXPORT_SYMBOL_GPL(cfi_cmdset_0006);
EXPORT_SYMBOL_GPL(cfi_cmdset_0701);

static struct mtd_info *cfi_amdstd_setup(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long devsize = (1<<cfi->cfiq->DevSize) * cfi->interleave;
	unsigned long offset = 0;
	int i,j;

	printk(KERN_NOTICE "number of %s chips: %d\n",
	       (cfi->cfi_mode == CFI_MODE_CFI)?"CFI":"JEDEC",cfi->numchips);
	 
	mtd->size = devsize * cfi->numchips;

	mtd->numeraseregions = cfi->cfiq->NumEraseRegions * cfi->numchips;
	mtd->eraseregions = kmalloc_array(mtd->numeraseregions,
					  sizeof(struct mtd_erase_region_info),
					  GFP_KERNEL);
	if (!mtd->eraseregions)
		goto setup_err;

	for (i=0; i<cfi->cfiq->NumEraseRegions; i++) {
		unsigned long ernum, ersize;
		ersize = ((cfi->cfiq->EraseRegionInfo[i] >> 8) & ~0xff) * cfi->interleave;
		ernum = (cfi->cfiq->EraseRegionInfo[i] & 0xffff) + 1;

		if (mtd->erasesize < ersize) {
			mtd->erasesize = ersize;
		}
		for (j=0; j<cfi->numchips; j++) {
			mtd->eraseregions[(j*cfi->cfiq->NumEraseRegions)+i].offset = (j*devsize)+offset;
			mtd->eraseregions[(j*cfi->cfiq->NumEraseRegions)+i].erasesize = ersize;
			mtd->eraseregions[(j*cfi->cfiq->NumEraseRegions)+i].numblocks = ernum;
		}
		offset += (ersize * ernum);
	}
	if (offset != devsize) {
		 
		printk(KERN_WARNING "Sum of regions (%lx) != total size of set of interleaved chips (%lx)\n", offset, devsize);
		goto setup_err;
	}

	__module_get(THIS_MODULE);
	register_reboot_notifier(&mtd->reboot_notifier);
	return mtd;

 setup_err:
	kfree(mtd->eraseregions);
	kfree(mtd);
	kfree(cfi->cmdset_priv);
	return NULL;
}

 
static int __xipram chip_ready(struct map_info *map, struct flchip *chip,
			       unsigned long addr, map_word *expected)
{
	struct cfi_private *cfi = map->fldrv_priv;
	map_word oldd, curd;
	int ret;

	if (cfi_use_status_reg(cfi)) {
		map_word ready = CMD(CFI_SR_DRB);
		 
		cfi_send_gen_cmd(0x70, cfi->addr_unlock1, chip->start, map, cfi,
				 cfi->device_type, NULL);
		curd = map_read(map, addr);

		return map_word_andequal(map, curd, ready, ready);
	}

	oldd = map_read(map, addr);
	curd = map_read(map, addr);

	ret = map_word_equal(map, oldd, curd);

	if (!ret || !expected)
		return ret;

	return map_word_equal(map, curd, *expected);
}

static int __xipram chip_good(struct map_info *map, struct flchip *chip,
			      unsigned long addr, map_word *expected)
{
	struct cfi_private *cfi = map->fldrv_priv;
	map_word *datum = expected;

	if (cfi->quirks & CFI_QUIRK_DQ_TRUE_DATA)
		datum = NULL;

	return chip_ready(map, chip, addr, datum);
}

static int get_chip(struct map_info *map, struct flchip *chip, unsigned long adr, int mode)
{
	DECLARE_WAITQUEUE(wait, current);
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long timeo;
	struct cfi_pri_amdstd *cfip = (struct cfi_pri_amdstd *)cfi->cmdset_priv;

 resettime:
	timeo = jiffies + HZ;
 retry:
	switch (chip->state) {

	case FL_STATUS:
		for (;;) {
			if (chip_ready(map, chip, adr, NULL))
				break;

			if (time_after(jiffies, timeo)) {
				printk(KERN_ERR "Waiting for chip to be ready timed out.\n");
				return -EIO;
			}
			mutex_unlock(&chip->mutex);
			cfi_udelay(1);
			mutex_lock(&chip->mutex);
			 
			goto retry;
		}
		return 0;

	case FL_READY:
	case FL_CFI_QUERY:
	case FL_JEDEC_QUERY:
		return 0;

	case FL_ERASING:
		if (!cfip || !(cfip->EraseSuspend & (0x1|0x2)) ||
		    !(mode == FL_READY || mode == FL_POINT ||
		    (mode == FL_WRITING && (cfip->EraseSuspend & 0x2))))
			goto sleep;

		 
		if ((adr & chip->in_progress_block_mask) ==
		    chip->in_progress_block_addr)
			goto sleep;

		 
		 
		map_write(map, CMD(0xB0), chip->in_progress_block_addr);
		chip->oldstate = FL_ERASING;
		chip->state = FL_ERASE_SUSPENDING;
		chip->erase_suspended = 1;
		for (;;) {
			if (chip_ready(map, chip, adr, NULL))
				break;

			if (time_after(jiffies, timeo)) {
				 
				put_chip(map, chip, adr);
				printk(KERN_ERR "MTD %s(): chip not ready after erase suspend\n", __func__);
				return -EIO;
			}

			mutex_unlock(&chip->mutex);
			cfi_udelay(1);
			mutex_lock(&chip->mutex);
			 
		}
		chip->state = FL_READY;
		return 0;

	case FL_XIP_WHILE_ERASING:
		if (mode != FL_READY && mode != FL_POINT &&
		    (!cfip || !(cfip->EraseSuspend&2)))
			goto sleep;
		chip->oldstate = chip->state;
		chip->state = FL_READY;
		return 0;

	case FL_SHUTDOWN:
		 
		return -EIO;

	case FL_POINT:
		 
		if (mode == FL_READY && chip->oldstate == FL_READY)
			return 0;
		fallthrough;
	default:
	sleep:
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);
		mutex_unlock(&chip->mutex);
		schedule();
		remove_wait_queue(&chip->wq, &wait);
		mutex_lock(&chip->mutex);
		goto resettime;
	}
}


static void put_chip(struct map_info *map, struct flchip *chip, unsigned long adr)
{
	struct cfi_private *cfi = map->fldrv_priv;

	switch(chip->oldstate) {
	case FL_ERASING:
		cfi_fixup_m29ew_erase_suspend(map,
			chip->in_progress_block_addr);
		map_write(map, cfi->sector_erase_cmd, chip->in_progress_block_addr);
		cfi_fixup_m29ew_delay_after_resume(cfi);
		chip->oldstate = FL_READY;
		chip->state = FL_ERASING;
		break;

	case FL_XIP_WHILE_ERASING:
		chip->state = chip->oldstate;
		chip->oldstate = FL_READY;
		break;

	case FL_READY:
	case FL_STATUS:
		break;
	default:
		printk(KERN_ERR "MTD: put_chip() called with oldstate %d!!\n", chip->oldstate);
	}
	wake_up(&chip->wq);
}

#ifdef CONFIG_MTD_XIP

 

static void xip_disable(struct map_info *map, struct flchip *chip,
			unsigned long adr)
{
	 
	(void) map_read(map, adr);  
	local_irq_disable();
}

static void __xipram xip_enable(struct map_info *map, struct flchip *chip,
				unsigned long adr)
{
	struct cfi_private *cfi = map->fldrv_priv;

	if (chip->state != FL_POINT && chip->state != FL_READY) {
		map_write(map, CMD(0xf0), adr);
		chip->state = FL_READY;
	}
	(void) map_read(map, adr);
	xip_iprefetch();
	local_irq_enable();
}

 

static void __xipram xip_udelay(struct map_info *map, struct flchip *chip,
				unsigned long adr, int usec)
{
	struct cfi_private *cfi = map->fldrv_priv;
	struct cfi_pri_amdstd *extp = cfi->cmdset_priv;
	map_word status, OK = CMD(0x80);
	unsigned long suspended, start = xip_currtime();
	flstate_t oldstate;

	do {
		cpu_relax();
		if (xip_irqpending() && extp &&
		    ((chip->state == FL_ERASING && (extp->EraseSuspend & 2))) &&
		    (cfi_interleave_is_1(cfi) || chip->oldstate == FL_READY)) {
			 
			map_write(map, CMD(0xb0), adr);
			usec -= xip_elapsed_since(start);
			suspended = xip_currtime();
			do {
				if (xip_elapsed_since(suspended) > 100000) {
					 
					return;
				}
				status = map_read(map, adr);
			} while (!map_word_andequal(map, status, OK, OK));

			 
			oldstate = chip->state;
			if (!map_word_bitsset(map, status, CMD(0x40)))
				break;
			chip->state = FL_XIP_WHILE_ERASING;
			chip->erase_suspended = 1;
			map_write(map, CMD(0xf0), adr);
			(void) map_read(map, adr);
			xip_iprefetch();
			local_irq_enable();
			mutex_unlock(&chip->mutex);
			xip_iprefetch();
			cond_resched();

			 
			mutex_lock(&chip->mutex);
			while (chip->state != FL_XIP_WHILE_ERASING) {
				DECLARE_WAITQUEUE(wait, current);
				set_current_state(TASK_UNINTERRUPTIBLE);
				add_wait_queue(&chip->wq, &wait);
				mutex_unlock(&chip->mutex);
				schedule();
				remove_wait_queue(&chip->wq, &wait);
				mutex_lock(&chip->mutex);
			}
			 
			local_irq_disable();

			 
			cfi_fixup_m29ew_erase_suspend(map, adr);
			 
			map_write(map, cfi->sector_erase_cmd, adr);
			chip->state = oldstate;
			start = xip_currtime();
		} else if (usec >= 1000000/HZ) {
			 
			xip_cpu_idle();
		}
		status = map_read(map, adr);
	} while (!map_word_andequal(map, status, OK, OK)
		 && xip_elapsed_since(start) < usec);
}

#define UDELAY(map, chip, adr, usec)  xip_udelay(map, chip, adr, usec)

 
#define XIP_INVAL_CACHED_RANGE(map, from, size)  \
	INVALIDATE_CACHED_RANGE(map, from, size)

#define INVALIDATE_CACHE_UDELAY(map, chip, adr, len, usec)  \
	UDELAY(map, chip, adr, usec)

 

#else

#define xip_disable(map, chip, adr)
#define xip_enable(map, chip, adr)
#define XIP_INVAL_CACHED_RANGE(x...)

#define UDELAY(map, chip, adr, usec)  \
do {  \
	mutex_unlock(&chip->mutex);  \
	cfi_udelay(usec);  \
	mutex_lock(&chip->mutex);  \
} while (0)

#define INVALIDATE_CACHE_UDELAY(map, chip, adr, len, usec)  \
do {  \
	mutex_unlock(&chip->mutex);  \
	INVALIDATE_CACHED_RANGE(map, adr, len);  \
	cfi_udelay(usec);  \
	mutex_lock(&chip->mutex);  \
} while (0)

#endif

static inline int do_read_onechip(struct map_info *map, struct flchip *chip, loff_t adr, size_t len, u_char *buf)
{
	unsigned long cmd_addr;
	struct cfi_private *cfi = map->fldrv_priv;
	int ret;

	adr += chip->start;

	 
	cmd_addr = adr & ~(map_bankwidth(map)-1);

	mutex_lock(&chip->mutex);
	ret = get_chip(map, chip, cmd_addr, FL_READY);
	if (ret) {
		mutex_unlock(&chip->mutex);
		return ret;
	}

	if (chip->state != FL_POINT && chip->state != FL_READY) {
		map_write(map, CMD(0xf0), cmd_addr);
		chip->state = FL_READY;
	}

	map_copy_from(map, buf, adr, len);

	put_chip(map, chip, cmd_addr);

	mutex_unlock(&chip->mutex);
	return 0;
}


static int cfi_amdstd_read (struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long ofs;
	int chipnum;
	int ret = 0;

	 
	chipnum = (from >> cfi->chipshift);
	ofs = from - (chipnum <<  cfi->chipshift);

	while (len) {
		unsigned long thislen;

		if (chipnum >= cfi->numchips)
			break;

		if ((len + ofs -1) >> cfi->chipshift)
			thislen = (1<<cfi->chipshift) - ofs;
		else
			thislen = len;

		ret = do_read_onechip(map, &cfi->chips[chipnum], ofs, thislen, buf);
		if (ret)
			break;

		*retlen += thislen;
		len -= thislen;
		buf += thislen;

		ofs = 0;
		chipnum++;
	}
	return ret;
}

typedef int (*otp_op_t)(struct map_info *map, struct flchip *chip,
			loff_t adr, size_t len, u_char *buf, size_t grouplen);

static inline void otp_enter(struct map_info *map, struct flchip *chip,
			     loff_t adr, size_t len)
{
	struct cfi_private *cfi = map->fldrv_priv;

	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi,
			 cfi->device_type, NULL);
	cfi_send_gen_cmd(0x88, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);

	INVALIDATE_CACHED_RANGE(map, chip->start + adr, len);
}

static inline void otp_exit(struct map_info *map, struct flchip *chip,
			    loff_t adr, size_t len)
{
	struct cfi_private *cfi = map->fldrv_priv;

	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi,
			 cfi->device_type, NULL);
	cfi_send_gen_cmd(0x90, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);
	cfi_send_gen_cmd(0x00, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);

	INVALIDATE_CACHED_RANGE(map, chip->start + adr, len);
}

static inline int do_read_secsi_onechip(struct map_info *map,
					struct flchip *chip, loff_t adr,
					size_t len, u_char *buf,
					size_t grouplen)
{
	DECLARE_WAITQUEUE(wait, current);

 retry:
	mutex_lock(&chip->mutex);

	if (chip->state != FL_READY){
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);

		mutex_unlock(&chip->mutex);

		schedule();
		remove_wait_queue(&chip->wq, &wait);

		goto retry;
	}

	adr += chip->start;

	chip->state = FL_READY;

	otp_enter(map, chip, adr, len);
	map_copy_from(map, buf, adr, len);
	otp_exit(map, chip, adr, len);

	wake_up(&chip->wq);
	mutex_unlock(&chip->mutex);

	return 0;
}

static int cfi_amdstd_secsi_read (struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long ofs;
	int chipnum;
	int ret = 0;

	 
	 
	chipnum=from>>3;
	ofs=from & 7;

	while (len) {
		unsigned long thislen;

		if (chipnum >= cfi->numchips)
			break;

		if ((len + ofs -1) >> 3)
			thislen = (1<<3) - ofs;
		else
			thislen = len;

		ret = do_read_secsi_onechip(map, &cfi->chips[chipnum], ofs,
					    thislen, buf, 0);
		if (ret)
			break;

		*retlen += thislen;
		len -= thislen;
		buf += thislen;

		ofs = 0;
		chipnum++;
	}
	return ret;
}

static int __xipram do_write_oneword(struct map_info *map, struct flchip *chip,
				     unsigned long adr, map_word datum,
				     int mode);

static int do_otp_write(struct map_info *map, struct flchip *chip, loff_t adr,
			size_t len, u_char *buf, size_t grouplen)
{
	int ret;
	while (len) {
		unsigned long bus_ofs = adr & ~(map_bankwidth(map)-1);
		int gap = adr - bus_ofs;
		int n = min_t(int, len, map_bankwidth(map) - gap);
		map_word datum = map_word_ff(map);

		if (n != map_bankwidth(map)) {
			 
			otp_enter(map, chip, bus_ofs, map_bankwidth(map));
			datum = map_read(map, bus_ofs);
			otp_exit(map, chip, bus_ofs, map_bankwidth(map));
		}

		datum = map_word_load_partial(map, datum, buf, gap, n);
		ret = do_write_oneword(map, chip, bus_ofs, datum, FL_OTP_WRITE);
		if (ret)
			return ret;

		adr += n;
		buf += n;
		len -= n;
	}

	return 0;
}

static int do_otp_lock(struct map_info *map, struct flchip *chip, loff_t adr,
		       size_t len, u_char *buf, size_t grouplen)
{
	struct cfi_private *cfi = map->fldrv_priv;
	uint8_t lockreg;
	unsigned long timeo;
	int ret;

	 
	if ((adr != 0) || (len != grouplen))
		return -EINVAL;

	mutex_lock(&chip->mutex);
	ret = get_chip(map, chip, chip->start, FL_LOCKING);
	if (ret) {
		mutex_unlock(&chip->mutex);
		return ret;
	}
	chip->state = FL_LOCKING;

	 
	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi,
			 cfi->device_type, NULL);
	cfi_send_gen_cmd(0x40, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);

	 
	lockreg = cfi_read_query(map, 0);

	 
	lockreg &= ~0x01;

	 
	 
	map_write(map, CMD(0xA0), chip->start);
	map_write(map, CMD(lockreg), chip->start);

	 
	timeo = jiffies + msecs_to_jiffies(2);
	for (;;) {
		if (chip_ready(map, chip, adr, NULL))
			break;

		if (time_after(jiffies, timeo)) {
			pr_err("Waiting for chip to be ready timed out.\n");
			ret = -EIO;
			break;
		}
		UDELAY(map, chip, 0, 1);
	}

	 
	map_write(map, CMD(0x90), chip->start);
	map_write(map, CMD(0x00), chip->start);

	chip->state = FL_READY;
	put_chip(map, chip, chip->start);
	mutex_unlock(&chip->mutex);

	return ret;
}

static int cfi_amdstd_otp_walk(struct mtd_info *mtd, loff_t from, size_t len,
			       size_t *retlen, u_char *buf,
			       otp_op_t action, int user_regs)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int ofs_factor = cfi->interleave * cfi->device_type;
	unsigned long base;
	int chipnum;
	struct flchip *chip;
	uint8_t otp, lockreg;
	int ret;

	size_t user_size, factory_size, otpsize;
	loff_t user_offset, factory_offset, otpoffset;
	int user_locked = 0, otplocked;

	*retlen = 0;

	for (chipnum = 0; chipnum < cfi->numchips; chipnum++) {
		chip = &cfi->chips[chipnum];
		factory_size = 0;
		user_size = 0;

		 
		if (is_m29ew(cfi)) {
			base = chip->start;

			 
			mutex_lock(&chip->mutex);
			ret = get_chip(map, chip, base, FL_CFI_QUERY);
			if (ret) {
				mutex_unlock(&chip->mutex);
				return ret;
			}
			cfi_qry_mode_on(base, map, cfi);
			otp = cfi_read_query(map, base + 0x3 * ofs_factor);
			cfi_qry_mode_off(base, map, cfi);
			put_chip(map, chip, base);
			mutex_unlock(&chip->mutex);

			if (otp & 0x80) {
				 
				factory_offset = 0;
				factory_size = 0x100;
			} else {
				 
				user_offset = 0;
				user_size = 0x100;

				mutex_lock(&chip->mutex);
				ret = get_chip(map, chip, base, FL_LOCKING);
				if (ret) {
					mutex_unlock(&chip->mutex);
					return ret;
				}

				 
				cfi_send_gen_cmd(0xAA, cfi->addr_unlock1,
						 chip->start, map, cfi,
						 cfi->device_type, NULL);
				cfi_send_gen_cmd(0x55, cfi->addr_unlock2,
						 chip->start, map, cfi,
						 cfi->device_type, NULL);
				cfi_send_gen_cmd(0x40, cfi->addr_unlock1,
						 chip->start, map, cfi,
						 cfi->device_type, NULL);
				 
				lockreg = cfi_read_query(map, 0);
				 
				map_write(map, CMD(0x90), chip->start);
				map_write(map, CMD(0x00), chip->start);
				put_chip(map, chip, chip->start);
				mutex_unlock(&chip->mutex);

				user_locked = ((lockreg & 0x01) == 0x00);
			}
		}

		otpsize = user_regs ? user_size : factory_size;
		if (!otpsize)
			continue;
		otpoffset = user_regs ? user_offset : factory_offset;
		otplocked = user_regs ? user_locked : 1;

		if (!action) {
			 
			struct otp_info *otpinfo;
			len -= sizeof(*otpinfo);
			if (len <= 0)
				return -ENOSPC;
			otpinfo = (struct otp_info *)buf;
			otpinfo->start = from;
			otpinfo->length = otpsize;
			otpinfo->locked = otplocked;
			buf += sizeof(*otpinfo);
			*retlen += sizeof(*otpinfo);
			from += otpsize;
		} else if ((from < otpsize) && (len > 0)) {
			size_t size;
			size = (len < otpsize - from) ? len : otpsize - from;
			ret = action(map, chip, otpoffset + from, size, buf,
				     otpsize);
			if (ret < 0)
				return ret;

			buf += size;
			len -= size;
			*retlen += size;
			from = 0;
		} else {
			from -= otpsize;
		}
	}
	return 0;
}

static int cfi_amdstd_get_fact_prot_info(struct mtd_info *mtd, size_t len,
					 size_t *retlen, struct otp_info *buf)
{
	return cfi_amdstd_otp_walk(mtd, 0, len, retlen, (u_char *)buf,
				   NULL, 0);
}

static int cfi_amdstd_get_user_prot_info(struct mtd_info *mtd, size_t len,
					 size_t *retlen, struct otp_info *buf)
{
	return cfi_amdstd_otp_walk(mtd, 0, len, retlen, (u_char *)buf,
				   NULL, 1);
}

static int cfi_amdstd_read_fact_prot_reg(struct mtd_info *mtd, loff_t from,
					 size_t len, size_t *retlen,
					 u_char *buf)
{
	return cfi_amdstd_otp_walk(mtd, from, len, retlen,
				   buf, do_read_secsi_onechip, 0);
}

static int cfi_amdstd_read_user_prot_reg(struct mtd_info *mtd, loff_t from,
					 size_t len, size_t *retlen,
					 u_char *buf)
{
	return cfi_amdstd_otp_walk(mtd, from, len, retlen,
				   buf, do_read_secsi_onechip, 1);
}

static int cfi_amdstd_write_user_prot_reg(struct mtd_info *mtd, loff_t from,
					  size_t len, size_t *retlen,
					  const u_char *buf)
{
	return cfi_amdstd_otp_walk(mtd, from, len, retlen, (u_char *)buf,
				   do_otp_write, 1);
}

static int cfi_amdstd_lock_user_prot_reg(struct mtd_info *mtd, loff_t from,
					 size_t len)
{
	size_t retlen;
	return cfi_amdstd_otp_walk(mtd, from, len, &retlen, NULL,
				   do_otp_lock, 1);
}

static int __xipram do_write_oneword_once(struct map_info *map,
					  struct flchip *chip,
					  unsigned long adr, map_word datum,
					  int mode, struct cfi_private *cfi)
{
	unsigned long timeo;
	 
	unsigned long uWriteTimeout = (HZ / 1000) + 1;
	int ret = 0;

	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0xA0, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	map_write(map, datum, adr);
	chip->state = mode;

	INVALIDATE_CACHE_UDELAY(map, chip,
				adr, map_bankwidth(map),
				chip->word_write_time);

	 
	timeo = jiffies + uWriteTimeout;
	for (;;) {
		if (chip->state != mode) {
			 
			DECLARE_WAITQUEUE(wait, current);

			set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&chip->wq, &wait);
			mutex_unlock(&chip->mutex);
			schedule();
			remove_wait_queue(&chip->wq, &wait);
			timeo = jiffies + (HZ / 2);  
			mutex_lock(&chip->mutex);
			continue;
		}

		 
		if (time_after(jiffies, timeo) &&
		    !chip_good(map, chip, adr, &datum)) {
			xip_enable(map, chip, adr);
			printk(KERN_WARNING "MTD %s(): software timeout\n", __func__);
			xip_disable(map, chip, adr);
			ret = -EIO;
			break;
		}

		if (chip_good(map, chip, adr, &datum)) {
			if (cfi_check_err_status(map, chip, adr))
				ret = -EIO;
			break;
		}

		 
		UDELAY(map, chip, adr, 1);
	}

	return ret;
}

static int __xipram do_write_oneword_start(struct map_info *map,
					   struct flchip *chip,
					   unsigned long adr, int mode)
{
	int ret;

	mutex_lock(&chip->mutex);

	ret = get_chip(map, chip, adr, mode);
	if (ret) {
		mutex_unlock(&chip->mutex);
		return ret;
	}

	if (mode == FL_OTP_WRITE)
		otp_enter(map, chip, adr, map_bankwidth(map));

	return ret;
}

static void __xipram do_write_oneword_done(struct map_info *map,
					   struct flchip *chip,
					   unsigned long adr, int mode)
{
	if (mode == FL_OTP_WRITE)
		otp_exit(map, chip, adr, map_bankwidth(map));

	chip->state = FL_READY;
	DISABLE_VPP(map);
	put_chip(map, chip, adr);

	mutex_unlock(&chip->mutex);
}

static int __xipram do_write_oneword_retry(struct map_info *map,
					   struct flchip *chip,
					   unsigned long adr, map_word datum,
					   int mode)
{
	struct cfi_private *cfi = map->fldrv_priv;
	int ret = 0;
	map_word oldd;
	int retry_cnt = 0;

	 
	oldd = map_read(map, adr);
	if (map_word_equal(map, oldd, datum)) {
		pr_debug("MTD %s(): NOP\n", __func__);
		return ret;
	}

	XIP_INVAL_CACHED_RANGE(map, adr, map_bankwidth(map));
	ENABLE_VPP(map);
	xip_disable(map, chip, adr);

 retry:
	ret = do_write_oneword_once(map, chip, adr, datum, mode, cfi);
	if (ret) {
		 
		map_write(map, CMD(0xF0), chip->start);
		 

		if (++retry_cnt <= MAX_RETRIES) {
			ret = 0;
			goto retry;
		}
	}
	xip_enable(map, chip, adr);

	return ret;
}

static int __xipram do_write_oneword(struct map_info *map, struct flchip *chip,
				     unsigned long adr, map_word datum,
				     int mode)
{
	int ret;

	adr += chip->start;

	pr_debug("MTD %s(): WRITE 0x%.8lx(0x%.8lx)\n", __func__, adr,
		 datum.x[0]);

	ret = do_write_oneword_start(map, chip, adr, mode);
	if (ret)
		return ret;

	ret = do_write_oneword_retry(map, chip, adr, datum, mode);

	do_write_oneword_done(map, chip, adr, mode);

	return ret;
}


static int cfi_amdstd_write_words(struct mtd_info *mtd, loff_t to, size_t len,
				  size_t *retlen, const u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int ret;
	int chipnum;
	unsigned long ofs, chipstart;
	DECLARE_WAITQUEUE(wait, current);

	chipnum = to >> cfi->chipshift;
	ofs = to  - (chipnum << cfi->chipshift);
	chipstart = cfi->chips[chipnum].start;

	 
	if (ofs & (map_bankwidth(map)-1)) {
		unsigned long bus_ofs = ofs & ~(map_bankwidth(map)-1);
		int i = ofs - bus_ofs;
		int n = 0;
		map_word tmp_buf;

 retry:
		mutex_lock(&cfi->chips[chipnum].mutex);

		if (cfi->chips[chipnum].state != FL_READY) {
			set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&cfi->chips[chipnum].wq, &wait);

			mutex_unlock(&cfi->chips[chipnum].mutex);

			schedule();
			remove_wait_queue(&cfi->chips[chipnum].wq, &wait);
			goto retry;
		}

		 
		tmp_buf = map_read(map, bus_ofs+chipstart);

		mutex_unlock(&cfi->chips[chipnum].mutex);

		 
		n = min_t(int, len, map_bankwidth(map)-i);

		tmp_buf = map_word_load_partial(map, tmp_buf, buf, i, n);

		ret = do_write_oneword(map, &cfi->chips[chipnum],
				       bus_ofs, tmp_buf, FL_WRITING);
		if (ret)
			return ret;

		ofs += n;
		buf += n;
		(*retlen) += n;
		len -= n;

		if (ofs >> cfi->chipshift) {
			chipnum ++;
			ofs = 0;
			if (chipnum == cfi->numchips)
				return 0;
		}
	}

	 
	while(len >= map_bankwidth(map)) {
		map_word datum;

		datum = map_word_load(map, buf);

		ret = do_write_oneword(map, &cfi->chips[chipnum],
				       ofs, datum, FL_WRITING);
		if (ret)
			return ret;

		ofs += map_bankwidth(map);
		buf += map_bankwidth(map);
		(*retlen) += map_bankwidth(map);
		len -= map_bankwidth(map);

		if (ofs >> cfi->chipshift) {
			chipnum ++;
			ofs = 0;
			if (chipnum == cfi->numchips)
				return 0;
			chipstart = cfi->chips[chipnum].start;
		}
	}

	 
	if (len & (map_bankwidth(map)-1)) {
		map_word tmp_buf;

 retry1:
		mutex_lock(&cfi->chips[chipnum].mutex);

		if (cfi->chips[chipnum].state != FL_READY) {
			set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&cfi->chips[chipnum].wq, &wait);

			mutex_unlock(&cfi->chips[chipnum].mutex);

			schedule();
			remove_wait_queue(&cfi->chips[chipnum].wq, &wait);
			goto retry1;
		}

		tmp_buf = map_read(map, ofs + chipstart);

		mutex_unlock(&cfi->chips[chipnum].mutex);

		tmp_buf = map_word_load_partial(map, tmp_buf, buf, 0, len);

		ret = do_write_oneword(map, &cfi->chips[chipnum],
				       ofs, tmp_buf, FL_WRITING);
		if (ret)
			return ret;

		(*retlen) += len;
	}

	return 0;
}

#if !FORCE_WORD_WRITE
static int __xipram do_write_buffer_wait(struct map_info *map,
					 struct flchip *chip, unsigned long adr,
					 map_word datum)
{
	unsigned long timeo;
	unsigned long u_write_timeout;
	int ret = 0;

	 
	u_write_timeout = usecs_to_jiffies(chip->buffer_write_time_max);
	timeo = jiffies + u_write_timeout;

	for (;;) {
		if (chip->state != FL_WRITING) {
			 
			DECLARE_WAITQUEUE(wait, current);

			set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&chip->wq, &wait);
			mutex_unlock(&chip->mutex);
			schedule();
			remove_wait_queue(&chip->wq, &wait);
			timeo = jiffies + (HZ / 2);  
			mutex_lock(&chip->mutex);
			continue;
		}

		 
		if (time_after(jiffies, timeo) &&
		    !chip_good(map, chip, adr, &datum)) {
			pr_err("MTD %s(): software timeout, address:0x%.8lx.\n",
			       __func__, adr);
			ret = -EIO;
			break;
		}

		if (chip_good(map, chip, adr, &datum)) {
			if (cfi_check_err_status(map, chip, adr))
				ret = -EIO;
			break;
		}

		 
		UDELAY(map, chip, adr, 1);
	}

	return ret;
}

static void __xipram do_write_buffer_reset(struct map_info *map,
					   struct flchip *chip,
					   struct cfi_private *cfi)
{
	 
	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi,
			 cfi->device_type, NULL);
	cfi_send_gen_cmd(0xF0, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);

	 
}

 
static int __xipram do_write_buffer(struct map_info *map, struct flchip *chip,
				    unsigned long adr, const u_char *buf,
				    int len)
{
	struct cfi_private *cfi = map->fldrv_priv;
	int ret;
	unsigned long cmd_adr;
	int z, words;
	map_word datum;

	adr += chip->start;
	cmd_adr = adr;

	mutex_lock(&chip->mutex);
	ret = get_chip(map, chip, adr, FL_WRITING);
	if (ret) {
		mutex_unlock(&chip->mutex);
		return ret;
	}

	datum = map_word_load(map, buf);

	pr_debug("MTD %s(): WRITE 0x%.8lx(0x%.8lx)\n",
		 __func__, adr, datum.x[0]);

	XIP_INVAL_CACHED_RANGE(map, adr, len);
	ENABLE_VPP(map);
	xip_disable(map, chip, cmd_adr);

	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, cfi->device_type, NULL);

	 
	map_write(map, CMD(0x25), cmd_adr);

	chip->state = FL_WRITING_TO_BUFFER;

	 
	words = len / map_bankwidth(map);
	map_write(map, CMD(words - 1), cmd_adr);
	 
	z = 0;
	while(z < words * map_bankwidth(map)) {
		datum = map_word_load(map, buf);
		map_write(map, datum, adr + z);

		z += map_bankwidth(map);
		buf += map_bankwidth(map);
	}
	z -= map_bankwidth(map);

	adr += z;

	 
	map_write(map, CMD(0x29), cmd_adr);
	chip->state = FL_WRITING;

	INVALIDATE_CACHE_UDELAY(map, chip,
				adr, map_bankwidth(map),
				chip->word_write_time);

	ret = do_write_buffer_wait(map, chip, adr, datum);
	if (ret)
		do_write_buffer_reset(map, chip, cfi);

	xip_enable(map, chip, adr);

	chip->state = FL_READY;
	DISABLE_VPP(map);
	put_chip(map, chip, adr);
	mutex_unlock(&chip->mutex);

	return ret;
}


static int cfi_amdstd_write_buffers(struct mtd_info *mtd, loff_t to, size_t len,
				    size_t *retlen, const u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int wbufsize = cfi_interleave(cfi) << cfi->cfiq->MaxBufWriteSize;
	int ret;
	int chipnum;
	unsigned long ofs;

	chipnum = to >> cfi->chipshift;
	ofs = to  - (chipnum << cfi->chipshift);

	 
	if (ofs & (map_bankwidth(map)-1)) {
		size_t local_len = (-ofs)&(map_bankwidth(map)-1);
		if (local_len > len)
			local_len = len;
		ret = cfi_amdstd_write_words(mtd, ofs + (chipnum<<cfi->chipshift),
					     local_len, retlen, buf);
		if (ret)
			return ret;
		ofs += local_len;
		buf += local_len;
		len -= local_len;

		if (ofs >> cfi->chipshift) {
			chipnum ++;
			ofs = 0;
			if (chipnum == cfi->numchips)
				return 0;
		}
	}

	 
	while (len >= map_bankwidth(map) * 2) {
		 
		int size = wbufsize - (ofs & (wbufsize-1));

		if (size > len)
			size = len;
		if (size % map_bankwidth(map))
			size -= size % map_bankwidth(map);

		ret = do_write_buffer(map, &cfi->chips[chipnum],
				      ofs, buf, size);
		if (ret)
			return ret;

		ofs += size;
		buf += size;
		(*retlen) += size;
		len -= size;

		if (ofs >> cfi->chipshift) {
			chipnum ++;
			ofs = 0;
			if (chipnum == cfi->numchips)
				return 0;
		}
	}

	if (len) {
		size_t retlen_dregs = 0;

		ret = cfi_amdstd_write_words(mtd, ofs + (chipnum<<cfi->chipshift),
					     len, &retlen_dregs, buf);

		*retlen += retlen_dregs;
		return ret;
	}

	return 0;
}
#endif  

 
static int cfi_amdstd_panic_wait(struct map_info *map, struct flchip *chip,
				 unsigned long adr)
{
	struct cfi_private *cfi = map->fldrv_priv;
	int retries = 10;
	int i;

	 
	if (chip->state == FL_READY && chip_ready(map, chip, adr, NULL))
		return 0;

	 
	while (retries > 0) {
		const unsigned long timeo = (HZ / 1000) + 1;

		 
		map_write(map, CMD(0xF0), chip->start);

		 
		for (i = 0; i < jiffies_to_usecs(timeo); i++) {
			if (chip_ready(map, chip, adr, NULL))
				return 0;

			udelay(1);
		}

		retries--;
	}

	 
	return -EBUSY;
}

 
static int do_panic_write_oneword(struct map_info *map, struct flchip *chip,
				  unsigned long adr, map_word datum)
{
	const unsigned long uWriteTimeout = (HZ / 1000) + 1;
	struct cfi_private *cfi = map->fldrv_priv;
	int retry_cnt = 0;
	map_word oldd;
	int ret;
	int i;

	adr += chip->start;

	ret = cfi_amdstd_panic_wait(map, chip, adr);
	if (ret)
		return ret;

	pr_debug("MTD %s(): PANIC WRITE 0x%.8lx(0x%.8lx)\n",
			__func__, adr, datum.x[0]);

	 
	oldd = map_read(map, adr);
	if (map_word_equal(map, oldd, datum)) {
		pr_debug("MTD %s(): NOP\n", __func__);
		goto op_done;
	}

	ENABLE_VPP(map);

retry:
	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0xA0, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	map_write(map, datum, adr);

	for (i = 0; i < jiffies_to_usecs(uWriteTimeout); i++) {
		if (chip_ready(map, chip, adr, NULL))
			break;

		udelay(1);
	}

	if (!chip_ready(map, chip, adr, &datum) ||
	    cfi_check_err_status(map, chip, adr)) {
		 
		map_write(map, CMD(0xF0), chip->start);
		 

		if (++retry_cnt <= MAX_RETRIES)
			goto retry;

		ret = -EIO;
	}

op_done:
	DISABLE_VPP(map);
	return ret;
}

 
static int cfi_amdstd_panic_write(struct mtd_info *mtd, loff_t to, size_t len,
				  size_t *retlen, const u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long ofs, chipstart;
	int ret;
	int chipnum;

	chipnum = to >> cfi->chipshift;
	ofs = to - (chipnum << cfi->chipshift);
	chipstart = cfi->chips[chipnum].start;

	 
	if (ofs & (map_bankwidth(map) - 1)) {
		unsigned long bus_ofs = ofs & ~(map_bankwidth(map) - 1);
		int i = ofs - bus_ofs;
		int n = 0;
		map_word tmp_buf;

		ret = cfi_amdstd_panic_wait(map, &cfi->chips[chipnum], bus_ofs);
		if (ret)
			return ret;

		 
		tmp_buf = map_read(map, bus_ofs + chipstart);

		 
		n = min_t(int, len, map_bankwidth(map) - i);

		tmp_buf = map_word_load_partial(map, tmp_buf, buf, i, n);

		ret = do_panic_write_oneword(map, &cfi->chips[chipnum],
					     bus_ofs, tmp_buf);
		if (ret)
			return ret;

		ofs += n;
		buf += n;
		(*retlen) += n;
		len -= n;

		if (ofs >> cfi->chipshift) {
			chipnum++;
			ofs = 0;
			if (chipnum == cfi->numchips)
				return 0;
		}
	}

	 
	while (len >= map_bankwidth(map)) {
		map_word datum;

		datum = map_word_load(map, buf);

		ret = do_panic_write_oneword(map, &cfi->chips[chipnum],
					     ofs, datum);
		if (ret)
			return ret;

		ofs += map_bankwidth(map);
		buf += map_bankwidth(map);
		(*retlen) += map_bankwidth(map);
		len -= map_bankwidth(map);

		if (ofs >> cfi->chipshift) {
			chipnum++;
			ofs = 0;
			if (chipnum == cfi->numchips)
				return 0;

			chipstart = cfi->chips[chipnum].start;
		}
	}

	 
	if (len & (map_bankwidth(map) - 1)) {
		map_word tmp_buf;

		ret = cfi_amdstd_panic_wait(map, &cfi->chips[chipnum], ofs);
		if (ret)
			return ret;

		tmp_buf = map_read(map, ofs + chipstart);

		tmp_buf = map_word_load_partial(map, tmp_buf, buf, 0, len);

		ret = do_panic_write_oneword(map, &cfi->chips[chipnum],
					     ofs, tmp_buf);
		if (ret)
			return ret;

		(*retlen) += len;
	}

	return 0;
}


 
static int __xipram do_erase_chip(struct map_info *map, struct flchip *chip)
{
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long timeo = jiffies + HZ;
	unsigned long int adr;
	DECLARE_WAITQUEUE(wait, current);
	int ret;
	int retry_cnt = 0;
	map_word datum = map_word_ff(map);

	adr = cfi->addr_unlock1;

	mutex_lock(&chip->mutex);
	ret = get_chip(map, chip, adr, FL_ERASING);
	if (ret) {
		mutex_unlock(&chip->mutex);
		return ret;
	}

	pr_debug("MTD %s(): ERASE 0x%.8lx\n",
	       __func__, chip->start);

	XIP_INVAL_CACHED_RANGE(map, adr, map->size);
	ENABLE_VPP(map);
	xip_disable(map, chip, adr);

 retry:
	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x80, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x10, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);

	chip->state = FL_ERASING;
	chip->erase_suspended = 0;
	chip->in_progress_block_addr = adr;
	chip->in_progress_block_mask = ~(map->size - 1);

	INVALIDATE_CACHE_UDELAY(map, chip,
				adr, map->size,
				chip->erase_time*500);

	timeo = jiffies + (HZ*20);

	for (;;) {
		if (chip->state != FL_ERASING) {
			 
			set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&chip->wq, &wait);
			mutex_unlock(&chip->mutex);
			schedule();
			remove_wait_queue(&chip->wq, &wait);
			mutex_lock(&chip->mutex);
			continue;
		}
		if (chip->erase_suspended) {
			 
			timeo = jiffies + (HZ*20);  
			chip->erase_suspended = 0;
		}

		if (chip_ready(map, chip, adr, &datum)) {
			if (cfi_check_err_status(map, chip, adr))
				ret = -EIO;
			break;
		}

		if (time_after(jiffies, timeo)) {
			printk(KERN_WARNING "MTD %s(): software timeout\n",
			       __func__);
			ret = -EIO;
			break;
		}

		 
		UDELAY(map, chip, adr, 1000000/HZ);
	}
	 
	if (ret) {
		 
		map_write(map, CMD(0xF0), chip->start);
		 

		if (++retry_cnt <= MAX_RETRIES) {
			ret = 0;
			goto retry;
		}
	}

	chip->state = FL_READY;
	xip_enable(map, chip, adr);
	DISABLE_VPP(map);
	put_chip(map, chip, adr);
	mutex_unlock(&chip->mutex);

	return ret;
}


static int __xipram do_erase_oneblock(struct map_info *map, struct flchip *chip, unsigned long adr, int len, void *thunk)
{
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long timeo = jiffies + HZ;
	DECLARE_WAITQUEUE(wait, current);
	int ret;
	int retry_cnt = 0;
	map_word datum = map_word_ff(map);

	adr += chip->start;

	mutex_lock(&chip->mutex);
	ret = get_chip(map, chip, adr, FL_ERASING);
	if (ret) {
		mutex_unlock(&chip->mutex);
		return ret;
	}

	pr_debug("MTD %s(): ERASE 0x%.8lx\n",
		 __func__, adr);

	XIP_INVAL_CACHED_RANGE(map, adr, len);
	ENABLE_VPP(map);
	xip_disable(map, chip, adr);

 retry:
	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x80, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, cfi->device_type, NULL);
	map_write(map, cfi->sector_erase_cmd, adr);

	chip->state = FL_ERASING;
	chip->erase_suspended = 0;
	chip->in_progress_block_addr = adr;
	chip->in_progress_block_mask = ~(len - 1);

	INVALIDATE_CACHE_UDELAY(map, chip,
				adr, len,
				chip->erase_time*500);

	timeo = jiffies + (HZ*20);

	for (;;) {
		if (chip->state != FL_ERASING) {
			 
			set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&chip->wq, &wait);
			mutex_unlock(&chip->mutex);
			schedule();
			remove_wait_queue(&chip->wq, &wait);
			mutex_lock(&chip->mutex);
			continue;
		}
		if (chip->erase_suspended) {
			 
			timeo = jiffies + (HZ*20);  
			chip->erase_suspended = 0;
		}

		if (chip_ready(map, chip, adr, &datum)) {
			if (cfi_check_err_status(map, chip, adr))
				ret = -EIO;
			break;
		}

		if (time_after(jiffies, timeo)) {
			printk(KERN_WARNING "MTD %s(): software timeout\n",
			       __func__);
			ret = -EIO;
			break;
		}

		 
		UDELAY(map, chip, adr, 1000000/HZ);
	}
	 
	if (ret) {
		 
		map_write(map, CMD(0xF0), chip->start);
		 

		if (++retry_cnt <= MAX_RETRIES) {
			ret = 0;
			goto retry;
		}
	}

	chip->state = FL_READY;
	xip_enable(map, chip, adr);
	DISABLE_VPP(map);
	put_chip(map, chip, adr);
	mutex_unlock(&chip->mutex);
	return ret;
}


static int cfi_amdstd_erase_varsize(struct mtd_info *mtd, struct erase_info *instr)
{
	return cfi_varsize_frob(mtd, do_erase_oneblock, instr->addr,
				instr->len, NULL);
}


static int cfi_amdstd_erase_chip(struct mtd_info *mtd, struct erase_info *instr)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;

	if (instr->addr != 0)
		return -EINVAL;

	if (instr->len != mtd->size)
		return -EINVAL;

	return do_erase_chip(map, &cfi->chips[0]);
}

static int do_atmel_lock(struct map_info *map, struct flchip *chip,
			 unsigned long adr, int len, void *thunk)
{
	struct cfi_private *cfi = map->fldrv_priv;
	int ret;

	mutex_lock(&chip->mutex);
	ret = get_chip(map, chip, adr + chip->start, FL_LOCKING);
	if (ret)
		goto out_unlock;
	chip->state = FL_LOCKING;

	pr_debug("MTD %s(): LOCK 0x%08lx len %d\n", __func__, adr, len);

	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi,
			 cfi->device_type, NULL);
	cfi_send_gen_cmd(0x80, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);
	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi,
			 cfi->device_type, NULL);
	map_write(map, CMD(0x40), chip->start + adr);

	chip->state = FL_READY;
	put_chip(map, chip, adr + chip->start);
	ret = 0;

out_unlock:
	mutex_unlock(&chip->mutex);
	return ret;
}

static int do_atmel_unlock(struct map_info *map, struct flchip *chip,
			   unsigned long adr, int len, void *thunk)
{
	struct cfi_private *cfi = map->fldrv_priv;
	int ret;

	mutex_lock(&chip->mutex);
	ret = get_chip(map, chip, adr + chip->start, FL_UNLOCKING);
	if (ret)
		goto out_unlock;
	chip->state = FL_UNLOCKING;

	pr_debug("MTD %s(): LOCK 0x%08lx len %d\n", __func__, adr, len);

	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);
	map_write(map, CMD(0x70), adr);

	chip->state = FL_READY;
	put_chip(map, chip, adr + chip->start);
	ret = 0;

out_unlock:
	mutex_unlock(&chip->mutex);
	return ret;
}

static int cfi_atmel_lock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	return cfi_varsize_frob(mtd, do_atmel_lock, ofs, len, NULL);
}

static int cfi_atmel_unlock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	return cfi_varsize_frob(mtd, do_atmel_unlock, ofs, len, NULL);
}

 

struct ppb_lock {
	struct flchip *chip;
	unsigned long adr;
	int locked;
};

#define DO_XXLOCK_ONEBLOCK_LOCK		((void *)1)
#define DO_XXLOCK_ONEBLOCK_UNLOCK	((void *)2)
#define DO_XXLOCK_ONEBLOCK_GETLOCK	((void *)3)

static int __maybe_unused do_ppb_xxlock(struct map_info *map,
					struct flchip *chip,
					unsigned long adr, int len, void *thunk)
{
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long timeo;
	int ret;

	adr += chip->start;
	mutex_lock(&chip->mutex);
	ret = get_chip(map, chip, adr, FL_LOCKING);
	if (ret) {
		mutex_unlock(&chip->mutex);
		return ret;
	}

	pr_debug("MTD %s(): XXLOCK 0x%08lx len %d\n", __func__, adr, len);

	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi,
			 cfi->device_type, NULL);
	 
	cfi_send_gen_cmd(0xC0, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);

	if (thunk == DO_XXLOCK_ONEBLOCK_LOCK) {
		chip->state = FL_LOCKING;
		map_write(map, CMD(0xA0), adr);
		map_write(map, CMD(0x00), adr);
	} else if (thunk == DO_XXLOCK_ONEBLOCK_UNLOCK) {
		 
		chip->state = FL_UNLOCKING;
		map_write(map, CMD(0x80), chip->start);
		map_write(map, CMD(0x30), chip->start);
	} else if (thunk == DO_XXLOCK_ONEBLOCK_GETLOCK) {
		chip->state = FL_JEDEC_QUERY;
		 
		ret = !cfi_read_query(map, adr);
	} else
		BUG();

	 
	timeo = jiffies + msecs_to_jiffies(2000);	 
	for (;;) {
		if (chip_ready(map, chip, adr, NULL))
			break;

		if (time_after(jiffies, timeo)) {
			printk(KERN_ERR "Waiting for chip to be ready timed out.\n");
			ret = -EIO;
			break;
		}

		UDELAY(map, chip, adr, 1);
	}

	 
	map_write(map, CMD(0x90), chip->start);
	map_write(map, CMD(0x00), chip->start);

	chip->state = FL_READY;
	put_chip(map, chip, adr);
	mutex_unlock(&chip->mutex);

	return ret;
}

static int __maybe_unused cfi_ppb_lock(struct mtd_info *mtd, loff_t ofs,
				       uint64_t len)
{
	return cfi_varsize_frob(mtd, do_ppb_xxlock, ofs, len,
				DO_XXLOCK_ONEBLOCK_LOCK);
}

static int __maybe_unused cfi_ppb_unlock(struct mtd_info *mtd, loff_t ofs,
					 uint64_t len)
{
	struct mtd_erase_region_info *regions = mtd->eraseregions;
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	struct ppb_lock *sect;
	unsigned long adr;
	loff_t offset;
	uint64_t length;
	int chipnum;
	int i;
	int sectors;
	int ret;
	int max_sectors;

	 
	max_sectors = 0;
	for (i = 0; i < mtd->numeraseregions; i++)
		max_sectors += regions[i].numblocks;

	sect = kcalloc(max_sectors, sizeof(struct ppb_lock), GFP_KERNEL);
	if (!sect)
		return -ENOMEM;

	 
	i = 0;
	chipnum = 0;
	adr = 0;
	sectors = 0;
	offset = 0;
	length = mtd->size;

	while (length) {
		int size = regions[i].erasesize;

		 
		if ((offset < ofs) || (offset >= (ofs + len))) {
			sect[sectors].chip = &cfi->chips[chipnum];
			sect[sectors].adr = adr;
			sect[sectors].locked = do_ppb_xxlock(
				map, &cfi->chips[chipnum], adr, 0,
				DO_XXLOCK_ONEBLOCK_GETLOCK);
		}

		adr += size;
		offset += size;
		length -= size;

		if (offset == regions[i].offset + size * regions[i].numblocks)
			i++;

		if (adr >> cfi->chipshift) {
			if (offset >= (ofs + len))
				break;
			adr = 0;
			chipnum++;

			if (chipnum >= cfi->numchips)
				break;
		}

		sectors++;
		if (sectors >= max_sectors) {
			printk(KERN_ERR "Only %d sectors for PPB locking supported!\n",
			       max_sectors);
			kfree(sect);
			return -EINVAL;
		}
	}

	 
	ret = cfi_varsize_frob(mtd, do_ppb_xxlock, ofs, len,
			       DO_XXLOCK_ONEBLOCK_UNLOCK);
	if (ret) {
		kfree(sect);
		return ret;
	}

	 
	for (i = 0; i < sectors; i++) {
		if (sect[i].locked)
			do_ppb_xxlock(map, sect[i].chip, sect[i].adr, 0,
				      DO_XXLOCK_ONEBLOCK_LOCK);
	}

	kfree(sect);
	return ret;
}

static int __maybe_unused cfi_ppb_is_locked(struct mtd_info *mtd, loff_t ofs,
					    uint64_t len)
{
	return cfi_varsize_frob(mtd, do_ppb_xxlock, ofs, len,
				DO_XXLOCK_ONEBLOCK_GETLOCK) ? 1 : 0;
}

static void cfi_amdstd_sync (struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int i;
	struct flchip *chip;
	int ret = 0;
	DECLARE_WAITQUEUE(wait, current);

	for (i=0; !ret && i<cfi->numchips; i++) {
		chip = &cfi->chips[i];

	retry:
		mutex_lock(&chip->mutex);

		switch(chip->state) {
		case FL_READY:
		case FL_STATUS:
		case FL_CFI_QUERY:
		case FL_JEDEC_QUERY:
			chip->oldstate = chip->state;
			chip->state = FL_SYNCING;
			 
			fallthrough;
		case FL_SYNCING:
			mutex_unlock(&chip->mutex);
			break;

		default:
			 
			set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&chip->wq, &wait);

			mutex_unlock(&chip->mutex);

			schedule();

			remove_wait_queue(&chip->wq, &wait);

			goto retry;
		}
	}

	 

	for (i--; i >=0; i--) {
		chip = &cfi->chips[i];

		mutex_lock(&chip->mutex);

		if (chip->state == FL_SYNCING) {
			chip->state = chip->oldstate;
			wake_up(&chip->wq);
		}
		mutex_unlock(&chip->mutex);
	}
}


static int cfi_amdstd_suspend(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int i;
	struct flchip *chip;
	int ret = 0;

	for (i=0; !ret && i<cfi->numchips; i++) {
		chip = &cfi->chips[i];

		mutex_lock(&chip->mutex);

		switch(chip->state) {
		case FL_READY:
		case FL_STATUS:
		case FL_CFI_QUERY:
		case FL_JEDEC_QUERY:
			chip->oldstate = chip->state;
			chip->state = FL_PM_SUSPENDED;
			 
			break;
		case FL_PM_SUSPENDED:
			break;

		default:
			ret = -EAGAIN;
			break;
		}
		mutex_unlock(&chip->mutex);
	}

	 

	if (ret) {
		for (i--; i >=0; i--) {
			chip = &cfi->chips[i];

			mutex_lock(&chip->mutex);

			if (chip->state == FL_PM_SUSPENDED) {
				chip->state = chip->oldstate;
				wake_up(&chip->wq);
			}
			mutex_unlock(&chip->mutex);
		}
	}

	return ret;
}


static void cfi_amdstd_resume(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int i;
	struct flchip *chip;

	for (i=0; i<cfi->numchips; i++) {

		chip = &cfi->chips[i];

		mutex_lock(&chip->mutex);

		if (chip->state == FL_PM_SUSPENDED) {
			chip->state = FL_READY;
			map_write(map, CMD(0xF0), chip->start);
			wake_up(&chip->wq);
		}
		else
			printk(KERN_ERR "Argh. Chip not in PM_SUSPENDED state upon resume()\n");

		mutex_unlock(&chip->mutex);
	}
}


 
static int cfi_amdstd_reset(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int i, ret;
	struct flchip *chip;

	for (i = 0; i < cfi->numchips; i++) {

		chip = &cfi->chips[i];

		mutex_lock(&chip->mutex);

		ret = get_chip(map, chip, chip->start, FL_SHUTDOWN);
		if (!ret) {
			map_write(map, CMD(0xF0), chip->start);
			chip->state = FL_SHUTDOWN;
			put_chip(map, chip, chip->start);
		}

		mutex_unlock(&chip->mutex);
	}

	return 0;
}


static int cfi_amdstd_reboot(struct notifier_block *nb, unsigned long val,
			       void *v)
{
	struct mtd_info *mtd;

	mtd = container_of(nb, struct mtd_info, reboot_notifier);
	cfi_amdstd_reset(mtd);
	return NOTIFY_DONE;
}


static void cfi_amdstd_destroy(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;

	cfi_amdstd_reset(mtd);
	unregister_reboot_notifier(&mtd->reboot_notifier);
	kfree(cfi->cmdset_priv);
	kfree(cfi->cfiq);
	kfree(cfi);
	kfree(mtd->eraseregions);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Crossnet Co. <info@crossnet.co.jp> et al.");
MODULE_DESCRIPTION("MTD chip driver for AMD/Fujitsu flash chips");
MODULE_ALIAS("cfi_cmdset_0006");
MODULE_ALIAS("cfi_cmdset_0701");
