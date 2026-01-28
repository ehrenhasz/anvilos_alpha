#ifndef _SELINUX_OBJSEC_H_
#define _SELINUX_OBJSEC_H_
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/binfmts.h>
#include <linux/in.h>
#include <linux/spinlock.h>
#include <linux/lsm_hooks.h>
#include <linux/msg.h>
#include <net/net_namespace.h>
#include "flask.h"
#include "avc.h"
struct task_security_struct {
	u32 osid;		 
	u32 sid;		 
	u32 exec_sid;		 
	u32 create_sid;		 
	u32 keycreate_sid;	 
	u32 sockcreate_sid;	 
} __randomize_layout;
enum label_initialized {
	LABEL_INVALID,		 
	LABEL_INITIALIZED,	 
	LABEL_PENDING
};
struct inode_security_struct {
	struct inode *inode;	 
	struct list_head list;	 
	u32 task_sid;		 
	u32 sid;		 
	u16 sclass;		 
	unsigned char initialized;	 
	spinlock_t lock;
};
struct file_security_struct {
	u32 sid;		 
	u32 fown_sid;		 
	u32 isid;		 
	u32 pseqno;		 
};
struct superblock_security_struct {
	u32 sid;			 
	u32 def_sid;			 
	u32 mntpoint_sid;		 
	unsigned short behavior;	 
	unsigned short flags;		 
	struct mutex lock;
	struct list_head isec_head;
	spinlock_t isec_lock;
};
struct msg_security_struct {
	u32 sid;	 
};
struct ipc_security_struct {
	u16 sclass;	 
	u32 sid;	 
};
struct netif_security_struct {
	struct net *ns;			 
	int ifindex;			 
	u32 sid;			 
};
struct netnode_security_struct {
	union {
		__be32 ipv4;		 
		struct in6_addr ipv6;	 
	} addr;
	u32 sid;			 
	u16 family;			 
};
struct netport_security_struct {
	u32 sid;			 
	u16 port;			 
	u8 protocol;			 
};
struct sk_security_struct {
#ifdef CONFIG_NETLABEL
	enum {				 
		NLBL_UNSET = 0,
		NLBL_REQUIRE,
		NLBL_LABELED,
		NLBL_REQSKB,
		NLBL_CONNLABELED,
	} nlbl_state;
	struct netlbl_lsm_secattr *nlbl_secattr;  
#endif
	u32 sid;			 
	u32 peer_sid;			 
	u16 sclass;			 
	enum {				 
		SCTP_ASSOC_UNSET = 0,
		SCTP_ASSOC_SET,
	} sctp_assoc_state;
};
struct tun_security_struct {
	u32 sid;			 
};
struct key_security_struct {
	u32 sid;	 
};
struct ib_security_struct {
	u32 sid;         
};
struct pkey_security_struct {
	u64	subnet_prefix;  
	u16	pkey;	 
	u32	sid;	 
};
struct bpf_security_struct {
	u32 sid;   
};
struct perf_event_security_struct {
	u32 sid;   
};
extern struct lsm_blob_sizes selinux_blob_sizes;
static inline struct task_security_struct *selinux_cred(const struct cred *cred)
{
	return cred->security + selinux_blob_sizes.lbs_cred;
}
static inline struct file_security_struct *selinux_file(const struct file *file)
{
	return file->f_security + selinux_blob_sizes.lbs_file;
}
static inline struct inode_security_struct *selinux_inode(
						const struct inode *inode)
{
	if (unlikely(!inode->i_security))
		return NULL;
	return inode->i_security + selinux_blob_sizes.lbs_inode;
}
static inline struct msg_security_struct *selinux_msg_msg(
						const struct msg_msg *msg_msg)
{
	return msg_msg->security + selinux_blob_sizes.lbs_msg_msg;
}
static inline struct ipc_security_struct *selinux_ipc(
						const struct kern_ipc_perm *ipc)
{
	return ipc->security + selinux_blob_sizes.lbs_ipc;
}
static inline u32 current_sid(void)
{
	const struct task_security_struct *tsec = selinux_cred(current_cred());
	return tsec->sid;
}
static inline struct superblock_security_struct *selinux_superblock(
					const struct super_block *superblock)
{
	return superblock->s_security + selinux_blob_sizes.lbs_superblock;
}
#endif  
