#ifndef _SCMI_NOTIFY_H
#define _SCMI_NOTIFY_H
#include <linux/device.h>
#include <linux/ktime.h>
#include <linux/types.h>
#define SCMI_PROTO_QUEUE_SZ	4096
struct scmi_event {
	u8	id;
	size_t	max_payld_sz;
	size_t	max_report_sz;
};
struct scmi_protocol_handle;
struct scmi_event_ops {
	int (*get_num_sources)(const struct scmi_protocol_handle *ph);
	int (*set_notify_enabled)(const struct scmi_protocol_handle *ph,
				  u8 evt_id, u32 src_id, bool enabled);
	void *(*fill_custom_report)(const struct scmi_protocol_handle *ph,
				    u8 evt_id, ktime_t timestamp,
				    const void *payld, size_t payld_sz,
				    void *report, u32 *src_id);
};
struct scmi_protocol_events {
	size_t				queue_sz;
	const struct scmi_event_ops	*ops;
	const struct scmi_event		*evts;
	unsigned int			num_events;
	unsigned int			num_sources;
};
int scmi_notification_init(struct scmi_handle *handle);
void scmi_notification_exit(struct scmi_handle *handle);
int scmi_register_protocol_events(const struct scmi_handle *handle, u8 proto_id,
				  const struct scmi_protocol_handle *ph,
				  const struct scmi_protocol_events *ee);
void scmi_deregister_protocol_events(const struct scmi_handle *handle,
				     u8 proto_id);
int scmi_notify(const struct scmi_handle *handle, u8 proto_id, u8 evt_id,
		const void *buf, size_t len, ktime_t ts);
#endif  
