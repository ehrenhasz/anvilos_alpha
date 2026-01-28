

#ifndef _SCMI_PROTOCOLS_H
#define _SCMI_PROTOCOLS_H

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

#define PROTOCOL_REV_MINOR_MASK	GENMASK(15, 0)
#define PROTOCOL_REV_MAJOR_MASK	GENMASK(31, 16)
#define PROTOCOL_REV_MAJOR(x)	((u16)(FIELD_GET(PROTOCOL_REV_MAJOR_MASK, (x))))
#define PROTOCOL_REV_MINOR(x)	((u16)(FIELD_GET(PROTOCOL_REV_MINOR_MASK, (x))))

enum scmi_common_cmd {
	PROTOCOL_VERSION = 0x0,
	PROTOCOL_ATTRIBUTES = 0x1,
	PROTOCOL_MESSAGE_ATTRIBUTES = 0x2,
};


struct scmi_msg_resp_prot_version {
	__le16 minor_version;
	__le16 major_version;
};


struct scmi_msg {
	void *buf;
	size_t len;
};


struct scmi_msg_hdr {
	u8 id;
	u8 protocol_id;
	u8 type;
	u16 seq;
	u32 status;
	bool poll_completion;
};


struct scmi_xfer {
	int transfer_id;
	struct scmi_msg_hdr hdr;
	struct scmi_msg tx;
	struct scmi_msg rx;
	struct completion done;
	struct completion *async_done;
	bool pending;
	struct hlist_node node;
	refcount_t users;
#define SCMI_XFER_FREE		0
#define SCMI_XFER_BUSY		1
	atomic_t busy;
#define SCMI_XFER_SENT_OK	0
#define SCMI_XFER_RESP_OK	1
#define SCMI_XFER_DRESP_OK	2
	int state;
#define SCMI_XFER_FLAG_IS_RAW	BIT(0)
#define SCMI_XFER_IS_RAW(x)	((x)->flags & SCMI_XFER_FLAG_IS_RAW)
#define SCMI_XFER_FLAG_CHAN_SET	BIT(1)
#define SCMI_XFER_IS_CHAN_SET(x)	\
	((x)->flags & SCMI_XFER_FLAG_CHAN_SET)
	int flags;
	
	spinlock_t lock;
	void *priv;
};

struct scmi_xfer_ops;
struct scmi_proto_helpers_ops;


struct scmi_protocol_handle {
	struct device *dev;
	const struct scmi_xfer_ops *xops;
	const struct scmi_proto_helpers_ops *hops;
	int (*set_priv)(const struct scmi_protocol_handle *ph, void *priv);
	void *(*get_priv)(const struct scmi_protocol_handle *ph);
};


struct scmi_iterator_state {
	unsigned int desc_index;
	unsigned int num_returned;
	unsigned int num_remaining;
	unsigned int max_resources;
	unsigned int loop_idx;
	size_t rx_len;
	void *priv;
};


struct scmi_iterator_ops {
	void (*prepare_message)(void *message, unsigned int desc_index,
				const void *priv);
	int (*update_state)(struct scmi_iterator_state *st,
			    const void *response, void *priv);
	int (*process_response)(const struct scmi_protocol_handle *ph,
				const void *response,
				struct scmi_iterator_state *st, void *priv);
};

struct scmi_fc_db_info {
	int width;
	u64 set;
	u64 mask;
	void __iomem *addr;
};

struct scmi_fc_info {
	void __iomem *set_addr;
	void __iomem *get_addr;
	struct scmi_fc_db_info *set_db;
};


struct scmi_proto_helpers_ops {
	int (*extended_name_get)(const struct scmi_protocol_handle *ph,
				 u8 cmd_id, u32 res_id, char *name, size_t len);
	void *(*iter_response_init)(const struct scmi_protocol_handle *ph,
				    struct scmi_iterator_ops *ops,
				    unsigned int max_resources, u8 msg_id,
				    size_t tx_size, void *priv);
	int (*iter_response_run)(void *iter);
	void (*fastchannel_init)(const struct scmi_protocol_handle *ph,
				 u8 describe_id, u32 message_id,
				 u32 valid_size, u32 domain,
				 void __iomem **p_addr,
				 struct scmi_fc_db_info **p_db);
	void (*fastchannel_db_ring)(struct scmi_fc_db_info *db);
};


struct scmi_xfer_ops {
	int (*version_get)(const struct scmi_protocol_handle *ph, u32 *version);
	int (*xfer_get_init)(const struct scmi_protocol_handle *ph, u8 msg_id,
			     size_t tx_size, size_t rx_size,
			     struct scmi_xfer **p);
	void (*reset_rx_to_maxsz)(const struct scmi_protocol_handle *ph,
				  struct scmi_xfer *xfer);
	int (*do_xfer)(const struct scmi_protocol_handle *ph,
		       struct scmi_xfer *xfer);
	int (*do_xfer_with_response)(const struct scmi_protocol_handle *ph,
				     struct scmi_xfer *xfer);
	void (*xfer_put)(const struct scmi_protocol_handle *ph,
			 struct scmi_xfer *xfer);
};

typedef int (*scmi_prot_init_ph_fn_t)(const struct scmi_protocol_handle *);


struct scmi_protocol {
	const u8				id;
	struct module				*owner;
	const scmi_prot_init_ph_fn_t		instance_init;
	const scmi_prot_init_ph_fn_t		instance_deinit;
	const void				*ops;
	const struct scmi_protocol_events	*events;
};

#define DEFINE_SCMI_PROTOCOL_REGISTER_UNREGISTER(name, proto)	\
static const struct scmi_protocol *__this_proto = &(proto);	\
								\
int __init scmi_##name##_register(void)				\
{								\
	return scmi_protocol_register(__this_proto);		\
}								\
								\
void __exit scmi_##name##_unregister(void)			\
{								\
	scmi_protocol_unregister(__this_proto);			\
}

#define DECLARE_SCMI_REGISTER_UNREGISTER(func)		\
	int __init scmi_##func##_register(void);	\
	void __exit scmi_##func##_unregister(void)
DECLARE_SCMI_REGISTER_UNREGISTER(base);
DECLARE_SCMI_REGISTER_UNREGISTER(clock);
DECLARE_SCMI_REGISTER_UNREGISTER(perf);
DECLARE_SCMI_REGISTER_UNREGISTER(power);
DECLARE_SCMI_REGISTER_UNREGISTER(reset);
DECLARE_SCMI_REGISTER_UNREGISTER(sensors);
DECLARE_SCMI_REGISTER_UNREGISTER(voltage);
DECLARE_SCMI_REGISTER_UNREGISTER(system);
DECLARE_SCMI_REGISTER_UNREGISTER(powercap);

#endif 
