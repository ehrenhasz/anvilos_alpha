
 

#include <linux/can/dev.h>

 

static const u8 dlc2len[] = {
	0, 1, 2, 3, 4, 5, 6, 7,
	8, 12, 16, 20, 24, 32, 48, 64
};

 
u8 can_fd_dlc2len(u8 dlc)
{
	return dlc2len[dlc & 0x0F];
}
EXPORT_SYMBOL_GPL(can_fd_dlc2len);

static const u8 len2dlc[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,	 
	9, 9, 9, 9,			 
	10, 10, 10, 10,			 
	11, 11, 11, 11,			 
	12, 12, 12, 12,			 
	13, 13, 13, 13, 13, 13, 13, 13,	 
	14, 14, 14, 14, 14, 14, 14, 14,	 
	14, 14, 14, 14, 14, 14, 14, 14,	 
	15, 15, 15, 15, 15, 15, 15, 15,	 
	15, 15, 15, 15, 15, 15, 15, 15	 
};

 
u8 can_fd_len2dlc(u8 len)
{
	 
	BUILD_BUG_ON(ARRAY_SIZE(len2dlc) != CANFD_MAX_DLEN + 1);

	if (unlikely(len > CANFD_MAX_DLEN))
		return CANFD_MAX_DLC;

	return len2dlc[len];
}
EXPORT_SYMBOL_GPL(can_fd_len2dlc);

 
unsigned int can_skb_get_frame_len(const struct sk_buff *skb)
{
	const struct canfd_frame *cf = (const struct canfd_frame *)skb->data;
	u8 len;

	if (can_is_canfd_skb(skb))
		len = canfd_sanitize_len(cf->len);
	else if (cf->can_id & CAN_RTR_FLAG)
		len = 0;
	else
		len = cf->len;

	return can_frame_bytes(can_is_canfd_skb(skb), cf->can_id & CAN_EFF_FLAG,
			       false, len);
}
EXPORT_SYMBOL_GPL(can_skb_get_frame_len);
