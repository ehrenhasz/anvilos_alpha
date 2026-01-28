#ifndef _NETLABEL_USER_H
#define _NETLABEL_USER_H
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/capability.h>
#include <linux/audit.h>
#include <net/netlink.h>
#include <net/genetlink.h>
#include <net/netlabel.h>
static inline void netlbl_netlink_auditinfo(struct netlbl_audit *audit_info)
{
	security_current_getsecid_subj(&audit_info->secid);
	audit_info->loginuid = audit_get_loginuid(current);
	audit_info->sessionid = audit_get_sessionid(current);
}
int netlbl_netlink_init(void);
struct audit_buffer *netlbl_audit_start_common(int type,
					      struct netlbl_audit *audit_info);
#endif
