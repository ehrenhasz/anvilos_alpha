#ifndef _FM10K_MBX_H_
#define _FM10K_MBX_H_
struct fm10k_mbx_info;
#include "fm10k_type.h"
#include "fm10k_tlv.h"
#define FM10K_MBMEM(_n)		((_n) + 0x18000)
#define FM10K_MBMEM_VF(_n, _m)	(((_n) * 0x10) + (_m) + 0x18000)
#define FM10K_MBMEM_SM(_n)	((_n) + 0x18400)
#define FM10K_MBMEM_PF(_n)	((_n) + 0x18600)
#define FM10K_MBMEM_PF_XOR	(FM10K_MBMEM_SM(0) ^ FM10K_MBMEM_PF(0))
#define FM10K_MBX(_n)		((_n) + 0x18800)
#define FM10K_MBX_REQ				0x00000002
#define FM10K_MBX_ACK				0x00000004
#define FM10K_MBX_REQ_INTERRUPT			0x00000008
#define FM10K_MBX_ACK_INTERRUPT			0x00000010
#define FM10K_MBX_INTERRUPT_ENABLE		0x00000020
#define FM10K_MBX_INTERRUPT_DISABLE		0x00000040
#define FM10K_MBX_GLOBAL_REQ_INTERRUPT		0x00000200
#define FM10K_MBX_GLOBAL_ACK_INTERRUPT		0x00000400
#define FM10K_MBICR(_n)		((_n) + 0x18840)
#define FM10K_GMBX		0x18842
#define FM10K_VFMBX		0x00010
#define FM10K_VFMBMEM(_n)	((_n) + 0x00020)
#define FM10K_VFMBMEM_LEN	16
#define FM10K_VFMBMEM_VF_XOR	(FM10K_VFMBMEM_LEN / 2)
#define FM10K_MBX_DISCONNECT_TIMEOUT		500
#define FM10K_MBX_POLL_DELAY			19
#define FM10K_MBX_INT_DELAY			20
enum fm10k_mbx_state {
	FM10K_STATE_CLOSED,
	FM10K_STATE_CONNECT,
	FM10K_STATE_OPEN,
	FM10K_STATE_DISCONNECT,
};
#define FM10K_MSG_HDR_MASK(name) \
	((0x1u << FM10K_MSG_##name##_SIZE) - 1)
#define FM10K_MSG_HDR_FIELD_SET(value, name) \
	(((u32)(value) & FM10K_MSG_HDR_MASK(name)) << FM10K_MSG_##name##_SHIFT)
#define FM10K_MSG_HDR_FIELD_GET(value, name) \
	((u16)((value) >> FM10K_MSG_##name##_SHIFT) & FM10K_MSG_HDR_MASK(name))
#define FM10K_MSG_TYPE_SHIFT			0
#define FM10K_MSG_TYPE_SIZE			4
#define FM10K_MSG_TAIL_SHIFT			4
#define FM10K_MSG_TAIL_SIZE			4
#define FM10K_MSG_HEAD_SHIFT			8
#define FM10K_MSG_HEAD_SIZE			4
#define FM10K_MSG_RSVD0_SHIFT			12
#define FM10K_MSG_RSVD0_SIZE			4
#define FM10K_MSG_CRC_SHIFT			16
#define FM10K_MSG_CRC_SIZE			16
#define FM10K_MSG_CONNECT_SIZE_SHIFT		16
#define FM10K_MSG_CONNECT_SIZE_SIZE		16
#define FM10K_MSG_ERR_NO_SHIFT			16
#define FM10K_MSG_ERR_NO_SIZE			16
enum fm10k_msg_type {
	FM10K_MSG_DATA			= 0x8,
	FM10K_MSG_CONNECT		= 0xC,
	FM10K_MSG_DISCONNECT		= 0xD,
	FM10K_MSG_ERROR			= 0xE,
};
#define FM10K_SM_MBX_VERSION		1
#define FM10K_SM_MBX_FIFO_LEN		(FM10K_MBMEM_PF_XOR - 1)
#define FM10K_MSG_SM_TAIL_SHIFT			0
#define FM10K_MSG_SM_TAIL_SIZE			12
#define FM10K_MSG_SM_VER_SHIFT			12
#define FM10K_MSG_SM_VER_SIZE			4
#define FM10K_MSG_SM_HEAD_SHIFT			16
#define FM10K_MSG_SM_HEAD_SIZE			12
#define FM10K_MSG_SM_ERR_SHIFT			28
#define FM10K_MSG_SM_ERR_SIZE			4
#define FM10K_MBX_ERR(_n) ((_n) - 512)
#define FM10K_MBX_ERR_NO_MBX		FM10K_MBX_ERR(0x01)
#define FM10K_MBX_ERR_NO_SPACE		FM10K_MBX_ERR(0x03)
#define FM10K_MBX_ERR_TAIL		FM10K_MBX_ERR(0x05)
#define FM10K_MBX_ERR_HEAD		FM10K_MBX_ERR(0x06)
#define FM10K_MBX_ERR_SRC		FM10K_MBX_ERR(0x08)
#define FM10K_MBX_ERR_TYPE		FM10K_MBX_ERR(0x09)
#define FM10K_MBX_ERR_SIZE		FM10K_MBX_ERR(0x0B)
#define FM10K_MBX_ERR_BUSY		FM10K_MBX_ERR(0x0C)
#define FM10K_MBX_ERR_RSVD0		FM10K_MBX_ERR(0x0E)
#define FM10K_MBX_ERR_CRC		FM10K_MBX_ERR(0x0F)
#define FM10K_MBX_CRC_SEED		0xFFFF
struct fm10k_mbx_ops {
	s32 (*connect)(struct fm10k_hw *, struct fm10k_mbx_info *);
	void (*disconnect)(struct fm10k_hw *, struct fm10k_mbx_info *);
	bool (*rx_ready)(struct fm10k_mbx_info *);
	bool (*tx_ready)(struct fm10k_mbx_info *, u16);
	bool (*tx_complete)(struct fm10k_mbx_info *);
	s32 (*enqueue_tx)(struct fm10k_hw *, struct fm10k_mbx_info *,
			  const u32 *);
	s32 (*process)(struct fm10k_hw *, struct fm10k_mbx_info *);
	s32 (*register_handlers)(struct fm10k_mbx_info *,
				 const struct fm10k_msg_data *);
};
struct fm10k_mbx_fifo {
	u32 *buffer;
	u16 head;
	u16 tail;
	u16 size;
};
#define FM10K_MBX_TX_BUFFER_SIZE	512
#define FM10K_MBX_RX_BUFFER_SIZE	128
#define FM10K_MBX_BUFFER_SIZE \
	(FM10K_MBX_TX_BUFFER_SIZE + FM10K_MBX_RX_BUFFER_SIZE)
#define FM10K_MBX_MSG_MAX_SIZE \
	((FM10K_MBX_TX_BUFFER_SIZE - 1) & (FM10K_MBX_RX_BUFFER_SIZE - 1))
#define FM10K_VFMBX_MSG_MTU	((FM10K_VFMBMEM_LEN / 2) - 1)
#define FM10K_MBX_INIT_TIMEOUT	2000  
#define FM10K_MBX_INIT_DELAY	500   
struct fm10k_mbx_info {
	struct fm10k_mbx_ops ops;
	const struct fm10k_msg_data *msg_data;
	struct fm10k_mbx_fifo rx;
	struct fm10k_mbx_fifo tx;
	u32 timeout;
	u32 udelay;
	u32 mbx_reg, mbmem_reg, mbx_lock, mbx_hdr;
	u16 max_size, mbmem_len;
	u16 tail, tail_len, pulled;
	u16 head, head_len, pushed;
	u16 local, remote;
	enum fm10k_mbx_state state;
	s32 test_result;
	u64 tx_busy;
	u64 tx_dropped;
	u64 tx_messages;
	u64 tx_dwords;
	u64 tx_mbmem_pulled;
	u64 rx_messages;
	u64 rx_dwords;
	u64 rx_mbmem_pushed;
	u64 rx_parse_err;
	u32 buffer[FM10K_MBX_BUFFER_SIZE];
};
s32 fm10k_pfvf_mbx_init(struct fm10k_hw *, struct fm10k_mbx_info *,
			const struct fm10k_msg_data *, u8);
s32 fm10k_sm_mbx_init(struct fm10k_hw *, struct fm10k_mbx_info *,
		      const struct fm10k_msg_data *);
#endif  
