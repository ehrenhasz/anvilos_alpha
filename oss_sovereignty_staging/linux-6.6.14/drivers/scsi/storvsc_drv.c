
 

#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/hyperv.h>
#include <linux/blkdev.h>
#include <linux/dma-mapping.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_devinfo.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_transport_fc.h>
#include <scsi/scsi_transport.h>

 

 

#define VMSTOR_PROTO_VERSION(MAJOR_, MINOR_)	((((MAJOR_) & 0xff) << 8) | \
						(((MINOR_) & 0xff)))
#define VMSTOR_PROTO_VERSION_WIN6	VMSTOR_PROTO_VERSION(2, 0)
#define VMSTOR_PROTO_VERSION_WIN7	VMSTOR_PROTO_VERSION(4, 2)
#define VMSTOR_PROTO_VERSION_WIN8	VMSTOR_PROTO_VERSION(5, 1)
#define VMSTOR_PROTO_VERSION_WIN8_1	VMSTOR_PROTO_VERSION(6, 0)
#define VMSTOR_PROTO_VERSION_WIN10	VMSTOR_PROTO_VERSION(6, 2)

 
#define CALLBACK_TIMEOUT               2

 
enum vstor_packet_operation {
	VSTOR_OPERATION_COMPLETE_IO		= 1,
	VSTOR_OPERATION_REMOVE_DEVICE		= 2,
	VSTOR_OPERATION_EXECUTE_SRB		= 3,
	VSTOR_OPERATION_RESET_LUN		= 4,
	VSTOR_OPERATION_RESET_ADAPTER		= 5,
	VSTOR_OPERATION_RESET_BUS		= 6,
	VSTOR_OPERATION_BEGIN_INITIALIZATION	= 7,
	VSTOR_OPERATION_END_INITIALIZATION	= 8,
	VSTOR_OPERATION_QUERY_PROTOCOL_VERSION	= 9,
	VSTOR_OPERATION_QUERY_PROPERTIES	= 10,
	VSTOR_OPERATION_ENUMERATE_BUS		= 11,
	VSTOR_OPERATION_FCHBA_DATA              = 12,
	VSTOR_OPERATION_CREATE_SUB_CHANNELS     = 13,
	VSTOR_OPERATION_MAXIMUM                 = 13
};

 

struct hv_fc_wwn_packet {
	u8	primary_active;
	u8	reserved1[3];
	u8	primary_port_wwn[8];
	u8	primary_node_wwn[8];
	u8	secondary_port_wwn[8];
	u8	secondary_node_wwn[8];
};



 

#define SRB_FLAGS_QUEUE_ACTION_ENABLE		0x00000002
#define SRB_FLAGS_DISABLE_DISCONNECT		0x00000004
#define SRB_FLAGS_DISABLE_SYNCH_TRANSFER	0x00000008
#define SRB_FLAGS_BYPASS_FROZEN_QUEUE		0x00000010
#define SRB_FLAGS_DISABLE_AUTOSENSE		0x00000020
#define SRB_FLAGS_DATA_IN			0x00000040
#define SRB_FLAGS_DATA_OUT			0x00000080
#define SRB_FLAGS_NO_DATA_TRANSFER		0x00000000
#define SRB_FLAGS_UNSPECIFIED_DIRECTION	(SRB_FLAGS_DATA_IN | SRB_FLAGS_DATA_OUT)
#define SRB_FLAGS_NO_QUEUE_FREEZE		0x00000100
#define SRB_FLAGS_ADAPTER_CACHE_ENABLE		0x00000200
#define SRB_FLAGS_FREE_SENSE_BUFFER		0x00000400

 
#define SRB_FLAGS_D3_PROCESSING			0x00000800
#define SRB_FLAGS_IS_ACTIVE			0x00010000
#define SRB_FLAGS_ALLOCATED_FROM_ZONE		0x00020000
#define SRB_FLAGS_SGLIST_FROM_POOL		0x00040000
#define SRB_FLAGS_BYPASS_LOCKED_QUEUE		0x00080000
#define SRB_FLAGS_NO_KEEP_AWAKE			0x00100000
#define SRB_FLAGS_PORT_DRIVER_ALLOCSENSE	0x00200000
#define SRB_FLAGS_PORT_DRIVER_SENSEHASPORT	0x00400000
#define SRB_FLAGS_DONT_START_NEXT_PACKET	0x00800000
#define SRB_FLAGS_PORT_DRIVER_RESERVED		0x0F000000
#define SRB_FLAGS_CLASS_DRIVER_RESERVED		0xF0000000

#define SP_UNTAGGED			((unsigned char) ~0)
#define SRB_SIMPLE_TAG_REQUEST		0x20

 
#define STORVSC_MAX_CMD_LEN			0x10

 
#define STORVSC_SENSE_BUFFER_SIZE		0x14
#define STORVSC_MAX_BUF_LEN_WITH_PADDING	0x14

 
static int vmstor_proto_version;

#define STORVSC_LOGGING_NONE	0
#define STORVSC_LOGGING_ERROR	1
#define STORVSC_LOGGING_WARN	2

static int logging_level = STORVSC_LOGGING_ERROR;
module_param(logging_level, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(logging_level,
	"Logging level, 0 - None, 1 - Error (default), 2 - Warning.");

static inline bool do_logging(int level)
{
	return logging_level >= level;
}

#define storvsc_log(dev, level, fmt, ...)			\
do {								\
	if (do_logging(level))					\
		dev_warn(&(dev)->device, fmt, ##__VA_ARGS__);	\
} while (0)

struct vmscsi_request {
	u16 length;
	u8 srb_status;
	u8 scsi_status;

	u8  port_number;
	u8  path_id;
	u8  target_id;
	u8  lun;

	u8  cdb_length;
	u8  sense_info_length;
	u8  data_in;
	u8  reserved;

	u32 data_transfer_length;

	union {
		u8 cdb[STORVSC_MAX_CMD_LEN];
		u8 sense_data[STORVSC_SENSE_BUFFER_SIZE];
		u8 reserved_array[STORVSC_MAX_BUF_LEN_WITH_PADDING];
	};
	 
	u16 reserve;
	u8  queue_tag;
	u8  queue_action;
	u32 srb_flags;
	u32 time_out_value;
	u32 queue_sort_ey;

} __attribute((packed));

 

static const int protocol_version[] = {
		VMSTOR_PROTO_VERSION_WIN10,
		VMSTOR_PROTO_VERSION_WIN8_1,
		VMSTOR_PROTO_VERSION_WIN8,
};


 

#define STORAGE_CHANNEL_SUPPORTS_MULTI_CHANNEL		0x1

struct vmstorage_channel_properties {
	u32 reserved;
	u16 max_channel_cnt;
	u16 reserved1;

	u32 flags;
	u32   max_transfer_bytes;

	u64  reserved2;
} __packed;

 
struct vmstorage_protocol_version {
	 
	u16 major_minor;

	 
	u16 revision;
} __packed;

 
#define STORAGE_CHANNEL_REMOVABLE_FLAG		0x1
#define STORAGE_CHANNEL_EMULATED_IDE_FLAG	0x2

struct vstor_packet {
	 
	enum vstor_packet_operation operation;

	 
	u32 flags;

	 
	u32 status;

	 
	union {
		 
		struct vmscsi_request vm_srb;

		 
		struct vmstorage_channel_properties storage_channel_properties;

		 
		struct vmstorage_protocol_version version;

		 
		struct hv_fc_wwn_packet wwn_packet;

		 
		u16 sub_channel_count;

		 
		u8  buffer[0x34];
	};
} __packed;

 

#define REQUEST_COMPLETION_FLAG	0x1

 
enum storvsc_request_type {
	WRITE_TYPE = 0,
	READ_TYPE,
	UNKNOWN_TYPE,
};

 
#define SRB_STATUS_AUTOSENSE_VALID	0x80
#define SRB_STATUS_QUEUE_FROZEN		0x40

 
#define SRB_STATUS_SUCCESS		0x01
#define SRB_STATUS_ABORTED		0x02
#define SRB_STATUS_ERROR		0x04
#define SRB_STATUS_INVALID_REQUEST	0x06
#define SRB_STATUS_TIMEOUT		0x09
#define SRB_STATUS_SELECTION_TIMEOUT	0x0A
#define SRB_STATUS_BUS_RESET		0x0E
#define SRB_STATUS_DATA_OVERRUN		0x12
#define SRB_STATUS_INVALID_LUN		0x20
#define SRB_STATUS_INTERNAL_ERROR	0x30

#define SRB_STATUS(status) \
	(status & ~(SRB_STATUS_AUTOSENSE_VALID | SRB_STATUS_QUEUE_FROZEN))
 

static int storvsc_ringbuffer_size = (128 * 1024);
static u32 max_outstanding_req_per_channel;
static int storvsc_change_queue_depth(struct scsi_device *sdev, int queue_depth);

static int storvsc_vcpus_per_sub_channel = 4;
static unsigned int storvsc_max_hw_queues;

module_param(storvsc_ringbuffer_size, int, S_IRUGO);
MODULE_PARM_DESC(storvsc_ringbuffer_size, "Ring buffer size (bytes)");

module_param(storvsc_max_hw_queues, uint, 0644);
MODULE_PARM_DESC(storvsc_max_hw_queues, "Maximum number of hardware queues");

module_param(storvsc_vcpus_per_sub_channel, int, S_IRUGO);
MODULE_PARM_DESC(storvsc_vcpus_per_sub_channel, "Ratio of VCPUs to subchannels");

static int ring_avail_percent_lowater = 10;
module_param(ring_avail_percent_lowater, int, S_IRUGO);
MODULE_PARM_DESC(ring_avail_percent_lowater,
		"Select a channel if available ring size > this in percent");

 
static int storvsc_timeout = 180;

#if IS_ENABLED(CONFIG_SCSI_FC_ATTRS)
static struct scsi_transport_template *fc_transport_template;
#endif

static struct scsi_host_template scsi_driver;
static void storvsc_on_channel_callback(void *context);

#define STORVSC_MAX_LUNS_PER_TARGET			255
#define STORVSC_MAX_TARGETS				2
#define STORVSC_MAX_CHANNELS				8

#define STORVSC_FC_MAX_LUNS_PER_TARGET			255
#define STORVSC_FC_MAX_TARGETS				128
#define STORVSC_FC_MAX_CHANNELS				8
#define STORVSC_FC_MAX_XFER_SIZE			((u32)(512 * 1024))

#define STORVSC_IDE_MAX_LUNS_PER_TARGET			64
#define STORVSC_IDE_MAX_TARGETS				1
#define STORVSC_IDE_MAX_CHANNELS			1

 
#define STORVSC_MAX_PKT_SIZE (sizeof(struct vmpacket_descriptor) +\
			      sizeof(struct vstor_packet))

struct storvsc_cmd_request {
	struct scsi_cmnd *cmd;

	struct hv_device *device;

	 
	struct completion wait_event;

	struct vmbus_channel_packet_multipage_buffer mpb;
	struct vmbus_packet_mpb_array *payload;
	u32 payload_sz;

	struct vstor_packet vstor_packet;
};


 
struct storvsc_device {
	struct hv_device *device;

	bool	 destroy;
	bool	 drain_notify;
	atomic_t num_outstanding_req;
	struct Scsi_Host *host;

	wait_queue_head_t waiting_to_drain;

	 
	unsigned int port_number;
	unsigned char path_id;
	unsigned char target_id;

	 
	u32   max_transfer_bytes;
	 
	u16 num_sc;
	struct vmbus_channel **stor_chns;
	 
	struct cpumask alloced_cpus;
	 
	spinlock_t lock;
	 
	struct storvsc_cmd_request init_request;
	struct storvsc_cmd_request reset_request;
	 
	u64 node_name;
	u64 port_name;
#if IS_ENABLED(CONFIG_SCSI_FC_ATTRS)
	struct fc_rport *rport;
#endif
};

struct hv_host_device {
	struct hv_device *dev;
	unsigned int port;
	unsigned char path;
	unsigned char target;
	struct workqueue_struct *handle_error_wq;
	struct work_struct host_scan_work;
	struct Scsi_Host *host;
};

struct storvsc_scan_work {
	struct work_struct work;
	struct Scsi_Host *host;
	u8 lun;
	u8 tgt_id;
};

static void storvsc_device_scan(struct work_struct *work)
{
	struct storvsc_scan_work *wrk;
	struct scsi_device *sdev;

	wrk = container_of(work, struct storvsc_scan_work, work);

	sdev = scsi_device_lookup(wrk->host, 0, wrk->tgt_id, wrk->lun);
	if (!sdev)
		goto done;
	scsi_rescan_device(sdev);
	scsi_device_put(sdev);

done:
	kfree(wrk);
}

static void storvsc_host_scan(struct work_struct *work)
{
	struct Scsi_Host *host;
	struct scsi_device *sdev;
	struct hv_host_device *host_device =
		container_of(work, struct hv_host_device, host_scan_work);

	host = host_device->host;
	 
	mutex_lock(&host->scan_mutex);
	shost_for_each_device(sdev, host)
		scsi_test_unit_ready(sdev, 1, 1, NULL);
	mutex_unlock(&host->scan_mutex);
	 
	scsi_scan_host(host);
}

static void storvsc_remove_lun(struct work_struct *work)
{
	struct storvsc_scan_work *wrk;
	struct scsi_device *sdev;

	wrk = container_of(work, struct storvsc_scan_work, work);
	if (!scsi_host_get(wrk->host))
		goto done;

	sdev = scsi_device_lookup(wrk->host, 0, wrk->tgt_id, wrk->lun);

	if (sdev) {
		scsi_remove_device(sdev);
		scsi_device_put(sdev);
	}
	scsi_host_put(wrk->host);

done:
	kfree(wrk);
}


 

static inline struct storvsc_device *get_out_stor_device(
					struct hv_device *device)
{
	struct storvsc_device *stor_device;

	stor_device = hv_get_drvdata(device);

	if (stor_device && stor_device->destroy)
		stor_device = NULL;

	return stor_device;
}


static inline void storvsc_wait_to_drain(struct storvsc_device *dev)
{
	dev->drain_notify = true;
	wait_event(dev->waiting_to_drain,
		   atomic_read(&dev->num_outstanding_req) == 0);
	dev->drain_notify = false;
}

static inline struct storvsc_device *get_in_stor_device(
					struct hv_device *device)
{
	struct storvsc_device *stor_device;

	stor_device = hv_get_drvdata(device);

	if (!stor_device)
		goto get_in_err;

	 

	if (stor_device->destroy  &&
		(atomic_read(&stor_device->num_outstanding_req) == 0))
		stor_device = NULL;

get_in_err:
	return stor_device;

}

static void storvsc_change_target_cpu(struct vmbus_channel *channel, u32 old,
				      u32 new)
{
	struct storvsc_device *stor_device;
	struct vmbus_channel *cur_chn;
	bool old_is_alloced = false;
	struct hv_device *device;
	unsigned long flags;
	int cpu;

	device = channel->primary_channel ?
			channel->primary_channel->device_obj
				: channel->device_obj;
	stor_device = get_out_stor_device(device);
	if (!stor_device)
		return;

	 
	spin_lock_irqsave(&stor_device->lock, flags);

	 
	if (device->channel != channel && device->channel->target_cpu == old) {
		cur_chn = device->channel;
		old_is_alloced = true;
		goto old_is_alloced;
	}
	list_for_each_entry(cur_chn, &device->channel->sc_list, sc_list) {
		if (cur_chn == channel)
			continue;
		if (cur_chn->target_cpu == old) {
			old_is_alloced = true;
			goto old_is_alloced;
		}
	}

old_is_alloced:
	if (old_is_alloced)
		WRITE_ONCE(stor_device->stor_chns[old], cur_chn);
	else
		cpumask_clear_cpu(old, &stor_device->alloced_cpus);

	 
	for_each_possible_cpu(cpu) {
		if (stor_device->stor_chns[cpu] && !cpumask_test_cpu(
					cpu, &stor_device->alloced_cpus))
			WRITE_ONCE(stor_device->stor_chns[cpu], NULL);
	}

	WRITE_ONCE(stor_device->stor_chns[new], channel);
	cpumask_set_cpu(new, &stor_device->alloced_cpus);

	spin_unlock_irqrestore(&stor_device->lock, flags);
}

static u64 storvsc_next_request_id(struct vmbus_channel *channel, u64 rqst_addr)
{
	struct storvsc_cmd_request *request =
		(struct storvsc_cmd_request *)(unsigned long)rqst_addr;

	if (rqst_addr == VMBUS_RQST_INIT)
		return VMBUS_RQST_INIT;
	if (rqst_addr == VMBUS_RQST_RESET)
		return VMBUS_RQST_RESET;

	 
	return (u64)blk_mq_unique_tag(scsi_cmd_to_rq(request->cmd)) + 1;
}

static void handle_sc_creation(struct vmbus_channel *new_sc)
{
	struct hv_device *device = new_sc->primary_channel->device_obj;
	struct device *dev = &device->device;
	struct storvsc_device *stor_device;
	struct vmstorage_channel_properties props;
	int ret;

	stor_device = get_out_stor_device(device);
	if (!stor_device)
		return;

	memset(&props, 0, sizeof(struct vmstorage_channel_properties));
	new_sc->max_pkt_size = STORVSC_MAX_PKT_SIZE;

	new_sc->next_request_id_callback = storvsc_next_request_id;

	ret = vmbus_open(new_sc,
			 storvsc_ringbuffer_size,
			 storvsc_ringbuffer_size,
			 (void *)&props,
			 sizeof(struct vmstorage_channel_properties),
			 storvsc_on_channel_callback, new_sc);

	 
	if (ret != 0) {
		dev_err(dev, "Failed to open sub-channel: err=%d\n", ret);
		return;
	}

	new_sc->change_target_cpu_callback = storvsc_change_target_cpu;

	 
	stor_device->stor_chns[new_sc->target_cpu] = new_sc;
	cpumask_set_cpu(new_sc->target_cpu, &stor_device->alloced_cpus);
}

static void  handle_multichannel_storage(struct hv_device *device, int max_chns)
{
	struct device *dev = &device->device;
	struct storvsc_device *stor_device;
	int num_sc;
	struct storvsc_cmd_request *request;
	struct vstor_packet *vstor_packet;
	int ret, t;

	 
	num_sc = min((int)(num_online_cpus() - 1), max_chns);
	if (!num_sc)
		return;

	stor_device = get_out_stor_device(device);
	if (!stor_device)
		return;

	stor_device->num_sc = num_sc;
	request = &stor_device->init_request;
	vstor_packet = &request->vstor_packet;

	 
	vmbus_set_sc_create_callback(device->channel, handle_sc_creation);

	 
	memset(request, 0, sizeof(struct storvsc_cmd_request));
	init_completion(&request->wait_event);
	vstor_packet->operation = VSTOR_OPERATION_CREATE_SUB_CHANNELS;
	vstor_packet->flags = REQUEST_COMPLETION_FLAG;
	vstor_packet->sub_channel_count = num_sc;

	ret = vmbus_sendpacket(device->channel, vstor_packet,
			       sizeof(struct vstor_packet),
			       VMBUS_RQST_INIT,
			       VM_PKT_DATA_INBAND,
			       VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);

	if (ret != 0) {
		dev_err(dev, "Failed to create sub-channel: err=%d\n", ret);
		return;
	}

	t = wait_for_completion_timeout(&request->wait_event, 10*HZ);
	if (t == 0) {
		dev_err(dev, "Failed to create sub-channel: timed out\n");
		return;
	}

	if (vstor_packet->operation != VSTOR_OPERATION_COMPLETE_IO ||
	    vstor_packet->status != 0) {
		dev_err(dev, "Failed to create sub-channel: op=%d, sts=%d\n",
			vstor_packet->operation, vstor_packet->status);
		return;
	}

	 
}

static void cache_wwn(struct storvsc_device *stor_device,
		      struct vstor_packet *vstor_packet)
{
	 
	if (vstor_packet->wwn_packet.primary_active) {
		stor_device->node_name =
			wwn_to_u64(vstor_packet->wwn_packet.primary_node_wwn);
		stor_device->port_name =
			wwn_to_u64(vstor_packet->wwn_packet.primary_port_wwn);
	} else {
		stor_device->node_name =
			wwn_to_u64(vstor_packet->wwn_packet.secondary_node_wwn);
		stor_device->port_name =
			wwn_to_u64(vstor_packet->wwn_packet.secondary_port_wwn);
	}
}


static int storvsc_execute_vstor_op(struct hv_device *device,
				    struct storvsc_cmd_request *request,
				    bool status_check)
{
	struct storvsc_device *stor_device;
	struct vstor_packet *vstor_packet;
	int ret, t;

	stor_device = get_out_stor_device(device);
	if (!stor_device)
		return -ENODEV;

	vstor_packet = &request->vstor_packet;

	init_completion(&request->wait_event);
	vstor_packet->flags = REQUEST_COMPLETION_FLAG;

	ret = vmbus_sendpacket(device->channel, vstor_packet,
			       sizeof(struct vstor_packet),
			       VMBUS_RQST_INIT,
			       VM_PKT_DATA_INBAND,
			       VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);
	if (ret != 0)
		return ret;

	t = wait_for_completion_timeout(&request->wait_event, 5*HZ);
	if (t == 0)
		return -ETIMEDOUT;

	if (!status_check)
		return ret;

	if (vstor_packet->operation != VSTOR_OPERATION_COMPLETE_IO ||
	    vstor_packet->status != 0)
		return -EINVAL;

	return ret;
}

static int storvsc_channel_init(struct hv_device *device, bool is_fc)
{
	struct storvsc_device *stor_device;
	struct storvsc_cmd_request *request;
	struct vstor_packet *vstor_packet;
	int ret, i;
	int max_chns;
	bool process_sub_channels = false;

	stor_device = get_out_stor_device(device);
	if (!stor_device)
		return -ENODEV;

	request = &stor_device->init_request;
	vstor_packet = &request->vstor_packet;

	 
	memset(request, 0, sizeof(struct storvsc_cmd_request));
	vstor_packet->operation = VSTOR_OPERATION_BEGIN_INITIALIZATION;
	ret = storvsc_execute_vstor_op(device, request, true);
	if (ret)
		return ret;
	 

	for (i = 0; i < ARRAY_SIZE(protocol_version); i++) {
		 
		memset(vstor_packet, 0, sizeof(struct vstor_packet));
		vstor_packet->operation =
			VSTOR_OPERATION_QUERY_PROTOCOL_VERSION;

		vstor_packet->version.major_minor = protocol_version[i];

		 
		vstor_packet->version.revision = 0;
		ret = storvsc_execute_vstor_op(device, request, false);
		if (ret != 0)
			return ret;

		if (vstor_packet->operation != VSTOR_OPERATION_COMPLETE_IO)
			return -EINVAL;

		if (vstor_packet->status == 0) {
			vmstor_proto_version = protocol_version[i];

			break;
		}
	}

	if (vstor_packet->status != 0) {
		dev_err(&device->device, "Obsolete Hyper-V version\n");
		return -EINVAL;
	}


	memset(vstor_packet, 0, sizeof(struct vstor_packet));
	vstor_packet->operation = VSTOR_OPERATION_QUERY_PROPERTIES;
	ret = storvsc_execute_vstor_op(device, request, true);
	if (ret != 0)
		return ret;

	 
	max_chns = vstor_packet->storage_channel_properties.max_channel_cnt;

	 
	stor_device->stor_chns = kcalloc(num_possible_cpus(), sizeof(void *),
					 GFP_KERNEL);
	if (stor_device->stor_chns == NULL)
		return -ENOMEM;

	device->channel->change_target_cpu_callback = storvsc_change_target_cpu;

	stor_device->stor_chns[device->channel->target_cpu] = device->channel;
	cpumask_set_cpu(device->channel->target_cpu,
			&stor_device->alloced_cpus);

	if (vstor_packet->storage_channel_properties.flags &
	    STORAGE_CHANNEL_SUPPORTS_MULTI_CHANNEL)
		process_sub_channels = true;

	stor_device->max_transfer_bytes =
		vstor_packet->storage_channel_properties.max_transfer_bytes;

	if (!is_fc)
		goto done;

	 
	memset(vstor_packet, 0, sizeof(struct vstor_packet));
	vstor_packet->operation = VSTOR_OPERATION_FCHBA_DATA;
	ret = storvsc_execute_vstor_op(device, request, true);
	if (ret != 0)
		return ret;

	 
	cache_wwn(stor_device, vstor_packet);

done:

	memset(vstor_packet, 0, sizeof(struct vstor_packet));
	vstor_packet->operation = VSTOR_OPERATION_END_INITIALIZATION;
	ret = storvsc_execute_vstor_op(device, request, true);
	if (ret != 0)
		return ret;

	if (process_sub_channels)
		handle_multichannel_storage(device, max_chns);

	return ret;
}

static void storvsc_handle_error(struct vmscsi_request *vm_srb,
				struct scsi_cmnd *scmnd,
				struct Scsi_Host *host,
				u8 asc, u8 ascq)
{
	struct storvsc_scan_work *wrk;
	void (*process_err_fn)(struct work_struct *work);
	struct hv_host_device *host_dev = shost_priv(host);

	switch (SRB_STATUS(vm_srb->srb_status)) {
	case SRB_STATUS_ERROR:
	case SRB_STATUS_ABORTED:
	case SRB_STATUS_INVALID_REQUEST:
	case SRB_STATUS_INTERNAL_ERROR:
	case SRB_STATUS_TIMEOUT:
	case SRB_STATUS_SELECTION_TIMEOUT:
	case SRB_STATUS_BUS_RESET:
	case SRB_STATUS_DATA_OVERRUN:
		if (vm_srb->srb_status & SRB_STATUS_AUTOSENSE_VALID) {
			 
			if ((asc == 0x2a) && (ascq == 0x9)) {
				process_err_fn = storvsc_device_scan;
				 
				set_host_byte(scmnd, DID_REQUEUE);
				goto do_work;
			}

			 
			if ((asc == 0x3f) && (ascq != 0x03) &&
					(ascq != 0x0e)) {
				process_err_fn = storvsc_device_scan;
				set_host_byte(scmnd, DID_REQUEUE);
				goto do_work;
			}

			 
			return;
		}

		 
		switch (scmnd->cmnd[0]) {
		case ATA_16:
		case ATA_12:
			set_host_byte(scmnd, DID_PASSTHROUGH);
			break;
		 
		case TEST_UNIT_READY:
			break;
		default:
			set_host_byte(scmnd, DID_ERROR);
		}
		return;

	case SRB_STATUS_INVALID_LUN:
		set_host_byte(scmnd, DID_NO_CONNECT);
		process_err_fn = storvsc_remove_lun;
		goto do_work;

	}
	return;

do_work:
	 
	wrk = kmalloc(sizeof(struct storvsc_scan_work), GFP_ATOMIC);
	if (!wrk) {
		set_host_byte(scmnd, DID_BAD_TARGET);
		return;
	}

	wrk->host = host;
	wrk->lun = vm_srb->lun;
	wrk->tgt_id = vm_srb->target_id;
	INIT_WORK(&wrk->work, process_err_fn);
	queue_work(host_dev->handle_error_wq, &wrk->work);
}


static void storvsc_command_completion(struct storvsc_cmd_request *cmd_request,
				       struct storvsc_device *stor_dev)
{
	struct scsi_cmnd *scmnd = cmd_request->cmd;
	struct scsi_sense_hdr sense_hdr;
	struct vmscsi_request *vm_srb;
	u32 data_transfer_length;
	struct Scsi_Host *host;
	u32 payload_sz = cmd_request->payload_sz;
	void *payload = cmd_request->payload;
	bool sense_ok;

	host = stor_dev->host;

	vm_srb = &cmd_request->vstor_packet.vm_srb;
	data_transfer_length = vm_srb->data_transfer_length;

	scmnd->result = vm_srb->scsi_status;

	if (scmnd->result) {
		sense_ok = scsi_normalize_sense(scmnd->sense_buffer,
				SCSI_SENSE_BUFFERSIZE, &sense_hdr);

		if (sense_ok && do_logging(STORVSC_LOGGING_WARN))
			scsi_print_sense_hdr(scmnd->device, "storvsc",
					     &sense_hdr);
	}

	if (vm_srb->srb_status != SRB_STATUS_SUCCESS) {
		storvsc_handle_error(vm_srb, scmnd, host, sense_hdr.asc,
					 sense_hdr.ascq);
		 
		if (vm_srb->srb_status != SRB_STATUS_DATA_OVERRUN)
			data_transfer_length = 0;
	}

	 
	if (data_transfer_length > cmd_request->payload->range.len)
		data_transfer_length = cmd_request->payload->range.len;

	scsi_set_resid(scmnd,
		cmd_request->payload->range.len - data_transfer_length);

	scsi_done(scmnd);

	if (payload_sz >
		sizeof(struct vmbus_channel_packet_multipage_buffer))
		kfree(payload);
}

static void storvsc_on_io_completion(struct storvsc_device *stor_device,
				  struct vstor_packet *vstor_packet,
				  struct storvsc_cmd_request *request)
{
	struct vstor_packet *stor_pkt;
	struct hv_device *device = stor_device->device;

	stor_pkt = &request->vstor_packet;

	 

	if ((stor_pkt->vm_srb.cdb[0] == INQUIRY) ||
	   (stor_pkt->vm_srb.cdb[0] == MODE_SENSE)) {
		vstor_packet->vm_srb.scsi_status = 0;
		vstor_packet->vm_srb.srb_status = SRB_STATUS_SUCCESS;
	}

	 
	stor_pkt->vm_srb.scsi_status = vstor_packet->vm_srb.scsi_status;
	stor_pkt->vm_srb.srb_status = vstor_packet->vm_srb.srb_status;

	 
	stor_pkt->vm_srb.sense_info_length = min_t(u8, STORVSC_SENSE_BUFFER_SIZE,
		vstor_packet->vm_srb.sense_info_length);

	if (vstor_packet->vm_srb.scsi_status != 0 ||
	    vstor_packet->vm_srb.srb_status != SRB_STATUS_SUCCESS) {

		 
		int loglevel = (stor_pkt->vm_srb.cdb[0] == TEST_UNIT_READY) ?
			STORVSC_LOGGING_WARN : STORVSC_LOGGING_ERROR;

		storvsc_log(device, loglevel,
			"tag#%d cmd 0x%x status: scsi 0x%x srb 0x%x hv 0x%x\n",
			scsi_cmd_to_rq(request->cmd)->tag,
			stor_pkt->vm_srb.cdb[0],
			vstor_packet->vm_srb.scsi_status,
			vstor_packet->vm_srb.srb_status,
			vstor_packet->status);
	}

	if (vstor_packet->vm_srb.scsi_status == SAM_STAT_CHECK_CONDITION &&
	    (vstor_packet->vm_srb.srb_status & SRB_STATUS_AUTOSENSE_VALID))
		memcpy(request->cmd->sense_buffer,
		       vstor_packet->vm_srb.sense_data,
		       stor_pkt->vm_srb.sense_info_length);

	stor_pkt->vm_srb.data_transfer_length =
		vstor_packet->vm_srb.data_transfer_length;

	storvsc_command_completion(request, stor_device);

	if (atomic_dec_and_test(&stor_device->num_outstanding_req) &&
		stor_device->drain_notify)
		wake_up(&stor_device->waiting_to_drain);
}

static void storvsc_on_receive(struct storvsc_device *stor_device,
			     struct vstor_packet *vstor_packet,
			     struct storvsc_cmd_request *request)
{
	struct hv_host_device *host_dev;
	switch (vstor_packet->operation) {
	case VSTOR_OPERATION_COMPLETE_IO:
		storvsc_on_io_completion(stor_device, vstor_packet, request);
		break;

	case VSTOR_OPERATION_REMOVE_DEVICE:
	case VSTOR_OPERATION_ENUMERATE_BUS:
		host_dev = shost_priv(stor_device->host);
		queue_work(
			host_dev->handle_error_wq, &host_dev->host_scan_work);
		break;

	case VSTOR_OPERATION_FCHBA_DATA:
		cache_wwn(stor_device, vstor_packet);
#if IS_ENABLED(CONFIG_SCSI_FC_ATTRS)
		fc_host_node_name(stor_device->host) = stor_device->node_name;
		fc_host_port_name(stor_device->host) = stor_device->port_name;
#endif
		break;
	default:
		break;
	}
}

static void storvsc_on_channel_callback(void *context)
{
	struct vmbus_channel *channel = (struct vmbus_channel *)context;
	const struct vmpacket_descriptor *desc;
	struct hv_device *device;
	struct storvsc_device *stor_device;
	struct Scsi_Host *shost;
	unsigned long time_limit = jiffies + msecs_to_jiffies(CALLBACK_TIMEOUT);

	if (channel->primary_channel != NULL)
		device = channel->primary_channel->device_obj;
	else
		device = channel->device_obj;

	stor_device = get_in_stor_device(device);
	if (!stor_device)
		return;

	shost = stor_device->host;

	foreach_vmbus_pkt(desc, channel) {
		struct vstor_packet *packet = hv_pkt_data(desc);
		struct storvsc_cmd_request *request = NULL;
		u32 pktlen = hv_pkt_datalen(desc);
		u64 rqst_id = desc->trans_id;
		u32 minlen = rqst_id ? sizeof(struct vstor_packet) :
			sizeof(enum vstor_packet_operation);

		if (unlikely(time_after(jiffies, time_limit))) {
			hv_pkt_iter_close(channel);
			return;
		}

		if (pktlen < minlen) {
			dev_err(&device->device,
				"Invalid pkt: id=%llu, len=%u, minlen=%u\n",
				rqst_id, pktlen, minlen);
			continue;
		}

		if (rqst_id == VMBUS_RQST_INIT) {
			request = &stor_device->init_request;
		} else if (rqst_id == VMBUS_RQST_RESET) {
			request = &stor_device->reset_request;
		} else {
			 
			if (rqst_id == 0) {
				 
				if (packet->operation == VSTOR_OPERATION_COMPLETE_IO ||
				    packet->operation == VSTOR_OPERATION_FCHBA_DATA) {
					dev_err(&device->device, "Invalid packet with ID of 0\n");
					continue;
				}
			} else {
				struct scsi_cmnd *scmnd;

				 
				scmnd = scsi_host_find_tag(shost, rqst_id - 1);
				if (scmnd == NULL) {
					dev_err(&device->device, "Incorrect transaction ID\n");
					continue;
				}
				request = (struct storvsc_cmd_request *)scsi_cmd_priv(scmnd);
				scsi_dma_unmap(scmnd);
			}

			storvsc_on_receive(stor_device, packet, request);
			continue;
		}

		memcpy(&request->vstor_packet, packet,
		       sizeof(struct vstor_packet));
		complete(&request->wait_event);
	}
}

static int storvsc_connect_to_vsp(struct hv_device *device, u32 ring_size,
				  bool is_fc)
{
	struct vmstorage_channel_properties props;
	int ret;

	memset(&props, 0, sizeof(struct vmstorage_channel_properties));

	device->channel->max_pkt_size = STORVSC_MAX_PKT_SIZE;
	device->channel->next_request_id_callback = storvsc_next_request_id;

	ret = vmbus_open(device->channel,
			 ring_size,
			 ring_size,
			 (void *)&props,
			 sizeof(struct vmstorage_channel_properties),
			 storvsc_on_channel_callback, device->channel);

	if (ret != 0)
		return ret;

	ret = storvsc_channel_init(device, is_fc);

	return ret;
}

static int storvsc_dev_remove(struct hv_device *device)
{
	struct storvsc_device *stor_device;

	stor_device = hv_get_drvdata(device);

	stor_device->destroy = true;

	 
	wmb();

	 

	storvsc_wait_to_drain(stor_device);

	 
	hv_set_drvdata(device, NULL);

	 
	vmbus_close(device->channel);

	kfree(stor_device->stor_chns);
	kfree(stor_device);
	return 0;
}

static struct vmbus_channel *get_og_chn(struct storvsc_device *stor_device,
					u16 q_num)
{
	u16 slot = 0;
	u16 hash_qnum;
	const struct cpumask *node_mask;
	int num_channels, tgt_cpu;

	if (stor_device->num_sc == 0) {
		stor_device->stor_chns[q_num] = stor_device->device->channel;
		return stor_device->device->channel;
	}

	 

	node_mask = cpumask_of_node(cpu_to_node(q_num));

	num_channels = 0;
	for_each_cpu(tgt_cpu, &stor_device->alloced_cpus) {
		if (cpumask_test_cpu(tgt_cpu, node_mask))
			num_channels++;
	}
	if (num_channels == 0) {
		stor_device->stor_chns[q_num] = stor_device->device->channel;
		return stor_device->device->channel;
	}

	hash_qnum = q_num;
	while (hash_qnum >= num_channels)
		hash_qnum -= num_channels;

	for_each_cpu(tgt_cpu, &stor_device->alloced_cpus) {
		if (!cpumask_test_cpu(tgt_cpu, node_mask))
			continue;
		if (slot == hash_qnum)
			break;
		slot++;
	}

	stor_device->stor_chns[q_num] = stor_device->stor_chns[tgt_cpu];

	return stor_device->stor_chns[q_num];
}


static int storvsc_do_io(struct hv_device *device,
			 struct storvsc_cmd_request *request, u16 q_num)
{
	struct storvsc_device *stor_device;
	struct vstor_packet *vstor_packet;
	struct vmbus_channel *outgoing_channel, *channel;
	unsigned long flags;
	int ret = 0;
	const struct cpumask *node_mask;
	int tgt_cpu;

	vstor_packet = &request->vstor_packet;
	stor_device = get_out_stor_device(device);

	if (!stor_device)
		return -ENODEV;


	request->device  = device;
	 
	 
	outgoing_channel = READ_ONCE(stor_device->stor_chns[q_num]);
	if (outgoing_channel != NULL) {
		if (outgoing_channel->target_cpu == q_num) {
			 
			node_mask = cpumask_of_node(cpu_to_node(q_num));
			for_each_cpu_wrap(tgt_cpu,
				 &stor_device->alloced_cpus, q_num + 1) {
				if (!cpumask_test_cpu(tgt_cpu, node_mask))
					continue;
				if (tgt_cpu == q_num)
					continue;
				channel = READ_ONCE(
					stor_device->stor_chns[tgt_cpu]);
				if (channel == NULL)
					continue;
				if (hv_get_avail_to_write_percent(
							&channel->outbound)
						> ring_avail_percent_lowater) {
					outgoing_channel = channel;
					goto found_channel;
				}
			}

			 
			if (hv_get_avail_to_write_percent(
						&outgoing_channel->outbound)
					> ring_avail_percent_lowater)
				goto found_channel;

			 
			for_each_cpu(tgt_cpu, &stor_device->alloced_cpus) {
				if (cpumask_test_cpu(tgt_cpu, node_mask))
					continue;
				channel = READ_ONCE(
					stor_device->stor_chns[tgt_cpu]);
				if (channel == NULL)
					continue;
				if (hv_get_avail_to_write_percent(
							&channel->outbound)
						> ring_avail_percent_lowater) {
					outgoing_channel = channel;
					goto found_channel;
				}
			}
		}
	} else {
		spin_lock_irqsave(&stor_device->lock, flags);
		outgoing_channel = stor_device->stor_chns[q_num];
		if (outgoing_channel != NULL) {
			spin_unlock_irqrestore(&stor_device->lock, flags);
			goto found_channel;
		}
		outgoing_channel = get_og_chn(stor_device, q_num);
		spin_unlock_irqrestore(&stor_device->lock, flags);
	}

found_channel:
	vstor_packet->flags |= REQUEST_COMPLETION_FLAG;

	vstor_packet->vm_srb.length = sizeof(struct vmscsi_request);


	vstor_packet->vm_srb.sense_info_length = STORVSC_SENSE_BUFFER_SIZE;


	vstor_packet->vm_srb.data_transfer_length =
	request->payload->range.len;

	vstor_packet->operation = VSTOR_OPERATION_EXECUTE_SRB;

	if (request->payload->range.len) {

		ret = vmbus_sendpacket_mpb_desc(outgoing_channel,
				request->payload, request->payload_sz,
				vstor_packet,
				sizeof(struct vstor_packet),
				(unsigned long)request);
	} else {
		ret = vmbus_sendpacket(outgoing_channel, vstor_packet,
			       sizeof(struct vstor_packet),
			       (unsigned long)request,
			       VM_PKT_DATA_INBAND,
			       VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);
	}

	if (ret != 0)
		return ret;

	atomic_inc(&stor_device->num_outstanding_req);

	return ret;
}

static int storvsc_device_alloc(struct scsi_device *sdevice)
{
	 
	sdevice->sdev_bflags = BLIST_REPORTLUN2 | BLIST_TRY_VPD_PAGES;

	return 0;
}

static int storvsc_device_configure(struct scsi_device *sdevice)
{
	blk_queue_rq_timeout(sdevice->request_queue, (storvsc_timeout * HZ));

	 
	sdevice->no_report_opcodes = 1;
	sdevice->no_write_same = 1;

	 
	if (!strncmp(sdevice->vendor, "Msft", 4)) {
		switch (vmstor_proto_version) {
		case VMSTOR_PROTO_VERSION_WIN8:
		case VMSTOR_PROTO_VERSION_WIN8_1:
			sdevice->scsi_level = SCSI_SPC_3;
			break;
		}

		if (vmstor_proto_version >= VMSTOR_PROTO_VERSION_WIN10)
			sdevice->no_write_same = 0;
	}

	return 0;
}

static int storvsc_get_chs(struct scsi_device *sdev, struct block_device * bdev,
			   sector_t capacity, int *info)
{
	sector_t nsect = capacity;
	sector_t cylinders = nsect;
	int heads, sectors_pt;

	 
	heads = 0xff;
	sectors_pt = 0x3f;       
	sector_div(cylinders, heads * sectors_pt);
	if ((sector_t)(cylinders + 1) * heads * sectors_pt < nsect)
		cylinders = 0xffff;

	info[0] = heads;
	info[1] = sectors_pt;
	info[2] = (int)cylinders;

	return 0;
}

static int storvsc_host_reset_handler(struct scsi_cmnd *scmnd)
{
	struct hv_host_device *host_dev = shost_priv(scmnd->device->host);
	struct hv_device *device = host_dev->dev;

	struct storvsc_device *stor_device;
	struct storvsc_cmd_request *request;
	struct vstor_packet *vstor_packet;
	int ret, t;

	stor_device = get_out_stor_device(device);
	if (!stor_device)
		return FAILED;

	request = &stor_device->reset_request;
	vstor_packet = &request->vstor_packet;
	memset(vstor_packet, 0, sizeof(struct vstor_packet));

	init_completion(&request->wait_event);

	vstor_packet->operation = VSTOR_OPERATION_RESET_BUS;
	vstor_packet->flags = REQUEST_COMPLETION_FLAG;
	vstor_packet->vm_srb.path_id = stor_device->path_id;

	ret = vmbus_sendpacket(device->channel, vstor_packet,
			       sizeof(struct vstor_packet),
			       VMBUS_RQST_RESET,
			       VM_PKT_DATA_INBAND,
			       VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);
	if (ret != 0)
		return FAILED;

	t = wait_for_completion_timeout(&request->wait_event, 5*HZ);
	if (t == 0)
		return TIMEOUT_ERROR;


	 
	storvsc_wait_to_drain(stor_device);

	return SUCCESS;
}

 
static enum scsi_timeout_action storvsc_eh_timed_out(struct scsi_cmnd *scmnd)
{
	return SCSI_EH_RESET_TIMER;
}

static bool storvsc_scsi_cmd_ok(struct scsi_cmnd *scmnd)
{
	bool allowed = true;
	u8 scsi_op = scmnd->cmnd[0];

	switch (scsi_op) {
	 
	case WRITE_SAME:
	 
	case SET_WINDOW:
		set_host_byte(scmnd, DID_ERROR);
		allowed = false;
		break;
	default:
		break;
	}
	return allowed;
}

static int storvsc_queuecommand(struct Scsi_Host *host, struct scsi_cmnd *scmnd)
{
	int ret;
	struct hv_host_device *host_dev = shost_priv(host);
	struct hv_device *dev = host_dev->dev;
	struct storvsc_cmd_request *cmd_request = scsi_cmd_priv(scmnd);
	struct scatterlist *sgl;
	struct vmscsi_request *vm_srb;
	struct vmbus_packet_mpb_array  *payload;
	u32 payload_sz;
	u32 length;

	if (vmstor_proto_version <= VMSTOR_PROTO_VERSION_WIN8) {
		 
		if (!storvsc_scsi_cmd_ok(scmnd)) {
			scsi_done(scmnd);
			return 0;
		}
	}

	 
	cmd_request->cmd = scmnd;

	memset(&cmd_request->vstor_packet, 0, sizeof(struct vstor_packet));
	vm_srb = &cmd_request->vstor_packet.vm_srb;
	vm_srb->time_out_value = 60;

	vm_srb->srb_flags |=
		SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

	if (scmnd->device->tagged_supported) {
		vm_srb->srb_flags |=
		(SRB_FLAGS_QUEUE_ACTION_ENABLE | SRB_FLAGS_NO_QUEUE_FREEZE);
		vm_srb->queue_tag = SP_UNTAGGED;
		vm_srb->queue_action = SRB_SIMPLE_TAG_REQUEST;
	}

	 
	switch (scmnd->sc_data_direction) {
	case DMA_TO_DEVICE:
		vm_srb->data_in = WRITE_TYPE;
		vm_srb->srb_flags |= SRB_FLAGS_DATA_OUT;
		break;
	case DMA_FROM_DEVICE:
		vm_srb->data_in = READ_TYPE;
		vm_srb->srb_flags |= SRB_FLAGS_DATA_IN;
		break;
	case DMA_NONE:
		vm_srb->data_in = UNKNOWN_TYPE;
		vm_srb->srb_flags |= SRB_FLAGS_NO_DATA_TRANSFER;
		break;
	default:
		 
		WARN(1, "Unexpected data direction: %d\n",
		     scmnd->sc_data_direction);
		return -EINVAL;
	}


	vm_srb->port_number = host_dev->port;
	vm_srb->path_id = scmnd->device->channel;
	vm_srb->target_id = scmnd->device->id;
	vm_srb->lun = scmnd->device->lun;

	vm_srb->cdb_length = scmnd->cmd_len;

	memcpy(vm_srb->cdb, scmnd->cmnd, vm_srb->cdb_length);

	sgl = (struct scatterlist *)scsi_sglist(scmnd);

	length = scsi_bufflen(scmnd);
	payload = (struct vmbus_packet_mpb_array *)&cmd_request->mpb;
	payload_sz = 0;

	if (scsi_sg_count(scmnd)) {
		unsigned long offset_in_hvpg = offset_in_hvpage(sgl->offset);
		unsigned int hvpg_count = HVPFN_UP(offset_in_hvpg + length);
		struct scatterlist *sg;
		unsigned long hvpfn, hvpfns_to_add;
		int j, i = 0, sg_count;

		payload_sz = (hvpg_count * sizeof(u64) +
			      sizeof(struct vmbus_packet_mpb_array));

		if (hvpg_count > MAX_PAGE_BUFFER_COUNT) {
			payload = kzalloc(payload_sz, GFP_ATOMIC);
			if (!payload)
				return SCSI_MLQUEUE_DEVICE_BUSY;
		}

		payload->range.len = length;
		payload->range.offset = offset_in_hvpg;

		sg_count = scsi_dma_map(scmnd);
		if (sg_count < 0) {
			ret = SCSI_MLQUEUE_DEVICE_BUSY;
			goto err_free_payload;
		}

		for_each_sg(sgl, sg, sg_count, j) {
			 
			hvpfn = HVPFN_DOWN(sg_dma_address(sg));
			hvpfns_to_add = HVPFN_UP(sg_dma_address(sg) +
						 sg_dma_len(sg)) - hvpfn;

			 
			while (hvpfns_to_add--)
				payload->range.pfn_array[i++] = hvpfn++;
		}
	}

	cmd_request->payload = payload;
	cmd_request->payload_sz = payload_sz;

	 
	ret = storvsc_do_io(dev, cmd_request, get_cpu());
	put_cpu();

	if (ret)
		scsi_dma_unmap(scmnd);

	if (ret == -EAGAIN) {
		 
		ret = SCSI_MLQUEUE_DEVICE_BUSY;
		goto err_free_payload;
	}

	return 0;

err_free_payload:
	if (payload_sz > sizeof(cmd_request->mpb))
		kfree(payload);

	return ret;
}

static struct scsi_host_template scsi_driver = {
	.module	=		THIS_MODULE,
	.name =			"storvsc_host_t",
	.cmd_size =             sizeof(struct storvsc_cmd_request),
	.bios_param =		storvsc_get_chs,
	.queuecommand =		storvsc_queuecommand,
	.eh_host_reset_handler =	storvsc_host_reset_handler,
	.proc_name =		"storvsc_host",
	.eh_timed_out =		storvsc_eh_timed_out,
	.slave_alloc =		storvsc_device_alloc,
	.slave_configure =	storvsc_device_configure,
	.cmd_per_lun =		2048,
	.this_id =		-1,
	 
	.virt_boundary_mask =	HV_HYP_PAGE_SIZE - 1,
	.no_write_same =	1,
	.track_queue_depth =	1,
	.change_queue_depth =	storvsc_change_queue_depth,
};

enum {
	SCSI_GUID,
	IDE_GUID,
	SFC_GUID,
};

static const struct hv_vmbus_device_id id_table[] = {
	 
	{ HV_SCSI_GUID,
	  .driver_data = SCSI_GUID
	},
	 
	{ HV_IDE_GUID,
	  .driver_data = IDE_GUID
	},
	 
	{
	  HV_SYNTHFC_GUID,
	  .driver_data = SFC_GUID
	},
	{ },
};

MODULE_DEVICE_TABLE(vmbus, id_table);

static const struct { guid_t guid; } fc_guid = { HV_SYNTHFC_GUID };

static bool hv_dev_is_fc(struct hv_device *hv_dev)
{
	return guid_equal(&fc_guid.guid, &hv_dev->dev_type);
}

static int storvsc_probe(struct hv_device *device,
			const struct hv_vmbus_device_id *dev_id)
{
	int ret;
	int num_cpus = num_online_cpus();
	int num_present_cpus = num_present_cpus();
	struct Scsi_Host *host;
	struct hv_host_device *host_dev;
	bool dev_is_ide = ((dev_id->driver_data == IDE_GUID) ? true : false);
	bool is_fc = ((dev_id->driver_data == SFC_GUID) ? true : false);
	int target = 0;
	struct storvsc_device *stor_device;
	int max_sub_channels = 0;
	u32 max_xfer_bytes;

	 
	if (!dev_is_ide)
		max_sub_channels =
			(num_cpus - 1) / storvsc_vcpus_per_sub_channel;

	scsi_driver.can_queue = max_outstanding_req_per_channel *
				(max_sub_channels + 1) *
				(100 - ring_avail_percent_lowater) / 100;

	host = scsi_host_alloc(&scsi_driver,
			       sizeof(struct hv_host_device));
	if (!host)
		return -ENOMEM;

	host_dev = shost_priv(host);
	memset(host_dev, 0, sizeof(struct hv_host_device));

	host_dev->port = host->host_no;
	host_dev->dev = device;
	host_dev->host = host;


	stor_device = kzalloc(sizeof(struct storvsc_device), GFP_KERNEL);
	if (!stor_device) {
		ret = -ENOMEM;
		goto err_out0;
	}

	stor_device->destroy = false;
	init_waitqueue_head(&stor_device->waiting_to_drain);
	stor_device->device = device;
	stor_device->host = host;
	spin_lock_init(&stor_device->lock);
	hv_set_drvdata(device, stor_device);
	dma_set_min_align_mask(&device->device, HV_HYP_PAGE_SIZE - 1);

	stor_device->port_number = host->host_no;
	ret = storvsc_connect_to_vsp(device, storvsc_ringbuffer_size, is_fc);
	if (ret)
		goto err_out1;

	host_dev->path = stor_device->path_id;
	host_dev->target = stor_device->target_id;

	switch (dev_id->driver_data) {
	case SFC_GUID:
		host->max_lun = STORVSC_FC_MAX_LUNS_PER_TARGET;
		host->max_id = STORVSC_FC_MAX_TARGETS;
		host->max_channel = STORVSC_FC_MAX_CHANNELS - 1;
#if IS_ENABLED(CONFIG_SCSI_FC_ATTRS)
		host->transportt = fc_transport_template;
#endif
		break;

	case SCSI_GUID:
		host->max_lun = STORVSC_MAX_LUNS_PER_TARGET;
		host->max_id = STORVSC_MAX_TARGETS;
		host->max_channel = STORVSC_MAX_CHANNELS - 1;
		break;

	default:
		host->max_lun = STORVSC_IDE_MAX_LUNS_PER_TARGET;
		host->max_id = STORVSC_IDE_MAX_TARGETS;
		host->max_channel = STORVSC_IDE_MAX_CHANNELS - 1;
		break;
	}
	 
	host->max_cmd_len = STORVSC_MAX_CMD_LEN;
	 
	max_xfer_bytes = round_down(stor_device->max_transfer_bytes, HV_HYP_PAGE_SIZE);
	if (is_fc)
		max_xfer_bytes = min(max_xfer_bytes, STORVSC_FC_MAX_XFER_SIZE);

	 
	host->max_sectors = max_xfer_bytes >> 9;
	 
	host->sg_tablesize = (max_xfer_bytes >> HV_HYP_PAGE_SHIFT) + 1;
	 
	if (!dev_is_ide) {
		if (storvsc_max_hw_queues > num_present_cpus) {
			storvsc_max_hw_queues = 0;
			storvsc_log(device, STORVSC_LOGGING_WARN,
				"Resetting invalid storvsc_max_hw_queues value to default.\n");
		}
		if (storvsc_max_hw_queues)
			host->nr_hw_queues = storvsc_max_hw_queues;
		else
			host->nr_hw_queues = num_present_cpus;
	}

	 
	host_dev->handle_error_wq =
			alloc_ordered_workqueue("storvsc_error_wq_%d",
						0,
						host->host_no);
	if (!host_dev->handle_error_wq) {
		ret = -ENOMEM;
		goto err_out2;
	}
	INIT_WORK(&host_dev->host_scan_work, storvsc_host_scan);
	 
	ret = scsi_add_host(host, &device->device);
	if (ret != 0)
		goto err_out3;

	if (!dev_is_ide) {
		scsi_scan_host(host);
	} else {
		target = (device->dev_instance.b[5] << 8 |
			 device->dev_instance.b[4]);
		ret = scsi_add_device(host, 0, target, 0);
		if (ret)
			goto err_out4;
	}
#if IS_ENABLED(CONFIG_SCSI_FC_ATTRS)
	if (host->transportt == fc_transport_template) {
		struct fc_rport_identifiers ids = {
			.roles = FC_PORT_ROLE_FCP_DUMMY_INITIATOR,
		};

		fc_host_node_name(host) = stor_device->node_name;
		fc_host_port_name(host) = stor_device->port_name;
		stor_device->rport = fc_remote_port_add(host, 0, &ids);
		if (!stor_device->rport) {
			ret = -ENOMEM;
			goto err_out4;
		}
	}
#endif
	return 0;

err_out4:
	scsi_remove_host(host);

err_out3:
	destroy_workqueue(host_dev->handle_error_wq);

err_out2:
	 
	storvsc_dev_remove(device);
	goto err_out0;

err_out1:
	kfree(stor_device->stor_chns);
	kfree(stor_device);

err_out0:
	scsi_host_put(host);
	return ret;
}

 
static int storvsc_change_queue_depth(struct scsi_device *sdev, int queue_depth)
{
	if (queue_depth > scsi_driver.can_queue)
		queue_depth = scsi_driver.can_queue;

	return scsi_change_queue_depth(sdev, queue_depth);
}

static void storvsc_remove(struct hv_device *dev)
{
	struct storvsc_device *stor_device = hv_get_drvdata(dev);
	struct Scsi_Host *host = stor_device->host;
	struct hv_host_device *host_dev = shost_priv(host);

#if IS_ENABLED(CONFIG_SCSI_FC_ATTRS)
	if (host->transportt == fc_transport_template) {
		fc_remote_port_delete(stor_device->rport);
		fc_remove_host(host);
	}
#endif
	destroy_workqueue(host_dev->handle_error_wq);
	scsi_remove_host(host);
	storvsc_dev_remove(dev);
	scsi_host_put(host);
}

static int storvsc_suspend(struct hv_device *hv_dev)
{
	struct storvsc_device *stor_device = hv_get_drvdata(hv_dev);
	struct Scsi_Host *host = stor_device->host;
	struct hv_host_device *host_dev = shost_priv(host);

	storvsc_wait_to_drain(stor_device);

	drain_workqueue(host_dev->handle_error_wq);

	vmbus_close(hv_dev->channel);

	kfree(stor_device->stor_chns);
	stor_device->stor_chns = NULL;

	cpumask_clear(&stor_device->alloced_cpus);

	return 0;
}

static int storvsc_resume(struct hv_device *hv_dev)
{
	int ret;

	ret = storvsc_connect_to_vsp(hv_dev, storvsc_ringbuffer_size,
				     hv_dev_is_fc(hv_dev));
	return ret;
}

static struct hv_driver storvsc_drv = {
	.name = KBUILD_MODNAME,
	.id_table = id_table,
	.probe = storvsc_probe,
	.remove = storvsc_remove,
	.suspend = storvsc_suspend,
	.resume = storvsc_resume,
	.driver = {
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};

#if IS_ENABLED(CONFIG_SCSI_FC_ATTRS)
static struct fc_function_template fc_transport_functions = {
	.show_host_node_name = 1,
	.show_host_port_name = 1,
};
#endif

static int __init storvsc_drv_init(void)
{
	int ret;

	 
	max_outstanding_req_per_channel =
		((storvsc_ringbuffer_size - PAGE_SIZE) /
		ALIGN(MAX_MULTIPAGE_BUFFER_PACKET +
		sizeof(struct vstor_packet) + sizeof(u64),
		sizeof(u64)));

#if IS_ENABLED(CONFIG_SCSI_FC_ATTRS)
	fc_transport_template = fc_attach_transport(&fc_transport_functions);
	if (!fc_transport_template)
		return -ENODEV;
#endif

	ret = vmbus_driver_register(&storvsc_drv);

#if IS_ENABLED(CONFIG_SCSI_FC_ATTRS)
	if (ret)
		fc_release_transport(fc_transport_template);
#endif

	return ret;
}

static void __exit storvsc_drv_exit(void)
{
	vmbus_driver_unregister(&storvsc_drv);
#if IS_ENABLED(CONFIG_SCSI_FC_ATTRS)
	fc_release_transport(fc_transport_template);
#endif
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Microsoft Hyper-V virtual storage driver");
module_init(storvsc_drv_init);
module_exit(storvsc_drv_exit);
