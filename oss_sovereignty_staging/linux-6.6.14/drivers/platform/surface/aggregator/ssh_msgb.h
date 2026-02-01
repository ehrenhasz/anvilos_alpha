 
 

#ifndef _SURFACE_AGGREGATOR_SSH_MSGB_H
#define _SURFACE_AGGREGATOR_SSH_MSGB_H

#include <asm/unaligned.h>
#include <linux/types.h>

#include <linux/surface_aggregator/controller.h>
#include <linux/surface_aggregator/serial_hub.h>

 
struct msgbuf {
	u8 *begin;
	u8 *end;
	u8 *ptr;
};

 
static inline void msgb_init(struct msgbuf *msgb, u8 *ptr, size_t cap)
{
	msgb->begin = ptr;
	msgb->end = ptr + cap;
	msgb->ptr = ptr;
}

 
static inline size_t msgb_bytes_used(const struct msgbuf *msgb)
{
	return msgb->ptr - msgb->begin;
}

static inline void __msgb_push_u8(struct msgbuf *msgb, u8 value)
{
	*msgb->ptr = value;
	msgb->ptr += sizeof(u8);
}

static inline void __msgb_push_u16(struct msgbuf *msgb, u16 value)
{
	put_unaligned_le16(value, msgb->ptr);
	msgb->ptr += sizeof(u16);
}

 
static inline void msgb_push_u16(struct msgbuf *msgb, u16 value)
{
	if (WARN_ON(msgb->ptr + sizeof(u16) > msgb->end))
		return;

	__msgb_push_u16(msgb, value);
}

 
static inline void msgb_push_syn(struct msgbuf *msgb)
{
	msgb_push_u16(msgb, SSH_MSG_SYN);
}

 
static inline void msgb_push_buf(struct msgbuf *msgb, const u8 *buf, size_t len)
{
	msgb->ptr = memcpy(msgb->ptr, buf, len) + len;
}

 
static inline void msgb_push_crc(struct msgbuf *msgb, const u8 *buf, size_t len)
{
	msgb_push_u16(msgb, ssh_crc(buf, len));
}

 
static inline void msgb_push_frame(struct msgbuf *msgb, u8 ty, u16 len, u8 seq)
{
	u8 *const begin = msgb->ptr;

	if (WARN_ON(msgb->ptr + sizeof(struct ssh_frame) > msgb->end))
		return;

	__msgb_push_u8(msgb, ty);	 
	__msgb_push_u16(msgb, len);	 
	__msgb_push_u8(msgb, seq);	 

	msgb_push_crc(msgb, begin, msgb->ptr - begin);
}

 
static inline void msgb_push_ack(struct msgbuf *msgb, u8 seq)
{
	 
	msgb_push_syn(msgb);

	 
	msgb_push_frame(msgb, SSH_FRAME_TYPE_ACK, 0x00, seq);

	 
	msgb_push_crc(msgb, msgb->ptr, 0);
}

 
static inline void msgb_push_nak(struct msgbuf *msgb)
{
	 
	msgb_push_syn(msgb);

	 
	msgb_push_frame(msgb, SSH_FRAME_TYPE_NAK, 0x00, 0x00);

	 
	msgb_push_crc(msgb, msgb->ptr, 0);
}

 
static inline void msgb_push_cmd(struct msgbuf *msgb, u8 seq, u16 rqid,
				 const struct ssam_request *rqst)
{
	const u8 type = SSH_FRAME_TYPE_DATA_SEQ;
	u8 *cmd;

	 
	msgb_push_syn(msgb);

	 
	msgb_push_frame(msgb, type, sizeof(struct ssh_command) + rqst->length, seq);

	 
	if (WARN_ON(msgb->ptr + sizeof(struct ssh_command) > msgb->end))
		return;

	cmd = msgb->ptr;

	__msgb_push_u8(msgb, SSH_PLD_TYPE_CMD);		 
	__msgb_push_u8(msgb, rqst->target_category);	 
	__msgb_push_u8(msgb, rqst->target_id);		 
	__msgb_push_u8(msgb, SSAM_SSH_TID_HOST);	 
	__msgb_push_u8(msgb, rqst->instance_id);	 
	__msgb_push_u16(msgb, rqid);			 
	__msgb_push_u8(msgb, rqst->command_id);		 

	 
	msgb_push_buf(msgb, rqst->payload, rqst->length);

	 
	msgb_push_crc(msgb, cmd, msgb->ptr - cmd);
}

#endif  
