 

#ifndef USNIC_IB_QP_GRP_H_
#define USNIC_IB_QP_GRP_H_

#include <linux/debugfs.h>
#include <rdma/ib_verbs.h>

#include "usnic_ib.h"
#include "usnic_abi.h"
#include "usnic_fwd.h"
#include "usnic_vnic.h"

 
struct usnic_ib_qp_grp {
	struct ib_qp				ibqp;
	enum ib_qp_state			state;
	int					grp_id;

	struct usnic_fwd_dev			*ufdev;
	struct usnic_ib_ucontext		*ctx;
	struct list_head			flows_lst;

	struct usnic_vnic_res_chunk		**res_chunk_list;

	pid_t					owner_pid;
	struct usnic_ib_vf			*vf;
	struct list_head			link;

	spinlock_t				lock;

	struct kobject				kobj;
};

struct usnic_ib_qp_grp_flow {
	struct usnic_fwd_flow		*flow;
	enum usnic_transport_type	trans_type;
	union {
		struct {
			uint16_t	port_num;
		} usnic_roce;
		struct {
			struct socket	*sock;
		} udp;
	};
	struct usnic_ib_qp_grp		*qp_grp;
	struct list_head		link;

	 
	struct dentry			*dbgfs_dentry;
	char				dentry_name[32];
};

extern const struct usnic_vnic_res_spec min_transport_spec[USNIC_TRANSPORT_MAX];

const char *usnic_ib_qp_grp_state_to_string(enum ib_qp_state state);
int usnic_ib_qp_grp_dump_hdr(char *buf, int buf_sz);
int usnic_ib_qp_grp_dump_rows(void *obj, char *buf, int buf_sz);
int usnic_ib_qp_grp_create(struct usnic_ib_qp_grp *qp,
			   struct usnic_fwd_dev *ufdev, struct usnic_ib_vf *vf,
			   struct usnic_ib_pd *pd,
			   struct usnic_vnic_res_spec *res_spec,
			   struct usnic_transport_spec *trans_spec);
void usnic_ib_qp_grp_destroy(struct usnic_ib_qp_grp *qp_grp);
int usnic_ib_qp_grp_modify(struct usnic_ib_qp_grp *qp_grp,
				enum ib_qp_state new_state,
				void *data);
struct usnic_vnic_res_chunk
*usnic_ib_qp_grp_get_chunk(struct usnic_ib_qp_grp *qp_grp,
				enum usnic_vnic_res_type type);
static inline
struct usnic_ib_qp_grp *to_uqp_grp(struct ib_qp *ibqp)
{
	return container_of(ibqp, struct usnic_ib_qp_grp, ibqp);
}
#endif  
