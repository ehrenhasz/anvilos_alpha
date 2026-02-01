 
#ifndef _IWPM_UTIL_H
#define _IWPM_UTIL_H

#include <linux/io.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/jhash.h>
#include <linux/kref.h>
#include <net/netlink.h>
#include <linux/errno.h>
#include <rdma/iw_portmap.h>
#include <rdma/rdma_netlink.h>


#define IWPM_NL_RETRANS		3
#define IWPM_NL_TIMEOUT		(10*HZ)
#define IWPM_MAPINFO_SKB_COUNT	20

#define IWPM_PID_UNDEFINED     -1
#define IWPM_PID_UNAVAILABLE   -2

#define IWPM_REG_UNDEF          0x01
#define IWPM_REG_VALID          0x02
#define IWPM_REG_INCOMPL        0x04

struct iwpm_nlmsg_request {
	struct list_head    inprocess_list;
	__u32               nlmsg_seq;
	void                *req_buffer;
	u8	            nl_client;
	u8                  request_done;
	u16                 err_code;
	struct semaphore    sem;
	struct kref         kref;
};

struct iwpm_mapping_info {
	struct hlist_node hlist_node;
	struct sockaddr_storage local_sockaddr;
	struct sockaddr_storage mapped_sockaddr;
	u8     nl_client;
	u32    map_flags;
};

struct iwpm_remote_info {
	struct hlist_node hlist_node;
	struct sockaddr_storage remote_sockaddr;
	struct sockaddr_storage mapped_loc_sockaddr;
	struct sockaddr_storage mapped_rem_sockaddr;
	u8     nl_client;
};

struct iwpm_admin_data {
	atomic_t nlmsg_seq;
	u32      reg_list[RDMA_NL_NUM_CLIENTS];
};

 
struct iwpm_nlmsg_request *iwpm_get_nlmsg_request(__u32 nlmsg_seq,
						u8 nl_client, gfp_t gfp);

 
void iwpm_free_nlmsg_request(struct kref *kref);

 
struct iwpm_nlmsg_request *iwpm_find_nlmsg_request(__u32 echo_seq);

 
int iwpm_wait_complete_req(struct iwpm_nlmsg_request *nlmsg_request);

 
int iwpm_get_nlmsg_seq(void);

 
void iwpm_add_remote_info(struct iwpm_remote_info *reminfo);

 
u32 iwpm_check_registration(u8 nl_client, u32 reg);

 
void iwpm_set_registration(u8 nl_client, u32 reg);

 
u32 iwpm_get_registration(u8 nl_client);

 
int iwpm_send_mapinfo(u8 nl_client, int iwpm_pid);

 
int iwpm_mapinfo_available(void);

 
int iwpm_compare_sockaddr(struct sockaddr_storage *a_sockaddr,
			struct sockaddr_storage *b_sockaddr);

 
static inline int iwpm_validate_nlmsg_attr(struct nlattr *nltb[],
					   int nla_count)
{
	int i;
	for (i = 1; i < nla_count; i++) {
		if (!nltb[i])
			return -EINVAL;
	}
	return 0;
}

 
struct sk_buff *iwpm_create_nlmsg(u32 nl_op, struct nlmsghdr **nlh,
					int nl_client);

 
int iwpm_parse_nlmsg(struct netlink_callback *cb, int policy_max,
				const struct nla_policy *nlmsg_policy,
				struct nlattr *nltb[], const char *msg_type);

 
void iwpm_print_sockaddr(struct sockaddr_storage *sockaddr, char *msg);

 
int iwpm_send_hello(u8 nl_client, int iwpm_pid, u16 abi_version);
extern u16 iwpm_ulib_version;
#endif
