
 

 

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/smp.h>

#include <linux/sunrpc/addr.h>
#include <linux/sunrpc/svc_rdma.h>

#include "xprt_rdma.h"
#include <trace/events/rpcrdma.h>

 

static unsigned int xprt_rdma_slot_table_entries = RPCRDMA_DEF_SLOT_TABLE;
unsigned int xprt_rdma_max_inline_read = RPCRDMA_DEF_INLINE;
unsigned int xprt_rdma_max_inline_write = RPCRDMA_DEF_INLINE;
unsigned int xprt_rdma_memreg_strategy		= RPCRDMA_FRWR;
int xprt_rdma_pad_optimize;
static struct xprt_class xprt_rdma;

#if IS_ENABLED(CONFIG_SUNRPC_DEBUG)

static unsigned int min_slot_table_size = RPCRDMA_MIN_SLOT_TABLE;
static unsigned int max_slot_table_size = RPCRDMA_MAX_SLOT_TABLE;
static unsigned int min_inline_size = RPCRDMA_MIN_INLINE;
static unsigned int max_inline_size = RPCRDMA_MAX_INLINE;
static unsigned int max_padding = PAGE_SIZE;
static unsigned int min_memreg = RPCRDMA_BOUNCEBUFFERS;
static unsigned int max_memreg = RPCRDMA_LAST - 1;
static unsigned int dummy;

static struct ctl_table_header *sunrpc_table_header;

static struct ctl_table xr_tunables_table[] = {
	{
		.procname	= "rdma_slot_table_entries",
		.data		= &xprt_rdma_slot_table_entries,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_slot_table_size,
		.extra2		= &max_slot_table_size
	},
	{
		.procname	= "rdma_max_inline_read",
		.data		= &xprt_rdma_max_inline_read,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_inline_size,
		.extra2		= &max_inline_size,
	},
	{
		.procname	= "rdma_max_inline_write",
		.data		= &xprt_rdma_max_inline_write,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_inline_size,
		.extra2		= &max_inline_size,
	},
	{
		.procname	= "rdma_inline_write_padding",
		.data		= &dummy,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &max_padding,
	},
	{
		.procname	= "rdma_memreg_strategy",
		.data		= &xprt_rdma_memreg_strategy,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_memreg,
		.extra2		= &max_memreg,
	},
	{
		.procname	= "rdma_pad_optimize",
		.data		= &xprt_rdma_pad_optimize,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{ },
};

#endif

static const struct rpc_xprt_ops xprt_rdma_procs;

static void
xprt_rdma_format_addresses4(struct rpc_xprt *xprt, struct sockaddr *sap)
{
	struct sockaddr_in *sin = (struct sockaddr_in *)sap;
	char buf[20];

	snprintf(buf, sizeof(buf), "%08x", ntohl(sin->sin_addr.s_addr));
	xprt->address_strings[RPC_DISPLAY_HEX_ADDR] = kstrdup(buf, GFP_KERNEL);

	xprt->address_strings[RPC_DISPLAY_NETID] = RPCBIND_NETID_RDMA;
}

static void
xprt_rdma_format_addresses6(struct rpc_xprt *xprt, struct sockaddr *sap)
{
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sap;
	char buf[40];

	snprintf(buf, sizeof(buf), "%pi6", &sin6->sin6_addr);
	xprt->address_strings[RPC_DISPLAY_HEX_ADDR] = kstrdup(buf, GFP_KERNEL);

	xprt->address_strings[RPC_DISPLAY_NETID] = RPCBIND_NETID_RDMA6;
}

void
xprt_rdma_format_addresses(struct rpc_xprt *xprt, struct sockaddr *sap)
{
	char buf[128];

	switch (sap->sa_family) {
	case AF_INET:
		xprt_rdma_format_addresses4(xprt, sap);
		break;
	case AF_INET6:
		xprt_rdma_format_addresses6(xprt, sap);
		break;
	default:
		pr_err("rpcrdma: Unrecognized address family\n");
		return;
	}

	(void)rpc_ntop(sap, buf, sizeof(buf));
	xprt->address_strings[RPC_DISPLAY_ADDR] = kstrdup(buf, GFP_KERNEL);

	snprintf(buf, sizeof(buf), "%u", rpc_get_port(sap));
	xprt->address_strings[RPC_DISPLAY_PORT] = kstrdup(buf, GFP_KERNEL);

	snprintf(buf, sizeof(buf), "%4hx", rpc_get_port(sap));
	xprt->address_strings[RPC_DISPLAY_HEX_PORT] = kstrdup(buf, GFP_KERNEL);

	xprt->address_strings[RPC_DISPLAY_PROTO] = "rdma";
}

void
xprt_rdma_free_addresses(struct rpc_xprt *xprt)
{
	unsigned int i;

	for (i = 0; i < RPC_DISPLAY_MAX; i++)
		switch (i) {
		case RPC_DISPLAY_PROTO:
		case RPC_DISPLAY_NETID:
			continue;
		default:
			kfree(xprt->address_strings[i]);
		}
}

 
static void
xprt_rdma_connect_worker(struct work_struct *work)
{
	struct rpcrdma_xprt *r_xprt = container_of(work, struct rpcrdma_xprt,
						   rx_connect_worker.work);
	struct rpc_xprt *xprt = &r_xprt->rx_xprt;
	unsigned int pflags = current->flags;
	int rc;

	if (atomic_read(&xprt->swapper))
		current->flags |= PF_MEMALLOC;
	rc = rpcrdma_xprt_connect(r_xprt);
	xprt_clear_connecting(xprt);
	if (!rc) {
		xprt->connect_cookie++;
		xprt->stat.connect_count++;
		xprt->stat.connect_time += (long)jiffies -
					   xprt->stat.connect_start;
		xprt_set_connected(xprt);
		rc = -EAGAIN;
	} else
		rpcrdma_xprt_disconnect(r_xprt);
	xprt_unlock_connect(xprt, r_xprt);
	xprt_wake_pending_tasks(xprt, rc);
	current_restore_flags(pflags, PF_MEMALLOC);
}

 
static void
xprt_rdma_inject_disconnect(struct rpc_xprt *xprt)
{
	struct rpcrdma_xprt *r_xprt = rpcx_to_rdmax(xprt);

	trace_xprtrdma_op_inject_dsc(r_xprt);
	rdma_disconnect(r_xprt->rx_ep->re_id);
}

 
static void
xprt_rdma_destroy(struct rpc_xprt *xprt)
{
	struct rpcrdma_xprt *r_xprt = rpcx_to_rdmax(xprt);

	cancel_delayed_work_sync(&r_xprt->rx_connect_worker);

	rpcrdma_xprt_disconnect(r_xprt);
	rpcrdma_buffer_destroy(&r_xprt->rx_buf);

	xprt_rdma_free_addresses(xprt);
	xprt_free(xprt);

	module_put(THIS_MODULE);
}

 
static const struct rpc_timeout xprt_rdma_default_timeout = {
	.to_initval = 60 * HZ,
	.to_maxval = 60 * HZ,
};

 
static struct rpc_xprt *
xprt_setup_rdma(struct xprt_create *args)
{
	struct rpc_xprt *xprt;
	struct rpcrdma_xprt *new_xprt;
	struct sockaddr *sap;
	int rc;

	if (args->addrlen > sizeof(xprt->addr))
		return ERR_PTR(-EBADF);

	if (!try_module_get(THIS_MODULE))
		return ERR_PTR(-EIO);

	xprt = xprt_alloc(args->net, sizeof(struct rpcrdma_xprt), 0,
			  xprt_rdma_slot_table_entries);
	if (!xprt) {
		module_put(THIS_MODULE);
		return ERR_PTR(-ENOMEM);
	}

	xprt->timeout = &xprt_rdma_default_timeout;
	xprt->connect_timeout = xprt->timeout->to_initval;
	xprt->max_reconnect_timeout = xprt->timeout->to_maxval;
	xprt->bind_timeout = RPCRDMA_BIND_TO;
	xprt->reestablish_timeout = RPCRDMA_INIT_REEST_TO;
	xprt->idle_timeout = RPCRDMA_IDLE_DISC_TO;

	xprt->resvport = 0;		 
	xprt->ops = &xprt_rdma_procs;

	 
	sap = args->dstaddr;

	 
	xprt->prot = IPPROTO_TCP;
	xprt->xprt_class = &xprt_rdma;
	xprt->addrlen = args->addrlen;
	memcpy(&xprt->addr, sap, xprt->addrlen);

	if (rpc_get_port(sap))
		xprt_set_bound(xprt);
	xprt_rdma_format_addresses(xprt, sap);

	new_xprt = rpcx_to_rdmax(xprt);
	rc = rpcrdma_buffer_create(new_xprt);
	if (rc) {
		xprt_rdma_free_addresses(xprt);
		xprt_free(xprt);
		module_put(THIS_MODULE);
		return ERR_PTR(rc);
	}

	INIT_DELAYED_WORK(&new_xprt->rx_connect_worker,
			  xprt_rdma_connect_worker);

	xprt->max_payload = RPCRDMA_MAX_DATA_SEGS << PAGE_SHIFT;

	return xprt;
}

 
void xprt_rdma_close(struct rpc_xprt *xprt)
{
	struct rpcrdma_xprt *r_xprt = rpcx_to_rdmax(xprt);

	rpcrdma_xprt_disconnect(r_xprt);

	xprt->reestablish_timeout = 0;
	++xprt->connect_cookie;
	xprt_disconnect_done(xprt);
}

 
static void
xprt_rdma_set_port(struct rpc_xprt *xprt, u16 port)
{
	struct sockaddr *sap = (struct sockaddr *)&xprt->addr;
	char buf[8];

	rpc_set_port(sap, port);

	kfree(xprt->address_strings[RPC_DISPLAY_PORT]);
	snprintf(buf, sizeof(buf), "%u", port);
	xprt->address_strings[RPC_DISPLAY_PORT] = kstrdup(buf, GFP_KERNEL);

	kfree(xprt->address_strings[RPC_DISPLAY_HEX_PORT]);
	snprintf(buf, sizeof(buf), "%4hx", port);
	xprt->address_strings[RPC_DISPLAY_HEX_PORT] = kstrdup(buf, GFP_KERNEL);
}

 
static void
xprt_rdma_timer(struct rpc_xprt *xprt, struct rpc_task *task)
{
	xprt_force_disconnect(xprt);
}

 
static void xprt_rdma_set_connect_timeout(struct rpc_xprt *xprt,
					  unsigned long connect_timeout,
					  unsigned long reconnect_timeout)
{
	struct rpcrdma_xprt *r_xprt = rpcx_to_rdmax(xprt);

	trace_xprtrdma_op_set_cto(r_xprt, connect_timeout, reconnect_timeout);

	spin_lock(&xprt->transport_lock);

	if (connect_timeout < xprt->connect_timeout) {
		struct rpc_timeout to;
		unsigned long initval;

		to = *xprt->timeout;
		initval = connect_timeout;
		if (initval < RPCRDMA_INIT_REEST_TO << 1)
			initval = RPCRDMA_INIT_REEST_TO << 1;
		to.to_initval = initval;
		to.to_maxval = initval;
		r_xprt->rx_timeout = to;
		xprt->timeout = &r_xprt->rx_timeout;
		xprt->connect_timeout = connect_timeout;
	}

	if (reconnect_timeout < xprt->max_reconnect_timeout)
		xprt->max_reconnect_timeout = reconnect_timeout;

	spin_unlock(&xprt->transport_lock);
}

 
static void
xprt_rdma_connect(struct rpc_xprt *xprt, struct rpc_task *task)
{
	struct rpcrdma_xprt *r_xprt = rpcx_to_rdmax(xprt);
	struct rpcrdma_ep *ep = r_xprt->rx_ep;
	unsigned long delay;

	WARN_ON_ONCE(!xprt_lock_connect(xprt, task, r_xprt));

	delay = 0;
	if (ep && ep->re_connect_status != 0) {
		delay = xprt_reconnect_delay(xprt);
		xprt_reconnect_backoff(xprt, RPCRDMA_INIT_REEST_TO);
	}
	trace_xprtrdma_op_connect(r_xprt, delay);
	queue_delayed_work(system_long_wq, &r_xprt->rx_connect_worker, delay);
}

 
static void
xprt_rdma_alloc_slot(struct rpc_xprt *xprt, struct rpc_task *task)
{
	struct rpcrdma_xprt *r_xprt = rpcx_to_rdmax(xprt);
	struct rpcrdma_req *req;

	req = rpcrdma_buffer_get(&r_xprt->rx_buf);
	if (!req)
		goto out_sleep;
	task->tk_rqstp = &req->rl_slot;
	task->tk_status = 0;
	return;

out_sleep:
	task->tk_status = -ENOMEM;
	xprt_add_backlog(xprt, task);
}

 
static void
xprt_rdma_free_slot(struct rpc_xprt *xprt, struct rpc_rqst *rqst)
{
	struct rpcrdma_xprt *r_xprt =
		container_of(xprt, struct rpcrdma_xprt, rx_xprt);

	rpcrdma_reply_put(&r_xprt->rx_buf, rpcr_to_rdmar(rqst));
	if (!xprt_wake_up_backlog(xprt, rqst)) {
		memset(rqst, 0, sizeof(*rqst));
		rpcrdma_buffer_put(&r_xprt->rx_buf, rpcr_to_rdmar(rqst));
	}
}

static bool rpcrdma_check_regbuf(struct rpcrdma_xprt *r_xprt,
				 struct rpcrdma_regbuf *rb, size_t size,
				 gfp_t flags)
{
	if (unlikely(rdmab_length(rb) < size)) {
		if (!rpcrdma_regbuf_realloc(rb, size, flags))
			return false;
		r_xprt->rx_stats.hardway_register_count += size;
	}
	return true;
}

 
static int
xprt_rdma_allocate(struct rpc_task *task)
{
	struct rpc_rqst *rqst = task->tk_rqstp;
	struct rpcrdma_xprt *r_xprt = rpcx_to_rdmax(rqst->rq_xprt);
	struct rpcrdma_req *req = rpcr_to_rdmar(rqst);
	gfp_t flags = rpc_task_gfp_mask();

	if (!rpcrdma_check_regbuf(r_xprt, req->rl_sendbuf, rqst->rq_callsize,
				  flags))
		goto out_fail;
	if (!rpcrdma_check_regbuf(r_xprt, req->rl_recvbuf, rqst->rq_rcvsize,
				  flags))
		goto out_fail;

	rqst->rq_buffer = rdmab_data(req->rl_sendbuf);
	rqst->rq_rbuffer = rdmab_data(req->rl_recvbuf);
	return 0;

out_fail:
	return -ENOMEM;
}

 
static void
xprt_rdma_free(struct rpc_task *task)
{
	struct rpc_rqst *rqst = task->tk_rqstp;
	struct rpcrdma_req *req = rpcr_to_rdmar(rqst);

	if (unlikely(!list_empty(&req->rl_registered))) {
		trace_xprtrdma_mrs_zap(task);
		frwr_unmap_sync(rpcx_to_rdmax(rqst->rq_xprt), req);
	}

	 
}

 
static int
xprt_rdma_send_request(struct rpc_rqst *rqst)
{
	struct rpc_xprt *xprt = rqst->rq_xprt;
	struct rpcrdma_req *req = rpcr_to_rdmar(rqst);
	struct rpcrdma_xprt *r_xprt = rpcx_to_rdmax(xprt);
	int rc = 0;

#if defined(CONFIG_SUNRPC_BACKCHANNEL)
	if (unlikely(!rqst->rq_buffer))
		return xprt_rdma_bc_send_reply(rqst);
#endif	 

	if (!xprt_connected(xprt))
		return -ENOTCONN;

	if (!xprt_request_get_cong(xprt, rqst))
		return -EBADSLT;

	rc = rpcrdma_marshal_req(r_xprt, rqst);
	if (rc < 0)
		goto failed_marshal;

	 
	if (rqst->rq_connect_cookie == xprt->connect_cookie)
		goto drop_connection;
	rqst->rq_xtime = ktime_get();

	if (frwr_send(r_xprt, req))
		goto drop_connection;

	rqst->rq_xmit_bytes_sent += rqst->rq_snd_buf.len;

	 
	if (!rpc_reply_expected(rqst->rq_task))
		goto drop_connection;
	return 0;

failed_marshal:
	if (rc != -ENOTCONN)
		return rc;
drop_connection:
	xprt_rdma_close(xprt);
	return -ENOTCONN;
}

void xprt_rdma_print_stats(struct rpc_xprt *xprt, struct seq_file *seq)
{
	struct rpcrdma_xprt *r_xprt = rpcx_to_rdmax(xprt);
	long idle_time = 0;

	if (xprt_connected(xprt))
		idle_time = (long)(jiffies - xprt->last_used) / HZ;

	seq_puts(seq, "\txprt:\trdma ");
	seq_printf(seq, "%u %lu %lu %lu %ld %lu %lu %lu %llu %llu ",
		   0,	 
		   xprt->stat.bind_count,
		   xprt->stat.connect_count,
		   xprt->stat.connect_time / HZ,
		   idle_time,
		   xprt->stat.sends,
		   xprt->stat.recvs,
		   xprt->stat.bad_xids,
		   xprt->stat.req_u,
		   xprt->stat.bklog_u);
	seq_printf(seq, "%lu %lu %lu %llu %llu %llu %llu %lu %lu %lu %lu ",
		   r_xprt->rx_stats.read_chunk_count,
		   r_xprt->rx_stats.write_chunk_count,
		   r_xprt->rx_stats.reply_chunk_count,
		   r_xprt->rx_stats.total_rdma_request,
		   r_xprt->rx_stats.total_rdma_reply,
		   r_xprt->rx_stats.pullup_copy_count,
		   r_xprt->rx_stats.fixup_copy_count,
		   r_xprt->rx_stats.hardway_register_count,
		   r_xprt->rx_stats.failed_marshal_count,
		   r_xprt->rx_stats.bad_reply_count,
		   r_xprt->rx_stats.nomsg_call_count);
	seq_printf(seq, "%lu %lu %lu %lu %lu %lu\n",
		   r_xprt->rx_stats.mrs_recycled,
		   r_xprt->rx_stats.mrs_orphaned,
		   r_xprt->rx_stats.mrs_allocated,
		   r_xprt->rx_stats.local_inv_needed,
		   r_xprt->rx_stats.empty_sendctx_q,
		   r_xprt->rx_stats.reply_waits_for_send);
}

static int
xprt_rdma_enable_swap(struct rpc_xprt *xprt)
{
	return 0;
}

static void
xprt_rdma_disable_swap(struct rpc_xprt *xprt)
{
}

 

static const struct rpc_xprt_ops xprt_rdma_procs = {
	.reserve_xprt		= xprt_reserve_xprt_cong,
	.release_xprt		= xprt_release_xprt_cong,  
	.alloc_slot		= xprt_rdma_alloc_slot,
	.free_slot		= xprt_rdma_free_slot,
	.release_request	= xprt_release_rqst_cong,        
	.wait_for_reply_request	= xprt_wait_for_reply_request_def,  
	.timer			= xprt_rdma_timer,
	.rpcbind		= rpcb_getport_async,	 
	.set_port		= xprt_rdma_set_port,
	.connect		= xprt_rdma_connect,
	.buf_alloc		= xprt_rdma_allocate,
	.buf_free		= xprt_rdma_free,
	.send_request		= xprt_rdma_send_request,
	.close			= xprt_rdma_close,
	.destroy		= xprt_rdma_destroy,
	.set_connect_timeout	= xprt_rdma_set_connect_timeout,
	.print_stats		= xprt_rdma_print_stats,
	.enable_swap		= xprt_rdma_enable_swap,
	.disable_swap		= xprt_rdma_disable_swap,
	.inject_disconnect	= xprt_rdma_inject_disconnect,
#if defined(CONFIG_SUNRPC_BACKCHANNEL)
	.bc_setup		= xprt_rdma_bc_setup,
	.bc_maxpayload		= xprt_rdma_bc_maxpayload,
	.bc_num_slots		= xprt_rdma_bc_max_slots,
	.bc_free_rqst		= xprt_rdma_bc_free_rqst,
	.bc_destroy		= xprt_rdma_bc_destroy,
#endif
};

static struct xprt_class xprt_rdma = {
	.list			= LIST_HEAD_INIT(xprt_rdma.list),
	.name			= "rdma",
	.owner			= THIS_MODULE,
	.ident			= XPRT_TRANSPORT_RDMA,
	.setup			= xprt_setup_rdma,
	.netid			= { "rdma", "rdma6", "" },
};

void xprt_rdma_cleanup(void)
{
#if IS_ENABLED(CONFIG_SUNRPC_DEBUG)
	if (sunrpc_table_header) {
		unregister_sysctl_table(sunrpc_table_header);
		sunrpc_table_header = NULL;
	}
#endif

	xprt_unregister_transport(&xprt_rdma);
	xprt_unregister_transport(&xprt_rdma_bc);
}

int xprt_rdma_init(void)
{
	int rc;

	rc = xprt_register_transport(&xprt_rdma);
	if (rc)
		return rc;

	rc = xprt_register_transport(&xprt_rdma_bc);
	if (rc) {
		xprt_unregister_transport(&xprt_rdma);
		return rc;
	}

#if IS_ENABLED(CONFIG_SUNRPC_DEBUG)
	if (!sunrpc_table_header)
		sunrpc_table_header = register_sysctl("sunrpc", xr_tunables_table);
#endif
	return 0;
}
