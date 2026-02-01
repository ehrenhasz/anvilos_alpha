 
 
#ifndef _SCMI_COMMON_H
#define _SCMI_COMMON_H

#include <linux/bitfield.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/hashtable.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/refcount.h>
#include <linux/scmi_protocol.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include <asm/unaligned.h>

#include "protocols.h"
#include "notify.h"

#define SCMI_MAX_CHANNELS		256

#define SCMI_MAX_RESPONSE_TIMEOUT	(2 * MSEC_PER_SEC)

enum scmi_error_codes {
	SCMI_SUCCESS = 0,	 
	SCMI_ERR_SUPPORT = -1,	 
	SCMI_ERR_PARAMS = -2,	 
	SCMI_ERR_ACCESS = -3,	 
	SCMI_ERR_ENTRY = -4,	 
	SCMI_ERR_RANGE = -5,	 
	SCMI_ERR_BUSY = -6,	 
	SCMI_ERR_COMMS = -7,	 
	SCMI_ERR_GENERIC = -8,	 
	SCMI_ERR_HARDWARE = -9,	 
	SCMI_ERR_PROTOCOL = -10, 
};

static const int scmi_linux_errmap[] = {
	 
	0,			 
	-EOPNOTSUPP,		 
	-EINVAL,		 
	-EACCES,		 
	-ENOENT,		 
	-ERANGE,		 
	-EBUSY,			 
	-ECOMM,			 
	-EIO,			 
	-EREMOTEIO,		 
	-EPROTO,		 
};

static inline int scmi_to_linux_errno(int errno)
{
	int err_idx = -errno;

	if (err_idx >= SCMI_SUCCESS && err_idx < ARRAY_SIZE(scmi_linux_errmap))
		return scmi_linux_errmap[err_idx];
	return -EIO;
}

#define MSG_ID_MASK		GENMASK(7, 0)
#define MSG_XTRACT_ID(hdr)	FIELD_GET(MSG_ID_MASK, (hdr))
#define MSG_TYPE_MASK		GENMASK(9, 8)
#define MSG_XTRACT_TYPE(hdr)	FIELD_GET(MSG_TYPE_MASK, (hdr))
#define MSG_TYPE_COMMAND	0
#define MSG_TYPE_DELAYED_RESP	2
#define MSG_TYPE_NOTIFICATION	3
#define MSG_PROTOCOL_ID_MASK	GENMASK(17, 10)
#define MSG_XTRACT_PROT_ID(hdr)	FIELD_GET(MSG_PROTOCOL_ID_MASK, (hdr))
#define MSG_TOKEN_ID_MASK	GENMASK(27, 18)
#define MSG_XTRACT_TOKEN(hdr)	FIELD_GET(MSG_TOKEN_ID_MASK, (hdr))
#define MSG_TOKEN_MAX		(MSG_XTRACT_TOKEN(MSG_TOKEN_ID_MASK) + 1)

 
#define SCMI_PENDING_XFERS_HT_ORDER_SZ		9

 
static inline u32 pack_scmi_header(struct scmi_msg_hdr *hdr)
{
	return FIELD_PREP(MSG_ID_MASK, hdr->id) |
		FIELD_PREP(MSG_TYPE_MASK, hdr->type) |
		FIELD_PREP(MSG_TOKEN_ID_MASK, hdr->seq) |
		FIELD_PREP(MSG_PROTOCOL_ID_MASK, hdr->protocol_id);
}

 
static inline void unpack_scmi_header(u32 msg_hdr, struct scmi_msg_hdr *hdr)
{
	hdr->id = MSG_XTRACT_ID(msg_hdr);
	hdr->protocol_id = MSG_XTRACT_PROT_ID(msg_hdr);
	hdr->type = MSG_XTRACT_TYPE(msg_hdr);
}

 
#define XFER_FIND(__ht, __k)					\
({								\
	typeof(__k) k_ = __k;					\
	struct scmi_xfer *xfer_ = NULL;				\
								\
	hash_for_each_possible((__ht), xfer_, node, k_)		\
		if (xfer_->hdr.seq == k_)			\
			break;					\
	xfer_;							\
})

struct scmi_revision_info *
scmi_revision_area_get(const struct scmi_protocol_handle *ph);
void scmi_setup_protocol_implemented(const struct scmi_protocol_handle *ph,
				     u8 *prot_imp);

extern struct bus_type scmi_bus_type;

#define SCMI_BUS_NOTIFY_DEVICE_REQUEST		0
#define SCMI_BUS_NOTIFY_DEVICE_UNREQUEST	1
extern struct blocking_notifier_head scmi_requested_devices_nh;

struct scmi_device *scmi_device_create(struct device_node *np,
				       struct device *parent, int protocol,
				       const char *name);
void scmi_device_destroy(struct device *parent, int protocol, const char *name);

int scmi_protocol_acquire(const struct scmi_handle *handle, u8 protocol_id);
void scmi_protocol_release(const struct scmi_handle *handle, u8 protocol_id);

 
 
struct scmi_chan_info {
	int id;
	struct device *dev;
	unsigned int rx_timeout_ms;
	struct scmi_handle *handle;
	bool no_completion_irq;
	void *transport_info;
};

 
struct scmi_transport_ops {
	int (*link_supplier)(struct device *dev);
	bool (*chan_available)(struct device_node *of_node, int idx);
	int (*chan_setup)(struct scmi_chan_info *cinfo, struct device *dev,
			  bool tx);
	int (*chan_free)(int id, void *p, void *data);
	unsigned int (*get_max_msg)(struct scmi_chan_info *base_cinfo);
	int (*send_message)(struct scmi_chan_info *cinfo,
			    struct scmi_xfer *xfer);
	void (*mark_txdone)(struct scmi_chan_info *cinfo, int ret,
			    struct scmi_xfer *xfer);
	void (*fetch_response)(struct scmi_chan_info *cinfo,
			       struct scmi_xfer *xfer);
	void (*fetch_notification)(struct scmi_chan_info *cinfo,
				   size_t max_len, struct scmi_xfer *xfer);
	void (*clear_channel)(struct scmi_chan_info *cinfo);
	bool (*poll_done)(struct scmi_chan_info *cinfo, struct scmi_xfer *xfer);
};

 
struct scmi_desc {
	int (*transport_init)(void);
	void (*transport_exit)(void);
	const struct scmi_transport_ops *ops;
	int max_rx_timeout_ms;
	int max_msg;
	int max_msg_size;
	const bool force_polling;
	const bool sync_cmds_completed_on_ret;
	const bool atomic_enabled;
};

static inline bool is_polling_required(struct scmi_chan_info *cinfo,
				       const struct scmi_desc *desc)
{
	return cinfo->no_completion_irq || desc->force_polling;
}

static inline bool is_transport_polling_capable(const struct scmi_desc *desc)
{
	return desc->ops->poll_done || desc->sync_cmds_completed_on_ret;
}

static inline bool is_polling_enabled(struct scmi_chan_info *cinfo,
				      const struct scmi_desc *desc)
{
	return is_polling_required(cinfo, desc) &&
		is_transport_polling_capable(desc);
}

void scmi_xfer_raw_put(const struct scmi_handle *handle,
		       struct scmi_xfer *xfer);
struct scmi_xfer *scmi_xfer_raw_get(const struct scmi_handle *handle);
struct scmi_chan_info *
scmi_xfer_raw_channel_get(const struct scmi_handle *handle, u8 protocol_id);

int scmi_xfer_raw_inflight_register(const struct scmi_handle *handle,
				    struct scmi_xfer *xfer);

int scmi_xfer_raw_wait_for_message_response(struct scmi_chan_info *cinfo,
					    struct scmi_xfer *xfer,
					    unsigned int timeout_ms);
#ifdef CONFIG_ARM_SCMI_TRANSPORT_MAILBOX
extern const struct scmi_desc scmi_mailbox_desc;
#endif
#ifdef CONFIG_ARM_SCMI_TRANSPORT_SMC
extern const struct scmi_desc scmi_smc_desc;
#endif
#ifdef CONFIG_ARM_SCMI_TRANSPORT_VIRTIO
extern const struct scmi_desc scmi_virtio_desc;
#endif
#ifdef CONFIG_ARM_SCMI_TRANSPORT_OPTEE
extern const struct scmi_desc scmi_optee_desc;
#endif

void scmi_rx_callback(struct scmi_chan_info *cinfo, u32 msg_hdr, void *priv);

 
struct scmi_shared_mem;

void shmem_tx_prepare(struct scmi_shared_mem __iomem *shmem,
		      struct scmi_xfer *xfer, struct scmi_chan_info *cinfo);
u32 shmem_read_header(struct scmi_shared_mem __iomem *shmem);
void shmem_fetch_response(struct scmi_shared_mem __iomem *shmem,
			  struct scmi_xfer *xfer);
void shmem_fetch_notification(struct scmi_shared_mem __iomem *shmem,
			      size_t max_len, struct scmi_xfer *xfer);
void shmem_clear_channel(struct scmi_shared_mem __iomem *shmem);
bool shmem_poll_done(struct scmi_shared_mem __iomem *shmem,
		     struct scmi_xfer *xfer);

 
struct scmi_msg_payld;

 
#define SCMI_MSG_MAX_PROT_OVERHEAD (2 * sizeof(__le32))

size_t msg_response_size(struct scmi_xfer *xfer);
size_t msg_command_size(struct scmi_xfer *xfer);
void msg_tx_prepare(struct scmi_msg_payld *msg, struct scmi_xfer *xfer);
u32 msg_read_header(struct scmi_msg_payld *msg);
void msg_fetch_response(struct scmi_msg_payld *msg, size_t len,
			struct scmi_xfer *xfer);
void msg_fetch_notification(struct scmi_msg_payld *msg, size_t len,
			    size_t max_len, struct scmi_xfer *xfer);

void scmi_notification_instance_data_set(const struct scmi_handle *handle,
					 void *priv);
void *scmi_notification_instance_data_get(const struct scmi_handle *handle);
#endif  
