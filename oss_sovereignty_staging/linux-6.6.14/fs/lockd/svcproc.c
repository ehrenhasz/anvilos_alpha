
 

#include <linux/types.h>
#include <linux/time.h>
#include <linux/lockd/lockd.h>
#include <linux/lockd/share.h>
#include <linux/sunrpc/svc_xprt.h>

#define NLMDBG_FACILITY		NLMDBG_CLIENT

#ifdef CONFIG_LOCKD_V4
static __be32
cast_to_nlm(__be32 status, u32 vers)
{
	 
	if (vers != 4){
		switch (status) {
		case nlm_granted:
		case nlm_lck_denied:
		case nlm_lck_denied_nolocks:
		case nlm_lck_blocked:
		case nlm_lck_denied_grace_period:
		case nlm_drop_reply:
			break;
		case nlm4_deadlock:
			status = nlm_lck_denied;
			break;
		default:
			status = nlm_lck_denied_nolocks;
		}
	}

	return (status);
}
#define	cast_status(status) (cast_to_nlm(status, rqstp->rq_vers))
#else
#define cast_status(status) (status)
#endif

 
static __be32
nlmsvc_retrieve_args(struct svc_rqst *rqstp, struct nlm_args *argp,
			struct nlm_host **hostp, struct nlm_file **filp)
{
	struct nlm_host		*host = NULL;
	struct nlm_file		*file = NULL;
	struct nlm_lock		*lock = &argp->lock;
	int			mode;
	__be32			error = 0;

	 
	if (!nlmsvc_ops)
		return nlm_lck_denied_nolocks;

	 
	if (!(host = nlmsvc_lookup_host(rqstp, lock->caller, lock->len))
	 || (argp->monitor && nsm_monitor(host) < 0))
		goto no_locks;
	*hostp = host;

	 
	if (filp != NULL) {
		error = cast_status(nlm_lookup_file(rqstp, &file, lock));
		if (error != 0)
			goto no_locks;
		*filp = file;

		 
		mode = lock_to_openmode(&lock->fl);
		lock->fl.fl_flags = FL_POSIX;
		lock->fl.fl_file  = file->f_file[mode];
		lock->fl.fl_pid = current->tgid;
		lock->fl.fl_lmops = &nlmsvc_lock_operations;
		nlmsvc_locks_init_private(&lock->fl, host, (pid_t)lock->svid);
		if (!lock->fl.fl_owner) {
			 
			nlmsvc_release_host(host);
			return nlm_lck_denied_nolocks;
		}
	}

	return 0;

no_locks:
	nlmsvc_release_host(host);
	if (error)
		return error;
	return nlm_lck_denied_nolocks;
}

 
static __be32
nlmsvc_proc_null(struct svc_rqst *rqstp)
{
	dprintk("lockd: NULL          called\n");
	return rpc_success;
}

 
static __be32
__nlmsvc_proc_test(struct svc_rqst *rqstp, struct nlm_res *resp)
{
	struct nlm_args *argp = rqstp->rq_argp;
	struct nlm_host	*host;
	struct nlm_file	*file;
	struct nlm_lockowner *test_owner;
	__be32 rc = rpc_success;

	dprintk("lockd: TEST          called\n");
	resp->cookie = argp->cookie;

	 
	if ((resp->status = nlmsvc_retrieve_args(rqstp, argp, &host, &file)))
		return resp->status == nlm_drop_reply ? rpc_drop_reply :rpc_success;

	test_owner = argp->lock.fl.fl_owner;

	 
	resp->status = cast_status(nlmsvc_testlock(rqstp, file, host, &argp->lock, &resp->lock, &resp->cookie));
	if (resp->status == nlm_drop_reply)
		rc = rpc_drop_reply;
	else
		dprintk("lockd: TEST          status %d vers %d\n",
			ntohl(resp->status), rqstp->rq_vers);

	nlmsvc_put_lockowner(test_owner);
	nlmsvc_release_host(host);
	nlm_release_file(file);
	return rc;
}

static __be32
nlmsvc_proc_test(struct svc_rqst *rqstp)
{
	return __nlmsvc_proc_test(rqstp, rqstp->rq_resp);
}

static __be32
__nlmsvc_proc_lock(struct svc_rqst *rqstp, struct nlm_res *resp)
{
	struct nlm_args *argp = rqstp->rq_argp;
	struct nlm_host	*host;
	struct nlm_file	*file;
	__be32 rc = rpc_success;

	dprintk("lockd: LOCK          called\n");

	resp->cookie = argp->cookie;

	 
	if ((resp->status = nlmsvc_retrieve_args(rqstp, argp, &host, &file)))
		return resp->status == nlm_drop_reply ? rpc_drop_reply :rpc_success;

#if 0
	 
	if (host->h_nsmstate && host->h_nsmstate != argp->state) {
		resp->status = nlm_lck_denied_nolocks;
	} else
#endif

	 
	resp->status = cast_status(nlmsvc_lock(rqstp, file, host, &argp->lock,
					       argp->block, &argp->cookie,
					       argp->reclaim));
	if (resp->status == nlm_drop_reply)
		rc = rpc_drop_reply;
	else
		dprintk("lockd: LOCK         status %d\n", ntohl(resp->status));

	nlmsvc_release_lockowner(&argp->lock);
	nlmsvc_release_host(host);
	nlm_release_file(file);
	return rc;
}

static __be32
nlmsvc_proc_lock(struct svc_rqst *rqstp)
{
	return __nlmsvc_proc_lock(rqstp, rqstp->rq_resp);
}

static __be32
__nlmsvc_proc_cancel(struct svc_rqst *rqstp, struct nlm_res *resp)
{
	struct nlm_args *argp = rqstp->rq_argp;
	struct nlm_host	*host;
	struct nlm_file	*file;
	struct net *net = SVC_NET(rqstp);

	dprintk("lockd: CANCEL        called\n");

	resp->cookie = argp->cookie;

	 
	if (locks_in_grace(net)) {
		resp->status = nlm_lck_denied_grace_period;
		return rpc_success;
	}

	 
	if ((resp->status = nlmsvc_retrieve_args(rqstp, argp, &host, &file)))
		return resp->status == nlm_drop_reply ? rpc_drop_reply :rpc_success;

	 
	resp->status = cast_status(nlmsvc_cancel_blocked(net, file, &argp->lock));

	dprintk("lockd: CANCEL        status %d\n", ntohl(resp->status));
	nlmsvc_release_lockowner(&argp->lock);
	nlmsvc_release_host(host);
	nlm_release_file(file);
	return rpc_success;
}

static __be32
nlmsvc_proc_cancel(struct svc_rqst *rqstp)
{
	return __nlmsvc_proc_cancel(rqstp, rqstp->rq_resp);
}

 
static __be32
__nlmsvc_proc_unlock(struct svc_rqst *rqstp, struct nlm_res *resp)
{
	struct nlm_args *argp = rqstp->rq_argp;
	struct nlm_host	*host;
	struct nlm_file	*file;
	struct net *net = SVC_NET(rqstp);

	dprintk("lockd: UNLOCK        called\n");

	resp->cookie = argp->cookie;

	 
	if (locks_in_grace(net)) {
		resp->status = nlm_lck_denied_grace_period;
		return rpc_success;
	}

	 
	if ((resp->status = nlmsvc_retrieve_args(rqstp, argp, &host, &file)))
		return resp->status == nlm_drop_reply ? rpc_drop_reply :rpc_success;

	 
	resp->status = cast_status(nlmsvc_unlock(net, file, &argp->lock));

	dprintk("lockd: UNLOCK        status %d\n", ntohl(resp->status));
	nlmsvc_release_lockowner(&argp->lock);
	nlmsvc_release_host(host);
	nlm_release_file(file);
	return rpc_success;
}

static __be32
nlmsvc_proc_unlock(struct svc_rqst *rqstp)
{
	return __nlmsvc_proc_unlock(rqstp, rqstp->rq_resp);
}

 
static __be32
__nlmsvc_proc_granted(struct svc_rqst *rqstp, struct nlm_res *resp)
{
	struct nlm_args *argp = rqstp->rq_argp;

	resp->cookie = argp->cookie;

	dprintk("lockd: GRANTED       called\n");
	resp->status = nlmclnt_grant(svc_addr(rqstp), &argp->lock);
	dprintk("lockd: GRANTED       status %d\n", ntohl(resp->status));
	return rpc_success;
}

static __be32
nlmsvc_proc_granted(struct svc_rqst *rqstp)
{
	return __nlmsvc_proc_granted(rqstp, rqstp->rq_resp);
}

 
static void nlmsvc_callback_exit(struct rpc_task *task, void *data)
{
}

void nlmsvc_release_call(struct nlm_rqst *call)
{
	if (!refcount_dec_and_test(&call->a_count))
		return;
	nlmsvc_release_host(call->a_host);
	kfree(call);
}

static void nlmsvc_callback_release(void *data)
{
	nlmsvc_release_call(data);
}

static const struct rpc_call_ops nlmsvc_callback_ops = {
	.rpc_call_done = nlmsvc_callback_exit,
	.rpc_release = nlmsvc_callback_release,
};

 
static __be32 nlmsvc_callback(struct svc_rqst *rqstp, u32 proc,
		__be32 (*func)(struct svc_rqst *, struct nlm_res *))
{
	struct nlm_args *argp = rqstp->rq_argp;
	struct nlm_host	*host;
	struct nlm_rqst	*call;
	__be32 stat;

	host = nlmsvc_lookup_host(rqstp,
				  argp->lock.caller,
				  argp->lock.len);
	if (host == NULL)
		return rpc_system_err;

	call = nlm_alloc_call(host);
	nlmsvc_release_host(host);
	if (call == NULL)
		return rpc_system_err;

	stat = func(rqstp, &call->a_res);
	if (stat != 0) {
		nlmsvc_release_call(call);
		return stat;
	}

	call->a_flags = RPC_TASK_ASYNC;
	if (nlm_async_reply(call, proc, &nlmsvc_callback_ops) < 0)
		return rpc_system_err;
	return rpc_success;
}

static __be32 nlmsvc_proc_test_msg(struct svc_rqst *rqstp)
{
	dprintk("lockd: TEST_MSG      called\n");
	return nlmsvc_callback(rqstp, NLMPROC_TEST_RES, __nlmsvc_proc_test);
}

static __be32 nlmsvc_proc_lock_msg(struct svc_rqst *rqstp)
{
	dprintk("lockd: LOCK_MSG      called\n");
	return nlmsvc_callback(rqstp, NLMPROC_LOCK_RES, __nlmsvc_proc_lock);
}

static __be32 nlmsvc_proc_cancel_msg(struct svc_rqst *rqstp)
{
	dprintk("lockd: CANCEL_MSG    called\n");
	return nlmsvc_callback(rqstp, NLMPROC_CANCEL_RES, __nlmsvc_proc_cancel);
}

static __be32
nlmsvc_proc_unlock_msg(struct svc_rqst *rqstp)
{
	dprintk("lockd: UNLOCK_MSG    called\n");
	return nlmsvc_callback(rqstp, NLMPROC_UNLOCK_RES, __nlmsvc_proc_unlock);
}

static __be32
nlmsvc_proc_granted_msg(struct svc_rqst *rqstp)
{
	dprintk("lockd: GRANTED_MSG   called\n");
	return nlmsvc_callback(rqstp, NLMPROC_GRANTED_RES, __nlmsvc_proc_granted);
}

 
static __be32
nlmsvc_proc_share(struct svc_rqst *rqstp)
{
	struct nlm_args *argp = rqstp->rq_argp;
	struct nlm_res *resp = rqstp->rq_resp;
	struct nlm_host	*host;
	struct nlm_file	*file;

	dprintk("lockd: SHARE         called\n");

	resp->cookie = argp->cookie;

	 
	if (locks_in_grace(SVC_NET(rqstp)) && !argp->reclaim) {
		resp->status = nlm_lck_denied_grace_period;
		return rpc_success;
	}

	 
	if ((resp->status = nlmsvc_retrieve_args(rqstp, argp, &host, &file)))
		return resp->status == nlm_drop_reply ? rpc_drop_reply :rpc_success;

	 
	resp->status = cast_status(nlmsvc_share_file(host, file, argp));

	dprintk("lockd: SHARE         status %d\n", ntohl(resp->status));
	nlmsvc_release_lockowner(&argp->lock);
	nlmsvc_release_host(host);
	nlm_release_file(file);
	return rpc_success;
}

 
static __be32
nlmsvc_proc_unshare(struct svc_rqst *rqstp)
{
	struct nlm_args *argp = rqstp->rq_argp;
	struct nlm_res *resp = rqstp->rq_resp;
	struct nlm_host	*host;
	struct nlm_file	*file;

	dprintk("lockd: UNSHARE       called\n");

	resp->cookie = argp->cookie;

	 
	if (locks_in_grace(SVC_NET(rqstp))) {
		resp->status = nlm_lck_denied_grace_period;
		return rpc_success;
	}

	 
	if ((resp->status = nlmsvc_retrieve_args(rqstp, argp, &host, &file)))
		return resp->status == nlm_drop_reply ? rpc_drop_reply :rpc_success;

	 
	resp->status = cast_status(nlmsvc_unshare_file(host, file, argp));

	dprintk("lockd: UNSHARE       status %d\n", ntohl(resp->status));
	nlmsvc_release_lockowner(&argp->lock);
	nlmsvc_release_host(host);
	nlm_release_file(file);
	return rpc_success;
}

 
static __be32
nlmsvc_proc_nm_lock(struct svc_rqst *rqstp)
{
	struct nlm_args *argp = rqstp->rq_argp;

	dprintk("lockd: NM_LOCK       called\n");

	argp->monitor = 0;		 
	return nlmsvc_proc_lock(rqstp);
}

 
static __be32
nlmsvc_proc_free_all(struct svc_rqst *rqstp)
{
	struct nlm_args *argp = rqstp->rq_argp;
	struct nlm_host	*host;

	 
	if (nlmsvc_retrieve_args(rqstp, argp, &host, NULL))
		return rpc_success;

	nlmsvc_free_host_resources(host);
	nlmsvc_release_host(host);
	return rpc_success;
}

 
static __be32
nlmsvc_proc_sm_notify(struct svc_rqst *rqstp)
{
	struct nlm_reboot *argp = rqstp->rq_argp;

	dprintk("lockd: SM_NOTIFY     called\n");

	if (!nlm_privileged_requester(rqstp)) {
		char buf[RPC_MAX_ADDRBUFLEN];
		printk(KERN_WARNING "lockd: rejected NSM callback from %s\n",
				svc_print_addr(rqstp, buf, sizeof(buf)));
		return rpc_system_err;
	}

	nlm_host_rebooted(SVC_NET(rqstp), argp);
	return rpc_success;
}

 
static __be32
nlmsvc_proc_granted_res(struct svc_rqst *rqstp)
{
	struct nlm_res *argp = rqstp->rq_argp;

	if (!nlmsvc_ops)
		return rpc_success;

	dprintk("lockd: GRANTED_RES   called\n");

	nlmsvc_grant_reply(&argp->cookie, argp->status);
	return rpc_success;
}

static __be32
nlmsvc_proc_unused(struct svc_rqst *rqstp)
{
	return rpc_proc_unavail;
}

 

struct nlm_void			{ int dummy; };

#define	Ck	(1+XDR_QUADLEN(NLM_MAXCOOKIELEN))	 
#define	St	1				 
#define	No	(1+1024/4)			 
#define	Rg	2				 

const struct svc_procedure nlmsvc_procedures[24] = {
	[NLMPROC_NULL] = {
		.pc_func = nlmsvc_proc_null,
		.pc_decode = nlmsvc_decode_void,
		.pc_encode = nlmsvc_encode_void,
		.pc_argsize = sizeof(struct nlm_void),
		.pc_argzero = sizeof(struct nlm_void),
		.pc_ressize = sizeof(struct nlm_void),
		.pc_xdrressize = St,
		.pc_name = "NULL",
	},
	[NLMPROC_TEST] = {
		.pc_func = nlmsvc_proc_test,
		.pc_decode = nlmsvc_decode_testargs,
		.pc_encode = nlmsvc_encode_testres,
		.pc_argsize = sizeof(struct nlm_args),
		.pc_argzero = sizeof(struct nlm_args),
		.pc_ressize = sizeof(struct nlm_res),
		.pc_xdrressize = Ck+St+2+No+Rg,
		.pc_name = "TEST",
	},
	[NLMPROC_LOCK] = {
		.pc_func = nlmsvc_proc_lock,
		.pc_decode = nlmsvc_decode_lockargs,
		.pc_encode = nlmsvc_encode_res,
		.pc_argsize = sizeof(struct nlm_args),
		.pc_argzero = sizeof(struct nlm_args),
		.pc_ressize = sizeof(struct nlm_res),
		.pc_xdrressize = Ck+St,
		.pc_name = "LOCK",
	},
	[NLMPROC_CANCEL] = {
		.pc_func = nlmsvc_proc_cancel,
		.pc_decode = nlmsvc_decode_cancargs,
		.pc_encode = nlmsvc_encode_res,
		.pc_argsize = sizeof(struct nlm_args),
		.pc_argzero = sizeof(struct nlm_args),
		.pc_ressize = sizeof(struct nlm_res),
		.pc_xdrressize = Ck+St,
		.pc_name = "CANCEL",
	},
	[NLMPROC_UNLOCK] = {
		.pc_func = nlmsvc_proc_unlock,
		.pc_decode = nlmsvc_decode_unlockargs,
		.pc_encode = nlmsvc_encode_res,
		.pc_argsize = sizeof(struct nlm_args),
		.pc_argzero = sizeof(struct nlm_args),
		.pc_ressize = sizeof(struct nlm_res),
		.pc_xdrressize = Ck+St,
		.pc_name = "UNLOCK",
	},
	[NLMPROC_GRANTED] = {
		.pc_func = nlmsvc_proc_granted,
		.pc_decode = nlmsvc_decode_testargs,
		.pc_encode = nlmsvc_encode_res,
		.pc_argsize = sizeof(struct nlm_args),
		.pc_argzero = sizeof(struct nlm_args),
		.pc_ressize = sizeof(struct nlm_res),
		.pc_xdrressize = Ck+St,
		.pc_name = "GRANTED",
	},
	[NLMPROC_TEST_MSG] = {
		.pc_func = nlmsvc_proc_test_msg,
		.pc_decode = nlmsvc_decode_testargs,
		.pc_encode = nlmsvc_encode_void,
		.pc_argsize = sizeof(struct nlm_args),
		.pc_argzero = sizeof(struct nlm_args),
		.pc_ressize = sizeof(struct nlm_void),
		.pc_xdrressize = St,
		.pc_name = "TEST_MSG",
	},
	[NLMPROC_LOCK_MSG] = {
		.pc_func = nlmsvc_proc_lock_msg,
		.pc_decode = nlmsvc_decode_lockargs,
		.pc_encode = nlmsvc_encode_void,
		.pc_argsize = sizeof(struct nlm_args),
		.pc_argzero = sizeof(struct nlm_args),
		.pc_ressize = sizeof(struct nlm_void),
		.pc_xdrressize = St,
		.pc_name = "LOCK_MSG",
	},
	[NLMPROC_CANCEL_MSG] = {
		.pc_func = nlmsvc_proc_cancel_msg,
		.pc_decode = nlmsvc_decode_cancargs,
		.pc_encode = nlmsvc_encode_void,
		.pc_argsize = sizeof(struct nlm_args),
		.pc_argzero = sizeof(struct nlm_args),
		.pc_ressize = sizeof(struct nlm_void),
		.pc_xdrressize = St,
		.pc_name = "CANCEL_MSG",
	},
	[NLMPROC_UNLOCK_MSG] = {
		.pc_func = nlmsvc_proc_unlock_msg,
		.pc_decode = nlmsvc_decode_unlockargs,
		.pc_encode = nlmsvc_encode_void,
		.pc_argsize = sizeof(struct nlm_args),
		.pc_argzero = sizeof(struct nlm_args),
		.pc_ressize = sizeof(struct nlm_void),
		.pc_xdrressize = St,
		.pc_name = "UNLOCK_MSG",
	},
	[NLMPROC_GRANTED_MSG] = {
		.pc_func = nlmsvc_proc_granted_msg,
		.pc_decode = nlmsvc_decode_testargs,
		.pc_encode = nlmsvc_encode_void,
		.pc_argsize = sizeof(struct nlm_args),
		.pc_argzero = sizeof(struct nlm_args),
		.pc_ressize = sizeof(struct nlm_void),
		.pc_xdrressize = St,
		.pc_name = "GRANTED_MSG",
	},
	[NLMPROC_TEST_RES] = {
		.pc_func = nlmsvc_proc_null,
		.pc_decode = nlmsvc_decode_void,
		.pc_encode = nlmsvc_encode_void,
		.pc_argsize = sizeof(struct nlm_res),
		.pc_argzero = sizeof(struct nlm_res),
		.pc_ressize = sizeof(struct nlm_void),
		.pc_xdrressize = St,
		.pc_name = "TEST_RES",
	},
	[NLMPROC_LOCK_RES] = {
		.pc_func = nlmsvc_proc_null,
		.pc_decode = nlmsvc_decode_void,
		.pc_encode = nlmsvc_encode_void,
		.pc_argsize = sizeof(struct nlm_res),
		.pc_argzero = sizeof(struct nlm_res),
		.pc_ressize = sizeof(struct nlm_void),
		.pc_xdrressize = St,
		.pc_name = "LOCK_RES",
	},
	[NLMPROC_CANCEL_RES] = {
		.pc_func = nlmsvc_proc_null,
		.pc_decode = nlmsvc_decode_void,
		.pc_encode = nlmsvc_encode_void,
		.pc_argsize = sizeof(struct nlm_res),
		.pc_argzero = sizeof(struct nlm_res),
		.pc_ressize = sizeof(struct nlm_void),
		.pc_xdrressize = St,
		.pc_name = "CANCEL_RES",
	},
	[NLMPROC_UNLOCK_RES] = {
		.pc_func = nlmsvc_proc_null,
		.pc_decode = nlmsvc_decode_void,
		.pc_encode = nlmsvc_encode_void,
		.pc_argsize = sizeof(struct nlm_res),
		.pc_argzero = sizeof(struct nlm_res),
		.pc_ressize = sizeof(struct nlm_void),
		.pc_xdrressize = St,
		.pc_name = "UNLOCK_RES",
	},
	[NLMPROC_GRANTED_RES] = {
		.pc_func = nlmsvc_proc_granted_res,
		.pc_decode = nlmsvc_decode_res,
		.pc_encode = nlmsvc_encode_void,
		.pc_argsize = sizeof(struct nlm_res),
		.pc_argzero = sizeof(struct nlm_res),
		.pc_ressize = sizeof(struct nlm_void),
		.pc_xdrressize = St,
		.pc_name = "GRANTED_RES",
	},
	[NLMPROC_NSM_NOTIFY] = {
		.pc_func = nlmsvc_proc_sm_notify,
		.pc_decode = nlmsvc_decode_reboot,
		.pc_encode = nlmsvc_encode_void,
		.pc_argsize = sizeof(struct nlm_reboot),
		.pc_argzero = sizeof(struct nlm_reboot),
		.pc_ressize = sizeof(struct nlm_void),
		.pc_xdrressize = St,
		.pc_name = "SM_NOTIFY",
	},
	[17] = {
		.pc_func = nlmsvc_proc_unused,
		.pc_decode = nlmsvc_decode_void,
		.pc_encode = nlmsvc_encode_void,
		.pc_argsize = sizeof(struct nlm_void),
		.pc_argzero = sizeof(struct nlm_void),
		.pc_ressize = sizeof(struct nlm_void),
		.pc_xdrressize = St,
		.pc_name = "UNUSED",
	},
	[18] = {
		.pc_func = nlmsvc_proc_unused,
		.pc_decode = nlmsvc_decode_void,
		.pc_encode = nlmsvc_encode_void,
		.pc_argsize = sizeof(struct nlm_void),
		.pc_argzero = sizeof(struct nlm_void),
		.pc_ressize = sizeof(struct nlm_void),
		.pc_xdrressize = St,
		.pc_name = "UNUSED",
	},
	[19] = {
		.pc_func = nlmsvc_proc_unused,
		.pc_decode = nlmsvc_decode_void,
		.pc_encode = nlmsvc_encode_void,
		.pc_argsize = sizeof(struct nlm_void),
		.pc_argzero = sizeof(struct nlm_void),
		.pc_ressize = sizeof(struct nlm_void),
		.pc_xdrressize = St,
		.pc_name = "UNUSED",
	},
	[NLMPROC_SHARE] = {
		.pc_func = nlmsvc_proc_share,
		.pc_decode = nlmsvc_decode_shareargs,
		.pc_encode = nlmsvc_encode_shareres,
		.pc_argsize = sizeof(struct nlm_args),
		.pc_argzero = sizeof(struct nlm_args),
		.pc_ressize = sizeof(struct nlm_res),
		.pc_xdrressize = Ck+St+1,
		.pc_name = "SHARE",
	},
	[NLMPROC_UNSHARE] = {
		.pc_func = nlmsvc_proc_unshare,
		.pc_decode = nlmsvc_decode_shareargs,
		.pc_encode = nlmsvc_encode_shareres,
		.pc_argsize = sizeof(struct nlm_args),
		.pc_argzero = sizeof(struct nlm_args),
		.pc_ressize = sizeof(struct nlm_res),
		.pc_xdrressize = Ck+St+1,
		.pc_name = "UNSHARE",
	},
	[NLMPROC_NM_LOCK] = {
		.pc_func = nlmsvc_proc_nm_lock,
		.pc_decode = nlmsvc_decode_lockargs,
		.pc_encode = nlmsvc_encode_res,
		.pc_argsize = sizeof(struct nlm_args),
		.pc_argzero = sizeof(struct nlm_args),
		.pc_ressize = sizeof(struct nlm_res),
		.pc_xdrressize = Ck+St,
		.pc_name = "NM_LOCK",
	},
	[NLMPROC_FREE_ALL] = {
		.pc_func = nlmsvc_proc_free_all,
		.pc_decode = nlmsvc_decode_notify,
		.pc_encode = nlmsvc_encode_void,
		.pc_argsize = sizeof(struct nlm_args),
		.pc_argzero = sizeof(struct nlm_args),
		.pc_ressize = sizeof(struct nlm_void),
		.pc_xdrressize = 0,
		.pc_name = "FREE_ALL",
	},
};
