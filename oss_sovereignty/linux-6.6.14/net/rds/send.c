 
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/gfp.h>
#include <net/sock.h>
#include <linux/in.h>
#include <linux/list.h>
#include <linux/ratelimit.h>
#include <linux/export.h>
#include <linux/sizes.h>

#include "rds.h"

 
static int send_batch_count = SZ_1K;
module_param(send_batch_count, int, 0444);
MODULE_PARM_DESC(send_batch_count, " batch factor when working the send queue");

static void rds_send_remove_from_sock(struct list_head *messages, int status);

 
void rds_send_path_reset(struct rds_conn_path *cp)
{
	struct rds_message *rm, *tmp;
	unsigned long flags;

	if (cp->cp_xmit_rm) {
		rm = cp->cp_xmit_rm;
		cp->cp_xmit_rm = NULL;
		 
		rds_message_unmapped(rm);
		rds_message_put(rm);
	}

	cp->cp_xmit_sg = 0;
	cp->cp_xmit_hdr_off = 0;
	cp->cp_xmit_data_off = 0;
	cp->cp_xmit_atomic_sent = 0;
	cp->cp_xmit_rdma_sent = 0;
	cp->cp_xmit_data_sent = 0;

	cp->cp_conn->c_map_queued = 0;

	cp->cp_unacked_packets = rds_sysctl_max_unacked_packets;
	cp->cp_unacked_bytes = rds_sysctl_max_unacked_bytes;

	 
	spin_lock_irqsave(&cp->cp_lock, flags);
	list_for_each_entry_safe(rm, tmp, &cp->cp_retrans, m_conn_item) {
		set_bit(RDS_MSG_ACK_REQUIRED, &rm->m_flags);
		set_bit(RDS_MSG_RETRANSMITTED, &rm->m_flags);
	}
	list_splice_init(&cp->cp_retrans, &cp->cp_send_queue);
	spin_unlock_irqrestore(&cp->cp_lock, flags);
}
EXPORT_SYMBOL_GPL(rds_send_path_reset);

static int acquire_in_xmit(struct rds_conn_path *cp)
{
	return test_and_set_bit(RDS_IN_XMIT, &cp->cp_flags) == 0;
}

static void release_in_xmit(struct rds_conn_path *cp)
{
	clear_bit(RDS_IN_XMIT, &cp->cp_flags);
	smp_mb__after_atomic();
	 
	if (waitqueue_active(&cp->cp_waitq))
		wake_up_all(&cp->cp_waitq);
}

 
int rds_send_xmit(struct rds_conn_path *cp)
{
	struct rds_connection *conn = cp->cp_conn;
	struct rds_message *rm;
	unsigned long flags;
	unsigned int tmp;
	struct scatterlist *sg;
	int ret = 0;
	LIST_HEAD(to_be_dropped);
	int batch_count;
	unsigned long send_gen = 0;
	int same_rm = 0;

restart:
	batch_count = 0;

	 
	if (!acquire_in_xmit(cp)) {
		rds_stats_inc(s_send_lock_contention);
		ret = -ENOMEM;
		goto out;
	}

	if (rds_destroy_pending(cp->cp_conn)) {
		release_in_xmit(cp);
		ret = -ENETUNREACH;  
		goto out;
	}

	 
	send_gen = READ_ONCE(cp->cp_send_gen) + 1;
	WRITE_ONCE(cp->cp_send_gen, send_gen);

	 
	if (!rds_conn_path_up(cp)) {
		release_in_xmit(cp);
		ret = 0;
		goto out;
	}

	if (conn->c_trans->xmit_path_prepare)
		conn->c_trans->xmit_path_prepare(cp);

	 
	while (1) {

		rm = cp->cp_xmit_rm;

		if (!rm) {
			same_rm = 0;
		} else {
			same_rm++;
			if (same_rm >= 4096) {
				rds_stats_inc(s_send_stuck_rm);
				ret = -EAGAIN;
				break;
			}
		}

		 
		if (!rm && test_and_clear_bit(0, &conn->c_map_queued)) {
			rm = rds_cong_update_alloc(conn);
			if (IS_ERR(rm)) {
				ret = PTR_ERR(rm);
				break;
			}
			rm->data.op_active = 1;
			rm->m_inc.i_conn_path = cp;
			rm->m_inc.i_conn = cp->cp_conn;

			cp->cp_xmit_rm = rm;
		}

		 
		if (!rm) {
			unsigned int len;

			batch_count++;

			 
			if (batch_count >= send_batch_count)
				goto over_batch;

			spin_lock_irqsave(&cp->cp_lock, flags);

			if (!list_empty(&cp->cp_send_queue)) {
				rm = list_entry(cp->cp_send_queue.next,
						struct rds_message,
						m_conn_item);
				rds_message_addref(rm);

				 
				list_move_tail(&rm->m_conn_item,
					       &cp->cp_retrans);
			}

			spin_unlock_irqrestore(&cp->cp_lock, flags);

			if (!rm)
				break;

			 
			if (test_bit(RDS_MSG_FLUSH, &rm->m_flags) ||
			    (rm->rdma.op_active &&
			    test_bit(RDS_MSG_RETRANSMITTED, &rm->m_flags))) {
				spin_lock_irqsave(&cp->cp_lock, flags);
				if (test_and_clear_bit(RDS_MSG_ON_CONN, &rm->m_flags))
					list_move(&rm->m_conn_item, &to_be_dropped);
				spin_unlock_irqrestore(&cp->cp_lock, flags);
				continue;
			}

			 
			len = ntohl(rm->m_inc.i_hdr.h_len);
			if (cp->cp_unacked_packets == 0 ||
			    cp->cp_unacked_bytes < len) {
				set_bit(RDS_MSG_ACK_REQUIRED, &rm->m_flags);

				cp->cp_unacked_packets =
					rds_sysctl_max_unacked_packets;
				cp->cp_unacked_bytes =
					rds_sysctl_max_unacked_bytes;
				rds_stats_inc(s_send_ack_required);
			} else {
				cp->cp_unacked_bytes -= len;
				cp->cp_unacked_packets--;
			}

			cp->cp_xmit_rm = rm;
		}

		 
		if (rm->rdma.op_active && !cp->cp_xmit_rdma_sent) {
			rm->m_final_op = &rm->rdma;
			 
			set_bit(RDS_MSG_MAPPED, &rm->m_flags);
			ret = conn->c_trans->xmit_rdma(conn, &rm->rdma);
			if (ret) {
				clear_bit(RDS_MSG_MAPPED, &rm->m_flags);
				wake_up_interruptible(&rm->m_flush_wait);
				break;
			}
			cp->cp_xmit_rdma_sent = 1;

		}

		if (rm->atomic.op_active && !cp->cp_xmit_atomic_sent) {
			rm->m_final_op = &rm->atomic;
			 
			set_bit(RDS_MSG_MAPPED, &rm->m_flags);
			ret = conn->c_trans->xmit_atomic(conn, &rm->atomic);
			if (ret) {
				clear_bit(RDS_MSG_MAPPED, &rm->m_flags);
				wake_up_interruptible(&rm->m_flush_wait);
				break;
			}
			cp->cp_xmit_atomic_sent = 1;

		}

		 
		if (rm->data.op_nents == 0) {
			int ops_present;
			int all_ops_are_silent = 1;

			ops_present = (rm->atomic.op_active || rm->rdma.op_active);
			if (rm->atomic.op_active && !rm->atomic.op_silent)
				all_ops_are_silent = 0;
			if (rm->rdma.op_active && !rm->rdma.op_silent)
				all_ops_are_silent = 0;

			if (ops_present && all_ops_are_silent
			    && !rm->m_rdma_cookie)
				rm->data.op_active = 0;
		}

		if (rm->data.op_active && !cp->cp_xmit_data_sent) {
			rm->m_final_op = &rm->data;

			ret = conn->c_trans->xmit(conn, rm,
						  cp->cp_xmit_hdr_off,
						  cp->cp_xmit_sg,
						  cp->cp_xmit_data_off);
			if (ret <= 0)
				break;

			if (cp->cp_xmit_hdr_off < sizeof(struct rds_header)) {
				tmp = min_t(int, ret,
					    sizeof(struct rds_header) -
					    cp->cp_xmit_hdr_off);
				cp->cp_xmit_hdr_off += tmp;
				ret -= tmp;
			}

			sg = &rm->data.op_sg[cp->cp_xmit_sg];
			while (ret) {
				tmp = min_t(int, ret, sg->length -
						      cp->cp_xmit_data_off);
				cp->cp_xmit_data_off += tmp;
				ret -= tmp;
				if (cp->cp_xmit_data_off == sg->length) {
					cp->cp_xmit_data_off = 0;
					sg++;
					cp->cp_xmit_sg++;
					BUG_ON(ret != 0 && cp->cp_xmit_sg ==
					       rm->data.op_nents);
				}
			}

			if (cp->cp_xmit_hdr_off == sizeof(struct rds_header) &&
			    (cp->cp_xmit_sg == rm->data.op_nents))
				cp->cp_xmit_data_sent = 1;
		}

		 
		if (!rm->data.op_active || cp->cp_xmit_data_sent) {
			cp->cp_xmit_rm = NULL;
			cp->cp_xmit_sg = 0;
			cp->cp_xmit_hdr_off = 0;
			cp->cp_xmit_data_off = 0;
			cp->cp_xmit_rdma_sent = 0;
			cp->cp_xmit_atomic_sent = 0;
			cp->cp_xmit_data_sent = 0;

			rds_message_put(rm);
		}
	}

over_batch:
	if (conn->c_trans->xmit_path_complete)
		conn->c_trans->xmit_path_complete(cp);
	release_in_xmit(cp);

	 
	if (!list_empty(&to_be_dropped)) {
		 
		list_for_each_entry(rm, &to_be_dropped, m_conn_item)
			rds_message_put(rm);
		rds_send_remove_from_sock(&to_be_dropped, RDS_RDMA_DROPPED);
	}

	 
	if (ret == 0) {
		bool raced;

		smp_mb();
		raced = send_gen != READ_ONCE(cp->cp_send_gen);

		if ((test_bit(0, &conn->c_map_queued) ||
		    !list_empty(&cp->cp_send_queue)) && !raced) {
			if (batch_count < send_batch_count)
				goto restart;
			rcu_read_lock();
			if (rds_destroy_pending(cp->cp_conn))
				ret = -ENETUNREACH;
			else
				queue_delayed_work(rds_wq, &cp->cp_send_w, 1);
			rcu_read_unlock();
		} else if (raced) {
			rds_stats_inc(s_send_lock_queue_raced);
		}
	}
out:
	return ret;
}
EXPORT_SYMBOL_GPL(rds_send_xmit);

static void rds_send_sndbuf_remove(struct rds_sock *rs, struct rds_message *rm)
{
	u32 len = be32_to_cpu(rm->m_inc.i_hdr.h_len);

	assert_spin_locked(&rs->rs_lock);

	BUG_ON(rs->rs_snd_bytes < len);
	rs->rs_snd_bytes -= len;

	if (rs->rs_snd_bytes == 0)
		rds_stats_inc(s_send_queue_empty);
}

static inline int rds_send_is_acked(struct rds_message *rm, u64 ack,
				    is_acked_func is_acked)
{
	if (is_acked)
		return is_acked(rm, ack);
	return be64_to_cpu(rm->m_inc.i_hdr.h_sequence) <= ack;
}

 
void rds_rdma_send_complete(struct rds_message *rm, int status)
{
	struct rds_sock *rs = NULL;
	struct rm_rdma_op *ro;
	struct rds_notifier *notifier;
	unsigned long flags;

	spin_lock_irqsave(&rm->m_rs_lock, flags);

	ro = &rm->rdma;
	if (test_bit(RDS_MSG_ON_SOCK, &rm->m_flags) &&
	    ro->op_active && ro->op_notify && ro->op_notifier) {
		notifier = ro->op_notifier;
		rs = rm->m_rs;
		sock_hold(rds_rs_to_sk(rs));

		notifier->n_status = status;
		spin_lock(&rs->rs_lock);
		list_add_tail(&notifier->n_list, &rs->rs_notify_queue);
		spin_unlock(&rs->rs_lock);

		ro->op_notifier = NULL;
	}

	spin_unlock_irqrestore(&rm->m_rs_lock, flags);

	if (rs) {
		rds_wake_sk_sleep(rs);
		sock_put(rds_rs_to_sk(rs));
	}
}
EXPORT_SYMBOL_GPL(rds_rdma_send_complete);

 
void rds_atomic_send_complete(struct rds_message *rm, int status)
{
	struct rds_sock *rs = NULL;
	struct rm_atomic_op *ao;
	struct rds_notifier *notifier;
	unsigned long flags;

	spin_lock_irqsave(&rm->m_rs_lock, flags);

	ao = &rm->atomic;
	if (test_bit(RDS_MSG_ON_SOCK, &rm->m_flags)
	    && ao->op_active && ao->op_notify && ao->op_notifier) {
		notifier = ao->op_notifier;
		rs = rm->m_rs;
		sock_hold(rds_rs_to_sk(rs));

		notifier->n_status = status;
		spin_lock(&rs->rs_lock);
		list_add_tail(&notifier->n_list, &rs->rs_notify_queue);
		spin_unlock(&rs->rs_lock);

		ao->op_notifier = NULL;
	}

	spin_unlock_irqrestore(&rm->m_rs_lock, flags);

	if (rs) {
		rds_wake_sk_sleep(rs);
		sock_put(rds_rs_to_sk(rs));
	}
}
EXPORT_SYMBOL_GPL(rds_atomic_send_complete);

 
static inline void
__rds_send_complete(struct rds_sock *rs, struct rds_message *rm, int status)
{
	struct rm_rdma_op *ro;
	struct rm_atomic_op *ao;

	ro = &rm->rdma;
	if (ro->op_active && ro->op_notify && ro->op_notifier) {
		ro->op_notifier->n_status = status;
		list_add_tail(&ro->op_notifier->n_list, &rs->rs_notify_queue);
		ro->op_notifier = NULL;
	}

	ao = &rm->atomic;
	if (ao->op_active && ao->op_notify && ao->op_notifier) {
		ao->op_notifier->n_status = status;
		list_add_tail(&ao->op_notifier->n_list, &rs->rs_notify_queue);
		ao->op_notifier = NULL;
	}

	 
}

 
static void rds_send_remove_from_sock(struct list_head *messages, int status)
{
	unsigned long flags;
	struct rds_sock *rs = NULL;
	struct rds_message *rm;

	while (!list_empty(messages)) {
		int was_on_sock = 0;

		rm = list_entry(messages->next, struct rds_message,
				m_conn_item);
		list_del_init(&rm->m_conn_item);

		 
		spin_lock_irqsave(&rm->m_rs_lock, flags);
		if (!test_bit(RDS_MSG_ON_SOCK, &rm->m_flags))
			goto unlock_and_drop;

		if (rs != rm->m_rs) {
			if (rs) {
				rds_wake_sk_sleep(rs);
				sock_put(rds_rs_to_sk(rs));
			}
			rs = rm->m_rs;
			if (rs)
				sock_hold(rds_rs_to_sk(rs));
		}
		if (!rs)
			goto unlock_and_drop;
		spin_lock(&rs->rs_lock);

		if (test_and_clear_bit(RDS_MSG_ON_SOCK, &rm->m_flags)) {
			struct rm_rdma_op *ro = &rm->rdma;
			struct rds_notifier *notifier;

			list_del_init(&rm->m_sock_item);
			rds_send_sndbuf_remove(rs, rm);

			if (ro->op_active && ro->op_notifier &&
			       (ro->op_notify || (ro->op_recverr && status))) {
				notifier = ro->op_notifier;
				list_add_tail(&notifier->n_list,
						&rs->rs_notify_queue);
				if (!notifier->n_status)
					notifier->n_status = status;
				rm->rdma.op_notifier = NULL;
			}
			was_on_sock = 1;
		}
		spin_unlock(&rs->rs_lock);

unlock_and_drop:
		spin_unlock_irqrestore(&rm->m_rs_lock, flags);
		rds_message_put(rm);
		if (was_on_sock)
			rds_message_put(rm);
	}

	if (rs) {
		rds_wake_sk_sleep(rs);
		sock_put(rds_rs_to_sk(rs));
	}
}

 
void rds_send_path_drop_acked(struct rds_conn_path *cp, u64 ack,
			      is_acked_func is_acked)
{
	struct rds_message *rm, *tmp;
	unsigned long flags;
	LIST_HEAD(list);

	spin_lock_irqsave(&cp->cp_lock, flags);

	list_for_each_entry_safe(rm, tmp, &cp->cp_retrans, m_conn_item) {
		if (!rds_send_is_acked(rm, ack, is_acked))
			break;

		list_move(&rm->m_conn_item, &list);
		clear_bit(RDS_MSG_ON_CONN, &rm->m_flags);
	}

	 
	if (!list_empty(&list))
		smp_mb__after_atomic();

	spin_unlock_irqrestore(&cp->cp_lock, flags);

	 
	rds_send_remove_from_sock(&list, RDS_RDMA_SUCCESS);
}
EXPORT_SYMBOL_GPL(rds_send_path_drop_acked);

void rds_send_drop_acked(struct rds_connection *conn, u64 ack,
			 is_acked_func is_acked)
{
	WARN_ON(conn->c_trans->t_mp_capable);
	rds_send_path_drop_acked(&conn->c_path[0], ack, is_acked);
}
EXPORT_SYMBOL_GPL(rds_send_drop_acked);

void rds_send_drop_to(struct rds_sock *rs, struct sockaddr_in6 *dest)
{
	struct rds_message *rm, *tmp;
	struct rds_connection *conn;
	struct rds_conn_path *cp;
	unsigned long flags;
	LIST_HEAD(list);

	 
	spin_lock_irqsave(&rs->rs_lock, flags);

	list_for_each_entry_safe(rm, tmp, &rs->rs_send_queue, m_sock_item) {
		if (dest &&
		    (!ipv6_addr_equal(&dest->sin6_addr, &rm->m_daddr) ||
		     dest->sin6_port != rm->m_inc.i_hdr.h_dport))
			continue;

		list_move(&rm->m_sock_item, &list);
		rds_send_sndbuf_remove(rs, rm);
		clear_bit(RDS_MSG_ON_SOCK, &rm->m_flags);
	}

	 
	smp_mb__after_atomic();

	spin_unlock_irqrestore(&rs->rs_lock, flags);

	if (list_empty(&list))
		return;

	 
	list_for_each_entry(rm, &list, m_sock_item) {

		conn = rm->m_inc.i_conn;
		if (conn->c_trans->t_mp_capable)
			cp = rm->m_inc.i_conn_path;
		else
			cp = &conn->c_path[0];

		spin_lock_irqsave(&cp->cp_lock, flags);
		 
		if (!test_and_clear_bit(RDS_MSG_ON_CONN, &rm->m_flags)) {
			spin_unlock_irqrestore(&cp->cp_lock, flags);
			continue;
		}
		list_del_init(&rm->m_conn_item);
		spin_unlock_irqrestore(&cp->cp_lock, flags);

		 
		spin_lock_irqsave(&rm->m_rs_lock, flags);

		spin_lock(&rs->rs_lock);
		__rds_send_complete(rs, rm, RDS_RDMA_CANCELED);
		spin_unlock(&rs->rs_lock);

		spin_unlock_irqrestore(&rm->m_rs_lock, flags);

		rds_message_put(rm);
	}

	rds_wake_sk_sleep(rs);

	while (!list_empty(&list)) {
		rm = list_entry(list.next, struct rds_message, m_sock_item);
		list_del_init(&rm->m_sock_item);
		rds_message_wait(rm);

		 
		spin_lock_irqsave(&rm->m_rs_lock, flags);

		spin_lock(&rs->rs_lock);
		__rds_send_complete(rs, rm, RDS_RDMA_CANCELED);
		spin_unlock(&rs->rs_lock);

		spin_unlock_irqrestore(&rm->m_rs_lock, flags);

		rds_message_put(rm);
	}
}

 
static int rds_send_queue_rm(struct rds_sock *rs, struct rds_connection *conn,
			     struct rds_conn_path *cp,
			     struct rds_message *rm, __be16 sport,
			     __be16 dport, int *queued)
{
	unsigned long flags;
	u32 len;

	if (*queued)
		goto out;

	len = be32_to_cpu(rm->m_inc.i_hdr.h_len);

	 
	spin_lock_irqsave(&rs->rs_lock, flags);

	 
	if (rs->rs_snd_bytes < rds_sk_sndbuf(rs)) {
		rs->rs_snd_bytes += len;

		 
		if (rs->rs_snd_bytes >= rds_sk_sndbuf(rs) / 2)
			set_bit(RDS_MSG_ACK_REQUIRED, &rm->m_flags);

		list_add_tail(&rm->m_sock_item, &rs->rs_send_queue);
		set_bit(RDS_MSG_ON_SOCK, &rm->m_flags);
		rds_message_addref(rm);
		sock_hold(rds_rs_to_sk(rs));
		rm->m_rs = rs;

		 
		rds_message_populate_header(&rm->m_inc.i_hdr, sport, dport, 0);
		rm->m_inc.i_conn = conn;
		rm->m_inc.i_conn_path = cp;
		rds_message_addref(rm);

		spin_lock(&cp->cp_lock);
		rm->m_inc.i_hdr.h_sequence = cpu_to_be64(cp->cp_next_tx_seq++);
		list_add_tail(&rm->m_conn_item, &cp->cp_send_queue);
		set_bit(RDS_MSG_ON_CONN, &rm->m_flags);
		spin_unlock(&cp->cp_lock);

		rdsdebug("queued msg %p len %d, rs %p bytes %d seq %llu\n",
			 rm, len, rs, rs->rs_snd_bytes,
			 (unsigned long long)be64_to_cpu(rm->m_inc.i_hdr.h_sequence));

		*queued = 1;
	}

	spin_unlock_irqrestore(&rs->rs_lock, flags);
out:
	return *queued;
}

 
static int rds_rm_size(struct msghdr *msg, int num_sgs,
		       struct rds_iov_vector_arr *vct)
{
	struct cmsghdr *cmsg;
	int size = 0;
	int cmsg_groups = 0;
	int retval;
	bool zcopy_cookie = false;
	struct rds_iov_vector *iov, *tmp_iov;

	if (num_sgs < 0)
		return -EINVAL;

	for_each_cmsghdr(cmsg, msg) {
		if (!CMSG_OK(msg, cmsg))
			return -EINVAL;

		if (cmsg->cmsg_level != SOL_RDS)
			continue;

		switch (cmsg->cmsg_type) {
		case RDS_CMSG_RDMA_ARGS:
			if (vct->indx >= vct->len) {
				vct->len += vct->incr;
				tmp_iov =
					krealloc(vct->vec,
						 vct->len *
						 sizeof(struct rds_iov_vector),
						 GFP_KERNEL);
				if (!tmp_iov) {
					vct->len -= vct->incr;
					return -ENOMEM;
				}
				vct->vec = tmp_iov;
			}
			iov = &vct->vec[vct->indx];
			memset(iov, 0, sizeof(struct rds_iov_vector));
			vct->indx++;
			cmsg_groups |= 1;
			retval = rds_rdma_extra_size(CMSG_DATA(cmsg), iov);
			if (retval < 0)
				return retval;
			size += retval;

			break;

		case RDS_CMSG_ZCOPY_COOKIE:
			zcopy_cookie = true;
			fallthrough;

		case RDS_CMSG_RDMA_DEST:
		case RDS_CMSG_RDMA_MAP:
			cmsg_groups |= 2;
			 
			break;

		case RDS_CMSG_ATOMIC_CSWP:
		case RDS_CMSG_ATOMIC_FADD:
		case RDS_CMSG_MASKED_ATOMIC_CSWP:
		case RDS_CMSG_MASKED_ATOMIC_FADD:
			cmsg_groups |= 1;
			size += sizeof(struct scatterlist);
			break;

		default:
			return -EINVAL;
		}

	}

	if ((msg->msg_flags & MSG_ZEROCOPY) && !zcopy_cookie)
		return -EINVAL;

	size += num_sgs * sizeof(struct scatterlist);

	 
	if (cmsg_groups == 3)
		return -EINVAL;

	return size;
}

static int rds_cmsg_zcopy(struct rds_sock *rs, struct rds_message *rm,
			  struct cmsghdr *cmsg)
{
	u32 *cookie;

	if (cmsg->cmsg_len < CMSG_LEN(sizeof(*cookie)) ||
	    !rm->data.op_mmp_znotifier)
		return -EINVAL;
	cookie = CMSG_DATA(cmsg);
	rm->data.op_mmp_znotifier->z_cookie = *cookie;
	return 0;
}

static int rds_cmsg_send(struct rds_sock *rs, struct rds_message *rm,
			 struct msghdr *msg, int *allocated_mr,
			 struct rds_iov_vector_arr *vct)
{
	struct cmsghdr *cmsg;
	int ret = 0, ind = 0;

	for_each_cmsghdr(cmsg, msg) {
		if (!CMSG_OK(msg, cmsg))
			return -EINVAL;

		if (cmsg->cmsg_level != SOL_RDS)
			continue;

		 
		switch (cmsg->cmsg_type) {
		case RDS_CMSG_RDMA_ARGS:
			if (ind >= vct->indx)
				return -ENOMEM;
			ret = rds_cmsg_rdma_args(rs, rm, cmsg, &vct->vec[ind]);
			ind++;
			break;

		case RDS_CMSG_RDMA_DEST:
			ret = rds_cmsg_rdma_dest(rs, rm, cmsg);
			break;

		case RDS_CMSG_RDMA_MAP:
			ret = rds_cmsg_rdma_map(rs, rm, cmsg);
			if (!ret)
				*allocated_mr = 1;
			else if (ret == -ENODEV)
				 
				ret = -EAGAIN;
			break;
		case RDS_CMSG_ATOMIC_CSWP:
		case RDS_CMSG_ATOMIC_FADD:
		case RDS_CMSG_MASKED_ATOMIC_CSWP:
		case RDS_CMSG_MASKED_ATOMIC_FADD:
			ret = rds_cmsg_atomic(rs, rm, cmsg);
			break;

		case RDS_CMSG_ZCOPY_COOKIE:
			ret = rds_cmsg_zcopy(rs, rm, cmsg);
			break;

		default:
			return -EINVAL;
		}

		if (ret)
			break;
	}

	return ret;
}

static int rds_send_mprds_hash(struct rds_sock *rs,
			       struct rds_connection *conn, int nonblock)
{
	int hash;

	if (conn->c_npaths == 0)
		hash = RDS_MPATH_HASH(rs, RDS_MPATH_WORKERS);
	else
		hash = RDS_MPATH_HASH(rs, conn->c_npaths);
	if (conn->c_npaths == 0 && hash != 0) {
		rds_send_ping(conn, 0);

		 
		if (conn->c_npaths == 0) {
			 
			if (nonblock)
				return 0;
			if (wait_event_interruptible(conn->c_hs_waitq,
						     conn->c_npaths != 0))
				hash = 0;
		}
		if (conn->c_npaths == 1)
			hash = 0;
	}
	return hash;
}

static int rds_rdma_bytes(struct msghdr *msg, size_t *rdma_bytes)
{
	struct rds_rdma_args *args;
	struct cmsghdr *cmsg;

	for_each_cmsghdr(cmsg, msg) {
		if (!CMSG_OK(msg, cmsg))
			return -EINVAL;

		if (cmsg->cmsg_level != SOL_RDS)
			continue;

		if (cmsg->cmsg_type == RDS_CMSG_RDMA_ARGS) {
			if (cmsg->cmsg_len <
			    CMSG_LEN(sizeof(struct rds_rdma_args)))
				return -EINVAL;
			args = CMSG_DATA(cmsg);
			*rdma_bytes += args->remote_vec.bytes;
		}
	}
	return 0;
}

int rds_sendmsg(struct socket *sock, struct msghdr *msg, size_t payload_len)
{
	struct sock *sk = sock->sk;
	struct rds_sock *rs = rds_sk_to_rs(sk);
	DECLARE_SOCKADDR(struct sockaddr_in6 *, sin6, msg->msg_name);
	DECLARE_SOCKADDR(struct sockaddr_in *, usin, msg->msg_name);
	__be16 dport;
	struct rds_message *rm = NULL;
	struct rds_connection *conn;
	int ret = 0;
	int queued = 0, allocated_mr = 0;
	int nonblock = msg->msg_flags & MSG_DONTWAIT;
	long timeo = sock_sndtimeo(sk, nonblock);
	struct rds_conn_path *cpath;
	struct in6_addr daddr;
	__u32 scope_id = 0;
	size_t rdma_payload_len = 0;
	bool zcopy = ((msg->msg_flags & MSG_ZEROCOPY) &&
		      sock_flag(rds_rs_to_sk(rs), SOCK_ZEROCOPY));
	int num_sgs = DIV_ROUND_UP(payload_len, PAGE_SIZE);
	int namelen;
	struct rds_iov_vector_arr vct;
	int ind;

	memset(&vct, 0, sizeof(vct));

	 
	vct.incr = 1;

	 
	 
	if (msg->msg_flags & ~(MSG_DONTWAIT | MSG_CMSG_COMPAT | MSG_ZEROCOPY)) {
		ret = -EOPNOTSUPP;
		goto out;
	}

	namelen = msg->msg_namelen;
	if (namelen != 0) {
		if (namelen < sizeof(*usin)) {
			ret = -EINVAL;
			goto out;
		}
		switch (usin->sin_family) {
		case AF_INET:
			if (usin->sin_addr.s_addr == htonl(INADDR_ANY) ||
			    usin->sin_addr.s_addr == htonl(INADDR_BROADCAST) ||
			    ipv4_is_multicast(usin->sin_addr.s_addr)) {
				ret = -EINVAL;
				goto out;
			}
			ipv6_addr_set_v4mapped(usin->sin_addr.s_addr, &daddr);
			dport = usin->sin_port;
			break;

#if IS_ENABLED(CONFIG_IPV6)
		case AF_INET6: {
			int addr_type;

			if (namelen < sizeof(*sin6)) {
				ret = -EINVAL;
				goto out;
			}
			addr_type = ipv6_addr_type(&sin6->sin6_addr);
			if (!(addr_type & IPV6_ADDR_UNICAST)) {
				__be32 addr4;

				if (!(addr_type & IPV6_ADDR_MAPPED)) {
					ret = -EINVAL;
					goto out;
				}

				 
				addr4 = sin6->sin6_addr.s6_addr32[3];
				if (addr4 == htonl(INADDR_ANY) ||
				    addr4 == htonl(INADDR_BROADCAST) ||
				    ipv4_is_multicast(addr4)) {
					ret = -EINVAL;
					goto out;
				}
			}
			if (addr_type & IPV6_ADDR_LINKLOCAL) {
				if (sin6->sin6_scope_id == 0) {
					ret = -EINVAL;
					goto out;
				}
				scope_id = sin6->sin6_scope_id;
			}

			daddr = sin6->sin6_addr;
			dport = sin6->sin6_port;
			break;
		}
#endif

		default:
			ret = -EINVAL;
			goto out;
		}
	} else {
		 
		lock_sock(sk);
		daddr = rs->rs_conn_addr;
		dport = rs->rs_conn_port;
		scope_id = rs->rs_bound_scope_id;
		release_sock(sk);
	}

	lock_sock(sk);
	if (ipv6_addr_any(&rs->rs_bound_addr) || ipv6_addr_any(&daddr)) {
		release_sock(sk);
		ret = -ENOTCONN;
		goto out;
	} else if (namelen != 0) {
		 
		if (ipv6_addr_v4mapped(&daddr) ^
		    ipv6_addr_v4mapped(&rs->rs_bound_addr)) {
			release_sock(sk);
			ret = -EOPNOTSUPP;
			goto out;
		}
		 
		if (scope_id != rs->rs_bound_scope_id) {
			if (!scope_id) {
				scope_id = rs->rs_bound_scope_id;
			} else if (rs->rs_bound_scope_id) {
				release_sock(sk);
				ret = -EINVAL;
				goto out;
			}
		}
	}
	release_sock(sk);

	ret = rds_rdma_bytes(msg, &rdma_payload_len);
	if (ret)
		goto out;

	if (max_t(size_t, payload_len, rdma_payload_len) > RDS_MAX_MSG_SIZE) {
		ret = -EMSGSIZE;
		goto out;
	}

	if (payload_len > rds_sk_sndbuf(rs)) {
		ret = -EMSGSIZE;
		goto out;
	}

	if (zcopy) {
		if (rs->rs_transport->t_type != RDS_TRANS_TCP) {
			ret = -EOPNOTSUPP;
			goto out;
		}
		num_sgs = iov_iter_npages(&msg->msg_iter, INT_MAX);
	}
	 
	ret = rds_rm_size(msg, num_sgs, &vct);
	if (ret < 0)
		goto out;

	rm = rds_message_alloc(ret, GFP_KERNEL);
	if (!rm) {
		ret = -ENOMEM;
		goto out;
	}

	 
	if (payload_len) {
		rm->data.op_sg = rds_message_alloc_sgs(rm, num_sgs);
		if (IS_ERR(rm->data.op_sg)) {
			ret = PTR_ERR(rm->data.op_sg);
			goto out;
		}
		ret = rds_message_copy_from_user(rm, &msg->msg_iter, zcopy);
		if (ret)
			goto out;
	}
	rm->data.op_active = 1;

	rm->m_daddr = daddr;

	 
	if (rs->rs_conn && ipv6_addr_equal(&rs->rs_conn->c_faddr, &daddr) &&
	    rs->rs_tos == rs->rs_conn->c_tos) {
		conn = rs->rs_conn;
	} else {
		conn = rds_conn_create_outgoing(sock_net(sock->sk),
						&rs->rs_bound_addr, &daddr,
						rs->rs_transport, rs->rs_tos,
						sock->sk->sk_allocation,
						scope_id);
		if (IS_ERR(conn)) {
			ret = PTR_ERR(conn);
			goto out;
		}
		rs->rs_conn = conn;
	}

	if (conn->c_trans->t_mp_capable)
		cpath = &conn->c_path[rds_send_mprds_hash(rs, conn, nonblock)];
	else
		cpath = &conn->c_path[0];

	rm->m_conn_path = cpath;

	 
	ret = rds_cmsg_send(rs, rm, msg, &allocated_mr, &vct);
	if (ret) {
		 
		if (ret ==  -EAGAIN)
			rds_conn_connect_if_down(conn);
		goto out;
	}

	if (rm->rdma.op_active && !conn->c_trans->xmit_rdma) {
		printk_ratelimited(KERN_NOTICE "rdma_op %p conn xmit_rdma %p\n",
			       &rm->rdma, conn->c_trans->xmit_rdma);
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (rm->atomic.op_active && !conn->c_trans->xmit_atomic) {
		printk_ratelimited(KERN_NOTICE "atomic_op %p conn xmit_atomic %p\n",
			       &rm->atomic, conn->c_trans->xmit_atomic);
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (rds_destroy_pending(conn)) {
		ret = -EAGAIN;
		goto out;
	}

	if (rds_conn_path_down(cpath))
		rds_check_all_paths(conn);

	ret = rds_cong_wait(conn->c_fcong, dport, nonblock, rs);
	if (ret) {
		rs->rs_seen_congestion = 1;
		goto out;
	}
	while (!rds_send_queue_rm(rs, conn, cpath, rm, rs->rs_bound_port,
				  dport, &queued)) {
		rds_stats_inc(s_send_queue_full);

		if (nonblock) {
			ret = -EAGAIN;
			goto out;
		}

		timeo = wait_event_interruptible_timeout(*sk_sleep(sk),
					rds_send_queue_rm(rs, conn, cpath, rm,
							  rs->rs_bound_port,
							  dport,
							  &queued),
					timeo);
		rdsdebug("sendmsg woke queued %d timeo %ld\n", queued, timeo);
		if (timeo > 0 || timeo == MAX_SCHEDULE_TIMEOUT)
			continue;

		ret = timeo;
		if (ret == 0)
			ret = -ETIMEDOUT;
		goto out;
	}

	 
	rds_stats_inc(s_send_queued);

	ret = rds_send_xmit(cpath);
	if (ret == -ENOMEM || ret == -EAGAIN) {
		ret = 0;
		rcu_read_lock();
		if (rds_destroy_pending(cpath->cp_conn))
			ret = -ENETUNREACH;
		else
			queue_delayed_work(rds_wq, &cpath->cp_send_w, 1);
		rcu_read_unlock();
	}
	if (ret)
		goto out;
	rds_message_put(rm);

	for (ind = 0; ind < vct.indx; ind++)
		kfree(vct.vec[ind].iov);
	kfree(vct.vec);

	return payload_len;

out:
	for (ind = 0; ind < vct.indx; ind++)
		kfree(vct.vec[ind].iov);
	kfree(vct.vec);

	 
	if (allocated_mr)
		rds_rdma_unuse(rs, rds_rdma_cookie_key(rm->m_rdma_cookie), 1);

	if (rm)
		rds_message_put(rm);
	return ret;
}

 
static int
rds_send_probe(struct rds_conn_path *cp, __be16 sport,
	       __be16 dport, u8 h_flags)
{
	struct rds_message *rm;
	unsigned long flags;
	int ret = 0;

	rm = rds_message_alloc(0, GFP_ATOMIC);
	if (!rm) {
		ret = -ENOMEM;
		goto out;
	}

	rm->m_daddr = cp->cp_conn->c_faddr;
	rm->data.op_active = 1;

	rds_conn_path_connect_if_down(cp);

	ret = rds_cong_wait(cp->cp_conn->c_fcong, dport, 1, NULL);
	if (ret)
		goto out;

	spin_lock_irqsave(&cp->cp_lock, flags);
	list_add_tail(&rm->m_conn_item, &cp->cp_send_queue);
	set_bit(RDS_MSG_ON_CONN, &rm->m_flags);
	rds_message_addref(rm);
	rm->m_inc.i_conn = cp->cp_conn;
	rm->m_inc.i_conn_path = cp;

	rds_message_populate_header(&rm->m_inc.i_hdr, sport, dport,
				    cp->cp_next_tx_seq);
	rm->m_inc.i_hdr.h_flags |= h_flags;
	cp->cp_next_tx_seq++;

	if (RDS_HS_PROBE(be16_to_cpu(sport), be16_to_cpu(dport)) &&
	    cp->cp_conn->c_trans->t_mp_capable) {
		u16 npaths = cpu_to_be16(RDS_MPATH_WORKERS);
		u32 my_gen_num = cpu_to_be32(cp->cp_conn->c_my_gen_num);

		rds_message_add_extension(&rm->m_inc.i_hdr,
					  RDS_EXTHDR_NPATHS, &npaths,
					  sizeof(npaths));
		rds_message_add_extension(&rm->m_inc.i_hdr,
					  RDS_EXTHDR_GEN_NUM,
					  &my_gen_num,
					  sizeof(u32));
	}
	spin_unlock_irqrestore(&cp->cp_lock, flags);

	rds_stats_inc(s_send_queued);
	rds_stats_inc(s_send_pong);

	 
	rcu_read_lock();
	if (!rds_destroy_pending(cp->cp_conn))
		queue_delayed_work(rds_wq, &cp->cp_send_w, 1);
	rcu_read_unlock();

	rds_message_put(rm);
	return 0;

out:
	if (rm)
		rds_message_put(rm);
	return ret;
}

int
rds_send_pong(struct rds_conn_path *cp, __be16 dport)
{
	return rds_send_probe(cp, 0, dport, 0);
}

void
rds_send_ping(struct rds_connection *conn, int cp_index)
{
	unsigned long flags;
	struct rds_conn_path *cp = &conn->c_path[cp_index];

	spin_lock_irqsave(&cp->cp_lock, flags);
	if (conn->c_ping_triggered) {
		spin_unlock_irqrestore(&cp->cp_lock, flags);
		return;
	}
	conn->c_ping_triggered = 1;
	spin_unlock_irqrestore(&cp->cp_lock, flags);
	rds_send_probe(cp, cpu_to_be16(RDS_FLAG_PROBE_PORT), 0, 0);
}
EXPORT_SYMBOL_GPL(rds_send_ping);
