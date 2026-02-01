
 

#include <linux/types.h>

#include "common.h"

 
struct scmi_msg_payld {
	__le32 msg_header;
	__le32 msg_payload[];
};

 
size_t msg_command_size(struct scmi_xfer *xfer)
{
	return sizeof(struct scmi_msg_payld) + xfer->tx.len;
}

 
size_t msg_response_size(struct scmi_xfer *xfer)
{
	return sizeof(struct scmi_msg_payld) + sizeof(__le32) + xfer->rx.len;
}

 
void msg_tx_prepare(struct scmi_msg_payld *msg, struct scmi_xfer *xfer)
{
	msg->msg_header = cpu_to_le32(pack_scmi_header(&xfer->hdr));
	if (xfer->tx.buf)
		memcpy(msg->msg_payload, xfer->tx.buf, xfer->tx.len);
}

 
u32 msg_read_header(struct scmi_msg_payld *msg)
{
	return le32_to_cpu(msg->msg_header);
}

 
void msg_fetch_response(struct scmi_msg_payld *msg, size_t len,
			struct scmi_xfer *xfer)
{
	size_t prefix_len = sizeof(*msg) + sizeof(msg->msg_payload[0]);

	xfer->hdr.status = le32_to_cpu(msg->msg_payload[0]);
	xfer->rx.len = min_t(size_t, xfer->rx.len,
			     len >= prefix_len ? len - prefix_len : 0);

	 
	memcpy(xfer->rx.buf, &msg->msg_payload[1], xfer->rx.len);
}

 
void msg_fetch_notification(struct scmi_msg_payld *msg, size_t len,
			    size_t max_len, struct scmi_xfer *xfer)
{
	xfer->rx.len = min_t(size_t, max_len,
			     len >= sizeof(*msg) ? len - sizeof(*msg) : 0);

	 
	memcpy(xfer->rx.buf, msg->msg_payload, xfer->rx.len);
}
