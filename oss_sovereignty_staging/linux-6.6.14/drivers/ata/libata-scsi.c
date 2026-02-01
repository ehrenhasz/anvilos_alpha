
 

#include <linux/compat.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/spinlock.h>
#include <linux/export.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_transport.h>
#include <linux/libata.h>
#include <linux/hdreg.h>
#include <linux/uaccess.h>
#include <linux/suspend.h>
#include <asm/unaligned.h>
#include <linux/ioprio.h>
#include <linux/of.h>

#include "libata.h"
#include "libata-transport.h"

#define ATA_SCSI_RBUF_SIZE	2048

static DEFINE_SPINLOCK(ata_scsi_rbuf_lock);
static u8 ata_scsi_rbuf[ATA_SCSI_RBUF_SIZE];

typedef unsigned int (*ata_xlat_func_t)(struct ata_queued_cmd *qc);

static struct ata_device *__ata_scsi_find_dev(struct ata_port *ap,
					const struct scsi_device *scsidev);

#define RW_RECOVERY_MPAGE		0x1
#define RW_RECOVERY_MPAGE_LEN		12
#define CACHE_MPAGE			0x8
#define CACHE_MPAGE_LEN			20
#define CONTROL_MPAGE			0xa
#define CONTROL_MPAGE_LEN		12
#define ALL_MPAGES			0x3f
#define ALL_SUB_MPAGES			0xff
#define CDL_T2A_SUB_MPAGE		0x07
#define CDL_T2B_SUB_MPAGE		0x08
#define CDL_T2_SUB_MPAGE_LEN		232
#define ATA_FEATURE_SUB_MPAGE		0xf2
#define ATA_FEATURE_SUB_MPAGE_LEN	16

static const u8 def_rw_recovery_mpage[RW_RECOVERY_MPAGE_LEN] = {
	RW_RECOVERY_MPAGE,
	RW_RECOVERY_MPAGE_LEN - 2,
	(1 << 7),	 
	0,		 
	0, 0, 0, 0,
	0,		 
	0, 0, 0
};

static const u8 def_cache_mpage[CACHE_MPAGE_LEN] = {
	CACHE_MPAGE,
	CACHE_MPAGE_LEN - 2,
	0,		 
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0,		 
	0, 0, 0, 0, 0, 0, 0
};

static const u8 def_control_mpage[CONTROL_MPAGE_LEN] = {
	CONTROL_MPAGE,
	CONTROL_MPAGE_LEN - 2,
	2,	 
	0,	 
	0, 0, 0, 0, 0xff, 0xff,
	0, 30	 
};

static ssize_t ata_scsi_park_show(struct device *device,
				  struct device_attribute *attr, char *buf)
{
	struct scsi_device *sdev = to_scsi_device(device);
	struct ata_port *ap;
	struct ata_link *link;
	struct ata_device *dev;
	unsigned long now;
	unsigned int msecs;
	int rc = 0;

	ap = ata_shost_to_port(sdev->host);

	spin_lock_irq(ap->lock);
	dev = ata_scsi_find_dev(ap, sdev);
	if (!dev) {
		rc = -ENODEV;
		goto unlock;
	}
	if (dev->flags & ATA_DFLAG_NO_UNLOAD) {
		rc = -EOPNOTSUPP;
		goto unlock;
	}

	link = dev->link;
	now = jiffies;
	if (ap->pflags & ATA_PFLAG_EH_IN_PROGRESS &&
	    link->eh_context.unloaded_mask & (1 << dev->devno) &&
	    time_after(dev->unpark_deadline, now))
		msecs = jiffies_to_msecs(dev->unpark_deadline - now);
	else
		msecs = 0;

unlock:
	spin_unlock_irq(ap->lock);

	return rc ? rc : sysfs_emit(buf, "%u\n", msecs);
}

static ssize_t ata_scsi_park_store(struct device *device,
				   struct device_attribute *attr,
				   const char *buf, size_t len)
{
	struct scsi_device *sdev = to_scsi_device(device);
	struct ata_port *ap;
	struct ata_device *dev;
	int input;
	unsigned long flags;
	int rc;

	rc = kstrtoint(buf, 10, &input);
	if (rc)
		return rc;
	if (input < -2)
		return -EINVAL;
	if (input > ATA_TMOUT_MAX_PARK) {
		rc = -EOVERFLOW;
		input = ATA_TMOUT_MAX_PARK;
	}

	ap = ata_shost_to_port(sdev->host);

	spin_lock_irqsave(ap->lock, flags);
	dev = ata_scsi_find_dev(ap, sdev);
	if (unlikely(!dev)) {
		rc = -ENODEV;
		goto unlock;
	}
	if (dev->class != ATA_DEV_ATA &&
	    dev->class != ATA_DEV_ZAC) {
		rc = -EOPNOTSUPP;
		goto unlock;
	}

	if (input >= 0) {
		if (dev->flags & ATA_DFLAG_NO_UNLOAD) {
			rc = -EOPNOTSUPP;
			goto unlock;
		}

		dev->unpark_deadline = ata_deadline(jiffies, input);
		dev->link->eh_info.dev_action[dev->devno] |= ATA_EH_PARK;
		ata_port_schedule_eh(ap);
		complete(&ap->park_req_pending);
	} else {
		switch (input) {
		case -1:
			dev->flags &= ~ATA_DFLAG_NO_UNLOAD;
			break;
		case -2:
			dev->flags |= ATA_DFLAG_NO_UNLOAD;
			break;
		}
	}
unlock:
	spin_unlock_irqrestore(ap->lock, flags);

	return rc ? rc : len;
}
DEVICE_ATTR(unload_heads, S_IRUGO | S_IWUSR,
	    ata_scsi_park_show, ata_scsi_park_store);
EXPORT_SYMBOL_GPL(dev_attr_unload_heads);

bool ata_scsi_sense_is_valid(u8 sk, u8 asc, u8 ascq)
{
	 
	if (sk == 0 && asc == 0 && ascq == 0)
		return false;

	 
	if (sk > COMPLETED)
		return false;

	return true;
}

void ata_scsi_set_sense(struct ata_device *dev, struct scsi_cmnd *cmd,
			u8 sk, u8 asc, u8 ascq)
{
	bool d_sense = (dev->flags & ATA_DFLAG_D_SENSE);

	scsi_build_sense(cmd, d_sense, sk, asc, ascq);
}

void ata_scsi_set_sense_information(struct ata_device *dev,
				    struct scsi_cmnd *cmd,
				    const struct ata_taskfile *tf)
{
	u64 information;

	information = ata_tf_read_block(tf, dev);
	if (information == U64_MAX)
		return;

	scsi_set_sense_information(cmd->sense_buffer,
				   SCSI_SENSE_BUFFERSIZE, information);
}

static void ata_scsi_set_invalid_field(struct ata_device *dev,
				       struct scsi_cmnd *cmd, u16 field, u8 bit)
{
	ata_scsi_set_sense(dev, cmd, ILLEGAL_REQUEST, 0x24, 0x0);
	 
	scsi_set_sense_field_pointer(cmd->sense_buffer, SCSI_SENSE_BUFFERSIZE,
				     field, bit, 1);
}

static void ata_scsi_set_invalid_parameter(struct ata_device *dev,
					   struct scsi_cmnd *cmd, u16 field)
{
	 
	ata_scsi_set_sense(dev, cmd, ILLEGAL_REQUEST, 0x26, 0x0);
	scsi_set_sense_field_pointer(cmd->sense_buffer, SCSI_SENSE_BUFFERSIZE,
				     field, 0xff, 0);
}

static struct attribute *ata_common_sdev_attrs[] = {
	&dev_attr_unload_heads.attr,
	NULL
};

static const struct attribute_group ata_common_sdev_attr_group = {
	.attrs = ata_common_sdev_attrs
};

const struct attribute_group *ata_common_sdev_groups[] = {
	&ata_common_sdev_attr_group,
	NULL
};
EXPORT_SYMBOL_GPL(ata_common_sdev_groups);

 
int ata_std_bios_param(struct scsi_device *sdev, struct block_device *bdev,
		       sector_t capacity, int geom[])
{
	geom[0] = 255;
	geom[1] = 63;
	sector_div(capacity, 255*63);
	geom[2] = capacity;

	return 0;
}
EXPORT_SYMBOL_GPL(ata_std_bios_param);

 
void ata_scsi_unlock_native_capacity(struct scsi_device *sdev)
{
	struct ata_port *ap = ata_shost_to_port(sdev->host);
	struct ata_device *dev;
	unsigned long flags;

	spin_lock_irqsave(ap->lock, flags);

	dev = ata_scsi_find_dev(ap, sdev);
	if (dev && dev->n_sectors < dev->n_native_sectors) {
		dev->flags |= ATA_DFLAG_UNLOCK_HPA;
		dev->link->eh_info.action |= ATA_EH_RESET;
		ata_port_schedule_eh(ap);
	}

	spin_unlock_irqrestore(ap->lock, flags);
	ata_port_wait_eh(ap);
}
EXPORT_SYMBOL_GPL(ata_scsi_unlock_native_capacity);

 
static int ata_get_identity(struct ata_port *ap, struct scsi_device *sdev,
			    void __user *arg)
{
	struct ata_device *dev = ata_scsi_find_dev(ap, sdev);
	u16 __user *dst = arg;
	char buf[40];

	if (!dev)
		return -ENOMSG;

	if (copy_to_user(dst, dev->id, ATA_ID_WORDS * sizeof(u16)))
		return -EFAULT;

	ata_id_string(dev->id, buf, ATA_ID_PROD, ATA_ID_PROD_LEN);
	if (copy_to_user(dst + ATA_ID_PROD, buf, ATA_ID_PROD_LEN))
		return -EFAULT;

	ata_id_string(dev->id, buf, ATA_ID_FW_REV, ATA_ID_FW_REV_LEN);
	if (copy_to_user(dst + ATA_ID_FW_REV, buf, ATA_ID_FW_REV_LEN))
		return -EFAULT;

	ata_id_string(dev->id, buf, ATA_ID_SERNO, ATA_ID_SERNO_LEN);
	if (copy_to_user(dst + ATA_ID_SERNO, buf, ATA_ID_SERNO_LEN))
		return -EFAULT;

	return 0;
}

 
int ata_cmd_ioctl(struct scsi_device *scsidev, void __user *arg)
{
	int rc = 0;
	u8 sensebuf[SCSI_SENSE_BUFFERSIZE];
	u8 scsi_cmd[MAX_COMMAND_SIZE];
	u8 args[4], *argbuf = NULL;
	int argsize = 0;
	struct scsi_sense_hdr sshdr;
	const struct scsi_exec_args exec_args = {
		.sshdr = &sshdr,
		.sense = sensebuf,
		.sense_len = sizeof(sensebuf),
	};
	int cmd_result;

	if (arg == NULL)
		return -EINVAL;

	if (copy_from_user(args, arg, sizeof(args)))
		return -EFAULT;

	memset(sensebuf, 0, sizeof(sensebuf));
	memset(scsi_cmd, 0, sizeof(scsi_cmd));

	if (args[3]) {
		argsize = ATA_SECT_SIZE * args[3];
		argbuf = kmalloc(argsize, GFP_KERNEL);
		if (argbuf == NULL) {
			rc = -ENOMEM;
			goto error;
		}

		scsi_cmd[1]  = (4 << 1);  
		scsi_cmd[2]  = 0x0e;      
	} else {
		scsi_cmd[1]  = (3 << 1);  
		scsi_cmd[2]  = 0x20;      
	}

	scsi_cmd[0] = ATA_16;

	scsi_cmd[4] = args[2];
	if (args[0] == ATA_CMD_SMART) {  
		scsi_cmd[6]  = args[3];
		scsi_cmd[8]  = args[1];
		scsi_cmd[10] = ATA_SMART_LBAM_PASS;
		scsi_cmd[12] = ATA_SMART_LBAH_PASS;
	} else {
		scsi_cmd[6]  = args[1];
	}
	scsi_cmd[14] = args[0];

	 
	cmd_result = scsi_execute_cmd(scsidev, scsi_cmd, REQ_OP_DRV_IN, argbuf,
				      argsize, 10 * HZ, 5, &exec_args);
	if (cmd_result < 0) {
		rc = cmd_result;
		goto error;
	}
	if (scsi_sense_valid(&sshdr)) { 
		u8 *desc = sensebuf + 8;

		 
		if (scsi_status_is_check_condition(cmd_result)) {
			if (sshdr.sense_key == RECOVERED_ERROR &&
			    sshdr.asc == 0 && sshdr.ascq == 0x1d)
				cmd_result &= ~SAM_STAT_CHECK_CONDITION;
		}

		 
		if (sensebuf[0] == 0x72 &&	 
		    desc[0] == 0x09) {		 
			args[0] = desc[13];	 
			args[1] = desc[3];	 
			args[2] = desc[5];	 
			if (copy_to_user(arg, args, sizeof(args)))
				rc = -EFAULT;
		}
	}


	if (cmd_result) {
		rc = -EIO;
		goto error;
	}

	if ((argbuf)
	 && copy_to_user(arg + sizeof(args), argbuf, argsize))
		rc = -EFAULT;
error:
	kfree(argbuf);
	return rc;
}

 
int ata_task_ioctl(struct scsi_device *scsidev, void __user *arg)
{
	int rc = 0;
	u8 sensebuf[SCSI_SENSE_BUFFERSIZE];
	u8 scsi_cmd[MAX_COMMAND_SIZE];
	u8 args[7];
	struct scsi_sense_hdr sshdr;
	int cmd_result;
	const struct scsi_exec_args exec_args = {
		.sshdr = &sshdr,
		.sense = sensebuf,
		.sense_len = sizeof(sensebuf),
	};

	if (arg == NULL)
		return -EINVAL;

	if (copy_from_user(args, arg, sizeof(args)))
		return -EFAULT;

	memset(sensebuf, 0, sizeof(sensebuf));
	memset(scsi_cmd, 0, sizeof(scsi_cmd));
	scsi_cmd[0]  = ATA_16;
	scsi_cmd[1]  = (3 << 1);  
	scsi_cmd[2]  = 0x20;      
	scsi_cmd[4]  = args[1];
	scsi_cmd[6]  = args[2];
	scsi_cmd[8]  = args[3];
	scsi_cmd[10] = args[4];
	scsi_cmd[12] = args[5];
	scsi_cmd[13] = args[6] & 0x4f;
	scsi_cmd[14] = args[0];

	 
	cmd_result = scsi_execute_cmd(scsidev, scsi_cmd, REQ_OP_DRV_IN, NULL,
				      0, 10 * HZ, 5, &exec_args);
	if (cmd_result < 0) {
		rc = cmd_result;
		goto error;
	}
	if (scsi_sense_valid(&sshdr)) { 
		u8 *desc = sensebuf + 8;

		 
		if (cmd_result & SAM_STAT_CHECK_CONDITION) {
			if (sshdr.sense_key == RECOVERED_ERROR &&
			    sshdr.asc == 0 && sshdr.ascq == 0x1d)
				cmd_result &= ~SAM_STAT_CHECK_CONDITION;
		}

		 
		if (sensebuf[0] == 0x72 &&	 
				desc[0] == 0x09) { 
			args[0] = desc[13];	 
			args[1] = desc[3];	 
			args[2] = desc[5];	 
			args[3] = desc[7];	 
			args[4] = desc[9];	 
			args[5] = desc[11];	 
			args[6] = desc[12];	 
			if (copy_to_user(arg, args, sizeof(args)))
				rc = -EFAULT;
		}
	}

	if (cmd_result) {
		rc = -EIO;
		goto error;
	}

 error:
	return rc;
}

static bool ata_ioc32(struct ata_port *ap)
{
	if (ap->flags & ATA_FLAG_PIO_DMA)
		return true;
	if (ap->pflags & ATA_PFLAG_PIO32)
		return true;
	return false;
}

 
int ata_sas_scsi_ioctl(struct ata_port *ap, struct scsi_device *scsidev,
		     unsigned int cmd, void __user *arg)
{
	unsigned long val;
	int rc = -EINVAL;
	unsigned long flags;

	switch (cmd) {
	case HDIO_GET_32BIT:
		spin_lock_irqsave(ap->lock, flags);
		val = ata_ioc32(ap);
		spin_unlock_irqrestore(ap->lock, flags);
#ifdef CONFIG_COMPAT
		if (in_compat_syscall())
			return put_user(val, (compat_ulong_t __user *)arg);
#endif
		return put_user(val, (unsigned long __user *)arg);

	case HDIO_SET_32BIT:
		val = (unsigned long) arg;
		rc = 0;
		spin_lock_irqsave(ap->lock, flags);
		if (ap->pflags & ATA_PFLAG_PIO32CHANGE) {
			if (val)
				ap->pflags |= ATA_PFLAG_PIO32;
			else
				ap->pflags &= ~ATA_PFLAG_PIO32;
		} else {
			if (val != ata_ioc32(ap))
				rc = -EINVAL;
		}
		spin_unlock_irqrestore(ap->lock, flags);
		return rc;

	case HDIO_GET_IDENTITY:
		return ata_get_identity(ap, scsidev, arg);

	case HDIO_DRIVE_CMD:
		if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RAWIO))
			return -EACCES;
		return ata_cmd_ioctl(scsidev, arg);

	case HDIO_DRIVE_TASK:
		if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RAWIO))
			return -EACCES;
		return ata_task_ioctl(scsidev, arg);

	default:
		rc = -ENOTTY;
		break;
	}

	return rc;
}
EXPORT_SYMBOL_GPL(ata_sas_scsi_ioctl);

int ata_scsi_ioctl(struct scsi_device *scsidev, unsigned int cmd,
		   void __user *arg)
{
	return ata_sas_scsi_ioctl(ata_shost_to_port(scsidev->host),
				scsidev, cmd, arg);
}
EXPORT_SYMBOL_GPL(ata_scsi_ioctl);

 
static struct ata_queued_cmd *ata_scsi_qc_new(struct ata_device *dev,
					      struct scsi_cmnd *cmd)
{
	struct ata_port *ap = dev->link->ap;
	struct ata_queued_cmd *qc;
	int tag;

	if (unlikely(ata_port_is_frozen(ap)))
		goto fail;

	if (ap->flags & ATA_FLAG_SAS_HOST) {
		 
		if (WARN_ON_ONCE(cmd->budget_token >= ATA_MAX_QUEUE))
			goto fail;
		tag = cmd->budget_token;
	} else {
		tag = scsi_cmd_to_rq(cmd)->tag;
	}

	qc = __ata_qc_from_tag(ap, tag);
	qc->tag = qc->hw_tag = tag;
	qc->ap = ap;
	qc->dev = dev;

	ata_qc_reinit(qc);

	qc->scsicmd = cmd;
	qc->scsidone = scsi_done;

	qc->sg = scsi_sglist(cmd);
	qc->n_elem = scsi_sg_count(cmd);

	if (scsi_cmd_to_rq(cmd)->rq_flags & RQF_QUIET)
		qc->flags |= ATA_QCFLAG_QUIET;

	return qc;

fail:
	set_host_byte(cmd, DID_OK);
	set_status_byte(cmd, SAM_STAT_TASK_SET_FULL);
	scsi_done(cmd);
	return NULL;
}

static void ata_qc_set_pc_nbytes(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *scmd = qc->scsicmd;

	qc->extrabytes = scmd->extra_len;
	qc->nbytes = scsi_bufflen(scmd) + qc->extrabytes;
}

 
static void ata_to_sense_error(unsigned id, u8 drv_stat, u8 drv_err, u8 *sk,
			       u8 *asc, u8 *ascq)
{
	int i;

	 
	static const unsigned char sense_table[][4] = {
		 
		{0xd1,		ABORTED_COMMAND, 0x00, 0x00},
			 
		 
		{0xd0,		ABORTED_COMMAND, 0x00, 0x00},
			 
		 
		{0x61,		HARDWARE_ERROR, 0x00, 0x00},
			 
		 		 
		{0x84,		ABORTED_COMMAND, 0x47, 0x00},
			 
		 
		{0x37,		NOT_READY, 0x04, 0x00},
			 
		 
		{0x09,		NOT_READY, 0x04, 0x00},
			 
		 
		{0x01,		MEDIUM_ERROR, 0x13, 0x00},
			 
		 
		{0x02,		HARDWARE_ERROR, 0x00, 0x00},
			 
		 
		 
		{0x08,		NOT_READY, 0x04, 0x00},
			 
		 
		{0x10,		ILLEGAL_REQUEST, 0x21, 0x00},
			 
		 
		{0x20,		UNIT_ATTENTION, 0x28, 0x00},
			 
		 
		{0x40,		MEDIUM_ERROR, 0x11, 0x04},
			 
		 
		{0x80,		MEDIUM_ERROR, 0x11, 0x04},
			 
		{0xFF, 0xFF, 0xFF, 0xFF},  
	};
	static const unsigned char stat_table[][4] = {
		 
		{0x80,		ABORTED_COMMAND, 0x47, 0x00},
		 
		{0x40,		ILLEGAL_REQUEST, 0x21, 0x04},
		 
		{0x20,		HARDWARE_ERROR,  0x44, 0x00},
		 
		{0x08,		ABORTED_COMMAND, 0x47, 0x00},
		 
		{0x04,		RECOVERED_ERROR, 0x11, 0x00},
		 
		{0xFF, 0xFF, 0xFF, 0xFF},  
	};

	 
	if (drv_stat & ATA_BUSY) {
		drv_err = 0;	 
	}

	if (drv_err) {
		 
		for (i = 0; sense_table[i][0] != 0xFF; i++) {
			 
			if ((sense_table[i][0] & drv_err) ==
			    sense_table[i][0]) {
				*sk = sense_table[i][1];
				*asc = sense_table[i][2];
				*ascq = sense_table[i][3];
				return;
			}
		}
	}

	 
	for (i = 0; stat_table[i][0] != 0xFF; i++) {
		if (stat_table[i][0] & drv_stat) {
			*sk = stat_table[i][1];
			*asc = stat_table[i][2];
			*ascq = stat_table[i][3];
			return;
		}
	}

	 
	*sk = ABORTED_COMMAND;
	*asc = 0x00;
	*ascq = 0x00;
}

 
static void ata_gen_passthru_sense(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *cmd = qc->scsicmd;
	struct ata_taskfile *tf = &qc->result_tf;
	unsigned char *sb = cmd->sense_buffer;
	unsigned char *desc = sb + 8;
	u8 sense_key, asc, ascq;

	memset(sb, 0, SCSI_SENSE_BUFFERSIZE);

	 
	if (qc->err_mask ||
	    tf->status & (ATA_BUSY | ATA_DF | ATA_ERR | ATA_DRQ)) {
		ata_to_sense_error(qc->ap->print_id, tf->status, tf->error,
				   &sense_key, &asc, &ascq);
		ata_scsi_set_sense(qc->dev, cmd, sense_key, asc, ascq);
	} else {
		 
		scsi_build_sense(cmd, 1, RECOVERED_ERROR, 0, 0x1D);
	}

	if ((cmd->sense_buffer[0] & 0x7f) >= 0x72) {
		u8 len;

		 
		len = sb[7];
		desc = (char *)scsi_sense_desc_find(sb, len + 8, 9);
		if (!desc) {
			if (SCSI_SENSE_BUFFERSIZE < len + 14)
				return;
			sb[7] = len + 14;
			desc = sb + 8 + len;
		}
		desc[0] = 9;
		desc[1] = 12;
		 
		desc[2] = 0x00;
		desc[3] = tf->error;
		desc[5] = tf->nsect;
		desc[7] = tf->lbal;
		desc[9] = tf->lbam;
		desc[11] = tf->lbah;
		desc[12] = tf->device;
		desc[13] = tf->status;

		 
		if (tf->flags & ATA_TFLAG_LBA48) {
			desc[2] |= 0x01;
			desc[4] = tf->hob_nsect;
			desc[6] = tf->hob_lbal;
			desc[8] = tf->hob_lbam;
			desc[10] = tf->hob_lbah;
		}
	} else {
		 
		desc[0] = tf->error;
		desc[1] = tf->status;
		desc[2] = tf->device;
		desc[3] = tf->nsect;
		desc[7] = 0;
		if (tf->flags & ATA_TFLAG_LBA48)  {
			desc[8] |= 0x80;
			if (tf->hob_nsect)
				desc[8] |= 0x40;
			if (tf->hob_lbal || tf->hob_lbam || tf->hob_lbah)
				desc[8] |= 0x20;
		}
		desc[9] = tf->lbal;
		desc[10] = tf->lbam;
		desc[11] = tf->lbah;
	}
}

 
static void ata_gen_ata_sense(struct ata_queued_cmd *qc)
{
	struct ata_device *dev = qc->dev;
	struct scsi_cmnd *cmd = qc->scsicmd;
	struct ata_taskfile *tf = &qc->result_tf;
	unsigned char *sb = cmd->sense_buffer;
	u64 block;
	u8 sense_key, asc, ascq;

	memset(sb, 0, SCSI_SENSE_BUFFERSIZE);

	if (ata_dev_disabled(dev)) {
		 
		 
		ata_scsi_set_sense(dev, cmd, NOT_READY, 0x04, 0x21);
		return;
	}
	 
	if (qc->err_mask ||
	    tf->status & (ATA_BUSY | ATA_DF | ATA_ERR | ATA_DRQ)) {
		ata_to_sense_error(qc->ap->print_id, tf->status, tf->error,
				   &sense_key, &asc, &ascq);
		ata_scsi_set_sense(dev, cmd, sense_key, asc, ascq);
	} else {
		 
		ata_dev_warn(dev, "could not decode error status 0x%x err_mask 0x%x\n",
			     tf->status, qc->err_mask);
		ata_scsi_set_sense(dev, cmd, ABORTED_COMMAND, 0, 0);
		return;
	}

	block = ata_tf_read_block(&qc->result_tf, dev);
	if (block == U64_MAX)
		return;

	scsi_set_sense_information(sb, SCSI_SENSE_BUFFERSIZE, block);
}

void ata_scsi_sdev_config(struct scsi_device *sdev)
{
	sdev->use_10_for_rw = 1;
	sdev->use_10_for_ms = 1;
	sdev->no_write_same = 1;

	 
	sdev->max_device_blocked = 1;
}

 
bool ata_scsi_dma_need_drain(struct request *rq)
{
	struct scsi_cmnd *scmd = blk_mq_rq_to_pdu(rq);

	return atapi_cmd_type(scmd->cmnd[0]) == ATAPI_MISC;
}
EXPORT_SYMBOL_GPL(ata_scsi_dma_need_drain);

int ata_scsi_dev_config(struct scsi_device *sdev, struct ata_device *dev)
{
	struct request_queue *q = sdev->request_queue;
	int depth = 1;

	if (!ata_id_has_unload(dev->id))
		dev->flags |= ATA_DFLAG_NO_UNLOAD;

	 
	dev->max_sectors = min(dev->max_sectors, sdev->host->max_sectors);
	blk_queue_max_hw_sectors(q, dev->max_sectors);

	if (dev->class == ATA_DEV_ATAPI) {
		sdev->sector_size = ATA_SECT_SIZE;

		 
		blk_queue_update_dma_pad(q, ATA_DMA_PAD_SZ - 1);

		 
		blk_queue_max_segments(q, queue_max_segments(q) - 1);

		sdev->dma_drain_len = ATAPI_MAX_DRAIN;
		sdev->dma_drain_buf = kmalloc(sdev->dma_drain_len, GFP_NOIO);
		if (!sdev->dma_drain_buf) {
			ata_dev_err(dev, "drain buffer allocation failed\n");
			return -ENOMEM;
		}
	} else {
		sdev->sector_size = ata_id_logical_sector_size(dev->id);

		 
		sdev->manage_runtime_start_stop = 1;
		sdev->manage_shutdown = 1;
		sdev->force_runtime_start_on_system_start = 1;
	}

	 
	if (sdev->sector_size > PAGE_SIZE)
		ata_dev_warn(dev,
			"sector_size=%u > PAGE_SIZE, PIO may malfunction\n",
			sdev->sector_size);

	blk_queue_update_dma_alignment(q, sdev->sector_size - 1);

	if (dev->flags & ATA_DFLAG_AN)
		set_bit(SDEV_EVT_MEDIA_CHANGE, sdev->supported_events);

	if (ata_ncq_supported(dev))
		depth = min(sdev->host->can_queue, ata_id_queue_depth(dev->id));
	depth = min(ATA_MAX_QUEUE, depth);
	scsi_change_queue_depth(sdev, depth);

	if (dev->flags & ATA_DFLAG_TRUSTED)
		sdev->security_supported = 1;

	dev->sdev = sdev;
	return 0;
}

 

int ata_scsi_slave_alloc(struct scsi_device *sdev)
{
	struct ata_port *ap = ata_shost_to_port(sdev->host);
	struct device_link *link;

	ata_scsi_sdev_config(sdev);

	 
	link = device_link_add(&sdev->sdev_gendev, &ap->tdev,
			       DL_FLAG_STATELESS |
			       DL_FLAG_PM_RUNTIME | DL_FLAG_RPM_ACTIVE);
	if (!link) {
		ata_port_err(ap, "Failed to create link to scsi device %s\n",
			     dev_name(&sdev->sdev_gendev));
		return -ENODEV;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(ata_scsi_slave_alloc);

 

int ata_scsi_slave_config(struct scsi_device *sdev)
{
	struct ata_port *ap = ata_shost_to_port(sdev->host);
	struct ata_device *dev = __ata_scsi_find_dev(ap, sdev);

	if (dev)
		return ata_scsi_dev_config(sdev, dev);

	return 0;
}
EXPORT_SYMBOL_GPL(ata_scsi_slave_config);

 
void ata_scsi_slave_destroy(struct scsi_device *sdev)
{
	struct ata_port *ap = ata_shost_to_port(sdev->host);
	unsigned long flags;
	struct ata_device *dev;

	device_link_remove(&sdev->sdev_gendev, &ap->tdev);

	spin_lock_irqsave(ap->lock, flags);
	dev = __ata_scsi_find_dev(ap, sdev);
	if (dev && dev->sdev) {
		 
		dev->sdev = NULL;
		dev->flags |= ATA_DFLAG_DETACH;
		ata_port_schedule_eh(ap);
	}
	spin_unlock_irqrestore(ap->lock, flags);

	kfree(sdev->dma_drain_buf);
}
EXPORT_SYMBOL_GPL(ata_scsi_slave_destroy);

 
static unsigned int ata_scsi_start_stop_xlat(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *scmd = qc->scsicmd;
	struct ata_taskfile *tf = &qc->tf;
	const u8 *cdb = scmd->cmnd;
	u16 fp;
	u8 bp = 0xff;

	if (scmd->cmd_len < 5) {
		fp = 4;
		goto invalid_fld;
	}

	tf->flags |= ATA_TFLAG_DEVICE | ATA_TFLAG_ISADDR;
	tf->protocol = ATA_PROT_NODATA;
	if (cdb[1] & 0x1) {
		;	 
	}
	if (cdb[4] & 0x2) {
		fp = 4;
		bp = 1;
		goto invalid_fld;        
	}
	if (((cdb[4] >> 4) & 0xf) != 0) {
		fp = 4;
		bp = 3;
		goto invalid_fld;        
	}

	if (cdb[4] & 0x1) {
		tf->nsect = 1;   

		if (qc->dev->flags & ATA_DFLAG_LBA) {
			tf->flags |= ATA_TFLAG_LBA;

			tf->lbah = 0x0;
			tf->lbam = 0x0;
			tf->lbal = 0x0;
			tf->device |= ATA_LBA;
		} else {
			 
			tf->lbal = 0x1;  
			tf->lbam = 0x0;  
			tf->lbah = 0x0;  
		}

		tf->command = ATA_CMD_VERIFY;    
	} else {
		 
		if ((qc->ap->flags & ATA_FLAG_NO_POWEROFF_SPINDOWN) &&
		    system_state == SYSTEM_POWER_OFF)
			goto skip;

		if ((qc->ap->flags & ATA_FLAG_NO_HIBERNATE_SPINDOWN) &&
		    system_entering_hibernation())
			goto skip;

		 
		tf->command = ATA_CMD_STANDBYNOW1;
	}

	 

	return 0;

 invalid_fld:
	ata_scsi_set_invalid_field(qc->dev, scmd, fp, bp);
	return 1;
 skip:
	scmd->result = SAM_STAT_GOOD;
	return 1;
}


 
static unsigned int ata_scsi_flush_xlat(struct ata_queued_cmd *qc)
{
	struct ata_taskfile *tf = &qc->tf;

	tf->flags |= ATA_TFLAG_DEVICE;
	tf->protocol = ATA_PROT_NODATA;

	if (qc->dev->flags & ATA_DFLAG_FLUSH_EXT)
		tf->command = ATA_CMD_FLUSH_EXT;
	else
		tf->command = ATA_CMD_FLUSH;

	 
	qc->flags |= ATA_QCFLAG_IO;

	return 0;
}

 
static void scsi_6_lba_len(const u8 *cdb, u64 *plba, u32 *plen)
{
	u64 lba = 0;
	u32 len;

	lba |= ((u64)(cdb[1] & 0x1f)) << 16;
	lba |= ((u64)cdb[2]) << 8;
	lba |= ((u64)cdb[3]);

	len = cdb[4];

	*plba = lba;
	*plen = len;
}

 
static inline void scsi_10_lba_len(const u8 *cdb, u64 *plba, u32 *plen)
{
	*plba = get_unaligned_be32(&cdb[2]);
	*plen = get_unaligned_be16(&cdb[7]);
}

 
static inline void scsi_16_lba_len(const u8 *cdb, u64 *plba, u32 *plen)
{
	*plba = get_unaligned_be64(&cdb[2]);
	*plen = get_unaligned_be32(&cdb[10]);
}

 
static inline int scsi_dld(const u8 *cdb)
{
	return ((cdb[1] & 0x01) << 2) | ((cdb[14] >> 6) & 0x03);
}

 
static unsigned int ata_scsi_verify_xlat(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *scmd = qc->scsicmd;
	struct ata_taskfile *tf = &qc->tf;
	struct ata_device *dev = qc->dev;
	u64 dev_sectors = qc->dev->n_sectors;
	const u8 *cdb = scmd->cmnd;
	u64 block;
	u32 n_block;
	u16 fp;

	tf->flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE;
	tf->protocol = ATA_PROT_NODATA;

	switch (cdb[0]) {
	case VERIFY:
		if (scmd->cmd_len < 10) {
			fp = 9;
			goto invalid_fld;
		}
		scsi_10_lba_len(cdb, &block, &n_block);
		break;
	case VERIFY_16:
		if (scmd->cmd_len < 16) {
			fp = 15;
			goto invalid_fld;
		}
		scsi_16_lba_len(cdb, &block, &n_block);
		break;
	default:
		fp = 0;
		goto invalid_fld;
	}

	if (!n_block)
		goto nothing_to_do;
	if (block >= dev_sectors)
		goto out_of_range;
	if ((block + n_block) > dev_sectors)
		goto out_of_range;

	if (dev->flags & ATA_DFLAG_LBA) {
		tf->flags |= ATA_TFLAG_LBA;

		if (lba_28_ok(block, n_block)) {
			 
			tf->command = ATA_CMD_VERIFY;
			tf->device |= (block >> 24) & 0xf;
		} else if (lba_48_ok(block, n_block)) {
			if (!(dev->flags & ATA_DFLAG_LBA48))
				goto out_of_range;

			 
			tf->flags |= ATA_TFLAG_LBA48;
			tf->command = ATA_CMD_VERIFY_EXT;

			tf->hob_nsect = (n_block >> 8) & 0xff;

			tf->hob_lbah = (block >> 40) & 0xff;
			tf->hob_lbam = (block >> 32) & 0xff;
			tf->hob_lbal = (block >> 24) & 0xff;
		} else
			 
			goto out_of_range;

		tf->nsect = n_block & 0xff;

		tf->lbah = (block >> 16) & 0xff;
		tf->lbam = (block >> 8) & 0xff;
		tf->lbal = block & 0xff;

		tf->device |= ATA_LBA;
	} else {
		 
		u32 sect, head, cyl, track;

		if (!lba_28_ok(block, n_block))
			goto out_of_range;

		 
		track = (u32)block / dev->sectors;
		cyl   = track / dev->heads;
		head  = track % dev->heads;
		sect  = (u32)block % dev->sectors + 1;

		 
		if ((cyl >> 16) || (head >> 4) || (sect >> 8) || (!sect))
			goto out_of_range;

		tf->command = ATA_CMD_VERIFY;
		tf->nsect = n_block & 0xff;  
		tf->lbal = sect;
		tf->lbam = cyl;
		tf->lbah = cyl >> 8;
		tf->device |= head;
	}

	return 0;

invalid_fld:
	ata_scsi_set_invalid_field(qc->dev, scmd, fp, 0xff);
	return 1;

out_of_range:
	ata_scsi_set_sense(qc->dev, scmd, ILLEGAL_REQUEST, 0x21, 0x0);
	 
	return 1;

nothing_to_do:
	scmd->result = SAM_STAT_GOOD;
	return 1;
}

static bool ata_check_nblocks(struct scsi_cmnd *scmd, u32 n_blocks)
{
	struct request *rq = scsi_cmd_to_rq(scmd);
	u32 req_blocks;

	if (!blk_rq_is_passthrough(rq))
		return true;

	req_blocks = blk_rq_bytes(rq) / scmd->device->sector_size;
	if (n_blocks > req_blocks)
		return false;

	return true;
}

 
static unsigned int ata_scsi_rw_xlat(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *scmd = qc->scsicmd;
	const u8 *cdb = scmd->cmnd;
	struct request *rq = scsi_cmd_to_rq(scmd);
	int class = IOPRIO_PRIO_CLASS(req_get_ioprio(rq));
	unsigned int tf_flags = 0;
	int dld = 0;
	u64 block;
	u32 n_block;
	int rc;
	u16 fp = 0;

	switch (cdb[0]) {
	case WRITE_6:
	case WRITE_10:
	case WRITE_16:
		tf_flags |= ATA_TFLAG_WRITE;
		break;
	}

	 
	switch (cdb[0]) {
	case READ_10:
	case WRITE_10:
		if (unlikely(scmd->cmd_len < 10)) {
			fp = 9;
			goto invalid_fld;
		}
		scsi_10_lba_len(cdb, &block, &n_block);
		if (cdb[1] & (1 << 3))
			tf_flags |= ATA_TFLAG_FUA;
		if (!ata_check_nblocks(scmd, n_block))
			goto invalid_fld;
		break;
	case READ_6:
	case WRITE_6:
		if (unlikely(scmd->cmd_len < 6)) {
			fp = 5;
			goto invalid_fld;
		}
		scsi_6_lba_len(cdb, &block, &n_block);

		 
		if (!n_block)
			n_block = 256;
		if (!ata_check_nblocks(scmd, n_block))
			goto invalid_fld;
		break;
	case READ_16:
	case WRITE_16:
		if (unlikely(scmd->cmd_len < 16)) {
			fp = 15;
			goto invalid_fld;
		}
		scsi_16_lba_len(cdb, &block, &n_block);
		dld = scsi_dld(cdb);
		if (cdb[1] & (1 << 3))
			tf_flags |= ATA_TFLAG_FUA;
		if (!ata_check_nblocks(scmd, n_block))
			goto invalid_fld;
		break;
	default:
		fp = 0;
		goto invalid_fld;
	}

	 
	if (!n_block)
		 
		goto nothing_to_do;

	qc->flags |= ATA_QCFLAG_IO;
	qc->nbytes = n_block * scmd->device->sector_size;

	rc = ata_build_rw_tf(qc, block, n_block, tf_flags, dld, class);
	if (likely(rc == 0))
		return 0;

	if (rc == -ERANGE)
		goto out_of_range;
	 
invalid_fld:
	ata_scsi_set_invalid_field(qc->dev, scmd, fp, 0xff);
	return 1;

out_of_range:
	ata_scsi_set_sense(qc->dev, scmd, ILLEGAL_REQUEST, 0x21, 0x0);
	 
	return 1;

nothing_to_do:
	scmd->result = SAM_STAT_GOOD;
	return 1;
}

static void ata_qc_done(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *cmd = qc->scsicmd;
	void (*done)(struct scsi_cmnd *) = qc->scsidone;

	ata_qc_free(qc);
	done(cmd);
}

static void ata_scsi_qc_complete(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *cmd = qc->scsicmd;
	u8 *cdb = cmd->cmnd;
	int need_sense = (qc->err_mask != 0) &&
		!(qc->flags & ATA_QCFLAG_SENSE_VALID);

	 
	if (((cdb[0] == ATA_16) || (cdb[0] == ATA_12)) &&
	    ((cdb[2] & 0x20) || need_sense))
		ata_gen_passthru_sense(qc);
	else if (need_sense)
		ata_gen_ata_sense(qc);
	else
		 
		cmd->result &= 0x0000ffff;

	ata_qc_done(qc);
}

 
static int ata_scsi_translate(struct ata_device *dev, struct scsi_cmnd *cmd,
			      ata_xlat_func_t xlat_func)
{
	struct ata_port *ap = dev->link->ap;
	struct ata_queued_cmd *qc;
	int rc;

	qc = ata_scsi_qc_new(dev, cmd);
	if (!qc)
		goto err_mem;

	 
	if (cmd->sc_data_direction == DMA_FROM_DEVICE ||
	    cmd->sc_data_direction == DMA_TO_DEVICE) {
		if (unlikely(scsi_bufflen(cmd) < 1)) {
			ata_dev_warn(dev, "WARNING: zero len r/w req\n");
			goto err_did;
		}

		ata_sg_init(qc, scsi_sglist(cmd), scsi_sg_count(cmd));

		qc->dma_dir = cmd->sc_data_direction;
	}

	qc->complete_fn = ata_scsi_qc_complete;

	if (xlat_func(qc))
		goto early_finish;

	if (ap->ops->qc_defer) {
		if ((rc = ap->ops->qc_defer(qc)))
			goto defer;
	}

	 
	ata_qc_issue(qc);

	return 0;

early_finish:
	ata_qc_free(qc);
	scsi_done(cmd);
	return 0;

err_did:
	ata_qc_free(qc);
	cmd->result = (DID_ERROR << 16);
	scsi_done(cmd);
err_mem:
	return 0;

defer:
	ata_qc_free(qc);
	if (rc == ATA_DEFER_LINK)
		return SCSI_MLQUEUE_DEVICE_BUSY;
	else
		return SCSI_MLQUEUE_HOST_BUSY;
}

struct ata_scsi_args {
	struct ata_device	*dev;
	u16			*id;
	struct scsi_cmnd	*cmd;
};

 
static void ata_scsi_rbuf_fill(struct ata_scsi_args *args,
		unsigned int (*actor)(struct ata_scsi_args *args, u8 *rbuf))
{
	unsigned int rc;
	struct scsi_cmnd *cmd = args->cmd;
	unsigned long flags;

	spin_lock_irqsave(&ata_scsi_rbuf_lock, flags);

	memset(ata_scsi_rbuf, 0, ATA_SCSI_RBUF_SIZE);
	rc = actor(args, ata_scsi_rbuf);
	if (rc == 0)
		sg_copy_from_buffer(scsi_sglist(cmd), scsi_sg_count(cmd),
				    ata_scsi_rbuf, ATA_SCSI_RBUF_SIZE);

	spin_unlock_irqrestore(&ata_scsi_rbuf_lock, flags);

	if (rc == 0)
		cmd->result = SAM_STAT_GOOD;
}

 
static unsigned int ata_scsiop_inq_std(struct ata_scsi_args *args, u8 *rbuf)
{
	static const u8 versions[] = {
		0x00,
		0x60,	 

		0x03,
		0x20,	 

		0x03,
		0x00	 
	};
	static const u8 versions_zbc[] = {
		0x00,
		0xA0,	 

		0x06,
		0x00,	 

		0x05,
		0xC0,	 

		0x60,
		0x24,    
	};

	u8 hdr[] = {
		TYPE_DISK,
		0,
		0x5,	 
		2,
		95 - 4,
		0,
		0,
		2
	};

	 
	if (ata_id_removable(args->id) ||
	    (args->dev->link->ap->pflags & ATA_PFLAG_EXTERNAL))
		hdr[1] |= (1 << 7);

	if (args->dev->class == ATA_DEV_ZAC) {
		hdr[0] = TYPE_ZBC;
		hdr[2] = 0x7;  
	}

	if (args->dev->flags & ATA_DFLAG_CDL)
		hdr[2] = 0xd;  

	memcpy(rbuf, hdr, sizeof(hdr));
	memcpy(&rbuf[8], "ATA     ", 8);
	ata_id_string(args->id, &rbuf[16], ATA_ID_PROD, 16);

	 
	ata_id_string(args->id, &rbuf[32], ATA_ID_FW_REV + 2, 4);
	if (strncmp(&rbuf[32], "    ", 4) == 0)
		ata_id_string(args->id, &rbuf[32], ATA_ID_FW_REV, 4);

	if (rbuf[32] == 0 || rbuf[32] == ' ')
		memcpy(&rbuf[32], "n/a ", 4);

	if (ata_id_zoned_cap(args->id) || args->dev->class == ATA_DEV_ZAC)
		memcpy(rbuf + 58, versions_zbc, sizeof(versions_zbc));
	else
		memcpy(rbuf + 58, versions, sizeof(versions));

	return 0;
}

 
static unsigned int ata_scsiop_inq_00(struct ata_scsi_args *args, u8 *rbuf)
{
	int i, num_pages = 0;
	static const u8 pages[] = {
		0x00,	 
		0x80,	 
		0x83,	 
		0x89,	 
		0xb0,	 
		0xb1,	 
		0xb2,	 
		0xb6,	 
		0xb9,	 
	};

	for (i = 0; i < sizeof(pages); i++) {
		if (pages[i] == 0xb6 &&
		    !(args->dev->flags & ATA_DFLAG_ZAC))
			continue;
		rbuf[num_pages + 4] = pages[i];
		num_pages++;
	}
	rbuf[3] = num_pages;	 
	return 0;
}

 
static unsigned int ata_scsiop_inq_80(struct ata_scsi_args *args, u8 *rbuf)
{
	static const u8 hdr[] = {
		0,
		0x80,			 
		0,
		ATA_ID_SERNO_LEN,	 
	};

	memcpy(rbuf, hdr, sizeof(hdr));
	ata_id_string(args->id, (unsigned char *) &rbuf[4],
		      ATA_ID_SERNO, ATA_ID_SERNO_LEN);
	return 0;
}

 
static unsigned int ata_scsiop_inq_83(struct ata_scsi_args *args, u8 *rbuf)
{
	const int sat_model_serial_desc_len = 68;
	int num;

	rbuf[1] = 0x83;			 
	num = 4;

	 
	rbuf[num + 0] = 2;
	rbuf[num + 3] = ATA_ID_SERNO_LEN;
	num += 4;
	ata_id_string(args->id, (unsigned char *) rbuf + num,
		      ATA_ID_SERNO, ATA_ID_SERNO_LEN);
	num += ATA_ID_SERNO_LEN;

	 
	 
	rbuf[num + 0] = 2;
	rbuf[num + 1] = 1;
	rbuf[num + 3] = sat_model_serial_desc_len;
	num += 4;
	memcpy(rbuf + num, "ATA     ", 8);
	num += 8;
	ata_id_string(args->id, (unsigned char *) rbuf + num, ATA_ID_PROD,
		      ATA_ID_PROD_LEN);
	num += ATA_ID_PROD_LEN;
	ata_id_string(args->id, (unsigned char *) rbuf + num, ATA_ID_SERNO,
		      ATA_ID_SERNO_LEN);
	num += ATA_ID_SERNO_LEN;

	if (ata_id_has_wwn(args->id)) {
		 
		 
		rbuf[num + 0] = 1;
		rbuf[num + 1] = 3;
		rbuf[num + 3] = ATA_ID_WWN_LEN;
		num += 4;
		ata_id_string(args->id, (unsigned char *) rbuf + num,
			      ATA_ID_WWN, ATA_ID_WWN_LEN);
		num += ATA_ID_WWN_LEN;
	}
	rbuf[3] = num - 4;     
	return 0;
}

 
static unsigned int ata_scsiop_inq_89(struct ata_scsi_args *args, u8 *rbuf)
{
	rbuf[1] = 0x89;			 
	rbuf[2] = (0x238 >> 8);		 
	rbuf[3] = (0x238 & 0xff);

	memcpy(&rbuf[8], "linux   ", 8);
	memcpy(&rbuf[16], "libata          ", 16);
	memcpy(&rbuf[32], DRV_VERSION, 4);

	rbuf[36] = 0x34;		 
	rbuf[37] = (1 << 7);		 
					 

	 
	rbuf[38] = ATA_DRDY;		 
	rbuf[40] = 0x1;
	rbuf[48] = 0x1;

	rbuf[56] = ATA_CMD_ID_ATA;

	memcpy(&rbuf[60], &args->id[0], 512);
	return 0;
}

static unsigned int ata_scsiop_inq_b0(struct ata_scsi_args *args, u8 *rbuf)
{
	struct ata_device *dev = args->dev;
	u16 min_io_sectors;

	rbuf[1] = 0xb0;
	rbuf[3] = 0x3c;		 

	 
	min_io_sectors = 1 << ata_id_log2_per_physical_sector(args->id);
	put_unaligned_be16(min_io_sectors, &rbuf[6]);

	 
	if (ata_id_has_trim(args->id)) {
		u64 max_blocks = 65535 * ATA_MAX_TRIM_RNUM;

		if (dev->horkage & ATA_HORKAGE_MAX_TRIM_128M)
			max_blocks = 128 << (20 - SECTOR_SHIFT);

		put_unaligned_be64(max_blocks, &rbuf[36]);
		put_unaligned_be32(1, &rbuf[28]);
	}

	return 0;
}

static unsigned int ata_scsiop_inq_b1(struct ata_scsi_args *args, u8 *rbuf)
{
	int form_factor = ata_id_form_factor(args->id);
	int media_rotation_rate = ata_id_rotation_rate(args->id);
	u8 zoned = ata_id_zoned_cap(args->id);

	rbuf[1] = 0xb1;
	rbuf[3] = 0x3c;
	rbuf[4] = media_rotation_rate >> 8;
	rbuf[5] = media_rotation_rate;
	rbuf[7] = form_factor;
	if (zoned)
		rbuf[8] = (zoned << 4);

	return 0;
}

static unsigned int ata_scsiop_inq_b2(struct ata_scsi_args *args, u8 *rbuf)
{
	 
	rbuf[1] = 0xb2;
	rbuf[3] = 0x4;
	rbuf[5] = 1 << 6;	 

	return 0;
}

static unsigned int ata_scsiop_inq_b6(struct ata_scsi_args *args, u8 *rbuf)
{
	 
	rbuf[1] = 0xb6;
	rbuf[3] = 0x3C;

	 
	if (args->dev->zac_zoned_cap & 1)
		rbuf[4] |= 1;
	put_unaligned_be32(args->dev->zac_zones_optimal_open, &rbuf[8]);
	put_unaligned_be32(args->dev->zac_zones_optimal_nonseq, &rbuf[12]);
	put_unaligned_be32(args->dev->zac_zones_max_open, &rbuf[16]);

	return 0;
}

static unsigned int ata_scsiop_inq_b9(struct ata_scsi_args *args, u8 *rbuf)
{
	struct ata_cpr_log *cpr_log = args->dev->cpr_log;
	u8 *desc = &rbuf[64];
	int i;

	 
	rbuf[1] = 0xb9;
	put_unaligned_be16(64 + (int)cpr_log->nr_cpr * 32 - 4, &rbuf[2]);

	for (i = 0; i < cpr_log->nr_cpr; i++, desc += 32) {
		desc[0] = cpr_log->cpr[i].num;
		desc[1] = cpr_log->cpr[i].num_storage_elements;
		put_unaligned_be64(cpr_log->cpr[i].start_lba, &desc[8]);
		put_unaligned_be64(cpr_log->cpr[i].num_lbas, &desc[16]);
	}

	return 0;
}

 
static void modecpy(u8 *dest, const u8 *src, int n, bool changeable)
{
	if (changeable) {
		memcpy(dest, src, 2);
		memset(dest + 2, 0, n - 2);
	} else {
		memcpy(dest, src, n);
	}
}

 
static unsigned int ata_msense_caching(u16 *id, u8 *buf, bool changeable)
{
	modecpy(buf, def_cache_mpage, sizeof(def_cache_mpage), changeable);
	if (changeable) {
		buf[2] |= (1 << 2);	 
	} else {
		buf[2] |= (ata_id_wcache_enabled(id) << 2);	 
		buf[12] |= (!ata_id_rahead_enabled(id) << 5);	 
	}
	return sizeof(def_cache_mpage);
}

 
static unsigned int ata_msense_control_spg0(struct ata_device *dev, u8 *buf,
					    bool changeable)
{
	modecpy(buf, def_control_mpage,
		sizeof(def_control_mpage), changeable);
	if (changeable) {
		 
		buf[2] |= (1 << 2);
	} else {
		bool d_sense = (dev->flags & ATA_DFLAG_D_SENSE);

		 
		buf[2] |= (d_sense << 2);
	}

	return sizeof(def_control_mpage);
}

 
static inline u16 ata_xlat_cdl_limit(u8 *buf)
{
	u32 limit = get_unaligned_le32(buf);

	return min_t(u32, limit / 10000, 65535);
}

 
static unsigned int ata_msense_control_spgt2(struct ata_device *dev, u8 *buf,
					     u8 spg)
{
	u8 *b, *cdl = dev->cdl, *desc;
	u32 policy;
	int i;

	 
	buf[0] = CONTROL_MPAGE;
	buf[1] = spg;
	put_unaligned_be16(CDL_T2_SUB_MPAGE_LEN - 4, &buf[2]);
	if (spg == CDL_T2A_SUB_MPAGE) {
		 
		buf[7] = (cdl[0] & 0x03) << 4;
		desc = cdl + 64;
	} else {
		 
		desc = cdl + 288;
	}

	 
	b = &buf[8];
	policy = get_unaligned_le32(&cdl[0]);
	for (i = 0; i < 7; i++, b += 32, desc += 32) {
		 
		b[0] = 0x0a;

		 
		put_unaligned_be16(ata_xlat_cdl_limit(&desc[8]), &b[2]);
		b[6] = ((policy >> 8) & 0x0f) << 4;

		 
		put_unaligned_be16(ata_xlat_cdl_limit(&desc[4]), &b[4]);
		b[6] |= (policy >> 4) & 0x0f;

		 
		put_unaligned_be16(ata_xlat_cdl_limit(&desc[16]), &b[10]);
		b[14] = policy & 0x0f;
	}

	return CDL_T2_SUB_MPAGE_LEN;
}

 
static unsigned int ata_msense_control_ata_feature(struct ata_device *dev,
						   u8 *buf)
{
	 
	buf[0] = CONTROL_MPAGE | (1 << 6);
	buf[1] = ATA_FEATURE_SUB_MPAGE;

	 
	put_unaligned_be16(ATA_FEATURE_SUB_MPAGE_LEN - 4, &buf[2]);

	if (dev->flags & ATA_DFLAG_CDL)
		buf[4] = 0x02;  
	else
		buf[4] = 0;

	return ATA_FEATURE_SUB_MPAGE_LEN;
}

 
static unsigned int ata_msense_control(struct ata_device *dev, u8 *buf,
				       u8 spg, bool changeable)
{
	unsigned int n;

	switch (spg) {
	case 0:
		return ata_msense_control_spg0(dev, buf, changeable);
	case CDL_T2A_SUB_MPAGE:
	case CDL_T2B_SUB_MPAGE:
		return ata_msense_control_spgt2(dev, buf, spg);
	case ATA_FEATURE_SUB_MPAGE:
		return ata_msense_control_ata_feature(dev, buf);
	case ALL_SUB_MPAGES:
		n = ata_msense_control_spg0(dev, buf, changeable);
		n += ata_msense_control_spgt2(dev, buf + n, CDL_T2A_SUB_MPAGE);
		n += ata_msense_control_spgt2(dev, buf + n, CDL_T2A_SUB_MPAGE);
		n += ata_msense_control_ata_feature(dev, buf + n);
		return n;
	default:
		return 0;
	}
}

 
static unsigned int ata_msense_rw_recovery(u8 *buf, bool changeable)
{
	modecpy(buf, def_rw_recovery_mpage, sizeof(def_rw_recovery_mpage),
		changeable);
	return sizeof(def_rw_recovery_mpage);
}

 
static unsigned int ata_scsiop_mode_sense(struct ata_scsi_args *args, u8 *rbuf)
{
	struct ata_device *dev = args->dev;
	u8 *scsicmd = args->cmd->cmnd, *p = rbuf;
	static const u8 sat_blk_desc[] = {
		0, 0, 0, 0,	 
		0,
		0, 0x2, 0x0	 
	};
	u8 pg, spg;
	unsigned int ebd, page_control, six_byte;
	u8 dpofua = 0, bp = 0xff;
	u16 fp;

	six_byte = (scsicmd[0] == MODE_SENSE);
	ebd = !(scsicmd[1] & 0x8);       
	 

	page_control = scsicmd[2] >> 6;
	switch (page_control) {
	case 0:  
	case 1:  
	case 2:  
		break;   
	case 3:  
		goto saving_not_supp;
	default:
		fp = 2;
		bp = 6;
		goto invalid_fld;
	}

	if (six_byte)
		p += 4 + (ebd ? 8 : 0);
	else
		p += 8 + (ebd ? 8 : 0);

	pg = scsicmd[2] & 0x3f;
	spg = scsicmd[3];

	 
	if (spg) {
		switch (spg) {
		case ALL_SUB_MPAGES:
			break;
		case CDL_T2A_SUB_MPAGE:
		case CDL_T2B_SUB_MPAGE:
		case ATA_FEATURE_SUB_MPAGE:
			if (dev->flags & ATA_DFLAG_CDL && pg == CONTROL_MPAGE)
				break;
			fallthrough;
		default:
			fp = 3;
			goto invalid_fld;
		}
	}

	switch(pg) {
	case RW_RECOVERY_MPAGE:
		p += ata_msense_rw_recovery(p, page_control == 1);
		break;

	case CACHE_MPAGE:
		p += ata_msense_caching(args->id, p, page_control == 1);
		break;

	case CONTROL_MPAGE:
		p += ata_msense_control(args->dev, p, spg, page_control == 1);
		break;

	case ALL_MPAGES:
		p += ata_msense_rw_recovery(p, page_control == 1);
		p += ata_msense_caching(args->id, p, page_control == 1);
		p += ata_msense_control(args->dev, p, spg, page_control == 1);
		break;

	default:		 
		fp = 2;
		goto invalid_fld;
	}

	if (dev->flags & ATA_DFLAG_FUA)
		dpofua = 1 << 4;

	if (six_byte) {
		rbuf[0] = p - rbuf - 1;
		rbuf[2] |= dpofua;
		if (ebd) {
			rbuf[3] = sizeof(sat_blk_desc);
			memcpy(rbuf + 4, sat_blk_desc, sizeof(sat_blk_desc));
		}
	} else {
		put_unaligned_be16(p - rbuf - 2, &rbuf[0]);
		rbuf[3] |= dpofua;
		if (ebd) {
			rbuf[7] = sizeof(sat_blk_desc);
			memcpy(rbuf + 8, sat_blk_desc, sizeof(sat_blk_desc));
		}
	}
	return 0;

invalid_fld:
	ata_scsi_set_invalid_field(dev, args->cmd, fp, bp);
	return 1;

saving_not_supp:
	ata_scsi_set_sense(dev, args->cmd, ILLEGAL_REQUEST, 0x39, 0x0);
	  
	return 1;
}

 
static unsigned int ata_scsiop_read_cap(struct ata_scsi_args *args, u8 *rbuf)
{
	struct ata_device *dev = args->dev;
	u64 last_lba = dev->n_sectors - 1;  
	u32 sector_size;  
	u8 log2_per_phys;
	u16 lowest_aligned;

	sector_size = ata_id_logical_sector_size(dev->id);
	log2_per_phys = ata_id_log2_per_physical_sector(dev->id);
	lowest_aligned = ata_id_logical_sector_offset(dev->id, log2_per_phys);

	if (args->cmd->cmnd[0] == READ_CAPACITY) {
		if (last_lba >= 0xffffffffULL)
			last_lba = 0xffffffff;

		 
		rbuf[0] = last_lba >> (8 * 3);
		rbuf[1] = last_lba >> (8 * 2);
		rbuf[2] = last_lba >> (8 * 1);
		rbuf[3] = last_lba;

		 
		rbuf[4] = sector_size >> (8 * 3);
		rbuf[5] = sector_size >> (8 * 2);
		rbuf[6] = sector_size >> (8 * 1);
		rbuf[7] = sector_size;
	} else {
		 
		rbuf[0] = last_lba >> (8 * 7);
		rbuf[1] = last_lba >> (8 * 6);
		rbuf[2] = last_lba >> (8 * 5);
		rbuf[3] = last_lba >> (8 * 4);
		rbuf[4] = last_lba >> (8 * 3);
		rbuf[5] = last_lba >> (8 * 2);
		rbuf[6] = last_lba >> (8 * 1);
		rbuf[7] = last_lba;

		 
		rbuf[ 8] = sector_size >> (8 * 3);
		rbuf[ 9] = sector_size >> (8 * 2);
		rbuf[10] = sector_size >> (8 * 1);
		rbuf[11] = sector_size;

		rbuf[12] = 0;
		rbuf[13] = log2_per_phys;
		rbuf[14] = (lowest_aligned >> 8) & 0x3f;
		rbuf[15] = lowest_aligned;

		if (ata_id_has_trim(args->id) &&
		    !(dev->horkage & ATA_HORKAGE_NOTRIM)) {
			rbuf[14] |= 0x80;  

			if (ata_id_has_zero_after_trim(args->id) &&
			    dev->horkage & ATA_HORKAGE_ZERO_AFTER_TRIM) {
				ata_dev_info(dev, "Enabling discard_zeroes_data\n");
				rbuf[14] |= 0x40;  
			}
		}
		if (ata_id_zoned_cap(args->id) ||
		    args->dev->class == ATA_DEV_ZAC)
			rbuf[12] = (1 << 4);  
	}
	return 0;
}

 
static unsigned int ata_scsiop_report_luns(struct ata_scsi_args *args, u8 *rbuf)
{
	rbuf[3] = 8;	 

	return 0;
}

 
static void atapi_fixup_inquiry(struct scsi_cmnd *cmd)
{
	u8 buf[4];

	sg_copy_to_buffer(scsi_sglist(cmd), scsi_sg_count(cmd), buf, 4);
	if (buf[2] == 0) {
		buf[2] = 0x5;
		buf[3] = 0x32;
	}
	sg_copy_from_buffer(scsi_sglist(cmd), scsi_sg_count(cmd), buf, 4);
}

static void atapi_qc_complete(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *cmd = qc->scsicmd;
	unsigned int err_mask = qc->err_mask;

	 
	if (unlikely(err_mask || qc->flags & ATA_QCFLAG_SENSE_VALID)) {

		if (!(qc->flags & ATA_QCFLAG_SENSE_VALID)) {
			 
			ata_gen_passthru_sense(qc);
		}

		 
		if (qc->cdb[0] == ALLOW_MEDIUM_REMOVAL && qc->dev->sdev)
			qc->dev->sdev->locked = 0;

		qc->scsicmd->result = SAM_STAT_CHECK_CONDITION;
		ata_qc_done(qc);
		return;
	}

	 
	if (cmd->cmnd[0] == INQUIRY && (cmd->cmnd[1] & 0x03) == 0)
		atapi_fixup_inquiry(cmd);
	cmd->result = SAM_STAT_GOOD;

	ata_qc_done(qc);
}
 
static unsigned int atapi_xlat(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *scmd = qc->scsicmd;
	struct ata_device *dev = qc->dev;
	int nodata = (scmd->sc_data_direction == DMA_NONE);
	int using_pio = !nodata && (dev->flags & ATA_DFLAG_PIO);
	unsigned int nbytes;

	memset(qc->cdb, 0, dev->cdb_len);
	memcpy(qc->cdb, scmd->cmnd, scmd->cmd_len);

	qc->complete_fn = atapi_qc_complete;

	qc->tf.flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE;
	if (scmd->sc_data_direction == DMA_TO_DEVICE) {
		qc->tf.flags |= ATA_TFLAG_WRITE;
	}

	qc->tf.command = ATA_CMD_PACKET;
	ata_qc_set_pc_nbytes(qc);

	 
	if (!nodata && !using_pio && atapi_check_dma(qc))
		using_pio = 1;

	 
	nbytes = min(ata_qc_raw_nbytes(qc), (unsigned int)63 * 1024);

	 
	if (nbytes & 0x1)
		nbytes++;

	qc->tf.lbam = (nbytes & 0xFF);
	qc->tf.lbah = (nbytes >> 8);

	if (nodata)
		qc->tf.protocol = ATAPI_PROT_NODATA;
	else if (using_pio)
		qc->tf.protocol = ATAPI_PROT_PIO;
	else {
		 
		qc->tf.protocol = ATAPI_PROT_DMA;
		qc->tf.feature |= ATAPI_PKT_DMA;

		if ((dev->flags & ATA_DFLAG_DMADIR) &&
		    (scmd->sc_data_direction != DMA_TO_DEVICE))
			 
			qc->tf.feature |= ATAPI_DMADIR;
	}


	 
	return 0;
}

static struct ata_device *ata_find_dev(struct ata_port *ap, unsigned int devno)
{
	 
	if (likely(!sata_pmp_attached(ap))) {
		int link_max_devices = ata_link_max_devices(&ap->link);

		if (link_max_devices == 1)
			return &ap->link.device[0];

		if (devno < link_max_devices)
			return &ap->link.device[devno];

		return NULL;
	}

	 
	if (devno < ap->nr_pmp_links)
		return &ap->pmp_link[devno].device[0];

	return NULL;
}

static struct ata_device *__ata_scsi_find_dev(struct ata_port *ap,
					      const struct scsi_device *scsidev)
{
	int devno;

	 
	if (!sata_pmp_attached(ap)) {
		if (unlikely(scsidev->channel || scsidev->lun))
			return NULL;
		devno = scsidev->id;
	} else {
		if (unlikely(scsidev->id || scsidev->lun))
			return NULL;
		devno = scsidev->channel;
	}

	return ata_find_dev(ap, devno);
}

 
struct ata_device *
ata_scsi_find_dev(struct ata_port *ap, const struct scsi_device *scsidev)
{
	struct ata_device *dev = __ata_scsi_find_dev(ap, scsidev);

	if (unlikely(!dev || !ata_dev_enabled(dev)))
		return NULL;

	return dev;
}

 
static u8
ata_scsi_map_proto(u8 byte1)
{
	switch((byte1 & 0x1e) >> 1) {
	case 3:		 
		return ATA_PROT_NODATA;

	case 6:		 
	case 10:	 
	case 11:	 
		return ATA_PROT_DMA;

	case 4:		 
	case 5:		 
		return ATA_PROT_PIO;

	case 12:	 
		return ATA_PROT_NCQ;

	case 0:		 
	case 1:		 
	case 8:		 
	case 9:		 
	case 7:		 
	case 15:	 
	default:	 
		break;
	}

	return ATA_PROT_UNKNOWN;
}

 
static unsigned int ata_scsi_pass_thru(struct ata_queued_cmd *qc)
{
	struct ata_taskfile *tf = &(qc->tf);
	struct scsi_cmnd *scmd = qc->scsicmd;
	struct ata_device *dev = qc->dev;
	const u8 *cdb = scmd->cmnd;
	u16 fp;
	u16 cdb_offset = 0;

	 
	if (cdb[0] == VARIABLE_LENGTH_CMD)
		cdb_offset = 9;

	tf->protocol = ata_scsi_map_proto(cdb[1 + cdb_offset]);
	if (tf->protocol == ATA_PROT_UNKNOWN) {
		fp = 1;
		goto invalid_fld;
	}

	if ((cdb[2 + cdb_offset] & 0x3) == 0) {
		 
		if (scmd->sc_data_direction != DMA_NONE) {
			fp = 2 + cdb_offset;
			goto invalid_fld;
		}

		if (ata_is_ncq(tf->protocol))
			tf->protocol = ATA_PROT_NCQ_NODATA;
	}

	 
	tf->flags |= ATA_TFLAG_LBA;

	 
	switch (cdb[0]) {
	case ATA_16:
		 
		if (cdb[1] & 0x01) {
			tf->hob_feature = cdb[3];
			tf->hob_nsect = cdb[5];
			tf->hob_lbal = cdb[7];
			tf->hob_lbam = cdb[9];
			tf->hob_lbah = cdb[11];
			tf->flags |= ATA_TFLAG_LBA48;
		} else
			tf->flags &= ~ATA_TFLAG_LBA48;

		 
		tf->feature = cdb[4];
		tf->nsect = cdb[6];
		tf->lbal = cdb[8];
		tf->lbam = cdb[10];
		tf->lbah = cdb[12];
		tf->device = cdb[13];
		tf->command = cdb[14];
		break;
	case ATA_12:
		 
		tf->flags &= ~ATA_TFLAG_LBA48;

		tf->feature = cdb[3];
		tf->nsect = cdb[4];
		tf->lbal = cdb[5];
		tf->lbam = cdb[6];
		tf->lbah = cdb[7];
		tf->device = cdb[8];
		tf->command = cdb[9];
		break;
	default:
		 
		if (cdb[10] & 0x01) {
			tf->hob_feature = cdb[20];
			tf->hob_nsect = cdb[22];
			tf->hob_lbal = cdb[16];
			tf->hob_lbam = cdb[15];
			tf->hob_lbah = cdb[14];
			tf->flags |= ATA_TFLAG_LBA48;
		} else
			tf->flags &= ~ATA_TFLAG_LBA48;

		tf->feature = cdb[21];
		tf->nsect = cdb[23];
		tf->lbal = cdb[19];
		tf->lbam = cdb[18];
		tf->lbah = cdb[17];
		tf->device = cdb[24];
		tf->command = cdb[25];
		tf->auxiliary = get_unaligned_be32(&cdb[28]);
		break;
	}

	 
	if (ata_is_ncq(tf->protocol))
		tf->nsect = qc->hw_tag << 3;

	 
	tf->device = dev->devno ?
		tf->device | ATA_DEV1 : tf->device & ~ATA_DEV1;

	switch (tf->command) {
	 
	case ATA_CMD_READ_LONG:
	case ATA_CMD_READ_LONG_ONCE:
	case ATA_CMD_WRITE_LONG:
	case ATA_CMD_WRITE_LONG_ONCE:
		if (tf->protocol != ATA_PROT_PIO || tf->nsect != 1) {
			fp = 1;
			goto invalid_fld;
		}
		qc->sect_size = scsi_bufflen(scmd);
		break;

	 
	case ATA_CMD_CFA_WRITE_NE:
	case ATA_CMD_CFA_TRANS_SECT:
	case ATA_CMD_CFA_WRITE_MULT_NE:
	 
	case ATA_CMD_READ:
	case ATA_CMD_READ_EXT:
	case ATA_CMD_READ_QUEUED:
	 
	case ATA_CMD_FPDMA_READ:
	case ATA_CMD_READ_MULTI:
	case ATA_CMD_READ_MULTI_EXT:
	case ATA_CMD_PIO_READ:
	case ATA_CMD_PIO_READ_EXT:
	case ATA_CMD_READ_STREAM_DMA_EXT:
	case ATA_CMD_READ_STREAM_EXT:
	case ATA_CMD_VERIFY:
	case ATA_CMD_VERIFY_EXT:
	case ATA_CMD_WRITE:
	case ATA_CMD_WRITE_EXT:
	case ATA_CMD_WRITE_FUA_EXT:
	case ATA_CMD_WRITE_QUEUED:
	case ATA_CMD_WRITE_QUEUED_FUA_EXT:
	case ATA_CMD_FPDMA_WRITE:
	case ATA_CMD_WRITE_MULTI:
	case ATA_CMD_WRITE_MULTI_EXT:
	case ATA_CMD_WRITE_MULTI_FUA_EXT:
	case ATA_CMD_PIO_WRITE:
	case ATA_CMD_PIO_WRITE_EXT:
	case ATA_CMD_WRITE_STREAM_DMA_EXT:
	case ATA_CMD_WRITE_STREAM_EXT:
		qc->sect_size = scmd->device->sector_size;
		break;

	 
	default:
		qc->sect_size = ATA_SECT_SIZE;
	}

	 
	tf->flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE;
	if (scmd->sc_data_direction == DMA_TO_DEVICE)
		tf->flags |= ATA_TFLAG_WRITE;

	qc->flags |= ATA_QCFLAG_RESULT_TF | ATA_QCFLAG_QUIET;

	 
	ata_qc_set_pc_nbytes(qc);

	 
	if (tf->protocol == ATA_PROT_DMA && !ata_dma_enabled(dev)) {
		fp = 1;
		goto invalid_fld;
	}

	 
	if (ata_is_ncq(tf->protocol) && !ata_ncq_enabled(dev)) {
		fp = 1;
		goto invalid_fld;
	}

	 
	if ((cdb[1] & 0xe0) && !is_multi_taskfile(tf)) {
		fp = 1;
		goto invalid_fld;
	}

	if (is_multi_taskfile(tf)) {
		unsigned int multi_count = 1 << (cdb[1] >> 5);

		 
		if (multi_count != dev->multi_count)
			ata_dev_warn(dev, "invalid multi_count %u ignored\n",
				     multi_count);
	}

	 
	if (tf->command == ATA_CMD_SET_FEATURES &&
	    tf->feature == SETFEATURES_XFER) {
		fp = (cdb[0] == ATA_16) ? 4 : 3;
		goto invalid_fld;
	}

	 
	if (tf->command >= 0x5C && tf->command <= 0x5F && !libata_allow_tpm) {
		fp = (cdb[0] == ATA_16) ? 14 : 9;
		goto invalid_fld;
	}

	return 0;

 invalid_fld:
	ata_scsi_set_invalid_field(dev, scmd, fp, 0xff);
	return 1;
}

 
static size_t ata_format_dsm_trim_descr(struct scsi_cmnd *cmd, u32 trmax,
					u64 sector, u32 count)
{
	struct scsi_device *sdp = cmd->device;
	size_t len = sdp->sector_size;
	size_t r;
	__le64 *buf;
	u32 i = 0;
	unsigned long flags;

	WARN_ON(len > ATA_SCSI_RBUF_SIZE);

	if (len > ATA_SCSI_RBUF_SIZE)
		len = ATA_SCSI_RBUF_SIZE;

	spin_lock_irqsave(&ata_scsi_rbuf_lock, flags);
	buf = ((void *)ata_scsi_rbuf);
	memset(buf, 0, len);
	while (i < trmax) {
		u64 entry = sector |
			((u64)(count > 0xffff ? 0xffff : count) << 48);
		buf[i++] = __cpu_to_le64(entry);
		if (count <= 0xffff)
			break;
		count -= 0xffff;
		sector += 0xffff;
	}
	r = sg_copy_from_buffer(scsi_sglist(cmd), scsi_sg_count(cmd), buf, len);
	spin_unlock_irqrestore(&ata_scsi_rbuf_lock, flags);

	return r;
}

 
static unsigned int ata_scsi_write_same_xlat(struct ata_queued_cmd *qc)
{
	struct ata_taskfile *tf = &qc->tf;
	struct scsi_cmnd *scmd = qc->scsicmd;
	struct scsi_device *sdp = scmd->device;
	size_t len = sdp->sector_size;
	struct ata_device *dev = qc->dev;
	const u8 *cdb = scmd->cmnd;
	u64 block;
	u32 n_block;
	const u32 trmax = len >> 3;
	u32 size;
	u16 fp;
	u8 bp = 0xff;
	u8 unmap = cdb[1] & 0x8;

	 
	if (unlikely(!ata_dma_enabled(dev)))
		goto invalid_opcode;

	 
	if (unlikely(blk_rq_is_passthrough(scsi_cmd_to_rq(scmd))))
		goto invalid_opcode;

	if (unlikely(scmd->cmd_len < 16)) {
		fp = 15;
		goto invalid_fld;
	}
	scsi_16_lba_len(cdb, &block, &n_block);

	if (!unmap ||
	    (dev->horkage & ATA_HORKAGE_NOTRIM) ||
	    !ata_id_has_trim(dev->id)) {
		fp = 1;
		bp = 3;
		goto invalid_fld;
	}
	 
	if (n_block > 0xffff * trmax) {
		fp = 2;
		goto invalid_fld;
	}

	 
	if (!scsi_sg_count(scmd))
		goto invalid_param_len;

	 

	size = ata_format_dsm_trim_descr(scmd, trmax, block, n_block);
	if (size != len)
		goto invalid_param_len;

	if (ata_ncq_enabled(dev) && ata_fpdma_dsm_supported(dev)) {
		 
		tf->protocol = ATA_PROT_NCQ;
		tf->command = ATA_CMD_FPDMA_SEND;
		tf->hob_nsect = ATA_SUBCMD_FPDMA_SEND_DSM & 0x1f;
		tf->nsect = qc->hw_tag << 3;
		tf->hob_feature = (size / 512) >> 8;
		tf->feature = size / 512;

		tf->auxiliary = 1;
	} else {
		tf->protocol = ATA_PROT_DMA;
		tf->hob_feature = 0;
		tf->feature = ATA_DSM_TRIM;
		tf->hob_nsect = (size / 512) >> 8;
		tf->nsect = size / 512;
		tf->command = ATA_CMD_DSM;
	}

	tf->flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE | ATA_TFLAG_LBA48 |
		     ATA_TFLAG_WRITE;

	ata_qc_set_pc_nbytes(qc);

	return 0;

invalid_fld:
	ata_scsi_set_invalid_field(dev, scmd, fp, bp);
	return 1;
invalid_param_len:
	 
	ata_scsi_set_sense(dev, scmd, ILLEGAL_REQUEST, 0x1a, 0x0);
	return 1;
invalid_opcode:
	 
	ata_scsi_set_sense(dev, scmd, ILLEGAL_REQUEST, 0x20, 0x0);
	return 1;
}

 
static unsigned int ata_scsiop_maint_in(struct ata_scsi_args *args, u8 *rbuf)
{
	struct ata_device *dev = args->dev;
	u8 *cdb = args->cmd->cmnd;
	u8 supported = 0, cdlp = 0, rwcdlp = 0;
	unsigned int err = 0;

	if (cdb[2] != 1 && cdb[2] != 3) {
		ata_dev_warn(dev, "invalid command format %d\n", cdb[2]);
		err = 2;
		goto out;
	}

	switch (cdb[3]) {
	case INQUIRY:
	case MODE_SENSE:
	case MODE_SENSE_10:
	case READ_CAPACITY:
	case SERVICE_ACTION_IN_16:
	case REPORT_LUNS:
	case REQUEST_SENSE:
	case SYNCHRONIZE_CACHE:
	case SYNCHRONIZE_CACHE_16:
	case REZERO_UNIT:
	case SEEK_6:
	case SEEK_10:
	case TEST_UNIT_READY:
	case SEND_DIAGNOSTIC:
	case MAINTENANCE_IN:
	case READ_6:
	case READ_10:
	case WRITE_6:
	case WRITE_10:
	case ATA_12:
	case ATA_16:
	case VERIFY:
	case VERIFY_16:
	case MODE_SELECT:
	case MODE_SELECT_10:
	case START_STOP:
		supported = 3;
		break;
	case READ_16:
		supported = 3;
		if (dev->flags & ATA_DFLAG_CDL) {
			 
			rwcdlp = 0x01;
			cdlp = 0x01 << 3;
		}
		break;
	case WRITE_16:
		supported = 3;
		if (dev->flags & ATA_DFLAG_CDL) {
			 
			rwcdlp = 0x01;
			cdlp = 0x02 << 3;
		}
		break;
	case ZBC_IN:
	case ZBC_OUT:
		if (ata_id_zoned_cap(dev->id) ||
		    dev->class == ATA_DEV_ZAC)
			supported = 3;
		break;
	case SECURITY_PROTOCOL_IN:
	case SECURITY_PROTOCOL_OUT:
		if (dev->flags & ATA_DFLAG_TRUSTED)
			supported = 3;
		break;
	default:
		break;
	}
out:
	 
	rbuf[0] = rwcdlp;
	rbuf[1] = cdlp | supported;
	return err;
}

 
static void ata_scsi_report_zones_complete(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *scmd = qc->scsicmd;
	struct sg_mapping_iter miter;
	unsigned long flags;
	unsigned int bytes = 0;

	sg_miter_start(&miter, scsi_sglist(scmd), scsi_sg_count(scmd),
		       SG_MITER_TO_SG | SG_MITER_ATOMIC);

	local_irq_save(flags);
	while (sg_miter_next(&miter)) {
		unsigned int offset = 0;

		if (bytes == 0) {
			char *hdr;
			u32 list_length;
			u64 max_lba, opt_lba;
			u16 same;

			 
			hdr = miter.addr;
			list_length = get_unaligned_le32(&hdr[0]);
			same = get_unaligned_le16(&hdr[4]);
			max_lba = get_unaligned_le64(&hdr[8]);
			opt_lba = get_unaligned_le64(&hdr[16]);
			put_unaligned_be32(list_length, &hdr[0]);
			hdr[4] = same & 0xf;
			put_unaligned_be64(max_lba, &hdr[8]);
			put_unaligned_be64(opt_lba, &hdr[16]);
			offset += 64;
			bytes += 64;
		}
		while (offset < miter.length) {
			char *rec;
			u8 cond, type, non_seq, reset;
			u64 size, start, wp;

			 
			rec = miter.addr + offset;
			type = rec[0] & 0xf;
			cond = (rec[1] >> 4) & 0xf;
			non_seq = (rec[1] & 2);
			reset = (rec[1] & 1);
			size = get_unaligned_le64(&rec[8]);
			start = get_unaligned_le64(&rec[16]);
			wp = get_unaligned_le64(&rec[24]);
			rec[0] = type;
			rec[1] = (cond << 4) | non_seq | reset;
			put_unaligned_be64(size, &rec[8]);
			put_unaligned_be64(start, &rec[16]);
			put_unaligned_be64(wp, &rec[24]);
			WARN_ON(offset + 64 > miter.length);
			offset += 64;
			bytes += 64;
		}
	}
	sg_miter_stop(&miter);
	local_irq_restore(flags);

	ata_scsi_qc_complete(qc);
}

static unsigned int ata_scsi_zbc_in_xlat(struct ata_queued_cmd *qc)
{
	struct ata_taskfile *tf = &qc->tf;
	struct scsi_cmnd *scmd = qc->scsicmd;
	const u8 *cdb = scmd->cmnd;
	u16 sect, fp = (u16)-1;
	u8 sa, options, bp = 0xff;
	u64 block;
	u32 n_block;

	if (unlikely(scmd->cmd_len < 16)) {
		ata_dev_warn(qc->dev, "invalid cdb length %d\n",
			     scmd->cmd_len);
		fp = 15;
		goto invalid_fld;
	}
	scsi_16_lba_len(cdb, &block, &n_block);
	if (n_block != scsi_bufflen(scmd)) {
		ata_dev_warn(qc->dev, "non-matching transfer count (%d/%d)\n",
			     n_block, scsi_bufflen(scmd));
		goto invalid_param_len;
	}
	sa = cdb[1] & 0x1f;
	if (sa != ZI_REPORT_ZONES) {
		ata_dev_warn(qc->dev, "invalid service action %d\n", sa);
		fp = 1;
		goto invalid_fld;
	}
	 
	if ((n_block / 512) > 0xffff || n_block < 512 || (n_block % 512)) {
		ata_dev_warn(qc->dev, "invalid transfer count %d\n", n_block);
		goto invalid_param_len;
	}
	sect = n_block / 512;
	options = cdb[14] & 0xbf;

	if (ata_ncq_enabled(qc->dev) &&
	    ata_fpdma_zac_mgmt_in_supported(qc->dev)) {
		tf->protocol = ATA_PROT_NCQ;
		tf->command = ATA_CMD_FPDMA_RECV;
		tf->hob_nsect = ATA_SUBCMD_FPDMA_RECV_ZAC_MGMT_IN & 0x1f;
		tf->nsect = qc->hw_tag << 3;
		tf->feature = sect & 0xff;
		tf->hob_feature = (sect >> 8) & 0xff;
		tf->auxiliary = ATA_SUBCMD_ZAC_MGMT_IN_REPORT_ZONES | (options << 8);
	} else {
		tf->command = ATA_CMD_ZAC_MGMT_IN;
		tf->feature = ATA_SUBCMD_ZAC_MGMT_IN_REPORT_ZONES;
		tf->protocol = ATA_PROT_DMA;
		tf->hob_feature = options;
		tf->hob_nsect = (sect >> 8) & 0xff;
		tf->nsect = sect & 0xff;
	}
	tf->device = ATA_LBA;
	tf->lbah = (block >> 16) & 0xff;
	tf->lbam = (block >> 8) & 0xff;
	tf->lbal = block & 0xff;
	tf->hob_lbah = (block >> 40) & 0xff;
	tf->hob_lbam = (block >> 32) & 0xff;
	tf->hob_lbal = (block >> 24) & 0xff;

	tf->flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE | ATA_TFLAG_LBA48;
	qc->flags |= ATA_QCFLAG_RESULT_TF;

	ata_qc_set_pc_nbytes(qc);

	qc->complete_fn = ata_scsi_report_zones_complete;

	return 0;

invalid_fld:
	ata_scsi_set_invalid_field(qc->dev, scmd, fp, bp);
	return 1;

invalid_param_len:
	 
	ata_scsi_set_sense(qc->dev, scmd, ILLEGAL_REQUEST, 0x1a, 0x0);
	return 1;
}

static unsigned int ata_scsi_zbc_out_xlat(struct ata_queued_cmd *qc)
{
	struct ata_taskfile *tf = &qc->tf;
	struct scsi_cmnd *scmd = qc->scsicmd;
	struct ata_device *dev = qc->dev;
	const u8 *cdb = scmd->cmnd;
	u8 all, sa;
	u64 block;
	u32 n_block;
	u16 fp = (u16)-1;

	if (unlikely(scmd->cmd_len < 16)) {
		fp = 15;
		goto invalid_fld;
	}

	sa = cdb[1] & 0x1f;
	if ((sa != ZO_CLOSE_ZONE) && (sa != ZO_FINISH_ZONE) &&
	    (sa != ZO_OPEN_ZONE) && (sa != ZO_RESET_WRITE_POINTER)) {
		fp = 1;
		goto invalid_fld;
	}

	scsi_16_lba_len(cdb, &block, &n_block);
	if (n_block) {
		 
		goto invalid_param_len;
	}

	all = cdb[14] & 0x1;
	if (all) {
		 
		block = 0;
	} else if (block >= dev->n_sectors) {
		 
		fp = 2;
		goto invalid_fld;
	}

	if (ata_ncq_enabled(qc->dev) &&
	    ata_fpdma_zac_mgmt_out_supported(qc->dev)) {
		tf->protocol = ATA_PROT_NCQ_NODATA;
		tf->command = ATA_CMD_NCQ_NON_DATA;
		tf->feature = ATA_SUBCMD_NCQ_NON_DATA_ZAC_MGMT_OUT;
		tf->nsect = qc->hw_tag << 3;
		tf->auxiliary = sa | ((u16)all << 8);
	} else {
		tf->protocol = ATA_PROT_NODATA;
		tf->command = ATA_CMD_ZAC_MGMT_OUT;
		tf->feature = sa;
		tf->hob_feature = all;
	}
	tf->lbah = (block >> 16) & 0xff;
	tf->lbam = (block >> 8) & 0xff;
	tf->lbal = block & 0xff;
	tf->hob_lbah = (block >> 40) & 0xff;
	tf->hob_lbam = (block >> 32) & 0xff;
	tf->hob_lbal = (block >> 24) & 0xff;
	tf->device = ATA_LBA;
	tf->flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE | ATA_TFLAG_LBA48;

	return 0;

 invalid_fld:
	ata_scsi_set_invalid_field(qc->dev, scmd, fp, 0xff);
	return 1;
invalid_param_len:
	 
	ata_scsi_set_sense(qc->dev, scmd, ILLEGAL_REQUEST, 0x1a, 0x0);
	return 1;
}

 
static int ata_mselect_caching(struct ata_queued_cmd *qc,
			       const u8 *buf, int len, u16 *fp)
{
	struct ata_taskfile *tf = &qc->tf;
	struct ata_device *dev = qc->dev;
	u8 mpage[CACHE_MPAGE_LEN];
	u8 wce;
	int i;

	 

	if (len != CACHE_MPAGE_LEN - 2) {
		*fp = min(len, CACHE_MPAGE_LEN - 2);
		return -EINVAL;
	}

	wce = buf[0] & (1 << 2);

	 
	ata_msense_caching(dev->id, mpage, false);
	for (i = 0; i < CACHE_MPAGE_LEN - 2; i++) {
		if (i == 0)
			continue;
		if (mpage[i + 2] != buf[i]) {
			*fp = i;
			return -EINVAL;
		}
	}

	tf->flags |= ATA_TFLAG_DEVICE | ATA_TFLAG_ISADDR;
	tf->protocol = ATA_PROT_NODATA;
	tf->nsect = 0;
	tf->command = ATA_CMD_SET_FEATURES;
	tf->feature = wce ? SETFEATURES_WC_ON : SETFEATURES_WC_OFF;
	return 0;
}

 
static int ata_mselect_control_spg0(struct ata_queued_cmd *qc,
				    const u8 *buf, int len, u16 *fp)
{
	struct ata_device *dev = qc->dev;
	u8 mpage[CONTROL_MPAGE_LEN];
	u8 d_sense;
	int i;

	 

	if (len != CONTROL_MPAGE_LEN - 2) {
		*fp = min(len, CONTROL_MPAGE_LEN - 2);
		return -EINVAL;
	}

	d_sense = buf[0] & (1 << 2);

	 
	ata_msense_control_spg0(dev, mpage, false);
	for (i = 0; i < CONTROL_MPAGE_LEN - 2; i++) {
		if (i == 0)
			continue;
		if (mpage[2 + i] != buf[i]) {
			*fp = i;
			return -EINVAL;
		}
	}
	if (d_sense & (1 << 2))
		dev->flags |= ATA_DFLAG_D_SENSE;
	else
		dev->flags &= ~ATA_DFLAG_D_SENSE;
	return 0;
}

 
static unsigned int ata_mselect_control_ata_feature(struct ata_queued_cmd *qc,
						    const u8 *buf, int len,
						    u16 *fp)
{
	struct ata_device *dev = qc->dev;
	struct ata_taskfile *tf = &qc->tf;
	u8 cdl_action;

	 
	if (len != ATA_FEATURE_SUB_MPAGE_LEN - 4) {
		*fp = min(len, ATA_FEATURE_SUB_MPAGE_LEN - 4);
		return -EINVAL;
	}

	 
	switch (buf[0] & 0x03) {
	case 0:
		 
		cdl_action = 0;
		dev->flags &= ~ATA_DFLAG_CDL_ENABLED;
		break;
	case 0x02:
		 
		if (dev->flags & ATA_DFLAG_NCQ_PRIO_ENABLED) {
			ata_dev_err(dev,
				"NCQ priority must be disabled to enable CDL\n");
			return -EINVAL;
		}
		cdl_action = 1;
		dev->flags |= ATA_DFLAG_CDL_ENABLED;
		break;
	default:
		*fp = 0;
		return -EINVAL;
	}

	tf->flags |= ATA_TFLAG_DEVICE | ATA_TFLAG_ISADDR;
	tf->protocol = ATA_PROT_NODATA;
	tf->command = ATA_CMD_SET_FEATURES;
	tf->feature = SETFEATURES_CDL;
	tf->nsect = cdl_action;

	return 1;
}

 
static int ata_mselect_control(struct ata_queued_cmd *qc, u8 spg,
			       const u8 *buf, int len, u16 *fp)
{
	switch (spg) {
	case 0:
		return ata_mselect_control_spg0(qc, buf, len, fp);
	case ATA_FEATURE_SUB_MPAGE:
		return ata_mselect_control_ata_feature(qc, buf, len, fp);
	default:
		return -EINVAL;
	}
}

 
static unsigned int ata_scsi_mode_select_xlat(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *scmd = qc->scsicmd;
	const u8 *cdb = scmd->cmnd;
	u8 pg, spg;
	unsigned six_byte, pg_len, hdr_len, bd_len;
	int len, ret;
	u16 fp = (u16)-1;
	u8 bp = 0xff;
	u8 buffer[64];
	const u8 *p = buffer;

	six_byte = (cdb[0] == MODE_SELECT);
	if (six_byte) {
		if (scmd->cmd_len < 5) {
			fp = 4;
			goto invalid_fld;
		}

		len = cdb[4];
		hdr_len = 4;
	} else {
		if (scmd->cmd_len < 9) {
			fp = 8;
			goto invalid_fld;
		}

		len = get_unaligned_be16(&cdb[7]);
		hdr_len = 8;
	}

	 
	if ((cdb[1] & 0x11) != 0x10) {
		fp = 1;
		bp = (cdb[1] & 0x01) ? 1 : 5;
		goto invalid_fld;
	}

	 
	if (!scsi_sg_count(scmd) || scsi_sglist(scmd)->length < len)
		goto invalid_param_len;

	 
	if (len < hdr_len)
		goto invalid_param_len;

	if (!sg_copy_to_buffer(scsi_sglist(scmd), scsi_sg_count(scmd),
			       buffer, sizeof(buffer)))
		goto invalid_param_len;

	if (six_byte)
		bd_len = p[3];
	else
		bd_len = get_unaligned_be16(&p[6]);

	len -= hdr_len;
	p += hdr_len;
	if (len < bd_len)
		goto invalid_param_len;
	if (bd_len != 0 && bd_len != 8) {
		fp = (six_byte) ? 3 : 6;
		fp += bd_len + hdr_len;
		goto invalid_param;
	}

	len -= bd_len;
	p += bd_len;
	if (len == 0)
		goto skip;

	 
	pg = p[0] & 0x3f;
	if (p[0] & 0x40) {
		if (len < 4)
			goto invalid_param_len;

		spg = p[1];
		pg_len = get_unaligned_be16(&p[2]);
		p += 4;
		len -= 4;
	} else {
		if (len < 2)
			goto invalid_param_len;

		spg = 0;
		pg_len = p[1];
		p += 2;
		len -= 2;
	}

	 
	if (spg) {
		switch (spg) {
		case ALL_SUB_MPAGES:
			 
			if (pg == CONTROL_MPAGE) {
				fp = (p[0] & 0x40) ? 1 : 0;
				fp += hdr_len + bd_len;
				goto invalid_param;
			}
			break;
		case ATA_FEATURE_SUB_MPAGE:
			if (qc->dev->flags & ATA_DFLAG_CDL &&
			    pg == CONTROL_MPAGE)
				break;
			fallthrough;
		default:
			fp = (p[0] & 0x40) ? 1 : 0;
			fp += hdr_len + bd_len;
			goto invalid_param;
		}
	}
	if (pg_len > len)
		goto invalid_param_len;

	switch (pg) {
	case CACHE_MPAGE:
		if (ata_mselect_caching(qc, p, pg_len, &fp) < 0) {
			fp += hdr_len + bd_len;
			goto invalid_param;
		}
		break;
	case CONTROL_MPAGE:
		ret = ata_mselect_control(qc, spg, p, pg_len, &fp);
		if (ret < 0) {
			fp += hdr_len + bd_len;
			goto invalid_param;
		}
		if (!ret)
			goto skip;  
		break;
	default:
		 
		fp = bd_len + hdr_len;
		goto invalid_param;
	}

	 
	if (len > pg_len)
		goto invalid_param;

	return 0;

 invalid_fld:
	ata_scsi_set_invalid_field(qc->dev, scmd, fp, bp);
	return 1;

 invalid_param:
	ata_scsi_set_invalid_parameter(qc->dev, scmd, fp);
	return 1;

 invalid_param_len:
	 
	ata_scsi_set_sense(qc->dev, scmd, ILLEGAL_REQUEST, 0x1a, 0x0);
	return 1;

 skip:
	scmd->result = SAM_STAT_GOOD;
	return 1;
}

static u8 ata_scsi_trusted_op(u32 len, bool send, bool dma)
{
	if (len == 0)
		return ATA_CMD_TRUSTED_NONDATA;
	else if (send)
		return dma ? ATA_CMD_TRUSTED_SND_DMA : ATA_CMD_TRUSTED_SND;
	else
		return dma ? ATA_CMD_TRUSTED_RCV_DMA : ATA_CMD_TRUSTED_RCV;
}

static unsigned int ata_scsi_security_inout_xlat(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *scmd = qc->scsicmd;
	const u8 *cdb = scmd->cmnd;
	struct ata_taskfile *tf = &qc->tf;
	u8 secp = cdb[1];
	bool send = (cdb[0] == SECURITY_PROTOCOL_OUT);
	u16 spsp = get_unaligned_be16(&cdb[2]);
	u32 len = get_unaligned_be32(&cdb[6]);
	bool dma = !(qc->dev->flags & ATA_DFLAG_PIO);

	 
	if (secp == 0xef) {
		ata_scsi_set_invalid_field(qc->dev, scmd, 1, 0);
		return 1;
	}

	if (cdb[4] & 7) {  
		if (len > 0xffff) {
			ata_scsi_set_invalid_field(qc->dev, scmd, 6, 0);
			return 1;
		}
	} else {
		if (len > 0x01fffe00) {
			ata_scsi_set_invalid_field(qc->dev, scmd, 6, 0);
			return 1;
		}

		 
		len = (len + 511) / 512;
	}

	tf->protocol = dma ? ATA_PROT_DMA : ATA_PROT_PIO;
	tf->flags |= ATA_TFLAG_DEVICE | ATA_TFLAG_ISADDR | ATA_TFLAG_LBA;
	if (send)
		tf->flags |= ATA_TFLAG_WRITE;
	tf->command = ata_scsi_trusted_op(len, send, dma);
	tf->feature = secp;
	tf->lbam = spsp & 0xff;
	tf->lbah = spsp >> 8;

	if (len) {
		tf->nsect = len & 0xff;
		tf->lbal = len >> 8;
	} else {
		if (!send)
			tf->lbah = (1 << 7);
	}

	ata_qc_set_pc_nbytes(qc);
	return 0;
}

 
static unsigned int ata_scsi_var_len_cdb_xlat(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *scmd = qc->scsicmd;
	const u8 *cdb = scmd->cmnd;
	const u16 sa = get_unaligned_be16(&cdb[8]);

	 
	if (sa == ATA_32)
		return ata_scsi_pass_thru(qc);

	 
	return 1;
}

 

static inline ata_xlat_func_t ata_get_xlat_func(struct ata_device *dev, u8 cmd)
{
	switch (cmd) {
	case READ_6:
	case READ_10:
	case READ_16:

	case WRITE_6:
	case WRITE_10:
	case WRITE_16:
		return ata_scsi_rw_xlat;

	case WRITE_SAME_16:
		return ata_scsi_write_same_xlat;

	case SYNCHRONIZE_CACHE:
	case SYNCHRONIZE_CACHE_16:
		if (ata_try_flush_cache(dev))
			return ata_scsi_flush_xlat;
		break;

	case VERIFY:
	case VERIFY_16:
		return ata_scsi_verify_xlat;

	case ATA_12:
	case ATA_16:
		return ata_scsi_pass_thru;

	case VARIABLE_LENGTH_CMD:
		return ata_scsi_var_len_cdb_xlat;

	case MODE_SELECT:
	case MODE_SELECT_10:
		return ata_scsi_mode_select_xlat;

	case ZBC_IN:
		return ata_scsi_zbc_in_xlat;

	case ZBC_OUT:
		return ata_scsi_zbc_out_xlat;

	case SECURITY_PROTOCOL_IN:
	case SECURITY_PROTOCOL_OUT:
		if (!(dev->flags & ATA_DFLAG_TRUSTED))
			break;
		return ata_scsi_security_inout_xlat;

	case START_STOP:
		return ata_scsi_start_stop_xlat;
	}

	return NULL;
}

int __ata_scsi_queuecmd(struct scsi_cmnd *scmd, struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;
	u8 scsi_op = scmd->cmnd[0];
	ata_xlat_func_t xlat_func;

	 
	if (ap->pflags & (ATA_PFLAG_EH_PENDING | ATA_PFLAG_EH_IN_PROGRESS))
		return SCSI_MLQUEUE_DEVICE_BUSY;

	if (unlikely(!scmd->cmd_len))
		goto bad_cdb_len;

	if (dev->class == ATA_DEV_ATA || dev->class == ATA_DEV_ZAC) {
		if (unlikely(scmd->cmd_len > dev->cdb_len))
			goto bad_cdb_len;

		xlat_func = ata_get_xlat_func(dev, scsi_op);
	} else if (likely((scsi_op != ATA_16) || !atapi_passthru16)) {
		 
		int len = COMMAND_SIZE(scsi_op);

		if (unlikely(len > scmd->cmd_len ||
			     len > dev->cdb_len ||
			     scmd->cmd_len > ATAPI_CDB_LEN))
			goto bad_cdb_len;

		xlat_func = atapi_xlat;
	} else {
		 
		if (unlikely(scmd->cmd_len > 16))
			goto bad_cdb_len;

		xlat_func = ata_get_xlat_func(dev, scsi_op);
	}

	if (xlat_func)
		return ata_scsi_translate(dev, scmd, xlat_func);

	ata_scsi_simulate(dev, scmd);

	return 0;

 bad_cdb_len:
	scmd->result = DID_ERROR << 16;
	scsi_done(scmd);
	return 0;
}

 
int ata_scsi_queuecmd(struct Scsi_Host *shost, struct scsi_cmnd *cmd)
{
	struct ata_port *ap;
	struct ata_device *dev;
	struct scsi_device *scsidev = cmd->device;
	int rc = 0;
	unsigned long irq_flags;

	ap = ata_shost_to_port(shost);

	spin_lock_irqsave(ap->lock, irq_flags);

	dev = ata_scsi_find_dev(ap, scsidev);
	if (likely(dev))
		rc = __ata_scsi_queuecmd(cmd, dev);
	else {
		cmd->result = (DID_BAD_TARGET << 16);
		scsi_done(cmd);
	}

	spin_unlock_irqrestore(ap->lock, irq_flags);

	return rc;
}
EXPORT_SYMBOL_GPL(ata_scsi_queuecmd);

 

void ata_scsi_simulate(struct ata_device *dev, struct scsi_cmnd *cmd)
{
	struct ata_scsi_args args;
	const u8 *scsicmd = cmd->cmnd;
	u8 tmp8;

	args.dev = dev;
	args.id = dev->id;
	args.cmd = cmd;

	switch(scsicmd[0]) {
	case INQUIRY:
		if (scsicmd[1] & 2)		    
			ata_scsi_set_invalid_field(dev, cmd, 1, 0xff);
		else if ((scsicmd[1] & 1) == 0)     
			ata_scsi_rbuf_fill(&args, ata_scsiop_inq_std);
		else switch (scsicmd[2]) {
		case 0x00:
			ata_scsi_rbuf_fill(&args, ata_scsiop_inq_00);
			break;
		case 0x80:
			ata_scsi_rbuf_fill(&args, ata_scsiop_inq_80);
			break;
		case 0x83:
			ata_scsi_rbuf_fill(&args, ata_scsiop_inq_83);
			break;
		case 0x89:
			ata_scsi_rbuf_fill(&args, ata_scsiop_inq_89);
			break;
		case 0xb0:
			ata_scsi_rbuf_fill(&args, ata_scsiop_inq_b0);
			break;
		case 0xb1:
			ata_scsi_rbuf_fill(&args, ata_scsiop_inq_b1);
			break;
		case 0xb2:
			ata_scsi_rbuf_fill(&args, ata_scsiop_inq_b2);
			break;
		case 0xb6:
			if (dev->flags & ATA_DFLAG_ZAC)
				ata_scsi_rbuf_fill(&args, ata_scsiop_inq_b6);
			else
				ata_scsi_set_invalid_field(dev, cmd, 2, 0xff);
			break;
		case 0xb9:
			if (dev->cpr_log)
				ata_scsi_rbuf_fill(&args, ata_scsiop_inq_b9);
			else
				ata_scsi_set_invalid_field(dev, cmd, 2, 0xff);
			break;
		default:
			ata_scsi_set_invalid_field(dev, cmd, 2, 0xff);
			break;
		}
		break;

	case MODE_SENSE:
	case MODE_SENSE_10:
		ata_scsi_rbuf_fill(&args, ata_scsiop_mode_sense);
		break;

	case READ_CAPACITY:
		ata_scsi_rbuf_fill(&args, ata_scsiop_read_cap);
		break;

	case SERVICE_ACTION_IN_16:
		if ((scsicmd[1] & 0x1f) == SAI_READ_CAPACITY_16)
			ata_scsi_rbuf_fill(&args, ata_scsiop_read_cap);
		else
			ata_scsi_set_invalid_field(dev, cmd, 1, 0xff);
		break;

	case REPORT_LUNS:
		ata_scsi_rbuf_fill(&args, ata_scsiop_report_luns);
		break;

	case REQUEST_SENSE:
		ata_scsi_set_sense(dev, cmd, 0, 0, 0);
		break;

	 
	case SYNCHRONIZE_CACHE:
	case SYNCHRONIZE_CACHE_16:
		fallthrough;

	 
	case REZERO_UNIT:
	case SEEK_6:
	case SEEK_10:
	case TEST_UNIT_READY:
		break;

	case SEND_DIAGNOSTIC:
		tmp8 = scsicmd[1] & ~(1 << 3);
		if (tmp8 != 0x4 || scsicmd[3] || scsicmd[4])
			ata_scsi_set_invalid_field(dev, cmd, 1, 0xff);
		break;

	case MAINTENANCE_IN:
		if ((scsicmd[1] & 0x1f) == MI_REPORT_SUPPORTED_OPERATION_CODES)
			ata_scsi_rbuf_fill(&args, ata_scsiop_maint_in);
		else
			ata_scsi_set_invalid_field(dev, cmd, 1, 0xff);
		break;

	 
	default:
		ata_scsi_set_sense(dev, cmd, ILLEGAL_REQUEST, 0x20, 0x0);
		 
		break;
	}

	scsi_done(cmd);
}

int ata_scsi_add_hosts(struct ata_host *host, const struct scsi_host_template *sht)
{
	int i, rc;

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];
		struct Scsi_Host *shost;

		rc = -ENOMEM;
		shost = scsi_host_alloc(sht, sizeof(struct ata_port *));
		if (!shost)
			goto err_alloc;

		shost->eh_noresume = 1;
		*(struct ata_port **)&shost->hostdata[0] = ap;
		ap->scsi_host = shost;

		shost->transportt = ata_scsi_transport_template;
		shost->unique_id = ap->print_id;
		shost->max_id = 16;
		shost->max_lun = 1;
		shost->max_channel = 1;
		shost->max_cmd_len = 32;

		 
		shost->max_host_blocked = 1;

		rc = scsi_add_host_with_dma(shost, &ap->tdev, ap->host->dev);
		if (rc)
			goto err_alloc;
	}

	return 0;

 err_alloc:
	while (--i >= 0) {
		struct Scsi_Host *shost = host->ports[i]->scsi_host;

		 
		scsi_remove_host(shost);
	}
	return rc;
}

#ifdef CONFIG_OF
static void ata_scsi_assign_ofnode(struct ata_device *dev, struct ata_port *ap)
{
	struct scsi_device *sdev = dev->sdev;
	struct device *d = ap->host->dev;
	struct device_node *np = d->of_node;
	struct device_node *child;

	for_each_available_child_of_node(np, child) {
		int ret;
		u32 val;

		ret = of_property_read_u32(child, "reg", &val);
		if (ret)
			continue;
		if (val == dev->devno) {
			dev_dbg(d, "found matching device node\n");
			sdev->sdev_gendev.of_node = child;
			return;
		}
	}
}
#else
static void ata_scsi_assign_ofnode(struct ata_device *dev, struct ata_port *ap)
{
}
#endif

void ata_scsi_scan_host(struct ata_port *ap, int sync)
{
	int tries = 5;
	struct ata_device *last_failed_dev = NULL;
	struct ata_link *link;
	struct ata_device *dev;

 repeat:
	ata_for_each_link(link, ap, EDGE) {
		ata_for_each_dev(dev, link, ENABLED) {
			struct scsi_device *sdev;
			int channel = 0, id = 0;

			if (dev->sdev)
				continue;

			if (ata_is_host_link(link))
				id = dev->devno;
			else
				channel = link->pmp;

			sdev = __scsi_add_device(ap->scsi_host, channel, id, 0,
						 NULL);
			if (!IS_ERR(sdev)) {
				dev->sdev = sdev;
				ata_scsi_assign_ofnode(dev, ap);
				scsi_device_put(sdev);
			} else {
				dev->sdev = NULL;
			}
		}
	}

	 
	ata_for_each_link(link, ap, EDGE) {
		ata_for_each_dev(dev, link, ENABLED) {
			if (!dev->sdev)
				goto exit_loop;
		}
	}
 exit_loop:
	if (!link)
		return;

	 
	if (sync) {
		 
		if (dev != last_failed_dev) {
			msleep(100);
			last_failed_dev = dev;
			goto repeat;
		}

		 
		if (--tries) {
			msleep(100);
			goto repeat;
		}

		ata_port_err(ap,
			     "WARNING: synchronous SCSI scan failed without making any progress, switching to async\n");
	}

	queue_delayed_work(system_long_wq, &ap->hotplug_task,
			   round_jiffies_relative(HZ));
}

 
int ata_scsi_offline_dev(struct ata_device *dev)
{
	if (dev->sdev) {
		scsi_device_set_state(dev->sdev, SDEV_OFFLINE);
		return 1;
	}
	return 0;
}

 
static void ata_scsi_remove_dev(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;
	struct scsi_device *sdev;
	unsigned long flags;

	 
	mutex_lock(&ap->scsi_host->scan_mutex);
	spin_lock_irqsave(ap->lock, flags);

	 
	sdev = dev->sdev;
	dev->sdev = NULL;

	if (sdev) {
		 
		if (scsi_device_get(sdev) == 0) {
			 
			scsi_device_set_state(sdev, SDEV_OFFLINE);
		} else {
			WARN_ON(1);
			sdev = NULL;
		}
	}

	spin_unlock_irqrestore(ap->lock, flags);
	mutex_unlock(&ap->scsi_host->scan_mutex);

	if (sdev) {
		ata_dev_info(dev, "detaching (SCSI %s)\n",
			     dev_name(&sdev->sdev_gendev));

		scsi_remove_device(sdev);
		scsi_device_put(sdev);
	}
}

static void ata_scsi_handle_link_detach(struct ata_link *link)
{
	struct ata_port *ap = link->ap;
	struct ata_device *dev;

	ata_for_each_dev(dev, link, ALL) {
		unsigned long flags;

		if (!(dev->flags & ATA_DFLAG_DETACHED))
			continue;

		spin_lock_irqsave(ap->lock, flags);
		dev->flags &= ~ATA_DFLAG_DETACHED;
		spin_unlock_irqrestore(ap->lock, flags);

		if (zpodd_dev_enabled(dev))
			zpodd_exit(dev);

		ata_scsi_remove_dev(dev);
	}
}

 
void ata_scsi_media_change_notify(struct ata_device *dev)
{
	if (dev->sdev)
		sdev_evt_send_simple(dev->sdev, SDEV_EVT_MEDIA_CHANGE,
				     GFP_ATOMIC);
}

 
void ata_scsi_hotplug(struct work_struct *work)
{
	struct ata_port *ap =
		container_of(work, struct ata_port, hotplug_task.work);
	int i;

	if (ap->pflags & ATA_PFLAG_UNLOADING)
		return;

	mutex_lock(&ap->scsi_scan_mutex);

	 
	ata_scsi_handle_link_detach(&ap->link);
	if (ap->pmp_link)
		for (i = 0; i < SATA_PMP_MAX_PORTS; i++)
			ata_scsi_handle_link_detach(&ap->pmp_link[i]);

	 
	ata_scsi_scan_host(ap, 0);

	mutex_unlock(&ap->scsi_scan_mutex);
}

 
int ata_scsi_user_scan(struct Scsi_Host *shost, unsigned int channel,
		       unsigned int id, u64 lun)
{
	struct ata_port *ap = ata_shost_to_port(shost);
	unsigned long flags;
	int devno, rc = 0;

	if (lun != SCAN_WILD_CARD && lun)
		return -EINVAL;

	if (!sata_pmp_attached(ap)) {
		if (channel != SCAN_WILD_CARD && channel)
			return -EINVAL;
		devno = id;
	} else {
		if (id != SCAN_WILD_CARD && id)
			return -EINVAL;
		devno = channel;
	}

	spin_lock_irqsave(ap->lock, flags);

	if (devno == SCAN_WILD_CARD) {
		struct ata_link *link;

		ata_for_each_link(link, ap, EDGE) {
			struct ata_eh_info *ehi = &link->eh_info;
			ehi->probe_mask |= ATA_ALL_DEVICES;
			ehi->action |= ATA_EH_RESET;
		}
	} else {
		struct ata_device *dev = ata_find_dev(ap, devno);

		if (dev) {
			struct ata_eh_info *ehi = &dev->link->eh_info;
			ehi->probe_mask |= 1 << dev->devno;
			ehi->action |= ATA_EH_RESET;
		} else
			rc = -EINVAL;
	}

	if (rc == 0) {
		ata_port_schedule_eh(ap);
		spin_unlock_irqrestore(ap->lock, flags);
		ata_port_wait_eh(ap);
	} else
		spin_unlock_irqrestore(ap->lock, flags);

	return rc;
}

 
void ata_scsi_dev_rescan(struct work_struct *work)
{
	struct ata_port *ap =
		container_of(work, struct ata_port, scsi_rescan_task.work);
	struct ata_link *link;
	struct ata_device *dev;
	unsigned long flags;
	int ret = 0;

	mutex_lock(&ap->scsi_scan_mutex);
	spin_lock_irqsave(ap->lock, flags);

	ata_for_each_link(link, ap, EDGE) {
		ata_for_each_dev(dev, link, ENABLED) {
			struct scsi_device *sdev = dev->sdev;

			 
			if (ap->pflags & ATA_PFLAG_SUSPENDED)
				goto unlock;

			if (!sdev)
				continue;
			if (scsi_device_get(sdev))
				continue;

			spin_unlock_irqrestore(ap->lock, flags);
			ret = scsi_rescan_device(sdev);
			scsi_device_put(sdev);
			spin_lock_irqsave(ap->lock, flags);

			if (ret)
				goto unlock;
		}
	}

unlock:
	spin_unlock_irqrestore(ap->lock, flags);
	mutex_unlock(&ap->scsi_scan_mutex);

	 
	if (ret)
		schedule_delayed_work(&ap->scsi_rescan_task,
				      msecs_to_jiffies(5));
}
