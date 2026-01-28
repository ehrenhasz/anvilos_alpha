#ifndef _SELINUX_IB_PKEY_H
#define _SELINUX_IB_PKEY_H
#include <linux/types.h>
#include "flask.h"
#ifdef CONFIG_SECURITY_INFINIBAND
void sel_ib_pkey_flush(void);
int sel_ib_pkey_sid(u64 subnet_prefix, u16 pkey, u32 *sid);
#else
static inline void sel_ib_pkey_flush(void)
{
	return;
}
static inline int sel_ib_pkey_sid(u64 subnet_prefix, u16 pkey, u32 *sid)
{
	*sid = SECINITSID_UNLABELED;
	return 0;
}
#endif
#endif
