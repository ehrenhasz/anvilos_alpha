#ifndef _QCA_FRAMING_H
#define _QCA_FRAMING_H
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/types.h>
#define QCAFRM_GATHER 0
#define QCAFRM_NOHEAD (QCAFRM_ERR_BASE - 1)
#define QCAFRM_NOTAIL (QCAFRM_ERR_BASE - 2)
#define QCAFRM_INVLEN (QCAFRM_ERR_BASE - 3)
#define QCAFRM_INVFRAME (QCAFRM_ERR_BASE - 4)
#define QCAFRM_MIN_MTU (ETH_ZLEN - ETH_HLEN)
#define QCAFRM_MAX_MTU ETH_DATA_LEN
#define QCAFRM_MIN_LEN (QCAFRM_MIN_MTU + ETH_HLEN)
#define QCAFRM_MAX_LEN (QCAFRM_MAX_MTU + VLAN_ETH_HLEN)
#define QCAFRM_HEADER_LEN 8
#define QCAFRM_FOOTER_LEN 2
#define QCAFRM_ERR_BASE -1000
enum qcafrm_state {
	QCAFRM_HW_LEN0 = 0x8000,
	QCAFRM_HW_LEN1 = QCAFRM_HW_LEN0 - 1,
	QCAFRM_HW_LEN2 = QCAFRM_HW_LEN1 - 1,
	QCAFRM_HW_LEN3 = QCAFRM_HW_LEN2 - 1,
	QCAFRM_WAIT_AA1 = QCAFRM_HW_LEN3 - 1,
	QCAFRM_WAIT_AA2 = QCAFRM_WAIT_AA1 - 1,
	QCAFRM_WAIT_AA3 = QCAFRM_WAIT_AA2 - 1,
	QCAFRM_WAIT_AA4 = QCAFRM_WAIT_AA3 - 1,
	QCAFRM_WAIT_LEN_BYTE0 = QCAFRM_WAIT_AA4 - 1,
	QCAFRM_WAIT_LEN_BYTE1 = QCAFRM_WAIT_AA4 - 2,
	QCAFRM_WAIT_RSVD_BYTE1 = QCAFRM_WAIT_AA4 - 3,
	QCAFRM_WAIT_RSVD_BYTE2 = QCAFRM_WAIT_AA4 - 4,
	QCAFRM_WAIT_551 = 1,
	QCAFRM_WAIT_552 = QCAFRM_WAIT_551 - 1
};
struct qcafrm_handle {
	enum qcafrm_state state;
	enum qcafrm_state init;
	u16 offset;
	u16 len;
};
u16 qcafrm_create_header(u8 *buf, u16 len);
u16 qcafrm_create_footer(u8 *buf);
static inline void qcafrm_fsm_init_spi(struct qcafrm_handle *handle)
{
	handle->init = QCAFRM_HW_LEN0;
	handle->state = handle->init;
}
static inline void qcafrm_fsm_init_uart(struct qcafrm_handle *handle)
{
	handle->init = QCAFRM_WAIT_AA1;
	handle->state = handle->init;
}
s32 qcafrm_fsm_decode(struct qcafrm_handle *handle, u8 *buf, u16 buf_len, u8 recv_byte);
#endif  
