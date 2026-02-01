
 

#include <linux/types.h>
#include <linux/module.h>
#include <linux/sunrpc/types.h>
#include <linux/sunrpc/xdr.h>
#include <linux/sunrpc/svcsock.h>
#include <linux/sunrpc/svcauth.h>
#include <linux/err.h>
#include <linux/hash.h>

#include <trace/events/sunrpc.h>

#include "sunrpc.h"

#define RPCDBG_FACILITY	RPCDBG_AUTH


 
extern struct auth_ops svcauth_null;
extern struct auth_ops svcauth_unix;
extern struct auth_ops svcauth_tls;

static struct auth_ops __rcu *authtab[RPC_AUTH_MAXFLAVOR] = {
	[RPC_AUTH_NULL] = (struct auth_ops __force __rcu *)&svcauth_null,
	[RPC_AUTH_UNIX] = (struct auth_ops __force __rcu *)&svcauth_unix,
	[RPC_AUTH_TLS]  = (struct auth_ops __force __rcu *)&svcauth_tls,
};

static struct auth_ops *
svc_get_auth_ops(rpc_authflavor_t flavor)
{
	struct auth_ops		*aops;

	if (flavor >= RPC_AUTH_MAXFLAVOR)
		return NULL;
	rcu_read_lock();
	aops = rcu_dereference(authtab[flavor]);
	if (aops != NULL && !try_module_get(aops->owner))
		aops = NULL;
	rcu_read_unlock();
	return aops;
}

static void
svc_put_auth_ops(struct auth_ops *aops)
{
	module_put(aops->owner);
}

 
enum svc_auth_status svc_authenticate(struct svc_rqst *rqstp)
{
	struct auth_ops *aops;
	u32 flavor;

	rqstp->rq_auth_stat = rpc_auth_ok;

	 
	if (xdr_stream_decode_u32(&rqstp->rq_arg_stream, &flavor) < 0)
		return SVC_GARBAGE;

	aops = svc_get_auth_ops(flavor);
	if (aops == NULL) {
		rqstp->rq_auth_stat = rpc_autherr_badcred;
		return SVC_DENIED;
	}

	rqstp->rq_auth_slack = 0;
	init_svc_cred(&rqstp->rq_cred);

	rqstp->rq_authop = aops;
	return aops->accept(rqstp);
}
EXPORT_SYMBOL_GPL(svc_authenticate);

 
enum svc_auth_status svc_set_client(struct svc_rqst *rqstp)
{
	rqstp->rq_client = NULL;
	return rqstp->rq_authop->set_client(rqstp);
}
EXPORT_SYMBOL_GPL(svc_set_client);

 
int svc_authorise(struct svc_rqst *rqstp)
{
	struct auth_ops *aops = rqstp->rq_authop;
	int rv = 0;

	rqstp->rq_authop = NULL;

	if (aops) {
		rv = aops->release(rqstp);
		svc_put_auth_ops(aops);
	}
	return rv;
}

int
svc_auth_register(rpc_authflavor_t flavor, struct auth_ops *aops)
{
	struct auth_ops *old;
	int rv = -EINVAL;

	if (flavor < RPC_AUTH_MAXFLAVOR) {
		old = cmpxchg((struct auth_ops ** __force)&authtab[flavor], NULL, aops);
		if (old == NULL || old == aops)
			rv = 0;
	}
	return rv;
}
EXPORT_SYMBOL_GPL(svc_auth_register);

void
svc_auth_unregister(rpc_authflavor_t flavor)
{
	if (flavor < RPC_AUTH_MAXFLAVOR)
		rcu_assign_pointer(authtab[flavor], NULL);
}
EXPORT_SYMBOL_GPL(svc_auth_unregister);

 

#define	DN_HASHBITS	6
#define	DN_HASHMAX	(1<<DN_HASHBITS)

static struct hlist_head	auth_domain_table[DN_HASHMAX];
static DEFINE_SPINLOCK(auth_domain_lock);

static void auth_domain_release(struct kref *kref)
	__releases(&auth_domain_lock)
{
	struct auth_domain *dom = container_of(kref, struct auth_domain, ref);

	hlist_del_rcu(&dom->hash);
	dom->flavour->domain_release(dom);
	spin_unlock(&auth_domain_lock);
}

void auth_domain_put(struct auth_domain *dom)
{
	kref_put_lock(&dom->ref, auth_domain_release, &auth_domain_lock);
}
EXPORT_SYMBOL_GPL(auth_domain_put);

struct auth_domain *
auth_domain_lookup(char *name, struct auth_domain *new)
{
	struct auth_domain *hp;
	struct hlist_head *head;

	head = &auth_domain_table[hash_str(name, DN_HASHBITS)];

	spin_lock(&auth_domain_lock);

	hlist_for_each_entry(hp, head, hash) {
		if (strcmp(hp->name, name)==0) {
			kref_get(&hp->ref);
			spin_unlock(&auth_domain_lock);
			return hp;
		}
	}
	if (new)
		hlist_add_head_rcu(&new->hash, head);
	spin_unlock(&auth_domain_lock);
	return new;
}
EXPORT_SYMBOL_GPL(auth_domain_lookup);

struct auth_domain *auth_domain_find(char *name)
{
	struct auth_domain *hp;
	struct hlist_head *head;

	head = &auth_domain_table[hash_str(name, DN_HASHBITS)];

	rcu_read_lock();
	hlist_for_each_entry_rcu(hp, head, hash) {
		if (strcmp(hp->name, name)==0) {
			if (!kref_get_unless_zero(&hp->ref))
				hp = NULL;
			rcu_read_unlock();
			return hp;
		}
	}
	rcu_read_unlock();
	return NULL;
}
EXPORT_SYMBOL_GPL(auth_domain_find);

 

void auth_domain_cleanup(void)
{
	int h;
	struct auth_domain *hp;

	for (h = 0; h < DN_HASHMAX; h++)
		hlist_for_each_entry(hp, &auth_domain_table[h], hash)
			pr_warn("svc: domain %s still present at module unload.\n",
				hp->name);
}
