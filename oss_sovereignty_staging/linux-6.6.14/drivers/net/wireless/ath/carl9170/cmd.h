 
#ifndef __CMD_H
#define __CMD_H

#include "carl9170.h"

 
int carl9170_write_reg(struct ar9170 *ar, const u32 reg, const u32 val);
int carl9170_read_reg(struct ar9170 *ar, const u32 reg, u32 *val);
int carl9170_read_mreg(struct ar9170 *ar, const int nregs,
		       const u32 *regs, u32 *out);
int carl9170_echo_test(struct ar9170 *ar, u32 v);
int carl9170_reboot(struct ar9170 *ar);
int carl9170_mac_reset(struct ar9170 *ar);
int carl9170_powersave(struct ar9170 *ar, const bool power_on);
int carl9170_collect_tally(struct ar9170 *ar);
int carl9170_bcn_ctrl(struct ar9170 *ar, const unsigned int vif_id,
		       const u32 mode, const u32 addr, const u32 len);

static inline int carl9170_flush_cab(struct ar9170 *ar,
				     const unsigned int vif_id)
{
	return carl9170_bcn_ctrl(ar, vif_id, CARL9170_BCN_CTRL_DRAIN, 0, 0);
}

static inline int carl9170_rx_filter(struct ar9170 *ar,
				     const unsigned int _rx_filter)
{
	__le32 rx_filter = cpu_to_le32(_rx_filter);

	return carl9170_exec_cmd(ar, CARL9170_CMD_RX_FILTER,
				sizeof(rx_filter), (u8 *)&rx_filter,
				0, NULL);
}

struct carl9170_cmd *carl9170_cmd_buf(struct ar9170 *ar,
	const enum carl9170_cmd_oids cmd, const unsigned int len);

 
#define carl9170_regwrite_begin(ar)					\
do {									\
	int __nreg = 0, __err = 0;					\
	struct ar9170 *__ar = ar;

#define carl9170_regwrite(r, v) do {					\
	__ar->cmd_buf[2 * __nreg + 1] = cpu_to_le32(r);			\
	__ar->cmd_buf[2 * __nreg + 2] = cpu_to_le32(v);			\
	__nreg++;							\
	if ((__nreg >= PAYLOAD_MAX / 2)) {				\
		if (IS_ACCEPTING_CMD(__ar))				\
			__err = carl9170_exec_cmd(__ar,			\
				CARL9170_CMD_WREG, 8 * __nreg,		\
				(u8 *) &__ar->cmd_buf[1], 0, NULL);	\
		else							\
			goto __regwrite_out;				\
									\
		__nreg = 0;						\
		if (__err)						\
			goto __regwrite_out;				\
	}								\
} while (0)

#define carl9170_regwrite_finish()					\
__regwrite_out :							\
	if (__err == 0 && __nreg) {					\
		if (IS_ACCEPTING_CMD(__ar))				\
			__err = carl9170_exec_cmd(__ar,			\
				CARL9170_CMD_WREG, 8 * __nreg,		\
				(u8 *) &__ar->cmd_buf[1], 0, NULL);	\
		__nreg = 0;						\
	}

#define carl9170_regwrite_result()					\
	__err;								\
} while (0)


#define carl9170_async_regwrite_get_buf()				\
do {									\
	__nreg = 0;							\
	__cmd = carl9170_cmd_buf(__carl, CARL9170_CMD_WREG_ASYNC,	\
				 CARL9170_MAX_CMD_PAYLOAD_LEN);		\
	if (__cmd == NULL) {						\
		__err = -ENOMEM;					\
		goto __async_regwrite_out;				\
	}								\
} while (0)

#define carl9170_async_regwrite_begin(carl)				\
do {									\
	struct ar9170 *__carl = carl;					\
	struct carl9170_cmd *__cmd;					\
	unsigned int __nreg;						\
	int  __err = 0;							\
	carl9170_async_regwrite_get_buf();				\

#define carl9170_async_regwrite_flush()					\
do {									\
	if (__cmd == NULL || __nreg == 0)				\
		break;							\
									\
	if (IS_ACCEPTING_CMD(__carl) && __nreg) {			\
		__cmd->hdr.len = 8 * __nreg;				\
		__err = __carl9170_exec_cmd(__carl, __cmd, true);	\
		__cmd = NULL;						\
		break;							\
	}								\
	goto __async_regwrite_out;					\
} while (0)

#define carl9170_async_regwrite(r, v) do {				\
	if (__cmd == NULL)						\
		carl9170_async_regwrite_get_buf();			\
	__cmd->wreg.regs[__nreg].addr = cpu_to_le32(r);			\
	__cmd->wreg.regs[__nreg].val = cpu_to_le32(v);			\
	__nreg++;							\
	if ((__nreg >= PAYLOAD_MAX / 2))				\
		carl9170_async_regwrite_flush();			\
} while (0)

#define carl9170_async_regwrite_finish() do {				\
__async_regwrite_out:							\
	if (__cmd != NULL && __err == 0)				\
		carl9170_async_regwrite_flush();			\
	kfree(__cmd);							\
} while (0)								\

#define carl9170_async_regwrite_result()				\
	__err;								\
} while (0)

#endif  
