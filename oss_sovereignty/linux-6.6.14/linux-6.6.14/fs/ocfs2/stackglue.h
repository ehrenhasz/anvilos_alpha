#ifndef STACKGLUE_H
#define STACKGLUE_H
#include <linux/types.h>
#include <linux/list.h>
#include <linux/dlmconstants.h>
#include "dlm/dlmapi.h"
#include <linux/dlm.h>
struct file;
struct file_lock;
#define DLM_LKF_LOCAL		0x00100000
#define GROUP_NAME_MAX		64
#define CLUSTER_NAME_MAX	16
struct ocfs2_protocol_version {
	u8 pv_major;
	u8 pv_minor;
};
struct fsdlm_lksb_plus_lvb {
	struct dlm_lksb lksb;
	char lvb[DLM_LVB_LEN];
};
struct ocfs2_cluster_connection;
struct ocfs2_dlm_lksb {
	 union {
		 struct dlm_lockstatus lksb_o2dlm;
		 struct dlm_lksb lksb_fsdlm;
		 struct fsdlm_lksb_plus_lvb padding;
	 };
	 struct ocfs2_cluster_connection *lksb_conn;
};
struct ocfs2_locking_protocol {
	struct ocfs2_protocol_version lp_max_version;
	void (*lp_lock_ast)(struct ocfs2_dlm_lksb *lksb);
	void (*lp_blocking_ast)(struct ocfs2_dlm_lksb *lksb, int level);
	void (*lp_unlock_ast)(struct ocfs2_dlm_lksb *lksb, int error);
};
struct ocfs2_cluster_connection {
	char cc_name[GROUP_NAME_MAX + 1];
	int cc_namelen;
	char cc_cluster_name[CLUSTER_NAME_MAX + 1];
	int cc_cluster_name_len;
	struct ocfs2_protocol_version cc_version;
	struct ocfs2_locking_protocol *cc_proto;
	void (*cc_recovery_handler)(int node_num, void *recovery_data);
	void *cc_recovery_data;
	void *cc_lockspace;
	void *cc_private;
};
struct ocfs2_stack_operations {
	int (*connect)(struct ocfs2_cluster_connection *conn);
	int (*disconnect)(struct ocfs2_cluster_connection *conn);
	int (*this_node)(struct ocfs2_cluster_connection *conn,
			 unsigned int *node);
	int (*dlm_lock)(struct ocfs2_cluster_connection *conn,
			int mode,
			struct ocfs2_dlm_lksb *lksb,
			u32 flags,
			void *name,
			unsigned int namelen);
	int (*dlm_unlock)(struct ocfs2_cluster_connection *conn,
			  struct ocfs2_dlm_lksb *lksb,
			  u32 flags);
	int (*lock_status)(struct ocfs2_dlm_lksb *lksb);
	int (*lvb_valid)(struct ocfs2_dlm_lksb *lksb);
	void *(*lock_lvb)(struct ocfs2_dlm_lksb *lksb);
	int (*plock)(struct ocfs2_cluster_connection *conn,
		     u64 ino,
		     struct file *file,
		     int cmd,
		     struct file_lock *fl);
	void (*dump_lksb)(struct ocfs2_dlm_lksb *lksb);
};
struct ocfs2_stack_plugin {
	char *sp_name;
	struct ocfs2_stack_operations *sp_ops;
	struct module *sp_owner;
	struct list_head sp_list;
	unsigned int sp_count;
	struct ocfs2_protocol_version sp_max_proto;
};
int ocfs2_cluster_connect(const char *stack_name,
			  const char *cluster_name,
			  int cluster_name_len,
			  const char *group,
			  int grouplen,
			  struct ocfs2_locking_protocol *lproto,
			  void (*recovery_handler)(int node_num,
						   void *recovery_data),
			  void *recovery_data,
			  struct ocfs2_cluster_connection **conn);
int ocfs2_cluster_connect_agnostic(const char *group,
				   int grouplen,
				   struct ocfs2_locking_protocol *lproto,
				   void (*recovery_handler)(int node_num,
							    void *recovery_data),
				   void *recovery_data,
				   struct ocfs2_cluster_connection **conn);
int ocfs2_cluster_disconnect(struct ocfs2_cluster_connection *conn,
			     int hangup_pending);
void ocfs2_cluster_hangup(const char *group, int grouplen);
int ocfs2_cluster_this_node(struct ocfs2_cluster_connection *conn,
			    unsigned int *node);
struct ocfs2_lock_res;
int ocfs2_dlm_lock(struct ocfs2_cluster_connection *conn,
		   int mode,
		   struct ocfs2_dlm_lksb *lksb,
		   u32 flags,
		   void *name,
		   unsigned int namelen);
int ocfs2_dlm_unlock(struct ocfs2_cluster_connection *conn,
		     struct ocfs2_dlm_lksb *lksb,
		     u32 flags);
int ocfs2_dlm_lock_status(struct ocfs2_dlm_lksb *lksb);
int ocfs2_dlm_lvb_valid(struct ocfs2_dlm_lksb *lksb);
void *ocfs2_dlm_lvb(struct ocfs2_dlm_lksb *lksb);
void ocfs2_dlm_dump_lksb(struct ocfs2_dlm_lksb *lksb);
int ocfs2_stack_supports_plocks(void);
int ocfs2_plock(struct ocfs2_cluster_connection *conn, u64 ino,
		struct file *file, int cmd, struct file_lock *fl);
void ocfs2_stack_glue_set_max_proto_version(struct ocfs2_protocol_version *max_proto);
int ocfs2_stack_glue_register(struct ocfs2_stack_plugin *plugin);
void ocfs2_stack_glue_unregister(struct ocfs2_stack_plugin *plugin);
extern struct kset *ocfs2_kset;
#endif   
