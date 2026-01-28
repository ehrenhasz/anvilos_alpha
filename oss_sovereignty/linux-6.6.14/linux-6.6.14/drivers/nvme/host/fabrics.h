#ifndef _NVME_FABRICS_H
#define _NVME_FABRICS_H 1
#include <linux/in.h>
#include <linux/inet.h>
#define NVMF_MIN_QUEUE_SIZE	16
#define NVMF_MAX_QUEUE_SIZE	1024
#define NVMF_DEF_QUEUE_SIZE	128
#define NVMF_DEF_RECONNECT_DELAY	10
#define NVMF_DEF_CTRL_LOSS_TMO		600
#define NVMF_DEF_FAIL_FAST_TMO		-1
#define NVMF_RESERVED_TAGS	1
struct nvmf_host {
	struct kref		ref;
	struct list_head	list;
	char			nqn[NVMF_NQN_SIZE];
	uuid_t			id;
};
enum {
	NVMF_OPT_ERR		= 0,
	NVMF_OPT_TRANSPORT	= 1 << 0,
	NVMF_OPT_NQN		= 1 << 1,
	NVMF_OPT_TRADDR		= 1 << 2,
	NVMF_OPT_TRSVCID	= 1 << 3,
	NVMF_OPT_QUEUE_SIZE	= 1 << 4,
	NVMF_OPT_NR_IO_QUEUES	= 1 << 5,
	NVMF_OPT_TL_RETRY_COUNT	= 1 << 6,
	NVMF_OPT_KATO		= 1 << 7,
	NVMF_OPT_HOSTNQN	= 1 << 8,
	NVMF_OPT_RECONNECT_DELAY = 1 << 9,
	NVMF_OPT_HOST_TRADDR	= 1 << 10,
	NVMF_OPT_CTRL_LOSS_TMO	= 1 << 11,
	NVMF_OPT_HOST_ID	= 1 << 12,
	NVMF_OPT_DUP_CONNECT	= 1 << 13,
	NVMF_OPT_DISABLE_SQFLOW = 1 << 14,
	NVMF_OPT_HDR_DIGEST	= 1 << 15,
	NVMF_OPT_DATA_DIGEST	= 1 << 16,
	NVMF_OPT_NR_WRITE_QUEUES = 1 << 17,
	NVMF_OPT_NR_POLL_QUEUES = 1 << 18,
	NVMF_OPT_TOS		= 1 << 19,
	NVMF_OPT_FAIL_FAST_TMO	= 1 << 20,
	NVMF_OPT_HOST_IFACE	= 1 << 21,
	NVMF_OPT_DISCOVERY	= 1 << 22,
	NVMF_OPT_DHCHAP_SECRET	= 1 << 23,
	NVMF_OPT_DHCHAP_CTRL_SECRET = 1 << 24,
};
struct nvmf_ctrl_options {
	unsigned		mask;
	int			max_reconnects;
	char			*transport;
	char			*subsysnqn;
	char			*traddr;
	char			*trsvcid;
	char			*host_traddr;
	char			*host_iface;
	size_t			queue_size;
	unsigned int		nr_io_queues;
	unsigned int		reconnect_delay;
	bool			discovery_nqn;
	bool			duplicate_connect;
	unsigned int		kato;
	struct nvmf_host	*host;
	char			*dhchap_secret;
	char			*dhchap_ctrl_secret;
	bool			disable_sqflow;
	bool			hdr_digest;
	bool			data_digest;
	unsigned int		nr_write_queues;
	unsigned int		nr_poll_queues;
	int			tos;
	int			fast_io_fail_tmo;
};
struct nvmf_transport_ops {
	struct list_head	entry;
	struct module		*module;
	const char		*name;
	int			required_opts;
	int			allowed_opts;
	struct nvme_ctrl	*(*create_ctrl)(struct device *dev,
					struct nvmf_ctrl_options *opts);
};
static inline bool
nvmf_ctlr_matches_baseopts(struct nvme_ctrl *ctrl,
			struct nvmf_ctrl_options *opts)
{
	if (ctrl->state == NVME_CTRL_DELETING ||
	    ctrl->state == NVME_CTRL_DELETING_NOIO ||
	    ctrl->state == NVME_CTRL_DEAD ||
	    strcmp(opts->subsysnqn, ctrl->opts->subsysnqn) ||
	    strcmp(opts->host->nqn, ctrl->opts->host->nqn) ||
	    !uuid_equal(&opts->host->id, &ctrl->opts->host->id))
		return false;
	return true;
}
static inline char *nvmf_ctrl_subsysnqn(struct nvme_ctrl *ctrl)
{
	if (!ctrl->subsys ||
	    !strcmp(ctrl->opts->subsysnqn, NVME_DISC_SUBSYS_NAME))
		return ctrl->opts->subsysnqn;
	return ctrl->subsys->subnqn;
}
static inline void nvmf_complete_timed_out_request(struct request *rq)
{
	if (blk_mq_request_started(rq) && !blk_mq_request_completed(rq)) {
		nvme_req(rq)->status = NVME_SC_HOST_ABORTED_CMD;
		blk_mq_complete_request(rq);
	}
}
static inline unsigned int nvmf_nr_io_queues(struct nvmf_ctrl_options *opts)
{
	return min(opts->nr_io_queues, num_online_cpus()) +
		min(opts->nr_write_queues, num_online_cpus()) +
		min(opts->nr_poll_queues, num_online_cpus());
}
int nvmf_reg_read32(struct nvme_ctrl *ctrl, u32 off, u32 *val);
int nvmf_reg_read64(struct nvme_ctrl *ctrl, u32 off, u64 *val);
int nvmf_reg_write32(struct nvme_ctrl *ctrl, u32 off, u32 val);
int nvmf_connect_admin_queue(struct nvme_ctrl *ctrl);
int nvmf_connect_io_queue(struct nvme_ctrl *ctrl, u16 qid);
int nvmf_register_transport(struct nvmf_transport_ops *ops);
void nvmf_unregister_transport(struct nvmf_transport_ops *ops);
void nvmf_free_options(struct nvmf_ctrl_options *opts);
int nvmf_get_address(struct nvme_ctrl *ctrl, char *buf, int size);
bool nvmf_should_reconnect(struct nvme_ctrl *ctrl);
bool nvmf_ip_options_match(struct nvme_ctrl *ctrl,
		struct nvmf_ctrl_options *opts);
void nvmf_set_io_queues(struct nvmf_ctrl_options *opts, u32 nr_io_queues,
			u32 io_queues[HCTX_MAX_TYPES]);
void nvmf_map_queues(struct blk_mq_tag_set *set, struct nvme_ctrl *ctrl,
		     u32 io_queues[HCTX_MAX_TYPES]);
#endif  
