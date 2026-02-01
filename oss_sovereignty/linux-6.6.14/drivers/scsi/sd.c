
 

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/bio.h>
#include <linux/hdreg.h>
#include <linux/errno.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/blk-pm.h>
#include <linux/delay.h>
#include <linux/major.h>
#include <linux/mutex.h>
#include <linux/string_helpers.h>
#include <linux/slab.h>
#include <linux/sed-opal.h>
#include <linux/pm_runtime.h>
#include <linux/pr.h>
#include <linux/t10-pi.h>
#include <linux/uaccess.h>
#include <asm/unaligned.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_driver.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_ioctl.h>
#include <scsi/scsicam.h>
#include <scsi/scsi_common.h>

#include "sd.h"
#include "scsi_priv.h"
#include "scsi_logging.h"

MODULE_AUTHOR("Eric Youngdale");
MODULE_DESCRIPTION("SCSI disk (sd) driver");
MODULE_LICENSE("GPL");

MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK0_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK1_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK2_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK3_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK4_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK5_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK6_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK7_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK8_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK9_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK10_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK11_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK12_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK13_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK14_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK15_MAJOR);
MODULE_ALIAS_SCSI_DEVICE(TYPE_DISK);
MODULE_ALIAS_SCSI_DEVICE(TYPE_MOD);
MODULE_ALIAS_SCSI_DEVICE(TYPE_RBC);
MODULE_ALIAS_SCSI_DEVICE(TYPE_ZBC);

#define SD_MINORS	16

static void sd_config_discard(struct scsi_disk *, unsigned int);
static void sd_config_write_same(struct scsi_disk *);
static int  sd_revalidate_disk(struct gendisk *);
static void sd_unlock_native_capacity(struct gendisk *disk);
static void sd_shutdown(struct device *);
static void sd_read_capacity(struct scsi_disk *sdkp, unsigned char *buffer);
static void scsi_disk_release(struct device *cdev);

static DEFINE_IDA(sd_index_ida);

static mempool_t *sd_page_pool;
static struct lock_class_key sd_bio_compl_lkclass;

static const char *sd_cache_types[] = {
	"write through", "none", "write back",
	"write back, no read (daft)"
};

static void sd_set_flush_flag(struct scsi_disk *sdkp)
{
	bool wc = false, fua = false;

	if (sdkp->WCE) {
		wc = true;
		if (sdkp->DPOFUA)
			fua = true;
	}

	blk_queue_write_cache(sdkp->disk->queue, wc, fua);
}

static ssize_t
cache_type_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	int ct, rcd, wce, sp;
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdp = sdkp->device;
	char buffer[64];
	char *buffer_data;
	struct scsi_mode_data data;
	struct scsi_sense_hdr sshdr;
	static const char temp[] = "temporary ";
	int len;

	if (sdp->type != TYPE_DISK && sdp->type != TYPE_ZBC)
		 
		return -EINVAL;

	if (strncmp(buf, temp, sizeof(temp) - 1) == 0) {
		buf += sizeof(temp) - 1;
		sdkp->cache_override = 1;
	} else {
		sdkp->cache_override = 0;
	}

	ct = sysfs_match_string(sd_cache_types, buf);
	if (ct < 0)
		return -EINVAL;

	rcd = ct & 0x01 ? 1 : 0;
	wce = (ct & 0x02) && !sdkp->write_prot ? 1 : 0;

	if (sdkp->cache_override) {
		sdkp->WCE = wce;
		sdkp->RCD = rcd;
		sd_set_flush_flag(sdkp);
		return count;
	}

	if (scsi_mode_sense(sdp, 0x08, 8, 0, buffer, sizeof(buffer), SD_TIMEOUT,
			    sdkp->max_retries, &data, NULL))
		return -EINVAL;
	len = min_t(size_t, sizeof(buffer), data.length - data.header_length -
		  data.block_descriptor_length);
	buffer_data = buffer + data.header_length +
		data.block_descriptor_length;
	buffer_data[2] &= ~0x05;
	buffer_data[2] |= wce << 2 | rcd;
	sp = buffer_data[0] & 0x80 ? 1 : 0;
	buffer_data[0] &= ~0x80;

	 
	data.device_specific = 0;

	if (scsi_mode_select(sdp, 1, sp, buffer_data, len, SD_TIMEOUT,
			     sdkp->max_retries, &data, &sshdr)) {
		if (scsi_sense_valid(&sshdr))
			sd_print_sense_hdr(sdkp, &sshdr);
		return -EINVAL;
	}
	sd_revalidate_disk(sdkp->disk);
	return count;
}

static ssize_t
manage_start_stop_show(struct device *dev,
		       struct device_attribute *attr, char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdp = sdkp->device;

	return sysfs_emit(buf, "%u\n",
			  sdp->manage_system_start_stop &&
			  sdp->manage_runtime_start_stop &&
			  sdp->manage_shutdown);
}
static DEVICE_ATTR_RO(manage_start_stop);

static ssize_t
manage_system_start_stop_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdp = sdkp->device;

	return sysfs_emit(buf, "%u\n", sdp->manage_system_start_stop);
}

static ssize_t
manage_system_start_stop_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdp = sdkp->device;
	bool v;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (kstrtobool(buf, &v))
		return -EINVAL;

	sdp->manage_system_start_stop = v;

	return count;
}
static DEVICE_ATTR_RW(manage_system_start_stop);

static ssize_t
manage_runtime_start_stop_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdp = sdkp->device;

	return sysfs_emit(buf, "%u\n", sdp->manage_runtime_start_stop);
}

static ssize_t
manage_runtime_start_stop_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdp = sdkp->device;
	bool v;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (kstrtobool(buf, &v))
		return -EINVAL;

	sdp->manage_runtime_start_stop = v;

	return count;
}
static DEVICE_ATTR_RW(manage_runtime_start_stop);

static ssize_t manage_shutdown_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdp = sdkp->device;

	return sysfs_emit(buf, "%u\n", sdp->manage_shutdown);
}

static ssize_t manage_shutdown_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdp = sdkp->device;
	bool v;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (kstrtobool(buf, &v))
		return -EINVAL;

	sdp->manage_shutdown = v;

	return count;
}
static DEVICE_ATTR_RW(manage_shutdown);

static ssize_t
allow_restart_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	return sprintf(buf, "%u\n", sdkp->device->allow_restart);
}

static ssize_t
allow_restart_store(struct device *dev, struct device_attribute *attr,
		    const char *buf, size_t count)
{
	bool v;
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdp = sdkp->device;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (sdp->type != TYPE_DISK && sdp->type != TYPE_ZBC)
		return -EINVAL;

	if (kstrtobool(buf, &v))
		return -EINVAL;

	sdp->allow_restart = v;

	return count;
}
static DEVICE_ATTR_RW(allow_restart);

static ssize_t
cache_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	int ct = sdkp->RCD + 2*sdkp->WCE;

	return sprintf(buf, "%s\n", sd_cache_types[ct]);
}
static DEVICE_ATTR_RW(cache_type);

static ssize_t
FUA_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	return sprintf(buf, "%u\n", sdkp->DPOFUA);
}
static DEVICE_ATTR_RO(FUA);

static ssize_t
protection_type_show(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	return sprintf(buf, "%u\n", sdkp->protection_type);
}

static ssize_t
protection_type_store(struct device *dev, struct device_attribute *attr,
		      const char *buf, size_t count)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	unsigned int val;
	int err;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	err = kstrtouint(buf, 10, &val);

	if (err)
		return err;

	if (val <= T10_PI_TYPE3_PROTECTION)
		sdkp->protection_type = val;

	return count;
}
static DEVICE_ATTR_RW(protection_type);

static ssize_t
protection_mode_show(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdp = sdkp->device;
	unsigned int dif, dix;

	dif = scsi_host_dif_capable(sdp->host, sdkp->protection_type);
	dix = scsi_host_dix_capable(sdp->host, sdkp->protection_type);

	if (!dix && scsi_host_dix_capable(sdp->host, T10_PI_TYPE0_PROTECTION)) {
		dif = 0;
		dix = 1;
	}

	if (!dif && !dix)
		return sprintf(buf, "none\n");

	return sprintf(buf, "%s%u\n", dix ? "dix" : "dif", dif);
}
static DEVICE_ATTR_RO(protection_mode);

static ssize_t
app_tag_own_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	return sprintf(buf, "%u\n", sdkp->ATO);
}
static DEVICE_ATTR_RO(app_tag_own);

static ssize_t
thin_provisioning_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	return sprintf(buf, "%u\n", sdkp->lbpme);
}
static DEVICE_ATTR_RO(thin_provisioning);

 
static const char *lbp_mode[] = {
	[SD_LBP_FULL]		= "full",
	[SD_LBP_UNMAP]		= "unmap",
	[SD_LBP_WS16]		= "writesame_16",
	[SD_LBP_WS10]		= "writesame_10",
	[SD_LBP_ZERO]		= "writesame_zero",
	[SD_LBP_DISABLE]	= "disabled",
};

static ssize_t
provisioning_mode_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	return sprintf(buf, "%s\n", lbp_mode[sdkp->provisioning_mode]);
}

static ssize_t
provisioning_mode_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdp = sdkp->device;
	int mode;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (sd_is_zoned(sdkp)) {
		sd_config_discard(sdkp, SD_LBP_DISABLE);
		return count;
	}

	if (sdp->type != TYPE_DISK)
		return -EINVAL;

	mode = sysfs_match_string(lbp_mode, buf);
	if (mode < 0)
		return -EINVAL;

	sd_config_discard(sdkp, mode);

	return count;
}
static DEVICE_ATTR_RW(provisioning_mode);

 
static const char *zeroing_mode[] = {
	[SD_ZERO_WRITE]		= "write",
	[SD_ZERO_WS]		= "writesame",
	[SD_ZERO_WS16_UNMAP]	= "writesame_16_unmap",
	[SD_ZERO_WS10_UNMAP]	= "writesame_10_unmap",
};

static ssize_t
zeroing_mode_show(struct device *dev, struct device_attribute *attr,
		  char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	return sprintf(buf, "%s\n", zeroing_mode[sdkp->zeroing_mode]);
}

static ssize_t
zeroing_mode_store(struct device *dev, struct device_attribute *attr,
		   const char *buf, size_t count)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	int mode;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	mode = sysfs_match_string(zeroing_mode, buf);
	if (mode < 0)
		return -EINVAL;

	sdkp->zeroing_mode = mode;

	return count;
}
static DEVICE_ATTR_RW(zeroing_mode);

static ssize_t
max_medium_access_timeouts_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	return sprintf(buf, "%u\n", sdkp->max_medium_access_timeouts);
}

static ssize_t
max_medium_access_timeouts_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	int err;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	err = kstrtouint(buf, 10, &sdkp->max_medium_access_timeouts);

	return err ? err : count;
}
static DEVICE_ATTR_RW(max_medium_access_timeouts);

static ssize_t
max_write_same_blocks_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	return sprintf(buf, "%u\n", sdkp->max_ws_blocks);
}

static ssize_t
max_write_same_blocks_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdp = sdkp->device;
	unsigned long max;
	int err;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (sdp->type != TYPE_DISK && sdp->type != TYPE_ZBC)
		return -EINVAL;

	err = kstrtoul(buf, 10, &max);

	if (err)
		return err;

	if (max == 0)
		sdp->no_write_same = 1;
	else if (max <= SD_MAX_WS16_BLOCKS) {
		sdp->no_write_same = 0;
		sdkp->max_ws_blocks = max;
	}

	sd_config_write_same(sdkp);

	return count;
}
static DEVICE_ATTR_RW(max_write_same_blocks);

static ssize_t
zoned_cap_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	if (sdkp->device->type == TYPE_ZBC)
		return sprintf(buf, "host-managed\n");
	if (sdkp->zoned == 1)
		return sprintf(buf, "host-aware\n");
	if (sdkp->zoned == 2)
		return sprintf(buf, "drive-managed\n");
	return sprintf(buf, "none\n");
}
static DEVICE_ATTR_RO(zoned_cap);

static ssize_t
max_retries_store(struct device *dev, struct device_attribute *attr,
		  const char *buf, size_t count)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdev = sdkp->device;
	int retries, err;

	err = kstrtoint(buf, 10, &retries);
	if (err)
		return err;

	if (retries == SCSI_CMD_RETRIES_NO_LIMIT || retries <= SD_MAX_RETRIES) {
		sdkp->max_retries = retries;
		return count;
	}

	sdev_printk(KERN_ERR, sdev, "max_retries must be between -1 and %d\n",
		    SD_MAX_RETRIES);
	return -EINVAL;
}

static ssize_t
max_retries_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	return sprintf(buf, "%d\n", sdkp->max_retries);
}

static DEVICE_ATTR_RW(max_retries);

static struct attribute *sd_disk_attrs[] = {
	&dev_attr_cache_type.attr,
	&dev_attr_FUA.attr,
	&dev_attr_allow_restart.attr,
	&dev_attr_manage_start_stop.attr,
	&dev_attr_manage_system_start_stop.attr,
	&dev_attr_manage_runtime_start_stop.attr,
	&dev_attr_manage_shutdown.attr,
	&dev_attr_protection_type.attr,
	&dev_attr_protection_mode.attr,
	&dev_attr_app_tag_own.attr,
	&dev_attr_thin_provisioning.attr,
	&dev_attr_provisioning_mode.attr,
	&dev_attr_zeroing_mode.attr,
	&dev_attr_max_write_same_blocks.attr,
	&dev_attr_max_medium_access_timeouts.attr,
	&dev_attr_zoned_cap.attr,
	&dev_attr_max_retries.attr,
	NULL,
};
ATTRIBUTE_GROUPS(sd_disk);

static struct class sd_disk_class = {
	.name		= "scsi_disk",
	.dev_release	= scsi_disk_release,
	.dev_groups	= sd_disk_groups,
};

 
static void sd_default_probe(dev_t devt)
{
}

 
static int sd_major(int major_idx)
{
	switch (major_idx) {
	case 0:
		return SCSI_DISK0_MAJOR;
	case 1 ... 7:
		return SCSI_DISK1_MAJOR + major_idx - 1;
	case 8 ... 15:
		return SCSI_DISK8_MAJOR + major_idx - 8;
	default:
		BUG();
		return 0;	 
	}
}

#ifdef CONFIG_BLK_SED_OPAL
static int sd_sec_submit(void *data, u16 spsp, u8 secp, void *buffer,
		size_t len, bool send)
{
	struct scsi_disk *sdkp = data;
	struct scsi_device *sdev = sdkp->device;
	u8 cdb[12] = { 0, };
	const struct scsi_exec_args exec_args = {
		.req_flags = BLK_MQ_REQ_PM,
	};
	int ret;

	cdb[0] = send ? SECURITY_PROTOCOL_OUT : SECURITY_PROTOCOL_IN;
	cdb[1] = secp;
	put_unaligned_be16(spsp, &cdb[2]);
	put_unaligned_be32(len, &cdb[6]);

	ret = scsi_execute_cmd(sdev, cdb, send ? REQ_OP_DRV_OUT : REQ_OP_DRV_IN,
			       buffer, len, SD_TIMEOUT, sdkp->max_retries,
			       &exec_args);
	return ret <= 0 ? ret : -EIO;
}
#endif  

 
static unsigned int sd_prot_op(bool write, bool dix, bool dif)
{
	 
	static const unsigned int ops[] = {	 
		SCSI_PROT_NORMAL,		 
		SCSI_PROT_READ_STRIP,		 
		SCSI_PROT_READ_INSERT,		 
		SCSI_PROT_READ_PASS,		 
		SCSI_PROT_NORMAL,		 
		SCSI_PROT_WRITE_INSERT,		 
		SCSI_PROT_WRITE_STRIP,		 
		SCSI_PROT_WRITE_PASS,		 
	};

	return ops[write << 2 | dix << 1 | dif];
}

 
static unsigned int sd_prot_flag_mask(unsigned int prot_op)
{
	static const unsigned int flag_mask[] = {
		[SCSI_PROT_NORMAL]		= 0,

		[SCSI_PROT_READ_STRIP]		= SCSI_PROT_TRANSFER_PI |
						  SCSI_PROT_GUARD_CHECK |
						  SCSI_PROT_REF_CHECK |
						  SCSI_PROT_REF_INCREMENT,

		[SCSI_PROT_READ_INSERT]		= SCSI_PROT_REF_INCREMENT |
						  SCSI_PROT_IP_CHECKSUM,

		[SCSI_PROT_READ_PASS]		= SCSI_PROT_TRANSFER_PI |
						  SCSI_PROT_GUARD_CHECK |
						  SCSI_PROT_REF_CHECK |
						  SCSI_PROT_REF_INCREMENT |
						  SCSI_PROT_IP_CHECKSUM,

		[SCSI_PROT_WRITE_INSERT]	= SCSI_PROT_TRANSFER_PI |
						  SCSI_PROT_REF_INCREMENT,

		[SCSI_PROT_WRITE_STRIP]		= SCSI_PROT_GUARD_CHECK |
						  SCSI_PROT_REF_CHECK |
						  SCSI_PROT_REF_INCREMENT |
						  SCSI_PROT_IP_CHECKSUM,

		[SCSI_PROT_WRITE_PASS]		= SCSI_PROT_TRANSFER_PI |
						  SCSI_PROT_GUARD_CHECK |
						  SCSI_PROT_REF_CHECK |
						  SCSI_PROT_REF_INCREMENT |
						  SCSI_PROT_IP_CHECKSUM,
	};

	return flag_mask[prot_op];
}

static unsigned char sd_setup_protect_cmnd(struct scsi_cmnd *scmd,
					   unsigned int dix, unsigned int dif)
{
	struct request *rq = scsi_cmd_to_rq(scmd);
	struct bio *bio = rq->bio;
	unsigned int prot_op = sd_prot_op(rq_data_dir(rq), dix, dif);
	unsigned int protect = 0;

	if (dix) {				 
		if (bio_integrity_flagged(bio, BIP_IP_CHECKSUM))
			scmd->prot_flags |= SCSI_PROT_IP_CHECKSUM;

		if (bio_integrity_flagged(bio, BIP_CTRL_NOCHECK) == false)
			scmd->prot_flags |= SCSI_PROT_GUARD_CHECK;
	}

	if (dif != T10_PI_TYPE3_PROTECTION) {	 
		scmd->prot_flags |= SCSI_PROT_REF_INCREMENT;

		if (bio_integrity_flagged(bio, BIP_CTRL_NOCHECK) == false)
			scmd->prot_flags |= SCSI_PROT_REF_CHECK;
	}

	if (dif) {				 
		scmd->prot_flags |= SCSI_PROT_TRANSFER_PI;

		if (bio_integrity_flagged(bio, BIP_DISK_NOCHECK))
			protect = 3 << 5;	 
		else
			protect = 1 << 5;	 
	}

	scsi_set_prot_op(scmd, prot_op);
	scsi_set_prot_type(scmd, dif);
	scmd->prot_flags &= sd_prot_flag_mask(prot_op);

	return protect;
}

static void sd_config_discard(struct scsi_disk *sdkp, unsigned int mode)
{
	struct request_queue *q = sdkp->disk->queue;
	unsigned int logical_block_size = sdkp->device->sector_size;
	unsigned int max_blocks = 0;

	q->limits.discard_alignment =
		sdkp->unmap_alignment * logical_block_size;
	q->limits.discard_granularity =
		max(sdkp->physical_block_size,
		    sdkp->unmap_granularity * logical_block_size);
	sdkp->provisioning_mode = mode;

	switch (mode) {

	case SD_LBP_FULL:
	case SD_LBP_DISABLE:
		blk_queue_max_discard_sectors(q, 0);
		return;

	case SD_LBP_UNMAP:
		max_blocks = min_not_zero(sdkp->max_unmap_blocks,
					  (u32)SD_MAX_WS16_BLOCKS);
		break;

	case SD_LBP_WS16:
		if (sdkp->device->unmap_limit_for_ws)
			max_blocks = sdkp->max_unmap_blocks;
		else
			max_blocks = sdkp->max_ws_blocks;

		max_blocks = min_not_zero(max_blocks, (u32)SD_MAX_WS16_BLOCKS);
		break;

	case SD_LBP_WS10:
		if (sdkp->device->unmap_limit_for_ws)
			max_blocks = sdkp->max_unmap_blocks;
		else
			max_blocks = sdkp->max_ws_blocks;

		max_blocks = min_not_zero(max_blocks, (u32)SD_MAX_WS10_BLOCKS);
		break;

	case SD_LBP_ZERO:
		max_blocks = min_not_zero(sdkp->max_ws_blocks,
					  (u32)SD_MAX_WS10_BLOCKS);
		break;
	}

	blk_queue_max_discard_sectors(q, max_blocks * (logical_block_size >> 9));
}

static void *sd_set_special_bvec(struct request *rq, unsigned int data_len)
{
	struct page *page;

	page = mempool_alloc(sd_page_pool, GFP_ATOMIC);
	if (!page)
		return NULL;
	clear_highpage(page);
	bvec_set_page(&rq->special_vec, page, data_len, 0);
	rq->rq_flags |= RQF_SPECIAL_PAYLOAD;
	return bvec_virt(&rq->special_vec);
}

static blk_status_t sd_setup_unmap_cmnd(struct scsi_cmnd *cmd)
{
	struct scsi_device *sdp = cmd->device;
	struct request *rq = scsi_cmd_to_rq(cmd);
	struct scsi_disk *sdkp = scsi_disk(rq->q->disk);
	u64 lba = sectors_to_logical(sdp, blk_rq_pos(rq));
	u32 nr_blocks = sectors_to_logical(sdp, blk_rq_sectors(rq));
	unsigned int data_len = 24;
	char *buf;

	buf = sd_set_special_bvec(rq, data_len);
	if (!buf)
		return BLK_STS_RESOURCE;

	cmd->cmd_len = 10;
	cmd->cmnd[0] = UNMAP;
	cmd->cmnd[8] = 24;

	put_unaligned_be16(6 + 16, &buf[0]);
	put_unaligned_be16(16, &buf[2]);
	put_unaligned_be64(lba, &buf[8]);
	put_unaligned_be32(nr_blocks, &buf[16]);

	cmd->allowed = sdkp->max_retries;
	cmd->transfersize = data_len;
	rq->timeout = SD_TIMEOUT;

	return scsi_alloc_sgtables(cmd);
}

static blk_status_t sd_setup_write_same16_cmnd(struct scsi_cmnd *cmd,
		bool unmap)
{
	struct scsi_device *sdp = cmd->device;
	struct request *rq = scsi_cmd_to_rq(cmd);
	struct scsi_disk *sdkp = scsi_disk(rq->q->disk);
	u64 lba = sectors_to_logical(sdp, blk_rq_pos(rq));
	u32 nr_blocks = sectors_to_logical(sdp, blk_rq_sectors(rq));
	u32 data_len = sdp->sector_size;

	if (!sd_set_special_bvec(rq, data_len))
		return BLK_STS_RESOURCE;

	cmd->cmd_len = 16;
	cmd->cmnd[0] = WRITE_SAME_16;
	if (unmap)
		cmd->cmnd[1] = 0x8;  
	put_unaligned_be64(lba, &cmd->cmnd[2]);
	put_unaligned_be32(nr_blocks, &cmd->cmnd[10]);

	cmd->allowed = sdkp->max_retries;
	cmd->transfersize = data_len;
	rq->timeout = unmap ? SD_TIMEOUT : SD_WRITE_SAME_TIMEOUT;

	return scsi_alloc_sgtables(cmd);
}

static blk_status_t sd_setup_write_same10_cmnd(struct scsi_cmnd *cmd,
		bool unmap)
{
	struct scsi_device *sdp = cmd->device;
	struct request *rq = scsi_cmd_to_rq(cmd);
	struct scsi_disk *sdkp = scsi_disk(rq->q->disk);
	u64 lba = sectors_to_logical(sdp, blk_rq_pos(rq));
	u32 nr_blocks = sectors_to_logical(sdp, blk_rq_sectors(rq));
	u32 data_len = sdp->sector_size;

	if (!sd_set_special_bvec(rq, data_len))
		return BLK_STS_RESOURCE;

	cmd->cmd_len = 10;
	cmd->cmnd[0] = WRITE_SAME;
	if (unmap)
		cmd->cmnd[1] = 0x8;  
	put_unaligned_be32(lba, &cmd->cmnd[2]);
	put_unaligned_be16(nr_blocks, &cmd->cmnd[7]);

	cmd->allowed = sdkp->max_retries;
	cmd->transfersize = data_len;
	rq->timeout = unmap ? SD_TIMEOUT : SD_WRITE_SAME_TIMEOUT;

	return scsi_alloc_sgtables(cmd);
}

static blk_status_t sd_setup_write_zeroes_cmnd(struct scsi_cmnd *cmd)
{
	struct request *rq = scsi_cmd_to_rq(cmd);
	struct scsi_device *sdp = cmd->device;
	struct scsi_disk *sdkp = scsi_disk(rq->q->disk);
	u64 lba = sectors_to_logical(sdp, blk_rq_pos(rq));
	u32 nr_blocks = sectors_to_logical(sdp, blk_rq_sectors(rq));

	if (!(rq->cmd_flags & REQ_NOUNMAP)) {
		switch (sdkp->zeroing_mode) {
		case SD_ZERO_WS16_UNMAP:
			return sd_setup_write_same16_cmnd(cmd, true);
		case SD_ZERO_WS10_UNMAP:
			return sd_setup_write_same10_cmnd(cmd, true);
		}
	}

	if (sdp->no_write_same) {
		rq->rq_flags |= RQF_QUIET;
		return BLK_STS_TARGET;
	}

	if (sdkp->ws16 || lba > 0xffffffff || nr_blocks > 0xffff)
		return sd_setup_write_same16_cmnd(cmd, false);

	return sd_setup_write_same10_cmnd(cmd, false);
}

static void sd_config_write_same(struct scsi_disk *sdkp)
{
	struct request_queue *q = sdkp->disk->queue;
	unsigned int logical_block_size = sdkp->device->sector_size;

	if (sdkp->device->no_write_same) {
		sdkp->max_ws_blocks = 0;
		goto out;
	}

	 
	if (sdkp->max_ws_blocks > SD_MAX_WS10_BLOCKS)
		sdkp->max_ws_blocks = min_not_zero(sdkp->max_ws_blocks,
						   (u32)SD_MAX_WS16_BLOCKS);
	else if (sdkp->ws16 || sdkp->ws10 || sdkp->device->no_report_opcodes)
		sdkp->max_ws_blocks = min_not_zero(sdkp->max_ws_blocks,
						   (u32)SD_MAX_WS10_BLOCKS);
	else {
		sdkp->device->no_write_same = 1;
		sdkp->max_ws_blocks = 0;
	}

	if (sdkp->lbprz && sdkp->lbpws)
		sdkp->zeroing_mode = SD_ZERO_WS16_UNMAP;
	else if (sdkp->lbprz && sdkp->lbpws10)
		sdkp->zeroing_mode = SD_ZERO_WS10_UNMAP;
	else if (sdkp->max_ws_blocks)
		sdkp->zeroing_mode = SD_ZERO_WS;
	else
		sdkp->zeroing_mode = SD_ZERO_WRITE;

	if (sdkp->max_ws_blocks &&
	    sdkp->physical_block_size > logical_block_size) {
		 
		sdkp->max_ws_blocks =
			round_down(sdkp->max_ws_blocks,
				   bytes_to_logical(sdkp->device,
						    sdkp->physical_block_size));
	}

out:
	blk_queue_max_write_zeroes_sectors(q, sdkp->max_ws_blocks *
					 (logical_block_size >> 9));
}

static blk_status_t sd_setup_flush_cmnd(struct scsi_cmnd *cmd)
{
	struct request *rq = scsi_cmd_to_rq(cmd);
	struct scsi_disk *sdkp = scsi_disk(rq->q->disk);

	 
	memset(&cmd->sdb, 0, sizeof(cmd->sdb));

	if (cmd->device->use_16_for_sync) {
		cmd->cmnd[0] = SYNCHRONIZE_CACHE_16;
		cmd->cmd_len = 16;
	} else {
		cmd->cmnd[0] = SYNCHRONIZE_CACHE;
		cmd->cmd_len = 10;
	}
	cmd->transfersize = 0;
	cmd->allowed = sdkp->max_retries;

	rq->timeout = rq->q->rq_timeout * SD_FLUSH_TIMEOUT_MULTIPLIER;
	return BLK_STS_OK;
}

static blk_status_t sd_setup_rw32_cmnd(struct scsi_cmnd *cmd, bool write,
				       sector_t lba, unsigned int nr_blocks,
				       unsigned char flags, unsigned int dld)
{
	cmd->cmd_len = SD_EXT_CDB_SIZE;
	cmd->cmnd[0]  = VARIABLE_LENGTH_CMD;
	cmd->cmnd[7]  = 0x18;  
	cmd->cmnd[9]  = write ? WRITE_32 : READ_32;
	cmd->cmnd[10] = flags;
	cmd->cmnd[11] = dld & 0x07;
	put_unaligned_be64(lba, &cmd->cmnd[12]);
	put_unaligned_be32(lba, &cmd->cmnd[20]);  
	put_unaligned_be32(nr_blocks, &cmd->cmnd[28]);

	return BLK_STS_OK;
}

static blk_status_t sd_setup_rw16_cmnd(struct scsi_cmnd *cmd, bool write,
				       sector_t lba, unsigned int nr_blocks,
				       unsigned char flags, unsigned int dld)
{
	cmd->cmd_len  = 16;
	cmd->cmnd[0]  = write ? WRITE_16 : READ_16;
	cmd->cmnd[1]  = flags | ((dld >> 2) & 0x01);
	cmd->cmnd[14] = (dld & 0x03) << 6;
	cmd->cmnd[15] = 0;
	put_unaligned_be64(lba, &cmd->cmnd[2]);
	put_unaligned_be32(nr_blocks, &cmd->cmnd[10]);

	return BLK_STS_OK;
}

static blk_status_t sd_setup_rw10_cmnd(struct scsi_cmnd *cmd, bool write,
				       sector_t lba, unsigned int nr_blocks,
				       unsigned char flags)
{
	cmd->cmd_len = 10;
	cmd->cmnd[0] = write ? WRITE_10 : READ_10;
	cmd->cmnd[1] = flags;
	cmd->cmnd[6] = 0;
	cmd->cmnd[9] = 0;
	put_unaligned_be32(lba, &cmd->cmnd[2]);
	put_unaligned_be16(nr_blocks, &cmd->cmnd[7]);

	return BLK_STS_OK;
}

static blk_status_t sd_setup_rw6_cmnd(struct scsi_cmnd *cmd, bool write,
				      sector_t lba, unsigned int nr_blocks,
				      unsigned char flags)
{
	 
	if (WARN_ON_ONCE(nr_blocks == 0))
		return BLK_STS_IOERR;

	if (unlikely(flags & 0x8)) {
		 
		scmd_printk(KERN_ERR, cmd, "FUA write on READ/WRITE(6) drive\n");
		return BLK_STS_IOERR;
	}

	cmd->cmd_len = 6;
	cmd->cmnd[0] = write ? WRITE_6 : READ_6;
	cmd->cmnd[1] = (lba >> 16) & 0x1f;
	cmd->cmnd[2] = (lba >> 8) & 0xff;
	cmd->cmnd[3] = lba & 0xff;
	cmd->cmnd[4] = nr_blocks;
	cmd->cmnd[5] = 0;

	return BLK_STS_OK;
}

 
static int sd_cdl_dld(struct scsi_disk *sdkp, struct scsi_cmnd *scmd)
{
	struct scsi_device *sdp = sdkp->device;
	int hint;

	if (!sdp->cdl_supported || !sdp->cdl_enable)
		return 0;

	 
	hint = IOPRIO_PRIO_HINT(req_get_ioprio(scsi_cmd_to_rq(scmd)));
	if (hint < IOPRIO_HINT_DEV_DURATION_LIMIT_1 ||
	    hint > IOPRIO_HINT_DEV_DURATION_LIMIT_7)
		return 0;

	return (hint - IOPRIO_HINT_DEV_DURATION_LIMIT_1) + 1;
}

static blk_status_t sd_setup_read_write_cmnd(struct scsi_cmnd *cmd)
{
	struct request *rq = scsi_cmd_to_rq(cmd);
	struct scsi_device *sdp = cmd->device;
	struct scsi_disk *sdkp = scsi_disk(rq->q->disk);
	sector_t lba = sectors_to_logical(sdp, blk_rq_pos(rq));
	sector_t threshold;
	unsigned int nr_blocks = sectors_to_logical(sdp, blk_rq_sectors(rq));
	unsigned int mask = logical_to_sectors(sdp, 1) - 1;
	bool write = rq_data_dir(rq) == WRITE;
	unsigned char protect, fua;
	unsigned int dld;
	blk_status_t ret;
	unsigned int dif;
	bool dix;

	ret = scsi_alloc_sgtables(cmd);
	if (ret != BLK_STS_OK)
		return ret;

	ret = BLK_STS_IOERR;
	if (!scsi_device_online(sdp) || sdp->changed) {
		scmd_printk(KERN_ERR, cmd, "device offline or changed\n");
		goto fail;
	}

	if (blk_rq_pos(rq) + blk_rq_sectors(rq) > get_capacity(rq->q->disk)) {
		scmd_printk(KERN_ERR, cmd, "access beyond end of device\n");
		goto fail;
	}

	if ((blk_rq_pos(rq) & mask) || (blk_rq_sectors(rq) & mask)) {
		scmd_printk(KERN_ERR, cmd, "request not aligned to the logical block size\n");
		goto fail;
	}

	 
	threshold = sdkp->capacity - SD_LAST_BUGGY_SECTORS;

	if (unlikely(sdp->last_sector_bug && lba + nr_blocks > threshold)) {
		if (lba < threshold) {
			 
			nr_blocks = threshold - lba;
		} else {
			 
			nr_blocks = 1;
		}
	}

	if (req_op(rq) == REQ_OP_ZONE_APPEND) {
		ret = sd_zbc_prepare_zone_append(cmd, &lba, nr_blocks);
		if (ret)
			goto fail;
	}

	fua = rq->cmd_flags & REQ_FUA ? 0x8 : 0;
	dix = scsi_prot_sg_count(cmd);
	dif = scsi_host_dif_capable(cmd->device->host, sdkp->protection_type);
	dld = sd_cdl_dld(sdkp, cmd);

	if (dif || dix)
		protect = sd_setup_protect_cmnd(cmd, dix, dif);
	else
		protect = 0;

	if (protect && sdkp->protection_type == T10_PI_TYPE2_PROTECTION) {
		ret = sd_setup_rw32_cmnd(cmd, write, lba, nr_blocks,
					 protect | fua, dld);
	} else if (sdp->use_16_for_rw || (nr_blocks > 0xffff)) {
		ret = sd_setup_rw16_cmnd(cmd, write, lba, nr_blocks,
					 protect | fua, dld);
	} else if ((nr_blocks > 0xff) || (lba > 0x1fffff) ||
		   sdp->use_10_for_rw || protect) {
		ret = sd_setup_rw10_cmnd(cmd, write, lba, nr_blocks,
					 protect | fua);
	} else {
		ret = sd_setup_rw6_cmnd(cmd, write, lba, nr_blocks,
					protect | fua);
	}

	if (unlikely(ret != BLK_STS_OK))
		goto fail;

	 
	cmd->transfersize = sdp->sector_size;
	cmd->underflow = nr_blocks << 9;
	cmd->allowed = sdkp->max_retries;
	cmd->sdb.length = nr_blocks * sdp->sector_size;

	SCSI_LOG_HLQUEUE(1,
			 scmd_printk(KERN_INFO, cmd,
				     "%s: block=%llu, count=%d\n", __func__,
				     (unsigned long long)blk_rq_pos(rq),
				     blk_rq_sectors(rq)));
	SCSI_LOG_HLQUEUE(2,
			 scmd_printk(KERN_INFO, cmd,
				     "%s %d/%u 512 byte blocks.\n",
				     write ? "writing" : "reading", nr_blocks,
				     blk_rq_sectors(rq)));

	 
	return BLK_STS_OK;
fail:
	scsi_free_sgtables(cmd);
	return ret;
}

static blk_status_t sd_init_command(struct scsi_cmnd *cmd)
{
	struct request *rq = scsi_cmd_to_rq(cmd);

	switch (req_op(rq)) {
	case REQ_OP_DISCARD:
		switch (scsi_disk(rq->q->disk)->provisioning_mode) {
		case SD_LBP_UNMAP:
			return sd_setup_unmap_cmnd(cmd);
		case SD_LBP_WS16:
			return sd_setup_write_same16_cmnd(cmd, true);
		case SD_LBP_WS10:
			return sd_setup_write_same10_cmnd(cmd, true);
		case SD_LBP_ZERO:
			return sd_setup_write_same10_cmnd(cmd, false);
		default:
			return BLK_STS_TARGET;
		}
	case REQ_OP_WRITE_ZEROES:
		return sd_setup_write_zeroes_cmnd(cmd);
	case REQ_OP_FLUSH:
		return sd_setup_flush_cmnd(cmd);
	case REQ_OP_READ:
	case REQ_OP_WRITE:
	case REQ_OP_ZONE_APPEND:
		return sd_setup_read_write_cmnd(cmd);
	case REQ_OP_ZONE_RESET:
		return sd_zbc_setup_zone_mgmt_cmnd(cmd, ZO_RESET_WRITE_POINTER,
						   false);
	case REQ_OP_ZONE_RESET_ALL:
		return sd_zbc_setup_zone_mgmt_cmnd(cmd, ZO_RESET_WRITE_POINTER,
						   true);
	case REQ_OP_ZONE_OPEN:
		return sd_zbc_setup_zone_mgmt_cmnd(cmd, ZO_OPEN_ZONE, false);
	case REQ_OP_ZONE_CLOSE:
		return sd_zbc_setup_zone_mgmt_cmnd(cmd, ZO_CLOSE_ZONE, false);
	case REQ_OP_ZONE_FINISH:
		return sd_zbc_setup_zone_mgmt_cmnd(cmd, ZO_FINISH_ZONE, false);
	default:
		WARN_ON_ONCE(1);
		return BLK_STS_NOTSUPP;
	}
}

static void sd_uninit_command(struct scsi_cmnd *SCpnt)
{
	struct request *rq = scsi_cmd_to_rq(SCpnt);

	if (rq->rq_flags & RQF_SPECIAL_PAYLOAD)
		mempool_free(rq->special_vec.bv_page, sd_page_pool);
}

static bool sd_need_revalidate(struct gendisk *disk, struct scsi_disk *sdkp)
{
	if (sdkp->device->removable || sdkp->write_prot) {
		if (disk_check_media_change(disk))
			return true;
	}

	 
	return test_bit(GD_NEED_PART_SCAN, &disk->state);
}

 
static int sd_open(struct gendisk *disk, blk_mode_t mode)
{
	struct scsi_disk *sdkp = scsi_disk(disk);
	struct scsi_device *sdev = sdkp->device;
	int retval;

	if (scsi_device_get(sdev))
		return -ENXIO;

	SCSI_LOG_HLQUEUE(3, sd_printk(KERN_INFO, sdkp, "sd_open\n"));

	 
	retval = -ENXIO;
	if (!scsi_block_when_processing_errors(sdev))
		goto error_out;

	if (sd_need_revalidate(disk, sdkp))
		sd_revalidate_disk(disk);

	 
	retval = -ENOMEDIUM;
	if (sdev->removable && !sdkp->media_present &&
	    !(mode & BLK_OPEN_NDELAY))
		goto error_out;

	 
	retval = -EROFS;
	if (sdkp->write_prot && (mode & BLK_OPEN_WRITE))
		goto error_out;

	 
	retval = -ENXIO;
	if (!scsi_device_online(sdev))
		goto error_out;

	if ((atomic_inc_return(&sdkp->openers) == 1) && sdev->removable) {
		if (scsi_block_when_processing_errors(sdev))
			scsi_set_medium_removal(sdev, SCSI_REMOVAL_PREVENT);
	}

	return 0;

error_out:
	scsi_device_put(sdev);
	return retval;	
}

 
static void sd_release(struct gendisk *disk)
{
	struct scsi_disk *sdkp = scsi_disk(disk);
	struct scsi_device *sdev = sdkp->device;

	SCSI_LOG_HLQUEUE(3, sd_printk(KERN_INFO, sdkp, "sd_release\n"));

	if (atomic_dec_return(&sdkp->openers) == 0 && sdev->removable) {
		if (scsi_block_when_processing_errors(sdev))
			scsi_set_medium_removal(sdev, SCSI_REMOVAL_ALLOW);
	}

	scsi_device_put(sdev);
}

static int sd_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	struct scsi_disk *sdkp = scsi_disk(bdev->bd_disk);
	struct scsi_device *sdp = sdkp->device;
	struct Scsi_Host *host = sdp->host;
	sector_t capacity = logical_to_sectors(sdp, sdkp->capacity);
	int diskinfo[4];

	 
	diskinfo[0] = 0x40;	 
	diskinfo[1] = 0x20;	 
	diskinfo[2] = capacity >> 11;

	 
	if (host->hostt->bios_param)
		host->hostt->bios_param(sdp, bdev, capacity, diskinfo);
	else
		scsicam_bios_param(bdev, capacity, diskinfo);

	geo->heads = diskinfo[0];
	geo->sectors = diskinfo[1];
	geo->cylinders = diskinfo[2];
	return 0;
}

 
static int sd_ioctl(struct block_device *bdev, blk_mode_t mode,
		    unsigned int cmd, unsigned long arg)
{
	struct gendisk *disk = bdev->bd_disk;
	struct scsi_disk *sdkp = scsi_disk(disk);
	struct scsi_device *sdp = sdkp->device;
	void __user *p = (void __user *)arg;
	int error;
    
	SCSI_LOG_IOCTL(1, sd_printk(KERN_INFO, sdkp, "sd_ioctl: disk=%s, "
				    "cmd=0x%x\n", disk->disk_name, cmd));

	if (bdev_is_partition(bdev) && !capable(CAP_SYS_RAWIO))
		return -ENOIOCTLCMD;

	 
	error = scsi_ioctl_block_when_processing_errors(sdp, cmd,
			(mode & BLK_OPEN_NDELAY));
	if (error)
		return error;

	if (is_sed_ioctl(cmd))
		return sed_ioctl(sdkp->opal_dev, cmd, p);
	return scsi_ioctl(sdp, mode & BLK_OPEN_WRITE, cmd, p);
}

static void set_media_not_present(struct scsi_disk *sdkp)
{
	if (sdkp->media_present)
		sdkp->device->changed = 1;

	if (sdkp->device->removable) {
		sdkp->media_present = 0;
		sdkp->capacity = 0;
	}
}

static int media_not_present(struct scsi_disk *sdkp,
			     struct scsi_sense_hdr *sshdr)
{
	if (!scsi_sense_valid(sshdr))
		return 0;

	 
	switch (sshdr->sense_key) {
	case UNIT_ATTENTION:
	case NOT_READY:
		 
		if (sshdr->asc == 0x3A) {
			set_media_not_present(sdkp);
			return 1;
		}
	}
	return 0;
}

 
static unsigned int sd_check_events(struct gendisk *disk, unsigned int clearing)
{
	struct scsi_disk *sdkp = disk->private_data;
	struct scsi_device *sdp;
	int retval;
	bool disk_changed;

	if (!sdkp)
		return 0;

	sdp = sdkp->device;
	SCSI_LOG_HLQUEUE(3, sd_printk(KERN_INFO, sdkp, "sd_check_events\n"));

	 
	if (!scsi_device_online(sdp)) {
		set_media_not_present(sdkp);
		goto out;
	}

	 
	if (scsi_block_when_processing_errors(sdp)) {
		struct scsi_sense_hdr sshdr = { 0, };

		retval = scsi_test_unit_ready(sdp, SD_TIMEOUT, sdkp->max_retries,
					      &sshdr);

		 
		if (retval < 0 || host_byte(retval)) {
			set_media_not_present(sdkp);
			goto out;
		}

		if (media_not_present(sdkp, &sshdr))
			goto out;
	}

	 
	if (!sdkp->media_present)
		sdp->changed = 1;
	sdkp->media_present = 1;
out:
	 
	disk_changed = sdp->changed;
	sdp->changed = 0;
	return disk_changed ? DISK_EVENT_MEDIA_CHANGE : 0;
}

static int sd_sync_cache(struct scsi_disk *sdkp)
{
	int retries, res;
	struct scsi_device *sdp = sdkp->device;
	const int timeout = sdp->request_queue->rq_timeout
		* SD_FLUSH_TIMEOUT_MULTIPLIER;
	struct scsi_sense_hdr sshdr;
	const struct scsi_exec_args exec_args = {
		.req_flags = BLK_MQ_REQ_PM,
		.sshdr = &sshdr,
	};

	if (!scsi_device_online(sdp))
		return -ENODEV;

	for (retries = 3; retries > 0; --retries) {
		unsigned char cmd[16] = { 0 };

		if (sdp->use_16_for_sync)
			cmd[0] = SYNCHRONIZE_CACHE_16;
		else
			cmd[0] = SYNCHRONIZE_CACHE;
		 
		res = scsi_execute_cmd(sdp, cmd, REQ_OP_DRV_IN, NULL, 0,
				       timeout, sdkp->max_retries, &exec_args);
		if (res == 0)
			break;
	}

	if (res) {
		sd_print_result(sdkp, "Synchronize Cache(10) failed", res);

		if (res < 0)
			return res;

		if (scsi_status_is_check_condition(res) &&
		    scsi_sense_valid(&sshdr)) {
			sd_print_sense_hdr(sdkp, &sshdr);

			 
			if (sshdr.asc == 0x3a ||	 
			    sshdr.asc == 0x20 ||	 
			    (sshdr.asc == 0x74 && sshdr.ascq == 0x71))	 
				 
				return 0;
			 
			if (sshdr.sense_key == ILLEGAL_REQUEST)
				return 0;
		}

		switch (host_byte(res)) {
		 
		case DID_BAD_TARGET:
		case DID_NO_CONNECT:
			return 0;
		 
		case DID_BUS_BUSY:
		case DID_IMM_RETRY:
		case DID_REQUEUE:
		case DID_SOFT_ERROR:
			return -EBUSY;
		default:
			return -EIO;
		}
	}
	return 0;
}

static void sd_rescan(struct device *dev)
{
	struct scsi_disk *sdkp = dev_get_drvdata(dev);

	sd_revalidate_disk(sdkp->disk);
}

static int sd_get_unique_id(struct gendisk *disk, u8 id[16],
		enum blk_unique_id type)
{
	struct scsi_device *sdev = scsi_disk(disk)->device;
	const struct scsi_vpd *vpd;
	const unsigned char *d;
	int ret = -ENXIO, len;

	rcu_read_lock();
	vpd = rcu_dereference(sdev->vpd_pg83);
	if (!vpd)
		goto out_unlock;

	ret = -EINVAL;
	for (d = vpd->data + 4; d < vpd->data + vpd->len; d += d[3] + 4) {
		 
		if (((d[1] >> 4) & 0x3) != 0x00)
			continue;
		if ((d[1] & 0xf) != type)
			continue;

		 
		len = d[3];
		if (len != 8 && len != 12 && len != 16)
			continue;
		ret = len;
		memcpy(id, d + 4, len);
		if (len == 16)
			break;
	}
out_unlock:
	rcu_read_unlock();
	return ret;
}

static int sd_scsi_to_pr_err(struct scsi_sense_hdr *sshdr, int result)
{
	switch (host_byte(result)) {
	case DID_TRANSPORT_MARGINAL:
	case DID_TRANSPORT_DISRUPTED:
	case DID_BUS_BUSY:
		return PR_STS_RETRY_PATH_FAILURE;
	case DID_NO_CONNECT:
		return PR_STS_PATH_FAILED;
	case DID_TRANSPORT_FAILFAST:
		return PR_STS_PATH_FAST_FAILED;
	}

	switch (status_byte(result)) {
	case SAM_STAT_RESERVATION_CONFLICT:
		return PR_STS_RESERVATION_CONFLICT;
	case SAM_STAT_CHECK_CONDITION:
		if (!scsi_sense_valid(sshdr))
			return PR_STS_IOERR;

		if (sshdr->sense_key == ILLEGAL_REQUEST &&
		    (sshdr->asc == 0x26 || sshdr->asc == 0x24))
			return -EINVAL;

		fallthrough;
	default:
		return PR_STS_IOERR;
	}
}

static int sd_pr_in_command(struct block_device *bdev, u8 sa,
			    unsigned char *data, int data_len)
{
	struct scsi_disk *sdkp = scsi_disk(bdev->bd_disk);
	struct scsi_device *sdev = sdkp->device;
	struct scsi_sense_hdr sshdr;
	u8 cmd[10] = { PERSISTENT_RESERVE_IN, sa };
	const struct scsi_exec_args exec_args = {
		.sshdr = &sshdr,
	};
	int result;

	put_unaligned_be16(data_len, &cmd[7]);

	result = scsi_execute_cmd(sdev, cmd, REQ_OP_DRV_IN, data, data_len,
				  SD_TIMEOUT, sdkp->max_retries, &exec_args);
	if (scsi_status_is_check_condition(result) &&
	    scsi_sense_valid(&sshdr)) {
		sdev_printk(KERN_INFO, sdev, "PR command failed: %d\n", result);
		scsi_print_sense_hdr(sdev, NULL, &sshdr);
	}

	if (result <= 0)
		return result;

	return sd_scsi_to_pr_err(&sshdr, result);
}

static int sd_pr_read_keys(struct block_device *bdev, struct pr_keys *keys_info)
{
	int result, i, data_offset, num_copy_keys;
	u32 num_keys = keys_info->num_keys;
	int data_len = num_keys * 8 + 8;
	u8 *data;

	data = kzalloc(data_len, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	result = sd_pr_in_command(bdev, READ_KEYS, data, data_len);
	if (result)
		goto free_data;

	keys_info->generation = get_unaligned_be32(&data[0]);
	keys_info->num_keys = get_unaligned_be32(&data[4]) / 8;

	data_offset = 8;
	num_copy_keys = min(num_keys, keys_info->num_keys);

	for (i = 0; i < num_copy_keys; i++) {
		keys_info->keys[i] = get_unaligned_be64(&data[data_offset]);
		data_offset += 8;
	}

free_data:
	kfree(data);
	return result;
}

static int sd_pr_read_reservation(struct block_device *bdev,
				  struct pr_held_reservation *rsv)
{
	struct scsi_disk *sdkp = scsi_disk(bdev->bd_disk);
	struct scsi_device *sdev = sdkp->device;
	u8 data[24] = { };
	int result, len;

	result = sd_pr_in_command(bdev, READ_RESERVATION, data, sizeof(data));
	if (result)
		return result;

	len = get_unaligned_be32(&data[4]);
	if (!len)
		return 0;

	 
	if (len < 14) {
		sdev_printk(KERN_INFO, sdev,
			    "READ RESERVATION failed due to short return buffer of %d bytes\n",
			    len);
		return -EINVAL;
	}

	rsv->generation = get_unaligned_be32(&data[0]);
	rsv->key = get_unaligned_be64(&data[8]);
	rsv->type = scsi_pr_type_to_block(data[21] & 0x0f);
	return 0;
}

static int sd_pr_out_command(struct block_device *bdev, u8 sa, u64 key,
			     u64 sa_key, enum scsi_pr_type type, u8 flags)
{
	struct scsi_disk *sdkp = scsi_disk(bdev->bd_disk);
	struct scsi_device *sdev = sdkp->device;
	struct scsi_sense_hdr sshdr;
	const struct scsi_exec_args exec_args = {
		.sshdr = &sshdr,
	};
	int result;
	u8 cmd[16] = { 0, };
	u8 data[24] = { 0, };

	cmd[0] = PERSISTENT_RESERVE_OUT;
	cmd[1] = sa;
	cmd[2] = type;
	put_unaligned_be32(sizeof(data), &cmd[5]);

	put_unaligned_be64(key, &data[0]);
	put_unaligned_be64(sa_key, &data[8]);
	data[20] = flags;

	result = scsi_execute_cmd(sdev, cmd, REQ_OP_DRV_OUT, &data,
				  sizeof(data), SD_TIMEOUT, sdkp->max_retries,
				  &exec_args);

	if (scsi_status_is_check_condition(result) &&
	    scsi_sense_valid(&sshdr)) {
		sdev_printk(KERN_INFO, sdev, "PR command failed: %d\n", result);
		scsi_print_sense_hdr(sdev, NULL, &sshdr);
	}

	if (result <= 0)
		return result;

	return sd_scsi_to_pr_err(&sshdr, result);
}

static int sd_pr_register(struct block_device *bdev, u64 old_key, u64 new_key,
		u32 flags)
{
	if (flags & ~PR_FL_IGNORE_KEY)
		return -EOPNOTSUPP;
	return sd_pr_out_command(bdev, (flags & PR_FL_IGNORE_KEY) ? 0x06 : 0x00,
			old_key, new_key, 0,
			(1 << 0)  );
}

static int sd_pr_reserve(struct block_device *bdev, u64 key, enum pr_type type,
		u32 flags)
{
	if (flags)
		return -EOPNOTSUPP;
	return sd_pr_out_command(bdev, 0x01, key, 0,
				 block_pr_type_to_scsi(type), 0);
}

static int sd_pr_release(struct block_device *bdev, u64 key, enum pr_type type)
{
	return sd_pr_out_command(bdev, 0x02, key, 0,
				 block_pr_type_to_scsi(type), 0);
}

static int sd_pr_preempt(struct block_device *bdev, u64 old_key, u64 new_key,
		enum pr_type type, bool abort)
{
	return sd_pr_out_command(bdev, abort ? 0x05 : 0x04, old_key, new_key,
				 block_pr_type_to_scsi(type), 0);
}

static int sd_pr_clear(struct block_device *bdev, u64 key)
{
	return sd_pr_out_command(bdev, 0x03, key, 0, 0, 0);
}

static const struct pr_ops sd_pr_ops = {
	.pr_register	= sd_pr_register,
	.pr_reserve	= sd_pr_reserve,
	.pr_release	= sd_pr_release,
	.pr_preempt	= sd_pr_preempt,
	.pr_clear	= sd_pr_clear,
	.pr_read_keys	= sd_pr_read_keys,
	.pr_read_reservation = sd_pr_read_reservation,
};

static void scsi_disk_free_disk(struct gendisk *disk)
{
	struct scsi_disk *sdkp = scsi_disk(disk);

	put_device(&sdkp->disk_dev);
}

static const struct block_device_operations sd_fops = {
	.owner			= THIS_MODULE,
	.open			= sd_open,
	.release		= sd_release,
	.ioctl			= sd_ioctl,
	.getgeo			= sd_getgeo,
	.compat_ioctl		= blkdev_compat_ptr_ioctl,
	.check_events		= sd_check_events,
	.unlock_native_capacity	= sd_unlock_native_capacity,
	.report_zones		= sd_zbc_report_zones,
	.get_unique_id		= sd_get_unique_id,
	.free_disk		= scsi_disk_free_disk,
	.pr_ops			= &sd_pr_ops,
};

 
static void sd_eh_reset(struct scsi_cmnd *scmd)
{
	struct scsi_disk *sdkp = scsi_disk(scsi_cmd_to_rq(scmd)->q->disk);

	 
	sdkp->ignore_medium_access_errors = false;
}

 
static int sd_eh_action(struct scsi_cmnd *scmd, int eh_disp)
{
	struct scsi_disk *sdkp = scsi_disk(scsi_cmd_to_rq(scmd)->q->disk);
	struct scsi_device *sdev = scmd->device;

	if (!scsi_device_online(sdev) ||
	    !scsi_medium_access_command(scmd) ||
	    host_byte(scmd->result) != DID_TIME_OUT ||
	    eh_disp != SUCCESS)
		return eh_disp;

	 
	if (!sdkp->ignore_medium_access_errors) {
		sdkp->medium_access_timed_out++;
		sdkp->ignore_medium_access_errors = true;
	}

	 
	if (sdkp->medium_access_timed_out >= sdkp->max_medium_access_timeouts) {
		scmd_printk(KERN_ERR, scmd,
			    "Medium access timeout failure. Offlining disk!\n");
		mutex_lock(&sdev->state_mutex);
		scsi_device_set_state(sdev, SDEV_OFFLINE);
		mutex_unlock(&sdev->state_mutex);

		return SUCCESS;
	}

	return eh_disp;
}

static unsigned int sd_completed_bytes(struct scsi_cmnd *scmd)
{
	struct request *req = scsi_cmd_to_rq(scmd);
	struct scsi_device *sdev = scmd->device;
	unsigned int transferred, good_bytes;
	u64 start_lba, end_lba, bad_lba;

	 
	if (scsi_bufflen(scmd) <= sdev->sector_size)
		return 0;

	 
	if (!scsi_get_sense_info_fld(scmd->sense_buffer,
				     SCSI_SENSE_BUFFERSIZE,
				     &bad_lba))
		return 0;

	 
	start_lba = sectors_to_logical(sdev, blk_rq_pos(req));
	end_lba = start_lba + bytes_to_logical(sdev, scsi_bufflen(scmd));
	if (bad_lba < start_lba || bad_lba >= end_lba)
		return 0;

	 
	transferred = scsi_bufflen(scmd) - scsi_get_resid(scmd);

	 
	good_bytes = logical_to_bytes(sdev, bad_lba - start_lba);

	return min(good_bytes, transferred);
}

 
static int sd_done(struct scsi_cmnd *SCpnt)
{
	int result = SCpnt->result;
	unsigned int good_bytes = result ? 0 : scsi_bufflen(SCpnt);
	unsigned int sector_size = SCpnt->device->sector_size;
	unsigned int resid;
	struct scsi_sense_hdr sshdr;
	struct request *req = scsi_cmd_to_rq(SCpnt);
	struct scsi_disk *sdkp = scsi_disk(req->q->disk);
	int sense_valid = 0;
	int sense_deferred = 0;

	switch (req_op(req)) {
	case REQ_OP_DISCARD:
	case REQ_OP_WRITE_ZEROES:
	case REQ_OP_ZONE_RESET:
	case REQ_OP_ZONE_RESET_ALL:
	case REQ_OP_ZONE_OPEN:
	case REQ_OP_ZONE_CLOSE:
	case REQ_OP_ZONE_FINISH:
		if (!result) {
			good_bytes = blk_rq_bytes(req);
			scsi_set_resid(SCpnt, 0);
		} else {
			good_bytes = 0;
			scsi_set_resid(SCpnt, blk_rq_bytes(req));
		}
		break;
	default:
		 
		resid = scsi_get_resid(SCpnt);
		if (resid & (sector_size - 1)) {
			sd_printk(KERN_INFO, sdkp,
				"Unaligned partial completion (resid=%u, sector_sz=%u)\n",
				resid, sector_size);
			scsi_print_command(SCpnt);
			resid = min(scsi_bufflen(SCpnt),
				    round_up(resid, sector_size));
			scsi_set_resid(SCpnt, resid);
		}
	}

	if (result) {
		sense_valid = scsi_command_normalize_sense(SCpnt, &sshdr);
		if (sense_valid)
			sense_deferred = scsi_sense_is_deferred(&sshdr);
	}
	sdkp->medium_access_timed_out = 0;

	if (!scsi_status_is_check_condition(result) &&
	    (!sense_valid || sense_deferred))
		goto out;

	switch (sshdr.sense_key) {
	case HARDWARE_ERROR:
	case MEDIUM_ERROR:
		good_bytes = sd_completed_bytes(SCpnt);
		break;
	case RECOVERED_ERROR:
		good_bytes = scsi_bufflen(SCpnt);
		break;
	case NO_SENSE:
		 
		SCpnt->result = 0;
		memset(SCpnt->sense_buffer, 0, SCSI_SENSE_BUFFERSIZE);
		break;
	case ABORTED_COMMAND:
		if (sshdr.asc == 0x10)   
			good_bytes = sd_completed_bytes(SCpnt);
		break;
	case ILLEGAL_REQUEST:
		switch (sshdr.asc) {
		case 0x10:	 
			good_bytes = sd_completed_bytes(SCpnt);
			break;
		case 0x20:	 
		case 0x24:	 
			switch (SCpnt->cmnd[0]) {
			case UNMAP:
				sd_config_discard(sdkp, SD_LBP_DISABLE);
				break;
			case WRITE_SAME_16:
			case WRITE_SAME:
				if (SCpnt->cmnd[1] & 8) {  
					sd_config_discard(sdkp, SD_LBP_DISABLE);
				} else {
					sdkp->device->no_write_same = 1;
					sd_config_write_same(sdkp);
					req->rq_flags |= RQF_QUIET;
				}
				break;
			}
		}
		break;
	default:
		break;
	}

 out:
	if (sd_is_zoned(sdkp))
		good_bytes = sd_zbc_complete(SCpnt, good_bytes, &sshdr);

	SCSI_LOG_HLCOMPLETE(1, scmd_printk(KERN_INFO, SCpnt,
					   "sd_done: completed %d of %d bytes\n",
					   good_bytes, scsi_bufflen(SCpnt)));

	return good_bytes;
}

 
static void
sd_spinup_disk(struct scsi_disk *sdkp)
{
	unsigned char cmd[10];
	unsigned long spintime_expire = 0;
	int retries, spintime;
	unsigned int the_result;
	struct scsi_sense_hdr sshdr;
	const struct scsi_exec_args exec_args = {
		.sshdr = &sshdr,
	};
	int sense_valid = 0;

	spintime = 0;

	 
	 
	do {
		retries = 0;

		do {
			bool media_was_present = sdkp->media_present;

			cmd[0] = TEST_UNIT_READY;
			memset((void *) &cmd[1], 0, 9);

			the_result = scsi_execute_cmd(sdkp->device, cmd,
						      REQ_OP_DRV_IN, NULL, 0,
						      SD_TIMEOUT,
						      sdkp->max_retries,
						      &exec_args);

			 
			if (media_not_present(sdkp, &sshdr)) {
				if (media_was_present)
					sd_printk(KERN_NOTICE, sdkp, "Media removed, stopped polling\n");
				return;
			}

			if (the_result)
				sense_valid = scsi_sense_valid(&sshdr);
			retries++;
		} while (retries < 3 &&
			 (!scsi_status_is_good(the_result) ||
			  (scsi_status_is_check_condition(the_result) &&
			  sense_valid && sshdr.sense_key == UNIT_ATTENTION)));

		if (!scsi_status_is_check_condition(the_result)) {
			 
			if(!spintime && !scsi_status_is_good(the_result)) {
				sd_print_result(sdkp, "Test Unit Ready failed",
						the_result);
			}
			break;
		}

		 
		if (sdkp->device->no_start_on_add)
			break;

		if (sense_valid && sshdr.sense_key == NOT_READY) {
			if (sshdr.asc == 4 && sshdr.ascq == 3)
				break;	 
			if (sshdr.asc == 4 && sshdr.ascq == 0xb)
				break;	 
			if (sshdr.asc == 4 && sshdr.ascq == 0xc)
				break;	 
			if (sshdr.asc == 4 && sshdr.ascq == 0x1b)
				break;	 
			 
			if (!spintime) {
				sd_printk(KERN_NOTICE, sdkp, "Spinning up disk...");
				cmd[0] = START_STOP;
				cmd[1] = 1;	 
				memset((void *) &cmd[2], 0, 8);
				cmd[4] = 1;	 
				if (sdkp->device->start_stop_pwr_cond)
					cmd[4] |= 1 << 4;
				scsi_execute_cmd(sdkp->device, cmd,
						 REQ_OP_DRV_IN, NULL, 0,
						 SD_TIMEOUT, sdkp->max_retries,
						 &exec_args);
				spintime_expire = jiffies + 100 * HZ;
				spintime = 1;
			}
			 
			msleep(1000);
			printk(KERN_CONT ".");

		 
		} else if (sense_valid &&
				sshdr.sense_key == UNIT_ATTENTION &&
				sshdr.asc == 0x28) {
			if (!spintime) {
				spintime_expire = jiffies + 5 * HZ;
				spintime = 1;
			}
			 
			msleep(1000);
		} else {
			 
			if(!spintime) {
				sd_printk(KERN_NOTICE, sdkp, "Unit Not Ready\n");
				sd_print_sense_hdr(sdkp, &sshdr);
			}
			break;
		}
				
	} while (spintime && time_before_eq(jiffies, spintime_expire));

	if (spintime) {
		if (scsi_status_is_good(the_result))
			printk(KERN_CONT "ready\n");
		else
			printk(KERN_CONT "not responding...\n");
	}
}

 
static int sd_read_protection_type(struct scsi_disk *sdkp, unsigned char *buffer)
{
	struct scsi_device *sdp = sdkp->device;
	u8 type;

	if (scsi_device_protection(sdp) == 0 || (buffer[12] & 1) == 0) {
		sdkp->protection_type = 0;
		return 0;
	}

	type = ((buffer[12] >> 1) & 7) + 1;  

	if (type > T10_PI_TYPE3_PROTECTION) {
		sd_printk(KERN_ERR, sdkp, "formatted with unsupported"	\
			  " protection type %u. Disabling disk!\n",
			  type);
		sdkp->protection_type = 0;
		return -ENODEV;
	}

	sdkp->protection_type = type;

	return 0;
}

static void sd_config_protection(struct scsi_disk *sdkp)
{
	struct scsi_device *sdp = sdkp->device;

	sd_dif_config_host(sdkp);

	if (!sdkp->protection_type)
		return;

	if (!scsi_host_dif_capable(sdp->host, sdkp->protection_type)) {
		sd_first_printk(KERN_NOTICE, sdkp,
				"Disabling DIF Type %u protection\n",
				sdkp->protection_type);
		sdkp->protection_type = 0;
	}

	sd_first_printk(KERN_NOTICE, sdkp, "Enabling DIF Type %u protection\n",
			sdkp->protection_type);
}

static void read_capacity_error(struct scsi_disk *sdkp, struct scsi_device *sdp,
			struct scsi_sense_hdr *sshdr, int sense_valid,
			int the_result)
{
	if (sense_valid)
		sd_print_sense_hdr(sdkp, sshdr);
	else
		sd_printk(KERN_NOTICE, sdkp, "Sense not available.\n");

	 
	if (sdp->removable &&
	    sense_valid && sshdr->sense_key == NOT_READY)
		set_media_not_present(sdkp);

	 
	sdkp->capacity = 0;  
}

#define RC16_LEN 32
#if RC16_LEN > SD_BUF_SIZE
#error RC16_LEN must not be more than SD_BUF_SIZE
#endif

#define READ_CAPACITY_RETRIES_ON_RESET	10

static int read_capacity_16(struct scsi_disk *sdkp, struct scsi_device *sdp,
						unsigned char *buffer)
{
	unsigned char cmd[16];
	struct scsi_sense_hdr sshdr;
	const struct scsi_exec_args exec_args = {
		.sshdr = &sshdr,
	};
	int sense_valid = 0;
	int the_result;
	int retries = 3, reset_retries = READ_CAPACITY_RETRIES_ON_RESET;
	unsigned int alignment;
	unsigned long long lba;
	unsigned sector_size;

	if (sdp->no_read_capacity_16)
		return -EINVAL;

	do {
		memset(cmd, 0, 16);
		cmd[0] = SERVICE_ACTION_IN_16;
		cmd[1] = SAI_READ_CAPACITY_16;
		cmd[13] = RC16_LEN;
		memset(buffer, 0, RC16_LEN);

		the_result = scsi_execute_cmd(sdp, cmd, REQ_OP_DRV_IN,
					      buffer, RC16_LEN, SD_TIMEOUT,
					      sdkp->max_retries, &exec_args);

		if (media_not_present(sdkp, &sshdr))
			return -ENODEV;

		if (the_result > 0) {
			sense_valid = scsi_sense_valid(&sshdr);
			if (sense_valid &&
			    sshdr.sense_key == ILLEGAL_REQUEST &&
			    (sshdr.asc == 0x20 || sshdr.asc == 0x24) &&
			    sshdr.ascq == 0x00)
				 
				return -EINVAL;
			if (sense_valid &&
			    sshdr.sense_key == UNIT_ATTENTION &&
			    sshdr.asc == 0x29 && sshdr.ascq == 0x00)
				 
				if (--reset_retries > 0)
					continue;
		}
		retries--;

	} while (the_result && retries);

	if (the_result) {
		sd_print_result(sdkp, "Read Capacity(16) failed", the_result);
		read_capacity_error(sdkp, sdp, &sshdr, sense_valid, the_result);
		return -EINVAL;
	}

	sector_size = get_unaligned_be32(&buffer[8]);
	lba = get_unaligned_be64(&buffer[0]);

	if (sd_read_protection_type(sdkp, buffer) < 0) {
		sdkp->capacity = 0;
		return -ENODEV;
	}

	 
	sdkp->physical_block_size = (1 << (buffer[13] & 0xf)) * sector_size;

	 
	sdkp->rc_basis = (buffer[12] >> 4) & 0x3;

	 
	alignment = ((buffer[14] & 0x3f) << 8 | buffer[15]) * sector_size;
	blk_queue_alignment_offset(sdp->request_queue, alignment);
	if (alignment && sdkp->first_scan)
		sd_printk(KERN_NOTICE, sdkp,
			  "physical block alignment offset: %u\n", alignment);

	if (buffer[14] & 0x80) {  
		sdkp->lbpme = 1;

		if (buffer[14] & 0x40)  
			sdkp->lbprz = 1;

		sd_config_discard(sdkp, SD_LBP_WS16);
	}

	sdkp->capacity = lba + 1;
	return sector_size;
}

static int read_capacity_10(struct scsi_disk *sdkp, struct scsi_device *sdp,
						unsigned char *buffer)
{
	unsigned char cmd[16];
	struct scsi_sense_hdr sshdr;
	const struct scsi_exec_args exec_args = {
		.sshdr = &sshdr,
	};
	int sense_valid = 0;
	int the_result;
	int retries = 3, reset_retries = READ_CAPACITY_RETRIES_ON_RESET;
	sector_t lba;
	unsigned sector_size;

	do {
		cmd[0] = READ_CAPACITY;
		memset(&cmd[1], 0, 9);
		memset(buffer, 0, 8);

		the_result = scsi_execute_cmd(sdp, cmd, REQ_OP_DRV_IN, buffer,
					      8, SD_TIMEOUT, sdkp->max_retries,
					      &exec_args);

		if (media_not_present(sdkp, &sshdr))
			return -ENODEV;

		if (the_result > 0) {
			sense_valid = scsi_sense_valid(&sshdr);
			if (sense_valid &&
			    sshdr.sense_key == UNIT_ATTENTION &&
			    sshdr.asc == 0x29 && sshdr.ascq == 0x00)
				 
				if (--reset_retries > 0)
					continue;
		}
		retries--;

	} while (the_result && retries);

	if (the_result) {
		sd_print_result(sdkp, "Read Capacity(10) failed", the_result);
		read_capacity_error(sdkp, sdp, &sshdr, sense_valid, the_result);
		return -EINVAL;
	}

	sector_size = get_unaligned_be32(&buffer[4]);
	lba = get_unaligned_be32(&buffer[0]);

	if (sdp->no_read_capacity_16 && (lba == 0xffffffff)) {
		 
		sdkp->capacity = 0;
		sdkp->physical_block_size = sector_size;
		return sector_size;
	}

	sdkp->capacity = lba + 1;
	sdkp->physical_block_size = sector_size;
	return sector_size;
}

static int sd_try_rc16_first(struct scsi_device *sdp)
{
	if (sdp->host->max_cmd_len < 16)
		return 0;
	if (sdp->try_rc_10_first)
		return 0;
	if (sdp->scsi_level > SCSI_SPC_2)
		return 1;
	if (scsi_device_protection(sdp))
		return 1;
	return 0;
}

 
static void
sd_read_capacity(struct scsi_disk *sdkp, unsigned char *buffer)
{
	int sector_size;
	struct scsi_device *sdp = sdkp->device;

	if (sd_try_rc16_first(sdp)) {
		sector_size = read_capacity_16(sdkp, sdp, buffer);
		if (sector_size == -EOVERFLOW)
			goto got_data;
		if (sector_size == -ENODEV)
			return;
		if (sector_size < 0)
			sector_size = read_capacity_10(sdkp, sdp, buffer);
		if (sector_size < 0)
			return;
	} else {
		sector_size = read_capacity_10(sdkp, sdp, buffer);
		if (sector_size == -EOVERFLOW)
			goto got_data;
		if (sector_size < 0)
			return;
		if ((sizeof(sdkp->capacity) > 4) &&
		    (sdkp->capacity > 0xffffffffULL)) {
			int old_sector_size = sector_size;
			sd_printk(KERN_NOTICE, sdkp, "Very big device. "
					"Trying to use READ CAPACITY(16).\n");
			sector_size = read_capacity_16(sdkp, sdp, buffer);
			if (sector_size < 0) {
				sd_printk(KERN_NOTICE, sdkp,
					"Using 0xffffffff as device size\n");
				sdkp->capacity = 1 + (sector_t) 0xffffffff;
				sector_size = old_sector_size;
				goto got_data;
			}
			 
			sdp->try_rc_10_first = 0;
		}
	}

	 
	if (sdp->fix_capacity ||
	    (sdp->guess_capacity && (sdkp->capacity & 0x01))) {
		sd_printk(KERN_INFO, sdkp, "Adjusting the sector count "
				"from its reported value: %llu\n",
				(unsigned long long) sdkp->capacity);
		--sdkp->capacity;
	}

got_data:
	if (sector_size == 0) {
		sector_size = 512;
		sd_printk(KERN_NOTICE, sdkp, "Sector size 0 reported, "
			  "assuming 512.\n");
	}

	if (sector_size != 512 &&
	    sector_size != 1024 &&
	    sector_size != 2048 &&
	    sector_size != 4096) {
		sd_printk(KERN_NOTICE, sdkp, "Unsupported sector size %d.\n",
			  sector_size);
		 
		sdkp->capacity = 0;
		 
		sector_size = 512;
	}
	blk_queue_logical_block_size(sdp->request_queue, sector_size);
	blk_queue_physical_block_size(sdp->request_queue,
				      sdkp->physical_block_size);
	sdkp->device->sector_size = sector_size;

	if (sdkp->capacity > 0xffffffff)
		sdp->use_16_for_rw = 1;

}

 
static void
sd_print_capacity(struct scsi_disk *sdkp,
		  sector_t old_capacity)
{
	int sector_size = sdkp->device->sector_size;
	char cap_str_2[10], cap_str_10[10];

	if (!sdkp->first_scan && old_capacity == sdkp->capacity)
		return;

	string_get_size(sdkp->capacity, sector_size,
			STRING_UNITS_2, cap_str_2, sizeof(cap_str_2));
	string_get_size(sdkp->capacity, sector_size,
			STRING_UNITS_10, cap_str_10, sizeof(cap_str_10));

	sd_printk(KERN_NOTICE, sdkp,
		  "%llu %d-byte logical blocks: (%s/%s)\n",
		  (unsigned long long)sdkp->capacity,
		  sector_size, cap_str_10, cap_str_2);

	if (sdkp->physical_block_size != sector_size)
		sd_printk(KERN_NOTICE, sdkp,
			  "%u-byte physical blocks\n",
			  sdkp->physical_block_size);
}

 
static inline int
sd_do_mode_sense(struct scsi_disk *sdkp, int dbd, int modepage,
		 unsigned char *buffer, int len, struct scsi_mode_data *data,
		 struct scsi_sense_hdr *sshdr)
{
	 
	if (sdkp->device->use_10_for_ms && len < 8)
		len = 8;

	return scsi_mode_sense(sdkp->device, dbd, modepage, 0, buffer, len,
			       SD_TIMEOUT, sdkp->max_retries, data, sshdr);
}

 
static void
sd_read_write_protect_flag(struct scsi_disk *sdkp, unsigned char *buffer)
{
	int res;
	struct scsi_device *sdp = sdkp->device;
	struct scsi_mode_data data;
	int old_wp = sdkp->write_prot;

	set_disk_ro(sdkp->disk, 0);
	if (sdp->skip_ms_page_3f) {
		sd_first_printk(KERN_NOTICE, sdkp, "Assuming Write Enabled\n");
		return;
	}

	if (sdp->use_192_bytes_for_3f) {
		res = sd_do_mode_sense(sdkp, 0, 0x3F, buffer, 192, &data, NULL);
	} else {
		 
		res = sd_do_mode_sense(sdkp, 0, 0x3F, buffer, 4, &data, NULL);

		 
		if (res < 0)
			res = sd_do_mode_sense(sdkp, 0, 0, buffer, 4, &data, NULL);

		 
		if (res < 0)
			res = sd_do_mode_sense(sdkp, 0, 0x3F, buffer, 255,
					       &data, NULL);
	}

	if (res < 0) {
		sd_first_printk(KERN_WARNING, sdkp,
			  "Test WP failed, assume Write Enabled\n");
	} else {
		sdkp->write_prot = ((data.device_specific & 0x80) != 0);
		set_disk_ro(sdkp->disk, sdkp->write_prot);
		if (sdkp->first_scan || old_wp != sdkp->write_prot) {
			sd_printk(KERN_NOTICE, sdkp, "Write Protect is %s\n",
				  sdkp->write_prot ? "on" : "off");
			sd_printk(KERN_DEBUG, sdkp, "Mode Sense: %4ph\n", buffer);
		}
	}
}

 
static void
sd_read_cache_type(struct scsi_disk *sdkp, unsigned char *buffer)
{
	int len = 0, res;
	struct scsi_device *sdp = sdkp->device;

	int dbd;
	int modepage;
	int first_len;
	struct scsi_mode_data data;
	struct scsi_sense_hdr sshdr;
	int old_wce = sdkp->WCE;
	int old_rcd = sdkp->RCD;
	int old_dpofua = sdkp->DPOFUA;


	if (sdkp->cache_override)
		return;

	first_len = 4;
	if (sdp->skip_ms_page_8) {
		if (sdp->type == TYPE_RBC)
			goto defaults;
		else {
			if (sdp->skip_ms_page_3f)
				goto defaults;
			modepage = 0x3F;
			if (sdp->use_192_bytes_for_3f)
				first_len = 192;
			dbd = 0;
		}
	} else if (sdp->type == TYPE_RBC) {
		modepage = 6;
		dbd = 8;
	} else {
		modepage = 8;
		dbd = 0;
	}

	 
	res = sd_do_mode_sense(sdkp, dbd, modepage, buffer, first_len,
			&data, &sshdr);

	if (res < 0)
		goto bad_sense;

	if (!data.header_length) {
		modepage = 6;
		first_len = 0;
		sd_first_printk(KERN_ERR, sdkp,
				"Missing header in MODE_SENSE response\n");
	}

	 
	len = data.length;

	 
	if (len < 3)
		goto bad_sense;
	else if (len > SD_BUF_SIZE) {
		sd_first_printk(KERN_NOTICE, sdkp, "Truncating mode parameter "
			  "data from %d to %d bytes\n", len, SD_BUF_SIZE);
		len = SD_BUF_SIZE;
	}
	if (modepage == 0x3F && sdp->use_192_bytes_for_3f)
		len = 192;

	 
	if (len > first_len)
		res = sd_do_mode_sense(sdkp, dbd, modepage, buffer, len,
				&data, &sshdr);

	if (!res) {
		int offset = data.header_length + data.block_descriptor_length;

		while (offset < len) {
			u8 page_code = buffer[offset] & 0x3F;
			u8 spf       = buffer[offset] & 0x40;

			if (page_code == 8 || page_code == 6) {
				 
				if (len - offset <= 2) {
					sd_first_printk(KERN_ERR, sdkp,
						"Incomplete mode parameter "
							"data\n");
					goto defaults;
				} else {
					modepage = page_code;
					goto Page_found;
				}
			} else {
				 
				if (spf && len - offset > 3)
					offset += 4 + (buffer[offset+2] << 8) +
						buffer[offset+3];
				else if (!spf && len - offset > 1)
					offset += 2 + buffer[offset+1];
				else {
					sd_first_printk(KERN_ERR, sdkp,
							"Incomplete mode "
							"parameter data\n");
					goto defaults;
				}
			}
		}

		sd_first_printk(KERN_WARNING, sdkp,
				"No Caching mode page found\n");
		goto defaults;

	Page_found:
		if (modepage == 8) {
			sdkp->WCE = ((buffer[offset + 2] & 0x04) != 0);
			sdkp->RCD = ((buffer[offset + 2] & 0x01) != 0);
		} else {
			sdkp->WCE = ((buffer[offset + 2] & 0x01) == 0);
			sdkp->RCD = 0;
		}

		sdkp->DPOFUA = (data.device_specific & 0x10) != 0;
		if (sdp->broken_fua) {
			sd_first_printk(KERN_NOTICE, sdkp, "Disabling FUA\n");
			sdkp->DPOFUA = 0;
		} else if (sdkp->DPOFUA && !sdkp->device->use_10_for_rw &&
			   !sdkp->device->use_16_for_rw) {
			sd_first_printk(KERN_NOTICE, sdkp,
				  "Uses READ/WRITE(6), disabling FUA\n");
			sdkp->DPOFUA = 0;
		}

		 
		if (sdkp->WCE && sdkp->write_prot)
			sdkp->WCE = 0;

		if (sdkp->first_scan || old_wce != sdkp->WCE ||
		    old_rcd != sdkp->RCD || old_dpofua != sdkp->DPOFUA)
			sd_printk(KERN_NOTICE, sdkp,
				  "Write cache: %s, read cache: %s, %s\n",
				  sdkp->WCE ? "enabled" : "disabled",
				  sdkp->RCD ? "disabled" : "enabled",
				  sdkp->DPOFUA ? "supports DPO and FUA"
				  : "doesn't support DPO or FUA");

		return;
	}

bad_sense:
	if (scsi_sense_valid(&sshdr) &&
	    sshdr.sense_key == ILLEGAL_REQUEST &&
	    sshdr.asc == 0x24 && sshdr.ascq == 0x0)
		 
		sd_first_printk(KERN_NOTICE, sdkp, "Cache data unavailable\n");
	else
		sd_first_printk(KERN_ERR, sdkp,
				"Asking for cache data failed\n");

defaults:
	if (sdp->wce_default_on) {
		sd_first_printk(KERN_NOTICE, sdkp,
				"Assuming drive cache: write back\n");
		sdkp->WCE = 1;
	} else {
		sd_first_printk(KERN_WARNING, sdkp,
				"Assuming drive cache: write through\n");
		sdkp->WCE = 0;
	}
	sdkp->RCD = 0;
	sdkp->DPOFUA = 0;
}

 
static void sd_read_app_tag_own(struct scsi_disk *sdkp, unsigned char *buffer)
{
	int res, offset;
	struct scsi_device *sdp = sdkp->device;
	struct scsi_mode_data data;
	struct scsi_sense_hdr sshdr;

	if (sdp->type != TYPE_DISK && sdp->type != TYPE_ZBC)
		return;

	if (sdkp->protection_type == 0)
		return;

	res = scsi_mode_sense(sdp, 1, 0x0a, 0, buffer, 36, SD_TIMEOUT,
			      sdkp->max_retries, &data, &sshdr);

	if (res < 0 || !data.header_length ||
	    data.length < 6) {
		sd_first_printk(KERN_WARNING, sdkp,
			  "getting Control mode page failed, assume no ATO\n");

		if (scsi_sense_valid(&sshdr))
			sd_print_sense_hdr(sdkp, &sshdr);

		return;
	}

	offset = data.header_length + data.block_descriptor_length;

	if ((buffer[offset] & 0x3f) != 0x0a) {
		sd_first_printk(KERN_ERR, sdkp, "ATO Got wrong page\n");
		return;
	}

	if ((buffer[offset + 5] & 0x80) == 0)
		return;

	sdkp->ATO = 1;

	return;
}

 
static void sd_read_block_limits(struct scsi_disk *sdkp)
{
	struct scsi_vpd *vpd;

	rcu_read_lock();

	vpd = rcu_dereference(sdkp->device->vpd_pgb0);
	if (!vpd || vpd->len < 16)
		goto out;

	sdkp->min_xfer_blocks = get_unaligned_be16(&vpd->data[6]);
	sdkp->max_xfer_blocks = get_unaligned_be32(&vpd->data[8]);
	sdkp->opt_xfer_blocks = get_unaligned_be32(&vpd->data[12]);

	if (vpd->len >= 64) {
		unsigned int lba_count, desc_count;

		sdkp->max_ws_blocks = (u32)get_unaligned_be64(&vpd->data[36]);

		if (!sdkp->lbpme)
			goto out;

		lba_count = get_unaligned_be32(&vpd->data[20]);
		desc_count = get_unaligned_be32(&vpd->data[24]);

		if (lba_count && desc_count)
			sdkp->max_unmap_blocks = lba_count;

		sdkp->unmap_granularity = get_unaligned_be32(&vpd->data[28]);

		if (vpd->data[32] & 0x80)
			sdkp->unmap_alignment =
				get_unaligned_be32(&vpd->data[32]) & ~(1 << 31);

		if (!sdkp->lbpvpd) {  

			if (sdkp->max_unmap_blocks)
				sd_config_discard(sdkp, SD_LBP_UNMAP);
			else
				sd_config_discard(sdkp, SD_LBP_WS16);

		} else {	 
			if (sdkp->lbpu && sdkp->max_unmap_blocks)
				sd_config_discard(sdkp, SD_LBP_UNMAP);
			else if (sdkp->lbpws)
				sd_config_discard(sdkp, SD_LBP_WS16);
			else if (sdkp->lbpws10)
				sd_config_discard(sdkp, SD_LBP_WS10);
			else
				sd_config_discard(sdkp, SD_LBP_DISABLE);
		}
	}

 out:
	rcu_read_unlock();
}

 
static void sd_read_block_characteristics(struct scsi_disk *sdkp)
{
	struct request_queue *q = sdkp->disk->queue;
	struct scsi_vpd *vpd;
	u16 rot;
	u8 zoned;

	rcu_read_lock();
	vpd = rcu_dereference(sdkp->device->vpd_pgb1);

	if (!vpd || vpd->len < 8) {
		rcu_read_unlock();
	        return;
	}

	rot = get_unaligned_be16(&vpd->data[4]);
	zoned = (vpd->data[8] >> 4) & 3;
	rcu_read_unlock();

	if (rot == 1) {
		blk_queue_flag_set(QUEUE_FLAG_NONROT, q);
		blk_queue_flag_clear(QUEUE_FLAG_ADD_RANDOM, q);
	}

	if (sdkp->device->type == TYPE_ZBC) {
		 
		disk_set_zoned(sdkp->disk, BLK_ZONED_HM);
		blk_queue_zone_write_granularity(q, sdkp->physical_block_size);
	} else {
		sdkp->zoned = zoned;
		if (sdkp->zoned == 1) {
			 
			disk_set_zoned(sdkp->disk, BLK_ZONED_HA);
		} else {
			 
			disk_set_zoned(sdkp->disk, BLK_ZONED_NONE);
		}
	}

	if (!sdkp->first_scan)
		return;

	if (blk_queue_is_zoned(q)) {
		sd_printk(KERN_NOTICE, sdkp, "Host-%s zoned block device\n",
		      q->limits.zoned == BLK_ZONED_HM ? "managed" : "aware");
	} else {
		if (sdkp->zoned == 1)
			sd_printk(KERN_NOTICE, sdkp,
				  "Host-aware SMR disk used as regular disk\n");
		else if (sdkp->zoned == 2)
			sd_printk(KERN_NOTICE, sdkp,
				  "Drive-managed SMR disk\n");
	}
}

 
static void sd_read_block_provisioning(struct scsi_disk *sdkp)
{
	struct scsi_vpd *vpd;

	if (sdkp->lbpme == 0)
		return;

	rcu_read_lock();
	vpd = rcu_dereference(sdkp->device->vpd_pgb2);

	if (!vpd || vpd->len < 8) {
		rcu_read_unlock();
		return;
	}

	sdkp->lbpvpd	= 1;
	sdkp->lbpu	= (vpd->data[5] >> 7) & 1;  
	sdkp->lbpws	= (vpd->data[5] >> 6) & 1;  
	sdkp->lbpws10	= (vpd->data[5] >> 5) & 1;  
	rcu_read_unlock();
}

static void sd_read_write_same(struct scsi_disk *sdkp, unsigned char *buffer)
{
	struct scsi_device *sdev = sdkp->device;

	if (sdev->host->no_write_same) {
		sdev->no_write_same = 1;

		return;
	}

	if (scsi_report_opcode(sdev, buffer, SD_BUF_SIZE, INQUIRY, 0) < 0) {
		struct scsi_vpd *vpd;

		sdev->no_report_opcodes = 1;

		 
		rcu_read_lock();
		vpd = rcu_dereference(sdev->vpd_pg89);
		if (vpd)
			sdev->no_write_same = 1;
		rcu_read_unlock();
	}

	if (scsi_report_opcode(sdev, buffer, SD_BUF_SIZE, WRITE_SAME_16, 0) == 1)
		sdkp->ws16 = 1;

	if (scsi_report_opcode(sdev, buffer, SD_BUF_SIZE, WRITE_SAME, 0) == 1)
		sdkp->ws10 = 1;
}

static void sd_read_security(struct scsi_disk *sdkp, unsigned char *buffer)
{
	struct scsi_device *sdev = sdkp->device;

	if (!sdev->security_supported)
		return;

	if (scsi_report_opcode(sdev, buffer, SD_BUF_SIZE,
			SECURITY_PROTOCOL_IN, 0) == 1 &&
	    scsi_report_opcode(sdev, buffer, SD_BUF_SIZE,
			SECURITY_PROTOCOL_OUT, 0) == 1)
		sdkp->security = 1;
}

static inline sector_t sd64_to_sectors(struct scsi_disk *sdkp, u8 *buf)
{
	return logical_to_sectors(sdkp->device, get_unaligned_be64(buf));
}

 
static void sd_read_cpr(struct scsi_disk *sdkp)
{
	struct blk_independent_access_ranges *iars = NULL;
	unsigned char *buffer = NULL;
	unsigned int nr_cpr = 0;
	int i, vpd_len, buf_len = SD_BUF_SIZE;
	u8 *desc;

	 
	if (sdkp->first_scan)
		return;

	if (!sdkp->capacity)
		goto out;

	 
	buf_len = 64 + 256*32;
	buffer = kmalloc(buf_len, GFP_KERNEL);
	if (!buffer || scsi_get_vpd_page(sdkp->device, 0xb9, buffer, buf_len))
		goto out;

	 
	vpd_len = get_unaligned_be16(&buffer[2]) + 4;
	if (vpd_len > buf_len || vpd_len < 64 + 32 || (vpd_len & 31)) {
		sd_printk(KERN_ERR, sdkp,
			  "Invalid Concurrent Positioning Ranges VPD page\n");
		goto out;
	}

	nr_cpr = (vpd_len - 64) / 32;
	if (nr_cpr == 1) {
		nr_cpr = 0;
		goto out;
	}

	iars = disk_alloc_independent_access_ranges(sdkp->disk, nr_cpr);
	if (!iars) {
		nr_cpr = 0;
		goto out;
	}

	desc = &buffer[64];
	for (i = 0; i < nr_cpr; i++, desc += 32) {
		if (desc[0] != i) {
			sd_printk(KERN_ERR, sdkp,
				"Invalid Concurrent Positioning Range number\n");
			nr_cpr = 0;
			break;
		}

		iars->ia_range[i].sector = sd64_to_sectors(sdkp, desc + 8);
		iars->ia_range[i].nr_sectors = sd64_to_sectors(sdkp, desc + 16);
	}

out:
	disk_set_independent_access_ranges(sdkp->disk, iars);
	if (nr_cpr && sdkp->nr_actuators != nr_cpr) {
		sd_printk(KERN_NOTICE, sdkp,
			  "%u concurrent positioning ranges\n", nr_cpr);
		sdkp->nr_actuators = nr_cpr;
	}

	kfree(buffer);
}

static bool sd_validate_min_xfer_size(struct scsi_disk *sdkp)
{
	struct scsi_device *sdp = sdkp->device;
	unsigned int min_xfer_bytes =
		logical_to_bytes(sdp, sdkp->min_xfer_blocks);

	if (sdkp->min_xfer_blocks == 0)
		return false;

	if (min_xfer_bytes & (sdkp->physical_block_size - 1)) {
		sd_first_printk(KERN_WARNING, sdkp,
				"Preferred minimum I/O size %u bytes not a " \
				"multiple of physical block size (%u bytes)\n",
				min_xfer_bytes, sdkp->physical_block_size);
		sdkp->min_xfer_blocks = 0;
		return false;
	}

	sd_first_printk(KERN_INFO, sdkp, "Preferred minimum I/O size %u bytes\n",
			min_xfer_bytes);
	return true;
}

 
static bool sd_validate_opt_xfer_size(struct scsi_disk *sdkp,
				      unsigned int dev_max)
{
	struct scsi_device *sdp = sdkp->device;
	unsigned int opt_xfer_bytes =
		logical_to_bytes(sdp, sdkp->opt_xfer_blocks);
	unsigned int min_xfer_bytes =
		logical_to_bytes(sdp, sdkp->min_xfer_blocks);

	if (sdkp->opt_xfer_blocks == 0)
		return false;

	if (sdkp->opt_xfer_blocks > dev_max) {
		sd_first_printk(KERN_WARNING, sdkp,
				"Optimal transfer size %u logical blocks " \
				"> dev_max (%u logical blocks)\n",
				sdkp->opt_xfer_blocks, dev_max);
		return false;
	}

	if (sdkp->opt_xfer_blocks > SD_DEF_XFER_BLOCKS) {
		sd_first_printk(KERN_WARNING, sdkp,
				"Optimal transfer size %u logical blocks " \
				"> sd driver limit (%u logical blocks)\n",
				sdkp->opt_xfer_blocks, SD_DEF_XFER_BLOCKS);
		return false;
	}

	if (opt_xfer_bytes < PAGE_SIZE) {
		sd_first_printk(KERN_WARNING, sdkp,
				"Optimal transfer size %u bytes < " \
				"PAGE_SIZE (%u bytes)\n",
				opt_xfer_bytes, (unsigned int)PAGE_SIZE);
		return false;
	}

	if (min_xfer_bytes && opt_xfer_bytes % min_xfer_bytes) {
		sd_first_printk(KERN_WARNING, sdkp,
				"Optimal transfer size %u bytes not a " \
				"multiple of preferred minimum block " \
				"size (%u bytes)\n",
				opt_xfer_bytes, min_xfer_bytes);
		return false;
	}

	if (opt_xfer_bytes & (sdkp->physical_block_size - 1)) {
		sd_first_printk(KERN_WARNING, sdkp,
				"Optimal transfer size %u bytes not a " \
				"multiple of physical block size (%u bytes)\n",
				opt_xfer_bytes, sdkp->physical_block_size);
		return false;
	}

	sd_first_printk(KERN_INFO, sdkp, "Optimal transfer size %u bytes\n",
			opt_xfer_bytes);
	return true;
}

 
static int sd_revalidate_disk(struct gendisk *disk)
{
	struct scsi_disk *sdkp = scsi_disk(disk);
	struct scsi_device *sdp = sdkp->device;
	struct request_queue *q = sdkp->disk->queue;
	sector_t old_capacity = sdkp->capacity;
	unsigned char *buffer;
	unsigned int dev_max, rw_max;

	SCSI_LOG_HLQUEUE(3, sd_printk(KERN_INFO, sdkp,
				      "sd_revalidate_disk\n"));

	 
	if (!scsi_device_online(sdp))
		goto out;

	buffer = kmalloc(SD_BUF_SIZE, GFP_KERNEL);
	if (!buffer) {
		sd_printk(KERN_WARNING, sdkp, "sd_revalidate_disk: Memory "
			  "allocation failure.\n");
		goto out;
	}

	sd_spinup_disk(sdkp);

	 
	if (sdkp->media_present) {
		sd_read_capacity(sdkp, buffer);

		 
		blk_queue_flag_clear(QUEUE_FLAG_NONROT, q);
		blk_queue_flag_set(QUEUE_FLAG_ADD_RANDOM, q);

		if (scsi_device_supports_vpd(sdp)) {
			sd_read_block_provisioning(sdkp);
			sd_read_block_limits(sdkp);
			sd_read_block_characteristics(sdkp);
			sd_zbc_read_zones(sdkp, buffer);
			sd_read_cpr(sdkp);
		}

		sd_print_capacity(sdkp, old_capacity);

		sd_read_write_protect_flag(sdkp, buffer);
		sd_read_cache_type(sdkp, buffer);
		sd_read_app_tag_own(sdkp, buffer);
		sd_read_write_same(sdkp, buffer);
		sd_read_security(sdkp, buffer);
		sd_config_protection(sdkp);
	}

	 
	sd_set_flush_flag(sdkp);

	 
	dev_max = sdp->use_16_for_rw ? SD_MAX_XFER_BLOCKS : SD_DEF_XFER_BLOCKS;

	 
	dev_max = min_not_zero(dev_max, sdkp->max_xfer_blocks);
	q->limits.max_dev_sectors = logical_to_sectors(sdp, dev_max);

	if (sd_validate_min_xfer_size(sdkp))
		blk_queue_io_min(sdkp->disk->queue,
				 logical_to_bytes(sdp, sdkp->min_xfer_blocks));
	else
		blk_queue_io_min(sdkp->disk->queue, 0);

	if (sd_validate_opt_xfer_size(sdkp, dev_max)) {
		q->limits.io_opt = logical_to_bytes(sdp, sdkp->opt_xfer_blocks);
		rw_max = logical_to_sectors(sdp, sdkp->opt_xfer_blocks);
	} else {
		q->limits.io_opt = 0;
		rw_max = min_not_zero(logical_to_sectors(sdp, dev_max),
				      (sector_t)BLK_DEF_MAX_SECTORS);
	}

	 
	rw_max = min_not_zero(rw_max, sdp->host->opt_sectors);

	 
	rw_max = min(rw_max, queue_max_hw_sectors(q));

	 
	if (sdkp->first_scan ||
	    q->limits.max_sectors > q->limits.max_dev_sectors ||
	    q->limits.max_sectors > q->limits.max_hw_sectors)
		q->limits.max_sectors = rw_max;

	sdkp->first_scan = 0;

	set_capacity_and_notify(disk, logical_to_sectors(sdp, sdkp->capacity));
	sd_config_write_same(sdkp);
	kfree(buffer);

	 
	if (sd_zbc_revalidate_zones(sdkp))
		set_capacity_and_notify(disk, 0);

 out:
	return 0;
}

 
static void sd_unlock_native_capacity(struct gendisk *disk)
{
	struct scsi_device *sdev = scsi_disk(disk)->device;

	if (sdev->host->hostt->unlock_native_capacity)
		sdev->host->hostt->unlock_native_capacity(sdev);
}

 
static int sd_format_disk_name(char *prefix, int index, char *buf, int buflen)
{
	const int base = 'z' - 'a' + 1;
	char *begin = buf + strlen(prefix);
	char *end = buf + buflen;
	char *p;
	int unit;

	p = end - 1;
	*p = '\0';
	unit = base;
	do {
		if (p == begin)
			return -EINVAL;
		*--p = 'a' + (index % unit);
		index = (index / unit) - 1;
	} while (index >= 0);

	memmove(begin, p, end - p);
	memcpy(buf, prefix, strlen(prefix));

	return 0;
}

 
static int sd_probe(struct device *dev)
{
	struct scsi_device *sdp = to_scsi_device(dev);
	struct scsi_disk *sdkp;
	struct gendisk *gd;
	int index;
	int error;

	scsi_autopm_get_device(sdp);
	error = -ENODEV;
	if (sdp->type != TYPE_DISK &&
	    sdp->type != TYPE_ZBC &&
	    sdp->type != TYPE_MOD &&
	    sdp->type != TYPE_RBC)
		goto out;

	if (!IS_ENABLED(CONFIG_BLK_DEV_ZONED) && sdp->type == TYPE_ZBC) {
		sdev_printk(KERN_WARNING, sdp,
			    "Unsupported ZBC host-managed device.\n");
		goto out;
	}

	SCSI_LOG_HLQUEUE(3, sdev_printk(KERN_INFO, sdp,
					"sd_probe\n"));

	error = -ENOMEM;
	sdkp = kzalloc(sizeof(*sdkp), GFP_KERNEL);
	if (!sdkp)
		goto out;

	gd = blk_mq_alloc_disk_for_queue(sdp->request_queue,
					 &sd_bio_compl_lkclass);
	if (!gd)
		goto out_free;

	index = ida_alloc(&sd_index_ida, GFP_KERNEL);
	if (index < 0) {
		sdev_printk(KERN_WARNING, sdp, "sd_probe: memory exhausted.\n");
		goto out_put;
	}

	error = sd_format_disk_name("sd", index, gd->disk_name, DISK_NAME_LEN);
	if (error) {
		sdev_printk(KERN_WARNING, sdp, "SCSI disk (sd) name length exceeded.\n");
		goto out_free_index;
	}

	sdkp->device = sdp;
	sdkp->disk = gd;
	sdkp->index = index;
	sdkp->max_retries = SD_MAX_RETRIES;
	atomic_set(&sdkp->openers, 0);
	atomic_set(&sdkp->device->ioerr_cnt, 0);

	if (!sdp->request_queue->rq_timeout) {
		if (sdp->type != TYPE_MOD)
			blk_queue_rq_timeout(sdp->request_queue, SD_TIMEOUT);
		else
			blk_queue_rq_timeout(sdp->request_queue,
					     SD_MOD_TIMEOUT);
	}

	device_initialize(&sdkp->disk_dev);
	sdkp->disk_dev.parent = get_device(dev);
	sdkp->disk_dev.class = &sd_disk_class;
	dev_set_name(&sdkp->disk_dev, "%s", dev_name(dev));

	error = device_add(&sdkp->disk_dev);
	if (error) {
		put_device(&sdkp->disk_dev);
		goto out;
	}

	dev_set_drvdata(dev, sdkp);

	gd->major = sd_major((index & 0xf0) >> 4);
	gd->first_minor = ((index & 0xf) << 4) | (index & 0xfff00);
	gd->minors = SD_MINORS;

	gd->fops = &sd_fops;
	gd->private_data = sdkp;

	 
	sdp->sector_size = 512;
	sdkp->capacity = 0;
	sdkp->media_present = 1;
	sdkp->write_prot = 0;
	sdkp->cache_override = 0;
	sdkp->WCE = 0;
	sdkp->RCD = 0;
	sdkp->ATO = 0;
	sdkp->first_scan = 1;
	sdkp->max_medium_access_timeouts = SD_MAX_MEDIUM_TIMEOUTS;

	sd_revalidate_disk(gd);

	if (sdp->removable) {
		gd->flags |= GENHD_FL_REMOVABLE;
		gd->events |= DISK_EVENT_MEDIA_CHANGE;
		gd->event_flags = DISK_EVENT_FLAG_POLL | DISK_EVENT_FLAG_UEVENT;
	}

	blk_pm_runtime_init(sdp->request_queue, dev);
	if (sdp->rpm_autosuspend) {
		pm_runtime_set_autosuspend_delay(dev,
			sdp->host->hostt->rpm_autosuspend_delay);
	}

	error = device_add_disk(dev, gd, NULL);
	if (error) {
		put_device(&sdkp->disk_dev);
		put_disk(gd);
		goto out;
	}

	if (sdkp->security) {
		sdkp->opal_dev = init_opal_dev(sdkp, &sd_sec_submit);
		if (sdkp->opal_dev)
			sd_printk(KERN_NOTICE, sdkp, "supports TCG Opal\n");
	}

	sd_printk(KERN_NOTICE, sdkp, "Attached SCSI %sdisk\n",
		  sdp->removable ? "removable " : "");
	scsi_autopm_put_device(sdp);

	return 0;

 out_free_index:
	ida_free(&sd_index_ida, index);
 out_put:
	put_disk(gd);
 out_free:
	kfree(sdkp);
 out:
	scsi_autopm_put_device(sdp);
	return error;
}

 
static int sd_remove(struct device *dev)
{
	struct scsi_disk *sdkp = dev_get_drvdata(dev);

	scsi_autopm_get_device(sdkp->device);

	device_del(&sdkp->disk_dev);
	del_gendisk(sdkp->disk);
	if (!sdkp->suspended)
		sd_shutdown(dev);

	put_disk(sdkp->disk);
	return 0;
}

static void scsi_disk_release(struct device *dev)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	ida_free(&sd_index_ida, sdkp->index);
	sd_zbc_free_zone_info(sdkp);
	put_device(&sdkp->device->sdev_gendev);
	free_opal_dev(sdkp->opal_dev);

	kfree(sdkp);
}

static int sd_start_stop_device(struct scsi_disk *sdkp, int start)
{
	unsigned char cmd[6] = { START_STOP };	 
	struct scsi_sense_hdr sshdr;
	const struct scsi_exec_args exec_args = {
		.sshdr = &sshdr,
		.req_flags = BLK_MQ_REQ_PM,
	};
	struct scsi_device *sdp = sdkp->device;
	int res;

	if (start)
		cmd[4] |= 1;	 

	if (sdp->start_stop_pwr_cond)
		cmd[4] |= start ? 1 << 4 : 3 << 4;	 

	if (!scsi_device_online(sdp))
		return -ENODEV;

	res = scsi_execute_cmd(sdp, cmd, REQ_OP_DRV_IN, NULL, 0, SD_TIMEOUT,
			       sdkp->max_retries, &exec_args);
	if (res) {
		sd_print_result(sdkp, "Start/Stop Unit failed", res);
		if (res > 0 && scsi_sense_valid(&sshdr)) {
			sd_print_sense_hdr(sdkp, &sshdr);
			 
			if (sshdr.asc == 0x3a)
				res = 0;
		}
	}

	 
	if (res)
		return -EIO;

	return 0;
}

 
static void sd_shutdown(struct device *dev)
{
	struct scsi_disk *sdkp = dev_get_drvdata(dev);

	if (!sdkp)
		return;          

	if (pm_runtime_suspended(dev))
		return;

	if (sdkp->WCE && sdkp->media_present) {
		sd_printk(KERN_NOTICE, sdkp, "Synchronizing SCSI cache\n");
		sd_sync_cache(sdkp);
	}

	if ((system_state != SYSTEM_RESTART &&
	     sdkp->device->manage_system_start_stop) ||
	    (system_state == SYSTEM_POWER_OFF &&
	     sdkp->device->manage_shutdown)) {
		sd_printk(KERN_NOTICE, sdkp, "Stopping disk\n");
		sd_start_stop_device(sdkp, 0);
	}
}

static inline bool sd_do_start_stop(struct scsi_device *sdev, bool runtime)
{
	return (sdev->manage_system_start_stop && !runtime) ||
		(sdev->manage_runtime_start_stop && runtime);
}

static int sd_suspend_common(struct device *dev, bool runtime)
{
	struct scsi_disk *sdkp = dev_get_drvdata(dev);
	int ret = 0;

	if (!sdkp)	 
		return 0;

	if (sdkp->WCE && sdkp->media_present) {
		if (!sdkp->device->silence_suspend)
			sd_printk(KERN_NOTICE, sdkp, "Synchronizing SCSI cache\n");
		ret = sd_sync_cache(sdkp);
		 
		if (ret == -ENODEV)
			return 0;

		if (ret)
			return ret;
	}

	if (sd_do_start_stop(sdkp->device, runtime)) {
		if (!sdkp->device->silence_suspend)
			sd_printk(KERN_NOTICE, sdkp, "Stopping disk\n");
		 
		ret = sd_start_stop_device(sdkp, 0);
		if (!runtime)
			ret = 0;
	}

	if (!ret)
		sdkp->suspended = true;

	return ret;
}

static int sd_suspend_system(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 0;

	return sd_suspend_common(dev, false);
}

static int sd_suspend_runtime(struct device *dev)
{
	return sd_suspend_common(dev, true);
}

static int sd_resume(struct device *dev, bool runtime)
{
	struct scsi_disk *sdkp = dev_get_drvdata(dev);
	int ret = 0;

	if (!sdkp)	 
		return 0;

	if (!sd_do_start_stop(sdkp->device, runtime)) {
		sdkp->suspended = false;
		return 0;
	}

	if (!sdkp->device->no_start_on_resume) {
		sd_printk(KERN_NOTICE, sdkp, "Starting disk\n");
		ret = sd_start_stop_device(sdkp, 1);
	}

	if (!ret) {
		opal_unlock_from_suspend(sdkp->opal_dev);
		sdkp->suspended = false;
	}

	return ret;
}

static int sd_resume_system(struct device *dev)
{
	if (pm_runtime_suspended(dev)) {
		struct scsi_disk *sdkp = dev_get_drvdata(dev);
		struct scsi_device *sdp = sdkp ? sdkp->device : NULL;

		if (sdp && sdp->force_runtime_start_on_system_start)
			pm_request_resume(dev);

		return 0;
	}

	return sd_resume(dev, false);
}

static int sd_resume_runtime(struct device *dev)
{
	struct scsi_disk *sdkp = dev_get_drvdata(dev);
	struct scsi_device *sdp;

	if (!sdkp)	 
		return 0;

	sdp = sdkp->device;

	if (sdp->ignore_media_change) {
		 
		static const u8 cmd[10] = { REQUEST_SENSE };
		const struct scsi_exec_args exec_args = {
			.req_flags = BLK_MQ_REQ_PM,
		};

		if (scsi_execute_cmd(sdp, cmd, REQ_OP_DRV_IN, NULL, 0,
				     sdp->request_queue->rq_timeout, 1,
				     &exec_args))
			sd_printk(KERN_NOTICE, sdkp,
				  "Failed to clear sense data\n");
	}

	return sd_resume(dev, true);
}

static const struct dev_pm_ops sd_pm_ops = {
	.suspend		= sd_suspend_system,
	.resume			= sd_resume_system,
	.poweroff		= sd_suspend_system,
	.restore		= sd_resume_system,
	.runtime_suspend	= sd_suspend_runtime,
	.runtime_resume		= sd_resume_runtime,
};

static struct scsi_driver sd_template = {
	.gendrv = {
		.name		= "sd",
		.owner		= THIS_MODULE,
		.probe		= sd_probe,
		.probe_type	= PROBE_PREFER_ASYNCHRONOUS,
		.remove		= sd_remove,
		.shutdown	= sd_shutdown,
		.pm		= &sd_pm_ops,
	},
	.rescan			= sd_rescan,
	.init_command		= sd_init_command,
	.uninit_command		= sd_uninit_command,
	.done			= sd_done,
	.eh_action		= sd_eh_action,
	.eh_reset		= sd_eh_reset,
};

 
static int __init init_sd(void)
{
	int majors = 0, i, err;

	SCSI_LOG_HLQUEUE(3, printk("init_sd: sd driver entry point\n"));

	for (i = 0; i < SD_MAJORS; i++) {
		if (__register_blkdev(sd_major(i), "sd", sd_default_probe))
			continue;
		majors++;
	}

	if (!majors)
		return -ENODEV;

	err = class_register(&sd_disk_class);
	if (err)
		goto err_out;

	sd_page_pool = mempool_create_page_pool(SD_MEMPOOL_SIZE, 0);
	if (!sd_page_pool) {
		printk(KERN_ERR "sd: can't init discard page pool\n");
		err = -ENOMEM;
		goto err_out_class;
	}

	err = scsi_register_driver(&sd_template.gendrv);
	if (err)
		goto err_out_driver;

	return 0;

err_out_driver:
	mempool_destroy(sd_page_pool);
err_out_class:
	class_unregister(&sd_disk_class);
err_out:
	for (i = 0; i < SD_MAJORS; i++)
		unregister_blkdev(sd_major(i), "sd");
	return err;
}

 
static void __exit exit_sd(void)
{
	int i;

	SCSI_LOG_HLQUEUE(3, printk("exit_sd: exiting sd driver\n"));

	scsi_unregister_driver(&sd_template.gendrv);
	mempool_destroy(sd_page_pool);

	class_unregister(&sd_disk_class);

	for (i = 0; i < SD_MAJORS; i++)
		unregister_blkdev(sd_major(i), "sd");
}

module_init(init_sd);
module_exit(exit_sd);

void sd_print_sense_hdr(struct scsi_disk *sdkp, struct scsi_sense_hdr *sshdr)
{
	scsi_print_sense_hdr(sdkp->device,
			     sdkp->disk ? sdkp->disk->disk_name : NULL, sshdr);
}

void sd_print_result(const struct scsi_disk *sdkp, const char *msg, int result)
{
	const char *hb_string = scsi_hostbyte_string(result);

	if (hb_string)
		sd_printk(KERN_INFO, sdkp,
			  "%s: Result: hostbyte=%s driverbyte=%s\n", msg,
			  hb_string ? hb_string : "invalid",
			  "DRIVER_OK");
	else
		sd_printk(KERN_INFO, sdkp,
			  "%s: Result: hostbyte=0x%02x driverbyte=%s\n",
			  msg, host_byte(result), "DRIVER_OK");
}
