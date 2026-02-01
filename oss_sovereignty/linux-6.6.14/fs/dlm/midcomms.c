
 

 

 
#define DLM_DEBUG_FENCE_TERMINATION	0

#include <trace/events/dlm.h>
#include <net/tcp.h>

#include "dlm_internal.h"
#include "lowcomms.h"
#include "config.h"
#include "memory.h"
#include "lock.h"
#include "util.h"
#include "midcomms.h"

 
#define DLM_SEQ_INIT		0
 
#define DLM_SHUTDOWN_TIMEOUT	msecs_to_jiffies(5000)
#define DLM_VERSION_NOT_SET	0
#define DLM_SEND_ACK_BACK_MSG_THRESHOLD 32
#define DLM_RECV_ACK_BACK_MSG_THRESHOLD (DLM_SEND_ACK_BACK_MSG_THRESHOLD * 8)

struct midcomms_node {
	int nodeid;
	uint32_t version;
	atomic_t seq_send;
	atomic_t seq_next;
	 
	struct list_head send_queue;
	spinlock_t send_queue_lock;
	atomic_t send_queue_cnt;
#define DLM_NODE_FLAG_CLOSE	1
#define DLM_NODE_FLAG_STOP_TX	2
#define DLM_NODE_FLAG_STOP_RX	3
	atomic_t ulp_delivered;
	unsigned long flags;
	wait_queue_head_t shutdown_wait;

	 
#define DLM_CLOSED	1
#define DLM_ESTABLISHED	2
#define DLM_FIN_WAIT1	3
#define DLM_FIN_WAIT2	4
#define DLM_CLOSE_WAIT	5
#define DLM_LAST_ACK	6
#define DLM_CLOSING	7
	int state;
	spinlock_t state_lock;

	 
	int users;

	 
	void *debugfs;

	struct hlist_node hlist;
	struct rcu_head rcu;
};

struct dlm_mhandle {
	const union dlm_packet *inner_p;
	struct midcomms_node *node;
	struct dlm_opts *opts;
	struct dlm_msg *msg;
	bool committed;
	uint32_t seq;

	void (*ack_rcv)(struct midcomms_node *node);

	 
	int idx;

	struct list_head list;
	struct rcu_head rcu;
};

static struct hlist_head node_hash[CONN_HASH_SIZE];
static DEFINE_SPINLOCK(nodes_lock);
DEFINE_STATIC_SRCU(nodes_srcu);

 
static DEFINE_MUTEX(close_lock);

struct kmem_cache *dlm_midcomms_cache_create(void)
{
	return kmem_cache_create("dlm_mhandle", sizeof(struct dlm_mhandle),
				 0, 0, NULL);
}

static inline const char *dlm_state_str(int state)
{
	switch (state) {
	case DLM_CLOSED:
		return "CLOSED";
	case DLM_ESTABLISHED:
		return "ESTABLISHED";
	case DLM_FIN_WAIT1:
		return "FIN_WAIT1";
	case DLM_FIN_WAIT2:
		return "FIN_WAIT2";
	case DLM_CLOSE_WAIT:
		return "CLOSE_WAIT";
	case DLM_LAST_ACK:
		return "LAST_ACK";
	case DLM_CLOSING:
		return "CLOSING";
	default:
		return "UNKNOWN";
	}
}

const char *dlm_midcomms_state(struct midcomms_node *node)
{
	return dlm_state_str(node->state);
}

unsigned long dlm_midcomms_flags(struct midcomms_node *node)
{
	return node->flags;
}

int dlm_midcomms_send_queue_cnt(struct midcomms_node *node)
{
	return atomic_read(&node->send_queue_cnt);
}

uint32_t dlm_midcomms_version(struct midcomms_node *node)
{
	return node->version;
}

static struct midcomms_node *__find_node(int nodeid, int r)
{
	struct midcomms_node *node;

	hlist_for_each_entry_rcu(node, &node_hash[r], hlist) {
		if (node->nodeid == nodeid)
			return node;
	}

	return NULL;
}

static void dlm_mhandle_release(struct rcu_head *rcu)
{
	struct dlm_mhandle *mh = container_of(rcu, struct dlm_mhandle, rcu);

	dlm_lowcomms_put_msg(mh->msg);
	dlm_free_mhandle(mh);
}

static void dlm_mhandle_delete(struct midcomms_node *node,
			       struct dlm_mhandle *mh)
{
	list_del_rcu(&mh->list);
	atomic_dec(&node->send_queue_cnt);
	call_rcu(&mh->rcu, dlm_mhandle_release);
}

static void dlm_send_queue_flush(struct midcomms_node *node)
{
	struct dlm_mhandle *mh;

	pr_debug("flush midcomms send queue of node %d\n", node->nodeid);

	rcu_read_lock();
	spin_lock_bh(&node->send_queue_lock);
	list_for_each_entry_rcu(mh, &node->send_queue, list) {
		dlm_mhandle_delete(node, mh);
	}
	spin_unlock_bh(&node->send_queue_lock);
	rcu_read_unlock();
}

static void midcomms_node_reset(struct midcomms_node *node)
{
	pr_debug("reset node %d\n", node->nodeid);

	atomic_set(&node->seq_next, DLM_SEQ_INIT);
	atomic_set(&node->seq_send, DLM_SEQ_INIT);
	atomic_set(&node->ulp_delivered, 0);
	node->version = DLM_VERSION_NOT_SET;
	node->flags = 0;

	dlm_send_queue_flush(node);
	node->state = DLM_CLOSED;
	wake_up(&node->shutdown_wait);
}

static struct midcomms_node *nodeid2node(int nodeid)
{
	return __find_node(nodeid, nodeid_hash(nodeid));
}

int dlm_midcomms_addr(int nodeid, struct sockaddr_storage *addr, int len)
{
	int ret, idx, r = nodeid_hash(nodeid);
	struct midcomms_node *node;

	ret = dlm_lowcomms_addr(nodeid, addr, len);
	if (ret)
		return ret;

	idx = srcu_read_lock(&nodes_srcu);
	node = __find_node(nodeid, r);
	if (node) {
		srcu_read_unlock(&nodes_srcu, idx);
		return 0;
	}
	srcu_read_unlock(&nodes_srcu, idx);

	node = kmalloc(sizeof(*node), GFP_NOFS);
	if (!node)
		return -ENOMEM;

	node->nodeid = nodeid;
	spin_lock_init(&node->state_lock);
	spin_lock_init(&node->send_queue_lock);
	atomic_set(&node->send_queue_cnt, 0);
	INIT_LIST_HEAD(&node->send_queue);
	init_waitqueue_head(&node->shutdown_wait);
	node->users = 0;
	midcomms_node_reset(node);

	spin_lock(&nodes_lock);
	hlist_add_head_rcu(&node->hlist, &node_hash[r]);
	spin_unlock(&nodes_lock);

	node->debugfs = dlm_create_debug_comms_file(nodeid, node);
	return 0;
}

static int dlm_send_ack(int nodeid, uint32_t seq)
{
	int mb_len = sizeof(struct dlm_header);
	struct dlm_header *m_header;
	struct dlm_msg *msg;
	char *ppc;

	msg = dlm_lowcomms_new_msg(nodeid, mb_len, GFP_ATOMIC, &ppc,
				   NULL, NULL);
	if (!msg)
		return -ENOMEM;

	m_header = (struct dlm_header *)ppc;

	m_header->h_version = cpu_to_le32(DLM_HEADER_MAJOR | DLM_HEADER_MINOR);
	m_header->h_nodeid = cpu_to_le32(dlm_our_nodeid());
	m_header->h_length = cpu_to_le16(mb_len);
	m_header->h_cmd = DLM_ACK;
	m_header->u.h_seq = cpu_to_le32(seq);

	dlm_lowcomms_commit_msg(msg);
	dlm_lowcomms_put_msg(msg);

	return 0;
}

static void dlm_send_ack_threshold(struct midcomms_node *node,
				   uint32_t threshold)
{
	uint32_t oval, nval;
	bool send_ack;

	 
	do {
		oval = atomic_read(&node->ulp_delivered);
		send_ack = (oval > threshold);
		 
		if (!send_ack)
			break;

		nval = 0;
		 
	} while (atomic_cmpxchg(&node->ulp_delivered, oval, nval) != oval);

	if (send_ack)
		dlm_send_ack(node->nodeid, atomic_read(&node->seq_next));
}

static int dlm_send_fin(struct midcomms_node *node,
			void (*ack_rcv)(struct midcomms_node *node))
{
	int mb_len = sizeof(struct dlm_header);
	struct dlm_header *m_header;
	struct dlm_mhandle *mh;
	char *ppc;

	mh = dlm_midcomms_get_mhandle(node->nodeid, mb_len, GFP_ATOMIC, &ppc);
	if (!mh)
		return -ENOMEM;

	set_bit(DLM_NODE_FLAG_STOP_TX, &node->flags);
	mh->ack_rcv = ack_rcv;

	m_header = (struct dlm_header *)ppc;

	m_header->h_version = cpu_to_le32(DLM_HEADER_MAJOR | DLM_HEADER_MINOR);
	m_header->h_nodeid = cpu_to_le32(dlm_our_nodeid());
	m_header->h_length = cpu_to_le16(mb_len);
	m_header->h_cmd = DLM_FIN;

	pr_debug("sending fin msg to node %d\n", node->nodeid);
	dlm_midcomms_commit_mhandle(mh, NULL, 0);

	return 0;
}

static void dlm_receive_ack(struct midcomms_node *node, uint32_t seq)
{
	struct dlm_mhandle *mh;

	rcu_read_lock();
	list_for_each_entry_rcu(mh, &node->send_queue, list) {
		if (before(mh->seq, seq)) {
			if (mh->ack_rcv)
				mh->ack_rcv(node);
		} else {
			 
			break;
		}
	}

	spin_lock_bh(&node->send_queue_lock);
	list_for_each_entry_rcu(mh, &node->send_queue, list) {
		if (before(mh->seq, seq)) {
			dlm_mhandle_delete(node, mh);
		} else {
			 
			break;
		}
	}
	spin_unlock_bh(&node->send_queue_lock);
	rcu_read_unlock();
}

static void dlm_pas_fin_ack_rcv(struct midcomms_node *node)
{
	spin_lock(&node->state_lock);
	pr_debug("receive passive fin ack from node %d with state %s\n",
		 node->nodeid, dlm_state_str(node->state));

	switch (node->state) {
	case DLM_LAST_ACK:
		 
		midcomms_node_reset(node);
		break;
	case DLM_CLOSED:
		 
		wake_up(&node->shutdown_wait);
		break;
	default:
		spin_unlock(&node->state_lock);
		log_print("%s: unexpected state: %d",
			  __func__, node->state);
		WARN_ON_ONCE(1);
		return;
	}
	spin_unlock(&node->state_lock);
}

static void dlm_receive_buffer_3_2_trace(uint32_t seq,
					 const union dlm_packet *p)
{
	switch (p->header.h_cmd) {
	case DLM_MSG:
		trace_dlm_recv_message(dlm_our_nodeid(), seq, &p->message);
		break;
	case DLM_RCOM:
		trace_dlm_recv_rcom(dlm_our_nodeid(), seq, &p->rcom);
		break;
	default:
		break;
	}
}

static void dlm_midcomms_receive_buffer(const union dlm_packet *p,
					struct midcomms_node *node,
					uint32_t seq)
{
	bool is_expected_seq;
	uint32_t oval, nval;

	do {
		oval = atomic_read(&node->seq_next);
		is_expected_seq = (oval == seq);
		if (!is_expected_seq)
			break;

		nval = oval + 1;
	} while (atomic_cmpxchg(&node->seq_next, oval, nval) != oval);

	if (is_expected_seq) {
		switch (p->header.h_cmd) {
		case DLM_FIN:
			spin_lock(&node->state_lock);
			pr_debug("receive fin msg from node %d with state %s\n",
				 node->nodeid, dlm_state_str(node->state));

			switch (node->state) {
			case DLM_ESTABLISHED:
				dlm_send_ack(node->nodeid, nval);

				 
				if (node->users == 0) {
					node->state = DLM_LAST_ACK;
					pr_debug("switch node %d to state %s case 1\n",
						 node->nodeid, dlm_state_str(node->state));
					set_bit(DLM_NODE_FLAG_STOP_RX, &node->flags);
					dlm_send_fin(node, dlm_pas_fin_ack_rcv);
				} else {
					node->state = DLM_CLOSE_WAIT;
					pr_debug("switch node %d to state %s\n",
						 node->nodeid, dlm_state_str(node->state));
				}
				break;
			case DLM_FIN_WAIT1:
				dlm_send_ack(node->nodeid, nval);
				node->state = DLM_CLOSING;
				set_bit(DLM_NODE_FLAG_STOP_RX, &node->flags);
				pr_debug("switch node %d to state %s\n",
					 node->nodeid, dlm_state_str(node->state));
				break;
			case DLM_FIN_WAIT2:
				dlm_send_ack(node->nodeid, nval);
				midcomms_node_reset(node);
				pr_debug("switch node %d to state %s\n",
					 node->nodeid, dlm_state_str(node->state));
				break;
			case DLM_LAST_ACK:
				 
				break;
			default:
				spin_unlock(&node->state_lock);
				log_print("%s: unexpected state: %d",
					  __func__, node->state);
				WARN_ON_ONCE(1);
				return;
			}
			spin_unlock(&node->state_lock);
			break;
		default:
			WARN_ON_ONCE(test_bit(DLM_NODE_FLAG_STOP_RX, &node->flags));
			dlm_receive_buffer_3_2_trace(seq, p);
			dlm_receive_buffer(p, node->nodeid);
			atomic_inc(&node->ulp_delivered);
			 
			dlm_send_ack_threshold(node, DLM_RECV_ACK_BACK_MSG_THRESHOLD);
			break;
		}
	} else {
		 
		if (seq < oval)
			dlm_send_ack(node->nodeid, oval);

		log_print_ratelimited("ignore dlm msg because seq mismatch, seq: %u, expected: %u, nodeid: %d",
				      seq, oval, node->nodeid);
	}
}

static int dlm_opts_check_msglen(const union dlm_packet *p, uint16_t msglen,
				 int nodeid)
{
	int len = msglen;

	 
	if (len < sizeof(struct dlm_opts))
		return -1;
	len -= sizeof(struct dlm_opts);

	if (len < le16_to_cpu(p->opts.o_optlen))
		return -1;
	len -= le16_to_cpu(p->opts.o_optlen);

	switch (p->opts.o_nextcmd) {
	case DLM_FIN:
		if (len < sizeof(struct dlm_header)) {
			log_print("fin too small: %d, will skip this message from node %d",
				  len, nodeid);
			return -1;
		}

		break;
	case DLM_MSG:
		if (len < sizeof(struct dlm_message)) {
			log_print("msg too small: %d, will skip this message from node %d",
				  msglen, nodeid);
			return -1;
		}

		break;
	case DLM_RCOM:
		if (len < sizeof(struct dlm_rcom)) {
			log_print("rcom msg too small: %d, will skip this message from node %d",
				  len, nodeid);
			return -1;
		}

		break;
	default:
		log_print("unsupported o_nextcmd received: %u, will skip this message from node %d",
			  p->opts.o_nextcmd, nodeid);
		return -1;
	}

	return 0;
}

static void dlm_midcomms_receive_buffer_3_2(const union dlm_packet *p, int nodeid)
{
	uint16_t msglen = le16_to_cpu(p->header.h_length);
	struct midcomms_node *node;
	uint32_t seq;
	int ret, idx;

	idx = srcu_read_lock(&nodes_srcu);
	node = nodeid2node(nodeid);
	if (WARN_ON_ONCE(!node))
		goto out;

	switch (node->version) {
	case DLM_VERSION_NOT_SET:
		node->version = DLM_VERSION_3_2;
		wake_up(&node->shutdown_wait);
		log_print("version 0x%08x for node %d detected", DLM_VERSION_3_2,
			  node->nodeid);

		spin_lock(&node->state_lock);
		switch (node->state) {
		case DLM_CLOSED:
			node->state = DLM_ESTABLISHED;
			pr_debug("switch node %d to state %s\n",
				 node->nodeid, dlm_state_str(node->state));
			break;
		default:
			break;
		}
		spin_unlock(&node->state_lock);

		break;
	case DLM_VERSION_3_2:
		break;
	default:
		log_print_ratelimited("version mismatch detected, assumed 0x%08x but node %d has 0x%08x",
				      DLM_VERSION_3_2, node->nodeid, node->version);
		goto out;
	}

	switch (p->header.h_cmd) {
	case DLM_RCOM:
		 
		switch (p->rcom.rc_type) {
		case cpu_to_le32(DLM_RCOM_NAMES):
			fallthrough;
		case cpu_to_le32(DLM_RCOM_NAMES_REPLY):
			fallthrough;
		case cpu_to_le32(DLM_RCOM_STATUS):
			fallthrough;
		case cpu_to_le32(DLM_RCOM_STATUS_REPLY):
			break;
		default:
			log_print("unsupported rcom type received: %u, will skip this message from node %d",
				  le32_to_cpu(p->rcom.rc_type), nodeid);
			goto out;
		}

		WARN_ON_ONCE(test_bit(DLM_NODE_FLAG_STOP_RX, &node->flags));
		dlm_receive_buffer(p, nodeid);
		break;
	case DLM_OPTS:
		seq = le32_to_cpu(p->header.u.h_seq);

		ret = dlm_opts_check_msglen(p, msglen, nodeid);
		if (ret < 0) {
			log_print("opts msg too small: %u, will skip this message from node %d",
				  msglen, nodeid);
			goto out;
		}

		p = (union dlm_packet *)((unsigned char *)p->opts.o_opts +
					 le16_to_cpu(p->opts.o_optlen));

		 
		msglen = le16_to_cpu(p->header.h_length);
		switch (p->header.h_cmd) {
		case DLM_RCOM:
			if (msglen < sizeof(struct dlm_rcom)) {
				log_print("inner rcom msg too small: %u, will skip this message from node %d",
					  msglen, nodeid);
				goto out;
			}

			break;
		case DLM_MSG:
			if (msglen < sizeof(struct dlm_message)) {
				log_print("inner msg too small: %u, will skip this message from node %d",
					  msglen, nodeid);
				goto out;
			}

			break;
		case DLM_FIN:
			if (msglen < sizeof(struct dlm_header)) {
				log_print("inner fin too small: %u, will skip this message from node %d",
					  msglen, nodeid);
				goto out;
			}

			break;
		default:
			log_print("unsupported inner h_cmd received: %u, will skip this message from node %d",
				  msglen, nodeid);
			goto out;
		}

		dlm_midcomms_receive_buffer(p, node, seq);
		break;
	case DLM_ACK:
		seq = le32_to_cpu(p->header.u.h_seq);
		dlm_receive_ack(node, seq);
		break;
	default:
		log_print("unsupported h_cmd received: %u, will skip this message from node %d",
			  p->header.h_cmd, nodeid);
		break;
	}

out:
	srcu_read_unlock(&nodes_srcu, idx);
}

static void dlm_midcomms_receive_buffer_3_1(const union dlm_packet *p, int nodeid)
{
	uint16_t msglen = le16_to_cpu(p->header.h_length);
	struct midcomms_node *node;
	int idx;

	idx = srcu_read_lock(&nodes_srcu);
	node = nodeid2node(nodeid);
	if (WARN_ON_ONCE(!node)) {
		srcu_read_unlock(&nodes_srcu, idx);
		return;
	}

	switch (node->version) {
	case DLM_VERSION_NOT_SET:
		node->version = DLM_VERSION_3_1;
		wake_up(&node->shutdown_wait);
		log_print("version 0x%08x for node %d detected", DLM_VERSION_3_1,
			  node->nodeid);
		break;
	case DLM_VERSION_3_1:
		break;
	default:
		log_print_ratelimited("version mismatch detected, assumed 0x%08x but node %d has 0x%08x",
				      DLM_VERSION_3_1, node->nodeid, node->version);
		srcu_read_unlock(&nodes_srcu, idx);
		return;
	}
	srcu_read_unlock(&nodes_srcu, idx);

	switch (p->header.h_cmd) {
	case DLM_RCOM:
		 
		break;
	case DLM_MSG:
		if (msglen < sizeof(struct dlm_message)) {
			log_print("msg too small: %u, will skip this message from node %d",
				  msglen, nodeid);
			return;
		}

		break;
	default:
		log_print("unsupported h_cmd received: %u, will skip this message from node %d",
			  p->header.h_cmd, nodeid);
		return;
	}

	dlm_receive_buffer(p, nodeid);
}

int dlm_validate_incoming_buffer(int nodeid, unsigned char *buf, int len)
{
	const unsigned char *ptr = buf;
	const struct dlm_header *hd;
	uint16_t msglen;
	int ret = 0;

	while (len >= sizeof(struct dlm_header)) {
		hd = (struct dlm_header *)ptr;

		 
		msglen = le16_to_cpu(hd->h_length);
		if (msglen > DLM_MAX_SOCKET_BUFSIZE ||
		    msglen < sizeof(struct dlm_header)) {
			log_print("received invalid length header: %u from node %d, will abort message parsing",
				  msglen, nodeid);
			return -EBADMSG;
		}

		 
		if (msglen > len)
			break;

		ret += msglen;
		len -= msglen;
		ptr += msglen;
	}

	return ret;
}

 
int dlm_process_incoming_buffer(int nodeid, unsigned char *buf, int len)
{
	const unsigned char *ptr = buf;
	const struct dlm_header *hd;
	uint16_t msglen;
	int ret = 0;

	while (len >= sizeof(struct dlm_header)) {
		hd = (struct dlm_header *)ptr;

		msglen = le16_to_cpu(hd->h_length);
		if (msglen > len)
			break;

		switch (hd->h_version) {
		case cpu_to_le32(DLM_VERSION_3_1):
			dlm_midcomms_receive_buffer_3_1((const union dlm_packet *)ptr, nodeid);
			break;
		case cpu_to_le32(DLM_VERSION_3_2):
			dlm_midcomms_receive_buffer_3_2((const union dlm_packet *)ptr, nodeid);
			break;
		default:
			log_print("received invalid version header: %u from node %d, will skip this message",
				  le32_to_cpu(hd->h_version), nodeid);
			break;
		}

		ret += msglen;
		len -= msglen;
		ptr += msglen;
	}

	return ret;
}

void dlm_midcomms_unack_msg_resend(int nodeid)
{
	struct midcomms_node *node;
	struct dlm_mhandle *mh;
	int idx, ret;

	idx = srcu_read_lock(&nodes_srcu);
	node = nodeid2node(nodeid);
	if (WARN_ON_ONCE(!node)) {
		srcu_read_unlock(&nodes_srcu, idx);
		return;
	}

	 
	switch (node->version) {
	case DLM_VERSION_3_2:
		break;
	default:
		srcu_read_unlock(&nodes_srcu, idx);
		return;
	}

	rcu_read_lock();
	list_for_each_entry_rcu(mh, &node->send_queue, list) {
		if (!mh->committed)
			continue;

		ret = dlm_lowcomms_resend_msg(mh->msg);
		if (!ret)
			log_print_ratelimited("retransmit dlm msg, seq %u, nodeid %d",
					      mh->seq, node->nodeid);
	}
	rcu_read_unlock();
	srcu_read_unlock(&nodes_srcu, idx);
}

static void dlm_fill_opts_header(struct dlm_opts *opts, uint16_t inner_len,
				 uint32_t seq)
{
	opts->o_header.h_cmd = DLM_OPTS;
	opts->o_header.h_version = cpu_to_le32(DLM_HEADER_MAJOR | DLM_HEADER_MINOR);
	opts->o_header.h_nodeid = cpu_to_le32(dlm_our_nodeid());
	opts->o_header.h_length = cpu_to_le16(DLM_MIDCOMMS_OPT_LEN + inner_len);
	opts->o_header.u.h_seq = cpu_to_le32(seq);
}

static void midcomms_new_msg_cb(void *data)
{
	struct dlm_mhandle *mh = data;

	atomic_inc(&mh->node->send_queue_cnt);

	spin_lock_bh(&mh->node->send_queue_lock);
	list_add_tail_rcu(&mh->list, &mh->node->send_queue);
	spin_unlock_bh(&mh->node->send_queue_lock);

	mh->seq = atomic_fetch_inc(&mh->node->seq_send);
}

static struct dlm_msg *dlm_midcomms_get_msg_3_2(struct dlm_mhandle *mh, int nodeid,
						int len, gfp_t allocation, char **ppc)
{
	struct dlm_opts *opts;
	struct dlm_msg *msg;

	msg = dlm_lowcomms_new_msg(nodeid, len + DLM_MIDCOMMS_OPT_LEN,
				   allocation, ppc, midcomms_new_msg_cb, mh);
	if (!msg)
		return NULL;

	opts = (struct dlm_opts *)*ppc;
	mh->opts = opts;

	 
	dlm_fill_opts_header(opts, len, mh->seq);

	*ppc += sizeof(*opts);
	mh->inner_p = (const union dlm_packet *)*ppc;
	return msg;
}

 
#ifndef __CHECKER__
struct dlm_mhandle *dlm_midcomms_get_mhandle(int nodeid, int len,
					     gfp_t allocation, char **ppc)
{
	struct midcomms_node *node;
	struct dlm_mhandle *mh;
	struct dlm_msg *msg;
	int idx;

	idx = srcu_read_lock(&nodes_srcu);
	node = nodeid2node(nodeid);
	if (WARN_ON_ONCE(!node))
		goto err;

	 
	WARN_ON_ONCE(test_bit(DLM_NODE_FLAG_STOP_TX, &node->flags));

	mh = dlm_allocate_mhandle(allocation);
	if (!mh)
		goto err;

	mh->committed = false;
	mh->ack_rcv = NULL;
	mh->idx = idx;
	mh->node = node;

	switch (node->version) {
	case DLM_VERSION_3_1:
		msg = dlm_lowcomms_new_msg(nodeid, len, allocation, ppc,
					   NULL, NULL);
		if (!msg) {
			dlm_free_mhandle(mh);
			goto err;
		}

		break;
	case DLM_VERSION_3_2:
		 
		dlm_send_ack_threshold(node, DLM_SEND_ACK_BACK_MSG_THRESHOLD);

		msg = dlm_midcomms_get_msg_3_2(mh, nodeid, len, allocation,
					       ppc);
		if (!msg) {
			dlm_free_mhandle(mh);
			goto err;
		}
		break;
	default:
		dlm_free_mhandle(mh);
		WARN_ON_ONCE(1);
		goto err;
	}

	mh->msg = msg;

	 
	return mh;

err:
	srcu_read_unlock(&nodes_srcu, idx);
	return NULL;
}
#endif

static void dlm_midcomms_commit_msg_3_2_trace(const struct dlm_mhandle *mh,
					      const void *name, int namelen)
{
	switch (mh->inner_p->header.h_cmd) {
	case DLM_MSG:
		trace_dlm_send_message(mh->node->nodeid, mh->seq,
				       &mh->inner_p->message,
				       name, namelen);
		break;
	case DLM_RCOM:
		trace_dlm_send_rcom(mh->node->nodeid, mh->seq,
				    &mh->inner_p->rcom);
		break;
	default:
		 
		break;
	}
}

static void dlm_midcomms_commit_msg_3_2(struct dlm_mhandle *mh,
					const void *name, int namelen)
{
	 
	mh->opts->o_nextcmd = mh->inner_p->header.h_cmd;
	mh->committed = true;
	dlm_midcomms_commit_msg_3_2_trace(mh, name, namelen);
	dlm_lowcomms_commit_msg(mh->msg);
}

 
#ifndef __CHECKER__
void dlm_midcomms_commit_mhandle(struct dlm_mhandle *mh,
				 const void *name, int namelen)
{

	switch (mh->node->version) {
	case DLM_VERSION_3_1:
		srcu_read_unlock(&nodes_srcu, mh->idx);

		dlm_lowcomms_commit_msg(mh->msg);
		dlm_lowcomms_put_msg(mh->msg);
		 
		dlm_free_mhandle(mh);
		break;
	case DLM_VERSION_3_2:
		 
		rcu_read_lock();
		dlm_midcomms_commit_msg_3_2(mh, name, namelen);
		srcu_read_unlock(&nodes_srcu, mh->idx);
		rcu_read_unlock();
		break;
	default:
		srcu_read_unlock(&nodes_srcu, mh->idx);
		WARN_ON_ONCE(1);
		break;
	}
}
#endif

int dlm_midcomms_start(void)
{
	return dlm_lowcomms_start();
}

void dlm_midcomms_stop(void)
{
	dlm_lowcomms_stop();
}

void dlm_midcomms_init(void)
{
	int i;

	for (i = 0; i < CONN_HASH_SIZE; i++)
		INIT_HLIST_HEAD(&node_hash[i]);

	dlm_lowcomms_init();
}

static void midcomms_node_release(struct rcu_head *rcu)
{
	struct midcomms_node *node = container_of(rcu, struct midcomms_node, rcu);

	WARN_ON_ONCE(atomic_read(&node->send_queue_cnt));
	dlm_send_queue_flush(node);
	kfree(node);
}

void dlm_midcomms_exit(void)
{
	struct midcomms_node *node;
	int i, idx;

	idx = srcu_read_lock(&nodes_srcu);
	for (i = 0; i < CONN_HASH_SIZE; i++) {
		hlist_for_each_entry_rcu(node, &node_hash[i], hlist) {
			dlm_delete_debug_comms_file(node->debugfs);

			spin_lock(&nodes_lock);
			hlist_del_rcu(&node->hlist);
			spin_unlock(&nodes_lock);

			call_srcu(&nodes_srcu, &node->rcu, midcomms_node_release);
		}
	}
	srcu_read_unlock(&nodes_srcu, idx);

	dlm_lowcomms_exit();
}

static void dlm_act_fin_ack_rcv(struct midcomms_node *node)
{
	spin_lock(&node->state_lock);
	pr_debug("receive active fin ack from node %d with state %s\n",
		 node->nodeid, dlm_state_str(node->state));

	switch (node->state) {
	case DLM_FIN_WAIT1:
		node->state = DLM_FIN_WAIT2;
		pr_debug("switch node %d to state %s\n",
			 node->nodeid, dlm_state_str(node->state));
		break;
	case DLM_CLOSING:
		midcomms_node_reset(node);
		pr_debug("switch node %d to state %s\n",
			 node->nodeid, dlm_state_str(node->state));
		break;
	case DLM_CLOSED:
		 
		wake_up(&node->shutdown_wait);
		break;
	default:
		spin_unlock(&node->state_lock);
		log_print("%s: unexpected state: %d",
			  __func__, node->state);
		WARN_ON_ONCE(1);
		return;
	}
	spin_unlock(&node->state_lock);
}

void dlm_midcomms_add_member(int nodeid)
{
	struct midcomms_node *node;
	int idx;

	idx = srcu_read_lock(&nodes_srcu);
	node = nodeid2node(nodeid);
	if (WARN_ON_ONCE(!node)) {
		srcu_read_unlock(&nodes_srcu, idx);
		return;
	}

	spin_lock(&node->state_lock);
	if (!node->users) {
		pr_debug("receive add member from node %d with state %s\n",
			 node->nodeid, dlm_state_str(node->state));
		switch (node->state) {
		case DLM_ESTABLISHED:
			break;
		case DLM_CLOSED:
			node->state = DLM_ESTABLISHED;
			pr_debug("switch node %d to state %s\n",
				 node->nodeid, dlm_state_str(node->state));
			break;
		default:
			 
			log_print("reset node %d because shutdown stuck",
				  node->nodeid);

			midcomms_node_reset(node);
			node->state = DLM_ESTABLISHED;
			break;
		}
	}

	node->users++;
	pr_debug("node %d users inc count %d\n", nodeid, node->users);
	spin_unlock(&node->state_lock);

	srcu_read_unlock(&nodes_srcu, idx);
}

void dlm_midcomms_remove_member(int nodeid)
{
	struct midcomms_node *node;
	int idx;

	idx = srcu_read_lock(&nodes_srcu);
	node = nodeid2node(nodeid);
	 
	if (!node) {
		srcu_read_unlock(&nodes_srcu, idx);
		return;
	}

	spin_lock(&node->state_lock);
	 
	if (!node->users) {
		spin_unlock(&node->state_lock);
		srcu_read_unlock(&nodes_srcu, idx);
		return;
	}

	node->users--;
	pr_debug("node %d users dec count %d\n", nodeid, node->users);

	 
	if (node->users == 0) {
		pr_debug("receive remove member from node %d with state %s\n",
			 node->nodeid, dlm_state_str(node->state));
		switch (node->state) {
		case DLM_ESTABLISHED:
			break;
		case DLM_CLOSE_WAIT:
			 
			node->state = DLM_LAST_ACK;
			pr_debug("switch node %d to state %s case 2\n",
				 node->nodeid, dlm_state_str(node->state));
			set_bit(DLM_NODE_FLAG_STOP_RX, &node->flags);
			dlm_send_fin(node, dlm_pas_fin_ack_rcv);
			break;
		case DLM_LAST_ACK:
			 
			break;
		case DLM_CLOSED:
			 
			break;
		default:
			log_print("%s: unexpected state: %d",
				  __func__, node->state);
			break;
		}
	}
	spin_unlock(&node->state_lock);

	srcu_read_unlock(&nodes_srcu, idx);
}

void dlm_midcomms_version_wait(void)
{
	struct midcomms_node *node;
	int i, idx, ret;

	idx = srcu_read_lock(&nodes_srcu);
	for (i = 0; i < CONN_HASH_SIZE; i++) {
		hlist_for_each_entry_rcu(node, &node_hash[i], hlist) {
			ret = wait_event_timeout(node->shutdown_wait,
						 node->version != DLM_VERSION_NOT_SET ||
						 node->state == DLM_CLOSED ||
						 test_bit(DLM_NODE_FLAG_CLOSE, &node->flags),
						 DLM_SHUTDOWN_TIMEOUT);
			if (!ret || test_bit(DLM_NODE_FLAG_CLOSE, &node->flags))
				pr_debug("version wait timed out for node %d with state %s\n",
					 node->nodeid, dlm_state_str(node->state));
		}
	}
	srcu_read_unlock(&nodes_srcu, idx);
}

static void midcomms_shutdown(struct midcomms_node *node)
{
	int ret;

	 
	switch (node->version) {
	case DLM_VERSION_3_2:
		break;
	default:
		return;
	}

	spin_lock(&node->state_lock);
	pr_debug("receive active shutdown for node %d with state %s\n",
		 node->nodeid, dlm_state_str(node->state));
	switch (node->state) {
	case DLM_ESTABLISHED:
		node->state = DLM_FIN_WAIT1;
		pr_debug("switch node %d to state %s case 2\n",
			 node->nodeid, dlm_state_str(node->state));
		dlm_send_fin(node, dlm_act_fin_ack_rcv);
		break;
	case DLM_CLOSED:
		 
		break;
	default:
		 
		break;
	}
	spin_unlock(&node->state_lock);

	if (DLM_DEBUG_FENCE_TERMINATION)
		msleep(5000);

	 
	ret = wait_event_timeout(node->shutdown_wait,
				 node->state == DLM_CLOSED ||
				 test_bit(DLM_NODE_FLAG_CLOSE, &node->flags),
				 DLM_SHUTDOWN_TIMEOUT);
	if (!ret)
		pr_debug("active shutdown timed out for node %d with state %s\n",
			 node->nodeid, dlm_state_str(node->state));
	else
		pr_debug("active shutdown done for node %d with state %s\n",
			 node->nodeid, dlm_state_str(node->state));
}

void dlm_midcomms_shutdown(void)
{
	struct midcomms_node *node;
	int i, idx;

	mutex_lock(&close_lock);
	idx = srcu_read_lock(&nodes_srcu);
	for (i = 0; i < CONN_HASH_SIZE; i++) {
		hlist_for_each_entry_rcu(node, &node_hash[i], hlist) {
			midcomms_shutdown(node);
		}
	}

	dlm_lowcomms_shutdown();

	for (i = 0; i < CONN_HASH_SIZE; i++) {
		hlist_for_each_entry_rcu(node, &node_hash[i], hlist) {
			midcomms_node_reset(node);
		}
	}
	srcu_read_unlock(&nodes_srcu, idx);
	mutex_unlock(&close_lock);
}

int dlm_midcomms_close(int nodeid)
{
	struct midcomms_node *node;
	int idx, ret;

	idx = srcu_read_lock(&nodes_srcu);
	 
	node = nodeid2node(nodeid);
	if (node) {
		 
		set_bit(DLM_NODE_FLAG_CLOSE, &node->flags);
		wake_up(&node->shutdown_wait);
	}
	srcu_read_unlock(&nodes_srcu, idx);

	synchronize_srcu(&nodes_srcu);

	mutex_lock(&close_lock);
	idx = srcu_read_lock(&nodes_srcu);
	node = nodeid2node(nodeid);
	if (!node) {
		srcu_read_unlock(&nodes_srcu, idx);
		mutex_unlock(&close_lock);
		return dlm_lowcomms_close(nodeid);
	}

	ret = dlm_lowcomms_close(nodeid);
	dlm_delete_debug_comms_file(node->debugfs);

	spin_lock(&nodes_lock);
	hlist_del_rcu(&node->hlist);
	spin_unlock(&nodes_lock);
	srcu_read_unlock(&nodes_srcu, idx);

	 
	synchronize_srcu(&nodes_srcu);

	 
	dlm_send_queue_flush(node);

	call_srcu(&nodes_srcu, &node->rcu, midcomms_node_release);
	mutex_unlock(&close_lock);

	return ret;
}

 
struct dlm_rawmsg_data {
	struct midcomms_node *node;
	void *buf;
};

static void midcomms_new_rawmsg_cb(void *data)
{
	struct dlm_rawmsg_data *rd = data;
	struct dlm_header *h = rd->buf;

	switch (h->h_version) {
	case cpu_to_le32(DLM_VERSION_3_1):
		break;
	default:
		switch (h->h_cmd) {
		case DLM_OPTS:
			if (!h->u.h_seq)
				h->u.h_seq = cpu_to_le32(atomic_fetch_inc(&rd->node->seq_send));
			break;
		default:
			break;
		}
		break;
	}
}

int dlm_midcomms_rawmsg_send(struct midcomms_node *node, void *buf,
			     int buflen)
{
	struct dlm_rawmsg_data rd;
	struct dlm_msg *msg;
	char *msgbuf;

	rd.node = node;
	rd.buf = buf;

	msg = dlm_lowcomms_new_msg(node->nodeid, buflen, GFP_NOFS,
				   &msgbuf, midcomms_new_rawmsg_cb, &rd);
	if (!msg)
		return -ENOMEM;

	memcpy(msgbuf, buf, buflen);
	dlm_lowcomms_commit_msg(msg);
	return 0;
}

