
 

#include <linux/fs.h>
#include <linux/list.h>
#include <linux/gfp.h>
#include <linux/wait.h>
#include <linux/net.h>
#include <linux/delay.h>
#include <linux/freezer.h>
#include <linux/tcp.h>
#include <linux/bvec.h>
#include <linux/highmem.h>
#include <linux/uaccess.h>
#include <linux/processor.h>
#include <linux/mempool.h>
#include <linux/sched/signal.h>
#include <linux/task_io_accounting_ops.h>
#include "cifspdu.h"
#include "cifsglob.h"
#include "cifsproto.h"
#include "cifs_debug.h"
#include "smb2proto.h"
#include "smbdirect.h"

 
#define CIFS_MAX_IOV_SIZE 8

void
cifs_wake_up_task(struct mid_q_entry *mid)
{
	if (mid->mid_state == MID_RESPONSE_RECEIVED)
		mid->mid_state = MID_RESPONSE_READY;
	wake_up_process(mid->callback_data);
}

static struct mid_q_entry *
alloc_mid(const struct smb_hdr *smb_buffer, struct TCP_Server_Info *server)
{
	struct mid_q_entry *temp;

	if (server == NULL) {
		cifs_dbg(VFS, "%s: null TCP session\n", __func__);
		return NULL;
	}

	temp = mempool_alloc(cifs_mid_poolp, GFP_NOFS);
	memset(temp, 0, sizeof(struct mid_q_entry));
	kref_init(&temp->refcount);
	temp->mid = get_mid(smb_buffer);
	temp->pid = current->pid;
	temp->command = cpu_to_le16(smb_buffer->Command);
	cifs_dbg(FYI, "For smb_command %d\n", smb_buffer->Command);
	 
	 
	temp->when_alloc = jiffies;
	temp->server = server;

	 
	get_task_struct(current);
	temp->creator = current;
	temp->callback = cifs_wake_up_task;
	temp->callback_data = current;

	atomic_inc(&mid_count);
	temp->mid_state = MID_REQUEST_ALLOCATED;
	return temp;
}

void __release_mid(struct kref *refcount)
{
	struct mid_q_entry *midEntry =
			container_of(refcount, struct mid_q_entry, refcount);
#ifdef CONFIG_CIFS_STATS2
	__le16 command = midEntry->server->vals->lock_cmd;
	__u16 smb_cmd = le16_to_cpu(midEntry->command);
	unsigned long now;
	unsigned long roundtrip_time;
#endif
	struct TCP_Server_Info *server = midEntry->server;

	if (midEntry->resp_buf && (midEntry->mid_flags & MID_WAIT_CANCELLED) &&
	    (midEntry->mid_state == MID_RESPONSE_RECEIVED ||
	     midEntry->mid_state == MID_RESPONSE_READY) &&
	    server->ops->handle_cancelled_mid)
		server->ops->handle_cancelled_mid(midEntry, server);

	midEntry->mid_state = MID_FREE;
	atomic_dec(&mid_count);
	if (midEntry->large_buf)
		cifs_buf_release(midEntry->resp_buf);
	else
		cifs_small_buf_release(midEntry->resp_buf);
#ifdef CONFIG_CIFS_STATS2
	now = jiffies;
	if (now < midEntry->when_alloc)
		cifs_server_dbg(VFS, "Invalid mid allocation time\n");
	roundtrip_time = now - midEntry->when_alloc;

	if (smb_cmd < NUMBER_OF_SMB2_COMMANDS) {
		if (atomic_read(&server->num_cmds[smb_cmd]) == 0) {
			server->slowest_cmd[smb_cmd] = roundtrip_time;
			server->fastest_cmd[smb_cmd] = roundtrip_time;
		} else {
			if (server->slowest_cmd[smb_cmd] < roundtrip_time)
				server->slowest_cmd[smb_cmd] = roundtrip_time;
			else if (server->fastest_cmd[smb_cmd] > roundtrip_time)
				server->fastest_cmd[smb_cmd] = roundtrip_time;
		}
		cifs_stats_inc(&server->num_cmds[smb_cmd]);
		server->time_per_cmd[smb_cmd] += roundtrip_time;
	}
	 
	if ((slow_rsp_threshold != 0) &&
	    time_after(now, midEntry->when_alloc + (slow_rsp_threshold * HZ)) &&
	    (midEntry->command != command)) {
		 
		if (smb_cmd < NUMBER_OF_SMB2_COMMANDS)
			cifs_stats_inc(&server->smb2slowcmd[smb_cmd]);

		trace_smb3_slow_rsp(smb_cmd, midEntry->mid, midEntry->pid,
			       midEntry->when_sent, midEntry->when_received);
		if (cifsFYI & CIFS_TIMER) {
			pr_debug("slow rsp: cmd %d mid %llu",
				 midEntry->command, midEntry->mid);
			cifs_info("A: 0x%lx S: 0x%lx R: 0x%lx\n",
				  now - midEntry->when_alloc,
				  now - midEntry->when_sent,
				  now - midEntry->when_received);
		}
	}
#endif
	put_task_struct(midEntry->creator);

	mempool_free(midEntry, cifs_mid_poolp);
}

void
delete_mid(struct mid_q_entry *mid)
{
	spin_lock(&mid->server->mid_lock);
	if (!(mid->mid_flags & MID_DELETED)) {
		list_del_init(&mid->qhead);
		mid->mid_flags |= MID_DELETED;
	}
	spin_unlock(&mid->server->mid_lock);

	release_mid(mid);
}

 
static int
smb_send_kvec(struct TCP_Server_Info *server, struct msghdr *smb_msg,
	      size_t *sent)
{
	int rc = 0;
	int retries = 0;
	struct socket *ssocket = server->ssocket;

	*sent = 0;

	if (server->noblocksnd)
		smb_msg->msg_flags = MSG_DONTWAIT + MSG_NOSIGNAL;
	else
		smb_msg->msg_flags = MSG_NOSIGNAL;

	while (msg_data_left(smb_msg)) {
		 
		rc = sock_sendmsg(ssocket, smb_msg);
		if (rc == -EAGAIN) {
			retries++;
			if (retries >= 14 ||
			    (!server->noblocksnd && (retries > 2))) {
				cifs_server_dbg(VFS, "sends on sock %p stuck for 15 seconds\n",
					 ssocket);
				return -EAGAIN;
			}
			msleep(1 << retries);
			continue;
		}

		if (rc < 0)
			return rc;

		if (rc == 0) {
			 
			cifs_server_dbg(VFS, "tcp sent no data\n");
			msleep(500);
			continue;
		}

		 
		*sent += rc;
		retries = 0;  
	}
	return 0;
}

unsigned long
smb_rqst_len(struct TCP_Server_Info *server, struct smb_rqst *rqst)
{
	unsigned int i;
	struct kvec *iov;
	int nvec;
	unsigned long buflen = 0;

	if (!is_smb1(server) && rqst->rq_nvec >= 2 &&
	    rqst->rq_iov[0].iov_len == 4) {
		iov = &rqst->rq_iov[1];
		nvec = rqst->rq_nvec - 1;
	} else {
		iov = rqst->rq_iov;
		nvec = rqst->rq_nvec;
	}

	 
	for (i = 0; i < nvec; i++)
		buflen += iov[i].iov_len;

	buflen += iov_iter_count(&rqst->rq_iter);
	return buflen;
}

static int
__smb_send_rqst(struct TCP_Server_Info *server, int num_rqst,
		struct smb_rqst *rqst)
{
	int rc;
	struct kvec *iov;
	int n_vec;
	unsigned int send_length = 0;
	unsigned int i, j;
	sigset_t mask, oldmask;
	size_t total_len = 0, sent, size;
	struct socket *ssocket = server->ssocket;
	struct msghdr smb_msg = {};
	__be32 rfc1002_marker;

	cifs_in_send_inc(server);
	if (cifs_rdma_enabled(server)) {
		 
		rc = -EAGAIN;
		if (server->smbd_conn)
			rc = smbd_send(server, num_rqst, rqst);
		goto smbd_done;
	}

	rc = -EAGAIN;
	if (ssocket == NULL)
		goto out;

	rc = -ERESTARTSYS;
	if (fatal_signal_pending(current)) {
		cifs_dbg(FYI, "signal pending before send request\n");
		goto out;
	}

	rc = 0;
	 
	tcp_sock_set_cork(ssocket->sk, true);

	for (j = 0; j < num_rqst; j++)
		send_length += smb_rqst_len(server, &rqst[j]);
	rfc1002_marker = cpu_to_be32(send_length);

	 

	sigfillset(&mask);
	sigprocmask(SIG_BLOCK, &mask, &oldmask);

	 
	if (!is_smb1(server)) {
		struct kvec hiov = {
			.iov_base = &rfc1002_marker,
			.iov_len  = 4
		};
		iov_iter_kvec(&smb_msg.msg_iter, ITER_SOURCE, &hiov, 1, 4);
		rc = smb_send_kvec(server, &smb_msg, &sent);
		if (rc < 0)
			goto unmask;

		total_len += sent;
		send_length += 4;
	}

	cifs_dbg(FYI, "Sending smb: smb_len=%u\n", send_length);

	for (j = 0; j < num_rqst; j++) {
		iov = rqst[j].rq_iov;
		n_vec = rqst[j].rq_nvec;

		size = 0;
		for (i = 0; i < n_vec; i++) {
			dump_smb(iov[i].iov_base, iov[i].iov_len);
			size += iov[i].iov_len;
		}

		iov_iter_kvec(&smb_msg.msg_iter, ITER_SOURCE, iov, n_vec, size);

		rc = smb_send_kvec(server, &smb_msg, &sent);
		if (rc < 0)
			goto unmask;

		total_len += sent;

		if (iov_iter_count(&rqst[j].rq_iter) > 0) {
			smb_msg.msg_iter = rqst[j].rq_iter;
			rc = smb_send_kvec(server, &smb_msg, &sent);
			if (rc < 0)
				break;
			total_len += sent;
		}

}

unmask:
	sigprocmask(SIG_SETMASK, &oldmask, NULL);

	 

	if (signal_pending(current) && (total_len != send_length)) {
		cifs_dbg(FYI, "signal is pending after attempt to send\n");
		rc = -ERESTARTSYS;
	}

	 
	tcp_sock_set_cork(ssocket->sk, false);

	if ((total_len > 0) && (total_len != send_length)) {
		cifs_dbg(FYI, "partial send (wanted=%u sent=%zu): terminating session\n",
			 send_length, total_len);
		 
		cifs_signal_cifsd_for_reconnect(server, false);
		trace_smb3_partial_send_reconnect(server->CurrentMid,
						  server->conn_id, server->hostname);
	}
smbd_done:
	if (rc < 0 && rc != -EINTR)
		cifs_server_dbg(VFS, "Error %d sending data on socket to server\n",
			 rc);
	else if (rc > 0)
		rc = 0;
out:
	cifs_in_send_dec(server);
	return rc;
}

struct send_req_vars {
	struct smb2_transform_hdr tr_hdr;
	struct smb_rqst rqst[MAX_COMPOUND];
	struct kvec iov;
};

static int
smb_send_rqst(struct TCP_Server_Info *server, int num_rqst,
	      struct smb_rqst *rqst, int flags)
{
	struct send_req_vars *vars;
	struct smb_rqst *cur_rqst;
	struct kvec *iov;
	int rc;

	if (!(flags & CIFS_TRANSFORM_REQ))
		return __smb_send_rqst(server, num_rqst, rqst);

	if (num_rqst > MAX_COMPOUND - 1)
		return -ENOMEM;

	if (!server->ops->init_transform_rq) {
		cifs_server_dbg(VFS, "Encryption requested but transform callback is missing\n");
		return -EIO;
	}

	vars = kzalloc(sizeof(*vars), GFP_NOFS);
	if (!vars)
		return -ENOMEM;
	cur_rqst = vars->rqst;
	iov = &vars->iov;

	iov->iov_base = &vars->tr_hdr;
	iov->iov_len = sizeof(vars->tr_hdr);
	cur_rqst[0].rq_iov = iov;
	cur_rqst[0].rq_nvec = 1;

	rc = server->ops->init_transform_rq(server, num_rqst + 1,
					    &cur_rqst[0], rqst);
	if (rc)
		goto out;

	rc = __smb_send_rqst(server, num_rqst + 1, &cur_rqst[0]);
	smb3_free_compound_rqst(num_rqst, &cur_rqst[1]);
out:
	kfree(vars);
	return rc;
}

int
smb_send(struct TCP_Server_Info *server, struct smb_hdr *smb_buffer,
	 unsigned int smb_buf_length)
{
	struct kvec iov[2];
	struct smb_rqst rqst = { .rq_iov = iov,
				 .rq_nvec = 2 };

	iov[0].iov_base = smb_buffer;
	iov[0].iov_len = 4;
	iov[1].iov_base = (char *)smb_buffer + 4;
	iov[1].iov_len = smb_buf_length;

	return __smb_send_rqst(server, 1, &rqst);
}

static int
wait_for_free_credits(struct TCP_Server_Info *server, const int num_credits,
		      const int timeout, const int flags,
		      unsigned int *instance)
{
	long rc;
	int *credits;
	int optype;
	long int t;
	int scredits, in_flight;

	if (timeout < 0)
		t = MAX_JIFFY_OFFSET;
	else
		t = msecs_to_jiffies(timeout);

	optype = flags & CIFS_OP_MASK;

	*instance = 0;

	credits = server->ops->get_credits_field(server, optype);
	 
	if (*credits <= 0 && optype == CIFS_ECHO_OP)
		return -EAGAIN;

	spin_lock(&server->req_lock);
	if ((flags & CIFS_TIMEOUT_MASK) == CIFS_NON_BLOCKING) {
		 
		server->in_flight++;
		if (server->in_flight > server->max_in_flight)
			server->max_in_flight = server->in_flight;
		*credits -= 1;
		*instance = server->reconnect_instance;
		scredits = *credits;
		in_flight = server->in_flight;
		spin_unlock(&server->req_lock);

		trace_smb3_nblk_credits(server->CurrentMid,
				server->conn_id, server->hostname, scredits, -1, in_flight);
		cifs_dbg(FYI, "%s: remove %u credits total=%d\n",
				__func__, 1, scredits);

		return 0;
	}

	while (1) {
		spin_unlock(&server->req_lock);

		spin_lock(&server->srv_lock);
		if (server->tcpStatus == CifsExiting) {
			spin_unlock(&server->srv_lock);
			return -ENOENT;
		}
		spin_unlock(&server->srv_lock);

		spin_lock(&server->req_lock);
		if (*credits < num_credits) {
			scredits = *credits;
			spin_unlock(&server->req_lock);

			cifs_num_waiters_inc(server);
			rc = wait_event_killable_timeout(server->request_q,
				has_credits(server, credits, num_credits), t);
			cifs_num_waiters_dec(server);
			if (!rc) {
				spin_lock(&server->req_lock);
				scredits = *credits;
				in_flight = server->in_flight;
				spin_unlock(&server->req_lock);

				trace_smb3_credit_timeout(server->CurrentMid,
						server->conn_id, server->hostname, scredits,
						num_credits, in_flight);
				cifs_server_dbg(VFS, "wait timed out after %d ms\n",
						timeout);
				return -EBUSY;
			}
			if (rc == -ERESTARTSYS)
				return -ERESTARTSYS;
			spin_lock(&server->req_lock);
		} else {
			 
			if (!optype && num_credits == 1 &&
			    server->in_flight > 2 * MAX_COMPOUND &&
			    *credits <= MAX_COMPOUND) {
				spin_unlock(&server->req_lock);

				cifs_num_waiters_inc(server);
				rc = wait_event_killable_timeout(
					server->request_q,
					has_credits(server, credits,
						    MAX_COMPOUND + 1),
					t);
				cifs_num_waiters_dec(server);
				if (!rc) {
					spin_lock(&server->req_lock);
					scredits = *credits;
					in_flight = server->in_flight;
					spin_unlock(&server->req_lock);

					trace_smb3_credit_timeout(
							server->CurrentMid,
							server->conn_id, server->hostname,
							scredits, num_credits, in_flight);
					cifs_server_dbg(VFS, "wait timed out after %d ms\n",
							timeout);
					return -EBUSY;
				}
				if (rc == -ERESTARTSYS)
					return -ERESTARTSYS;
				spin_lock(&server->req_lock);
				continue;
			}

			 

			 
			if ((flags & CIFS_TIMEOUT_MASK) != CIFS_BLOCKING_OP) {
				*credits -= num_credits;
				server->in_flight += num_credits;
				if (server->in_flight > server->max_in_flight)
					server->max_in_flight = server->in_flight;
				*instance = server->reconnect_instance;
			}
			scredits = *credits;
			in_flight = server->in_flight;
			spin_unlock(&server->req_lock);

			trace_smb3_waitff_credits(server->CurrentMid,
					server->conn_id, server->hostname, scredits,
					-(num_credits), in_flight);
			cifs_dbg(FYI, "%s: remove %u credits total=%d\n",
					__func__, num_credits, scredits);
			break;
		}
	}
	return 0;
}

static int
wait_for_free_request(struct TCP_Server_Info *server, const int flags,
		      unsigned int *instance)
{
	return wait_for_free_credits(server, 1, -1, flags,
				     instance);
}

static int
wait_for_compound_request(struct TCP_Server_Info *server, int num,
			  const int flags, unsigned int *instance)
{
	int *credits;
	int scredits, in_flight;

	credits = server->ops->get_credits_field(server, flags & CIFS_OP_MASK);

	spin_lock(&server->req_lock);
	scredits = *credits;
	in_flight = server->in_flight;

	if (*credits < num) {
		 
		if (server->in_flight == 0) {
			spin_unlock(&server->req_lock);
			trace_smb3_insufficient_credits(server->CurrentMid,
					server->conn_id, server->hostname, scredits,
					num, in_flight);
			cifs_dbg(FYI, "%s: %d requests in flight, needed %d total=%d\n",
					__func__, in_flight, num, scredits);
			return -EDEADLK;
		}
	}
	spin_unlock(&server->req_lock);

	return wait_for_free_credits(server, num, 60000, flags,
				     instance);
}

int
cifs_wait_mtu_credits(struct TCP_Server_Info *server, unsigned int size,
		      unsigned int *num, struct cifs_credits *credits)
{
	*num = size;
	credits->value = 0;
	credits->instance = server->reconnect_instance;
	return 0;
}

static int allocate_mid(struct cifs_ses *ses, struct smb_hdr *in_buf,
			struct mid_q_entry **ppmidQ)
{
	spin_lock(&ses->ses_lock);
	if (ses->ses_status == SES_NEW) {
		if ((in_buf->Command != SMB_COM_SESSION_SETUP_ANDX) &&
			(in_buf->Command != SMB_COM_NEGOTIATE)) {
			spin_unlock(&ses->ses_lock);
			return -EAGAIN;
		}
		 
	}

	if (ses->ses_status == SES_EXITING) {
		 
		if (in_buf->Command != SMB_COM_LOGOFF_ANDX) {
			spin_unlock(&ses->ses_lock);
			return -EAGAIN;
		}
		 
	}
	spin_unlock(&ses->ses_lock);

	*ppmidQ = alloc_mid(in_buf, ses->server);
	if (*ppmidQ == NULL)
		return -ENOMEM;
	spin_lock(&ses->server->mid_lock);
	list_add_tail(&(*ppmidQ)->qhead, &ses->server->pending_mid_q);
	spin_unlock(&ses->server->mid_lock);
	return 0;
}

static int
wait_for_response(struct TCP_Server_Info *server, struct mid_q_entry *midQ)
{
	int error;

	error = wait_event_state(server->response_q,
				 midQ->mid_state != MID_REQUEST_SUBMITTED &&
				 midQ->mid_state != MID_RESPONSE_RECEIVED,
				 (TASK_KILLABLE|TASK_FREEZABLE_UNSAFE));
	if (error < 0)
		return -ERESTARTSYS;

	return 0;
}

struct mid_q_entry *
cifs_setup_async_request(struct TCP_Server_Info *server, struct smb_rqst *rqst)
{
	int rc;
	struct smb_hdr *hdr = (struct smb_hdr *)rqst->rq_iov[0].iov_base;
	struct mid_q_entry *mid;

	if (rqst->rq_iov[0].iov_len != 4 ||
	    rqst->rq_iov[0].iov_base + 4 != rqst->rq_iov[1].iov_base)
		return ERR_PTR(-EIO);

	 
	if (server->sign)
		hdr->Flags2 |= SMBFLG2_SECURITY_SIGNATURE;

	mid = alloc_mid(hdr, server);
	if (mid == NULL)
		return ERR_PTR(-ENOMEM);

	rc = cifs_sign_rqst(rqst, server, &mid->sequence_number);
	if (rc) {
		release_mid(mid);
		return ERR_PTR(rc);
	}

	return mid;
}

 
int
cifs_call_async(struct TCP_Server_Info *server, struct smb_rqst *rqst,
		mid_receive_t *receive, mid_callback_t *callback,
		mid_handle_t *handle, void *cbdata, const int flags,
		const struct cifs_credits *exist_credits)
{
	int rc;
	struct mid_q_entry *mid;
	struct cifs_credits credits = { .value = 0, .instance = 0 };
	unsigned int instance;
	int optype;

	optype = flags & CIFS_OP_MASK;

	if ((flags & CIFS_HAS_CREDITS) == 0) {
		rc = wait_for_free_request(server, flags, &instance);
		if (rc)
			return rc;
		credits.value = 1;
		credits.instance = instance;
	} else
		instance = exist_credits->instance;

	cifs_server_lock(server);

	 
	if (instance != server->reconnect_instance) {
		cifs_server_unlock(server);
		add_credits_and_wake_if(server, &credits, optype);
		return -EAGAIN;
	}

	mid = server->ops->setup_async_request(server, rqst);
	if (IS_ERR(mid)) {
		cifs_server_unlock(server);
		add_credits_and_wake_if(server, &credits, optype);
		return PTR_ERR(mid);
	}

	mid->receive = receive;
	mid->callback = callback;
	mid->callback_data = cbdata;
	mid->handle = handle;
	mid->mid_state = MID_REQUEST_SUBMITTED;

	 
	spin_lock(&server->mid_lock);
	list_add_tail(&mid->qhead, &server->pending_mid_q);
	spin_unlock(&server->mid_lock);

	 
	cifs_save_when_sent(mid);
	rc = smb_send_rqst(server, 1, rqst, flags);

	if (rc < 0) {
		revert_current_mid(server, mid->credits);
		server->sequence_number -= 2;
		delete_mid(mid);
	}

	cifs_server_unlock(server);

	if (rc == 0)
		return 0;

	add_credits_and_wake_if(server, &credits, optype);
	return rc;
}

 
int
SendReceiveNoRsp(const unsigned int xid, struct cifs_ses *ses,
		 char *in_buf, int flags)
{
	int rc;
	struct kvec iov[1];
	struct kvec rsp_iov;
	int resp_buf_type;

	iov[0].iov_base = in_buf;
	iov[0].iov_len = get_rfc1002_length(in_buf) + 4;
	flags |= CIFS_NO_RSP_BUF;
	rc = SendReceive2(xid, ses, iov, 1, &resp_buf_type, flags, &rsp_iov);
	cifs_dbg(NOISY, "SendRcvNoRsp flags %d rc %d\n", flags, rc);

	return rc;
}

static int
cifs_sync_mid_result(struct mid_q_entry *mid, struct TCP_Server_Info *server)
{
	int rc = 0;

	cifs_dbg(FYI, "%s: cmd=%d mid=%llu state=%d\n",
		 __func__, le16_to_cpu(mid->command), mid->mid, mid->mid_state);

	spin_lock(&server->mid_lock);
	switch (mid->mid_state) {
	case MID_RESPONSE_READY:
		spin_unlock(&server->mid_lock);
		return rc;
	case MID_RETRY_NEEDED:
		rc = -EAGAIN;
		break;
	case MID_RESPONSE_MALFORMED:
		rc = -EIO;
		break;
	case MID_SHUTDOWN:
		rc = -EHOSTDOWN;
		break;
	default:
		if (!(mid->mid_flags & MID_DELETED)) {
			list_del_init(&mid->qhead);
			mid->mid_flags |= MID_DELETED;
		}
		cifs_server_dbg(VFS, "%s: invalid mid state mid=%llu state=%d\n",
			 __func__, mid->mid, mid->mid_state);
		rc = -EIO;
	}
	spin_unlock(&server->mid_lock);

	release_mid(mid);
	return rc;
}

static inline int
send_cancel(struct TCP_Server_Info *server, struct smb_rqst *rqst,
	    struct mid_q_entry *mid)
{
	return server->ops->send_cancel ?
				server->ops->send_cancel(server, rqst, mid) : 0;
}

int
cifs_check_receive(struct mid_q_entry *mid, struct TCP_Server_Info *server,
		   bool log_error)
{
	unsigned int len = get_rfc1002_length(mid->resp_buf) + 4;

	dump_smb(mid->resp_buf, min_t(u32, 92, len));

	 
	if (server->sign) {
		struct kvec iov[2];
		int rc = 0;
		struct smb_rqst rqst = { .rq_iov = iov,
					 .rq_nvec = 2 };

		iov[0].iov_base = mid->resp_buf;
		iov[0].iov_len = 4;
		iov[1].iov_base = (char *)mid->resp_buf + 4;
		iov[1].iov_len = len - 4;
		 
		rc = cifs_verify_signature(&rqst, server,
					   mid->sequence_number);
		if (rc)
			cifs_server_dbg(VFS, "SMB signature verification returned error = %d\n",
				 rc);
	}

	 
	return map_and_check_smb_error(mid, log_error);
}

struct mid_q_entry *
cifs_setup_request(struct cifs_ses *ses, struct TCP_Server_Info *ignored,
		   struct smb_rqst *rqst)
{
	int rc;
	struct smb_hdr *hdr = (struct smb_hdr *)rqst->rq_iov[0].iov_base;
	struct mid_q_entry *mid;

	if (rqst->rq_iov[0].iov_len != 4 ||
	    rqst->rq_iov[0].iov_base + 4 != rqst->rq_iov[1].iov_base)
		return ERR_PTR(-EIO);

	rc = allocate_mid(ses, hdr, &mid);
	if (rc)
		return ERR_PTR(rc);
	rc = cifs_sign_rqst(rqst, ses->server, &mid->sequence_number);
	if (rc) {
		delete_mid(mid);
		return ERR_PTR(rc);
	}
	return mid;
}

static void
cifs_compound_callback(struct mid_q_entry *mid)
{
	struct TCP_Server_Info *server = mid->server;
	struct cifs_credits credits;

	credits.value = server->ops->get_credits(mid);
	credits.instance = server->reconnect_instance;

	add_credits(server, &credits, mid->optype);

	if (mid->mid_state == MID_RESPONSE_RECEIVED)
		mid->mid_state = MID_RESPONSE_READY;
}

static void
cifs_compound_last_callback(struct mid_q_entry *mid)
{
	cifs_compound_callback(mid);
	cifs_wake_up_task(mid);
}

static void
cifs_cancelled_callback(struct mid_q_entry *mid)
{
	cifs_compound_callback(mid);
	release_mid(mid);
}

 
struct TCP_Server_Info *cifs_pick_channel(struct cifs_ses *ses)
{
	uint index = 0;
	unsigned int min_in_flight = UINT_MAX, max_in_flight = 0;
	struct TCP_Server_Info *server = NULL;
	int i;

	if (!ses)
		return NULL;

	spin_lock(&ses->chan_lock);
	for (i = 0; i < ses->chan_count; i++) {
		server = ses->chans[i].server;
		if (!server)
			continue;

		 
		if (server->in_flight < min_in_flight) {
			min_in_flight = server->in_flight;
			index = i;
		}
		if (server->in_flight > max_in_flight)
			max_in_flight = server->in_flight;
	}

	 
	if (min_in_flight == max_in_flight) {
		index = (uint)atomic_inc_return(&ses->chan_seq);
		index %= ses->chan_count;
	}
	spin_unlock(&ses->chan_lock);

	return ses->chans[index].server;
}

int
compound_send_recv(const unsigned int xid, struct cifs_ses *ses,
		   struct TCP_Server_Info *server,
		   const int flags, const int num_rqst, struct smb_rqst *rqst,
		   int *resp_buf_type, struct kvec *resp_iov)
{
	int i, j, optype, rc = 0;
	struct mid_q_entry *midQ[MAX_COMPOUND];
	bool cancelled_mid[MAX_COMPOUND] = {false};
	struct cifs_credits credits[MAX_COMPOUND] = {
		{ .value = 0, .instance = 0 }
	};
	unsigned int instance;
	char *buf;

	optype = flags & CIFS_OP_MASK;

	for (i = 0; i < num_rqst; i++)
		resp_buf_type[i] = CIFS_NO_BUFFER;   

	if (!ses || !ses->server || !server) {
		cifs_dbg(VFS, "Null session\n");
		return -EIO;
	}

	spin_lock(&server->srv_lock);
	if (server->tcpStatus == CifsExiting) {
		spin_unlock(&server->srv_lock);
		return -ENOENT;
	}
	spin_unlock(&server->srv_lock);

	 
	rc = wait_for_compound_request(server, num_rqst, flags,
				       &instance);
	if (rc)
		return rc;

	for (i = 0; i < num_rqst; i++) {
		credits[i].value = 1;
		credits[i].instance = instance;
	}

	 

	cifs_server_lock(server);

	 
	if (instance != server->reconnect_instance) {
		cifs_server_unlock(server);
		for (j = 0; j < num_rqst; j++)
			add_credits(server, &credits[j], optype);
		return -EAGAIN;
	}

	for (i = 0; i < num_rqst; i++) {
		midQ[i] = server->ops->setup_request(ses, server, &rqst[i]);
		if (IS_ERR(midQ[i])) {
			revert_current_mid(server, i);
			for (j = 0; j < i; j++)
				delete_mid(midQ[j]);
			cifs_server_unlock(server);

			 
			for (j = 0; j < num_rqst; j++)
				add_credits(server, &credits[j], optype);
			return PTR_ERR(midQ[i]);
		}

		midQ[i]->mid_state = MID_REQUEST_SUBMITTED;
		midQ[i]->optype = optype;
		 
		if (i < num_rqst - 1)
			midQ[i]->callback = cifs_compound_callback;
		else
			midQ[i]->callback = cifs_compound_last_callback;
	}
	rc = smb_send_rqst(server, num_rqst, rqst, flags);

	for (i = 0; i < num_rqst; i++)
		cifs_save_when_sent(midQ[i]);

	if (rc < 0) {
		revert_current_mid(server, num_rqst);
		server->sequence_number -= 2;
	}

	cifs_server_unlock(server);

	 
	if (rc < 0 || (flags & CIFS_NO_SRV_RSP)) {
		for (i = 0; i < num_rqst; i++)
			add_credits(server, &credits[i], optype);
		goto out;
	}

	 

	 
	spin_lock(&ses->ses_lock);
	if ((ses->ses_status == SES_NEW) || (optype & CIFS_NEG_OP) || (optype & CIFS_SESS_OP)) {
		spin_unlock(&ses->ses_lock);

		cifs_server_lock(server);
		smb311_update_preauth_hash(ses, server, rqst[0].rq_iov, rqst[0].rq_nvec);
		cifs_server_unlock(server);

		spin_lock(&ses->ses_lock);
	}
	spin_unlock(&ses->ses_lock);

	for (i = 0; i < num_rqst; i++) {
		rc = wait_for_response(server, midQ[i]);
		if (rc != 0)
			break;
	}
	if (rc != 0) {
		for (; i < num_rqst; i++) {
			cifs_server_dbg(FYI, "Cancelling wait for mid %llu cmd: %d\n",
				 midQ[i]->mid, le16_to_cpu(midQ[i]->command));
			send_cancel(server, &rqst[i], midQ[i]);
			spin_lock(&server->mid_lock);
			midQ[i]->mid_flags |= MID_WAIT_CANCELLED;
			if (midQ[i]->mid_state == MID_REQUEST_SUBMITTED ||
			    midQ[i]->mid_state == MID_RESPONSE_RECEIVED) {
				midQ[i]->callback = cifs_cancelled_callback;
				cancelled_mid[i] = true;
				credits[i].value = 0;
			}
			spin_unlock(&server->mid_lock);
		}
	}

	for (i = 0; i < num_rqst; i++) {
		if (rc < 0)
			goto out;

		rc = cifs_sync_mid_result(midQ[i], server);
		if (rc != 0) {
			 
			cancelled_mid[i] = true;
			goto out;
		}

		if (!midQ[i]->resp_buf ||
		    midQ[i]->mid_state != MID_RESPONSE_READY) {
			rc = -EIO;
			cifs_dbg(FYI, "Bad MID state?\n");
			goto out;
		}

		buf = (char *)midQ[i]->resp_buf;
		resp_iov[i].iov_base = buf;
		resp_iov[i].iov_len = midQ[i]->resp_buf_size +
			HEADER_PREAMBLE_SIZE(server);

		if (midQ[i]->large_buf)
			resp_buf_type[i] = CIFS_LARGE_BUFFER;
		else
			resp_buf_type[i] = CIFS_SMALL_BUFFER;

		rc = server->ops->check_receive(midQ[i], server,
						     flags & CIFS_LOG_ERROR);

		 
		if ((flags & CIFS_NO_RSP_BUF) == 0)
			midQ[i]->resp_buf = NULL;

	}

	 
	spin_lock(&ses->ses_lock);
	if ((ses->ses_status == SES_NEW) || (optype & CIFS_NEG_OP) || (optype & CIFS_SESS_OP)) {
		struct kvec iov = {
			.iov_base = resp_iov[0].iov_base,
			.iov_len = resp_iov[0].iov_len
		};
		spin_unlock(&ses->ses_lock);
		cifs_server_lock(server);
		smb311_update_preauth_hash(ses, server, &iov, 1);
		cifs_server_unlock(server);
		spin_lock(&ses->ses_lock);
	}
	spin_unlock(&ses->ses_lock);

out:
	 
	for (i = 0; i < num_rqst; i++) {
		if (!cancelled_mid[i])
			delete_mid(midQ[i]);
	}

	return rc;
}

int
cifs_send_recv(const unsigned int xid, struct cifs_ses *ses,
	       struct TCP_Server_Info *server,
	       struct smb_rqst *rqst, int *resp_buf_type, const int flags,
	       struct kvec *resp_iov)
{
	return compound_send_recv(xid, ses, server, flags, 1,
				  rqst, resp_buf_type, resp_iov);
}

int
SendReceive2(const unsigned int xid, struct cifs_ses *ses,
	     struct kvec *iov, int n_vec, int *resp_buf_type  ,
	     const int flags, struct kvec *resp_iov)
{
	struct smb_rqst rqst;
	struct kvec s_iov[CIFS_MAX_IOV_SIZE], *new_iov;
	int rc;

	if (n_vec + 1 > CIFS_MAX_IOV_SIZE) {
		new_iov = kmalloc_array(n_vec + 1, sizeof(struct kvec),
					GFP_KERNEL);
		if (!new_iov) {
			 
			*resp_buf_type = CIFS_NO_BUFFER;
			return -ENOMEM;
		}
	} else
		new_iov = s_iov;

	 
	memcpy(new_iov + 1, iov, (sizeof(struct kvec) * n_vec));

	new_iov[0].iov_base = new_iov[1].iov_base;
	new_iov[0].iov_len = 4;
	new_iov[1].iov_base += 4;
	new_iov[1].iov_len -= 4;

	memset(&rqst, 0, sizeof(struct smb_rqst));
	rqst.rq_iov = new_iov;
	rqst.rq_nvec = n_vec + 1;

	rc = cifs_send_recv(xid, ses, ses->server,
			    &rqst, resp_buf_type, flags, resp_iov);
	if (n_vec + 1 > CIFS_MAX_IOV_SIZE)
		kfree(new_iov);
	return rc;
}

int
SendReceive(const unsigned int xid, struct cifs_ses *ses,
	    struct smb_hdr *in_buf, struct smb_hdr *out_buf,
	    int *pbytes_returned, const int flags)
{
	int rc = 0;
	struct mid_q_entry *midQ;
	unsigned int len = be32_to_cpu(in_buf->smb_buf_length);
	struct kvec iov = { .iov_base = in_buf, .iov_len = len };
	struct smb_rqst rqst = { .rq_iov = &iov, .rq_nvec = 1 };
	struct cifs_credits credits = { .value = 1, .instance = 0 };
	struct TCP_Server_Info *server;

	if (ses == NULL) {
		cifs_dbg(VFS, "Null smb session\n");
		return -EIO;
	}
	server = ses->server;
	if (server == NULL) {
		cifs_dbg(VFS, "Null tcp session\n");
		return -EIO;
	}

	spin_lock(&server->srv_lock);
	if (server->tcpStatus == CifsExiting) {
		spin_unlock(&server->srv_lock);
		return -ENOENT;
	}
	spin_unlock(&server->srv_lock);

	 

	if (len > CIFSMaxBufSize + MAX_CIFS_HDR_SIZE - 4) {
		cifs_server_dbg(VFS, "Invalid length, greater than maximum frame, %d\n",
				len);
		return -EIO;
	}

	rc = wait_for_free_request(server, flags, &credits.instance);
	if (rc)
		return rc;

	 

	cifs_server_lock(server);

	rc = allocate_mid(ses, in_buf, &midQ);
	if (rc) {
		cifs_server_unlock(server);
		 
		add_credits(server, &credits, 0);
		return rc;
	}

	rc = cifs_sign_smb(in_buf, server, &midQ->sequence_number);
	if (rc) {
		cifs_server_unlock(server);
		goto out;
	}

	midQ->mid_state = MID_REQUEST_SUBMITTED;

	rc = smb_send(server, in_buf, len);
	cifs_save_when_sent(midQ);

	if (rc < 0)
		server->sequence_number -= 2;

	cifs_server_unlock(server);

	if (rc < 0)
		goto out;

	rc = wait_for_response(server, midQ);
	if (rc != 0) {
		send_cancel(server, &rqst, midQ);
		spin_lock(&server->mid_lock);
		if (midQ->mid_state == MID_REQUEST_SUBMITTED ||
		    midQ->mid_state == MID_RESPONSE_RECEIVED) {
			 
			midQ->callback = release_mid;
			spin_unlock(&server->mid_lock);
			add_credits(server, &credits, 0);
			return rc;
		}
		spin_unlock(&server->mid_lock);
	}

	rc = cifs_sync_mid_result(midQ, server);
	if (rc != 0) {
		add_credits(server, &credits, 0);
		return rc;
	}

	if (!midQ->resp_buf || !out_buf ||
	    midQ->mid_state != MID_RESPONSE_READY) {
		rc = -EIO;
		cifs_server_dbg(VFS, "Bad MID state?\n");
		goto out;
	}

	*pbytes_returned = get_rfc1002_length(midQ->resp_buf);
	memcpy(out_buf, midQ->resp_buf, *pbytes_returned + 4);
	rc = cifs_check_receive(midQ, server, 0);
out:
	delete_mid(midQ);
	add_credits(server, &credits, 0);

	return rc;
}

 

static int
send_lock_cancel(const unsigned int xid, struct cifs_tcon *tcon,
			struct smb_hdr *in_buf,
			struct smb_hdr *out_buf)
{
	int bytes_returned;
	struct cifs_ses *ses = tcon->ses;
	LOCK_REQ *pSMB = (LOCK_REQ *)in_buf;

	 

	pSMB->LockType = LOCKING_ANDX_CANCEL_LOCK|LOCKING_ANDX_LARGE_FILES;
	pSMB->Timeout = 0;
	pSMB->hdr.Mid = get_next_mid(ses->server);

	return SendReceive(xid, ses, in_buf, out_buf,
			&bytes_returned, 0);
}

int
SendReceiveBlockingLock(const unsigned int xid, struct cifs_tcon *tcon,
	    struct smb_hdr *in_buf, struct smb_hdr *out_buf,
	    int *pbytes_returned)
{
	int rc = 0;
	int rstart = 0;
	struct mid_q_entry *midQ;
	struct cifs_ses *ses;
	unsigned int len = be32_to_cpu(in_buf->smb_buf_length);
	struct kvec iov = { .iov_base = in_buf, .iov_len = len };
	struct smb_rqst rqst = { .rq_iov = &iov, .rq_nvec = 1 };
	unsigned int instance;
	struct TCP_Server_Info *server;

	if (tcon == NULL || tcon->ses == NULL) {
		cifs_dbg(VFS, "Null smb session\n");
		return -EIO;
	}
	ses = tcon->ses;
	server = ses->server;

	if (server == NULL) {
		cifs_dbg(VFS, "Null tcp session\n");
		return -EIO;
	}

	spin_lock(&server->srv_lock);
	if (server->tcpStatus == CifsExiting) {
		spin_unlock(&server->srv_lock);
		return -ENOENT;
	}
	spin_unlock(&server->srv_lock);

	 

	if (len > CIFSMaxBufSize + MAX_CIFS_HDR_SIZE - 4) {
		cifs_tcon_dbg(VFS, "Invalid length, greater than maximum frame, %d\n",
			      len);
		return -EIO;
	}

	rc = wait_for_free_request(server, CIFS_BLOCKING_OP, &instance);
	if (rc)
		return rc;

	 

	cifs_server_lock(server);

	rc = allocate_mid(ses, in_buf, &midQ);
	if (rc) {
		cifs_server_unlock(server);
		return rc;
	}

	rc = cifs_sign_smb(in_buf, server, &midQ->sequence_number);
	if (rc) {
		delete_mid(midQ);
		cifs_server_unlock(server);
		return rc;
	}

	midQ->mid_state = MID_REQUEST_SUBMITTED;
	rc = smb_send(server, in_buf, len);
	cifs_save_when_sent(midQ);

	if (rc < 0)
		server->sequence_number -= 2;

	cifs_server_unlock(server);

	if (rc < 0) {
		delete_mid(midQ);
		return rc;
	}

	 
	rc = wait_event_interruptible(server->response_q,
		(!(midQ->mid_state == MID_REQUEST_SUBMITTED ||
		   midQ->mid_state == MID_RESPONSE_RECEIVED)) ||
		((server->tcpStatus != CifsGood) &&
		 (server->tcpStatus != CifsNew)));

	 
	spin_lock(&server->srv_lock);
	if ((rc == -ERESTARTSYS) &&
		(midQ->mid_state == MID_REQUEST_SUBMITTED ||
		 midQ->mid_state == MID_RESPONSE_RECEIVED) &&
		((server->tcpStatus == CifsGood) ||
		 (server->tcpStatus == CifsNew))) {
		spin_unlock(&server->srv_lock);

		if (in_buf->Command == SMB_COM_TRANSACTION2) {
			 
			rc = send_cancel(server, &rqst, midQ);
			if (rc) {
				delete_mid(midQ);
				return rc;
			}
		} else {
			 

			rc = send_lock_cancel(xid, tcon, in_buf, out_buf);

			 
			if (rc && rc != -ENOLCK) {
				delete_mid(midQ);
				return rc;
			}
		}

		rc = wait_for_response(server, midQ);
		if (rc) {
			send_cancel(server, &rqst, midQ);
			spin_lock(&server->mid_lock);
			if (midQ->mid_state == MID_REQUEST_SUBMITTED ||
			    midQ->mid_state == MID_RESPONSE_RECEIVED) {
				 
				midQ->callback = release_mid;
				spin_unlock(&server->mid_lock);
				return rc;
			}
			spin_unlock(&server->mid_lock);
		}

		 
		rstart = 1;
		spin_lock(&server->srv_lock);
	}
	spin_unlock(&server->srv_lock);

	rc = cifs_sync_mid_result(midQ, server);
	if (rc != 0)
		return rc;

	 
	if (out_buf == NULL || midQ->mid_state != MID_RESPONSE_READY) {
		rc = -EIO;
		cifs_tcon_dbg(VFS, "Bad MID state?\n");
		goto out;
	}

	*pbytes_returned = get_rfc1002_length(midQ->resp_buf);
	memcpy(out_buf, midQ->resp_buf, *pbytes_returned + 4);
	rc = cifs_check_receive(midQ, server, 0);
out:
	delete_mid(midQ);
	if (rstart && rc == -EACCES)
		return -ERESTARTSYS;
	return rc;
}

 
int
cifs_discard_remaining_data(struct TCP_Server_Info *server)
{
	unsigned int rfclen = server->pdu_size;
	size_t remaining = rfclen + HEADER_PREAMBLE_SIZE(server) -
		server->total_read;

	while (remaining > 0) {
		ssize_t length;

		length = cifs_discard_from_socket(server,
				min_t(size_t, remaining,
				      CIFSMaxBufSize + MAX_HEADER_SIZE(server)));
		if (length < 0)
			return length;
		server->total_read += length;
		remaining -= length;
	}

	return 0;
}

static int
__cifs_readv_discard(struct TCP_Server_Info *server, struct mid_q_entry *mid,
		     bool malformed)
{
	int length;

	length = cifs_discard_remaining_data(server);
	dequeue_mid(mid, malformed);
	mid->resp_buf = server->smallbuf;
	server->smallbuf = NULL;
	return length;
}

static int
cifs_readv_discard(struct TCP_Server_Info *server, struct mid_q_entry *mid)
{
	struct cifs_readdata *rdata = mid->callback_data;

	return  __cifs_readv_discard(server, mid, rdata->result);
}

int
cifs_readv_receive(struct TCP_Server_Info *server, struct mid_q_entry *mid)
{
	int length, len;
	unsigned int data_offset, data_len;
	struct cifs_readdata *rdata = mid->callback_data;
	char *buf = server->smallbuf;
	unsigned int buflen = server->pdu_size + HEADER_PREAMBLE_SIZE(server);
	bool use_rdma_mr = false;

	cifs_dbg(FYI, "%s: mid=%llu offset=%llu bytes=%u\n",
		 __func__, mid->mid, rdata->offset, rdata->bytes);

	 
	len = min_t(unsigned int, buflen, server->vals->read_rsp_size) -
							HEADER_SIZE(server) + 1;

	length = cifs_read_from_socket(server,
				       buf + HEADER_SIZE(server) - 1, len);
	if (length < 0)
		return length;
	server->total_read += length;

	if (server->ops->is_session_expired &&
	    server->ops->is_session_expired(buf)) {
		cifs_reconnect(server, true);
		return -1;
	}

	if (server->ops->is_status_pending &&
	    server->ops->is_status_pending(buf, server)) {
		cifs_discard_remaining_data(server);
		return -1;
	}

	 
	rdata->iov[0].iov_base = buf;
	rdata->iov[0].iov_len = HEADER_PREAMBLE_SIZE(server);
	rdata->iov[1].iov_base = buf + HEADER_PREAMBLE_SIZE(server);
	rdata->iov[1].iov_len =
		server->total_read - HEADER_PREAMBLE_SIZE(server);
	cifs_dbg(FYI, "0: iov_base=%p iov_len=%zu\n",
		 rdata->iov[0].iov_base, rdata->iov[0].iov_len);
	cifs_dbg(FYI, "1: iov_base=%p iov_len=%zu\n",
		 rdata->iov[1].iov_base, rdata->iov[1].iov_len);

	 
	rdata->result = server->ops->map_error(buf, false);
	if (rdata->result != 0) {
		cifs_dbg(FYI, "%s: server returned error %d\n",
			 __func__, rdata->result);
		 
		return __cifs_readv_discard(server, mid, false);
	}

	 
	if (server->total_read < server->vals->read_rsp_size) {
		cifs_dbg(FYI, "%s: server returned short header. got=%u expected=%zu\n",
			 __func__, server->total_read,
			 server->vals->read_rsp_size);
		rdata->result = -EIO;
		return cifs_readv_discard(server, mid);
	}

	data_offset = server->ops->read_data_offset(buf) +
		HEADER_PREAMBLE_SIZE(server);
	if (data_offset < server->total_read) {
		 
		cifs_dbg(FYI, "%s: data offset (%u) inside read response header\n",
			 __func__, data_offset);
		data_offset = server->total_read;
	} else if (data_offset > MAX_CIFS_SMALL_BUFFER_SIZE) {
		 
		cifs_dbg(FYI, "%s: data offset (%u) beyond end of smallbuf\n",
			 __func__, data_offset);
		rdata->result = -EIO;
		return cifs_readv_discard(server, mid);
	}

	cifs_dbg(FYI, "%s: total_read=%u data_offset=%u\n",
		 __func__, server->total_read, data_offset);

	len = data_offset - server->total_read;
	if (len > 0) {
		 
		length = cifs_read_from_socket(server,
					       buf + server->total_read, len);
		if (length < 0)
			return length;
		server->total_read += length;
	}

	 
#ifdef CONFIG_CIFS_SMB_DIRECT
	use_rdma_mr = rdata->mr;
#endif
	data_len = server->ops->read_data_length(buf, use_rdma_mr);
	if (!use_rdma_mr && (data_offset + data_len > buflen)) {
		 
		rdata->result = -EIO;
		return cifs_readv_discard(server, mid);
	}

#ifdef CONFIG_CIFS_SMB_DIRECT
	if (rdata->mr)
		length = data_len;  
	else
#endif
		length = cifs_read_iter_from_socket(server, &rdata->iter,
						    data_len);
	if (length > 0)
		rdata->got_bytes += length;
	server->total_read += length;

	cifs_dbg(FYI, "total_read=%u buflen=%u remaining=%u\n",
		 server->total_read, buflen, data_len);

	 
	if (server->total_read < buflen)
		return cifs_readv_discard(server, mid);

	dequeue_mid(mid, false);
	mid->resp_buf = server->smallbuf;
	server->smallbuf = NULL;
	return length;
}
