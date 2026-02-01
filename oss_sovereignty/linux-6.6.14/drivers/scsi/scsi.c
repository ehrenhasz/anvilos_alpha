
 

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/unistd.h>
#include <linux/spinlock.h>
#include <linux/kmod.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/mutex.h>
#include <asm/unaligned.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_driver.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>

#include "scsi_priv.h"
#include "scsi_logging.h"

#define CREATE_TRACE_POINTS
#include <trace/events/scsi.h>

 

 
unsigned int scsi_logging_level;
#if defined(CONFIG_SCSI_LOGGING)
EXPORT_SYMBOL(scsi_logging_level);
#endif

#ifdef CONFIG_SCSI_LOGGING
void scsi_log_send(struct scsi_cmnd *cmd)
{
	unsigned int level;

	 
	if (unlikely(scsi_logging_level)) {
		level = SCSI_LOG_LEVEL(SCSI_LOG_MLQUEUE_SHIFT,
				       SCSI_LOG_MLQUEUE_BITS);
		if (level > 1) {
			scmd_printk(KERN_INFO, cmd,
				    "Send: scmd 0x%p\n", cmd);
			scsi_print_command(cmd);
		}
	}
}

void scsi_log_completion(struct scsi_cmnd *cmd, int disposition)
{
	unsigned int level;

	 
	if (unlikely(scsi_logging_level)) {
		level = SCSI_LOG_LEVEL(SCSI_LOG_MLCOMPLETE_SHIFT,
				       SCSI_LOG_MLCOMPLETE_BITS);
		if (((level > 0) && (cmd->result || disposition != SUCCESS)) ||
		    (level > 1)) {
			scsi_print_result(cmd, "Done", disposition);
			scsi_print_command(cmd);
			if (scsi_status_is_check_condition(cmd->result))
				scsi_print_sense(cmd);
			if (level > 3)
				scmd_printk(KERN_INFO, cmd,
					    "scsi host busy %d failed %d\n",
					    scsi_host_busy(cmd->device->host),
					    cmd->device->host->host_failed);
		}
	}
}
#endif

 
void scsi_finish_command(struct scsi_cmnd *cmd)
{
	struct scsi_device *sdev = cmd->device;
	struct scsi_target *starget = scsi_target(sdev);
	struct Scsi_Host *shost = sdev->host;
	struct scsi_driver *drv;
	unsigned int good_bytes;

	scsi_device_unbusy(sdev, cmd);

	 
	if (atomic_read(&shost->host_blocked))
		atomic_set(&shost->host_blocked, 0);
	if (atomic_read(&starget->target_blocked))
		atomic_set(&starget->target_blocked, 0);
	if (atomic_read(&sdev->device_blocked))
		atomic_set(&sdev->device_blocked, 0);

	SCSI_LOG_MLCOMPLETE(4, sdev_printk(KERN_INFO, sdev,
				"Notifying upper driver of completion "
				"(result %x)\n", cmd->result));

	good_bytes = scsi_bufflen(cmd);
	if (!blk_rq_is_passthrough(scsi_cmd_to_rq(cmd))) {
		int old_good_bytes = good_bytes;
		drv = scsi_cmd_to_driver(cmd);
		if (drv->done)
			good_bytes = drv->done(cmd);
		 
		if (good_bytes == old_good_bytes)
			good_bytes -= scsi_get_resid(cmd);
	}
	scsi_io_completion(cmd, good_bytes);
}


 
int scsi_device_max_queue_depth(struct scsi_device *sdev)
{
	return min_t(int, sdev->host->can_queue, 4096);
}

 
int scsi_change_queue_depth(struct scsi_device *sdev, int depth)
{
	depth = min_t(int, depth, scsi_device_max_queue_depth(sdev));

	if (depth > 0) {
		sdev->queue_depth = depth;
		wmb();
	}

	if (sdev->request_queue)
		blk_set_queue_depth(sdev->request_queue, depth);

	sbitmap_resize(&sdev->budget_map, sdev->queue_depth);

	return sdev->queue_depth;
}
EXPORT_SYMBOL(scsi_change_queue_depth);

 
int scsi_track_queue_full(struct scsi_device *sdev, int depth)
{

	 
	if ((jiffies >> 4) == (sdev->last_queue_full_time >> 4))
		return 0;

	sdev->last_queue_full_time = jiffies;
	if (sdev->last_queue_full_depth != depth) {
		sdev->last_queue_full_count = 1;
		sdev->last_queue_full_depth = depth;
	} else {
		sdev->last_queue_full_count++;
	}

	if (sdev->last_queue_full_count <= 10)
		return 0;

	return scsi_change_queue_depth(sdev, depth);
}
EXPORT_SYMBOL(scsi_track_queue_full);

 
static int scsi_vpd_inquiry(struct scsi_device *sdev, unsigned char *buffer,
							u8 page, unsigned len)
{
	int result;
	unsigned char cmd[16];

	if (len < 4)
		return -EINVAL;

	cmd[0] = INQUIRY;
	cmd[1] = 1;		 
	cmd[2] = page;
	cmd[3] = len >> 8;
	cmd[4] = len & 0xff;
	cmd[5] = 0;		 

	 
	result = scsi_execute_cmd(sdev, cmd, REQ_OP_DRV_IN, buffer, len,
				  30 * HZ, 3, NULL);
	if (result)
		return -EIO;

	 
	if (buffer[1] != page)
		return -EIO;

	result = get_unaligned_be16(&buffer[2]);
	if (!result)
		return -EIO;

	return result + 4;
}

static int scsi_get_vpd_size(struct scsi_device *sdev, u8 page)
{
	unsigned char vpd_header[SCSI_VPD_HEADER_SIZE] __aligned(4);
	int result;

	if (sdev->no_vpd_size)
		return SCSI_DEFAULT_VPD_LEN;

	 
	result = scsi_vpd_inquiry(sdev, vpd_header, page, sizeof(vpd_header));
	if (result < 0)
		return 0;

	if (result < SCSI_VPD_HEADER_SIZE) {
		dev_warn_once(&sdev->sdev_gendev,
			      "%s: short VPD page 0x%02x length: %d bytes\n",
			      __func__, page, result);
		return 0;
	}

	return result;
}

 
int scsi_get_vpd_page(struct scsi_device *sdev, u8 page, unsigned char *buf,
		      int buf_len)
{
	int result, vpd_len;

	if (!scsi_device_supports_vpd(sdev))
		return -EINVAL;

	vpd_len = scsi_get_vpd_size(sdev, page);
	if (vpd_len <= 0)
		return -EINVAL;

	vpd_len = min(vpd_len, buf_len);

	 
	memset(buf, 0, buf_len);
	result = scsi_vpd_inquiry(sdev, buf, page, vpd_len);
	if (result < 0)
		return -EINVAL;
	else if (result > vpd_len)
		dev_warn_once(&sdev->sdev_gendev,
			      "%s: VPD page 0x%02x result %d > %d bytes\n",
			      __func__, page, result, vpd_len);

	return 0;
}
EXPORT_SYMBOL_GPL(scsi_get_vpd_page);

 
static struct scsi_vpd *scsi_get_vpd_buf(struct scsi_device *sdev, u8 page)
{
	struct scsi_vpd *vpd_buf;
	int vpd_len, result;

	vpd_len = scsi_get_vpd_size(sdev, page);
	if (vpd_len <= 0)
		return NULL;

retry_pg:
	 
	vpd_buf = kmalloc(sizeof(*vpd_buf) + vpd_len, GFP_KERNEL);
	if (!vpd_buf)
		return NULL;

	result = scsi_vpd_inquiry(sdev, vpd_buf->data, page, vpd_len);
	if (result < 0) {
		kfree(vpd_buf);
		return NULL;
	}
	if (result > vpd_len) {
		dev_warn_once(&sdev->sdev_gendev,
			      "%s: VPD page 0x%02x result %d > %d bytes\n",
			      __func__, page, result, vpd_len);
		vpd_len = result;
		kfree(vpd_buf);
		goto retry_pg;
	}

	vpd_buf->len = result;

	return vpd_buf;
}

static void scsi_update_vpd_page(struct scsi_device *sdev, u8 page,
				 struct scsi_vpd __rcu **sdev_vpd_buf)
{
	struct scsi_vpd *vpd_buf;

	vpd_buf = scsi_get_vpd_buf(sdev, page);
	if (!vpd_buf)
		return;

	mutex_lock(&sdev->inquiry_mutex);
	vpd_buf = rcu_replace_pointer(*sdev_vpd_buf, vpd_buf,
				      lockdep_is_held(&sdev->inquiry_mutex));
	mutex_unlock(&sdev->inquiry_mutex);

	if (vpd_buf)
		kfree_rcu(vpd_buf, rcu);
}

 
void scsi_attach_vpd(struct scsi_device *sdev)
{
	int i;
	struct scsi_vpd *vpd_buf;

	if (!scsi_device_supports_vpd(sdev))
		return;

	 
	vpd_buf = scsi_get_vpd_buf(sdev, 0);
	if (!vpd_buf)
		return;

	for (i = 4; i < vpd_buf->len; i++) {
		if (vpd_buf->data[i] == 0x0)
			scsi_update_vpd_page(sdev, 0x0, &sdev->vpd_pg0);
		if (vpd_buf->data[i] == 0x80)
			scsi_update_vpd_page(sdev, 0x80, &sdev->vpd_pg80);
		if (vpd_buf->data[i] == 0x83)
			scsi_update_vpd_page(sdev, 0x83, &sdev->vpd_pg83);
		if (vpd_buf->data[i] == 0x89)
			scsi_update_vpd_page(sdev, 0x89, &sdev->vpd_pg89);
		if (vpd_buf->data[i] == 0xb0)
			scsi_update_vpd_page(sdev, 0xb0, &sdev->vpd_pgb0);
		if (vpd_buf->data[i] == 0xb1)
			scsi_update_vpd_page(sdev, 0xb1, &sdev->vpd_pgb1);
		if (vpd_buf->data[i] == 0xb2)
			scsi_update_vpd_page(sdev, 0xb2, &sdev->vpd_pgb2);
	}
	kfree(vpd_buf);
}

 
int scsi_report_opcode(struct scsi_device *sdev, unsigned char *buffer,
		       unsigned int len, unsigned char opcode,
		       unsigned short sa)
{
	unsigned char cmd[16];
	struct scsi_sense_hdr sshdr;
	int result, request_len;
	const struct scsi_exec_args exec_args = {
		.sshdr = &sshdr,
	};

	if (sdev->no_report_opcodes || sdev->scsi_level < SCSI_SPC_3)
		return -EINVAL;

	 
	request_len = 4 + COMMAND_SIZE(opcode);
	if (request_len > len) {
		dev_warn_once(&sdev->sdev_gendev,
			      "%s: len %u bytes, opcode 0x%02x needs %u\n",
			      __func__, len, opcode, request_len);
		return -EINVAL;
	}

	memset(cmd, 0, 16);
	cmd[0] = MAINTENANCE_IN;
	cmd[1] = MI_REPORT_SUPPORTED_OPERATION_CODES;
	if (!sa) {
		cmd[2] = 1;	 
		cmd[3] = opcode;
	} else {
		cmd[2] = 3;	 
		cmd[3] = opcode;
		put_unaligned_be16(sa, &cmd[4]);
	}
	put_unaligned_be32(request_len, &cmd[6]);
	memset(buffer, 0, len);

	result = scsi_execute_cmd(sdev, cmd, REQ_OP_DRV_IN, buffer,
				  request_len, 30 * HZ, 3, &exec_args);
	if (result < 0)
		return result;
	if (result && scsi_sense_valid(&sshdr) &&
	    sshdr.sense_key == ILLEGAL_REQUEST &&
	    (sshdr.asc == 0x20 || sshdr.asc == 0x24) && sshdr.ascq == 0x00)
		return -EINVAL;

	if ((buffer[1] & 3) == 3)  
		return 1;

	return 0;
}
EXPORT_SYMBOL(scsi_report_opcode);

#define SCSI_CDL_CHECK_BUF_LEN	64

static bool scsi_cdl_check_cmd(struct scsi_device *sdev, u8 opcode, u16 sa,
			       unsigned char *buf)
{
	int ret;
	u8 cdlp;

	 
	ret = scsi_report_opcode(sdev, buf, SCSI_CDL_CHECK_BUF_LEN, opcode, sa);
	if (ret <= 0)
		return false;

	if ((buf[1] & 0x03) != 0x03)
		return false;

	 
	cdlp = (buf[1] & 0x18) >> 3;

	return cdlp == 0x01 || cdlp == 0x02;
}

 
void scsi_cdl_check(struct scsi_device *sdev)
{
	bool cdl_supported;
	unsigned char *buf;

	 
	if (sdev->scsi_level < SCSI_SPC_5) {
		sdev->cdl_supported = 0;
		return;
	}

	buf = kmalloc(SCSI_CDL_CHECK_BUF_LEN, GFP_KERNEL);
	if (!buf) {
		sdev->cdl_supported = 0;
		return;
	}

	 
	cdl_supported =
		scsi_cdl_check_cmd(sdev, READ_16, 0, buf) ||
		scsi_cdl_check_cmd(sdev, WRITE_16, 0, buf) ||
		scsi_cdl_check_cmd(sdev, VARIABLE_LENGTH_CMD, READ_32, buf) ||
		scsi_cdl_check_cmd(sdev, VARIABLE_LENGTH_CMD, WRITE_32, buf);
	if (cdl_supported) {
		 
		sdev->use_16_for_rw = 1;
		sdev->use_10_for_rw = 0;

		sdev->cdl_supported = 1;
	} else {
		sdev->cdl_supported = 0;
	}

	kfree(buf);
}

 
int scsi_cdl_enable(struct scsi_device *sdev, bool enable)
{
	struct scsi_mode_data data;
	struct scsi_sense_hdr sshdr;
	struct scsi_vpd *vpd;
	bool is_ata = false;
	char buf[64];
	int ret;

	if (!sdev->cdl_supported)
		return -EOPNOTSUPP;

	rcu_read_lock();
	vpd = rcu_dereference(sdev->vpd_pg89);
	if (vpd)
		is_ata = true;
	rcu_read_unlock();

	 
	if (is_ata) {
		char *buf_data;
		int len;

		ret = scsi_mode_sense(sdev, 0x08, 0x0a, 0xf2, buf, sizeof(buf),
				      5 * HZ, 3, &data, NULL);
		if (ret)
			return -EINVAL;

		 
		len = min_t(size_t, sizeof(buf),
			    data.length - data.header_length -
			    data.block_descriptor_length);
		buf_data = buf + data.header_length +
			data.block_descriptor_length;
		if (enable)
			buf_data[4] = 0x02;
		else
			buf_data[4] = 0;

		ret = scsi_mode_select(sdev, 1, 0, buf_data, len, 5 * HZ, 3,
				       &data, &sshdr);
		if (ret) {
			if (scsi_sense_valid(&sshdr))
				scsi_print_sense_hdr(sdev,
					dev_name(&sdev->sdev_gendev), &sshdr);
			return ret;
		}
	}

	sdev->cdl_enable = enable;

	return 0;
}

 
int scsi_device_get(struct scsi_device *sdev)
{
	if (sdev->sdev_state == SDEV_DEL || sdev->sdev_state == SDEV_CANCEL)
		goto fail;
	if (!try_module_get(sdev->host->hostt->module))
		goto fail;
	if (!get_device(&sdev->sdev_gendev))
		goto fail_put_module;
	return 0;

fail_put_module:
	module_put(sdev->host->hostt->module);
fail:
	return -ENXIO;
}
EXPORT_SYMBOL(scsi_device_get);

 
void scsi_device_put(struct scsi_device *sdev)
{
	struct module *mod = sdev->host->hostt->module;

	put_device(&sdev->sdev_gendev);
	module_put(mod);
}
EXPORT_SYMBOL(scsi_device_put);

 
struct scsi_device *__scsi_iterate_devices(struct Scsi_Host *shost,
					   struct scsi_device *prev)
{
	struct list_head *list = (prev ? &prev->siblings : &shost->__devices);
	struct scsi_device *next = NULL;
	unsigned long flags;

	spin_lock_irqsave(shost->host_lock, flags);
	while (list->next != &shost->__devices) {
		next = list_entry(list->next, struct scsi_device, siblings);
		 
		if (!scsi_device_get(next))
			break;
		next = NULL;
		list = list->next;
	}
	spin_unlock_irqrestore(shost->host_lock, flags);

	if (prev)
		scsi_device_put(prev);
	return next;
}
EXPORT_SYMBOL(__scsi_iterate_devices);

 
void starget_for_each_device(struct scsi_target *starget, void *data,
		     void (*fn)(struct scsi_device *, void *))
{
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct scsi_device *sdev;

	shost_for_each_device(sdev, shost) {
		if ((sdev->channel == starget->channel) &&
		    (sdev->id == starget->id))
			fn(sdev, data);
	}
}
EXPORT_SYMBOL(starget_for_each_device);

 
void __starget_for_each_device(struct scsi_target *starget, void *data,
			       void (*fn)(struct scsi_device *, void *))
{
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct scsi_device *sdev;

	__shost_for_each_device(sdev, shost) {
		if ((sdev->channel == starget->channel) &&
		    (sdev->id == starget->id))
			fn(sdev, data);
	}
}
EXPORT_SYMBOL(__starget_for_each_device);

 
struct scsi_device *__scsi_device_lookup_by_target(struct scsi_target *starget,
						   u64 lun)
{
	struct scsi_device *sdev;

	list_for_each_entry(sdev, &starget->devices, same_target_siblings) {
		if (sdev->sdev_state == SDEV_DEL)
			continue;
		if (sdev->lun ==lun)
			return sdev;
	}

	return NULL;
}
EXPORT_SYMBOL(__scsi_device_lookup_by_target);

 
struct scsi_device *scsi_device_lookup_by_target(struct scsi_target *starget,
						 u64 lun)
{
	struct scsi_device *sdev;
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	unsigned long flags;

	spin_lock_irqsave(shost->host_lock, flags);
	sdev = __scsi_device_lookup_by_target(starget, lun);
	if (sdev && scsi_device_get(sdev))
		sdev = NULL;
	spin_unlock_irqrestore(shost->host_lock, flags);

	return sdev;
}
EXPORT_SYMBOL(scsi_device_lookup_by_target);

 
struct scsi_device *__scsi_device_lookup(struct Scsi_Host *shost,
		uint channel, uint id, u64 lun)
{
	struct scsi_device *sdev;

	list_for_each_entry(sdev, &shost->__devices, siblings) {
		if (sdev->sdev_state == SDEV_DEL)
			continue;
		if (sdev->channel == channel && sdev->id == id &&
				sdev->lun ==lun)
			return sdev;
	}

	return NULL;
}
EXPORT_SYMBOL(__scsi_device_lookup);

 
struct scsi_device *scsi_device_lookup(struct Scsi_Host *shost,
		uint channel, uint id, u64 lun)
{
	struct scsi_device *sdev;
	unsigned long flags;

	spin_lock_irqsave(shost->host_lock, flags);
	sdev = __scsi_device_lookup(shost, channel, id, lun);
	if (sdev && scsi_device_get(sdev))
		sdev = NULL;
	spin_unlock_irqrestore(shost->host_lock, flags);

	return sdev;
}
EXPORT_SYMBOL(scsi_device_lookup);

MODULE_DESCRIPTION("SCSI core");
MODULE_LICENSE("GPL");

module_param(scsi_logging_level, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(scsi_logging_level, "a bit mask of logging levels");

static int __init init_scsi(void)
{
	int error;

	error = scsi_init_procfs();
	if (error)
		goto cleanup_queue;
	error = scsi_init_devinfo();
	if (error)
		goto cleanup_procfs;
	error = scsi_init_hosts();
	if (error)
		goto cleanup_devlist;
	error = scsi_init_sysctl();
	if (error)
		goto cleanup_hosts;
	error = scsi_sysfs_register();
	if (error)
		goto cleanup_sysctl;

	scsi_netlink_init();

	printk(KERN_NOTICE "SCSI subsystem initialized\n");
	return 0;

cleanup_sysctl:
	scsi_exit_sysctl();
cleanup_hosts:
	scsi_exit_hosts();
cleanup_devlist:
	scsi_exit_devinfo();
cleanup_procfs:
	scsi_exit_procfs();
cleanup_queue:
	scsi_exit_queue();
	printk(KERN_ERR "SCSI subsystem failed to initialize, error = %d\n",
	       -error);
	return error;
}

static void __exit exit_scsi(void)
{
	scsi_netlink_exit();
	scsi_sysfs_unregister();
	scsi_exit_sysctl();
	scsi_exit_hosts();
	scsi_exit_devinfo();
	scsi_exit_procfs();
	scsi_exit_queue();
}

subsys_initcall(init_scsi);
module_exit(exit_scsi);
