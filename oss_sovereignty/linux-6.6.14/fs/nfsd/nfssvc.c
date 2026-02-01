
 

#include <linux/sched/signal.h>
#include <linux/freezer.h>
#include <linux/module.h>
#include <linux/fs_struct.h>
#include <linux/swap.h>
#include <linux/siphash.h>

#include <linux/sunrpc/stats.h>
#include <linux/sunrpc/svcsock.h>
#include <linux/sunrpc/svc_xprt.h>
#include <linux/lockd/bind.h>
#include <linux/nfsacl.h>
#include <linux/seq_file.h>
#include <linux/inetdevice.h>
#include <net/addrconf.h>
#include <net/ipv6.h>
#include <net/net_namespace.h>
#include "nfsd.h"
#include "cache.h"
#include "vfs.h"
#include "netns.h"
#include "filecache.h"

#include "trace.h"

#define NFSDDBG_FACILITY	NFSDDBG_SVC

extern struct svc_program	nfsd_program;
static int			nfsd(void *vrqstp);
#if defined(CONFIG_NFSD_V2_ACL) || defined(CONFIG_NFSD_V3_ACL)
static int			nfsd_acl_rpcbind_set(struct net *,
						     const struct svc_program *,
						     u32, int,
						     unsigned short,
						     unsigned short);
static __be32			nfsd_acl_init_request(struct svc_rqst *,
						const struct svc_program *,
						struct svc_process_info *);
#endif
static int			nfsd_rpcbind_set(struct net *,
						 const struct svc_program *,
						 u32, int,
						 unsigned short,
						 unsigned short);
static __be32			nfsd_init_request(struct svc_rqst *,
						const struct svc_program *,
						struct svc_process_info *);

 
DEFINE_MUTEX(nfsd_mutex);

 
DEFINE_SPINLOCK(nfsd_drc_lock);
unsigned long	nfsd_drc_max_mem;
unsigned long	nfsd_drc_mem_used;

#if defined(CONFIG_NFSD_V2_ACL) || defined(CONFIG_NFSD_V3_ACL)
static struct svc_stat	nfsd_acl_svcstats;
static const struct svc_version *nfsd_acl_version[] = {
# if defined(CONFIG_NFSD_V2_ACL)
	[2] = &nfsd_acl_version2,
# endif
# if defined(CONFIG_NFSD_V3_ACL)
	[3] = &nfsd_acl_version3,
# endif
};

#define NFSD_ACL_MINVERS            2
#define NFSD_ACL_NRVERS		ARRAY_SIZE(nfsd_acl_version)

static struct svc_program	nfsd_acl_program = {
	.pg_prog		= NFS_ACL_PROGRAM,
	.pg_nvers		= NFSD_ACL_NRVERS,
	.pg_vers		= nfsd_acl_version,
	.pg_name		= "nfsacl",
	.pg_class		= "nfsd",
	.pg_stats		= &nfsd_acl_svcstats,
	.pg_authenticate	= &svc_set_client,
	.pg_init_request	= nfsd_acl_init_request,
	.pg_rpcbind_set		= nfsd_acl_rpcbind_set,
};

static struct svc_stat	nfsd_acl_svcstats = {
	.program	= &nfsd_acl_program,
};
#endif  

static const struct svc_version *nfsd_version[] = {
#if defined(CONFIG_NFSD_V2)
	[2] = &nfsd_version2,
#endif
	[3] = &nfsd_version3,
#if defined(CONFIG_NFSD_V4)
	[4] = &nfsd_version4,
#endif
};

#define NFSD_MINVERS    	2
#define NFSD_NRVERS		ARRAY_SIZE(nfsd_version)

struct svc_program		nfsd_program = {
#if defined(CONFIG_NFSD_V2_ACL) || defined(CONFIG_NFSD_V3_ACL)
	.pg_next		= &nfsd_acl_program,
#endif
	.pg_prog		= NFS_PROGRAM,		 
	.pg_nvers		= NFSD_NRVERS,		 
	.pg_vers		= nfsd_version,		 
	.pg_name		= "nfsd",		 
	.pg_class		= "nfsd",		 
	.pg_stats		= &nfsd_svcstats,	 
	.pg_authenticate	= &svc_set_client,	 
	.pg_init_request	= nfsd_init_request,
	.pg_rpcbind_set		= nfsd_rpcbind_set,
};

static bool
nfsd_support_version(int vers)
{
	if (vers >= NFSD_MINVERS && vers < NFSD_NRVERS)
		return nfsd_version[vers] != NULL;
	return false;
}

static bool *
nfsd_alloc_versions(void)
{
	bool *vers = kmalloc_array(NFSD_NRVERS, sizeof(bool), GFP_KERNEL);
	unsigned i;

	if (vers) {
		 
		for (i = 0; i < NFSD_NRVERS; i++)
			vers[i] = nfsd_support_version(i);
	}
	return vers;
}

static bool *
nfsd_alloc_minorversions(void)
{
	bool *vers = kmalloc_array(NFSD_SUPPORTED_MINOR_VERSION + 1,
			sizeof(bool), GFP_KERNEL);
	unsigned i;

	if (vers) {
		 
		for (i = 0; i <= NFSD_SUPPORTED_MINOR_VERSION; i++)
			vers[i] = nfsd_support_version(4);
	}
	return vers;
}

void
nfsd_netns_free_versions(struct nfsd_net *nn)
{
	kfree(nn->nfsd_versions);
	kfree(nn->nfsd4_minorversions);
	nn->nfsd_versions = NULL;
	nn->nfsd4_minorversions = NULL;
}

static void
nfsd_netns_init_versions(struct nfsd_net *nn)
{
	if (!nn->nfsd_versions) {
		nn->nfsd_versions = nfsd_alloc_versions();
		nn->nfsd4_minorversions = nfsd_alloc_minorversions();
		if (!nn->nfsd_versions || !nn->nfsd4_minorversions)
			nfsd_netns_free_versions(nn);
	}
}

int nfsd_vers(struct nfsd_net *nn, int vers, enum vers_op change)
{
	if (vers < NFSD_MINVERS || vers >= NFSD_NRVERS)
		return 0;
	switch(change) {
	case NFSD_SET:
		if (nn->nfsd_versions)
			nn->nfsd_versions[vers] = nfsd_support_version(vers);
		break;
	case NFSD_CLEAR:
		nfsd_netns_init_versions(nn);
		if (nn->nfsd_versions)
			nn->nfsd_versions[vers] = false;
		break;
	case NFSD_TEST:
		if (nn->nfsd_versions)
			return nn->nfsd_versions[vers];
		fallthrough;
	case NFSD_AVAIL:
		return nfsd_support_version(vers);
	}
	return 0;
}

static void
nfsd_adjust_nfsd_versions4(struct nfsd_net *nn)
{
	unsigned i;

	for (i = 0; i <= NFSD_SUPPORTED_MINOR_VERSION; i++) {
		if (nn->nfsd4_minorversions[i])
			return;
	}
	nfsd_vers(nn, 4, NFSD_CLEAR);
}

int nfsd_minorversion(struct nfsd_net *nn, u32 minorversion, enum vers_op change)
{
	if (minorversion > NFSD_SUPPORTED_MINOR_VERSION &&
	    change != NFSD_AVAIL)
		return -1;

	switch(change) {
	case NFSD_SET:
		if (nn->nfsd4_minorversions) {
			nfsd_vers(nn, 4, NFSD_SET);
			nn->nfsd4_minorversions[minorversion] =
				nfsd_vers(nn, 4, NFSD_TEST);
		}
		break;
	case NFSD_CLEAR:
		nfsd_netns_init_versions(nn);
		if (nn->nfsd4_minorversions) {
			nn->nfsd4_minorversions[minorversion] = false;
			nfsd_adjust_nfsd_versions4(nn);
		}
		break;
	case NFSD_TEST:
		if (nn->nfsd4_minorversions)
			return nn->nfsd4_minorversions[minorversion];
		return nfsd_vers(nn, 4, NFSD_TEST);
	case NFSD_AVAIL:
		return minorversion <= NFSD_SUPPORTED_MINOR_VERSION &&
			nfsd_vers(nn, 4, NFSD_AVAIL);
	}
	return 0;
}

 
#define	NFSD_MAXSERVS		8192

int nfsd_nrthreads(struct net *net)
{
	int rv = 0;
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);

	mutex_lock(&nfsd_mutex);
	if (nn->nfsd_serv)
		rv = nn->nfsd_serv->sv_nrthreads;
	mutex_unlock(&nfsd_mutex);
	return rv;
}

static int nfsd_init_socks(struct net *net, const struct cred *cred)
{
	int error;
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);

	if (!list_empty(&nn->nfsd_serv->sv_permsocks))
		return 0;

	error = svc_xprt_create(nn->nfsd_serv, "udp", net, PF_INET, NFS_PORT,
				SVC_SOCK_DEFAULTS, cred);
	if (error < 0)
		return error;

	error = svc_xprt_create(nn->nfsd_serv, "tcp", net, PF_INET, NFS_PORT,
				SVC_SOCK_DEFAULTS, cred);
	if (error < 0)
		return error;

	return 0;
}

static int nfsd_users = 0;

static int nfsd_startup_generic(void)
{
	int ret;

	if (nfsd_users++)
		return 0;

	ret = nfsd_file_cache_init();
	if (ret)
		goto dec_users;

	ret = nfs4_state_start();
	if (ret)
		goto out_file_cache;
	return 0;

out_file_cache:
	nfsd_file_cache_shutdown();
dec_users:
	nfsd_users--;
	return ret;
}

static void nfsd_shutdown_generic(void)
{
	if (--nfsd_users)
		return;

	nfs4_state_shutdown();
	nfsd_file_cache_shutdown();
}

static bool nfsd_needs_lockd(struct nfsd_net *nn)
{
	return nfsd_vers(nn, 2, NFSD_TEST) || nfsd_vers(nn, 3, NFSD_TEST);
}

 
void nfsd_copy_write_verifier(__be32 verf[2], struct nfsd_net *nn)
{
	int seq = 0;

	do {
		read_seqbegin_or_lock(&nn->writeverf_lock, &seq);
		memcpy(verf, nn->writeverf, sizeof(nn->writeverf));
	} while (need_seqretry(&nn->writeverf_lock, seq));
	done_seqretry(&nn->writeverf_lock, seq);
}

static void nfsd_reset_write_verifier_locked(struct nfsd_net *nn)
{
	struct timespec64 now;
	u64 verf;

	 
	ktime_get_raw_ts64(&now);
	verf = siphash_2u64(now.tv_sec, now.tv_nsec, &nn->siphash_key);
	memcpy(nn->writeverf, &verf, sizeof(nn->writeverf));
}

 
void nfsd_reset_write_verifier(struct nfsd_net *nn)
{
	write_seqlock(&nn->writeverf_lock);
	nfsd_reset_write_verifier_locked(nn);
	write_sequnlock(&nn->writeverf_lock);
}

 
static int nfsd_startup_net(struct net *net, const struct cred *cred)
{
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);
	int ret;

	if (nn->nfsd_net_up)
		return 0;

	ret = nfsd_startup_generic();
	if (ret)
		return ret;
	ret = nfsd_init_socks(net, cred);
	if (ret)
		goto out_socks;

	if (nfsd_needs_lockd(nn) && !nn->lockd_up) {
		ret = lockd_up(net, cred);
		if (ret)
			goto out_socks;
		nn->lockd_up = true;
	}

	ret = nfsd_file_cache_start_net(net);
	if (ret)
		goto out_lockd;

	ret = nfsd_reply_cache_init(nn);
	if (ret)
		goto out_filecache;

	ret = nfs4_state_start_net(net);
	if (ret)
		goto out_reply_cache;

#ifdef CONFIG_NFSD_V4_2_INTER_SSC
	nfsd4_ssc_init_umount_work(nn);
#endif
	nn->nfsd_net_up = true;
	return 0;

out_reply_cache:
	nfsd_reply_cache_shutdown(nn);
out_filecache:
	nfsd_file_cache_shutdown_net(net);
out_lockd:
	if (nn->lockd_up) {
		lockd_down(net);
		nn->lockd_up = false;
	}
out_socks:
	nfsd_shutdown_generic();
	return ret;
}

static void nfsd_shutdown_net(struct net *net)
{
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);

	nfs4_state_shutdown_net(net);
	nfsd_reply_cache_shutdown(nn);
	nfsd_file_cache_shutdown_net(net);
	if (nn->lockd_up) {
		lockd_down(net);
		nn->lockd_up = false;
	}
	nn->nfsd_net_up = false;
	nfsd_shutdown_generic();
}

static DEFINE_SPINLOCK(nfsd_notifier_lock);
static int nfsd_inetaddr_event(struct notifier_block *this, unsigned long event,
	void *ptr)
{
	struct in_ifaddr *ifa = (struct in_ifaddr *)ptr;
	struct net_device *dev = ifa->ifa_dev->dev;
	struct net *net = dev_net(dev);
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);
	struct sockaddr_in sin;

	if (event != NETDEV_DOWN || !nn->nfsd_serv)
		goto out;

	spin_lock(&nfsd_notifier_lock);
	if (nn->nfsd_serv) {
		dprintk("nfsd_inetaddr_event: removed %pI4\n", &ifa->ifa_local);
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = ifa->ifa_local;
		svc_age_temp_xprts_now(nn->nfsd_serv, (struct sockaddr *)&sin);
	}
	spin_unlock(&nfsd_notifier_lock);

out:
	return NOTIFY_DONE;
}

static struct notifier_block nfsd_inetaddr_notifier = {
	.notifier_call = nfsd_inetaddr_event,
};

#if IS_ENABLED(CONFIG_IPV6)
static int nfsd_inet6addr_event(struct notifier_block *this,
	unsigned long event, void *ptr)
{
	struct inet6_ifaddr *ifa = (struct inet6_ifaddr *)ptr;
	struct net_device *dev = ifa->idev->dev;
	struct net *net = dev_net(dev);
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);
	struct sockaddr_in6 sin6;

	if (event != NETDEV_DOWN || !nn->nfsd_serv)
		goto out;

	spin_lock(&nfsd_notifier_lock);
	if (nn->nfsd_serv) {
		dprintk("nfsd_inet6addr_event: removed %pI6\n", &ifa->addr);
		sin6.sin6_family = AF_INET6;
		sin6.sin6_addr = ifa->addr;
		if (ipv6_addr_type(&sin6.sin6_addr) & IPV6_ADDR_LINKLOCAL)
			sin6.sin6_scope_id = ifa->idev->dev->ifindex;
		svc_age_temp_xprts_now(nn->nfsd_serv, (struct sockaddr *)&sin6);
	}
	spin_unlock(&nfsd_notifier_lock);

out:
	return NOTIFY_DONE;
}

static struct notifier_block nfsd_inet6addr_notifier = {
	.notifier_call = nfsd_inet6addr_event,
};
#endif

 
static atomic_t nfsd_notifier_refcount = ATOMIC_INIT(0);

void nfsd_last_thread(struct net *net)
{
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);
	struct svc_serv *serv = nn->nfsd_serv;

	spin_lock(&nfsd_notifier_lock);
	nn->nfsd_serv = NULL;
	spin_unlock(&nfsd_notifier_lock);

	 
	if (atomic_dec_return(&nfsd_notifier_refcount) == 0) {
		unregister_inetaddr_notifier(&nfsd_inetaddr_notifier);
#if IS_ENABLED(CONFIG_IPV6)
		unregister_inet6addr_notifier(&nfsd_inet6addr_notifier);
#endif
	}

	svc_xprt_destroy_all(serv, net);

	 
	svc_rpcb_cleanup(serv, net);
	if (!nn->nfsd_net_up)
		return;

	nfsd_shutdown_net(net);
	pr_info("nfsd: last server has exited, flushing export cache\n");
	nfsd_export_flush(net);
}

void nfsd_reset_versions(struct nfsd_net *nn)
{
	int i;

	for (i = 0; i < NFSD_NRVERS; i++)
		if (nfsd_vers(nn, i, NFSD_TEST))
			return;

	for (i = 0; i < NFSD_NRVERS; i++)
		if (i != 4)
			nfsd_vers(nn, i, NFSD_SET);
		else {
			int minor = 0;
			while (nfsd_minorversion(nn, minor, NFSD_SET) >= 0)
				minor++;
		}
}

 
static void set_max_drc(void)
{
	#define NFSD_DRC_SIZE_SHIFT	7
	nfsd_drc_max_mem = (nr_free_buffer_pages()
					>> NFSD_DRC_SIZE_SHIFT) * PAGE_SIZE;
	nfsd_drc_mem_used = 0;
	dprintk("%s nfsd_drc_max_mem %lu \n", __func__, nfsd_drc_max_mem);
}

static int nfsd_get_default_max_blksize(void)
{
	struct sysinfo i;
	unsigned long long target;
	unsigned long ret;

	si_meminfo(&i);
	target = (i.totalram - i.totalhigh) << PAGE_SHIFT;
	 
	target >>= 12;

	ret = NFSSVC_MAXBLKSIZE;
	while (ret > target && ret >= 8*1024*2)
		ret /= 2;
	return ret;
}

void nfsd_shutdown_threads(struct net *net)
{
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);
	struct svc_serv *serv;

	mutex_lock(&nfsd_mutex);
	serv = nn->nfsd_serv;
	if (serv == NULL) {
		mutex_unlock(&nfsd_mutex);
		return;
	}

	svc_get(serv);
	 
	svc_set_num_threads(serv, NULL, 0);
	nfsd_last_thread(net);
	svc_put(serv);
	mutex_unlock(&nfsd_mutex);
}

bool i_am_nfsd(void)
{
	return kthread_func(current) == nfsd;
}

int nfsd_create_serv(struct net *net)
{
	int error;
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);
	struct svc_serv *serv;

	WARN_ON(!mutex_is_locked(&nfsd_mutex));
	if (nn->nfsd_serv) {
		svc_get(nn->nfsd_serv);
		return 0;
	}
	if (nfsd_max_blksize == 0)
		nfsd_max_blksize = nfsd_get_default_max_blksize();
	nfsd_reset_versions(nn);
	serv = svc_create_pooled(&nfsd_program, nfsd_max_blksize, nfsd);
	if (serv == NULL)
		return -ENOMEM;

	serv->sv_maxconn = nn->max_connections;
	error = svc_bind(serv, net);
	if (error < 0) {
		svc_put(serv);
		return error;
	}
	spin_lock(&nfsd_notifier_lock);
	nn->nfsd_serv = serv;
	spin_unlock(&nfsd_notifier_lock);

	set_max_drc();
	 
	if (atomic_inc_return(&nfsd_notifier_refcount) == 1) {
		register_inetaddr_notifier(&nfsd_inetaddr_notifier);
#if IS_ENABLED(CONFIG_IPV6)
		register_inet6addr_notifier(&nfsd_inet6addr_notifier);
#endif
	}
	nfsd_reset_write_verifier(nn);
	return 0;
}

int nfsd_nrpools(struct net *net)
{
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);

	if (nn->nfsd_serv == NULL)
		return 0;
	else
		return nn->nfsd_serv->sv_nrpools;
}

int nfsd_get_nrthreads(int n, int *nthreads, struct net *net)
{
	int i = 0;
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);

	if (nn->nfsd_serv != NULL) {
		for (i = 0; i < nn->nfsd_serv->sv_nrpools && i < n; i++)
			nthreads[i] = nn->nfsd_serv->sv_pools[i].sp_nrthreads;
	}

	return 0;
}

int nfsd_set_nrthreads(int n, int *nthreads, struct net *net)
{
	int i = 0;
	int tot = 0;
	int err = 0;
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);

	WARN_ON(!mutex_is_locked(&nfsd_mutex));

	if (nn->nfsd_serv == NULL || n <= 0)
		return 0;

	if (n > nn->nfsd_serv->sv_nrpools)
		n = nn->nfsd_serv->sv_nrpools;

	 
	tot = 0;
	for (i = 0; i < n; i++) {
		nthreads[i] = min(nthreads[i], NFSD_MAXSERVS);
		tot += nthreads[i];
	}
	if (tot > NFSD_MAXSERVS) {
		 
		for (i = 0; i < n && tot > 0; i++) {
			int new = nthreads[i] * NFSD_MAXSERVS / tot;
			tot -= (nthreads[i] - new);
			nthreads[i] = new;
		}
		for (i = 0; i < n && tot > 0; i++) {
			nthreads[i]--;
			tot--;
		}
	}

	 
	if (nthreads[0] == 0)
		nthreads[0] = 1;

	 
	svc_get(nn->nfsd_serv);
	for (i = 0; i < n; i++) {
		err = svc_set_num_threads(nn->nfsd_serv,
					  &nn->nfsd_serv->sv_pools[i],
					  nthreads[i]);
		if (err)
			break;
	}
	svc_put(nn->nfsd_serv);
	return err;
}

 
int
nfsd_svc(int nrservs, struct net *net, const struct cred *cred)
{
	int	error;
	bool	nfsd_up_before;
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);
	struct svc_serv *serv;

	mutex_lock(&nfsd_mutex);
	dprintk("nfsd: creating service\n");

	nrservs = max(nrservs, 0);
	nrservs = min(nrservs, NFSD_MAXSERVS);
	error = 0;

	if (nrservs == 0 && nn->nfsd_serv == NULL)
		goto out;

	strscpy(nn->nfsd_name, utsname()->nodename,
		sizeof(nn->nfsd_name));

	error = nfsd_create_serv(net);
	if (error)
		goto out;

	nfsd_up_before = nn->nfsd_net_up;
	serv = nn->nfsd_serv;

	error = nfsd_startup_net(net, cred);
	if (error)
		goto out_put;
	error = svc_set_num_threads(serv, NULL, nrservs);
	if (error)
		goto out_shutdown;
	error = serv->sv_nrthreads;
	if (error == 0)
		nfsd_last_thread(net);
out_shutdown:
	if (error < 0 && !nfsd_up_before)
		nfsd_shutdown_net(net);
out_put:
	 
	if (xchg(&nn->keep_active, 0))
		svc_put(serv);
	svc_put(serv);
out:
	mutex_unlock(&nfsd_mutex);
	return error;
}

#if defined(CONFIG_NFSD_V2_ACL) || defined(CONFIG_NFSD_V3_ACL)
static bool
nfsd_support_acl_version(int vers)
{
	if (vers >= NFSD_ACL_MINVERS && vers < NFSD_ACL_NRVERS)
		return nfsd_acl_version[vers] != NULL;
	return false;
}

static int
nfsd_acl_rpcbind_set(struct net *net, const struct svc_program *progp,
		     u32 version, int family, unsigned short proto,
		     unsigned short port)
{
	if (!nfsd_support_acl_version(version) ||
	    !nfsd_vers(net_generic(net, nfsd_net_id), version, NFSD_TEST))
		return 0;
	return svc_generic_rpcbind_set(net, progp, version, family,
			proto, port);
}

static __be32
nfsd_acl_init_request(struct svc_rqst *rqstp,
		      const struct svc_program *progp,
		      struct svc_process_info *ret)
{
	struct nfsd_net *nn = net_generic(SVC_NET(rqstp), nfsd_net_id);
	int i;

	if (likely(nfsd_support_acl_version(rqstp->rq_vers) &&
	    nfsd_vers(nn, rqstp->rq_vers, NFSD_TEST)))
		return svc_generic_init_request(rqstp, progp, ret);

	ret->mismatch.lovers = NFSD_ACL_NRVERS;
	for (i = NFSD_ACL_MINVERS; i < NFSD_ACL_NRVERS; i++) {
		if (nfsd_support_acl_version(rqstp->rq_vers) &&
		    nfsd_vers(nn, i, NFSD_TEST)) {
			ret->mismatch.lovers = i;
			break;
		}
	}
	if (ret->mismatch.lovers == NFSD_ACL_NRVERS)
		return rpc_prog_unavail;
	ret->mismatch.hivers = NFSD_ACL_MINVERS;
	for (i = NFSD_ACL_NRVERS - 1; i >= NFSD_ACL_MINVERS; i--) {
		if (nfsd_support_acl_version(rqstp->rq_vers) &&
		    nfsd_vers(nn, i, NFSD_TEST)) {
			ret->mismatch.hivers = i;
			break;
		}
	}
	return rpc_prog_mismatch;
}
#endif

static int
nfsd_rpcbind_set(struct net *net, const struct svc_program *progp,
		 u32 version, int family, unsigned short proto,
		 unsigned short port)
{
	if (!nfsd_vers(net_generic(net, nfsd_net_id), version, NFSD_TEST))
		return 0;
	return svc_generic_rpcbind_set(net, progp, version, family,
			proto, port);
}

static __be32
nfsd_init_request(struct svc_rqst *rqstp,
		  const struct svc_program *progp,
		  struct svc_process_info *ret)
{
	struct nfsd_net *nn = net_generic(SVC_NET(rqstp), nfsd_net_id);
	int i;

	if (likely(nfsd_vers(nn, rqstp->rq_vers, NFSD_TEST)))
		return svc_generic_init_request(rqstp, progp, ret);

	ret->mismatch.lovers = NFSD_NRVERS;
	for (i = NFSD_MINVERS; i < NFSD_NRVERS; i++) {
		if (nfsd_vers(nn, i, NFSD_TEST)) {
			ret->mismatch.lovers = i;
			break;
		}
	}
	if (ret->mismatch.lovers == NFSD_NRVERS)
		return rpc_prog_unavail;
	ret->mismatch.hivers = NFSD_MINVERS;
	for (i = NFSD_NRVERS - 1; i >= NFSD_MINVERS; i--) {
		if (nfsd_vers(nn, i, NFSD_TEST)) {
			ret->mismatch.hivers = i;
			break;
		}
	}
	return rpc_prog_mismatch;
}

 
static int
nfsd(void *vrqstp)
{
	struct svc_rqst *rqstp = (struct svc_rqst *) vrqstp;
	struct svc_xprt *perm_sock = list_entry(rqstp->rq_server->sv_permsocks.next, typeof(struct svc_xprt), xpt_list);
	struct net *net = perm_sock->xpt_net;
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);

	 
	if (unshare_fs_struct() < 0) {
		printk("Unable to start nfsd thread: out of memory\n");
		goto out;
	}

	current->fs->umask = 0;

	atomic_inc(&nfsdstats.th_cnt);

	set_freezable();

	 
	while (!kthread_should_stop()) {
		 
		rqstp->rq_server->sv_maxconn = nn->max_connections;

		svc_recv(rqstp);
	}

	atomic_dec(&nfsdstats.th_cnt);

out:
	 
	svc_exit_thread(rqstp);
	return 0;
}

 
int nfsd_dispatch(struct svc_rqst *rqstp)
{
	const struct svc_procedure *proc = rqstp->rq_procinfo;
	__be32 *statp = rqstp->rq_accept_statp;
	struct nfsd_cacherep *rp;
	unsigned int start, len;
	__be32 *nfs_reply;

	 
	rqstp->rq_cachetype = proc->pc_cachetype;

	 
	start = xdr_stream_pos(&rqstp->rq_arg_stream);
	len = xdr_stream_remaining(&rqstp->rq_arg_stream);
	if (!proc->pc_decode(rqstp, &rqstp->rq_arg_stream))
		goto out_decode_err;

	rp = NULL;
	switch (nfsd_cache_lookup(rqstp, start, len, &rp)) {
	case RC_DOIT:
		break;
	case RC_REPLY:
		goto out_cached_reply;
	case RC_DROPIT:
		goto out_dropit;
	}

	nfs_reply = xdr_inline_decode(&rqstp->rq_res_stream, 0);
	*statp = proc->pc_func(rqstp);
	if (test_bit(RQ_DROPME, &rqstp->rq_flags))
		goto out_update_drop;

	if (!proc->pc_encode(rqstp, &rqstp->rq_res_stream))
		goto out_encode_err;

	nfsd_cache_update(rqstp, rp, rqstp->rq_cachetype, nfs_reply);
out_cached_reply:
	return 1;

out_decode_err:
	trace_nfsd_garbage_args_err(rqstp);
	*statp = rpc_garbage_args;
	return 1;

out_update_drop:
	nfsd_cache_update(rqstp, rp, RC_NOCACHE, NULL);
out_dropit:
	return 0;

out_encode_err:
	trace_nfsd_cant_encode_err(rqstp);
	nfsd_cache_update(rqstp, rp, RC_NOCACHE, NULL);
	*statp = rpc_system_err;
	return 1;
}

 
bool nfssvc_decode_voidarg(struct svc_rqst *rqstp, struct xdr_stream *xdr)
{
	return true;
}

 
bool nfssvc_encode_voidres(struct svc_rqst *rqstp, struct xdr_stream *xdr)
{
	return true;
}

int nfsd_pool_stats_open(struct inode *inode, struct file *file)
{
	int ret;
	struct nfsd_net *nn = net_generic(inode->i_sb->s_fs_info, nfsd_net_id);

	mutex_lock(&nfsd_mutex);
	if (nn->nfsd_serv == NULL) {
		mutex_unlock(&nfsd_mutex);
		return -ENODEV;
	}
	svc_get(nn->nfsd_serv);
	ret = svc_pool_stats_open(nn->nfsd_serv, file);
	mutex_unlock(&nfsd_mutex);
	return ret;
}

int nfsd_pool_stats_release(struct inode *inode, struct file *file)
{
	struct seq_file *seq = file->private_data;
	struct svc_serv *serv = seq->private;
	int ret = seq_release(inode, file);

	mutex_lock(&nfsd_mutex);
	svc_put(serv);
	mutex_unlock(&nfsd_mutex);
	return ret;
}
