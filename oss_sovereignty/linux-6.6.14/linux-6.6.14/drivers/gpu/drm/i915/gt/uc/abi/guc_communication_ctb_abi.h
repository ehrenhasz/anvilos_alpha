#ifndef _ABI_GUC_COMMUNICATION_CTB_ABI_H
#define _ABI_GUC_COMMUNICATION_CTB_ABI_H
#include <linux/types.h>
#include <linux/build_bug.h>
#include "guc_messages_abi.h"
struct guc_ct_buffer_desc {
	u32 head;
	u32 tail;
	u32 status;
#define GUC_CTB_STATUS_NO_ERROR				0
#define GUC_CTB_STATUS_OVERFLOW				BIT(0)
#define GUC_CTB_STATUS_UNDERFLOW			BIT(1)
#define GUC_CTB_STATUS_MISMATCH				BIT(2)
#define GUC_CTB_STATUS_UNUSED				BIT(3)
	u32 reserved[13];
} __packed;
static_assert(sizeof(struct guc_ct_buffer_desc) == 64);
#define GUC_CTB_HDR_LEN				1u
#define GUC_CTB_MSG_MIN_LEN			GUC_CTB_HDR_LEN
#define GUC_CTB_MSG_MAX_LEN			256u
#define GUC_CTB_MSG_0_FENCE			(0xffffU << 16)
#define GUC_CTB_MSG_0_FORMAT			(0xf << 12)
#define   GUC_CTB_FORMAT_HXG			0u
#define GUC_CTB_MSG_0_RESERVED			(0xf << 8)
#define GUC_CTB_MSG_0_NUM_DWORDS		(0xff << 0)
#define GUC_CTB_HXG_MSG_MIN_LEN		(GUC_CTB_MSG_MIN_LEN + GUC_HXG_MSG_MIN_LEN)
#define GUC_CTB_HXG_MSG_MAX_LEN		GUC_CTB_MSG_MAX_LEN
#endif  
